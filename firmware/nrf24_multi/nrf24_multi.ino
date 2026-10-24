// ============================================================================
// DronLink - nrf24_multi  (port a ESP32-S3 de goebish/nRF24_multipro, GPLv3)
//
// Prueba protocolos de drones de juguete 2.4 GHz (nRF24/Beken) para descubrir
// con cual enlaza TU dron. Puente controlado por PUERTO SERIE (115200).
//
// Placa:  YD-ESP32-S3 (YD-ESP32-23 V1.3)
//   FQBN: esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi   (COM11)
// Radio:  nRF24L01+  (ver WIRING.md). SPI por HARDWARE.
//
// IMPORTANTE: compilar con  -fcommon  (el proyecto original AVR depende de
//   ello; el GCC del ESP32 usa -fno-common por defecto). El comando de
//   compilacion lo pasa como build-property; ver flash.ps1 / notas del chat.
//
// Driver (nRF24L01.ino), emulacion XN297 (XN297_emu.ino) y los .ino de cada
// protocolo se usan VERBATIM del proyecto original. Este archivo reemplaza al
// main AVR: provee la capa SPI del ESP32, los simbolos globales y un control
// por serie en lugar del PPM de una emisora RC.
// ============================================================================

#include <Arduino.h>
#include <SPI.h>
#include "iface_nrf24l01.h"

// Typedefs estilo Deviation que usan algunos protocolos (MJX, KN, Bayang...)
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

// ---- Pines nRF24 <-> YD-ESP32-S3 (ver WIRING.md) ---------------------------
#define CE_pin    9
#define CS_pin    10
#define SCK_pin   12
#define MOSI_pin  11
#define MISO_pin  13
#define ledPin    48    // LED onboard de la YD-S3 (feedback durante el bind)

// ---- Capa SPI (reemplaza softSPI.ino bit-bang por SPI de hardware) ---------
SPIClass nrfSPI(FSPI);

#define CS_on   digitalWrite(CS_pin, HIGH)
#define CS_off  digitalWrite(CS_pin, LOW)
#define CE_on   digitalWrite(CE_pin, HIGH)
#define CE_off  digitalWrite(CE_pin, LOW)

uint8_t spi_write(uint8_t command) { return nrfSPI.transfer(command); }
uint8_t spi_read()                 { return nrfSPI.transfer(0x00); }

void spi_write_address(uint8_t address, uint8_t data) {
    CS_off;
    nrfSPI.transfer(address);
    nrfSPI.transfer(data);
    CS_on;
}

uint8_t spi_read_address(uint8_t address) {
    CS_off;
    nrfSPI.transfer(address);
    uint8_t result = nrfSPI.transfer(0x00);
    CS_on;
    return result;
}

void Read_Packet(uint8_t *data, uint8_t length) {
    CS_off;
    nrfSPI.transfer(0x61);              // R_RX_PAYLOAD
    for (uint8_t i = 0; i < length; i++) data[i] = nrfSPI.transfer(0x00);
    CS_on;
}

// ---- Parametros de canal/control (del main original) -----------------------
#define RF_POWER TX_POWER_80mW
#define CHANNELS 12

enum chan_order {
    THROTTLE, AILERON, ELEVATOR, RUDDER,
    AUX1, AUX2, AUX3, AUX4, AUX5, AUX6, AUX7, AUX8,
};

#define PPM_MIN 1000
#define PPM_SAFE_THROTTLE 1050
#define PPM_MID 1500
#define PPM_MAX 2000
#define PPM_MIN_COMMAND 1300
#define PPM_MAX_COMMAND 1700
#define GET_FLAG(ch, mask)     (ppm[ch] > PPM_MAX_COMMAND ? mask : 0)
#define GET_FLAG_INV(ch, mask) (ppm[ch] < PPM_MIN_COMMAND ? mask : 0)

