/**
 * @file
 * @brief PC-sampling profiler: a TIM7-driven ISR samples the interrupted
 *        program counter and attributes each sample to a function using a
 *        build-generated symbol table (src/generated/symtab_data.c).
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

/** @brief Enable DWT PC sampling and start the periodic sample timer. */
void pc_profiler_init(void);

/**
 * @brief Fill @p out with the top @p max_entries functions by sample count,
 *        sorted descending.
 * @return Number of entries written (<= max_entries).
 */
size_t pc_profiler_get_top(struct pc_profile_entry *out, size_t max_entries);

/** @brief Total number of valid PC samples collected since init. */
uint32_t pc_profiler_total_samples(void);

/**
 * @brief Number of samples whose PC fell outside every known symbol range
 *        (as opposed to samples in a known function that's just not in the
 *        top N returned by pc_profiler_get_top()).
 */
uint32_t pc_profiler_unresolved_samples(void);

#endif /* APP_PC_PROFILER_H_ */
