/* Copyright (c) 2017-2018, Linaro Limited
 * Copyright (c) 2022, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_ABI_EVENT_TYPES_H_
#define ODP_ABI_EVENT_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <odp/api/deprecated.h>

/** @internal Dummy type for strong typing */
typedef struct { char dummy; /**< @internal Dummy */ } _odp_abi_event_t;

/** @ingroup odp_event
 *  @{
 */

typedef _odp_abi_event_t *odp_event_t;

#define ODP_EVENT_INVALID  ((odp_event_t)0)

typedef enum {
	ODP_EVENT_BUFFER = 1,
	ODP_EVENT_PACKET = 2,
	ODP_EVENT_TIMEOUT = 3,
	ODP_EVENT_IPSEC_STATUS = 5,
	ODP_EVENT_PACKET_VECTOR = 6,
	ODP_EVENT_PACKET_TX_COMPL = 7,
	ODP_EVENT_DMA_COMPL = 8,
} odp_event_type_t;

typedef enum {
	ODP_EVENT_NO_SUBTYPE = 0,
	ODP_EVENT_PACKET_BASIC = 1,
	ODP_EVENT_PACKET_CRYPTO = 2,
	ODP_EVENT_PACKET_IPSEC = 3,
	ODP_EVENT_PACKET_COMP = 4
} odp_event_subtype_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
