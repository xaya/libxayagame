/*
 * Standalone SHA-256 implementation.
 * Public domain, based on Brad Conte's implementation.
 */

#ifndef WASM_SHIMS_SHA256_H
#define WASM_SHIMS_SHA256_H

#include <cstddef>
#include <cstdint>

struct sha256_ctx {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
};

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t *hash);

#endif // WASM_SHIMS_SHA256_H
