#include "ui.h"
#include <string.h>
#include <math.h>

static Rectangle file_list_bounds;
static Rectangle preview_bounds;
static Rectangle footer_bounds;

void init_viewer(ViewerState *state, CapitalArchive *archive)
{
    state->archive = archive;
    state->selected_index = -1;
    state->scroll_offset = 0;
    state->filter_type = 0;
    state->search_text[0] = '\0';
    state->texture_loaded = false;
    state->music_loaded = false;
    state->is_playing = false;
    state->current_texture.id = 0;
    state->current_music = (Music){0};
}

void unload_current(ViewerState *state)
{
    if (state->texture_loaded && state->current_texture.id != 0) {
        UnloadTexture(state->current_texture);
        state->current_texture.id = 0;
        state->texture_loaded = false;
    }
    if (state->music_loaded) {
        StopMusicStream(state->current_music);
        UnloadMusicStream(state->current_music);
        state->current_music = (Music){0};
        state->music_loaded = false;
        state->is_playing = false;
    }
}

static char *get_display_name(const char *path)
{
    static char buffer[512];
    int j = 0;
    for (int i = 0; path[i] && j < (int)sizeof(buffer) - 1; i++) {
        if (path[i] == '\\') {
            buffer[j++] = '/';
        } else if (path[i] >= 32 && path[i] < 127) {
            buffer[j++] = path[i];
        } else {
            buffer[j++] = '?';
        }
    }
    buffer[j] = '\0';
    return buffer;
}

int filter_file(ViewerState *state, int index)
{
    if (!state->archive || index < 0 || (uint32_t)index >= state->archive->header.entry_count) {
        return 0;
    }
    
    CapitalFileInfo *file = &state->archive->files[index];
    
    if (file->entry.data_size == 0) {
        return 0;
    }
    
    if (state->filter_type > 0 && file->entry.type != state->filter_type) {
        return 0;
    }
    
    if (state->search_text[0] != '\0') {
        if (strcasestr(file->path, state->search_text) == NULL) {
            return 0;
        }
    }
    
    return 1;
}

int get_filtered_count(ViewerState *state)
{
    if (!state->archive) return 0;
    int count = 0;
    for (uint32_t i = 0; i < state->archive->header.entry_count; i++) {
        if (filter_file(state, i)) count++;
    }
    return count;
}

int get_filtered_index(ViewerState *state, int filtered_num)
{
    if (!state->archive || filtered_num < 0) return -1;
    int count = 0;
    for (uint32_t i = 0; i < state->archive->header.entry_count; i++) {
        if (filter_file(state, i)) {
            if (count == filtered_num) return i;
            count++;
        }
    }
    return -1;
}

void draw_file_list(ViewerState *state)
{
    DrawRectangleRec(file_list_bounds, DARKGRAY);
    DrawRectangleLinesEx(file_list_bounds, 1, GRAY);
    
    float y = file_list_bounds.y + 10;
    float x = file_list_bounds.x + 10;
    
    int filtered_count = get_filtered_count(state);
    if (filtered_count <= 0) return;
    
    int visible_height = (int)(file_list_bounds.height - 20) / 20;
    if (visible_height <= 0) visible_height = 1;
    
    if (state->scroll_offset > filtered_count - visible_height) {
        state->scroll_offset = filtered_count - visible_height;
    }
    if (state->scroll_offset < 0) state->scroll_offset = 0;
    
    int drawn = 0;
    int item_idx = 0;
    for (uint32_t i = 0; i < state->archive->header.entry_count && drawn < visible_height; i++) {
        if (!filter_file(state, i)) continue;
        
        if (item_idx < state->scroll_offset) {
            item_idx++;
            continue;
        }
        
        int is_selected = (item_idx == state->selected_index);
        Color bg_color = is_selected ? SKYBLUE : 
                         ((drawn % 2) == 0 ? DARKGRAY : (Color){50, 50, 50, 255});
        
        Rectangle item_rect = { file_list_bounds.x, y, file_list_bounds.width, 20 };
        DrawRectangleRec(item_rect, bg_color);
        
        CapitalFileInfo *file = &state->archive->files[i];
        const char *type_name = capital_viewer_get_type_name(file->entry.type);
        
        Color type_color = file->entry.type == 1 ? GOLD :
                          file->entry.type == 2 ? GREEN :
                          file->entry.type == 3 ? PINK : ORANGE;
        
        DrawText(type_name, (int)x, (int)y + 3, 10, type_color);
        
        char *display_name = get_display_name(file->path);
        char *filename = strrchr(display_name, '/');
        if (filename) filename++;
        else filename = display_name;
        
        DrawText(filename, (int)x + 50, (int)y + 3, 10, WHITE);
        
        y += 20;
        drawn++;
        item_idx++;
    }
    
    float scrollbar_height = file_list_bounds.height - 20;
    float thumb_height = scrollbar_height * ((float)visible_height / filtered_count);
    float thumb_y = file_list_bounds.y + 10 + scrollbar_height * ((float)state->scroll_offset / filtered_count);
    
    if (thumb_height < 20) thumb_height = 20;
    
    DrawRectangle((int)file_list_bounds.x + (int)file_list_bounds.width - 15, 
                  (int)file_list_bounds.y + 10, 10, (int)scrollbar_height, GRAY);
    DrawRectangle((int)file_list_bounds.x + (int)file_list_bounds.width - 15, 
                  (int)thumb_y, 10, (int)thumb_height, LIGHTGRAY);
}

