#include <uk/plat/common/lcpu.h>
#include <uk/plat/time.h>
#include <uk/print.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_SECONDARY_CORES  3
#define NR_SECONDARY_CORES 3
#define N        512
static double A[N][N] __attribute__((aligned(64)));
static double B[N][N] __attribute__((aligned(64)));
static double C[N][N] __attribute__((aligned(64)));
static volatile uint64_t results[CONFIG_UKPLAT_LCPU_MAXCOUNT];

static void multiply_rows(size_t start, size_t end)
{
    for (size_t i = start; i < end; i++) {
        for (size_t j = 0; j < N; j++) {
            double sum = 0;
            for (size_t k = 0; k < N; k++) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

static void remote_fn(struct __regs *regs __unused, void *arg __unused)
{
    unsigned core  = ukplat_lcpu_id();
    unsigned count = NR_SECONDARY_CORES + 1;
    size_t chunk   = N / count;
    size_t start   = core * chunk;
    size_t end     = (core + 1 == count) ? N : start + chunk;

    multiply_rows(start, end);

    uint64_t cs = 0;
    for (size_t i = start; i < end; i++)
        for (size_t j = 0; j < N; j++)
            cs ^= (uint64_t)C[i][j];
    results[core] = cs;
}

static void run_core_zero_share(void)
{
    size_t count = NR_SECONDARY_CORES + 1;
    size_t chunk = N / count;
    size_t start = 0;
    size_t end   = chunk;

    multiply_rows(start, end);

    uint64_t cs = 0;
    for (size_t i = start; i < end; i++)
        for (size_t j = 0; j < N; j++)
            cs ^= (uint64_t)C[i][j];
    results[0] = cs;
}

static void run_on_this_cpu(void)
{
    multiply_rows(0, N);

    uint64_t cs = 0;
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++)
            cs ^= (uint64_t)C[i][j];
    results[0] = cs;
}

static void print_matrix(const char *name, double M[N][N])
{
    uk_pr_err("=== Matrix %s ===\n", name);

    for (size_t i = 0; i < N; i++) {
        char line[256];
        size_t pos = 0;

        for (size_t j = 0; j < N; j++) {
            int n = snprintf(line + pos, sizeof(line) - pos,
                             "%8.2f ", M[i][j]);
            pos += (n > 0 ? n : 0);
            if (pos >= sizeof(line) - 16)
                break;
        }

        uk_pr_err("%s\n", line);
    }

    uk_pr_err("=== end %s ===\n", name);
}


int main(int argc, char *argv[])
{
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++) {
            A[i][j] = (double)(i + j);
            B[i][j] = (double)(i - j);
            C[i][j] = 0;
        }


    uint64_t t0 = ukplat_monotonic_clock();

    __lcpuidx secondaries[MAX_SECONDARY_CORES];
    unsigned   num_sec     = NR_SECONDARY_CORES;

    for (unsigned i = 0; i < num_sec; i++) {
        secondaries[i] = i + 1;
    }

    struct ukplat_lcpu_func fn = {
            .fn   = remote_fn,
            .user = NULL
    };

    int rc = ukplat_lcpu_run(secondaries, &num_sec, &fn, 0);
    if (rc)
            uk_pr_crit("ukplat_lcpu_run() failed: %d\n", rc);

    run_core_zero_share();

    ukplat_lcpu_wait(secondaries, &num_sec, 0);

    uint64_t t1 = ukplat_monotonic_clock();

    // uint64_t t0 = ukplat_monotonic_clock();
    // run_on_this_cpu();
    // uint64_t t1 = ukplat_monotonic_clock();

    uk_pr_err("Elapsed: %llu µs\n",
               (unsigned long long)((t1 - t0) / 1000));

    /* report per-core checksums */
    unsigned count = ukplat_lcpu_count();
    for (unsigned i = 0; i < count; i++)
    uk_pr_err("core %u checksum: 0x%llx\n",
                   i, (unsigned long long)results[i]);

    /* global checksum */
    uint64_t global = 0;
    for (unsigned i = 0; i < count; i++)
        global ^= results[i];
        uk_pr_err("Global checksum: 0x%llx\n",
               (unsigned long long)global);

    return 0;
}
