/**
 *  Constant-time functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * The following functions are implemented without using comparison operators, as those
 * might be translated to branches by some compilers on some platforms.
 */

#include <limits.h>

#include "common.h"
#include "constant_time_internal.h"
#include "mbedtls/constant_time.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_SSL_TLS_C)
#include "ssl_misc.h"
#endif

#if defined(MBEDTLS_RSA_C)
#include "mbedtls/rsa.h"
#endif

#include <string.h>
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#define PSA_TO_MBEDTLS_ERR(status) PSA_TO_MBEDTLS_ERR_LIST(status,    \
                                                           psa_to_ssl_errors,              \
                                                           psa_generic_status_to_mbedtls)
#endif

/*
 * Define MBEDTLS_EFFICIENT_UNALIGNED_VOLATILE_ACCESS where assembly is present to
 * perform fast unaligned access to volatile data.
 *
 * This is needed because mbedtls_get_unaligned_uintXX etc don't support volatile
 * memory accesses.
 *
 * Some of these definitions could be moved into alignment.h but for now they are
 * only used here.
 */
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS) && \
    (defined(MBEDTLS_CT_ARM_ASM) || defined(MBEDTLS_CT_AARCH64_ASM))

#define MBEDTLS_EFFICIENT_UNALIGNED_VOLATILE_ACCESS

static inline uint32_t mbedtls_get_unaligned_volatile_uint32(volatile const unsigned char *p)
{
    /* This is UB, even where it's safe:
     *    return *((volatile uint32_t*)p);
     * so instead the same thing is expressed in assembly below.
     */
    uint32_t r;
#if defined(MBEDTLS_CT_ARM_ASM)
    asm volatile ("ldr %0, [%1]" : "=r" (r) : "r" (p) :);
#elif defined(MBEDTLS_CT_AARCH64_ASM)
    asm volatile ("ldr %w0, [%1]" : "=r" (r) : "r" (p) :);
#else
#error No assembly defined for mbedtls_get_unaligned_volatile_uint32
#endif
    return r;
}
#endif /* defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS) &&
          (defined(MBEDTLS_CT_ARM_ASM) || defined(MBEDTLS_CT_AARCH64_ASM)) */

int mbedtls_ct_memcmp(const void *a,
                      const void *b,
                      size_t n)
{
    size_t i = 0;
    /*
     * `A` and `B` are cast to volatile to ensure that the compiler
     * generates code that always fully reads both buffers.
     * Otherwise it could generate a test to exit early if `diff` has all
     * bits set early in the loop.
     */
    volatile const unsigned char *A = (volatile const unsigned char *) a;
    volatile const unsigned char *B = (volatile const unsigned char *) b;
    uint32_t diff = 0;

#if defined(MBEDTLS_EFFICIENT_UNALIGNED_VOLATILE_ACCESS)
    for (; (i + 4) <= n; i += 4) {
        uint32_t x = mbedtls_get_unaligned_volatile_uint32(A + i);
        uint32_t y = mbedtls_get_unaligned_volatile_uint32(B + i);
        diff |= x ^ y;
    }
#endif

    for (; i < n; i++) {
        /* Read volatile data in order before computing diff.
         * This avoids IAR compiler warning:
         * 'the order of volatile accesses is undefined ..' */
        unsigned char x = A[i], y = B[i];
        diff |= x ^ y;
    }

    return (int) diff;
}

#if defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT)

void mbedtls_ct_memmove_left(void *start, size_t total, size_t offset)
{
    volatile unsigned char *buf = start;
    for (size_t i = 0; i < total; i++) {
        mbedtls_ct_condition_t no_op = mbedtls_ct_bool_gt(total - offset, i);
        /* The first `total - offset` passes are a no-op. The last
         * `offset` passes shift the data one byte to the left and
         * zero out the last byte. */
        for (size_t n = 0; n < total - 1; n++) {
            unsigned char current = buf[n];
            unsigned char next    = buf[n+1];
            buf[n] = mbedtls_ct_uint_if_new(no_op, current, next);
        }
        buf[total-1] = mbedtls_ct_uint_if0(no_op, buf[total-1]);
    }
}

#endif /* MBEDTLS_PKCS1_V15 && MBEDTLS_RSA_C && ! MBEDTLS_RSA_ALT */

void mbedtls_ct_memcpy_if(mbedtls_ct_condition_t condition,
                          unsigned char *dest,
                          const unsigned char *src1,
                          const unsigned char *src2,
                          size_t len)
{
    const uint32_t mask     = (uint32_t) condition;
    const uint32_t not_mask = (uint32_t) ~mbedtls_ct_compiler_opaque(condition);

    /* If src2 is NULL and condition == 0, then this function has no effect.
     * In this case, copy from dest back into dest. */
    if (src2 == NULL) {
        src2 = dest;
    }

    /* dest[i] = c1 == c2 ? src[i] : dest[i] */
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
    for (; (i + 4) <= len; i += 4) {
        uint32_t a = mbedtls_get_unaligned_uint32(src1 + i) & mask;
        uint32_t b = mbedtls_get_unaligned_uint32(src2 + i) & not_mask;
        mbedtls_put_unaligned_uint32(dest + i, a | b);
    }
#endif /* MBEDTLS_EFFICIENT_UNALIGNED_ACCESS */
    for (; i < len; i++) {
        dest[i] = (src1[i] & mask) | (src2[i] & not_mask);
    }
}

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)

void mbedtls_ct_memcpy_offset(unsigned char *dest,
                              const unsigned char *src,
                              size_t offset,
                              size_t offset_min,
                              size_t offset_max,
                              size_t len)
{
    size_t offsetval;

    for (offsetval = offset_min; offsetval <= offset_max; offsetval++) {
         mbedtls_ct_memcpy_if(mbedtls_ct_bool_eq(offsetval, offset), dest, src + offsetval, NULL,
                              len);
    }
}

#endif /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

#if defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT)

void mbedtls_ct_zeroize_if(mbedtls_ct_condition_t condition, void *buf, size_t len)
{
    uint32_t mask = (uint32_t) ~condition;
    uint8_t *p = (uint8_t *) buf;
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
    for (; (i + 4) <= len; i += 4) {
        mbedtls_put_unaligned_uint32((void *) (p + i),
                                     mbedtls_get_unaligned_uint32((void *) (p + i)) & mask);
    }
#endif
    for (; i < len; i++) {
        p[i] = p[i] & mask;
    }
}

#endif /* defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT) */
