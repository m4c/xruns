# xruns

Monitor audio buffer xruns on FreeBSD sound devices.

## Overview

`xruns` is a lightweight shell script for monitoring audio buffer underruns/overruns on FreeBSD. It parses output from `sndctl(8)` to extract xrun statistics.

Particularly useful when configuring USB audio devices (DACs, audio interfaces, etc.) — helps diagnose audible clicks, pops, or glitches, and verify bitperfect configuration.

The script is an addendum to the article: [FreeBSD audio diagnostics and optimisation(https://m4c.pl/blog/freebsd-audio-diagnostics-and-optimization/)].

### What are xruns?

An xrun occurs when the audio buffer cannot keep up with the data flow:

- **Underrun (playback)**: Application doesn't supply data fast enough — buffer empties before the sound card needs more samples. Result: clicks, pops, or gaps in audio.
- **Overrun (recording)**: Application doesn't read data fast enough — buffer overflows and incoming samples are lost. Result: gaps or glitches in recorded audio.

**Common causes:**

- Buffer size too small for system load
- High CPU usage or spikes
- Incorrect scheduler priorities
- Latency settings too aggressive (`hw.snd.latency`, `hw.snd.latency_profile`)
- Mismatched sample rates between application and hardware
- USB bandwidth issues (especially with multiple USB audio devices)
- Driver or hardware problems
- Improper sample rate or format conversion

**Why monitor xruns?**

- Zero xruns = clean audio path, no glitches
- Non-zero xruns = something is wrong, needs investigation
- Increasing xruns over time = systematic problem (CPU, USB, driver)
- Sudden spike = transient issue (background process, power management)

## Installation

Copy the script to a directory in your PATH:

```bash
cp xruns /usr/local/bin/xruns
chmod +x /usr/local/bin/xruns
```

## Usage

```
xruns [-d device] [-p] [-w] [-i interval]
```

### Options

| Option | Description |
|--------|-------------|
| `-d N` | Monitor device pcmN (default: system default) |
| `-p` | Show only playback channels (no recording) |
| `-w` | Watch mode — loop and show only changes |
| `-i SEC` | Interval in seconds for watch mode (default: 1) |
| `-h` | Show help |

### Examples

```bash
# Show xruns for default device
xruns

# Show xruns for pcm1
xruns -d 1

# Show only playback xruns for pcm0
xruns -d 0 -p

# Watch mode — monitor changes in real-time
xruns -w

# Watch playback xruns on pcm0, check every 2 seconds
xruns -d 0 -p -w -i 2
```

## Output

### Normal mode

```
pcm0:
  dsp0.play.0: 0 xruns
  dsp0.record.0: 0 xruns
```

### Watch mode

Only shows non-zero xruns when they change:

```
14:32:15.123 dsp0.play.0: 3 xruns (+3)
14:32:18.456 dsp0.play.0: 5 xruns (+2)
```

## Requirements

- FreeBSD 14.0 or later
- `sndctl(8)` — the script parses its output

## Related tools

- `sndctl(8)` — full sound device control utility
- `mixer(8)` — volume control
- `/dev/sndstat` — kernel sound status

## Further reading

- [FreeBSD audio setup: bitperfect, equalizer, realtime](https://m4c.pl/blog/freebsd-audio-setup-bitperfect-equalizer-realtime/) — practical guide to configuring audio on FreeBSD
- [Vox FreeBSD: How Sound Works](https://freebsdfoundation.org/our-work/journal/browser-based-edition/freebsd-15-0/vox-freebsd-how-sound-works/) — in-depth article about FreeBSD sound(4) internals by Christos Margiolis (author of sndctl)

## Tips for reducing xruns

1. **Increase buffer size** — trade latency for stability

2. **Lower latency settings**:
   ```bash
   sndctl realtime=1
   ```
   This sets `hw.snd.latency=0`, `hw.snd.latency_profile=0`, and `kern.timecounter.alloweddeviation=0` — reduces buffering and timing jitter. Note: this is not true realtime scheduling, just lower latency parameters.

3. **Check CPU usage** — high load causes xruns

4. **Use MMAP-capable applications** — JACK, Ardour for low latency

## License

BSD-2-Clause (same as FreeBSD)
