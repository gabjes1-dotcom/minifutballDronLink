// ============================================================================
// DronLink - nrf24_tx
// Transmisor XN297 para el dron-balon H18 (protocolo descifrado por nosotros).
// Controla el dron desde el PC por puerto serie.
//
// Placa: YD-ESP32-S3 (YD-ESP32-23 V1.3)  FQBN esp32s3,FlashSize=16M,PSRAM=opi
// Radio: nRF24L01+ (CE=9 CSN=10 SCK=12 MOSI=11 MISO=13)  compilar con -fcommon
//
// Protocolo (ver research/h18-protocol.md):
//   XN297, 1 Mbps, direccion 2A A4 B5 2D 24, canales {70,73,75,76,77,79,84}
//   payload 12 bytes:  00 00 B5 | TT RR PP YY | 00 FL 00 AA AA
//     TT=throttle(centro 7A), RR=roll(20), PP=pitch(20), YY=yaw(20), FL=flags(40; 44=despegue)
//
// Usa el driver (nRF24L01.ino) y XN297_emu.ino de goebish: XN297_WritePayload
// aplica el mismo cifrado+CRC XN297 que validamos al capturar.
// ============================================================================

#include <Arduino.h>
#include <SPI.h>
#include "iface_nrf24l01.h"

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

// ---- Pines + capa SPI (igual que nrf24_multi) ------------------------------
#define CE_pin 9
#define CS_pin 10
#define SCK_pin 12
#define MOSI_pin 11
#define MISO_pin 13
#define ledPin 48
#define RF_POWER TX_POWER_80mW

SPIClass nrfSPI(FSPI);
#define CS_on   digitalWrite(CS_pin, HIGH)
#define CS_off  digitalWrite(CS_pin, LOW)
#define CE_on   digitalWrite(CE_pin, HIGH)
#define CE_off  digitalWrite(CE_pin, LOW)

uint8_t spi_write(uint8_t c){ return nrfSPI.transfer(c); }
uint8_t spi_read(){ return nrfSPI.transfer(0x00); }
void spi_write_address(uint8_t a, uint8_t d){ CS_off; nrfSPI.transfer(a); nrfSPI.transfer(d); CS_on; }
uint8_t spi_read_address(uint8_t a){ CS_off; nrfSPI.transfer(a); uint8_t r=nrfSPI.transfer(0x00); CS_on; return r; }
void Read_Packet(uint8_t* data, uint8_t len){ CS_off; nrfSPI.transfer(0x61); for(uint8_t i=0;i<len;i++) data[i]=nrfSPI.transfer(0x00); CS_on; }

// ---- Protocolo H18 ----------------------------------------------------------
static const uint8_t H18_ADDR[5] = { 0x2A, 0xA4, 0xB5, 0x2D, 0x24 };
static uint8_t hop[] = { 70, 73, 75, 76, 77, 79, 84 };
static const uint8_t HOP_N = sizeof(hop);

// payload logico de 12 bytes (XN297_WritePayload lo cifra). Indices de control:
//   [3]=throttle [4]=roll [5]=pitch [6]=yaw [8]=flags
uint8_t msg[12] = { 0x00, 0x00, 0xB5, 0x6E, 0x20, 0x20, 0x20, 0x00, 0x40, 0x00, 0xAA, 0xAA };
// Throttle arranca en 0x6E = un poco POR DEBAJO del centro (0x7A) -> despegue
// suave y menos tendencia a trepar con pila llena. Los motores no arrancan hasta
// el despegue ('b'); 0x00 = "bajar a fondo" (impide despegar), por eso no usamos 0.

// Baliza de sync (Tipo B), replay crudo de los 4 estados capturados (9 bytes
// on-air tras el preambulo). E3 B1 4B EA 85 = direccion XN297 0x00.
static const uint8_t BEACON[4][9] = {
  {0xE3,0xB1,0x4B,0xEA,0x85, 0xBD,0x4C,0xB8,0x54},
  {0xE3,0xB1,0x4B,0xEA,0x85, 0xBD,0xC4,0xA8,0xD4},
  {0xE3,0xB1,0x4B,0xEA,0x85, 0xBC,0x5C,0x99,0x54},
  {0xE3,0xB1,0x4B,0xEA,0x85, 0xBC,0xD4,0x89,0xD4},
};
uint8_t bidx = 0;
bool sendBeacon = true;   // 'g' alterna enviar/no la baliza

