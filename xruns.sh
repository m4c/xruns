#!/bin/sh
#
# xruns.sh - Monitor audio buffer xruns on FreeBSD
#
# Usage:
#   xruns.sh [-d device] [-p] [-w] [-i interval]
#
# Options:
#   -d N        Monitor device pcmN (default: default device)
#   -p          Show only playback channels (no recording)
#   -w          Watch mode - loop and show only changes
#   -i SEC      Interval in seconds for watch mode (default: 1)
#   -h          Show help
#

usage() {
    cat << EOF
usage: ${0##*/} [-d device] [-p] [-w] [-i interval]

Options:
  -d N      Monitor device pcmN (default: system default)
  -p        Show only playback channels
  -w        Watch mode - loop and show only changes
  -i SEC    Interval in seconds for watch mode (default: 1)
  -h        Show this help

Examples:
  ${0##*/}              Show xruns for default device
  ${0##*/} -d 1         Show xruns for pcm1
  ${0##*/} -d 0 -p      Show only playback xruns for pcm0
  ${0##*/} -w           Watch mode - monitor changes
  ${0##*/} -d 0 -p -w   Watch playback xruns on pcm0
EOF
    exit 1
}

get_default_unit() {
    sysctl -n hw.snd.default_unit 2>/dev/null || echo "0"
}

get_xruns() {
    _unit="$1"
    _play_only="$2"
    
    sndctl -f /dev/dsp"${_unit}" -v -o 2>/dev/null | while read -r line; do
        case "$line" in
            *xruns=*)
                _chan="${line%%.*}"
                _xruns="${line##*=}"
                
                if [ "$_play_only" = "1" ]; then
                    case "$line" in
                        *play*) echo "${_chan} ${_xruns}" ;;
                    esac
                else
                    echo "${_chan} ${_xruns}"
                fi
                ;;
        esac
    done
}

show_xruns() {
    _unit="$1"
    _play_only="$2"
    
    echo "pcm${_unit}:"
    get_xruns "$_unit" "$_play_only" | while read -r chan xruns; do
        echo "  ${chan}: ${xruns} xruns"
    done
}

watch_xruns() {
    _unit="$1"
    _play_only="$2"
    _interval="$3"
    
    echo "Watching xruns on pcm${_unit} (Ctrl+C to stop)..."
    
    _prev_values=""
    _first_run=1
    
    while true; do
        _current=$(get_xruns "$_unit" "$_play_only")
        _timestamp=$(date +%H:%M:%S.%N | cut -c1-12)
        
        echo "$_current" | while read -r chan xruns; do
            [ -z "$chan" ] && continue
            [ "$xruns" = "0" ] && continue
            
            _prev=$(echo "$_prev_values" | grep "^${chan} " | cut -d' ' -f2)
            
            if [ "$_first_run" = "1" ]; then
                echo "${_timestamp} ${chan}: ${xruns} xruns"
            elif [ "$_prev" != "$xruns" ]; then
                if [ -n "$_prev" ] && [ "$_prev" != "0" ]; then
                    _diff=$((xruns - _prev))
                    echo "${_timestamp} ${chan}: ${xruns} xruns (+${_diff})"
                else
                    echo "${_timestamp} ${chan}: ${xruns} xruns"
                fi
            fi
        done
        
        _prev_values="$_current"
        _first_run=0
        
        sleep "$_interval"
    done
}

# Defaults
device=""
play_only=0
watch_mode=0
interval=1

# Parse options
while getopts "d:hi:pw" opt; do
    case "$opt" in
        d) device="$OPTARG" ;;
        i) interval="$OPTARG" ;;
        p) play_only=1 ;;
        w) watch_mode=1 ;;
        h|*) usage ;;
    esac
done

# Use default device if not specified
if [ -z "$device" ]; then
    device=$(get_default_unit)
fi

# Check if device exists
if ! sndctl -f /dev/dsp"${device}" >/dev/null 2>&1; then
    echo "Error: device pcm${device} not found" >&2
    exit 1
fi

# Run
if [ "$watch_mode" = "1" ]; then
    watch_xruns "$device" "$play_only" "$interval"
else
    show_xruns "$device" "$play_only"
fi
