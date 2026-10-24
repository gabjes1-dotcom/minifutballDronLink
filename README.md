# DronLink — control de un dron-balón chino (XN297) desde el PC

Ingeniería inversa y control, desde cero, del enlace de radio 2.4 GHz de un
**dron-balón de juguete H18** (mando `HW-L0712TG`, receptor `HY-HW-L0717R13`,
chip de radio lijado) usando un **ESP32-S3 + nRF24L01+** como transmisor y el PC
como cerebro.

Se partió de un chip sin marcas y un protocolo desconocido, y se llegó a
**pilotar el dron completo desde el teclado del PC** (throttle, roll, pitch, yaw,
despegue y aterrizaje). El objetivo siguiente es el modo **semiautónomo** con un
IMU (MPU-6050) a bordo para maniobras programadas en lazo cerrado.

## Qué se descifró

- **Modulación:** XN297, 1 Mbps.
- **Dirección de control:** `2A A4 B5 2D 24`; canales de salto `70,73,75,76,77,79,84`.
- **Payload de control (12 B):** `00 00 B5 | throttle roll pitch yaw | 00 flags 00 AA AA`
  (throttle centrado en `0x7A`; flags `0x44` = despegue/aterrizaje toggle).
- **Baliza de sync** y **paquete de BIND** (fijo, difundido en canales bajos al
  encender el mando) necesarios para que el dron enganche.
- Detalle completo en [`research/h18-protocol.md`](research/h18-protocol.md).

## Hardware

- Placa transmisora: **ESP32-S3** (YD-ESP32-S3) — también vale otro ESP32.
- Radio: **nRF24L01+** (SPI, 3.3 V, condensador de 10 µF en la alimentación).
- Cableado en [`WIRING.md`](WIRING.md).

## Estructura

```
firmware/
  test_nrf24/     diagnóstico del módulo + escáner de banda 2.4 GHz
  nrf24_multi/    probador de ~16 protocolos de juguete (port de goebish a ESP32)
  nrf24_sniff/    sniffer promiscuo XN297 (preámbulo 55 0F 71) + barrido
  nrf24_tx/       TRANSMISOR final: bind + control del H18, comandos por serie
tools/
  dron_control.py control por teclado en vivo (flechas + auto-centrado)
  scan_capture.ps1 captura/promedia barridos del sniffer
research/         protocolo descifrado y notas; informe de drones de show
captures/         datos crudos de barridos
```

## Uso rápido

1. Cablea el nRF24L01+ (ver `WIRING.md`) y flashea `firmware/nrf24_tx` en el ESP32-S3
   (`arduino-cli`, core `esp32:esp32`, **compilar con `-fcommon`**).
2. Enciende el dron (parpadeando) y lanza el control:
   ```
   py -3.11 tools/dron_control.py
   ```
3. `B` (bind) → `b` (despegue) → flechas (dirección), `w/s` (altura), `a/d` (yaw),
   `L` (aterrizar). Prueba siempre con hélices fuera primero.

## Créditos

El firmware usa y porta trabajo de estos proyectos (GPLv3), gracias a sus autores:

- **[goebish/nRF24_multipro](https://github.com/goebish/nrf24_multipro)** — implementaciones de
  los protocolos de juguete y la emulación XN297 (base de `nrf24_multi` y de la capa XN297 del TX).
- **[pascallanger/DIY-Multiprotocol-TX-Module](https://github.com/pascallanger/DIY-Multiprotocol-TX-Module)**
  — referencia del volcador XN297 y del CRC/scramble.
- **[Yveaux / michael-betz nRF24L01-sniffer](https://github.com/yetifrisstlama/nRF24L01-sniffer)**
  — técnica de recepción promiscua del nRF24.

## Licencia

**GPLv3** (ver [`LICENSE`](LICENSE)), por derivar de los proyectos GPLv3 citados.

## Aviso

Proyecto educativo / de hobby, sobre hardware propio. Respeta la normativa de radio
y de drones de tu país. Úsalo bajo tu responsabilidad.
