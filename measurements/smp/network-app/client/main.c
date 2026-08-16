#define _POSIX_C_SOURCE 200809L
#include <uk/plat/common/lcpu.h>
#include <uk/plat/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

#define SERVER_IP            "192.168.137.123"
#define PORT                 8080
#define BUFFER_SIZE          1000000
#define REPEAT_TEST          10

#define MAX_SECONDARY_CORES  3
#define NR_SECONDARY_CORES   3

#define TEA_DELTA        0x9e3779b9u
#define TEA_NUM_ROUNDS   4
#define TEA_HEAVY_LOOPS  1000

#define WINDOW_SIZE      128
#define LOOKAHEAD        4
#define MIN_MATCH        4

static void millisleep(unsigned int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000 };
    while (nanosleep(&ts, &ts) && errno == EINTR) {}
}

static unsigned char raw_buf[BUFFER_SIZE];
static unsigned char comp_slice[MAX_SECONDARY_CORES+1][BUFFER_SIZE];
static unsigned char enc_slice [MAX_SECONDARY_CORES+1][BUFFER_SIZE];
static int         slice_enc_len[MAX_SECONDARY_CORES+1];

static uint32_t crc32_table[256];
static void crc32_init(void) {
    const uint32_t poly = 0xEDB88320u;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (poly ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
}
static uint32_t compute_crc(const unsigned char *data, size_t len) {
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static int compress_lz77(const unsigned char *in, int in_len,
                         unsigned char *out, int *out_len)
{
    int wpos = 0, opos = 0;
    while (wpos < in_len) {
        int best_off = 0, best_len = 0;
        int start = wpos > WINDOW_SIZE ? wpos - WINDOW_SIZE : 0;
        for (int j = start; j < wpos; j++) {
            int len = 0;
            while (len < LOOKAHEAD
                && wpos + len < in_len
                && in[j + len] == in[wpos + len]) {
                len++;
            }
            if (len > best_len) {
                best_len = len;
                best_off = wpos - j;
                if (len == LOOKAHEAD) break;
            }
        }
        if (best_len >= MIN_MATCH) {
            if (opos + 4 > *out_len) return -1;
            out[opos++] = 1;
            out[opos++] = (best_off >> 8) & 0xFF;
            out[opos++] = best_off & 0xFF;
            out[opos++] = best_len;
            wpos += best_len;
        } else {
            if (opos + 2 > *out_len) return -1;
            out[opos++] = 0;
            out[opos++] = in[wpos++];
        }
    }
    *out_len = opos;
    return opos;
}

static inline void tea_encrypt_block(uint32_t v[2], const uint32_t k[4]) {
    uint32_t v0 = v[0], v1 = v[1], sum = 0;
    for (int i = 0; i < TEA_NUM_ROUNDS; i++) {
        sum  += TEA_DELTA;
        v0   += ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
        v1   += ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
    }
    v[0] = v0; v[1] = v1;
}

static void do_slice(unsigned core)
{
    unsigned total = NR_SECONDARY_CORES + 1;
    size_t slice_sz = BUFFER_SIZE / total;
    size_t start    = core * slice_sz;
    size_t len      = (core+1 == total) ? (BUFFER_SIZE - start) : slice_sz;

    int comp_len = (int)len;
    compress_lz77(raw_buf + start, comp_len,
                  comp_slice[core], &comp_len);

    static const unsigned char raw_key[16] = "0123456789012345";
    uint32_t k[4]; memcpy(k, raw_key, sizeof(k));
    int padded = (comp_len + 7) & ~7;
    memcpy(enc_slice[core], comp_slice[core], comp_len);
    memset(enc_slice[core] + comp_len, 0, padded - comp_len);
    for (int loop = 0; loop < TEA_HEAVY_LOOPS; loop++) {
        for (int i = 0; i < padded; i += 8) {
            uint32_t *blk = (uint32_t*)(enc_slice[core] + i);
            tea_encrypt_block(blk, k);
        }
    }
    slice_enc_len[core] = padded;
}

static void remote_fn(struct __regs *regs __unused, void *arg __unused)
{
    unsigned core = ukplat_lcpu_id();
    do_slice(core);
}

static void run_bsp_slice(void)
{
    do_slice(0);
}

static void run_single_core_timed(int sock,
    uint64_t *compute_us,
    uint64_t *send_us,
    uint32_t *crc_out)
{
    for (size_t i = 0; i < BUFFER_SIZE; i++)
    raw_buf[i] = rand() & 0xFF;

    uint64_t c0 = ukplat_monotonic_clock();

    unsigned total = NR_SECONDARY_CORES + 1;
    for (unsigned c = 0; c < total; c++) {
        do_slice(c);
    }

    size_t packet_len = 0;
    for (unsigned c = 0; c < total; c++)
    packet_len += slice_enc_len[c];
    unsigned char *packet = malloc(packet_len);
    size_t pos = 0;
    for (unsigned c = 0; c < total; c++) {
    memcpy(packet + pos, enc_slice[c], slice_enc_len[c]);
    pos += slice_enc_len[c];
    }

    uint32_t crc_n = compute_crc(packet, packet_len);
    uint32_t len_n = packet_len;

    uint64_t c1 = ukplat_monotonic_clock();
    *compute_us = c1 - c0;

    uint64_t s0 = ukplat_monotonic_clock();

    uint32_t crc_n_hton = htonl(crc_n);
    uint32_t len_n_hton = htonl(len_n);
    write(sock, &crc_n_hton, sizeof(crc_n_hton));
    write(sock, &len_n_hton, sizeof(len_n_hton));
    write(sock, packet, packet_len);

    uint64_t s1 = ukplat_monotonic_clock();
    *send_us = s1 - s0;

    *crc_out = crc_n;

    free(packet);
}

int main(void) {
    crc32_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    __lcpuidx secondaries[MAX_SECONDARY_CORES] = {1,2,3};
    unsigned num_sec = NR_SECONDARY_CORES;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT)
    };
    inet_pton(AF_INET, SERVER_IP, &srv.sin_addr);
    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect"); return 1;
    }
    printf("Connected.\n");

    uint64_t t0 = ukplat_monotonic_clock();

    // ----------- MULTICORE TEST START -----------

    for (int test = 0; test < REPEAT_TEST; test++) {
        for (size_t i = 0; i < BUFFER_SIZE; i++)
            raw_buf[i] = rand() & 0xFF;

        uint64_t c0 = ukplat_monotonic_clock();

        struct ukplat_lcpu_func fn = { .fn = remote_fn, .user = NULL };
        ukplat_lcpu_run(secondaries, &num_sec, &fn, 0);
        run_bsp_slice();
        ukplat_lcpu_wait(secondaries, &num_sec, 0);

        // concat & CRC
        size_t total = NR_SECONDARY_CORES + 1;
        size_t packet_len = 0;
        for (unsigned c = 0; c < total; c++)
            packet_len += slice_enc_len[c];
        unsigned char *packet = malloc(packet_len);
        size_t pos = 0;
        for (unsigned c = 0; c < total; c++) {
            memcpy(packet + pos, enc_slice[c], slice_enc_len[c]);
            pos += slice_enc_len[c];
        }
        uint32_t crc_n = htonl(compute_crc(packet, packet_len));
        uint32_t len_n = htonl(packet_len);

        uint64_t c1 = ukplat_monotonic_clock();
        uint64_t total_compute_us = (c1 - c0);

        // — measure send time ——
        uint64_t s0 = ukplat_monotonic_clock();
        write(sock, &crc_n, sizeof(crc_n));
        write(sock, &len_n, sizeof(len_n));
        write(sock, packet, packet_len);

        uint64_t s1 = ukplat_monotonic_clock();
        uint64_t total_send_us = (s1 - s0);

        // convert to milliseconds
        double io_ms      = total_send_us    / 1000.0;
        double comp_ms    = total_compute_us / 1000.0;
        double total_ms   = (total_send_us + total_compute_us) / 1000.0;

        printf("Test %2d — I/O: %5.1f ms, compute: %5.1f ms, total: %5.1f ms, CRC: 0x%08x\n",
               test,
               io_ms,
               comp_ms,
               total_ms,
               ntohl(crc_n));

        free(packet);
        millisleep(500);
    }

    // ----------- MULTICORE TEST END ---------------

    // ----------- SINGLE CORE TEST START -----------

    // for (int test = 0; test < REPEAT_TEST; test++) {
    //     uint64_t total_compute_us, total_send_us;
    //     uint32_t actual_crc;
    
    //     run_single_core_timed(sock,
    //                           &total_compute_us,
    //                           &total_send_us,
    //                           &actual_crc);
    
    //     // convert to ms
    //     double io_ms    = total_send_us    / 1000.0;
    //     double comp_ms  = total_compute_us / 1000.0;
    //     double tot_ms   = (total_send_us + total_compute_us) / 1000.0;
    
    //     printf("Test %2d — I/O: %5.1f ms, compute: %5.1f ms, total: %5.1f ms, CRC: 0x%08x\n",
    //            test,
    //            io_ms,
    //            comp_ms,
    //            tot_ms,
    //            actual_crc);
    
    //     millisleep(500);
    // }

    // ----------- SINGLE CORE TEST END -----------

    uint64_t t1 = ukplat_monotonic_clock();

    close(sock);
    return 0;
}
