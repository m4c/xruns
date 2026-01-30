# xruns

Monitor audio buffer xruns on FreeBSD sound devices.

## Overview

`xruns` is a lightweight tool for monitoring audio buffer underruns/overruns on FreeBSD. It reads directly from `/dev/sndstat` using the `SNDSTIOC_GET_DEVS` ioctl interface, without relying on `sndctl`.

### What are xruns?

- **Underrun (playback)**: Application doesn't supply data fast enough — buffer empties, causing clicks or silence.
- **Overrun (recording)**: Application doesn't read data fast enough — buffer overflows, data is lost.

Common causes: small buffers, CPU load, wrong scheduler priorities, driver issues.

## Building

Using FreeBSD make:

```bash
make
```

Or manually:

```bash
cc -o xruns xruns.c -lmixer -lnv
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

- FreeBSD 14.0 or later (requires sound(4) nvlist interface)
- Libraries: libmixer, libnv

## Related tools

- `sndctl(8)` — full sound device control utility
- `mixer(8)` — volume control
- `/dev/sndstat` — kernel sound status

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