// ---- Lista de protocolos (identica al original) ----------------------------
enum {
    PROTO_V2X2 = 0,
    PROTO_CG023,
    PROTO_CX10_BLUE,
    PROTO_CX10_GREEN,
    PROTO_H7,
    PROTO_BAYANG,
    PROTO_SYMAX5C1,
    PROTO_YD829,
    PROTO_H8_3D,
    PROTO_MJX,
    PROTO_SYMAXOLD,
    PROTO_HISKY,
    PROTO_KN,
    PROTO_YD717,
    PROTO_FQ777124,
    PROTO_E010,
    PROTO_BAYANG_SILVERWARE,
    PROTO_END
};

// ---- Globales que usan driver y protocolos ---------------------------------
uint8_t  transmitterID[4];
uint8_t  current_protocol;
uint8_t  packet[32];
static bool reset = true;

// Valores de control (lo que antes venia por PPM). Arranca en throttle minimo
// y palancas centradas: seguro para binding.
uint16_t ppm[12] = { PPM_MIN, PPM_MID, PPM_MID, PPM_MID,
                     PPM_MID, PPM_MID, PPM_MID, PPM_MID,
                     PPM_MID, PPM_MID, PPM_MID, PPM_MID };

// Bayang (variante silverware) escribe aqui su telemetria; la declaramos para
// que compile aunque no usemos frsky.
struct {
    uint16_t volt1;
    uint16_t rssi;
    uint8_t  updated;
    uint32_t lastUpdate;
} telemetry_data;

// ---- Menu de prueba: orden de ciclado y nombres ----------------------------
struct ProtoItem { uint8_t id; const char* name; };
const ProtoItem CYCLE[] = {
    { PROTO_BAYANG,     "Bayang (H8 mini, BayangToys, JJRC, Floureon H101)" },
    { PROTO_H8_3D,      "H8_3D (H8 mini 3D, JJRC H20/H22)" },
    { PROTO_V2X2,       "V2x2 (WLToys V2x2, JXD, JJRC H6C, Tarantula X6)" },
    { PROTO_CX10_BLUE,  "CX10 blue (CX-10/A/C, CX-11, Floureon FX-10)" },
    { PROTO_CX10_GREEN, "CX10 green (Cheerson CX-10 placa verde)" },
    { PROTO_SYMAX5C1,   "SymaX X5C-1 (X11, X12)" },
    { PROTO_SYMAXOLD,   "SymaX old (X5C, X2)" },
    { PROTO_CG023,      "CG023 (Eachine CG023/CG032, 3D X4)" },
    { PROTO_YD829,      "YD829 (YD-829/829C/822)" },
    { PROTO_MJX,        "MJX (X600, WLH08, X800, H26D)" },
    { PROTO_E010,       "E010 (Eachine E010, JJRC H36, NH-010)" },
    { PROTO_H7,         "H7 (Eachine H7, MT99xx)" },
    { PROTO_YD717,      "YD717 (CX-10 rojo viejo, JXD389/390, SH6057...)" },
    { PROTO_FQ777124,   "FQ777-124 (pocket drone)" },
    { PROTO_HISKY,      "HiSky (HFP80, HCP80/100, WLToys v933/944...)" },
    { PROTO_KN,         "KN (WLToys V930/931/939/966/977/988)" },
};
const uint8_t CYCLE_N = sizeof(CYCLE) / sizeof(CYCLE[0]);
uint8_t cycleIdx = 0;

bool     autoCycle = false;
uint32_t autoMs = 6000;          // ms por protocolo en modo auto
uint32_t lastSwitch = 0;

// Modo CAZA: re-envia el bind cada rebindMs para que el dron pueda enlazar
// en cualquier momento (sin cronometrar el encendido). Se apaga al pasar a
// VUELO (control continuo) una vez enlazado.
bool     hunting = true;
uint32_t lastBind = 0;
uint32_t rebindMs = 4000;

// ---- Prototipos de los procesos de cada protocolo (definidos en sus .ino) --
uint32_t process_V2x2();
uint32_t process_CG023();
uint32_t process_CX10();
uint32_t process_H7();
uint32_t process_Bayang();
uint32_t process_SymaX();
uint32_t process_H8_3D();
uint32_t process_MJX();
uint32_t process_HiSky();
uint32_t process_KN();
uint32_t process_YD717();
uint32_t process_FQ777124();

