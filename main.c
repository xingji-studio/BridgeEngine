#include "BridgeEngine.h"
#include "bapi_internal.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

typedef struct {
    float x, y;
    float vx, vy;
    float radius;
    bapi_color_t color;
} Ball;

static Ball balls[5] = {
    {100, 100, 3, 2, 30, {255, 0, 0, 255}},
    {200, 200, -2, 3, 25, {0, 255, 0, 255}},
    {300, 150, 2.5, -2, 35, {0, 0, 255, 255}},
    {400, 300, -3, -1.5, 28, {255, 255, 0, 255}},
    {500, 200, 2.5, 1.5, 32, {255, 0, 255, 255}},
};

static void draw_ball(Ball* ball) {
    int segments = 32;
    float angle_step = 2.0f * M_PI / segments;
    
    for (int i = 0; i < segments; i++) {
        float angle1 = i * angle_step;
        float angle2 = (i + 1) * angle_step;
        
        float x1 = ball->x + cosf(angle1) * ball->radius;
        float y1 = ball->y + sinf(angle1) * ball->radius;
        float x2 = ball->x + cosf(angle2) * ball->radius;
        float y2 = ball->y + sinf(angle2) * ball->radius;
        
        bapi_draw_triangle(ball->x, ball->y, x1, y1, x2, y2, ball->color);
    }
}

static void update_ball(Ball* ball, int width, int height) {
    ball->x += ball->vx;
    ball->y += ball->vy;
    
    if (ball->x - ball->radius < 0) {
        ball->x = ball->radius;
        ball->vx = -ball->vx;
    }
    if (ball->x + ball->radius > width) {
        ball->x = width - ball->radius;
        ball->vx = -ball->vx;
    }
    if (ball->y - ball->radius < 0) {
        ball->y = ball->radius;
        ball->vy = -ball->vy;
    }
    if (ball->y + ball->radius > height) {
        ball->y = height - ball->radius;
        ball->vy = -ball->vy;
    }
}

int main(int argc, char* argv[]) {
    printf("===========================================\n");
    printf("  BridgeEngine Complete Feature Demo\n");
    printf("===========================================\n\n");
    
    printf("[1/6] Initializing engine...\n");
    if (bapi_engine_init("BridgeEngine Demo", WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
        printf("Failed to initialize engine!\n");
        return 1;
    }
    printf("  Engine initialized successfully.\n");
    
    printf("[6/6] Initializing mouse drawing...\n");
    bapi_mouse_init();
    printf("  Mouse drawing initialized.\n");
    
    bapi_button_t* test_button = bapi_create_button(
        WINDOW_WIDTH / 2 - 100, 300, 200, 60, "Click Me!",
        bapi_color(50, 150, 50, 255), 
        bapi_color(70, 180, 70, 255), 
        bapi_color(30, 120, 30, 255), 
        bapi_color(255, 255, 255, 255),  
        24 
    );
    
    printf("\n===========================================\n");
    printf("  Demo is running!\n");
    printf("  - Balls bounce around the screen\n");
    printf("  - Click and drag to draw lines\n");
    printf("  - Press ESC to exit\n");
    printf("===========================================\n\n");
    
    bool running = true;
    int frameCount = 0;
    int fps = 0;
    uint32_t lastTime = bapi_get_ticks();
    uint32_t frameStartTime = lastTime;
    
    while (running) {
        bapi_event_t event;
        while (bapi_poll_event(&event)) {
            int type = bapi_event_get_type(&event);
            
            if (type == BAPI_EVENT_QUIT) {
                running = false;
                printf("\nQuit event received.\n");
            } else if (type == BAPI_EVENT_KEY_DOWN) {
                if (bapi_event_get_key_code(&event) == KEY_ESC) {
                    running = false;
                    printf("\nESC key pressed, exiting...\n");
                }
            } else {
                bapi_mouse_handle_event(&event);
            }
            
            // Update button state
            if (bapi_button_update(test_button, &event)) {
                printf("\nButton clicked!\n");
            }
        }
        
        bapi_render_clear();
        
        bapi_set_render_color(bapi_color(30, 30, 40, 255));
        
        for (int i = 0; i < 5; i++) {
            update_ball(&balls[i], WINDOW_WIDTH, WINDOW_HEIGHT);
        }
        
        for (int i = 0; i < 5; i++) {
            draw_ball(&balls[i]);
        }
        
        bapi_draw_rect(50, 50, 200, 150, bapi_color(100, 100, 100, 100));
        bapi_fill_rect(60, 60, 180, 50, bapi_color(50, 150, 50, 255));
        bapi_draw_pixel(100, 100, bapi_color(255, 0, 0, 255));
        
        bapi_draw_triangle(800, 100, 850, 200, 750, 200, bapi_color(255, 165, 0, 255));
        
        bapi_draw_image("image_example/XINGJI.png", 850, 550, 120, 120);
        
        bapi_draw_text("BridgeEngine", WINDOW_WIDTH / 2 - 200, 0, 48, bapi_color(255, 255, 255, 255));
        bapi_draw_text("Press ESC to exit | Click and drag to draw", WINDOW_WIDTH / 2 - 220, 80, 24, bapi_color(255, 255, 255, 255));
        
        
        bapi_fill_rect(50, 300, 60, 60, bapi_color_from_hex(0xFF0000FF));
        bapi_fill_rect(120, 300, 60, 60, bapi_color_from_hex(0x00FF00FF));
        bapi_fill_rect(190, 300, 60, 60, bapi_color_from_hex(0x0000FFFF));
        bapi_fill_rect(260, 300, 60, 60, bapi_color_from_hex(0xFFFF00FF));
        bapi_fill_rect(330, 300, 60, 60, bapi_color_from_hex(0xFF00FFFF));
        bapi_fill_rect(400, 300, 60, 60, bapi_color_from_hex(0x00FFFFFF));
        bapi_fill_rect(470, 300, 60, 60, bapi_color_from_hex(0xFFFFFFFF));
        bapi_fill_rect(540, 300, 60, 60, bapi_color_from_hex(0x808080FF));
        
        // Render button
        bapi_button_render(test_button);
        
        bapi_mouse_render();
        
        frameCount++;
        uint32_t currentTime = bapi_get_ticks();
        if (currentTime - frameStartTime >= 1000) {
            fps = frameCount;
            frameCount = 0;
            frameStartTime = currentTime;
            printf("\rFPS: %d", fps);
            fflush(stdout);
        }
        
        bapi_render_present();
        // bapi_delay(16);
    }
    
    printf("\n\n===========================================\n");
    printf("  Cleaning up resources...\n");
    printf("===========================================\n");
    
    // Destroy button
    bapi_destroy_button(test_button);
    
    bapi_mouse_cleanup();
    bapi_engine_quit();
    
    printf("  Cleanup complete.\n");
    printf("===========================================\n");
    printf("  Demo finished successfully!\n");
    printf("===========================================\n");
    
    return 0;
}