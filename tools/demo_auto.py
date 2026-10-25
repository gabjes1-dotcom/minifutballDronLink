#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Demo AUTONOMO del dron H18 para grabar (Facebook/Instagram).
# Coreografia open-loop por el transmisor ESP32 (firmware nrf24_tx):
#   bind -> despegue -> estabilizar -> adelante/atras/adelante (vuelve al inicio)
#   -> derecha/izquierda/derecha (vuelve al inicio) -> giro 360 -> aterrizar.
#
# Uso:  py -3.11 demo_auto.py
#   1) Enciende el DRON (parpadeando) con HELICES puestas y espacio libre.
#   2) Lanza este script; da una cuenta atras para empezar a grabar.
#   3) Ctrl+C en cualquier momento = ATERRIZAR (seguridad).
#
# AJUSTA los tiempos/conteos abajo segun como responda tu dron y tu bateria.

import sys, time, subprocess
try:
    import serial
except ImportError:
    print("Falta pyserial:  py -3.11 -m pip install pyserial"); sys.exit(1)

PORT = "COM11"
BAUD = 115200

# ---- Parametros ajustables -------------------------------------------------
PULSE_INTERVAL = 0.50   # s entre "pulsaciones" de stick (cada una ~deflexion+recentrado)
STABILIZE      = 3.0    # s de hover tras despegar
PHASE_PAUSE    = 1.2    # s de hover entre fases
SPIN_SECONDS   = 3.5    # s de giro continuo para ~360 (ajusta hasta que de una vuelta)
SPIN_INTERVAL  = 0.30   # s entre pulsos de yaw durante el giro
BIND_WAIT      = 2.5    # s tras bind para que enganche
TAKEOFF_WAIT   = 3.5    # s tras despegue

# Coreografia (conteo de pulsaciones). Suma neta = 0 -> vuelve al inicio.
FWD1, BACK, FWD2   = 2, 4, 2       # adelante / atras / adelante (vuelve al inicio)
RIGHT1, LEFT, RIGHT2 = 2, 4, 2     # derecha / izquierda / derecha (vuelve al inicio)

VOICE = True   # narracion por voz (Windows SAPI + SSML, sin dependencias)

# estilos de prosodia: (rate, pitch, volume)
STYLES = {
    "normal":  ("+12%", "+10%", "95"),    # animado
    "excited": ("+25%", "+30%", "100"),   # cuenta atras / maniobras
    "hype":    ("+45%", "+55%", "100"),   # LIFTOFF y momentos cumbre
}