// Paquete de BIND (fijo, 17 bytes on-air, CRC XN297 valido). El mando lo difunde
// en canales bajos al encenderse, para que el dron recien encendido lo encuentre
// y aprenda a seguir este enlace. Replay crudo.
static const uint8_t BIND_PKT[17] = {
  0xA9,0x02,0xD2,0x5F,0x57,0x8E,0x37,0x3A,0x4F,0x32,0x8C,0x88,0x12,0x3C,0xBB,0x18,0xC5
};
static const uint8_t BIND_CH[] = {17,20,24,28,32,36,40,44,48,52};
static const uint8_t BIND_CH_N = sizeof(BIND_CH);

const uint8_t THR=3, ROL=4, PIT=5, YAW=6, FLG=8;
const uint8_t RPY_CENTER=0x20, RPY_MIN=0x00, RPY_MAX=0x3F, THR_CENTER=0x7A, THR_HOVER=0x6E;

bool txOn = false;           // arranca en pausa: primero bind con 'B'
uint32_t takeoffUntil = 0;   // despegue = flag 0x04 activo hasta este millis

// Sticks con AUTO-CENTRADO: cada pulsacion deflecta fuerte y vuelve al centro.
uint32_t rollHold=0, pitchHold=0, yawHold=0;
const uint8_t  STICK_DEFL   = 0x18;   // deflexion desde el centro (0x20 -> 0x08..0x38)
const uint16_t STICK_HOLD_MS = 350;   // cuanto mantiene la deflexion antes de recentrar

// ---- Init XN297 TX ----------------------------------------------------------
void h18_tx_init(){
    NRF24L01_Initialize();
    NRF24L01_SetTxRxMode(TX_EN);
    XN297_SetTXAddr(H18_ADDR, 5);
    NRF24L01_FlushTx();
    NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
    NRF24L01_WriteReg(NRF24L01_01_EN_AA, 0x00);
    NRF24L01_WriteReg(NRF24L01_02_EN_RXADDR, 0x01);
    NRF24L01_WriteReg(NRF24L01_04_SETUP_RETR, 0x00);
    NRF24L01_SetBitrate(NRF24L01_BR_1M);
    NRF24L01_SetPower(RF_POWER);
    XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP));
}

// Envia control + baliza en todos los canales de salto (shotgun).
void h18_send_all_channels(){
    msg[FLG] = (millis() < takeoffUntil) ? 0x44 : 0x40;
    for(uint8_t i=0;i<HOP_N;i++){
        NRF24L01_WriteReg(NRF24L01_05_RF_CH, hop[i]);
        NRF24L01_SetTxRxMode(TX_EN);
        XN297_WritePayload(msg, 12);       // trama de control
        delayMicroseconds(450);
        if(sendBeacon){
            NRF24L01_FlushTx();
            NRF24L01_WritePayload((uint8_t*)BEACON[bidx], 9);  // baliza (replay crudo)
            bidx = (bidx + 1) & 3;
            delayMicroseconds(450);
        }
    }
}

// Rafaga de BIND: difunde el paquete fijo en los canales de bind ~700ms,
// luego arranca el control. Hazlo con el DRON ya encendido y esperando.
void h18_bind_burst(){
    Serial.println(">>> BIND burst (difundiendo en canales bajos)...");
    uint32_t t0 = millis(); uint8_t ci = 0;
    while(millis() - t0 < 700){
        NRF24L01_WriteReg(NRF24L01_05_RF_CH, BIND_CH[ci]);
        ci = (ci + 1) % BIND_CH_N;
        NRF24L01_SetTxRxMode(TX_EN);
        NRF24L01_FlushTx();
        NRF24L01_WritePayload((uint8_t*)BIND_PKT, 17);
        delayMicroseconds(500);
    }
    txOn = true;
    Serial.println(">>> BIND enviado. Control+baliza ON. Mira el LED del dron.");
}

int clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

void printState(){
    Serial.printf("TX %s | THR=%02X ROLL=%02X PITCH=%02X YAW=%02X FLAGS=%02X%s\n",
        txOn?"ON":"OFF", msg[THR], msg[ROL], msg[PIT], msg[YAW], msg[FLG],
        (millis()<takeoffUntil)?"  [DESPEGUE]":"");
}

