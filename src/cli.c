/* DOA2U Save Transfer - command line MAC search (macOS / Linux)
 *   cc -O3 -pthread -o doau-search src/cli.c src/doau_core.c src/bf_tables.c
 *   ./doau-search ups.dat 00:50:F2 [threads]
 */
#include "doau_core.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint8_t buf[UPS_SIZE], oui[3], found_mac[6];
static volatile int stop_flag, found;
static volatile uint32_t prog[256];
typedef struct { uint32_t lo, hi; int idx; } job_t;

static void *worker(void *p){
    job_t *j = p; uint8_t mac[6];
    if (ups_search(buf, oui, j->lo, j->hi, &stop_flag, &prog[j->idx], mac)) {
        if (!__sync_lock_test_and_set(&found, 1)) memcpy(found_mac, mac, 6);
        stop_flag = 1;
    }
    return NULL;
}

int main(int argc, char **argv){
    FILE *f; int n, i; pthread_t th[256]; job_t jobs[256]; uint32_t per; char s[32];
    if (argc < 3) { fprintf(stderr, "usage: %s ups.dat OUI(e.g. 00:50:F2) [threads]\n", argv[0]); return 2; }
    f = fopen(argv[1], "rb");
    if (!f || fread(buf, 1, UPS_SIZE, f) != UPS_SIZE || fgetc(f) != EOF) { fprintf(stderr, "not a %u-byte ups.dat\n", UPS_SIZE); return 2; }
    fclose(f);
    if (!parse_hex(argv[2], oui, 3)) { fprintf(stderr, "OUI must be 6 hex digits\n"); return 2; }
    n = argc > 3 ? atoi(argv[3]) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    if (n > 256) n = 256;
    per = (0x1000000u + n - 1) / n;
    for (i = 0; i < n; i++) {
        jobs[i].idx = i; jobs[i].lo = per * i;
        jobs[i].hi = per * (i + 1) > 0x1000000u ? 0x1000000u : per * (i + 1);
        pthread_create(&th[i], NULL, worker, &jobs[i]);
    }
    while (!stop_flag) {
        uint64_t t = 0; int done = 1;
        for (i = 0; i < n; i++) t += prog[i];
        fprintf(stderr, "\r%5.1f%%", t * 100.0 / 0x1000000u);
        usleep(500000);
        for (i = 0; i < n; i++) if (prog[i] < jobs[i].hi - jobs[i].lo) done = 0;
        if (done) break;
    }
    for (i = 0; i < n; i++) pthread_join(th[i], NULL);
    fprintf(stderr, "\n");
    if (found) { format_mac(found_mac, s); printf("%s\n", s); return 0; }
    printf("not found under this prefix\n"); return 1;
}
