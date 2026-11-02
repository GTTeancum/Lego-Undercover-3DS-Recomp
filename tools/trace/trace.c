/* Function-entry counters for a trace build of the recompiled game.
 * instrument.py makes every f_XXXXXXXX count its entries here; Zakuro's
 * headless probe calls recomp_trace_dump(frame) every few frames, which
 * appends "frame address count" for each function entered since the last
 * dump to $ZAKURO_TRACE_OUT and starts the counts over. Analysis only. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BASE 0x00100000u
#define SLOTS (0x00300000u >> 2) /* 3 MiB of code, one slot per word */

static uint32_t counts[SLOTS];

void recomp_trace_hit(uint32_t address) {
    uint32_t slot = (address - BASE) >> 2;
    if (slot < SLOTS) __atomic_fetch_add(&counts[slot], 1, __ATOMIC_RELAXED);
}

__attribute__((visibility("default"))) void recomp_trace_dump(uint64_t frame) {
    static FILE *out;
    if (!out) {
        const char *path = getenv("ZAKURO_TRACE_OUT");
        out = fopen(path ? path : "trace.txt", "w");
        if (!out) return;
    }
    for (uint32_t slot = 0; slot < SLOTS; slot++) {
        uint32_t n = __atomic_exchange_n(&counts[slot], 0, __ATOMIC_RELAXED);
        if (n) fprintf(out, "%llu %08X %u\n", (unsigned long long)frame, BASE + (slot << 2), n);
    }
    fflush(out);
}