void draw_preview(ViewerState *state)
{
    DrawRectangleRec(preview_bounds, (Color){30, 30, 30, 255});
    DrawRectangleLinesEx(preview_bounds, 1, GRAY);
    
    if (state->selected_index < 0) {
        const char *msg = "Select a file to preview";
        Vector2 text_size = MeasureTextEx(GetFontDefault(), msg, 20, 1);
        DrawText(msg, 
                 (int)(preview_bounds.x + (preview_bounds.width - text_size.x) / 2),
                 (int)(preview_bounds.y + (preview_bounds.height - text_size.y) / 2),
                 20, GRAY);
    }
}

void draw_footer(ViewerState *state)
{
    DrawRectangleRec(footer_bounds, (Color){40, 40, 40, 255});
    DrawRectangleLinesEx(footer_bounds, 1, GRAY);
    
    if (state->selected_index < 0) return;
    
    int actual_idx = get_filtered_index(state, state->selected_index);
    if (actual_idx < 0) return;
    
    CapitalFileInfo *file = &state->archive->files[actual_idx];
    char *display_path = get_display_name(file->path);
    
    char info[512];
    snprintf(info, sizeof(info), "[%s] %s (%u bytes)",
             capital_viewer_get_type_name(file->entry.type),
             display_path,
             file->entry.data_size);
    
    DrawText(info, (int)footer_bounds.x + 10, (int)footer_bounds.y + 20, 10, WHITE);
}

void update_viewer(ViewerState *state)
{
    if (!state->archive) return;
    
    file_list_bounds = (Rectangle){ 0, 0, FILE_LIST_WIDTH, GetScreenHeight() - FOOTER_HEIGHT };
    preview_bounds = (Rectangle){ FILE_LIST_WIDTH, 0, GetScreenWidth() - FILE_LIST_WIDTH, GetScreenHeight() - FOOTER_HEIGHT };
    footer_bounds = (Rectangle){ 0, GetScreenHeight() - FOOTER_HEIGHT, GetScreenWidth(), FOOTER_HEIGHT };
    
    int filtered_count = get_filtered_count(state);
    int visible_height = (int)(file_list_bounds.height - 20) / 20;
    if (visible_height <= 0) visible_height = 1;
    
    if (IsKeyPressed(KEY_DOWN)) {
        if (state->selected_index < filtered_count - 1) {
            state->selected_index++;
        }
    }
    if (IsKeyPressed(KEY_UP)) {
        if (state->selected_index > 0) {
            state->selected_index--;
        }
    }
    
    if (state->selected_index >= state->scroll_offset + visible_height) {
        state->scroll_offset = state->selected_index - visible_height + 1;
    }
    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    
    if (IsKeyPressed(KEY_HOME)) {
        state->selected_index = 0;
        state->scroll_offset = 0;
    }
    if (IsKeyPressed(KEY_END)) {
        state->selected_index = filtered_count - 1;
        state->scroll_offset = filtered_count - visible_height;
    }
    if (IsKeyPressed(KEY_PAGE_DOWN)) {
        state->selected_index += visible_height;
        if (state->selected_index >= filtered_count) {
            state->selected_index = filtered_count - 1;
        }
    }
    if (IsKeyPressed(KEY_PAGE_UP)) {
        state->selected_index -= visible_height;
        if (state->selected_index < 0) state->selected_index = 0;
    }
    
    if (IsKeyPressed(KEY_SPACE)) {
        if (state->music_loaded) {
            if (state->is_playing) {
                StopMusicStream(state->current_music);
                state->is_playing = false;
            } else {
                PlayMusicStream(state->current_music);
                state->is_playing = true;
            }
        }
    }
    
    int wheel = (int)GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, file_list_bounds)) {
            state->scroll_offset -= wheel * 3;
            if (state->scroll_offset < 0) state->scroll_offset = 0;
            int max_scroll = filtered_count - visible_height;
            if (max_scroll < 0) max_scroll = 0;
            if (state->scroll_offset > max_scroll) {
                state->scroll_offset = max_scroll;
            }
        }
    }
    
    if (state->music_loaded && state->is_playing) {
        UpdateMusicStream(state->current_music);
        if (!IsMusicStreamPlaying(state->current_music)) {
            state->is_playing = false;
        }
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, file_list_bounds)) {
            int click_idx = (int)((mouse.y - file_list_bounds.y - 10) / 20) + state->scroll_offset;
            if (click_idx >= 0 && click_idx < filtered_count) {
                unload_current(state);
                state->selected_index = click_idx;
                
                int actual_idx = get_filtered_index(state, click_idx);
                if (actual_idx >= 0) {
                    CapitalFileInfo *file = &state->archive->files[actual_idx];
                    uint8_t *data = NULL;
                    size_t size = 0;
                    
                    if (capital_viewer_get_file(state->archive, actual_idx, &data, &size) == 0 && data && size > 0) {
                        if (file->entry.type == 3) {
                            Image img = LoadImageFromMemory(".png", data, (int)size);
                            if (img.data && img.width > 0 && img.height > 0) {
                                state->current_texture = LoadTextureFromImage(img);
                                UnloadImage(img);
                                if (state->current_texture.id != 0) {
                                    state->texture_loaded = true;
                                }
                            }
                        } else if (file->entry.type == 2) {
                            state->current_music = LoadMusicStreamFromMemory(".ogg", data, (int)size);
                            if (state->current_music.ctxData) {
                                state->music_loaded = true;
                                state->is_playing = true;
                                PlayMusicStream(state->current_music);
                            }
                        }
                    }
                }
            }
        }
    }
}

