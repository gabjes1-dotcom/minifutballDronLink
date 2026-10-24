# Protocolo del dron-balón H18 (mando L0712 / receptor L0717)

## ✅ RESUELTO — el dron engancha con nuestra placa (2026-10-23)
Secuencia que FUNCIONA (firmware `firmware/nrf24_tx`):
1. Encender el DRON primero (queda esperando, LED parpadeando). Mando real apagado.
2. Nuestra placa difunde la **rafaga de BIND** (paquete fijo de 17 bytes, CRC XN297
   valido) en canales bajos {17,20,24,28,32,36,40,44,48,52} durante ~700 ms.
   BIND_PKT = A9 02 D2 5F 57 8E 37 3A 4F 32 8C 88 12 3C BB 18 C5
3. Luego pasa a CONTROL (addr 2A A4 B5 2D 24) + BALIZA, en canales {70,73,75,76,77,79,84}.
=> El dron engancha (LED fijo). CLAVE: el ORDEN (dron primero) y la rafaga de bind,
   que el mando solo emite al encenderse y que no estaba en el estado estable.

### CONTROL DE VUELO COMPLETO confirmado (2026-10-23)
Desde el PC el dron responde a los 4 ejes + despegue:
- Throttle (byte3): los motores arrancan y la velocidad sube con el throttle.
- Roll (byte4): cambian los motores de un lado.
- Pitch (byte5): cambian adelante/atras.
- Yaw (byte6): cambian en diagonal.
- Despegue (flag byte8=0x44): arranca motores.
El altitude-hold apaga los motores tras un rato sin ganar altura (seguridad).

### PARAR/ATERRIZAR = TOGGLE del mismo flag de despegue (RESUELTO 2026-10-23)
Confirmado por el usuario: el boton one-button es un TOGGLE. En el suelo = despega;
VOLANDO = aterriza/para. O sea, el MISMO flag de despegue (byte8 bit 0x04 -> 0x44)
pulsado de nuevo en vuelo = aterrizar y cortar motores. => Nuestro comando 'b'
(pulso de 0x44) ya sirve de despegue Y de aterrizaje/stop (segun el estado de vuelo).
Throttle a minimo (0x00) NO corta (en altitude-hold = "bajar", no "parar"); el stop
real es el toggle 'b' estando en vuelo.


Ingeniería inversa con nRF24L01+ en modo sniffer XN297 (preámbulo 55 0F 71),
descifrado offline con `scratchpad/xn297_decode.py`.

## Confirmado (validado por CRC XN297)

- **Chip:** familia nRF24/XN297 (cristal 16 MHz; descartados CC2500 y CYRF6936).
- **Modulación:** XN297.
- **Tasa:** 1 Mbps (2M y 250k dan 0 paquetes).
- **Canales de salto vistos:** 70, 73, 75, 76, 77, 79, 84 (RF ch; 2470–2484 MHz).
  Falta confirmar el conjunto y el orden completos.
- **Dirección (fija):** termina en `… B5 2D 24`. Longitud por confirmar:
  - AW=3 → `B5 2D 24`
  - AW=4 → `A4 B5 2D 24`
  - AW=5 → `2A A4 B5 2D 24`
  (El CRC cubre dirección+payload juntos, así que no distingue la frontera; se
  determina viendo qué bytes son fijos (dirección) vs. cambiantes (control).)
- **Payload en REPOSO (sticks quietos), 14 bytes con AW=3:**
  `25 54 00 00 B5 7A 20 20 20 00 40 00 AA AA`
- **Tamaño:** dirección+payload = 17 bytes, + 2 bytes CRC.
- Ninguno de los 16 protocolos de goebish enlazó (protocolo fuera de ese set).

## Pendiente

1. Mapear los bytes de control: capturar moviendo cada palanca (throttle,
   aileron, elevator, rudder) y botones; los bytes que cambian = control.
2. Confirmar longitud de dirección y conjunto/orden de canales de salto.
3. Firmware TX: XN297 1M, esta dirección, salto por estos canales, enviar el
   payload replicado con los bytes de control modificados → controlar desde PC.

## Avance mapeo (2026-10-23)

