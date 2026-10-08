/**
 * @file
 * @brief PC-sampling profiler: TIM7 ISR samples the interrupted PC and
 *        attributes it via a build-generated symbol table (symtab_data.c).
 */

#ifndef APP_PC_PROFILER_H_
#define APP_PC_PROFILER_H_

#include <stdint.h>
#include <stddef.h>

/* One row of the build-generated symbol table. */
struct symtab_entry {
    uint32_t addr;
    uint32_t size;
    const char *name;
};

/* Provided by src/generated/symtab_data.c, sorted ascending by addr. */
extern const struct symtab_entry pc_profiler_symtab[];
extern const unsigned int pc_profiler_symtab_count;

struct pc_profile_entry {
    const char *name;
    uint32_t addr;
    uint32_t size;
    uint32_t samples;
};

/** @brief Start the TIM7 1 kHz PC-sampling timer. */
void pc_profiler_init(void);

/**
 * @brief Fill @p out with the top @p max_entries functions by sample count,
 *        sorted descending.
 * @return Number of entries written (<= max_entries).
 */
size_t pc_profiler_get_top(struct pc_profile_entry *out, size_t max_entries);

/** @brief Total number of valid PC samples collected since init. */
uint32_t pc_profiler_total_samples(void);

/** @brief Samples whose PC fell outside every known symbol range (vs functions
 *        just missing from the top N of pc_profiler_get_top()). */
uint32_t pc_profiler_unresolved_samples(void);

#endif /* APP_PC_PROFILER_H_ */
