#!/bin/bash

# FFmpeg Video to GIF/WAV Converter for CircuitPython

# Usage: ./convert.sh input.mp4 [--fps FPS] [--rotation DEGREES]

if [ $# -lt 1 ] || [ $# -gt 5 ]; then
  echo "Usage: $0 input.mp4 [--fps FPS] [--rotation DEGREES]" >&2
  echo "  FPS: 1-30 (default: 10)" >&2
  echo "  Rotation: 0,90,180,270 (default: 0)" >&2
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

fps=10
rotation=0
width=240
height=135

# Parse arguments
shift
while [ $# -gt 0 ]; do
  case "$1" in
    --fps)
      shift
      if [ $# -eq 0 ] || ! [[ "$1" =~ ^[0-9]+$ ]] || [ "$1" -lt 1 ] || [ "$1" -gt 30 ]; then
        echo "Error: --fps requires integer 1-30" >&2
        exit 1
      fi
      fps="$1"
      shift
      ;;
    --rotation)
      shift
      if [ $# -eq 0 ] || ! [[ "$1" =~ ^(0|90|180|270)$ ]]; then
        echo "Error: --rotation requires 0, 90, 180, or 270" >&2
        exit 1
      fi
      rotation="$1"
      shift
      ;;
    *)
      echo "Error: Unknown argument '$1'" >&2
      exit 1
      ;;
  esac
done

# Set resolution based on rotation (90° and 270° swap width/height)
if [ "$rotation" = "90" ] || [ "$rotation" = "270" ]; then
  width=135
  height=240
fi

# Map rotation to FFmpeg filter
case "$rotation" in
  0)
    rotate_filter=""
    ;;
  90)
    rotate_filter="transpose=1"
    ;;
  180)
    rotate_filter="hflip,vflip"
    ;;
  270)
    rotate_filter="transpose=2"
    ;;
esac

base="${input%.*}"
gif_output="${base}.gif"
wav_output="${base}.wav"
palette="${base}_palette.png"

echo "Converting '$input' to GIF and WAV..."
echo "  FPS: $fps, Rotation: ${rotation}°, Resolution: ${width}x${height}"

# Build filter chains
palette_filters="fps=$fps"
[ -n "$rotate_filter" ] && palette_filters+=",${rotate_filter}"
palette_filters+=",scale=${width}:${height}:flags=lanczos:force_original_aspect_ratio=decrease"
palette_filters+=",pad=${width}:${height}:(ow-iw)/2:(oh-ih)/2:black"

gif_filters="fps=$fps"
[ -n "$rotate_filter" ] && gif_filters+=",${rotate_filter}"
gif_filters+=",scale=${width}:${height}:flags=lanczos:force_original_aspect_ratio=decrease"
gif_filters+=",pad=${width}:${height}:(ow-iw)/2:(oh-ih)/2:black"

# Generate GIF palette (first pass, from ALL frames)
# Use stats_mode=full so the palette represents the whole animation, not just the first frame. [web:67]
if ! ffmpeg -v warning -i "$input" \
  -vf "${palette_filters},palettegen=max_colors=64:stats_mode=full:reserve_transparent=1" \
  -y "$palette"; then
  echo "Error: Palette generation failed" >&2
  exit 1
fi

# Create GIF (second pass)
if ! ffmpeg -v warning -i "$input" -i "$palette" \
  -filter_complex "${gif_filters}[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" \
  -loop 0 "$gif_output" -y; then
  echo "Error: GIF creation failed" >&2
  rm -f "$palette"
  exit 1
fi

# Create WAV - 16 kHz sample rate (unchanged)
if ! ffmpeg -v warning -i "$input" -ac 1 -ar 16000 -sample_fmt s16 -y "$wav_output"; then
  echo "Error: WAV creation failed" >&2
  rm -f "$palette" "$gif_output"
  exit 1
fi

# Cleanup palette
rm -f "$palette"

echo "Done! Created:"
echo "  GIF: $gif_output (${width}x${height}, ${fps}fps, ${rotation}° rotation, CircuitPython/AnimatedGIF friendly)"
echo "  WAV: $wav_output (mono, 16kHz, 16-bit, MAX98357A compatible)"
