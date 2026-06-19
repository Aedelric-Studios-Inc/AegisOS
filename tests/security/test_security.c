/* SPDX-License-Identifier: Proprietary */
/* AegisOS — tests/security/test_security.c
 * Unit tests for the security subsystem.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- Minimal SHA-256 (mirrors security/src/integrity.rs) ---- */

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))

static void sha256_compress_c(uint32_t state[8], const uint8_t blk[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|
               ((uint32_t)blk[i*4+2]<<8)|(uint32_t)blk[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR32(w[i-15],7)^ROTR32(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = ROTR32(w[i-2],17)^ROTR32(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
    uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1   = ROTR32(e,6)^ROTR32(e,11)^ROTR32(e,25);
        uint32_t ch   = (e&f)^(~e&g);
        uint32_t t1   = h+S1+ch+SHA256_K[i]+w[i];
        uint32_t S0   = ROTR32(a,2)^ROTR32(a,13)^ROTR32(a,22);
        uint32_t maj  = (a&b)^(a&c)^(b&c);
        uint32_t t2   = S0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static void sha256_c(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint32_t state[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    uint64_t bitlen = (uint64_t)len * 8;
    uint8_t buf[64];
    size_t fill = 0;
    for (size_t i = 0; i < len; i++) {
        buf[fill++] = data[i];
        if (fill == 64) { sha256_compress_c(state, buf); fill = 0; }
    }
    buf[fill++] = 0x80;
    if (fill > 56) { while (fill < 64) buf[fill++] = 0; sha256_compress_c(state, buf); fill = 0; }
    while (fill < 56) buf[fill++] = 0;
    for (int i = 7; i >= 0; i--) { buf[fill++] = (uint8_t)(bitlen >> (i*8)); }
    sha256_compress_c(state, buf);
    for (int i = 0; i < 8; i++) {
        out[i*4+0]=(uint8_t)(state[i]>>24); out[i*4+1]=(uint8_t)(state[i]>>16);
        out[i*4+2]=(uint8_t)(state[i]>>8);  out[i*4+3]=(uint8_t)(state[i]);
    }
}

/* ---- Audit ring-buffer simulation ---- */

#define AUDIT_SIZE 16
typedef struct { uint8_t event; uint32_t tid; uint64_t detail; } audit_rec_t;
static audit_rec_t audit_buf[AUDIT_SIZE];
static int audit_head = 0;

static void audit_log_test(uint8_t event, uint32_t tid, uint64_t detail) {
    audit_buf[audit_head % AUDIT_SIZE] = (audit_rec_t){ event, tid, detail };
    audit_head++;
}

/* ---- Tests ---- */

static void test_sha256_nist_vector(void) {
    /* SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
    const uint8_t expected[32] = {
        0xba,0x78,0x16,0xbf, 0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde, 0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3, 0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61, 0xf2,0x00,0x15,0xad,
    };
    uint8_t got[32];
    sha256_c((const uint8_t *)"abc", 3, got);
    assert(memcmp(got, expected, 32) == 0);
    printf("  [PASS] SHA-256(\"abc\") matches expected value\n");
}

static void test_sha256_empty(void) {
    /* SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
    const uint8_t expected[32] = {
        0xe3,0xb0,0xc4,0x42, 0x98,0xfc,0x1c,0x14,
        0x9a,0xfb,0xf4,0xc8, 0x99,0x6f,0xb9,0x24,
        0x27,0xae,0x41,0xe4, 0x64,0x9b,0x93,0x4c,
        0xa4,0x95,0x99,0x1b, 0x78,0x52,0xb8,0x55,
    };
    uint8_t got[32];
    const uint8_t empty[1] = {0};
    sha256_c(empty, 0, got);
    assert(memcmp(got, expected, 32) == 0);
    printf("  [PASS] SHA-256(\"\") matches expected value\n");
}

static void test_sha256_deterministic(void) {
    uint8_t a[32], b[32];
    sha256_c((const uint8_t *)"AegisOS", 7, a);
    sha256_c((const uint8_t *)"AegisOS", 7, b);
    assert(memcmp(a, b, 32) == 0);
    printf("  [PASS] SHA-256 is deterministic\n");
}

static void test_sha256_avalanche(void) {
    uint8_t a[32], b[32];
    sha256_c((const uint8_t *)"AegisOS1", 8, a);
    sha256_c((const uint8_t *)"AegisOS2", 8, b);
    int diff = 0;
    for (int i = 0; i < 32; i++) diff += (a[i] != b[i]);
    assert(diff > 20); /* most bytes should differ */
    printf("  [PASS] SHA-256 avalanche effect (single-bit change alters >20 bytes)\n");
}

static void test_audit_ring_wrap(void) {
    audit_head = 0;
    for (int i = 0; i < AUDIT_SIZE + 3; i++) audit_log_test(1, (uint32_t)i, (uint64_t)i);
    /* Head should have wrapped; latest entry is at (audit_head-1) % AUDIT_SIZE */
    int last = (audit_head - 1) % AUDIT_SIZE;
    assert(audit_buf[last].tid == (uint32_t)(AUDIT_SIZE + 2));
    printf("  [PASS] Audit log ring buffer wraps correctly\n");
}

static void test_integrity_detect_tamper(void) {
    uint8_t data[] = "AegisOS secure image";
    uint8_t hash[32];
    sha256_c(data, sizeof(data)-1, hash);
    /* Tamper with one byte */
    data[0] ^= 0x01;
    uint8_t tampered_hash[32];
    sha256_c(data, sizeof(data)-1, tampered_hash);
    assert(memcmp(hash, tampered_hash, 32) != 0);
    printf("  [PASS] Hash detects single-byte tamper in data\n");
}

int main(void) {
    printf("[test_security] Running security unit tests...\n");
    test_sha256_nist_vector();
    test_sha256_empty();
    test_sha256_deterministic();
    test_sha256_avalanche();
    test_audit_ring_wrap();
    test_integrity_detect_tamper();
    printf("[test_security] All tests passed\n");
    return 0;
}
