# Drones para shows de luces — informe para emprender

*Investigación 2026-10-23. Las cifras provienen en buena parte de páginas
de fabricantes/vendedores; trátalas como estimaciones, no como pruebas
independientes. Fuentes al final.*

---

## 0. Por qué tus drones (juguete 2.4 GHz) NO sirven para un show

Confirmado: son dos mundos distintos. Un show de luces no se trata de
"controlar muchos drones a la vez con un mando" — ningún piloto mueve las
palancas. Cada dron **vuela solo** una trayectoria pre-programada con una
precisión de **centímetros**, sincronizado al milisegundo con los demás.

| | Tu dron de juguete | Dron de show |
|---|---|---|
| Posición | Ninguna (vuela "a ojo" del piloto) | GPS **RTK/DGPS**, precisión **cm** |
| Control | Mando humano, 1 dron | Trayectoria autónoma cargada antes del vuelo |
| Sincronía | — | Reloj común, coreografía al ms |
| Seguridad | — | Geo-cerca, paracaídas, auto-retorno, chequeo de flota |
| Enlace | 2.4 GHz propietario (lo que estamos hackeando) | Telemetría + sincronía de enjambre, no "pilotaje" |

El reto de un show **no es la radio** (lo que estás aprendiendo con el
nRF24), sino **posicionamiento de precisión + software de enjambre +
seguridad**. Por eso tu proyecto DronLink es excelente para *aprender RF y
control*, pero el negocio de shows se construye sobre otra base tecnológica.
Lo que sí te llevas de DronLink: entender a fondo el enlace de control, que
te hará un operador mucho más competente.

---

## 1. Características típicas de un dron de show

- **Peso:** ~180–500 g (muy ligeros a propósito; p. ej. Intel Shooting Star
  <330 g, Mazzy Star 0.48 kg, XF S-3Pro 188 g). El poco peso es clave para
  seguridad y regulación (en EE. UU. las exenciones suelen exigir <0.75 lb
  ≈ 340 g por dron en vuelo simultáneo).
- **Posicionamiento:** **RTK** (multi-constelación, precisión centimétrica)
  o DGPS con estaciones base en tierra que mandan correcciones en tiempo real.
  Esto es lo que impide que choquen y que las figuras salgan nítidas.
- **LEDs:** RGB o RGBW, 16 millones de colores, brillo alto (cientos de
  lúmenes, p. ej. hasta 640 LM). Es la "carga útil" del dron.
- **Batería / autonomía:** 3000–6000 mAh; autonomía anunciada 18–25 min,
  pero **tiempo real de show 5–8 min** (Intel), porque la coreografía y el
  margen de seguridad acortan mucho el vuelo útil.
- **Tolerancia al viento:** baja, ~7 m/s. El clima cancela shows.
- **Velocidad de animación:** limitada (Intel ~3 m/s) por seguridad y por
  mantener la formación.

---

## 2. El sistema completo (esto es lo que de verdad compras)

El dron es solo una pieza. Un sistema de show incluye:

1. **Flota de drones** (idénticos, intercambiables).
2. **Software de diseño/coreografía:** se diseña la animación 3D, se simula,
   y el software genera la trayectoria de cada dron evitando colisiones.
3. **Estación de control en tierra (GCS):** un solo operador lanza y vigila
   toda la flota; hace chequeo automático pre-vuelo y descarta drones no
   óptimos.
4. **Estación(es) RTK** en tierra para las correcciones de posición.
5. **Carga y logística:** racks/docks de carga (los sistemas modernos cargan
   en base para vuelos encadenados), cajas de transporte, el sistema montado
   en vehículo reduce el despliegue (DAMODA afirma de 4 h/20 personas a
   60 s/1 operador con su sistema vehicular).

Clave de negocio: **se vende "llave en mano"**. El fabricante serio no te
vende drones sueltos, te vende flota + software + formación.

---

## 3. Mercado y fabricantes (quién domina)

Según un reporte QYResearch (difundido por la propia DAMODA, tómalo con
pinzas), la cuota global de **equipo** de shows:

- **DAMODA (Shenzhen)** — 36.42 %. Líder. Récord de **11,787 drones**
  (Chongqing, jun-2025); dice ser la única capaz de >10k drones en un show.
- **HighGreat (Shenzhen)** — 24.12 %. Además fabricante; +5,000 shows en
  300 ciudades (incl. Invierno Beijing 2022).
- **EFY Intelligent Control** — 8.75 %.
- **Shanghai Qianji** — 7.28 %.
- **EHang** — 3.24 %.

Operadores/flotas occidentales conocidos: **Verge Aero** (sistema X7,
certificación EASA), **UVify** (récord Guinness 5,293 drones, 2024),
**Nova Sky Stories** (+9,000 drones), **Lumasky** (~7,000), **Verity Studios**
(interiores/indoor, spin-off de ETH Zúrich). Intel fue pionero
("Shooting Star") pero ya no es el foco del mercado.

**Lectura para ti:** el hardware puntero es chino (Shenzhen). La vía de
entrada realista para un emprendedor no es fabricar drones, sino **comprar un
sistema llave en mano** (p. ej. a un fabricante chino o a Verge Aero) y
vender *shows* como servicio.

---

## 4. Coste y modelos de negocio

**Dos vías de entrada** (lo dice la guía de negocio de Verge, y tiene sentido):

