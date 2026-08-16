#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/sysinfo.h>

#define PORT                8080
#define BUF_MAX             1000000
#define REPEAT_TESTS        10

#define TEA_ROUNDS          4
#define TEA_DELTA           0x9e3779b9u
#define TEA_HEAVY_LOOPS     1000

#define WINDOW_SIZE         128
#define LOOKAHEAD           4
#define MIN_MATCH           4

#define ENC_MAX             (BUF_MAX*2 + 8)
#define MAX_WORKERS         8

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

static void tea_decrypt_block(uint32_t v[2], const uint32_t k[4]) {
    uint32_t v0 = v[0], v1 = v[1],
             sum = TEA_DELTA * TEA_ROUNDS;
    for (int i = 0; i < TEA_ROUNDS; i++) {
        v1 -= ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
        v0 -= ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
        sum -= TEA_DELTA;
    }
    v[0] = v0; v[1] = v1;
}

struct decrypt_args {
    unsigned char *buf;
    int            start, end;
    uint32_t       key[4];
};

static void *decrypt_worker(void *varg) {
    struct decrypt_args *arg = varg;
    unsigned char *b = arg->buf;
    int s = arg->start, e = arg->end;
    uint32_t *k = arg->key;

    for (int loop = 0; loop < TEA_HEAVY_LOOPS; loop++) {
        for (int i = s; i < e; i += 8) {
            uint32_t block[2];
            memcpy(block, b + i, 8);
            tea_decrypt_block(block, k);
            memcpy(b + i, block, 8);
        }
    }
    return NULL;
}

static int decompress_lz77(const unsigned char *in, int in_len,
                           unsigned char *out, int *out_len)
{
    int r = 0, w = 0;
    while (r < in_len) {
        if (in[r] == 1) {
            if (r + 4 > in_len) return -1;
            int offset = (in[r+1] << 8) | in[r+2];
            int length = in[r+3];
            if (w - offset < 0 || w + length > *out_len) return -1;
            memcpy(out + w, out + w - offset, length);
            w += length;
            r += 4;
        } else {
            if (r + 2 > in_len || w + 1 > *out_len) return -1;
            out[w++] = in[r+1];
            r += 2;
        }
    }
    *out_len = w;
    return w;
}

static void millisleep(unsigned int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000 };
    while (nanosleep(&ts, &ts) && errno == EINTR) {}
}

static void run_single_core(unsigned char *enc_buf,
                            int payload_n,
                            unsigned char *mid,
                            unsigned char *raw,
                            uint32_t tea_key[4])
{
    uint32_t calc = compute_crc(enc_buf, payload_n);

    memcpy(mid, enc_buf, payload_n);
    for (int loop = 0; loop < TEA_HEAVY_LOOPS; loop++) {
        for (int i = 0; i < payload_n; i += 8) {
            uint32_t block[2];
            memcpy(block, mid + i, 8);
            tea_decrypt_block(block, tea_key);
            memcpy(mid + i, block, 8);
        }
    }

    int mid_len = payload_n;
    int raw_len = BUF_MAX;
    decompress_lz77(mid, mid_len, raw, &raw_len);
}

int main(void) {
    crc32_init();

    unsigned char enc_buf[ENC_MAX];
    unsigned char mid    [ENC_MAX];
    unsigned char raw    [BUF_MAX];

    uint32_t tea_key[4];
    memcpy(tea_key, "0123456789012345", 16);

    int ncpu = get_nprocs();
    int workers = ncpu < MAX_WORKERS ? ncpu : MAX_WORKERS;
    if (workers < 1) workers = 1;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(PORT)
    };
    bind(srv, (struct sockaddr*)&addr, sizeof(addr));
    listen(srv, 1);
    printf("Listening on %d…\n", PORT);

    int cli = accept(srv, NULL, NULL);
    printf("Client connected.\n");

    for (int t = 0; t < REPEAT_TESTS; t++) {
        struct timespec io_a, io_b;
        clock_gettime(CLOCK_MONOTONIC, &io_a);

        uint32_t crc_n, len_n;
        if (read(cli, &crc_n, 4) != 4 ||
            read(cli, &len_n, 4) != 4) {
            perror("read header"); exit(1);
        }
        uint32_t expected_crc = ntohl(crc_n);
        int payload_n         = ntohl(len_n);

        int got = 0;
        while (got < payload_n) {
            int r = read(cli, enc_buf + got, payload_n - got);
            if (r <= 0) { perror("read"); exit(1); }
            got += r;
        }

        clock_gettime(CLOCK_MONOTONIC, &io_b);
        uint64_t total_io_us = (io_b.tv_sec - io_a.tv_sec)*1000000
                            + (io_b.tv_nsec - io_a.tv_nsec)/1000;

        struct timespec c_a, c_b;
        clock_gettime(CLOCK_MONOTONIC, &c_a);

        uint32_t actual_crc = compute_crc(enc_buf, payload_n);
        if (actual_crc != expected_crc) {
            fprintf(stderr,
                "CRC mismatch: 0x%08x != 0x%08x\n",
                actual_crc, expected_crc);
            exit(1);
        }

        pthread_t      th[MAX_WORKERS];
        struct decrypt_args args[MAX_WORKERS];
        int slice = payload_n / workers;
        for (int w = 0; w < workers; w++) {
            args[w].buf   = mid;
            args[w].start = w * slice;
            args[w].end   = (w+1 == workers)
                            ? payload_n
                            : (w+1)*slice;
            memcpy(args[w].key, tea_key, sizeof(tea_key));

            if (w == 0) memcpy(mid, enc_buf, payload_n);
            pthread_create(&th[w], NULL,
                            decrypt_worker, &args[w]);
        }
        for (int w = 0; w < workers; w++)
            pthread_join(th[w], NULL);

        int mid_len = payload_n;
        int raw_len = BUF_MAX;
        if (decompress_lz77(mid, mid_len, raw, &raw_len) < 0) {
            fprintf(stderr, "LZ77 decompress failed\n");
            exit(1);
        }

        clock_gettime(CLOCK_MONOTONIC, &c_b);
        uint64_t total_compute_us = (c_b.tv_sec - c_a.tv_sec)*1000000
                            + (c_b.tv_nsec - c_a.tv_nsec)/1000;

        printf("Transmission %2d — I/O: %5.1f ms, compute: %5.1f ms, total: %5.1f ms, CRC: 0x%08x\n",
                t,
                total_io_us/1000.0,
                total_compute_us/1000.0,
                (total_io_us + total_compute_us)/1000.0,
                actual_crc);

        millisleep(10);
    }

    printf("Done. Sleeping 10 s…\n");
    millisleep(10000);
    close(cli);
    close(srv);
    return 0;
}
