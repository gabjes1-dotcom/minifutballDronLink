# Cableado nRF24L01+ ↔ YD-ESP32-S3 (YD-ESP32-23 V1.3)

Proyecto **DronLink**: control de drones 2.4 GHz desde el PC usando un
nRF24L01+ como radio y una **YD-ESP32-S3** (16 MB flash / 8 MB PSRAM OPI,
suele enumerar en **COM11**) como puente USB.

> Pines elegidos para la S3: se evitan los **GPIO 26–37** (los usa la
> flash/PSRAM octal) y los de strapping/USB (0, 3, 19, 20, 45, 46). Se usa
> el bloque SPI estándar de la S3 (GPIO 9–13), libre y expuesto en el header.

## Conexiones

El nRF24L01+ es SPI. El módulo trae 8 pines en 2 filas (vista desde arriba,
la antena/chip hacia arriba):

```
            +---------------------+
   GND  o   |                     |   o  VCC   (3.3 V !!!)
   CE   o   |   nRF24L01+  (PA?)   |   o  CSN
   SCK  o   |                     |   o  MOSI
   MISO o   |                     |   o  IRQ   (no se usa)
            +---------------------+
```

| nRF24L01+ | YD-ESP32-S3 | Nota |
|-----------|-------------|------|
| VCC       | **3V3**     | ⚠️ NUNCA a 5 V. El chip muere a 5 V. |
| GND       | GND         | |
| CE        | GPIO9       | Chip Enable |
| CSN       | GPIO10      | SPI Chip Select |
| SCK       | GPIO12      | SPI Clock |
| MOSI      | GPIO11      | SPI MOSI |
| MISO      | GPIO13      | SPI MISO |
| IRQ       | — (sin conectar) | opcional |

## ⚠️ Lo más importante: la alimentación

El nRF24L01+ da **picos de corriente** al transmitir. El regulador 3V3 de la
YD-ESP32-S3 aguanta mejor que el del C3 Super Mini, pero igual conviene
filtrar: si no, el *bind* falla intermitente o el módulo ni responde por SPI.
Dos reglas:

1. **Condensador de 10 µF (o 4.7–100 µF) directo entre VCC y GND del módulo**,
   lo más pegado posible a los pines del nRF24. Es casi obligatorio.
2. Los módulos con **PA/LNA** (los de antena externa grande) consumen más:
   si tienes uno de esos y da problemas, aliméntalo de una fuente 3.3 V
   aparte (compartiendo GND con el ESP32), no del 3V3 del C3.

## Qué hacer al conectarlo

1. Cablea según la tabla (con el condensador).
2. Flashea `firmware/test_nrf24/test_nrf24.ino`.
3. Abre el monitor serie a **115200 baudios**.
4. Debe salir `nRF24: OK` (lectura/escritura de registros correcta).
   Si sale `FAIL`, revisa cableado/alimentación antes de seguir.
5. Luego lanza el escáner (comando `s`) para ver la actividad 2.4 GHz
   del mando del dron.