- **Dirección confirmada: `2A A4 B5 2D 24`** (AW=5), en 284/284 paquetes válidos.
- Payload (12 bytes con AW=5). Fijos: bytes 0-2 = `00 00 B5`, bytes 7-11 = `00 40 00 AA AA`.
  **El control está en los bytes 3-6.**
  - Reposo:       `00 00 B5 | 7A 20 20 20 | 00 40 00 AA AA`
  - Throttle máx: `00 00 B5 | 00 01 04 01 | 00 40 00 AA AA` (byte4 oscila 01-09)
- **Byte 3 = THROTTLE**, centrado (altitude hold): centro `7A`, abajo `00`,
  arriba ~`FF` (arriba por reconfirmar). Throttle-abajo: `00 00 B5 00 20 20 20 00 40 00 AA AA`.
- **Byte 4 = ROLL/aileron** (centro `20`, derecha sube: der=`3B`). 
- **Byte 5 = PITCH/elevator** (centro `20`, adelante sube: `3E`).
- **Byte 6 = YAW/rudder** (centro `20`, derecha baja: `01`).

### MAPA COMPLETO de sticks (payload AW=5, 12 bytes)
`00 00 B5 | TT RR PP YY | 00 40 00 AA AA`
- byte3 TT = throttle (centro 7A, abajo 00, arriba ~FF)
- byte4 RR = roll    (centro 20, der 3B)
- byte5 PP = pitch   (centro 20, adelante 3E)
- byte6 YY = yaw     (centro 20, der 01)
- bytes 0-2 = `00 00 B5` fijos; bytes 7-11 = `00 40 00 AA AA` (flags/trim).
- **Byte 8 = FLAGS**: normal `40`, **despegue/armado = `44`** (bit 0x04).
  (Durante la pulsacion del boton, byte2 tambien cambia B5->00.)

## RESUMEN FINAL (protocolo descifrado)
- XN297, 1 Mbps, direccion `2A A4 B5 2D 24`, canales {70,73,75,76,77,79,84}.
- Payload 12 bytes: `00 00 B5 | TT RR PP YY | 00 FL 00 AA AA`
  TT=throttle(7A centro), RR=roll(20), PP=pitch(20), YY=yaw(20), FL=flags(40; 44=despegue).
- CRC XN297 (init 0xb5d2, poly 0x1021, tabla xorout). Preambulo 55 0F 71.
- Pendiente menor: reconfirmar throttle arriba (~FF) y orden/timing del salto de canales.

## HALLAZGO CLAVE: el mando manda DOS tramas (2026-10-23)
El mando emite dos tipos de trama intercalados ~50/50 (ambos presentes con el
dron apagado, o sea los emite el MANDO):
- **Tipo A = CONTROL** (direccion 2A A4 B5 2D 24, CRC valido): lo replicamos byte a byte.
- **Tipo B = BALIZA DE SYNC/BIND** (direccion `0x00`, paquete corto ~9 bytes en aire):
  payload descifrado de ~4 bytes que CICLA entre 4 estados (contador rodante) + 1 byte
  tipo CRC. NO valida con el CRC XN297 estandar (quiza enhanced / otro CRC).
  Payload descifrado (p0-p3) observado, 4 estados: `80 95 7B 9A`, `80 84 73 9B`,
  `00 9D FF 9A`, `00 8C F7 9B`.

### DIAGNOSTICO DEL ENLACE
Nuestra TX envia SOLO la trama de control -> el dron la recibe (hace "ciclo de
enganche") pero NO se engancha (LED sigue parpadeando) porque le falta la BALIZA.
Verificado: nuestro paquete de control es identico byte a byte al real.
Falta: generar/replicar la Tipo B (baliza con codigo rodante) e intercalarla con
el control en la TX. Caminos: (1) replay crudo de las 4 balizas, (2) decodificar
el contador+CRC de la baliza.
- Bytes 7-11 fijos = `00 40 00 AA AA` (flags/trim; byte8=0x40). El botón de
  despegue/altitude-hold probablemente togglea un flag aquí (pendiente).

## Datos crudos de referencia (CH75, mando+dron enlazados, reposo)

Trama de control (TIPO A), repetida idéntica (tras preámbulo 55 0F 71):
`C7 9C FE 4E AF BC E5 CB 53 AA 88 8C 12 6B EE 4A 92 87 61 AF` (+ cola de ruido)
→ descifrado: payload `25 54 00 00 B5 7A 20 20 20 00 40 00 AA AA`
