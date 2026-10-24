// ============================================================================
// DronLink - nrf24_sniff
// Sniffer PROMISCUO de 2.4 GHz con nRF24L01+ para capturar y volcar los
// paquetes que emite el mando del dron (modelo L0712/L0717, dron balon H18).
//
// Placa:  YD-ESP32-S3 (YD-ESP32-23 V1.3)  FQBN esp32s3,FlashSize=16M,PSRAM=opi
// Radio:  nRF24L01+ (mismo cableado que nrf24_multi: CE=9 CSN=10 SCK=12 MOSI=11 MISO=13)
//
// Metodo (Goodspeed/Betz): CRC off, auto-ack off, direccion corta de 2 bytes
// puesta a un patron tipo preambulo (0x55/0xAA) para "engancharse" al preambulo
// de CUALQUIER paquete; se recibe siempre 32 bytes crudos. Se barre el canal y
// se vuelca en hex lo que caiga. Luego se analiza (y se descifra XN297) offline.
//
// Serie 115200. Comandos:
//   a         barrer todos los canales (lista los que tienen trafico)
//   d         "dwell": quedarse en el canal fijo y volcar todo lo que llegue
//   f<n>      fijar canal n (0..125)   ej: f44
//   1 / 2 / 3 tasa 1Mbps / 2Mbps / 250kbps
//   p         rotar patron de direccion promiscua (55 55 / AA AA / 00 AA / 55)
//   x         parar
//   ?         ayuda
// ============================================================================

#include <Arduino.h>
#include <SPI.h>

// ---- Pines ------------------------------------------------------------------
static const int PIN_CE = 9, PIN_CSN = 10, PIN_SCK = 12, PIN_MOSI = 11, PIN_MISO = 13;

SPIClass nrfSPI(FSPI);
static inline void CSN(bool h){ digitalWrite(PIN_CSN, h); }
static inline void CE(bool h){ digitalWrite(PIN_CE, h); }

// ---- Registros nRF24 --------------------------------------------------------
enum {
  R_CONFIG=0x00, R_EN_AA=0x01, R_EN_RXADDR=0x02, R_SETUP_AW=0x03, R_SETUP_RETR=0x04,
  R_RF_CH=0x05, R_RF_SETUP=0x06, R_STATUS=0x07, R_RX_ADDR_P0=0x0A, R_RX_PW_P0=0x11,
  R_FIFO_STATUS=0x17, R_DYNPD=0x1C, R_FEATURE=0x1D
};
static const uint8_t C_R_RX_PAYLOAD=0x61, C_FLUSH_RX=0xE2, C_W_REGISTER=0x20, C_R_REGISTER=0x00, C_NOP=0xFF;

uint8_t wreg(uint8_t r, uint8_t v){ CSN(0); nrfSPI.transfer(C_W_REGISTER|r); uint8_t s=nrfSPI.transfer(v); CSN(1); return s; }
uint8_t rreg(uint8_t r){ CSN(0); nrfSPI.transfer(C_R_REGISTER|r); uint8_t v=nrfSPI.transfer(C_NOP); CSN(1); return v; }
void wregN(uint8_t r, const uint8_t* b, uint8_t n){ CSN(0); nrfSPI.transfer(C_W_REGISTER|r); for(uint8_t i=0;i<n;i++) nrfSPI.transfer(b[i]); CSN(1); }
void cmd(uint8_t c){ CSN(0); nrfSPI.transfer(c); CSN(1); }
void readPayload(uint8_t* b, uint8_t n){ CSN(0); nrfSPI.transfer(C_R_RX_PAYLOAD); for(uint8_t i=0;i<n;i++) b[i]=nrfSPI.transfer(C_NOP); CSN(1); }

// ---- Estado -----------------------------------------------------------------
uint8_t  rate = 0;          // 0=1M 1=2M 2=250K
uint8_t  rfsetup_for_rate(){ return rate==0?0x06 : rate==1?0x0E : 0x26; }
const char* rate_name(){ return rate==0?"1Mbps":rate==1?"2Mbps":"250kbps"; }

