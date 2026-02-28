#include "BridgeEngine.h"
#include "bapi_internal.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define CENTER_X (WINDOW_WIDTH / 2)
#define CENTER_Y (WINDOW_HEIGHT / 2)

#define COLOR_WHITE      bapi_color(255, 255, 255, 255)
#define COLOR_BLACK      bapi_color(0, 0, 0, 255)
#define COLOR_RED        bapi_color(255, 0, 0, 255)
#define COLOR_GREEN      bapi_color(0, 255, 0, 255)
#define COLOR_BLUE       bapi_color(0, 0, 255, 255)
#define COLOR_YELLOW     bapi_color(255, 255, 0, 255)
#define COLOR_CYAN       bapi_color(0, 255, 255, 255)
#define COLOR_MAGENTA    bapi_color(255, 0, 255, 255)
#define COLOR_ORANGE     bapi_color(255, 165, 0, 255)
#define COLOR_GRAY       bapi_color(128, 128, 128, 255)
#define COLOR_DARK_BG    bapi_color(30, 30, 40, 255)
#define COLOR_PANEL_BG   bapi_color(40, 40, 60, 255)
#define COLOR_PANEL_BORDER bapi_color(80, 80, 120, 255)

typedef struct {
    float x, y;
    float vx, vy;
    float radius;
    bapi_color_t color;
} Ball;

static bapi_scene_manager_t g_scene_manager = NULL;
static bapi_level_manager_t g_level_manager = NULL;
static bapi_button_t* g_demo_button = NULL;
static bapi_sound_t g_test_sound = NULL;
static bapi_video_t g_test_video = NULL;

static Ball g_balls[5] = {
    {100, 100, 3, 2, 30, {255, 80, 80, 255}},
    {200, 200, -2, 3, 25, {80, 255, 80, 255}},
    {300, 150, 2.5, -2, 35, {80, 80, 255, 255}},
    {400, 300, -3, -1.5, 28, {255, 255, 80, 255}},
    {500, 200, 2.5, 1.5, 32, {255, 80, 255, 255}},
};

static void draw_ball(Ball* ball) {
    bapi_fill_circle(ball->x, ball->y, ball->radius, ball->color);
}

static void update_ball(Ball* ball, int w, int h) {
    ball->x += ball->vx;
    ball->y += ball->vy;
    
    if (ball->x - ball->radius < 0) { ball->x = ball->radius; ball->vx = -ball->vx; }
    if (ball->x + ball->radius > w)  { ball->x = w - ball->radius; ball->vx = -ball->vx; }
    if (ball->y - ball->radius < 0) { ball->y = ball->radius; ball->vy = -ball->vy; }
    if (ball->y + ball->radius > h)  { ball->y = h - ball->radius; ball->vy = -ball->vy; }
}

static void draw_panel(int x, int y, int w, int h, const char* title) {
    bapi_fill_rect(x, y, w, h, COLOR_PANEL_BG);
    bapi_draw_rect(x, y, w, h, COLOR_PANEL_BORDER);
    if (title) {
        int title_x = x + (w - strlen(title) * 10) / 2;
        bapi_draw_text(title, title_x, y + 10, 20, COLOR_WHITE);
    }
}

static void draw_help_panel(void) {
    draw_panel(WINDOW_WIDTH - 260, 100, 250, 230, "Controls");
    bapi_draw_text("1 - Menu Scene",    WINDOW_WIDTH - 245, 140, 18, COLOR_CYAN);
    bapi_draw_text("2 - Game Scene",    WINDOW_WIDTH - 245, 165, 18, COLOR_GREEN);
    bapi_draw_text("3 - Settings Scene", WINDOW_WIDTH - 245, 190, 18, COLOR_ORANGE);
    bapi_draw_text("4 - Video Scene",   WINDOW_WIDTH - 245, 215, 18, COLOR_MAGENTA);
    bapi_draw_text("N - Next Level",    WINDOW_WIDTH - 245, 250, 18, COLOR_YELLOW);
    bapi_draw_text("P - Prev Level",    WINDOW_WIDTH - 245, 275, 18, COLOR_YELLOW);
    bapi_draw_text("ESC - Exit",        WINDOW_WIDTH - 245, 305, 18, COLOR_RED);
}

