/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_posix_extensions.h>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <errno.h>
#include <net/if.h>

#include <odp_api.h>
#include <odp_ethtool_stats.h>
#include <odp_debug_internal.h>
#include <odp_errno_define.h>

/*
 * Suppress bounds warnings about interior zero length arrays. Such an array
 * is used intentionally in sset_info.
 */
#if __GNUC__ >= 10
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
#endif

static struct ethtool_gstrings *get_stringset(int fd, struct ifreq *ifr)
{
	struct {
		struct ethtool_sset_info hdr;
		uint32_t buf[1]; /* overlaps with hdr.data[] */
	} sset_info;
	struct ethtool_drvinfo drvinfo;
	uint32_t len;
	struct ethtool_gstrings *strings;
	ptrdiff_t drvinfo_offset = offsetof(struct ethtool_drvinfo, n_stats);

	sset_info.hdr.cmd = ETHTOOL_GSSET_INFO;
	sset_info.hdr.reserved = 0;
	sset_info.hdr.sset_mask = 1ULL << ETH_SS_STATS;
	ifr->ifr_data =  (void *)&sset_info;
	if (ioctl(fd, SIOCETHTOOL, ifr) == 0) {
		len = sset_info.hdr.sset_mask ? sset_info.hdr.data[0] : 0;
	} else if (errno == EOPNOTSUPP && drvinfo_offset != 0) {
		/* Fallback for old kernel versions */
		drvinfo.cmd = ETHTOOL_GDRVINFO;
		ifr->ifr_data = (void *)&drvinfo;
		if (ioctl(fd, SIOCETHTOOL, ifr)) {
			_odp_errno = errno;
			ODP_ERR("Cannot get stats information\n");
			return NULL;
		}
		len = *(uint32_t *)(void *)((char *)&drvinfo + drvinfo_offset);
	} else {
		_odp_errno = errno;
		return NULL;
	}

	if (!len) {
		ODP_ERR("len is zero");
		return NULL;
	}

	strings = calloc(1, sizeof(*strings) + len * ETH_GSTRING_LEN);
	if (!strings) {
		ODP_ERR("alloc failed\n");
		return NULL;
	}

	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = ETH_SS_STATS;
	strings->len = len;
	ifr->ifr_data = (void *)strings;
	if (ioctl(fd, SIOCETHTOOL, ifr)) {
		_odp_errno = errno;
		ODP_ERR("Cannot get stats information\n");
		free(strings);
		return NULL;
	}

	return strings;
}

static int ethtool_stats(int fd, struct ifreq *ifr, odp_pktio_stats_t *stats)
{
	struct ethtool_gstrings *strings;
	struct ethtool_stats *estats;
	unsigned int n_stats, i;
	int err;
	int cnts;

	strings = get_stringset(fd, ifr);
	if (!strings)
		return -1;

	n_stats = strings->len;
	if (n_stats < 1) {
		ODP_ERR("no stats available\n");
		free(strings);
		return -1;
	}

	estats = calloc(1, n_stats * sizeof(uint64_t) +
			sizeof(struct ethtool_stats));
	if (!estats) {
		free(strings);
		return -1;
	}

	estats->cmd = ETHTOOL_GSTATS;
	estats->n_stats = n_stats;
	ifr->ifr_data = (void *)estats;
	err = ioctl(fd, SIOCETHTOOL, ifr);
	if (err < 0) {
		_odp_errno = errno;
		free(strings);
		free(estats);
		return -1;
	}

	cnts = 0;
	for (i = 0; i < n_stats; i++) {
		char *cnt = (char *)&strings->data[i * ETH_GSTRING_LEN];
		uint64_t val = estats->data[i];

		if (!strcmp(cnt, "rx_octets") ||
		    !strcmp(cnt, "rx_bytes")) {
			stats->in_octets = val;
			cnts++;
		} else if (!strcmp(cnt, "rx_packets")) {
			stats->in_packets = val;
			cnts++;
		} else if (!strcmp(cnt, "rx_ucast_packets") ||
			   !strcmp(cnt, "rx_unicast")) {
			stats->in_ucast_pkts = val;
			cnts++;
		} else if (!strcmp(cnt, "rx_broadcast") ||
			   !strcmp(cnt, "rx_bcast_packets")) {
			stats->in_bcast_pkts = val;
			cnts++;
		} else if (!strcmp(cnt, "rx_multicast") ||
			   !strcmp(cnt, "rx_mcast_packets")) {
			stats->in_mcast_pkts = val;
			cnts++;
		} else if (!strcmp(cnt, "rx_discards") ||
			   !strcmp(cnt, "rx_dropped")) {
			stats->in_discards = val;
			cnts++;
		} else if (!strcmp(cnt, "rx_errors")) {
			stats->in_errors = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_octets") ||
			   !strcmp(cnt, "tx_bytes")) {
			stats->out_octets = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_packets")) {
			stats->out_packets = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_ucast_packets") ||
			   !strcmp(cnt, "tx_unicast")) {
			stats->out_ucast_pkts = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_broadcast") ||
			   !strcmp(cnt, "tx_bcast_packets")) {
			stats->out_bcast_pkts = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_multicast") ||
			   !strcmp(cnt, "tx_mcast_packets")) {
			stats->out_mcast_pkts = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_discards") ||
			   !strcmp(cnt, "tx_dropped")) {
			stats->out_discards = val;
			cnts++;
		} else if (!strcmp(cnt, "tx_errors")) {
			stats->out_errors = val;
			cnts++;
		}
	}

	free(strings);
	free(estats);

	/* Ethtool strings came from kernel driver. Name of that
	 * strings is not universal. Current function needs to be updated
	 * if your driver has different names for counters */
	if (cnts < 14)
		return -1;

	return 0;
}

int _odp_ethtool_stats_get_fd(int fd, const char *name, odp_pktio_stats_t *stats)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", name);

	return ethtool_stats(fd, &ifr, stats);
}
