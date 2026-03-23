#include "ui.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *archive_path = NULL;
    
    if (argc > 1) {
        archive_path = argv[1];
    } else {
        printf("Usage: %s <capital_archive>\n", argv[0]);
        printf("Example: %s ../full.capital\n", argv[0]);
        return 1;
    }
    
    CapitalArchive archive;
    if (capital_viewer_open(&archive, archive_path) != 0) {
        fprintf(stderr, "Failed to open archive: %s\n", archive_path);
        return 1;
    }
    
    printf("Opened archive: %s\n", archive_path);
    printf("Entries: %u\n", archive.header.entry_count);
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 800, "Capital Archive Viewer");
    SetTargetFPS(60);
    
    ViewerState state;
    init_viewer(&state, &archive);
    
    while (!WindowShouldClose()) {
        update_viewer(&state);
        
        BeginDrawing();
        ClearBackground((Color){20, 20, 20, 255});
        draw_viewer(&state);
        EndDrawing();
    }
    
    unload_current(&state);
    capital_viewer_close(&archive);
    CloseWindow();
    
    return 0;
}