static void draw_color_palette(void) {
    const int size = 50;
    const int gap = 5;
    const int start_x = 50;
    const int y = WINDOW_HEIGHT - 80;
    
    uint32_t colors[] = {0xFF0000FF, 0x00FF00FF, 0x0000FFFF, 0xFFFF00FF, 0xFF00FFFF, 0x00FFFFFF, 0xFFFFFFFF, 0x808080FF};
    
    for (int i = 0; i < 8; i++) {
        int x = start_x + i * (size + gap);
        bapi_fill_rect(x, y, size, size, bapi_color_from_hex(colors[i]));
        bapi_draw_rect(x, y, size, size, COLOR_WHITE);
    }
}

static void draw_shapes_demo(void) {
    draw_panel(50, 450, 400, 150, "Shapes Demo");
    
    bapi_draw_circle(120, 520, 35, COLOR_RED);
    bapi_fill_circle(200, 520, 30, COLOR_GREEN);
    
    bapi_draw_polygon(300, 520, 30, 3, COLOR_YELLOW);
    bapi_fill_polygon(380, 520, 30, 4, COLOR_CYAN);
}

static void on_scene_menu_enter(bapi_scene_t scene) { (void)scene; printf("[Scene] Menu\n"); }
static void on_scene_menu_exit(bapi_scene_t scene) { (void)scene; }
static void on_scene_menu_update(bapi_scene_t scene, float dt) { (void)scene; (void)dt; }
static void on_scene_menu_render(bapi_scene_t scene) {
    (void)scene;
    bapi_draw_text("MENU", CENTER_X - 50, 150, 48, COLOR_YELLOW);
    bapi_draw_text("Welcome to BridgeEngine Demo!", CENTER_X - 150, 220, 24, COLOR_WHITE);
}

static void on_scene_game_enter(bapi_scene_t scene) { (void)scene; printf("[Scene] Game\n"); }
static void on_scene_game_exit(bapi_scene_t scene) { (void)scene; }
static void on_scene_game_update(bapi_scene_t scene, float dt) {
    (void)scene; (void)dt;
    for (int i = 0; i < 5; i++) update_ball(&g_balls[i], WINDOW_WIDTH, WINDOW_HEIGHT);
}
static void on_scene_game_render(bapi_scene_t scene) {
    (void)scene;
    bapi_draw_text("GAME", CENTER_X - 40, 150, 48, COLOR_GREEN);
    
    for (int i = 0; i < 5; i++) draw_ball(&g_balls[i]);
    
    draw_panel(600, 450, 200, 150, "Circles");
    bapi_draw_circle(680, 520, 35, COLOR_RED);
    bapi_fill_circle(750, 520, 30, COLOR_GREEN);
}

static void on_scene_settings_enter(bapi_scene_t scene) { (void)scene; printf("[Scene] Settings\n"); }
static void on_scene_settings_exit(bapi_scene_t scene) { (void)scene; }
static void on_scene_settings_update(bapi_scene_t scene, float dt) { (void)scene; (void)dt; }
static void on_scene_settings_render(bapi_scene_t scene) {
    (void)scene;
    bapi_draw_text("SETTINGS", CENTER_X - 70, 150, 48, COLOR_ORANGE);
    
    draw_panel(CENTER_X - 200, 220, 400, 250, "Configuration");
    bapi_draw_text("Audio Volume: 80%", CENTER_X - 80, 280, 20, COLOR_WHITE);
    bapi_fill_rect(CENTER_X - 150, 310, 240, 20, COLOR_GRAY);
    bapi_fill_rect(CENTER_X - 150, 310, 192, 20, COLOR_GREEN);
    
    bapi_draw_text("Graphics: High", CENTER_X - 60, 360, 20, COLOR_WHITE);
    bapi_draw_text("Fullscreen: Off", CENTER_X - 70, 400, 20, COLOR_WHITE);
}

