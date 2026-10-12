// PlaitsVoice: Mutable-Instruments-Plaits als monophone Klangerzeugung fuer Synthgirl.
//
// Die Klasse kapselt plaits::Voice. Sie wird ausschliesslich aus dem Audio-Interrupt
// (Render, NoteOn, NoteOff) bzw. per einfacher Parameter-Schreibzugriffe aus der
// Hauptschleife/UI bedient. Alle Parameter sind 0..1-Floats oder kleine Integer und
// werden atomar geschrieben, daher ist kein Locking noetig.
//
// Plaits rechnet intern in Bloecken zu plaits::kBlockSize (12) Samples, Synthgirl
// liefert Bloecke zu 16 Samples. Ein kleiner Ringpuffer gleicht das aus, damit alle
// Zeitkonstanten (Huellkurven, Decay) exakt so laufen wie auf dem Original-Modul.

#ifndef SYNTHGIRL_PLAITS_VOICE_H_
#define SYNTHGIRL_PLAITS_VOICE_H_

#include <stddef.h>
#include <stdint.h>

class PlaitsVoice {
   public:
    static const int kNumEngines = 24;

    // Spalten der Plaits-Seite auf dem Display bzw. Parameter-IDs.
    enum Param {
        P_ENGINE = 0,
        P_HARMONICS,
        P_TIMBRE,
        P_MORPH,
        P_DECAY,
        P_COLOUR,
        P_ENVMODE,
        P_ACTIVE,
        P_COUNT
    };

    enum EnvMode {
        ENV_GATE = 0,  // LPG folgt der gehaltenen Taste (Sustain), DECAY = Ausklingen
        ENV_PING = 1   // Jeder Anschlag "pingt" das LPG an, DECAY = Abklingzeit
    };

    void Init();

    // Aus dem Audio-Interrupt aufrufen.
    void NoteOn(uint8_t midiNote);
    void NoteOff();
    // Erzeugt `size` Mono-Samples im Bereich -1..1 (size beliebig, typisch 16).
    void Render(float* out, size_t size);

    // Parameter (0..1 bzw. Index). Werte ausserhalb werden begrenzt.
    void setEngine(int engine);
    void setParam(Param p, float value);  // fuer HARMONICS..COLOUR
    void setEnvMode(EnvMode m) { envMode_ = m; }
    void setActive(bool a) { active_ = a; }
    void setTune(float semitones) { tune_ = semitones; }

    int engine() const { return engine_; }
    float param(Param p) const;
    EnvMode envMode() const { return envMode_; }
    bool active() const { return active_; }

    // Wert-Schritte fuer die Bedienung: delta in "Display-Einheiten" (0..99 bzw. Index).
    void step(Param p, int delta);
    // 3-Zeichen-Darstellung fuer das Display (ohne Nullterminierung kopiert in out[0..2]).
    void format(Param p, char out[4]) const;

    static const char* engineName(int engine);
    static const char* paramTitle(Param p);

    // CPU-Messung: Zyklen des letzten Render-Aufrufs (nur auf dem Geraet gefuellt).
    uint32_t lastRenderCycles() const { return lastRenderCycles_; }

   private:
    void renderBlock();

    volatile int engine_ = 8;
    volatile float harmonics_ = 0.5f;
    volatile float timbre_ = 0.5f;
    volatile float morph_ = 0.5f;
    volatile float decay_ = 0.5f;
    volatile float colour_ = 0.5f;
    volatile float tune_ = 0.0f;
    volatile EnvMode envMode_ = ENV_GATE;
    volatile bool active_ = false;

    float note_ = 60.0f;
    bool gate_ = false;
    bool retrigger_ = false;  // Taste gewechselt, waehrend Gate high: Trigger kurz absenken

    // Ringpuffer 12er-Bloecke -> 16er-Bloecke
    static const size_t kFifoSize = 64;
    float fifo_[kFifoSize];
    size_t fifoRead_ = 0;
    size_t fifoWrite_ = 0;
    size_t fifoCount_ = 0;

    uint32_t lastRenderCycles_ = 0;
};

#endif  // SYNTHGIRL_PLAITS_VOICE_H_
