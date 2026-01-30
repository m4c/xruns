/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Marcin Szewczyk-Wilgan <marcins@m4c.pl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.

* xruns - Monitor audio buffer xruns on FreeBSD
 *
 * Usage:
 *   xruns [-d device] [-p] [-w] [-i interval]
 *
 * Options:
 *   -d N        Monitor device pcmN (default: default device)
 *   -p          Show only playback channels (no recording)
 *   -w          Watch mode - loop and show only changes
 *   -i SEC      Interval in seconds for watch mode (default: 1)
 *   -h          Show help
 *
 * Examples:
 *   xruns              Show xruns for default device
 *   xruns -d 1         Show xruns for pcm1
 *   xruns -d 0 -p      Show only playback xruns for pcm0
 *   xruns -d 0 -p -w   Watch playback xruns on pcm0
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/sndstat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <mixer.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CHANNELS 64

struct chan_xruns {
	char name[256];
	int direction;  /* 0 = input, 1 = output */
	uint64_t xruns;
	uint64_t prev_xruns;
	bool active;
};

struct xruns_state {
	int unit;
	char devname[64];
	struct chan_xruns chans[MAX_CHANNELS];
	int nchan;
};

static void
get_timestamp(char *buf, size_t size)
{
	struct timespec ts;
	struct tm tm;

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);
	snprintf(buf, size, "%02d:%02d:%02d.%03ld",
	    tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);
}

