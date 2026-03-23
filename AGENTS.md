# Fallout 3 Capital Archive Tool

## Project Overview

**Goal:** Extract assets from Fallout 3 (GOG version) and package them into a custom `.capital` binary archive format.

**Extracted Assets:**
- ESM files (unchanged)
- Audio data (converted to OGG Vorbis via ffmpeg)
- Textures (converted to PNG via ffmpeg, fallback to raw DDS)
- Video (converted to MPEG1/MP2 via ffmpeg)

## Project Structure

```
f3tool/
├── CMakeLists.txt           # Build configuration
├── AGENTS.md                # This file
├── .gitignore               # Git ignore patterns
├── include/
│   ├── bsa.h              # BSA archive structures and API
│   ├── esm.h              # ESM file structures
│   ├── archive.h          # Capital archive format definition
│   ├── audio.h            # Audio format detection
│   ├── image.h            # DDS/PNG image handling
│   ├── hash.h             # Bethesda filename hash
│   └── util.h             # Utility functions
├── src/
│   ├── bsa.c             # BSA parsing and extraction
│   ├── esm.c             # ESM parsing
│   ├── archive.c          # .capital archive creation
│   ├── audio.c           # Audio loading (WAV/MP3/OGG)
│   ├── image.c           # DDS parsing/PNG encoding
│   ├── hash.c            # Hash implementation
│   ├── util.c            # Utility implementations
│   └── main.c            # CLI entry point
└── build/                 # Build output (gitignored)
```

## Building

```bash
cd f3tool/build
cmake .. && make
```

## Usage

```bash
# Full extraction (ESM + audio + art + video)
./f3tool /path/to/fallout3 /tmp/output -A

# Quick test (limited files per type)
./f3tool /path/to/fallout3 /tmp/test -A -n 5 -v

# Extract specific types
./f3tool /path/to/fallout3 /tmp/output -e -a -i -V

# List files only
./f3tool /path/to/fallout3 -l

# Options:
#   -e, --esm      Extract ESM files
#   -a, --audio    Extract audio files (sounds from BSA + music)
#   -i, --art      Extract textures (DDS from textures\*)
#   -V, --video    Extract video files
#   -A, --all      Extract everything
#   -l, --list     List files in archives only
#   -n, --limit N  Limit extraction to N files per type
#   -v, --verbose  Verbose output
```

## Capital Archive Format

**Header (32 bytes):**
- Magic: `0x43415021` ("!PAC")
- Version: 1
- Entry count
- Flags
- Reserved[16]

**Entry (variable, 23+ bytes):**
- Type (1=ESM, 2=Audio, 3=Art, 4=Video)
- Path length (2 bytes)
- Data size (4 bytes)
- Offset (4 bytes)
- Metadata (varies by type: ESM=4, Audio=8, Art=12, Video=8 bytes)
- Path (variable length)

**Entry data** follows the index.

## Extraction Results

Full extraction from Fallout 3 (GOG version):
- **ESM:** 6 files (~329MB)
- **Audio:** 1626 files (music + sound effects)
- **Art:** 7863 files (all textures from `textures\*`)
- **Video:** 35 files
- **Total:** 9530 entries (~2.4GB archive)

## Current Status

### Working
- ✅ BSA file parsing (version 0x68 format)
- ✅ File listing from BSA archives
- ✅ ESM extraction (6 files)
- ✅ Audio extraction with OGG conversion via ffmpeg
- ✅ Music extraction from Data/Music folder (58 MP3s → OGG)
- ✅ Full texture extraction (all `textures\*` DDS files)
- ✅ DDS→PNG conversion via ffmpeg with fallback to raw DDS
- ✅ Video extraction (BIK → MPEG1/MP2)
- ✅ Capital archive creation

### Known Issues
- ⚠️ OGG conversion via libvorbis (in audio.c) crashes on certain files - using ffmpeg instead
- ⚠️ `capital_extract_all()` not implemented - cannot yet unpack .capital archives

### DLC Audio
DLC audio files are intentionally excluded from extraction.

## Compression Settings
- **OGG Vorbis:** `-q:a 4 -ac 2` (~128kbps VBR stereo)
- **MPEG1 Video:** `-c:v mpeg1video -c:a mp2 -b:v 2M -b:a 192k`
- **PNG:** `-compression_level 9 -pred median`

## Key Implementation Details

### BSA Parsing (bsa.c)
- Folder offsets are absolute, need to subtract baseline offset
- Baseline = sizeof(header) + folderCount * sizeof(FolderRecord) + totalFileNameLength
- File names stored after all file records in a separate block

### Audio Detection (audio.c)
- WAV: RIFF header detection
- MP3: 0xFF/0xF3/0xFB frame sync markers
- OGG: "OggS" magic

### Art Detection (main.c)
- Extension check: `.dds`, `.tga`
- Path check: `textures\*` (all game textures)
- DDS→PNG conversion via ffmpeg, fallback to raw DDS if conversion fails

### FFmpeg Commands Used
```bash
# Audio to OGG Vorbis (stereo, ~128kbps VBR)
ffmpeg -y -i input.wav -c:a libvorbis -q:a 4 -ac 2 output.ogg

# Music MP3 to OGG Vorbis
ffmpeg -y -i input.mp3 -c:a libvorbis -q:a 4 -ac 2 output.ogg

# Video BIK to MPEG1/MP2 (for pl_mpeg streaming)
ffmpeg -y -i input.bik -c:v mpeg1video -c:a mp2 -b:v 2M -b:a 192k -qscale:v 2 output.mpg

# DDS to PNG (max compression + median prediction)
ffmpeg -y -i input.dds -compression_level 9 -pred median output.png
```

## Next Steps
1. Implement `capital_extract_all()` to unpack .capital archives
2. Add progress reporting for long extractions
