#ifndef SG_DIAG_H
#define SG_DIAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SG_DIAG
#define SG_DIAG 0
#endif

#define SG_DIAG_MAGIC 0x53474447u /* "SGDG" */

enum {
    SG_DIAG_NONE = 0,
    SG_DIAG_FAULT = 1,       /* HardFault & Co. */
    SG_DIAG_AUDIO_HANG = 2,  /* Audio-Interrupt kommt nicht zurueck */
    SG_DIAG_MAIN_HANG = 3    /* Hauptschleife steht */
};

typedef struct {
    uint32_t magic;
    uint32_t type;
    uint32_t count;
    uint32_t pc;
    uint32_t lr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t mmfar;
    uint32_t crumb;
} SgDiagRecord;

#if SG_DIAG
extern volatile SgDiagRecord sgDiagRecord;
extern volatile uint32_t sgDiagCrumb;
extern volatile uint32_t sgDiagAudioBeat;
extern volatile uint32_t sgDiagMainBeat;
extern volatile uint32_t sgDiagArmMain;
void sgDiag_init(void);
#define SG_CRUMB(x) (sgDiagCrumb = (x))
#else
#define SG_CRUMB(x) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