static int
read_xruns(int target_unit, struct xruns_state *state)
{
	nvlist_t *nvl;
	const nvlist_t * const *di;
	const nvlist_t * const *cdi;
	struct sndstioc_nv_arg arg;
	size_t nitems, nchans;
	int fd, unit;
	size_t i, j;

	if ((fd = open("/dev/sndstat", O_RDONLY)) < 0) {
		warn("open(/dev/sndstat)");
		return (-1);
	}

	if (ioctl(fd, SNDSTIOC_REFRESH_DEVS, NULL) < 0) {
		warn("ioctl(SNDSTIOC_REFRESH_DEVS)");
		close(fd);
		return (-1);
	}

	arg.nbytes = 0;
	arg.buf = NULL;
	if (ioctl(fd, SNDSTIOC_GET_DEVS, &arg) < 0) {
		warn("ioctl(SNDSTIOC_GET_DEVS#1)");
		close(fd);
		return (-1);
	}

	if ((arg.buf = malloc(arg.nbytes)) == NULL) {
		warn("malloc");
		close(fd);
		return (-1);
	}

	if (ioctl(fd, SNDSTIOC_GET_DEVS, &arg) < 0) {
		warn("ioctl(SNDSTIOC_GET_DEVS#2)");
		free(arg.buf);
		close(fd);
		return (-1);
	}

	if ((nvl = nvlist_unpack(arg.buf, arg.nbytes, 0)) == NULL) {
		warn("nvlist_unpack");
		free(arg.buf);
		close(fd);
		return (-1);
	}

	if (nvlist_empty(nvl) || !nvlist_exists(nvl, SNDST_DSPS)) {
		warnx("no soundcards attached");
		nvlist_destroy(nvl);
		free(arg.buf);
		close(fd);
		return (-1);
	}

	/* Use default unit if not specified */
	if (target_unit < 0)
		target_unit = mixer_get_dunit();

	/* Find the requested device */
	di = nvlist_get_nvlist_array(nvl, SNDST_DSPS, &nitems);
	for (i = 0; i < nitems; i++) {
		if (strcmp(nvlist_get_string(di[i], SNDST_DSPS_PROVIDER),
		    SNDST_DSPS_SOUND4_PROVIDER) != 0)
			continue;

		if (!nvlist_exists(di[i], SNDST_DSPS_PROVIDER_INFO))
			continue;

		unit = nvlist_get_number(nvlist_get_nvlist(di[i],
		    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_UNIT);

		if (unit == target_unit)
			break;
	}

	if (i == nitems) {
		warnx("device pcm%d not found", target_unit);
		nvlist_destroy(nvl);
		free(arg.buf);
		close(fd);
		return (-1);
	}

	state->unit = target_unit;
	strlcpy(state->devname, nvlist_get_string(di[i], SNDST_DSPS_NAMEUNIT),
	    sizeof(state->devname));

	/* Get channel info */
	if (!nvlist_exists(nvlist_get_nvlist(di[i], SNDST_DSPS_PROVIDER_INFO),
	    SNDST_DSPS_SOUND4_CHAN_INFO)) {
		warnx("no channel info for %s", state->devname);
		nvlist_destroy(nvl);
		free(arg.buf);
		close(fd);
		return (-1);
	}

	cdi = nvlist_get_nvlist_array(
	    nvlist_get_nvlist(di[i], SNDST_DSPS_PROVIDER_INFO),
	    SNDST_DSPS_SOUND4_CHAN_INFO, &nchans);

	state->nchan = 0;
	for (j = 0; j < nchans && j < MAX_CHANNELS; j++) {
		struct chan_xruns *ch = &state->chans[state->nchan];
		int caps;

		strlcpy(ch->name,
		    nvlist_get_string(cdi[j], SNDST_DSPS_SOUND4_CHAN_NAME),
		    sizeof(ch->name));

		caps = nvlist_get_number(cdi[j], SNDST_DSPS_SOUND4_CHAN_CAPS);
		ch->direction = (caps & PCM_CAP_INPUT) ? 0 : 1;

		ch->prev_xruns = ch->xruns;
		ch->xruns = nvlist_get_number(cdi[j], SNDST_DSPS_SOUND4_CHAN_XRUNS);
		ch->active = true;

		state->nchan++;
	}

	nvlist_destroy(nvl);
	free(arg.buf);
	close(fd);

	return (0);
}

static void
print_xruns(struct xruns_state *state, bool play_only, bool show_timestamp)
{
	char ts[32] = "";

	if (show_timestamp)
		get_timestamp(ts, sizeof(ts));

	for (int i = 0; i < state->nchan; i++) {
		struct chan_xruns *ch = &state->chans[i];

		if (play_only && ch->direction == 0)
			continue;

		if (show_timestamp)
			printf("%s %s: %lu xruns\n", ts, ch->name, ch->xruns);
		else
			printf("%s: %lu xruns\n", ch->name, ch->xruns);
	}
}

static void
watch_xruns(int target_unit, bool play_only, int interval)
{
	struct xruns_state state = {0};
	struct xruns_state prev_state = {0};
	char ts[32];
	bool first_run = true;

	printf("Watching xruns on pcm%d (Ctrl+C to stop)...\n",
	    target_unit >= 0 ? target_unit : mixer_get_dunit());

	while (1) {
		if (read_xruns(target_unit, &state) < 0) {
			sleep(interval);
			continue;
		}

		for (int i = 0; i < state.nchan; i++) {
			struct chan_xruns *ch = &state.chans[i];

			if (play_only && ch->direction == 0)
				continue;

			/* Find previous value for this channel */
			uint64_t prev = 0;
			if (!first_run) {
				for (int k = 0; k < prev_state.nchan; k++) {
					if (strcmp(prev_state.chans[k].name, ch->name) == 0) {
						prev = prev_state.chans[k].xruns;
						break;
					}
				}
			}

			/* Show if xruns > 0 and changed (or first occurrence) */
			if (ch->xruns > 0 && (first_run || ch->xruns != prev)) {
				get_timestamp(ts, sizeof(ts));
				if (first_run || prev == 0) {
					printf("%s %s: %lu xruns\n",
					    ts, ch->name, ch->xruns);
				} else {
					printf("%s %s: %lu xruns (+%lu)\n",
					    ts, ch->name, ch->xruns,
					    ch->xruns - prev);
				}
				fflush(stdout);
			}
		}

		memcpy(&prev_state, &state, sizeof(state));
		first_run = false;
		sleep(interval);
	}
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: xruns [-d device] [-p] [-w] [-i interval]\n"
	    "\n"
	    "Options:\n"
	    "  -d N      Monitor device pcmN (default: system default)\n"
	    "  -p        Show only playback channels\n"
	    "  -w        Watch mode - loop and show only changes\n"
	    "  -i SEC    Interval in seconds for watch mode (default: 1)\n"
	    "  -h        Show this help\n"
	    "\n"
	    "Examples:\n"
	    "  xruns              Show xruns for default device\n"
	    "  xruns -d 1         Show xruns for pcm1\n"
	    "  xruns -d 0 -p      Show only playback xruns for pcm0\n"
	    "  xruns -d 0 -p -w   Watch playback xruns on pcm0\n"
	);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct xruns_state state = {0};
	int target_unit = -1;
	int interval = 1;
	bool play_only = false;
	bool watch_mode = false;
	int ch;

	while ((ch = getopt(argc, argv, "d:hi:pw")) != -1) {
		switch (ch) {
		case 'd':
			target_unit = atoi(optarg);
			if (target_unit < 0)
				errx(1, "invalid device number: %s", optarg);
			break;
		case 'i':
			interval = atoi(optarg);
			if (interval < 1)
				interval = 1;
			break;
		case 'p':
			play_only = true;
			break;
		case 'w':
			watch_mode = true;
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (watch_mode) {
		watch_xruns(target_unit, play_only, interval);
		/* not reached */
	}

	if (read_xruns(target_unit, &state) < 0)
		return (1);

	printf("%s:\n", state.devname);
	print_xruns(&state, play_only, false);

	return (0);
}