- **A) Flota propia** — compras el sistema y produces shows. Inversión inicial
  típica **$30,000 – $500,000+** según tamaño de flota. Shows pequeños
  (20–50 drones) se montan con fleet de **cinco cifras**; 150+ drones ya es
  inversión de seis cifras.
- **B) Subcontratar** — vendes y produces el show pero rentas/subcontratas la
  flota a un operador con equipo. **Mucho menos capital**, ideal para arrancar
  y aprender el mercado antes de comprar.

**Lo que cobra el mercado (precio al cliente):**

- ~**$200–$300 por dron por show**, con mínimo típico de **100 drones** →
  base de **$20,000–$30,000** por un show pequeño.
- Rango amplio citado: **$75–$500 por dron** según gama y complejidad.
- Proyecto comercial medio: **$40,000–$150,000** total.
- Shows pequeños (20–50 drones): **$5,000–$15,000**.
- Recargos de **20–40 %** en festivos/temporada alta.

**Costes recurrentes:** baterías (consumibles, se degradan), seguros (coberturas
de seis cifras), mantenimiento de flota, nómina (piloto al mando + tripulación
+ observadores), transporte, y **trámites regulatorios** (en EE. UU. el papeleo
FAA puede tardar ~3 meses).

---

## 5. Regulación (el verdadero cuello de botella)

Esto es tan importante como el equipo. La regla base en casi todo el mundo:
**un piloto = un dron**. Un show de 100+ drones con un operador es una
**excepción** que hay que solicitar y justificar.

**Referencia EE. UU. (FAA, la más documentada):**
- Bajo Part 107 estándar, un piloto **no puede** controlar varios drones.
- Un show necesita **varias exenciones (waivers)** a la vez:
  - **§107.35** (operar múltiples UAS simultáneamente) — se concede con
    condiciones (geo-cerca, límites de peso ~<0.75 lb por dron).
  - **Vuelo nocturno** (los shows son de noche).
  - **Autorización de espacio aéreo** si es cerca de clase B/C/D o parte de E
    (coordinación con control de tráfico aéreo).
- **Tripulación exigida:** piloto al mando (RPIC) + otro tripulante certificado
  + **observadores visuales (~1 por cada 100 drones)**, escalado al tamaño.

**Si estás en México** (lo asumo por el contexto; confírmame tu país): la
autoridad es la **AFAC** (antes DGAC). El concepto es el mismo —necesitas
registro de las aeronaves, piloto habilitado y **autorización especial** para
operación de enjambre/nocturna—, pero los trámites y formatos son distintos a
los de la FAA. **Antes de invertir, confirma con tu autoridad local qué permite
y qué exige**; es lo que define si el negocio es viable en tu zona y cuánto
tiempo/coste añade cada show.

---

## 6. Recomendación para arrancar

1. **Decide país y revisa la regulación local primero** (AFAC si es México).
   Es lo que puede matar o habilitar el negocio; hazlo antes de gastar.
2. **Empieza por la vía B (subcontratar/revender)** para aprender el mercado,
   clientes y márgenes sin inmovilizar capital en una flota.
3. Pide **cotizaciones llave en mano** a 2–3 fabricantes (DAMODA/HighGreat
   chinos vs Verge Aero) para una flota de arranque de ~**100 drones** y
   compara: hardware + software + formación + repuestos + soporte.
4. **Modelo de ingreso:** shows a ~$200–300/dron; un show de 100 drones ≈
   $20–30k. Calcula cuántos shows/año necesitas para amortizar la flota.
5. Sigue con **DronLink** en paralelo: te da dominio real del enlace de
   radio y del control de vuelo, que te hará mejor operador y te permite
   prototipar/entender el hardware sin depender de nadie.

---

## Fuentes

- [The Drone Girl — cuánto cuesta un show (2025)](https://www.thedronegirl.com/2025/07/17/drone-light-show-cost/)
- [The Drone Girl — mayores empresas de shows](https://www.thedronegirl.com/2025/07/30/biggest-drone-show-companies/)
- [Verge Aero — Quick Start Guide (negocio de shows)](https://www.verge.aero/QuickStartGuide_DroneLightShowBusiness_VergeAero.pdf)
- [Verge Aero X7](https://www.verge.aero/x7)
- [Intel Drone Light Show — fact sheet (PDF)](https://www.intel.com/content/dam/www/public/us/en/documents/fact-sheets/intel-drone-light-show-fact-sheet.pdf)
- [UVify IFO](https://www.uvify.com/product/ifo)
- [Grepow — baterías y requisitos de potencia](https://grepow.com/blog/how-drone-light-shows-are-created-and-key-battery-power-requirements.html)
- [Pyrotecnico — arquitectura técnica de un show](https://pyrotecnico.com/news/what-is-a-drone-show/)
- [DAMODA — cuota de mercado (QYResearch, nota de prensa)](https://en.dmduav.com/news/167.html)
- [DAMODA — coste de montar un show](https://en.dmduav.com/news/99.html)
- [Jrupprecht Law — drone light shows y waivers](https://jrupprechtlaw.com/drone-industry/drone-light-shows/)
- [Wiley — regulatory play-by-play del show del Super Bowl (PDF)](https://sitepilot09.firmseek.com/client/wiley2/www/printpilot-publication-A-Regulatory-Play-By-Play-Of-The-Super-Bowl-Drone-Show.pdf)
- [FAA — certificado de exención 2026 (ejemplo)](https://www.faa.gov/sites/faa.gov/files/107W-2026-02535-Tiffany-Norton-CoW.pdf)
