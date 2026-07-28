# REC Time Monitor 2.1.0

Nova interfície d'RTM2 per integrar el control del monitor directament a OBS.

## Novetats

- Acoblador compacte `rtm2`, sense entrades duplicades al menú Eines.
- Interfície disponible en català i anglès.
- Temps de gravació i compte enrere fins a la marca següent.
- Monitor ON/OFF amb candau contra tancaments accidentals.
- Selector de monitor, mode de pantalla completa i finestra flotant al 70%.
- Recuperació automàtica de l'última sortida disponible en iniciar OBS.
- Marques amb timecode editable, ajust accelerat amb les fletxes, color,
  durada i text d'avís.
- Finestra de missatges més compacta amb frases preparades persistents,
  editables directament amb doble clic.
- Ajustos de proporcions, tipografia i velocitat d'obertura dels diàlegs.

## Descàrregues

La release inclou ZIPs per a macOS universal i Windows x64. Cada plataforma
té un fitxer amb la versió `2.1.0` i un àlies `latest-version` amb el mateix
contingut.

## Instal·lació

### macOS

Descomprimeix el ZIP i copia `rec-time-monitor.plugin` a:
`~/Library/Application Support/obs-studio/plugins/`

### Windows

Descomprimeix el ZIP i copia la carpeta `rec-time-monitor` a:
`%ProgramData%\obs-studio\plugins\`

Reinicia OBS Studio després de la instal·lació.

## Llicència

REC Time Monitor es distribueix sota GNU GPL v2 o qualsevol versió posterior
(`GPL-2.0-or-later`).
