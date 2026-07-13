# REC Time Monitor — plugin natiu d'OBS

Plugin natiu (C++/Qt) per OBS Studio. Obre una finestra auxiliar, independent
de qualsevol escena, que mostra el **Programa** (el mix final que reben la
gravació i l'streaming, via `obs_render_main_texture()`) amb un cronòmetre de
`REC`/`LIVE` superposat per Qt. Aquesta capa es pinta directament a la
finestra, fora del graf de render d'OBS, així que **mai queda enregistrada ni
emesa** — només és visible per qui miri aquest monitor.

Pensat per obrir aquesta finestra a pantalla completa en un monitor extern de
cara a l'equip de plató, perquè vegin quant temps porten de programa.

## Ús

1. A OBS, ves a `Tools → Monitor de Programa + Cronòmetre`.
2. Mou la finestra que s'obre a un monitor extern (arrossega-la i posa-la en
   pantalla completa, `⌃⌘F` amb la finestra activa, o el botó verd de la
   cantonada de la finestra).
3. El badge mostra `REC` (vermell) mentre graves, `PAUSA` si la gravació està
   en pausa, `LIVE` (taronja) si només fas streaming, o `STANDBY` (gris) si no
   passa res.
4. Clic dret sobre la imatge del projector per accedir a la configuració o
   tancar la finestra.

## Configuració

`Tools → Configuració del Monitor de Programa` (o clic dret sobre el
projector) obre un panell que aplica els canvis a l'instant, sense botó
"Aplicar":

- **Avisos de temps**: llista d'avisos, cadascun amb el seu propi temps
  (format gran "00h 00m 00s" — clic a la meitat superior d'un número per
  pujar-lo, a la inferior per baixar-lo, sense caselles ni botons) i el seu
  propi color. En arribar-hi, el marc de la imatge canvia al color d'aquell
  avís; si se'n superen diversos, mana el més recent assolit. Botó
  "+ Afegeix un avís" per crear-ne més, "✕" per eliminar-ne.
- **Posició** del cronòmetre: les 4 cantonades o centrat a dalt/a baix.
- **Mida** del cronòmetre: 100%–300%.

