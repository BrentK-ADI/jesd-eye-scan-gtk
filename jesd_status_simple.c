/***************************************************************************//**
*   @file   jesd_status_simple.c
*   @brief  JESD204 Status Simple Information Utility
*   @author Brent Kowal (brent.kowal@analog.com)
********************************************************************************
* Copyright 2025 (c) Analog Devices, Inc.
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*  - Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  - Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*  - Neither the name of Analog Devices, Inc. nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*  - The use of this software may or may not infringe the patent rights
*    of one or more patent holders.  This license does not release you
*    from the requirement that you obtain separate licenses from these
*    patent holders to use this software.
*  - Use of the software either in source or binary form, must be run
*    on or directly connected to an Analog Devices Inc. component.
*
* THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

#include "jesd_common.h"

#define BASEDIR "/sys/bus/platform/drivers"

static struct jesd204b_laneinfo lane_info[MAX_LANES];
static char jesd_devices[MAX_DEVICES][PATH_MAX];


static void print_lane_status(struct jesd204b_laneinfo *info, unsigned lanes, int encoder)
{
	struct jesd204b_laneinfo *lane;
	int i;

	for (i = 0; i < lanes; i++) {
		lane = &info[i];
		printf("--- Lane %d\n", i);
		printf("   DID: %u, BID: %u, LID: %u, L: %u, SCR: %u, F: %u\n",
			lane->did, lane->bid, lane->lid, lane->l, lane->scr, lane->f);
		printf("   K: %u, M: %u, N: %u, CS: %u, S: %u, N': %u, HD: %u\n",
			info->k, info->m, info->n, info->cs, info->s, info->nd, info->hd);
		printf("   FCHK:0x%X, CF: %u\n", info->fchk, info->cf);
		printf("   ADJCNT: %u, PHYADJ: %u, ADJDIR: %u, JESD Ver: %u, SUBCLASS: %u\n",
			info->adjcnt, info->phyadj, info->adjdir, info->jesdv, info->subclassv);
		printf("   FC: %lu\n", info->fc);
		printf("   Lane Errors: %u\n", info->lane_errors);

		if(encoder == JESD204_ENCODER_8B10B) {
			printf("   Lane Latency: %u (MultiFrames), %u (Octets)\n", 
				info->lane_latency_multiframes, info->lane_latency_octets);
			printf("   CGS State: %s\n", info->cgs_state);
			printf("   Init. Frame Sync: %s\n", info->init_frame_sync);
			printf("   Init. Lane Align Sync: %s\n", info->init_lane_align_seq);
		} else {
			printf("   Ext. Multiblock Align: %s\n", info->ext_multiblock_align_state);
		}
	}
}

static void print_jesd_status(const char *device, int encoder)
{
	struct jesd204b_jesd204_status info;
	int lane_cnt;

	read_jesd204_status(get_full_device_path(BASEDIR, device), &info);
	printf("Device: %s - (%s)\n", device, encoder == JESD204_ENCODER_8B10B ? "8B10B" : "64B66B");
	printf("------------------------------\n");
	
	printf("Link State: %s\n", info.link_state);
	printf("Link Status: %s\n", info.link_status);	

	/* Note: All of the clock values are reported as MHz values when available,
	   and can be parsed using scanf, strtod, etc if being using in calculations
	 */
	printf("Link Clock (Measured): %s\n", info.measured_link_clock);
	printf("Link Clock (Reported): %s\n", info.reported_link_clock);
	printf("Device Clock (Measured): %s\n", info.measured_device_clock);
	printf("Device Clock (Reported): %s\n", info.reported_device_clock);	
	printf("Device Clock (Desired): %s\n", info.desired_device_clock);
	printf("Lane Rate: %s\n", info.lane_rate);
	printf("Lane Rate / %d: %s\n", encoder == JESD204_ENCODER_8B10B ? 40 : 66, info.lane_rate_div);
	printf("LMFC Rate: %s\n", info.lmfc_rate);
	printf("SYSREF Captured: %s\n", info.sysref_captured);
	printf("SYSREF Alignment Error: %s\n", info.sysref_alignment_error);
	if(encoder == JESD204_ENCODER_8B10B) {
		printf("SYNC~: %s\n", info.external_reset);
	}

	lane_cnt = read_all_laneinfo(get_full_device_path(BASEDIR, device), lane_info);
	if (lane_cnt) {
		print_lane_status(lane_info, lane_cnt, encoder);			
	}

	//Just some separation
	printf("\n\n");
}


int main(int argc, char *argv[])
{
	int dev_num = 0;
	int dev_idx = 0;
	int encoder = 0;

	dev_num = jesd_find_devices(BASEDIR, JESD204_RX_DRIVER_NAME, "status", jesd_devices, 0);
	dev_num = jesd_find_devices(BASEDIR, JESD204_TX_DRIVER_NAME, "status", jesd_devices, dev_num);
	if (!dev_num) {
		fprintf(stderr, "Failed to find JESD devices\n");
		return 0;
	}

	printf("Found %d JESD204 Link Layer peripherals\n\n", dev_num);
	
	for( dev_idx = 0; dev_idx < dev_num; dev_idx++ ) {
		encoder = read_encoding(get_full_device_path(BASEDIR, jesd_devices[dev_idx]));
		print_jesd_status(jesd_devices[dev_idx], encoder);
	}

	return 0;
}