// ---- Utilidades ------------------------------------------------------------
void set_txid(bool renew) {
    (void)renew;
    for (uint8_t i = 0; i < 4; i++) transmitterID[i] = random(256);
}

void init_protocol() {
    switch (current_protocol) {
        case PROTO_CG023:
        case PROTO_YD829:            CG023_init();  CG023_bind();  break;
        case PROTO_V2X2:             V2x2_init();   V2x2_bind();   break;
        case PROTO_CX10_GREEN:
        case PROTO_CX10_BLUE:        CX10_init();   CX10_bind();   break;
        case PROTO_H7:               H7_init();     H7_bind();     break;
        case PROTO_BAYANG:
        case PROTO_BAYANG_SILVERWARE:Bayang_init(); Bayang_bind(); break;
        case PROTO_SYMAX5C1:
        case PROTO_SYMAXOLD:         Symax_init();                 break;
        case PROTO_H8_3D:            H8_3D_init();  H8_3D_bind();   break;
        case PROTO_MJX:
        case PROTO_E010:             MJX_init();    MJX_bind();     break;
        case PROTO_HISKY:            HiSky_init();                 break;
        case PROTO_KN:               kn_start_tx(true);            break;
        case PROTO_YD717:            YD717_init();                 break;
        case PROTO_FQ777124:         FQ777124_init(); FQ777124_bind(); break;
    }
}

uint32_t dispatch_protocol() {
    switch (current_protocol) {
        case PROTO_CG023:
        case PROTO_YD829:            return process_CG023();
        case PROTO_V2X2:             return process_V2x2();
        case PROTO_CX10_GREEN:
        case PROTO_CX10_BLUE:        return process_CX10();
        case PROTO_H7:               return process_H7();
        case PROTO_BAYANG:
        case PROTO_BAYANG_SILVERWARE:return process_Bayang();
        case PROTO_SYMAX5C1:
        case PROTO_SYMAXOLD:         return process_SymaX();
        case PROTO_H8_3D:            return process_H8_3D();
        case PROTO_MJX:
        case PROTO_E010:             return process_MJX();
        case PROTO_HISKY:            return process_HiSky();
        case PROTO_KN:               return process_KN();
        case PROTO_YD717:            return process_YD717();
        case PROTO_FQ777124:         return process_FQ777124();
    }
    return micros() + 10000;
}

void applyProtocol() {
    reset = false;
    Serial.printf("\n>>> [%u/%u] Protocolo: %s\n", cycleIdx + 1, CYCLE_N, CYCLE[cycleIdx].name);
    Serial.println("    enviando secuencia de bind (puede tardar unos segundos)...");
    NRF24L01_Reset();
    NRF24L01_Initialize();
    init_protocol();
    lastSwitch = millis();
    Serial.println("    bind enviado; transmitiendo control. Mira el LED del dron: fijo = enlazado.");
}

void printMenu() {
    Serial.println(F("\n================ DronLink nrf24_multi ================"));
    Serial.println(F("Prueba de protocolos para enlazar tu dron 2.4 GHz."));
    Serial.println(F("Comandos:"));
    Serial.println(F("  <num>+Enter  seleccionar protocolo de la lista"));
    Serial.println(F("  n / p        siguiente / anterior protocolo"));
    Serial.println(F("  c            auto-ciclar (6 s c/u) on/off"));
    Serial.println(F("  r            re-bind del protocolo actual (nuevo ID)"));
    Serial.println(F("  f            CAZA (re-bind auto) <-> VUELO (control continuo)"));
    Serial.println(F("  s            estado actual"));
    Serial.println(F("  + / -        throttle +/- 50   (0 = throttle minimo)"));
    Serial.println(F("  ?            mostrar este menu"));
    Serial.println(F("------------------------------------------------------"));
    for (uint8_t i = 0; i < CYCLE_N; i++)
        Serial.printf("  %2u  %s\n", i, CYCLE[i].name);
    Serial.println(F("======================================================"));
}