La configuració es desa a
`~/Library/Application Support/obs-studio/plugin_config/rec-time-monitor/settings.ini`
(a Windows, la carpeta equivalent dins `%APPDATA%\obs-studio\plugin_config\`).

## Compilar i instal·lar (macOS, Apple Silicon)

```sh
./build-macos.sh
```

Això compila el plugin i el copia a
`~/Library/Application Support/obs-studio/plugins/rec-time-monitor.plugin`.
Cal **reiniciar OBS** perquè el carregui (o el detecti per primer cop).

Nota: el build s'ha de fer fora d'aquesta carpeta (sincronitzada amb iCloud
Drive) — per això l'script compila a `/tmp` i només copia el resultat final
aquí/a OBS. Els fitxers dins d'iCloud Drive porten metadades que fan fallar
`codesign`.

### Instal·lador `.pkg` (macOS)

```sh
./build-macos-installer.sh
```

Compila el plugin i genera `dist/REC-Time-Monitor-macOS.pkg`: un instal·lador
de doble clic que copia el plugin a
`~/Library/Application Support/obs-studio/plugins/` sense demanar contrasenya
d'administrador (instal·la només a la carpeta de l'usuari actual, domini
"Current User Home"). Verificat funcionant (`installer -pkg ... -target
CurrentUserHomeDirectory`).

El plugin i el `.pkg` es signen automàticament si al clauer hi ha els
certificats "Developer ID Application" i "Developer ID Installer" (equip
MOIZ I BARTRA PRODUCCIONS SL, B5DRNCGUYN) — si no hi són, cauen a un build
sense signar (aleshores cal clic dret → Obrir en comptes de doble clic).

Signar no n'hi ha prou: per evitar del tot l'avís de Gatekeeper cal també
**notaritzar-ho**:

```sh
./notarize-macos.sh
```

Necessita una configuració d'un sol cop (a fer directament al Terminal, mai
enganxant la contrasenya en un xat/IA):

```sh
xcrun notarytool store-credentials "rec-time-monitor-notary" \
  --apple-id "el-teu-apple-id@exemple.com" \
  --team-id "B5DRNCGUYN"
```

Demanarà una contrasenya específica d'app (es genera a
appleid.apple.com → "Inici de sessió i seguretat" → "Contrasenyes específiques
d'app") i la desa xifrada al clauer sota el nom de perfil
`rec-time-monitor-notary`; `notarize-macos.sh` ja el fa servir automàticament.

### Per què les versions de `buildspec.json` estan fixades a 30.2.3 / 2024-05-08

El plugin es compila contra les capçaleres i llibreries (`libobs`,
`obs-frontend-api`, Qt6) de la **mateixa versió d'OBS instal·lada** en aquesta
màquina (OBS 30.2.3), no contra la versió més recent que ve per defecte a la
plantilla. Vam detectar que compilar contra un Qt6 més nou (6.8.x, de la
plantilla original) i carregar-ho dins d'un OBS que porta Qt 6.6.3 provoca un
error de símbol no trobat en temps d'execució (`QPaintDevice::devicePixelRatio`).
Si algun dia actualitzes OBS a una versió major, actualitza també les versions
i hashes de `buildspec.json` (es poden treure del `buildspec.json` del tag
corresponent a <https://github.com/obsproject/obs-studio>) i torna a compilar.

## Estructura del codi

- `src/plugin-main.cpp` — registre del mòdul i de les entrades de menú.
- `src/time-tracker.{hpp,cpp}` — segueix els events de frontend
  (`OBS_FRONTEND_EVENT_RECORDING_*`, `..._STREAMING_*`) i calcula el temps
  transcorregut (amb gestió de pauses), sense dependre de cap altre plugin.
- `src/program-display.hpp` + `src/program-display.cpp` — widget Qt que
  incrusta un `obs_display_t` i hi dibuixa el Programa
  (`obs_render_main_texture`) amb "letterboxing", més el badge i el marc
  d'avís com a textures GPU. Multiplataforma excepte `CreateDisplay()` (el
  tros que dona a OBS el handle de finestra nativa), que viu a part:
  - `src/program-display-mac.mm` — versió macOS (`NSView`).
  - `src/program-display-win.cpp` — versió Windows (`HWND`); **sense provar
    en una màquina Windows real** (vegeu més avall).
- `src/program-monitor-dialog.{hpp,cpp}` — la finestra: cada 200 ms calcula
  l'estat (color/text/temps del badge, quin avís toca activar i de quin
  color) i ho envia a `OBSProgramDisplay`. També el menú contextual (clic
  dret).
- `src/badge-image.{hpp,cpp}` — dibuixa el badge (Qt `QPainter`) a una
  `QImage` amb fons transparent, a mida fixa segons `Settings::ScalePercent`.
- `src/time-unit-dial.{hpp,cpp}` — el número gran "00" clicable (meitat
  superior = +1, inferior = −1) que fa servir `BigTimeEditor`.
- `src/big-time-editor.{hpp,cpp}` — combina tres `TimeUnitDial` (h/m/s) per
  triar un temps sense caselles de text ni botons.
- `src/settings.{hpp,cpp}` — configuració persistent (`QSettings`): la
  posició/mida són atòmiques perquè es llegeixen des del fil de vídeo d'OBS;
  la llista d'avisos només es toca des del fil de la UI de Qt.
- `src/settings-dialog.{hpp,cpp}` — el panell de configuració, amb les files
  d'avisos generades dinàmicament.

El badge (i el marc de l'avís) **no són widgets de Qt superposats a sobre de
la finestra**: es pugen com a textura GPU i es dibuixen dins del mateix
`draw callback` que el vídeo (`program-display.cpp`). Superposar
finestres/widgets de Qt sobre la superfície nativa d'OBS es va provar primer
i sempre acabava en problemes de composició (fotogrames vells que no
s'esborraven, finestres que no es mantenien sincronitzades amb el projector);
dibuixar-ho tot al mateix fotograma GPU els evita per complet.

## Windows

El codi hauria de compilar a Windows (el `CMakePresets.json` ja porta un
preset `windows-x64`, i `buildspec.json` apunta a les mateixes versions
d'OBS/Qt6 que macOS, 30.2.3 / 2024-05-08), però **no s'ha compilat ni provat
mai en una màquina Windows real** — tot el desenvolupament d'aquest plugin
s'ha fet en un Mac, iterant contra errors que només apareixen en compilar i
executar de veritat (problemes de linkatge, de composició de finestres, un
crash en tancar...). És molt probable que `program-display-win.cpp` necessiti
ajustos similars la primera vegada que es compili i es provi a Windows. Quan
hi hagi una màquina Windows disponible, cal:

1. `cmake --preset windows-x64` (Visual Studio 17 2022) i
   `cmake --build --preset windows-x64`.
2. Copiar el `.dll` resultant a
   `%APPDATA%\obs-studio\plugins\rec-time-monitor\bin\64bit\`.
3. Depurar el que calgui — probablement el primer intent no carregarà a la
   primera.

### Instal·lador `.exe` (Windows) — script preparat, sense provar

`installer/windows/rec-time-monitor.iss` és un script d'[Inno
Setup](https://jrsoftware.org/isinfo.php) que empaqueta el resultat de
`cmake --build --preset windows-x64` (la carpeta
`build_x64\rundir\RelWithDebInfo\rec-time-monitor\`, que ja té l'estructura
`bin\64bit\` + `data\` correcta) en un instal·lador que el copia a
`%APPDATA%\obs-studio\plugins\rec-time-monitor\`.

Com que Inno Setup és una eina de Windows, aquest `.iss` **no s'ha compilat
mai** (només se n'ha revisat la sintaxi a mà) — no hi ha manera de generar ni
provar l'`.exe` sense una màquina Windows. Un cop hi hagi el `.dll` compilat:

```sh
cmake --preset windows-x64
cmake --build --preset windows-x64
cd installer\windows
iscc rec-time-monitor.iss
```

L'instal·lador (`installer\windows\Output\REC-Time-Monitor-Windows-Setup.exe`)
tampoc estarà signat, així que Windows Defender/SmartScreen probablement
avisarà de "editor desconegut" la primera vegada.
