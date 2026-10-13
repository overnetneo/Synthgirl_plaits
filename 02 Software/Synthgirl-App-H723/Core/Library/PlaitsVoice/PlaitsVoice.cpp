#include "PlaitsVoice.h"
#include "sg_diag.h"

#include <new>
#include <string.h>

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

#if defined(STM32H723xx)
#include "stm32h7xx.h"
// Plaits-Zustand und Arbeitsspeicher liegen im DTCM: 0 Wartezyklen, kein Cache noetig,
// und der knappe AXI-RAM (RAM_D1) bleibt fuer Synthgirl frei.
#define SG_DTCM __attribute__((section(".dtcm_bss"), aligned(8)))
#else
#define SG_DTCM
#endif

namespace {

const size_t kSharedBufferSize = 16384;  // wie im Plaits-Original

SG_DTCM uint8_t voiceStorage[sizeof(plaits::Voice)];
SG_DTCM char sharedBuffer[kSharedBufferSize];

plaits::Voice* voice = nullptr;
plaits::Patch patch;
plaits::Modulations modulations;

const char* const kEngineNames[PlaitsVoice::kNumEngines] = {
    "VCF", "PHD", "FM1", "FM2", "FM3", "TER", "STM", "CHP",  // Bank 1 (Plaits 1.2)
    "VA ", "WSH", "FM ", "GRN", "ADD", "WTB", "CHD", "SPC",  // Bank 2 (klassisch)
    "SWM", "NOI", "PRT", "STR", "MOD", "BD ", "SD ", "HH "   // Bank 3 (klassisch)
};

const char* const kParamTitles[PlaitsVoice::P_COUNT] = {
    "ENG", "HAR", "TIM", "MOR", "DEC", "COL", "ENV", "PLT"};

inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

}  // namespace

void PlaitsVoice::Init() {
    // Speicher nullen, BEVOR das Objekt konstruiert wird (der Konstruktor setzt die
    // vtable-Zeiger der Engines). Achtung: Ein normales memset direkt vor "placement
    // new" darf GCC als tote Speicherung entfernen (lifetime DSE) - genau das passierte
    // in 1.90, der DTCM enthielt dann Reste. Schreiben ueber einen volatile-Zeiger wird
    // nie wegoptimiert (zusaetzlich baut das Makefile mit -fno-lifetime-dse).
    volatile uint8_t* p = voiceStorage;
    for (size_t i = 0; i < sizeof(voiceStorage); ++i) p[i] = 0;
    memset(sharedBuffer, 0, sizeof(sharedBuffer));
    voice = new (voiceStorage) plaits::Voice();
    stmlib::BufferAllocator allocator(sharedBuffer, kSharedBufferSize);
    voice->Init(&allocator);

    memset(&patch, 0, sizeof(patch));
    memset(&modulations, 0, sizeof(modulations));

    // Keine Modulationseingaenge ausser Trigger/Level; die "Attenuverter" stehen auf 0,
    // damit die interne Decay-Huellkurve TIMBRE/MORPH/FREQ nicht unerwartet moduliert.
    patch.frequency_modulation_amount = 0.0f;
    patch.timbre_modulation_amount = 0.0f;
    patch.morph_modulation_amount = 0.0f;

    memset(fifo_, 0, sizeof(fifo_));
    fifoRead_ = fifoWrite_ = fifoCount_ = 0;
    gate_ = false;
    retrigger_ = false;
}

void PlaitsVoice::NoteOn(uint8_t midiNote) {
    note_ = static_cast<float>(midiNote);
    if (gate_) {
        retrigger_ = true;
    }
    gate_ = true;
}

void PlaitsVoice::NoteOff() {
    gate_ = false;
    retrigger_ = false;
}