uint8_t  addrPat = 0;       // patron de direccion promiscua
uint8_t  promAW; uint8_t promAddr[5]; uint8_t promAWlen;
void setAddrPattern(uint8_t p){
  // SETUP_AW: 0=2bytes(ilegal pero clones=2B), 1=3B,2=4B,3=5B
  switch(p){
    // Patron 0 = preambulo XN297 (55 0F 71), 3 bytes: engancha SOLO paquetes
    // XN297 reales (lo que usan estos juguetes) -> casi sin ruido.
    case 0: promAWlen=3; promAW=0x01; promAddr[0]=0x55; promAddr[1]=0x0F; promAddr[2]=0x71; break;
    case 1: promAWlen=2; promAW=0x00; promAddr[0]=0x55; promAddr[1]=0x55; break;
    case 2: promAWlen=2; promAW=0x00; promAddr[0]=0xAA; promAddr[1]=0xAA; break;
    default:promAWlen=2; promAW=0x00; promAddr[0]=0x00; promAddr[1]=0xAA; break;
  }
}

uint8_t  fixedCh = 44;
enum Mode { IDLE, SWEEP, DWELL, FASTSWEEP };
Mode mode = IDLE;

// ---- Configuracion promiscua (Betz/Goodspeed) ------------------------------
void promInit(uint8_t ch){
  CE(0);
  wreg(R_CONFIG, 0x70);            // IRQs OFF, CRC OFF, PWR_DOWN, PRX
  wreg(R_EN_AA, 0x00);             // auto-ack off
  wreg(R_EN_RXADDR, 0x01);         // pipe 0
  wreg(R_SETUP_AW, promAW);        // 2 bytes (ilegal->2B en clones)
  wreg(R_SETUP_RETR, 0x00);
  wreg(R_RF_CH, ch);
  wreg(R_RF_SETUP, rfsetup_for_rate());
  wregN(R_RX_ADDR_P0, promAddr, promAWlen);
  wreg(R_RX_PW_P0, 32);            // payload fijo 32
  wreg(R_DYNPD, 0x00);
  wreg(R_FEATURE, 0x00);
  cmd(C_FLUSH_RX);
  wreg(R_STATUS, 0x70);            // limpia flags
  wreg(R_CONFIG, 0x73);            // PWR_UP + PRX (CRC off, IRQ off)
  delayMicroseconds(1500);
  CE(1);
}

void setChannel(uint8_t ch){ CE(0); wreg(R_RF_CH, ch); cmd(C_FLUSH_RX); wreg(R_STATUS,0x70); CE(1); delayMicroseconds(300); }

bool rxReady(){ return !(rreg(R_FIFO_STATUS) & 0x01); }   // FIFO no vacia

void dumpFrame(uint8_t ch, const uint8_t* b){
  Serial.printf("CH%3u %-7s AW%u[%02X%02X] : ", ch, rate_name(), promAWlen, promAddr[0], promAddr[1]);
  for(uint8_t i=0;i<32;i++) Serial.printf("%02X", b[i]);
  Serial.println();
}

// ---- Barrido LENTO: cuenta paquetes por canal (una pasada) ------------------
void doSweep(){
  Serial.printf("\n== SWEEP %s patron[%02X%02X] (escuchando ~35ms/canal) ==\n", rate_name(), promAddr[0], promAddr[1]);
  uint16_t total=0;
  for(uint8_t ch=0; ch<126; ch++){
    setChannel(ch);
    uint32_t t0=millis(); uint8_t cnt=0; uint8_t sample[32]; bool got=false;
    while(millis()-t0 < 35){
      if(rxReady()){ readPayload(sample,32); wreg(R_STATUS,0x40); cnt++; got=true; if(cnt>=200) break; }
    }
    if(got){ total+=cnt; Serial.printf("CH%3u (%4u MHz): %3u pkts | muestra ", ch, 2400+ch, cnt); for(uint8_t i=0;i<16;i++) Serial.printf("%02X",sample[i]); Serial.println(); }
    if(Serial.available()) break;
  }
  Serial.printf("== fin sweep, total %u pkts ==\n", total);
}

