/* Synthgirl-Diagnose (nur mit -DSG_DIAG=1 gebaut)
 *
 * Findet heraus, WO das Geraet haengen bleibt:
 *  - Fault-Handler (HardFault/MemManage/BusFault/UsageFault) sichern PC/LR und die
 *    Fault-Register und starten neu.
 *  - TIM2 (hoechste Prioritaet, 100 Hz) ueberwacht zwei Herzschlaege: den Audio-
 *    Interrupt und die Hauptschleife. Steht einer still, wird der unterbrochene
 *    Programmzaehler (PC) gesichert und neu gestartet.
 *  Die Daten liegen im DTCM, der einen Software-Reset ueberlebt, und
 *  werden nach dem Neustart auf der Plaits-Seite angezeigt.
 */
#include "sg_diag.h"

#if SG_DIAG

#include "stm32h7xx.h"

/* Im DTCM (wird beim Start weder vom Bootloader noch von der App geloescht).
   Version 1.92 nutzte SRAM4, das lieferte auf dem Geraet nur Datenmuell. */
__attribute__((section(".dtcm_bss"))) volatile SgDiagRecord sgDiagRecord;
__attribute__((section(".dtcm_bss"))) volatile uint32_t sgDiagCrumb;

static uint32_t checksum(void) {
    return SG_DIAG_MAGIC ^ 0xA5A5A5A5u ^ sgDiagRecord.type ^ (sgDiagRecord.count * 3u) ^ sgDiagRecord.pc ^
           (sgDiagRecord.lr << 1) ^ sgDiagRecord.cfsr ^ sgDiagRecord.hfsr ^ sgDiagRecord.bfar ^ sgDiagRecord.mmfar ^
           (sgDiagRecord.crumb << 3);
}

volatile uint32_t sgDiagAudioBeat = 0;
volatile uint32_t sgDiagMainBeat = 0;
volatile uint32_t sgDiagArmMain = 0;

static uint32_t lastAudioBeat = 0, lastMainBeat = 0;
static uint32_t audioStill = 0, mainStill = 0;

static void saveAndReset(uint32_t type, const uint32_t* frame) {
    sgDiagRecord.type = type;
    sgDiagRecord.pc = frame ? frame[6] : 0;
    sgDiagRecord.lr = frame ? frame[5] : 0;
    sgDiagRecord.cfsr = SCB->CFSR;
    sgDiagRecord.hfsr = SCB->HFSR;
    sgDiagRecord.bfar = SCB->BFAR;
    sgDiagRecord.mmfar = SCB->MMFAR;
    sgDiagRecord.crumb = sgDiagCrumb;
    sgDiagRecord.count += 1;
    sgDiagRecord.magic = SG_DIAG_MAGIC;
    sgDiagRecord.check = checksum();
    __DSB();
    NVIC_SystemReset();
}

/* C-Teil der Handler; frame = vom Prozessor gesicherte Register des unterbrochenen Codes */
void sgDiag_faultC(const uint32_t* frame) { saveAndReset(SG_DIAG_FAULT, frame); }

void sgDiag_tim2C(const uint32_t* frame) {
    TIM2->SR = 0;
    uint32_t a = sgDiagAudioBeat, m = sgDiagMainBeat;
    if (a != lastAudioBeat) { lastAudioBeat = a; audioStill = 0; } else if (a != 0) { audioStill++; }
    if (m != lastMainBeat) { lastMainBeat = m; mainStill = 0; } else { mainStill++; }

    if (audioStill >= 50) {                       /* 0,5 s kein Audio-Block */
        saveAndReset(SG_DIAG_AUDIO_HANG, frame);
    }
    if (sgDiagArmMain && (mainStill >= 200)) {    /* 2 s Hauptschleife steht */
        saveAndReset(SG_DIAG_MAIN_HANG, frame);
    }
}

/* Einstiege: ermitteln den Stack (MSP/PSP) mit dem gesicherten Frame */
#define SG_NAKED_ENTRY(name, target)                     \
    __attribute__((naked)) void name(void) {             \
        __asm volatile("tst lr, #4      \n"              \
                       "ite eq          \n"              \
                       "mrseq r0, msp   \n"              \
                       "mrsne r0, psp   \n"              \
                       "b " #target "   \n");            \
    }

SG_NAKED_ENTRY(HardFault_Handler, sgDiag_faultC)
SG_NAKED_ENTRY(MemManage_Handler, sgDiag_faultC)
SG_NAKED_ENTRY(BusFault_Handler, sgDiag_faultC)
SG_NAKED_ENTRY(UsageFault_Handler, sgDiag_faultC)
SG_NAKED_ENTRY(TIM2_IRQHandler, sgDiag_tim2C)

void sgDiag_init(void) {
    if ((sgDiagRecord.magic != SG_DIAG_MAGIC) || (sgDiagRecord.check != checksum()) ||
        (sgDiagRecord.type > SG_DIAG_MAIN_HANG)) {
        /* erster Start nach dem Einschalten: SRAM4 enthaelt Zufall */
        sgDiagRecord.type = SG_DIAG_NONE;
        sgDiagRecord.count = 0;
        sgDiagRecord.pc = sgDiagRecord.lr = sgDiagRecord.crumb = 0;
        sgDiagRecord.cfsr = sgDiagRecord.hfsr = sgDiagRecord.bfar = sgDiagRecord.mmfar = 0;
        sgDiagRecord.magic = SG_DIAG_MAGIC;
        sgDiagRecord.check = checksum();
    }
    sgDiagCrumb = 0;

    /* Audio-Interrupts eine Stufe runter, damit TIM2 sie unterbrechen kann */
    HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 1, 0);
    HAL_NVIC_SetPriority(SPI3_IRQn, 1, 0);

    /* TIM2: 100 Hz. Timertakt = APB1-Timertakt (aus SystemCoreClock abgeleitet) */
    __HAL_RCC_TIM2_CLK_ENABLE();
    uint32_t timclk = HAL_RCC_GetPCLK1Freq() * 2U;
    TIM2->CR1 = 0;
    TIM2->PSC = (timclk / 10000U) - 1U;   /* 10 kHz */
    TIM2->ARR = 100U - 1U;                /* 100 Hz */
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UIE;
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}

#endif
