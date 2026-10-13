Plaits DSP (Mutable Instruments, Emilie Gillet), MIT-Lizenz
eurorack commit: f4cfa7697095a28d292bff758beb51deb49b5a86
stmlib commit:   d18def816c51d1da0c108236928b2bbd25c17481
Kopiert: plaits/dsp, plaits/resources.*, plaits/user_data.h, stmlib/dsp, stmlib/utils, stmlib/stmlib.h
Aenderung: plaits/dsp/dsp.h -> Samplerate 44100 Hz (Synthgirl)
Aenderung: plaits/user_data.h -> Flash-Mock ohne printf, aktiv auch bei SG_PLAITS (User-Daten ungenutzt, FM-Patches aus ROM)
Fix: plaits/dsp/engine2/chiptune_engine.cc -> alle kChordNumVoices Oszillatoren initialisieren (Original: nur kChordNumNotes)
