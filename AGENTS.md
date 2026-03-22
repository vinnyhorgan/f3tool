# Fallout 3 Capital Archive Tool

## Project Overview

**Goal:** Extract assets from Fallout 3 (GOG version) and package them into a custom `.capital` binary archive format.

**Extracted Assets:**
- ESM files (unchanged)
- Audio data (converted to OGG Vorbis via ffmpeg)
- UI art (converted to PNG via ffmpeg)

## Project Structure

```
/home/dvh/Downloads/capital/f3tool/
├── CMakeLists.txt           # Build configuration
├── include/
│   ├── bsa.h              # BSA archive structures and API
│   ├── esm.h               # ESM file structures
│   ├── archive.h          # Capital archive format definition
│   ├── audio.h            # Audio format detection
│   ├── image.h            # DDS/PNG image handling
│   ├── hash.h             # Bethesda filename hash
│   └── util.h             # Utility functions
├── src/
│   ├── bsa.c              # BSA parsing and extraction
│   ├── esm.c              # ESM parsing
│   ├── archive.c           # .capital archive creation
│   ├── audio.c            # Audio loading (WAV/MP3/OGG)
│   ├── image.c            # DDS parsing/PNG encoding
│   ├── hash.c             # Hash implementation
│   ├── util.c             # Utility implementations
│   └── main.c             # CLI entry point
└── build/                  # Build output

/home/dvh/Downloads/capital/f3/   # Fallout 3 game files
└── Data/
    ├── Fallout3.esm        # Main game file (288MB)
    ├── Fallout - Sound.bsa # Sound effects
    ├── Fallout - Voices.bsa # Voice files
    ├── Fallout - MenuVoices.bsa
    ├── Fallout - Misc.bsa  # Menus, XML, etc.
    ├── Fallout - Textures.bsa
    ├── Fallout - Meshes.bsa
    └── [DLC ESMs]
```

## Building

```bash
cd /home/dvh/Downloads/capital/f3tool/build
cmake .. && make
```

## Usage

```bash
# Quick test (5 files each)
./f3tool /path/to/fallout3 /tmp/test -a -n 5 -v

# Full extraction (ESM + audio + UI art)
./f3tool /path/to/fallout3 /tmp/output -e -a -i

# List files only
./f3tool /path/to/fallout3 -l

# Options:
#   -e, --esm      Extract ESM files
#   -a, --audio    Extract audio files
#   -i, --art      Extract UI art
#   -n, --limit N  Limit extraction to N files per type
#   -v, --verbose  Verbose output
```

## Capital Archive Format

**Header (24 bytes):**
- Magic: `0x43415021` ("!PAC")
- Version: 1
- Entry count
- Flags
- Reserved[16]

**Entry (variable):**
- Type (1=ESM, 2=Audio, 3=Art)
- Path length
- Data size
- Offset
- Metadata (varies by type)

**Entry data** follows the index.

## Current Status

### Working
- ✅ BSA file parsing (version 0x68 format)
- ✅ File listing from BSA archives
- ✅ ESM extraction (6 files, ~329MB)
- ✅ Audio extraction with OGG conversion via ffmpeg
- ✅ Music extraction from Data/Music folder (58 MP3s → OGG)
- ✅ UI art extraction (DDS→PNG via ffmpeg)
- ✅ Video extraction (BIK → MPEG1/MP2 for pl_mpeg)
- ✅ Capital archive creation

### Known Issues
- ⚠️ OGG conversion via libvorbis (in audio.c) crashes on certain files - using ffmpeg instead
- ⚠️ DDS→PNG conversion requires ffmpeg, not using native DDS decoder in image.c
- ⚠️ Art files limited to `textures\interface\*` and `.dds`/`.tga` files

### DLC Audio
DLC audio files are intentionally excluded from extraction.

### Compression Settings
- **OGG Vorbis:** `-q:a 4 -ac 2` (~128kbps VBR stereo, good game audio quality)
- **MPEG1 Video:** `-c:v mpeg1video -c:a mp2 -b:v 2M -b:a 192k` (for pl_mpeg streaming)
- **PNG:** `-compression_level 9 -pred median` (maximum lossless compression)

## Key Implementation Details

### BSA Parsing (bsa.c)
- Folder offsets are absolute, need to subtract baseline offset
- Baseline = sizeof(header) + folderCount * sizeof(FolderRecord) + totalFileNameLength
- File names stored after all file records in a separate block

### Audio Detection (audio.c)
- WAV: RIFF header detection
- MP3: 0xFF/0xF3/0xFB frame sync markers
- OGG: "OggS" magic

### UI Art Detection (main.c)
- Pattern matching: `interface\`, `menus\`, `textures\interface\`
- Extension check: `.dds`, `.tga`

### FFmpeg Commands Used
```bash
# Audio to OGG Vorbis (stereo, ~128kbps VBR)
ffmpeg -y -i input.wav -c:a libvorbis -q:a 4 -ac 2 output.ogg

# Music MP3 to OGG Vorbis (stereo, ~128kbps VBR)
ffmpeg -y -i input.mp3 -c:a libvorbis -q:a 4 -ac 2 output.ogg

# Video BIK to MPEG1/MP2 (for pl_mpeg streaming)
ffmpeg -y -i input.bik -c:v mpeg1video -c:a mp2 -b:v 2M -b:a 192k output.mpg

# DDS to PNG (max compression + median prediction)
ffmpeg -y -i input.dds -compression_level 9 -pred median output.png
```

## Next Steps
1. Debug native OGG encoder or improve ffmpeg integration
2. Test full extraction and verify archive integrity
3. Implement archive reader/extractor
4. Add progress reporting for long extractions
