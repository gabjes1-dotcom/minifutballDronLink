// ============================================================================
// DronLink - test_nrf24
// Diagnostico de cableado del nRF24L01+ + escaner de banda 2.4 GHz
// Placa: YD-ESP32-S3 (YD-ESP32-23 V1.3) - core esp32:esp32 3.3.7
//   FQBN: esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi   (suele ir por COM11)
//
// Objetivo de este sketch (fase 1, "aqui voy conectando"):
//   1) Confirmar que el nRF24L01+ esta bien cableado y alimentado
//      (lee/escribe registros por SPI y comprueba el round-trip).
//   2) Escanear los 126 canales de 2.4 GHz y mostrar donde hay actividad,
//      para ver el mando del dron transmitiendo (enciendelo cerca).
//
// Acceso a registros EN CRUDO (sin libreria RF24) para no depender de nada
// y para que el test sea tambien una prueba real del bus SPI.
//
// Monitor serie a 115200. Comandos: 't' test registros, 's' escaner continuo,
//   'x' parar escaner.
// ============================================================================

#include <SPI.h>

// ---- Pines (ver WIRING.md) --------------------------------------------------
// YD-ESP32-S3: se evitan GPIO 26-37 (flash/PSRAM octal) y strapping/USB.
static const int PIN_CE   = 9;
static const int PIN_CSN  = 10;
static const int PIN_SCK  = 12;
static const int PIN_MOSI = 11;
static const int PIN_MISO = 13;

// ---- Registros y comandos nRF24 --------------------------------------------
static const uint8_t CMD_R_REGISTER = 0x00;
static const uint8_t CMD_W_REGISTER = 0x20;
static const uint8_t CMD_FLUSH_RX   = 0xE2;
static const uint8_t CMD_FLUSH_TX   = 0xE1;
static const uint8_t CMD_NOP        = 0xFF;

static const uint8_t REG_CONFIG   = 0x00;
static const uint8_t REG_EN_AA    = 0x01;
static const uint8_t REG_SETUP_AW = 0x03;
static const uint8_t REG_RF_CH    = 0x05;
static const uint8_t REG_RF_SETUP = 0x06;
static const uint8_t REG_STATUS   = 0x07;
static const uint8_t REG_RPD      = 0x09;   // Received Power Detector (nRF24L01+)
static const uint8_t REG_TX_ADDR  = 0x10;

SPIClass spi(FSPI);
static const SPISettings kSpi(8000000, MSBFIRST, SPI_MODE0);

// ---- Helpers SPI ------------------------------------------------------------
static inline void csnLow()  { digitalWrite(PIN_CSN, LOW);  }
static inline void csnHigh() { digitalWrite(PIN_CSN, HIGH); }
static inline void ceLow()   { digitalWrite(PIN_CE,  LOW);  }
static inline void ceHigh()  { digitalWrite(PIN_CE,  HIGH); }

uint8_t writeReg(uint8_t reg, uint8_t val) {
  spi.beginTransaction(kSpi);
  csnLow();
  uint8_t status = spi.transfer(CMD_W_REGISTER | (reg & 0x1F));
  spi.transfer(val);
  csnHigh();
  spi.endTransaction();
  return status;
}

uint8_t readReg(uint8_t reg) {
  spi.beginTransaction(kSpi);
  csnLow();
  spi.transfer(CMD_R_REGISTER | (reg & 0x1F));
  uint8_t val = spi.transfer(CMD_NOP);
  csnHigh();
  spi.endTransaction();
  return val;
}

void writeRegMulti(uint8_t reg, const uint8_t* buf, uint8_t len) {
  spi.beginTransaction(kSpi);
  csnLow();
  spi.transfer(CMD_W_REGISTER | (reg & 0x1F));
  for (uint8_t i = 0; i < len; i++) spi.transfer(buf[i]);
  csnHigh();
  spi.endTransaction();
}

void readRegMulti(uint8_t reg, uint8_t* buf, uint8_t len) {
  spi.beginTransaction(kSpi);
  csnLow();
  spi.transfer(CMD_R_REGISTER | (reg & 0x1F));
  for (uint8_t i = 0; i < len; i++) buf[i] = spi.transfer(CMD_NOP);
  csnHigh();
  spi.endTransaction();
}

void sendCmd(uint8_t cmd) {
  spi.beginTransaction(kSpi);
  csnLow();
  spi.transfer(cmd);
  csnHigh();
  spi.endTransaction();
}

