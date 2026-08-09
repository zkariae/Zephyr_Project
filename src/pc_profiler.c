/**
 * @file
 * @brief PC-sampling profiler implementation: a dedicated hardware timer
 *        (TIM7, 1 kHz) interrupts directly through a naked trampoline that
 *        reads the PC the CPU hardware auto-stacked at exception entry —
 *        i.e. wherever foreground code was actually executing the instant
 *        before the interrupt fired.
 *
 * Earlier revision polled DWT->PCSR from a Zephyr k_timer callback; on this
 * board that read back the profiler's own PC (100% self-attribution on
 * hardware), since PCSR here doesn't free-run asynchronously in the
 * background the way a debug probe's trace hardware does. Reading the
 * hardware-stacked frame of a raw ISR is the standard, reliable technique
 * (the classic "poor man's profiler").
 */

#include "pc_profiler.h"
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/arch/arm/cortex_m/exception.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <stm32_ll_bus.h>
#include <stm32_ll_tim.h>

#define PC_PROFILER_MAX_SYMBOLS 2500 /* current build has ~1900 text symbols */

/* TIM7 kernel clock = 108MHz (SYSCLK=216MHz, APB1 prescaler=4, so
 * TIMxCLK = 2*PCLK1 per the STM32 rule when the APBx prescaler != 1).
 * PSC=107 -> 108MHz/108 = 1MHz counter clock; ARR=999 -> 1MHz/1000 = 1kHz. */
#define PC_PROFILER_TIM7_PSC 107u
#define PC_PROFILER_TIM7_ARR 999u

static atomic_t sample_counts[PC_PROFILER_MAX_SYMBOLS];
static atomic_t total_samples;
static atomic_t unresolved_samples;

/* Binary search for the symbol whose [addr, addr+size) range contains pc. */
static int find_symbol_index(uint32_t pc)
{
    int lo = 0;
    int hi = (int)pc_profiler_symtab_count - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t start = pc_profiler_symtab[mid].addr;
        uint32_t end = start + pc_profiler_symtab[mid].size;

        if (pc < start) {
            hi = mid - 1;
        } else if (pc >= end) {
            lo = mid + 1;
        } else {
            return mid;
        }
    }
    return -1;
}

/* Ordinary C function reached by tail branch (not call) from the naked
 * trampoline below, so `lr` on entry is still the original EXC_RETURN value
 * the hardware set at exception entry: this function's own compiler-
 * generated prologue/epilogue saves and restores it like any other
 * callee-saved register, and returns through it, performing a real
 * exception return (unstacking the frame from the correct MSP/PSP). */
/* used: only ever reached via the `b` in the naked trampoline's inline asm,
 * invisible to the compiler's normal call graph -- without this it gets
 * optimized away as dead code, leaving the branch target undefined. */
static void __attribute__((used)) pc_profiler_tim7_isr_c(uint32_t *frame)
{
    LL_TIM_ClearFlag_UPDATE(TIM7); /* must clear before return, or the IRQ re-fires immediately */

    uint32_t pc = frame[6]; /* frame = {r0,r1,r2,r3,r12,lr,pc,xpsr} */
    int idx = find_symbol_index(pc);

    if (idx >= 0 && idx < PC_PROFILER_MAX_SYMBOLS) {
        atomic_inc(&sample_counts[idx]);
    } else {
        atomic_inc(&unresolved_samples);
    }
    atomic_inc(&total_samples);
}

/* Naked: no compiler prologue, so `lr` here is exactly the hardware
 * EXC_RETURN value. EXC_RETURN bit 2 (SPSEL) tells us whether the
 * interrupted context was using MSP or PSP; that stack pointer's current
 * value points straight at the 8-word hardware-stacked frame. Branching
 * (not calling) into the C handler preserves `lr` so it can perform the
 * actual exception return. Installed via IRQ_DIRECT_CONNECT so the vector
 * table jumps here directly -- a normal IRQ_CONNECT goes through Zephyr's
 * shared _isr_wrapper first, which would overwrite lr before we see it. */
static void __attribute__((naked)) pc_profiler_tim7_isr(void)
{
    __asm volatile (
        "tst lr, #4                  \n"
        "ite eq                      \n"
        "mrseq r0, msp                \n"
        "mrsne r0, psp                \n"
        "b pc_profiler_tim7_isr_c    \n"
        : : : "memory"
    );
}

void pc_profiler_init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);

    LL_TIM_SetPrescaler(TIM7, PC_PROFILER_TIM7_PSC);
    LL_TIM_SetAutoReload(TIM7, PC_PROFILER_TIM7_ARR);
    LL_TIM_ClearFlag_UPDATE(TIM7);
    LL_TIM_EnableIT_UPDATE(TIM7);
    LL_TIM_EnableCounter(TIM7);

    /* Lowest priority: this statistical sampler must never preempt LTDC,
     * DMA, or any other peripheral IRQ (all at priority 0 on this board). */
    IRQ_DIRECT_CONNECT(TIM7_IRQn, IRQ_PRIO_LOWEST, pc_profiler_tim7_isr, 0);
    irq_enable(TIM7_IRQn);
}

size_t pc_profiler_get_top(struct pc_profile_entry *out, size_t max_entries)
{
    size_t count = 0;
    size_t n = MIN(pc_profiler_symtab_count, PC_PROFILER_MAX_SYMBOLS);

    for (size_t i = 0; i < n; i++) {
        uint32_t s = atomic_get(&sample_counts[i]);

        if (s == 0) {
            continue;
        }
        if (count == max_entries && s <= out[max_entries - 1].samples) {
            continue;
        }

        size_t pos = count < max_entries ? count : max_entries - 1;

        while (pos > 0 && out[pos - 1].samples < s) {
            out[pos] = out[pos - 1];
            pos--;
        }
        out[pos] = (struct pc_profile_entry){
            .name = pc_profiler_symtab[i].name,
            .addr = pc_profiler_symtab[i].addr,
            .size = pc_profiler_symtab[i].size,
            .samples = s,
        };
        if (count < max_entries) {
            count++;
        }
    }
    return count;
}

uint32_t pc_profiler_total_samples(void)
{
    return atomic_get(&total_samples);
}

uint32_t pc_profiler_unresolved_samples(void)
{
    return atomic_get(&unresolved_samples);
}