void selectIndex(int idx) {
    if (idx < 0 || idx >= CYCLE_N) { Serial.printf("indice fuera de rango (0..%u)\n", CYCLE_N - 1); return; }
    cycleIdx = idx;
    current_protocol = CYCLE[cycleIdx].id;
    set_txid(true);                 // ID de emisora nuevo al cambiar de protocolo
    hunting = true;                 // al cambiar de protocolo, volver a cazar
    reset = true;
}

void handleCmd(char ch) {
    switch (ch) {
        case 'n': selectIndex((cycleIdx + 1) % CYCLE_N); break;
        case 'p': selectIndex((cycleIdx + CYCLE_N - 1) % CYCLE_N); break;
        case 'c':
            autoCycle = !autoCycle;
            Serial.printf("auto-ciclado %s\n", autoCycle ? "ON" : "OFF");
            if (autoCycle) lastSwitch = millis();
            break;
        case 'r': set_txid(true); reset = true; Serial.println("re-bind (nuevo ID)..."); break;
        case 'f':
            hunting = !hunting;
            Serial.printf("modo %s\n", hunting ? "CAZA (re-bind cada 4s)" : "VUELO (control continuo)");
            break;
        case 's':
            Serial.printf("actual: [%u/%u] %s | modo %s | throttle=%u\n",
                          cycleIdx + 1, CYCLE_N, CYCLE[cycleIdx].name,
                          hunting ? "CAZA" : "VUELO", ppm[THROTTLE]);
            break;
        case '+': ppm[THROTTLE] = min(PPM_MAX, ppm[THROTTLE] + 50); Serial.printf("throttle=%u\n", ppm[THROTTLE]); break;
        case '-': ppm[THROTTLE] = max(PPM_MIN, ppm[THROTTLE] - 50); Serial.printf("throttle=%u\n", ppm[THROTTLE]); break;
        case '0': ppm[THROTTLE] = PPM_MIN; Serial.println("throttle=min"); break;
        case '?': printMenu(); break;
        default: break;   // ignora espacios, \r, \n
    }
}

void handleSerial() {
    static String tok = "";
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch >= '0' && ch <= '9') { tok += ch; continue; }
        if (ch == '\r' || ch == '\n') {
            if (tok.length()) { selectIndex(tok.toInt()); tok = ""; }
            continue;
        }
        if (tok.length()) { selectIndex(tok.toInt()); tok = ""; }  // separador no-digito
        handleCmd(ch);
    }
}

// ---- Arduino ---------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(400);

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    pinMode(CE_pin, OUTPUT);
    pinMode(CS_pin, OUTPUT);
    CE_off;
    CS_on;

    nrfSPI.begin(SCK_pin, MISO_pin, MOSI_pin, -1);     // CS lo manejamos a mano
    nrfSPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));  // unico maestro

    randomSeed(esp_random());
    set_txid(true);

    delay(100);  // power-on reset del nRF24
    NRF24L01_Reset();
    NRF24L01_Initialize();

    printMenu();
    // arranca en el primer protocolo (Bayang, el mas comun)
    cycleIdx = 0;
    current_protocol = CYCLE[0].id;
    reset = true;
}

void loop() {
    handleSerial();

    if (autoCycle && (millis() - lastSwitch >= autoMs)) {
        selectIndex((cycleIdx + 1) % CYCLE_N);
    }

    // Modo caza: re-lanza el bind periodicamente (asi el dron puede enlazar
    // en cualquier instante, no solo si se enciende justo durante el bind).
    if (hunting && !reset && (millis() - lastBind >= rebindMs)) {
        reset = true;
    }

    if (reset) {
        applyProtocol();
        lastBind = millis();
    }

    uint32_t timeout = dispatch_protocol();
    while ((int32_t)(micros() - timeout) < 0) {
        // espera activa hasta el proximo paquete; atiende serie entre tanto
        handleSerial();
    }
}
