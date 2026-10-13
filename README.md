# Synthgirl + Plaits

> **Fork von [Randomwaves/Synthgirl](https://github.com/Randomwaves/Synthgirl)** mit der Klangerzeugung von Mutable Instruments **Plaits** als zusätzlicher Engine.
> *English summary: this fork adds Mutable Instruments Plaits (24 synthesis models) as a sound engine to the Synthgirl firmware, controllable from the touch surface and via MIDI CC 20–27. Experimental test firmware.*

Die Änderungen liegen im Branch [`plaits`](https://github.com/overnetneo/Synthgirl_plaits/tree/plaits). `main` entspricht dem Original von Randomwaves.

**Status: Testfirmware, noch nicht vollständig auf der Hardware getestet.** Die Modelle und die CPU-Last werden gerade am Gerät geprüft. Zurück zum Original geht jederzeit über das SD-Update mit `Synthgirl_Fw_01_05.bin`, der Bootloader wird nicht verändert.

## Was ist neu

- **Plaits-Engine:** alle 24 Modelle von Plaits 1.2 als monophone Stimme. Sie ersetzt die Wavetable-Oszillatoren, solange sie aktiv ist. EQ, Filter, Effekte und Reverb des Synthgirl wirken weiter.
- **Plaits-Seite im Hauptmenü** mit acht Parametern, Bedienung über das Touchfeld
- **MIDI-CC 20–27** für alle Plaits-Parameter
- **Stimmung korrigiert:** Der I2S-Takt kommt jetzt aus PLL3 und liefert exakt 44,1 kHz. Vorher waren es 44,64 kHz, alles klang etwa 21 Cent zu hoch. Das betrifft auch die originalen Wavetable-Sounds.
- **Kommandozeilen-Build** per `Makefile` (ohne STM32CubeIDE) und `pack_firmware.py` für SD-Update-Dateien

## Installation

1. Die SD-Karte braucht die normale Synthgirl-Struktur: `System/` (mit `Data`, `File`, `Firmware`, `Image`, `Synthkit`) und `Wavetable/`, siehe `02 Software/Sd Card`.
   - Der Ordner `System/Firmware` muss vorhanden sein, auch wenn er leer ist. Git speichert keine leeren Ordner, deshalb fehlt er im Repo und muss von Hand angelegt werden. Sonst meldet das Gerät „Firmware Folder Error“.
2. `02 Software/Firmware/Synthgirl_Fw_01_91.bin` ins Hauptverzeichnis der Karte kopieren. Es darf keine weitere `Synthgirl_Fw_*.bin` dort liegen.
3. Einschalten. Der Bootloader zeigt „UPDATING FIRMWARE V1.91“.
4. Die `.bin` danach wieder von der Karte löschen, sonst wird bei jedem Start neu geflasht.

## Bedienung

Plaits ist nach dem Einschalten aktiv. Tastatur, Oktave, MIDI-Noten und die Song-Wiedergabe spielen Plaits.

- **Plaits-Seite öffnen:** Das Touchfeld in der Mitte der Pfeiltasten lang drücken öffnet das Hauptmenü (TEM, MEA, BAR …). Dort nochmal lang drücken: Oben links erscheint **PLT**. Ein weiterer langer Druck führt zurück.
- **Links/Rechts:** Spalte wählen
- **Hoch/Runter:** Wert ändern, Halten wiederholt bei Zahlenwerten
- **Zahl oben links:** CPU-Last des Audio-Teils in Prozent (grün < 70, gelb < 90, rot)

| Spalte | Parameter | MIDI-CC | Werte |
|---|---|---|---|
| ENG | Modell | 20 | 24 Modelle, siehe unten |
| HAR | Harmonics | 21 | 00–99 |
| TIM | Timbre | 22 | 00–99 |
| MOR | Morph | 23 | 00–99 |
| DEC | Decay (Ausklingzeit) | 24 | 00–99 |
| COL | Farbe des Low-Pass-Gates (VCA ↔ Filter) | 25 | 00–99 |
| ENV | Hüllkurve: GAT = gehalten, PNG = angeschlagen | 26 | < 64 GAT, ≥ 64 PNG |
| PLT | Plaits aus/an (aus = Original-Wavetables) | 27 | < 64 aus, ≥ 64 an |

MIDI-CC werden auf dem im System-Menü eingestellten MIDI-Empfangskanal ausgewertet.

**Modelle (ENG):**
- **Neuere Plaits-Modelle:** VCF (analog mit Filter), PHD (Phase Distortion), FM1–FM3 (6-Operator-FM), TER (Wave Terrain), STM (String Machine) und CHP (Chiptune)
- **Klassische Modelle:** VA (virtuell-analog), WSH (Waveshaper), FM (2-Operator), GRN (Granular-Formant), ADD (additiv), WTB (Wavetable) und CHD (Akkorde)
- **Sprache und Geräusche:** SPC (Sprache), SWM (Schwarm), NOI (gefiltertes Rauschen) und PRT (Partikel)
- **Physikalische Modelle:** STR (Saite) und MOD (Glocke, Platte)
- **Drums:** BD, SD und HH (Bassdrum, Snare, Hi-Hat)

## Bekannte Einschränkungen

- Monophon, keine Anschlagdynamik (die Touch-Tastatur liefert keine)
- Plaits-Einstellungen werden noch nicht gespeichert; nach dem Neustart gelten die Grundwerte.
- Die Synthgirl-Hüllkurve, der Arpeggiator und die Oszillator-Menüs wirken nur auf die Wavetables, nicht auf Plaits.
- CHP spielt im Akkord-Modus. Der getaktete Arpeggiator des Moduls ist nicht angebunden.

## Selbst bauen

Benötigt die ARM-Toolchain (`gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib`) und Python 3.

```sh
cd "02 Software/Synthgirl-App-H723"
make -j8                 # Firmware mit Plaits
# make PLAITS=0          # Original-Firmware ohne Plaits
python3 ../Firmware/pack_firmware.py build/Synthgirl-App-H723.bin 1 91 ../Firmware
```

`pack_firmware.py` erzeugt das Format des Bootloaders (Header, Version, Größe, CRC32). Die Version muss Major < 10 und Minor < 100 sein.

## Technische Details

- **Code:** Plaits-DSP und stmlib liegen unter `02 Software/Synthgirl-App-H723/Third_Party/eurorack`. Die Anbindung steckt in `Core/Library/PlaitsVoice/`, die Eingriffe in `Controller.cpp` sind mit `#if SG_PLAITS` markiert.
- **Blockgrößen:** Plaits rechnet in 12er-Blöcken, Synthgirl in 16er-Blöcken. Ein kleiner Puffer gleicht das aus.
- **Speicher:** Der Plaits-Zustand (~25 KB) liegt im DTCM. Der Instruktions-Cache ist aktiv, der Daten-Cache bleibt wegen der DMA-Puffer aus.
- **Änderungen an Plaits** (Details in `Third_Party/eurorack/README_SYNTHGIRL.txt`):
  - Samplerate 44,1 kHz
  - Flash-Speicher für User-Daten deaktiviert
  - Fix in `chiptune_engine.cc`: Im Original wurde ein Oszillator nie initialisiert. Auf dem Gerät führte das zum Einfrieren bei CHP.

## Lizenzen und Dank

- **Synthgirl** (Hardware und Firmware): © Randomwaves, [MIT-Lizenz](LICENSE)
- **Plaits** und **stmlib**: © Emilie Gillet / Mutable Instruments, MIT-Lizenz ([pichenettes/eurorack](https://github.com/pichenettes/eurorack), [pichenettes/stmlib](https://github.com/pichenettes/stmlib)). Die Lizenztexte stehen in den Quelldateien.

Dieser Fork ist ein inoffizielles Projekt und steht in keiner Verbindung zu Randomwaves oder Mutable Instruments.

---

## Original-README (Randomwaves)


**A touch-controlled hardware wavetable synthesizer** — dual-oscillator synth engine and DSP effects built around an STM32H723 (Cortex-M7), with a 480×854 color display, a fully capacitive-touch control surface, and MIDI/Sync I/O.

This repository holds the complete hardware design (Eagle/KiCad) and firmware (STM32CubeIDE / C++) for Synthgirl, released by [Randomwaves](https://randomwaves.io) as an open hardware/software project. Synthgirl shares its platform, DSP chain, and touch-based control surface with [Drumboy](https://github.com/Randomwaves/Drumboy), built around a dual-oscillator wavetable engine instead of a sample-layer sequencer.

---

## Overview

The MCU sits at the center of the same core subsystems as Drumboy: an NT35510 display over a 16-bit parallel GPIO bus, external SDRAM on the FMC bus for wavetable storage, an SGTL5000 audio codec over I2S/I2C, a microSD card over SDMMC2 for storage, and MIDI/Sync connectivity over USART/GPIO. The control surface is driven entirely by three capacitive-touch channels.

| | |
|---|---|
| **MCU** | STM32H723ZGT6 — Cortex-M7, LQFP-144 |
| **Display** | NT35510, 480×854, 16-bit parallel |
| **SDRAM** | External, FMC bus (wavetable storage) |
| **Audio codec** | SGTL5000 — I2S3 audio / I2C3 control |
| **Storage** | microSD, SDMMC2, FAT filesystem |
| **Control surface** | 3× capacitive-touch channels (CT0–CT2) |
| **Connectivity** | MIDI In/Out, Sync In/Out |
| **Firmware** | v1.05 (Original), v1.91 (dieser Fork, mit Plaits) |

## Features

- **Dual-oscillator wavetable engine** (Osc A / Osc B), each with independent LFO modulation
- **9 effect types** per send slot: Delay, Chorus, Flanger, Phaser, Compressor, Expander, Overdrive, Distortion, Bitcrusher
- **2 multimode filters + 2 send effects + stereo reverb** in the DSP chain
- **Built-in step sequencer** alongside live/MIDI note input
- **MIDI In/Out** and **Sync In/Out** for external clock/gate sync
- **SD-card firmware updates** via a dedicated bootloader, mirroring the same two-project split as Drumboy

## Repository structure

```
01 Hardware/
  Main/       Main board — Eagle CAD (Main.sch / Main.brd)
  Top/        Top/control-surface board — Eagle CAD (Top.sch / Top.brd)
  Connector/  Interconnect board — KiCad (migrated from Eagle)
  Bottom/     Base board — KiCad (migrated from Eagle)

02 Software/
  Synthgirl-Boot-H723/   Bootloader — STM32CubeIDE project
  Synthgirl-App-H723/    Application firmware — STM32CubeIDE project
  Firmware/               Prebuilt firmware binaries
  Sd Card/                Reference SD card contents (wavetables, presets, system data)
```

## Hardware

Main and Top were originally designed in **Autodesk Eagle** (`.sch` / `.brd`); Connector and Bottom have since been migrated to **KiCad**.

## Firmware

Built with **STM32CubeIDE**, written in C/C++ against the STM32 HAL. Firmware is split into `Synthgirl-Boot-H723` (bootloader) and `Synthgirl-App-H723` (application), sharing one flash device — the same split used across Randomwaves' STM32H723 instruments.

## License

Released under the [MIT License](LICENSE).

## Community

- 💬 [Discord community](https://discord.gg/5atevNH6UU) — join the discussion
- 🌐 [randomwaves.io](https://www.randomwaves.io)
- ✉️ [hello@randomwaves.io](mailto:hello@randomwaves.io)
