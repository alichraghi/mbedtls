/**
 * \file psa/crypto_platform.h
 *
 * \brief PSA cryptography module: Mbed TLS platfom definitions
 */
/*
 *  Copyright (C) 2018, ARM Limited, All Rights Reserved
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
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#ifndef PSA_CRYPTO_PLATFORM_H
#define PSA_CRYPTO_PLATFORM_H

/* Include the Mbed TLS configuration file, the way Mbed TLS does it
 * in each of its header files. */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "../mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

/* PSA requires several types which C99 provides in stdint.h. */
#include <stdint.h>

#endif /* PSA_CRYPTO_PLATFORM_H */