static void on_scene_video_enter(bapi_scene_t scene) { 
    (void)scene; 
    printf("[Scene] Video\n"); 
    if (g_test_video != NULL) {
        bapi_video_play(g_test_video);
    }
}
static void on_scene_video_exit(bapi_scene_t scene) { 
    (void)scene; 
    if (g_test_video != NULL) {
        bapi_video_stop(g_test_video);
    }
}
static void on_scene_video_update(bapi_scene_t scene, float dt) { (void)scene; (void)dt; }
static void on_scene_video_render(bapi_scene_t scene) {
    (void)scene;
    bapi_draw_text("VIDEO", CENTER_X - 40, 50, 48, COLOR_MAGENTA);
    
    if (g_test_video != NULL) {
        int vw, vh;
        bapi_video_get_size(g_test_video, &vw, &vh);
        
        bapi_video_render_fit(g_test_video, 0, 100, WINDOW_WIDTH, WINDOW_HEIGHT - 250);
        
        draw_panel(50, WINDOW_HEIGHT - 150, 300, 100, "Video Info");
        bapi_draw_text("File: XINGJILOGE.mp4", 70, WINDOW_HEIGHT - 115, 16, COLOR_WHITE);
        char size_text[64];
        snprintf(size_text, sizeof(size_text), "Size: %dx%d", vw, vh);
        bapi_draw_text(size_text, 70, WINDOW_HEIGHT - 90, 16, COLOR_WHITE);
        bapi_draw_text("Status: Playing", 70, WINDOW_HEIGHT - 65, 16, COLOR_GREEN);
    } else {
        bapi_draw_text("Video not loaded!", CENTER_X - 80, CENTER_Y, 24, COLOR_RED);
    }
}

static void on_level_load(bapi_level_t level) {
    printf("[Level] Loaded: %s\n", bapi_level_get_name(level));
}
static void on_level_unload(bapi_level_t level) {
    printf("[Level] Unloaded: %s\n", bapi_level_get_name(level));
}
static void on_level_update(bapi_level_t level, float dt) { (void)level; (void)dt; }
static void on_level_render(bapi_level_t level) {
    const char* name = bapi_level_get_name(level);
    int idx = bapi_level_get_index(level);
    
    bapi_color_t colors[] = {
        bapi_color(34, 139, 34, 255),
        bapi_color(210, 180, 140, 255),
        bapi_color(0, 105, 148, 255)
    };
    
    draw_panel(50, 100, 200, 80, "Current Level");
    bapi_fill_rect(70, 145, 160, 25, colors[(idx - 1) % 3]);
    bapi_draw_text(name, 90, 148, 18, COLOR_WHITE);
}

static void init_managers(void) {
    g_scene_manager = bapi_scene_manager_create();
    
    bapi_scene_callbacks_t menu_cb = {on_scene_menu_enter, on_scene_menu_exit, on_scene_menu_update, on_scene_menu_render, NULL};
    bapi_scene_callbacks_t game_cb = {on_scene_game_enter, on_scene_game_exit, on_scene_game_update, on_scene_game_render, NULL};
    bapi_scene_callbacks_t settings_cb = {on_scene_settings_enter, on_scene_settings_exit, on_scene_settings_update, on_scene_settings_render, NULL};
    bapi_scene_callbacks_t video_cb = {on_scene_video_enter, on_scene_video_exit, on_scene_video_update, on_scene_video_render, NULL};
    
    bapi_scene_manager_add_scene(g_scene_manager, bapi_scene_create("menu", menu_cb));
    bapi_scene_manager_add_scene(g_scene_manager, bapi_scene_create("game", game_cb));
    bapi_scene_manager_add_scene(g_scene_manager, bapi_scene_create("settings", settings_cb));
    bapi_scene_manager_add_scene(g_scene_manager, bapi_scene_create("video", video_cb));
    bapi_scene_manager_switch_scene(g_scene_manager, "menu");
    
    g_level_manager = bapi_level_manager_create();
    
    bapi_level_callbacks_t level_cb = {on_level_load, on_level_unload, on_level_update, on_level_render, NULL};
    bapi_level_manager_add_level(g_level_manager, bapi_level_create("Forest", 1, level_cb));
    bapi_level_manager_add_level(g_level_manager, bapi_level_create("Desert", 2, level_cb));
    bapi_level_manager_add_level(g_level_manager, bapi_level_create("Ocean", 3, level_cb));
    bapi_level_manager_load_level_by_index(g_level_manager, 1);
}

static void handle_keyboard(uint8_t key) {
    switch (key) {
        case '1': bapi_scene_manager_switch_scene(g_scene_manager, "menu"); break;
        case '2': bapi_scene_manager_switch_scene(g_scene_manager, "game"); break;
        case '3': bapi_scene_manager_switch_scene(g_scene_manager, "settings"); break;
        case '4': bapi_scene_manager_switch_scene(g_scene_manager, "video"); break;
        case 'n': case 'N': bapi_level_manager_next_level(g_level_manager); break;
        case 'p': case 'P': bapi_level_manager_previous_level(g_level_manager); break;
        case ' ':
            if (g_test_sound != NULL) {
                bapi_sound_play(g_test_sound);
                printf("[AUDIO] Playing test sound\n");
            }
            break;
    }
}