def _xml(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def say(text, style="normal"):
    print(f"  [voz:{style}] {text}")
    if not VOICE:
        return
    rate, pitch, vol = STYLES.get(style, STYLES["normal"])
    ssml = ('<speak version="1.0" xmlns="http://www.w3.org/2001/10/synthesis" xml:lang="es-ES">'
            f'<prosody rate="{rate}" pitch="{pitch}" volume="{vol}">{_xml(text)}</prosody></speak>')
    ssml_ps = ssml.replace("'", "''")   # escapar comillas para PowerShell
    ps = ("Add-Type -AssemblyName System.Speech;"
          "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;"
          "$v=$s.GetInstalledVoices()|?{$_.VoiceInfo.Culture.Name -like 'es*'}|select -First 1;"
          "if($v){$s.SelectVoice($v.VoiceInfo.Name)};"
          f"$s.SpeakSsml('{ssml_ps}')")
    try:
        subprocess.Popen(["powershell", "-NoProfile", "-Command", ps],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass

ser = None
def send(cmd, note=None):
    if note: print(f"  -> {note}")
    ser.write((cmd + "\r\n").encode())

def pulses(cmd, n, interval=PULSE_INTERVAL, label=""):
    print(f"  {label} x{n}")
    for _ in range(n):
        ser.write((cmd + "\r\n").encode())
        time.sleep(interval)

def land_and_exit(msg="ATERRIZANDO"):
    print(f"\n{msg}...")
    try:
        ser.write(b"L\r\n"); time.sleep(1.0)
        ser.write(b"z\r\n")   # throttle a minimo
    except Exception:
        pass

def main():
    global ser
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.05)
    except Exception as e:
        print(f"No pude abrir {PORT}: {e}  (cierra otros programas que usen el puerto)"); return
    time.sleep(0.4)

    print("="*54)
    print("  DEMO AUTONOMO H18  -  dron encendido (parpadeando),")
    print("  HELICES puestas, espacio libre. Ctrl+C = aterrizar.")
    print("="*54)
    print("\n  Prepara la camara... empieza a GRABAR!")
    say("Prepara la camara. Empieza a grabar."); time.sleep(5.0)

    try:
        send("B", "BIND (enganchando)")
        say("Control de mision a Dron Minifutbol. Enlazando con el vehiculo."); time.sleep(BIND_WAIT)
        say("Enlace confirmado. Todos los sistemas en verde.", "excited"); time.sleep(1.6)

        print("\n[ Cuenta regresiva ]")
        say("Comienza la cuenta regresiva final.", "excited"); time.sleep(1.6)
        for word in ["T menos cinco", "cuatro", "tres", "dos", "uno"]:
            print(f"  {word}"); say(word + "!", "excited"); time.sleep(1.0)
        say("Ignicion!", "excited"); time.sleep(0.7)

        print("\n[ Despegue ]")
        send("b", "DESPEGUE")
        say("Y despegue! Tenemos despegue del Dron Minifutbol!", "hype"); time.sleep(TAKEOFF_WAIT)

        send(" ", "estabilizar (centrar)")
        say("Vehiculo estable y nivelado. Firme como colibri con tres cafes.", "excited"); time.sleep(STABILIZE)

        print("\n[ Adelante / Atras / Adelante ]")
        say("El vehiculo cabecea al frente, con valentia.")
        pulses("i", FWD1, label="ADELANTE");  send(" "); time.sleep(PHASE_PAUSE)
        say("Y marcha atras... que olvide apagar la plancha.")
        pulses("k", BACK, label="ATRAS");     send(" "); time.sleep(PHASE_PAUSE)
        say("Correccion de trayectoria, de nuevo al frente, como si nada.")
        pulses("i", FWD2, label="ADELANTE");  send(" "); time.sleep(PHASE_PAUSE)

        print("\n[ Derecha / Izquierda / Derecha ]")
        say("Maniobra lateral a la derecha.")
        pulses("l", RIGHT1, label="DERECHA");   send(" "); time.sleep(PHASE_PAUSE)
        say("Y a la izquierda, bailando cumbia espacial.")
        pulses("j", LEFT,   label="IZQUIERDA"); send(" "); time.sleep(PHASE_PAUSE)
        say("Y a la derecha otra vez, para disimular.")
        pulses("l", RIGHT2, label="DERECHA");   send(" "); time.sleep(PHASE_PAUSE)

        print("\n[ Giro 360 sobre su eje ]")
        say("Iniciando rotacion completa! Trescientos sesenta grados de pura elegancia!", "excited")
        t0 = time.time()
        while time.time() - t0 < SPIN_SECONDS:
            ser.write(b"d\r\n"); time.sleep(SPIN_INTERVAL)
        send(" "); time.sleep(PHASE_PAUSE)

        print("\n[ Aterrizaje ]")
        say("Aproximacion final. Tren de aterrizaje... y contacto!", "excited")
        land_and_exit("ATERRIZANDO (fin del demo)")
        time.sleep(1.5)
        say("Mision cumplida. Dron Minifutbol en tierra. No olviden dar like y seguir!", "excited")
        print("\nDemo terminado. Ajusta tiempos/conteos arriba si hace falta.")
    except KeyboardInterrupt:
        land_and_exit("INTERRUPCION: aterrizando por seguridad")
    finally:
        time.sleep(0.5)
        ser.close()

if __name__ == "__main__":
    main()