void help(){
    Serial.println(F("\n=== DronLink nrf24_tx (control H18) ==="));
    Serial.println(F("Apaga el MANDO REAL. Dron encendido, HELICES FUERA para probar."));
    Serial.println(F("  w/s  throttle +/-      x=centro(7A)  z=minimo(00)"));
    Serial.println(F("  j/l  roll izq/der      i/k  pitch adel/atras   a/d  yaw izq/der"));
    Serial.println(F("  espacio  centrar roll/pitch/yaw"));
    Serial.println(F("  b    DESPEGUE suave      L  ATERRIZAR (toggle + descenso)"));
    Serial.println(F("  n    neutral total (throttle centro, ejes centro)"));
    Serial.println(F("  o    TX on/off         v=estado   ?=ayuda"));
}

void handleSerial(){
    while(Serial.available()){
        char c = Serial.read();
        switch(c){
            case 'w': msg[THR]=clampi(msg[THR]+6,0,255); printState(); break;
            case 's': msg[THR]=clampi(msg[THR]-6,0,255); printState(); break;
            case 'x': msg[THR]=THR_CENTER; printState(); break;
            case 'z': msg[THR]=0x00; printState(); break;
            case 'l': msg[ROL]=RPY_CENTER+STICK_DEFL; rollHold =millis()+STICK_HOLD_MS; printState(); break;
            case 'j': msg[ROL]=RPY_CENTER-STICK_DEFL; rollHold =millis()+STICK_HOLD_MS; printState(); break;
            case 'i': msg[PIT]=RPY_CENTER+STICK_DEFL; pitchHold=millis()+STICK_HOLD_MS; printState(); break;
            case 'k': msg[PIT]=RPY_CENTER-STICK_DEFL; pitchHold=millis()+STICK_HOLD_MS; printState(); break;
            case 'd': msg[YAW]=RPY_CENTER+STICK_DEFL; yawHold  =millis()+STICK_HOLD_MS; printState(); break;
            case 'a': msg[YAW]=RPY_CENTER-STICK_DEFL; yawHold  =millis()+STICK_HOLD_MS; printState(); break;
            case ' ': msg[ROL]=msg[PIT]=msg[YAW]=RPY_CENTER; rollHold=pitchHold=yawHold=0; printState(); break;
            case 'b': if(msg[THR]<THR_HOVER) msg[THR]=THR_HOVER;    // despegue SUAVE desde hover-bajo
                      takeoffUntil=millis()+700; Serial.println("DESPEGUE (suave)"); break;
            case 'L': msg[THR]=0x40; takeoffUntil=millis()+700;     // ATERRIZAR: toggle + descender
                      Serial.println("ATERRIZAR (toggle + descenso)"); break;
            case 'n': msg[THR]=THR_CENTER; msg[ROL]=msg[PIT]=msg[YAW]=RPY_CENTER; takeoffUntil=0; printState(); break;
            case 'o': txOn=!txOn; printState(); break;
            case 'g': sendBeacon=!sendBeacon; Serial.printf("baliza %s\n", sendBeacon?"ON":"OFF"); break;
            case 'B': h18_bind_burst(); break;
            case 'v': printState(); break;
            case '?': help(); break;
            default: break;
        }
    }
}

void setup(){
    Serial.begin(115200);
    delay(400);
    pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);
    pinMode(CE_pin, OUTPUT); pinMode(CS_pin, OUTPUT); CE_off; CS_on;
    nrfSPI.begin(SCK_pin, MISO_pin, MOSI_pin, -1);
    nrfSPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    delay(100);
    h18_tx_init();
    help();
    printState();
}

void loop(){
    handleSerial();
    // auto-centrado de sticks: tras STICK_HOLD_MS vuelven al centro
    uint32_t now = millis();
    if(rollHold  && now > rollHold ){ msg[ROL]=RPY_CENTER; rollHold =0; }
    if(pitchHold && now > pitchHold){ msg[PIT]=RPY_CENTER; pitchHold=0; }
    if(yawHold   && now > yawHold  ){ msg[YAW]=RPY_CENTER; yawHold  =0; }
    if(txOn) h18_send_all_channels();
    else delay(2);
}