static void draw_header(void) {
    bapi_fill_rect(0, 0, WINDOW_WIDTH, 50, COLOR_PANEL_BG);
    bapi_draw_text("BridgeEngine v1.0", 20, 12, 28, COLOR_WHITE);
    bapi_draw_text("Scene & Level Management Demo", CENTER_X - 140, 15, 20, COLOR_CYAN);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("   BridgeEngine Feature Demo\n");
    printf("========================================\n\n");
    
    if (bapi_engine_init("BridgeEngine Demo", WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
        printf("[ERROR] Engine init failed!\n");
        return 1;
    }
    
    bapi_mouse_init();
    bapi_text_init();
    init_managers();
    
    if (bapi_audio_init() != 0) {
        printf("[ERROR] Audio init failed!\n");
    } else {
        g_test_sound = bapi_sound_load("audio_example/test.wav");
        if (g_test_sound == NULL) {
            printf("[ERROR] Failed to load test sound!\n");
        } else {
            bapi_sound_set_loop(g_test_sound, 1);
            bapi_sound_play(g_test_sound);
            printf("[INFO] Audio initialized. Press SPACE to play sound.\n");
        }
    }
    
    if (bapi_video_init() != 0) {
        printf("[ERROR] Video init failed!\n");
    } else {
        g_test_video = bapi_video_load("video_example/XINGJILOGE.mp4");
        if (g_test_video == NULL) {
            printf("[ERROR] Failed to load test video!\n");
        } else {
            bapi_video_set_loop(g_test_video, 1);
            printf("[INFO] Video initialized. Press 4 to view video scene.\n");
        }
    }
    
    g_demo_button = bapi_create_button(
        CENTER_X - 80, WINDOW_HEIGHT - 160, 160, 50, "Click Me!",
        bapi_color(60, 130, 60, 255), bapi_color(80, 160, 80, 255),
        bapi_color(40, 100, 40, 255), COLOR_WHITE, 22
    );
    
    printf("[INFO] Demo running. Press ESC to exit.\n\n");
    
    bool running = true;
    int fps = 0, frames = 0;
    uint32_t fps_time = bapi_get_ticks();
    
    while (running) {
        bapi_event_t event;
        while (bapi_poll_event(&event)) {
            int type = bapi_event_get_type(&event);
            
            if (type == BAPI_EVENT_QUIT) running = false;
            else if (type == BAPI_EVENT_KEY_DOWN) {
                uint8_t key = bapi_event_get_key_code(&event);
                if (key == KEY_ESC) running = false;
                else handle_keyboard(key);
            } else {
                bapi_mouse_handle_event(&event);
            }
            
            if (bapi_button_update(g_demo_button, &event)) {
                printf("[EVENT] Button clicked!\n");
            }
        }
        
        bapi_render_clear();
        bapi_set_render_color(COLOR_DARK_BG);
        
        bapi_scene_manager_update(g_scene_manager, 0.016f);
        bapi_scene_manager_render(g_scene_manager);
        
        bapi_level_manager_update(g_level_manager, 0.016f);
        bapi_level_manager_render(g_level_manager);
        
        bapi_sound_update();
        bapi_video_update();
        
        draw_header();
        draw_help_panel();
        draw_color_palette();
        draw_shapes_demo();
        bapi_button_render(g_demo_button);
        
        bapi_mouse_render();
        
        frames++;
        uint32_t now = bapi_get_ticks();
        if (now - fps_time >= 1000) {
            fps = frames;
            frames = 0;
            fps_time = now;
            printf("\r[FPS: %d] ", fps);
        }
        
        bapi_render_present();
    }
    
    printf("\n\n[INFO] Shutting down...\n");
    
    bapi_destroy_button(g_demo_button);
    bapi_scene_manager_destroy(g_scene_manager);
    bapi_level_manager_destroy(g_level_manager);
    bapi_text_cleanup();
    if (g_test_sound != NULL) {
        bapi_sound_stop(g_test_sound);
        bapi_sound_free(g_test_sound);
    }
    bapi_audio_cleanup();
    if (g_test_video != NULL) {
        bapi_video_stop(g_test_video);
        bapi_video_free(g_test_video);
    }
    bapi_video_cleanup();
    bapi_mouse_cleanup();
    bapi_engine_quit();
    
    printf("[INFO] Cleanup complete. Goodbye!\n");
    return 0;
}
