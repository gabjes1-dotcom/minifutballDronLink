#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Control en vivo por teclado del dron H18, via el transmisor ESP32 (firmware nrf24_tx).
# El ESP32 ya hace el XN297/bind/control; aqui solo mandamos sus comandos por serie.
#
# Uso (Windows):  py -3.11 dron_control.py        (o ajusta PORT abajo)
# Requiere pyserial:  py -3.11 -m pip install pyserial
#   (si Norton rompe pip:  py -3.11 -m pip install --trusted-host pypi.org --trusted-host files.pythonhosted.org pyserial)

import sys, time
try:
    import serial
except ImportError:
    print("Falta pyserial. Instala con:  py -3.11 -m pip install pyserial")
    sys.exit(1)
try:
    import msvcrt   # teclado en Windows, sin dependencias
except ImportError:
    print("Este control por teclado es para Windows (usa msvcrt).")
    sys.exit(1)

PORT = "COM11"
BAUD = 115200

# tecla -> comando de una letra del firmware nrf24_tx
KEYMAP = {
    'w': 'w', 's': 's',          # throttle +/-
    'x': 'x', 'z': 'z',          # throttle centro / minimo
    'j': 'j', 'l': 'l',          # roll izquierda / derecha
    'i': 'i', 'k': 'k',          # pitch adelante / atras
    'a': 'a', 'd': 'd',          # yaw izquierda / derecha
    ' ': ' ',                    # centrar roll/pitch/yaw
    'b': 'b',                    # DESPEGUE (pulso)
    'n': 'n',                    # neutral total
    'g': 'g', 'o': 'o', 'v': 'v' # baliza / tx on-off / estado
}
# mayusculas: B = re-bind, L = aterrizar
EXTRA = {'B': 'B', 'L': 'L'}

# FLECHAS (msvcrt las da como prefijo 0x00/0xE0 + scancode):
#   arriba/abajo = adelante/atras (pitch i/k), izq/der = lados (roll j/l)
ARROWS = { b'H': 'i', b'P': 'k', b'K': 'j', b'M': 'l' }

HELP = """
==================  CONTROL H18 EN VIVO  ==================
  Secuencia: dron encendido y parpadeando -> pulsa  B  (bind)
             luego  b  (despegue) y  w  para subir.
  --------------------------------------------------------
  FLECHAS  arriba/abajo = adelante/atras   izq/der = lados
  w / s   throttle subir / bajar   x centro(hover)   z minimo
  a / d   yaw izq / der            ESPACIO  centrar ejes
  b       DESPEGUE suave   L ATERRIZAR   B re-bind   n neutral
  v       estado           q SALIR (throttle a minimo)
==========================================================
"""

def main():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.05)   # apertura normal (resetea el ESP32, probado OK)
    except Exception as e:
        print(f"No pude abrir {PORT}: {e}")
        print("Cierra cualquier monitor serie y revisa el puerto.")
        return
    time.sleep(0.3)
    print(HELP)
    print(f"Conectado a {PORT}. Pulsa teclas para volar. 'q' para salir.\n")
    try:
        while True:
            if msvcrt.kbhit():
                raw = msvcrt.getch()
                if raw in (b'\x00', b'\xe0'):      # tecla especial = flecha
                    code = msvcrt.getch()
                    cmd = ARROWS.get(code)
                    if cmd:
                        ser.write((cmd + '\r\n').encode())
                    continue
                try:
                    ch = raw.decode('latin-1')
                except Exception:
                    ch = ''
                if ch == 'q':
                    ser.write(b'z\r\n')   # throttle a minimo al salir
                    break
                cmd = EXTRA.get(ch) or KEYMAP.get(ch)
                if cmd:
                    ser.write((cmd + '\r\n').encode())
            # eco de lo que responde el ESP32
            data = ser.read(256)
            if data:
                sys.stdout.write(data.decode('latin-1', errors='ignore'))
                sys.stdout.flush()
            time.sleep(0.005)
    except KeyboardInterrupt:
        ser.write(b'z\r\n')
    finally:
        ser.close()
        print("\nSaliste. Throttle a minimo enviado.")

if __name__ == "__main__":
    main()