// ---- Barrido RAPIDO continuo: vuelca cada paquete con su canal --------------
// ~6 ms/canal sobre 0..99 -> toda la banda en ~0.6 s. Para cazar rafagas (bind).
void doFastSweep(){
  for(uint8_t ch=10; ch<60; ch++){     // zona del bind (canales bajos-medios)
    setChannel(ch);
    uint32_t t0=millis();
    while(millis()-t0 < 6){
      if(rxReady()){
        uint8_t b[32]; readPayload(b,32); wreg(R_STATUS,0x40);
        Serial.printf("CH%3u:", ch);
        for(uint8_t i=0;i<24;i++) Serial.printf("%02X", b[i]);
        Serial.println();
      }
    }
    if(Serial.available()) return;   // atiende comandos entre pasadas
  }
}

void setup(){
  Serial.begin(115200);
  delay(400);
  pinMode(PIN_CE, OUTPUT); pinMode(PIN_CSN, OUTPUT); CE(0); CSN(1);
  nrfSPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, -1);
  nrfSPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  delay(100);
  setAddrPattern(addrPat);
  promInit(fixedCh);
  Serial.println(F("\n=== DronLink nrf24_sniff (modo promiscuo) ==="));
  Serial.println(F("Enciende el MANDO del dron cerca (en modo bind: mando on, dron off)."));
  Serial.println(F("Comandos: a=barrer  d=dwell canal fijo  f<n>=fijar canal  1/2/3=tasa  p=patron  x=parar  ?=ayuda"));
  Serial.printf("Tasa=%s  patron[%02X%02X]  canal fijo=%u\n", rate_name(), promAddr[0], promAddr[1], fixedCh);
}

void handleSerial(){
  static String num="";
  while(Serial.available()){
    char c=Serial.read();
    if(c=='f'){ num=""; // leer numero a continuacion
      // espera breve a los digitos
      uint32_t t=millis(); String n="";
      while(millis()-t<300){ if(Serial.available()){ char d=Serial.read(); if(d>='0'&&d<='9'){ n+=d; t=millis(); } else break; } }
      if(n.length()){ fixedCh=constrain(n.toInt(),0,125); setChannel(fixedCh); Serial.printf("canal fijo=%u (%u MHz)\n",fixedCh,2400+fixedCh);}
      continue;
    }
    switch(c){
      case 'a': mode=SWEEP; break;
      case 'F': mode=FASTSWEEP; Serial.println("FAST SWEEP continuo (ch0-99, ~6ms/ch)"); break;
      case 'd': mode=DWELL; promInit(fixedCh); Serial.printf("DWELL en CH%u %s patron[%02X%02X]\n",fixedCh,rate_name(),promAddr[0],promAddr[1]); break;
      case 'x': mode=IDLE; Serial.println("parado"); break;
      case '1': rate=0; promInit(fixedCh); Serial.printf("tasa=%s\n",rate_name()); break;
      case '2': rate=1; promInit(fixedCh); Serial.printf("tasa=%s\n",rate_name()); break;
      case '3': rate=2; promInit(fixedCh); Serial.printf("tasa=%s\n",rate_name()); break;
      case 'p': addrPat=(addrPat+1)&3; setAddrPattern(addrPat); promInit(fixedCh); Serial.printf("patron[%02X%02X]\n",promAddr[0],promAddr[1]); break;
      case '?': Serial.println(F("a=barrer d=dwell f<n>=canal 1/2/3=tasa p=patron x=parar")); break;
      default: break;
    }
  }
}

void loop(){
  handleSerial();
  if(mode==SWEEP){ doSweep(); mode=IDLE; }
  else if(mode==FASTSWEEP){ doFastSweep(); }
  else if(mode==DWELL){
    if(rxReady()){ uint8_t b[32]; readPayload(b,32); wreg(R_STATUS,0x40); dumpFrame(fixedCh,b); }
  }
}