// ---- Test de cableado -------------------------------------------------------
// Escribe valores conocidos en registros escribibles y los lee de vuelta.
// Si el modulo esta mal cableado/alimentado, los read-back no coinciden.
bool testRegisters() {
  Serial.println(F("\n--- Test de registros nRF24 ---"));

  // 1) RF_CH: escribir 76 (0x4C) y leer.
  writeReg(REG_RF_CH, 0x4C);
  uint8_t ch = readReg(REG_RF_CH);
  Serial.printf("  RF_CH  escrito=0x4C leido=0x%02X %s\n", ch, ch == 0x4C ? "OK" : "FAIL");

  // 2) TX_ADDR: escribir 5 bytes y leer de vuelta.
  const uint8_t addr[5] = {0xE1, 0xC2, 0xA3, 0xB4, 0x55};
  uint8_t rb[5] = {0};
  writeRegMulti(REG_TX_ADDR, addr, 5);
  readRegMulti(REG_TX_ADDR, rb, 5);
  bool addrOk = true;
  for (uint8_t i = 0; i < 5; i++) if (rb[i] != addr[i]) addrOk = false;
  Serial.printf("  TX_ADDR  %02X%02X%02X%02X%02X  %s\n",
                rb[0], rb[1], rb[2], rb[3], rb[4], addrOk ? "OK" : "FAIL");

  // 3) SETUP_AW por defecto debe ser 0x03 (direcciones de 5 bytes).
  uint8_t aw = readReg(REG_SETUP_AW);
  Serial.printf("  SETUP_AW leido=0x%02X (esperado 0x03)\n", aw);

  // 4) CONFIG tras reset ~0x08.
  uint8_t cfg = readReg(REG_CONFIG);
  Serial.printf("  CONFIG   leido=0x%02X\n", cfg);

  bool ok = (ch == 0x4C) && addrOk && (aw == 0x03);
  Serial.printf(">> nRF24: %s\n", ok ? "OK (cableado y alimentacion correctos)"
                                      : "FAIL (revisa SPI / 3V3 / condensador)");
  return ok;
}

// ---- Escaner de banda -------------------------------------------------------
// Pone el modulo en RX sin CRC, recorre los 126 canales y, en cada uno, mide
// varias veces el RPD (hay portadora > -64 dBm). Dibuja una barra por canal.
// Enciende el mando del dron cerca: deberias ver picos en sus canales.
static const uint8_t SCAN_REPS = 100;
uint8_t scanCounts[126];

void scannerSetup() {
  ceLow();
  writeReg(REG_CONFIG, 0x03);   // PWR_UP=1, PRIM_RX=1, CRC off
  writeReg(REG_EN_AA, 0x00);    // sin auto-ack
  writeReg(REG_RF_SETUP, 0x06); // 1 Mbps, 0 dBm
  sendCmd(CMD_FLUSH_RX);
  writeReg(REG_STATUS, 0x70);   // limpia flags
  delay(2);
}

void scanOnce() {
  for (uint8_t ch = 0; ch < 126; ch++) {
    uint8_t hits = 0;
    for (uint8_t r = 0; r < SCAN_REPS; r++) {
      writeReg(REG_RF_CH, ch);
      ceHigh();
      delayMicroseconds(200);   // tiempo de asentamiento + escucha
      ceLow();
      if (readReg(REG_RPD) & 0x01) hits++;
    }
    scanCounts[ch] = hits;
  }
}

void printScan() {
  // Canal nRF = 2400 + ch MHz. WiFi 1/6/11 ~ ch 12/37/62 (ancho).
  Serial.println(F("\nch  MHz   actividad (RPD)"));
  for (uint8_t ch = 0; ch < 126; ch++) {
    uint8_t h = scanCounts[ch];
    if (h == 0) continue;              // solo canales con algo
    Serial.printf("%3d %4d  ", ch, 2400 + ch);
    uint8_t bars = h / 2;              // 0..50
    for (uint8_t i = 0; i < bars; i++) Serial.print('#');
    Serial.printf(" %u%%\n", h);
  }
  Serial.println(F("(vacio = sin actividad; enciende el mando del dron cerca)"));
}

// ---- Arduino ----------------------------------------------------------------
bool scanning = false;

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_CE, OUTPUT);
  pinMode(PIN_CSN, OUTPUT);
  ceLow();
  csnHigh();

  spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, -1);  // CSN manual

  delay(100);  // power-on reset del nRF24

  Serial.println(F("\n=== DronLink test_nrf24 ==="));
  Serial.println(F("Comandos:  t=test registros   s=escaner   x=parar"));
  testRegisters();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') { scanning = false; testRegisters(); }
    else if (c == 's') { scannerSetup(); scanning = true; Serial.println(F("\nEscaneando... (x para parar)")); }
    else if (c == 'x') { scanning = false; Serial.println(F("Escaner detenido.")); }
  }

  if (scanning) {
    scanOnce();
    printScan();
    delay(300);
  }
}