void draw_viewer(ViewerState *state)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){20, 20, 20, 255});
    
    draw_file_list(state);
    
    DrawRectangleRec(preview_bounds, (Color){30, 30, 30, 255});
    
    if (state->texture_loaded && state->current_texture.id != 0) {
        float tex_w = (float)state->current_texture.width;
        float tex_h = (float)state->current_texture.height;
        if (tex_w <= 0) tex_w = 1;
        if (tex_h <= 0) tex_h = 1;
        float scale = fminf(preview_bounds.width / tex_w,
                           preview_bounds.height / tex_h) * 0.9f;
        float w = tex_w * scale;
        float h = tex_h * scale;
        float px = preview_bounds.x + (preview_bounds.width - w) / 2;
        float py = preview_bounds.y + (preview_bounds.height - h) / 2;
        DrawTextureEx(state->current_texture, (Vector2){px, py}, 0, scale, WHITE);
        
        char info[128];
        snprintf(info, sizeof(info), "%d x %d", state->current_texture.width, state->current_texture.height);
        DrawText(info, (int)preview_bounds.x + 10, (int)preview_bounds.y + 10, 10, WHITE);
    } else if (state->music_loaded) {
        const char *msg = "Audio loaded";
        Vector2 text_size = MeasureTextEx(GetFontDefault(), msg, 20, 1);
        DrawText(msg,
                 (int)(preview_bounds.x + (preview_bounds.width - text_size.x) / 2),
                 (int)(preview_bounds.y + (preview_bounds.height - text_size.y) / 2),
                 20, GREEN);
        
        float progress = 0.0f;
        if (GetMusicTimeLength(state->current_music) > 0) {
            progress = GetMusicTimePlayed(state->current_music) / GetMusicTimeLength(state->current_music);
        }
        Rectangle bar = { preview_bounds.x + 50, preview_bounds.y + preview_bounds.height / 2 + 30, 
                         preview_bounds.width - 100, 10 };
        DrawRectangleRec(bar, GRAY);
        DrawRectangle((int)preview_bounds.x + 50, (int)preview_bounds.y + (int)preview_bounds.height / 2 + 30,
                     (int)(bar.width * progress), 10, SKYBLUE);
    } else if (state->selected_index >= 0) {
        int actual_idx = get_filtered_index(state, state->selected_index);
        if (actual_idx >= 0) {
            CapitalFileInfo *file = &state->archive->files[actual_idx];
            char *display_path = get_display_name(file->path);
            const char *type_name = capital_viewer_get_type_name(file->entry.type);
            char info[512];
            snprintf(info, sizeof(info), "Type: %s\nPath: %s\nSize: %u bytes",
                     type_name, display_path, file->entry.data_size);
            DrawText(info, (int)preview_bounds.x + 10, (int)preview_bounds.y + 10, 10, WHITE);
        }
    }
    
    Rectangle filter_bar = { 0, 0, FILE_LIST_WIDTH, 40 };
    DrawRectangleRec(filter_bar, (Color){50, 50, 50, 255});
    
    const char *filters[] = { "All", "ESM", "Audio", "Art", "Video" };
    float filter_x = 5;
    for (int i = 0; i < 5; i++) {
        Rectangle btn = { filter_x, 5, 55, 30 };
        Color bg = state->filter_type == i ? SKYBLUE : DARKGRAY;
        DrawRectangleRec(btn, bg);
        DrawText(filters[i], (int)btn.x + 5, (int)btn.y + 8, 10, WHITE);
        
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state->filter_type = i;
            state->selected_index = 0;
            state->scroll_offset = 0;
        }
        filter_x += 60;
    }
    
    draw_footer(state);
}