void PlaitsVoice::renderBlock() {
    patch.engine = engine_;
    patch.note = 0.0f;
    patch.harmonics = harmonics_;
    patch.timbre = timbre_;
    patch.morph = morph_;
    patch.decay = decay_;
    patch.lpg_colour = colour_;

    modulations.engine = 0.0f;
    modulations.note = note_ + tune_;
    modulations.frequency = 0.0f;
    modulations.harmonics = 0.0f;
    modulations.timbre = 0.0f;
    modulations.morph = 0.0f;
    modulations.frequency_patched = false;
    modulations.timbre_patched = false;
    modulations.morph_patched = false;
    // Chiptune (Engine 7) wird bei gepatchtem Trigger zum getakteten Arpeggiator ohne
    // eigene Huellkurve (auf dem Modul folgt dort ein externer VCA). Fuer das Spielen
    // per Tastatur daher: Trigger ungepatcht (Akkord-Modus), Lautstaerke folgt dem Gate.
    const bool chiptune = (engine_ == 7);
    modulations.trigger_patched = !chiptune;

    bool triggerHigh = gate_;
    if (retrigger_) {
        // Einen Block lang Trigger auf 0, damit Plaits die neue Flanke erkennt.
        triggerHigh = false;
        retrigger_ = false;
    }
    modulations.trigger = triggerHigh ? 1.0f : 0.0f;

    if ((envMode_ == ENV_GATE) || chiptune) {
        modulations.level_patched = true;
        modulations.level = gate_ ? 1.0f : 0.0f;
    } else {
        modulations.level_patched = false;
        modulations.level = 0.0f;
    }

    plaits::Voice::Frame frames[plaits::kBlockSize];
    SG_CRUMB(0x100 | (engine_ << 4) | 0x1);
    voice->Render(patch, modulations, frames, plaits::kBlockSize);
    SG_CRUMB(0x100 | (engine_ << 4) | 0xF);

    const float kInv = 1.0f / 32768.0f;
    for (size_t i = 0; i < plaits::kBlockSize; ++i) {
        fifo_[fifoWrite_] = static_cast<float>(frames[i].out) * kInv;
        fifoWrite_ = (fifoWrite_ + 1) % kFifoSize;
    }
    fifoCount_ += plaits::kBlockSize;
}

void PlaitsVoice::Render(float* out, size_t size) {
#if defined(STM32H723xx)
    uint32_t start = DWT->CYCCNT;
#endif
    for (size_t i = 0; i < size; ++i) {
        if (fifoCount_ == 0) {
            renderBlock();
        }
        out[i] = fifo_[fifoRead_];
        fifoRead_ = (fifoRead_ + 1) % kFifoSize;
        --fifoCount_;
    }
#if defined(STM32H723xx)
    lastRenderCycles_ = DWT->CYCCNT - start;
#endif
}

void PlaitsVoice::setEngine(int engine) {
    if (engine < 0) engine = 0;
    if (engine >= kNumEngines) engine = kNumEngines - 1;
    engine_ = engine;
}

void PlaitsVoice::setParam(Param p, float value) {
    value = clamp01(value);
    switch (p) {
        case P_HARMONICS: harmonics_ = value; break;
        case P_TIMBRE: timbre_ = value; break;
        case P_MORPH: morph_ = value; break;
        case P_DECAY: decay_ = value; break;
        case P_COLOUR: colour_ = value; break;
        case P_ENGINE: setEngine(static_cast<int>(value * (kNumEngines - 1) + 0.5f)); break;
        case P_ENVMODE: envMode_ = value >= 0.5f ? ENV_PING : ENV_GATE; break;
        case P_ACTIVE: active_ = value >= 0.5f; break;
        default: break;
    }
}

float PlaitsVoice::param(Param p) const {
    switch (p) {
        case P_HARMONICS: return harmonics_;
        case P_TIMBRE: return timbre_;
        case P_MORPH: return morph_;
        case P_DECAY: return decay_;
        case P_COLOUR: return colour_;
        default: return 0.0f;
    }
}

void PlaitsVoice::step(Param p, int delta) {
    switch (p) {
        case P_ENGINE: {
            int e = engine_ + delta;
            // Umlaufend, damit man schnell durch alle 24 Modelle kommt.
            while (e < 0) e += kNumEngines;
            engine_ = e % kNumEngines;
            break;
        }
        case P_ENVMODE:
            envMode_ = (envMode_ == ENV_GATE) ? ENV_PING : ENV_GATE;
            break;
        case P_ACTIVE:
            active_ = !active_;
            break;
        default: {
            // 0..99 Schritte wie die uebrigen Synthgirl-Menues.
            int v = static_cast<int>(param(p) * 99.0f + 0.5f) + delta;
            if (v < 0) v = 0;
            if (v > 99) v = 99;
            setParam(p, v / 99.0f);
            break;
        }
    }
}

void PlaitsVoice::format(Param p, char out[4]) const {
    switch (p) {
        case P_ENGINE:
            memcpy(out, kEngineNames[engine_], 3);
            break;
        case P_ENVMODE:
            memcpy(out, envMode_ == ENV_GATE ? "GAT" : "PNG", 3);
            break;
        case P_ACTIVE:
            memcpy(out, active_ ? " ON" : "OFF", 3);
            break;
        default: {
            int v = static_cast<int>(param(p) * 99.0f + 0.5f);
            out[0] = ' ';
            out[1] = static_cast<char>('0' + v / 10);
            out[2] = static_cast<char>('0' + v % 10);
            break;
        }
    }
    out[3] = '\0';
}

const char* PlaitsVoice::engineName(int engine) {
    if (engine < 0 || engine >= kNumEngines) return "???";
    return kEngineNames[engine];
}

const char* PlaitsVoice::paramTitle(Param p) {
    return (p >= 0 && p < P_COUNT) ? kParamTitles[p] : "???";
}
