#!/bin/bash

# FFmpeg Video to GIF/WAV Converter for CircuitPython
# Usage: ./convert.sh input.mp4 [--fps 15]

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <input_video.mp4> [--fps FPS]" >&2
    echo "  FPS: 1-30 (default: 15)" >&2
    exit 1
fi

input="$1"
if [ ! -f "$input" ]; then
    echo "Error: Input file '$input' not found" >&2
    exit 1
fi

if ! command -v ffmpeg &> /dev/null; then
    echo "Error: FFmpeg not installed" >&2
    exit 1
fi

fps=15
if [ $# -eq 2 ] && [ "$2" = "--fps" ]; then
    echo "Error: --fps requires a value (1-30)" >&2
    exit 1
elif [ $# -eq 3 ] && [ "$2" = "--fps" ]; then
    fps="$3"
    if ! [[ "$fps" =~ ^[0-9]+$ ]] || [ "$fps" -lt 1 ] || [ "$fps" -gt 30 ]; then
        echo "Error: FPS must be an integer between 1 and 30" >&2
        exit 1
    fi
    shift 2
fi

base="${input%.*}"
gif_output="${base}.gif"
wav_output="${base}.wav"
palette="${base}_palette.png"

echo "Converting '$input' to GIF and WAV (FPS: $fps)..."

# Generate GIF palette (first pass)
if ! ffmpeg -v warning -i "$input" \
    -vf "fps=$fps,scale=320:170:flags=lanczos:force_original_aspect_ratio=decrease,pad=320:170:(ow-iw)/2:(oh-ih)/2:black,palettegen=stats_mode=single" \
    -frames:v 1 -y "$palette"; then
    echo "Error: Palette generation failed" >&2
    exit 1
fi

# Create GIF (second pass)
if ! ffmpeg -v warning -i "$input" -i "$palette" \
    -filter_complex "fps=$fps,scale=320:170:flags=lanczos:force_original_aspect_ratio=decrease,pad=320:170:(ow-iw)/2:(oh-ih)/2:black[x];[x][1:v]paletteuse=dither=floyd_steinberg" \
    -y "$gif_output"; then
    echo "Error: GIF creation failed" >&2
    rm -f "$palette"
    exit 1
fi

# Create WAV
if ! ffmpeg -v warning -i "$input" -ac 1 -ar 8000 -sample_fmt s16 -y "$wav_output"; then
    echo "Error: WAV creation failed" >&2
    rm -f "$palette" "$gif_output"
    exit 1
fi

# Cleanup palette
rm -f "$palette"

echo "Done! Created:"
echo "  GIF: $gif_output (320x170, ${fps}fps, CircuitPython gifio compatible)"
echo "  WAV: $wav_output (mono, 8kHz, 16-bit, MAX98357A compatible)"
