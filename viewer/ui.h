#ifndef UI_H
#define UI_H

#include "archive.h"
#include <raylib.h>

#define MAX_FILES 10000
#define FILE_LIST_WIDTH 300
#define PREVIEW_WIDTH 800
#define FOOTER_HEIGHT 60

typedef struct {
    CapitalArchive *archive;
    int selected_index;
    int scroll_offset;
    int filter_type; // 0 = all, 1-4 = by type
    char search_text[64];
    Texture2D current_texture;
    bool texture_loaded;
    Music current_music;
    bool music_loaded;
    bool is_playing;
} ViewerState;

void init_viewer(ViewerState *state, CapitalArchive *archive);
void update_viewer(ViewerState *state);
void draw_viewer(ViewerState *state);
void unload_current(ViewerState *state);
int filter_file(ViewerState *state, int index);

#endif
