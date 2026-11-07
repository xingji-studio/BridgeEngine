#define BAPI_LOG_ENABLED

#include "BridgeEngine.h"

#include <math.h>
#include <stdio.h>

#define WINDOW_WIDTH	 960
#define WINDOW_HEIGHT	 540
#define TILE_SIZE		 32
#define MAP_WIDTH		 40
#define MAP_HEIGHT		 24
#define FIXED_STEP		 (1.0f / 60.0f)
#define MAX_FRAME_STEPS	 5
#define PLAYER_BODY_SIZE 16.0f
#define SLIME_BODY_SIZE	 20.0f
#define PLAYER_SPEED	 170.0f
#define SLIME_SPEED		 78.0f
#define SPRITE_SCALE	 2.0f
#define COIN_COUNT		 3
#define COIN_LAYER		 2u
#define REPATH_INTERVAL	 0.35f

typedef enum {
	GAME_PLAYING = 0,
	GAME_WON,
	GAME_LOST,
} game_state_t;

typedef struct {
	bapi_vec2_t		position;
	bapi_collider_t collider;
	int				collected;
} coin_t;

typedef struct {
	bapi_world_t			 world;
	bapi_entity_t			 player;
	bapi_entity_t			 slime;
	bapi_tilemap_t			 map;
	bapi_texture_t			 player_sheet;
	bapi_animator_t			 animator;
	bapi_timer_manager_t	 timers;
	bapi_path_t				 slime_path;
	bapi_camera_controller_t camera_controller;
	bapi_collision_world_t	 collision;
	bapi_tween_manager_t	 tweens;
	bapi_tween_t			 coin_pulse;
	bapi_particle_system_t	 dust;
	bapi_ui_t				 hud;
	coin_t					 coins[COIN_COUNT];
	game_state_t			 state;
	int						 coins_left;
	int						 slime_path_index;
	int						 repath_requested;
	float					 elapsed;
} game_t;

static void set_label(bapi_ui_t ui, const char *id, const char *text)
{
	bapi_ui_component_t label = bapi_ui_find(ui, id);
	if (label) bapi_ui_component_set_text(label, text);
}

static void set_visible(bapi_ui_t ui, const char *id, int visible)
{
	bapi_ui_component_t component = bapi_ui_find(ui, id);
	if (component) bapi_ui_component_set_visible(component, visible);
}

static const char *find_asset(const char *primary, const char *fallback)
{
	FILE *file = fopen(primary, "rb");
	if (file) {
		fclose(file);
		return primary;
	}
	file = fopen(fallback, "rb");
	if (file) {
		fclose(file);
		return fallback;
	}
	return NULL;
}

static int report_error(const char *message)
{
	const char *detail = bapi_get_last_error();
	fprintf(stderr, "%s%s%s\n", message, detail && detail[0] ? ": " : "", detail ? detail : "");
	return -1;
}

static bapi_vec2_t tile_center(int x, int y)
{
	return (bapi_vec2_t){(x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE};
}

static bapi_rect_t actor_body(bapi_vec2_t position, float size)
{
	return (bapi_rect_t){position.x - size * 0.5f, position.y - size * 0.5f, size, size};
}

static void request_repath(void *user_data)
{
	((game_t *)user_data)->repath_requested = 1;
}

static void build_map(bapi_tilemap_t map)
{
	for (int x = 0; x < MAP_WIDTH; x++) {
		bapi_tilemap_set_tile(map, x, 0, 2, 1);
		bapi_tilemap_set_tile(map, x, MAP_HEIGHT - 1, 2, 1);
	}
	for (int y = 0; y < MAP_HEIGHT; y++) {
		bapi_tilemap_set_tile(map, 0, y, 2, 1);
		bapi_tilemap_set_tile(map, MAP_WIDTH - 1, y, 2, 1);
	}
	for (int x = 8; x < 15; x++) bapi_tilemap_set_tile(map, x, 8, 1, 1);
	for (int y = 12; y < 17; y++) {
		bapi_tilemap_set_tile(map, 18, y, 3, 1);
		bapi_tilemap_set_tile(map, 27, y, 3, 1);
	}
	for (int x = 22; x < 25; x++) bapi_tilemap_set_tile(map, x, 17, 1, 1);
}

static int update_player(game_t *game, float dt)
{
	bapi_vec2_t direction = {0.0f, 0.0f};
	if (bapi_input_action_down("left")) direction.x -= 1.0f;
	if (bapi_input_action_down("right")) direction.x += 1.0f;
	if (bapi_input_action_down("up")) direction.y -= 1.0f;
	if (bapi_input_action_down("down")) direction.y += 1.0f;

	bapi_vec2_t delta = {0.0f, 0.0f};
	if (direction.x != 0.0f || direction.y != 0.0f) {
		float length = sqrtf(direction.x * direction.x + direction.y * direction.y);
		delta.x		 = direction.x / length * PLAYER_SPEED * dt;
		delta.y		 = direction.y / length * PLAYER_SPEED * dt;
	}

	bapi_vec2_t before = bapi_entity_get_position(game->player);
	bapi_rect_t body   = actor_body(before, PLAYER_BODY_SIZE);
	bapi_tilemap_move_rect(game->map, &body, delta);
	bapi_vec2_t after = {body.x + PLAYER_BODY_SIZE * 0.5f, body.y + PLAYER_BODY_SIZE * 0.5f};
	bapi_entity_set_position(game->player, after);

	if (direction.x != 0.0f) {
		bapi_entity_set_scale(
			game->player,
			(bapi_vec2_t){direction.x < 0.0f ? -SPRITE_SCALE : SPRITE_SCALE, SPRITE_SCALE});
	}

	float moved_x = after.x - before.x;
	float moved_y = after.y - before.y;
	int	  moving  = moved_x * moved_x + moved_y * moved_y > 0.000001f;
	bapi_animator_set_bool(game->animator, "moving", moving);
	bapi_animator_update(game->animator, dt);
	bapi_entity_set_texture(game->player, bapi_animator_get_texture(game->animator),
							bapi_animator_get_frame(game->animator));
	return moving;
}

static void repath_slime(game_t *game)
{
	if (game->slime_path) bapi_path_destroy(game->slime_path);
	bapi_vec2_t slime_position	= bapi_entity_get_position(game->slime);
	bapi_vec2_t player_position = bapi_entity_get_position(game->player);
	game->slime_path			= bapi_path_find(
		game->map, (int)(slime_position.x / TILE_SIZE), (int)(slime_position.y / TILE_SIZE),
		(int)(player_position.x / TILE_SIZE), (int)(player_position.y / TILE_SIZE), 0);
	game->slime_path_index = 0;
	game->repath_requested = 0;
}

static void update_slime(game_t *game, float dt)
{
	if (game->repath_requested) repath_slime(game);
	if (!game->slime_path || game->slime_path_index >= bapi_path_count(game->slime_path)) return;

	int tx = 0;
	int ty = 0;
	if (bapi_path_get(game->slime_path, game->slime_path_index, &tx, &ty) != 0) {
		game->slime_path_index++;
		return;
	}

	bapi_vec2_t position = bapi_entity_get_position(game->slime);
	bapi_vec2_t target	 = tile_center(tx, ty);
	float		dx		 = target.x - position.x;
	float		dy		 = target.y - position.y;
	float		distance = sqrtf(dx * dx + dy * dy);
	float		move	 = SLIME_SPEED * dt;
	if (distance <= move) {
		bapi_entity_set_position(game->slime, target);
		game->slime_path_index++;
	} else if (distance > 0.0f) {
		bapi_entity_set_position(game->slime, (bapi_vec2_t){position.x + dx / distance * move,
															position.y + dy / distance * move});
	}
}

static void finish_game(game_t *game, game_state_t state)
{
	game->state = state;
	bapi_animator_set_bool(game->animator, "moving", 0);
	bapi_animator_update(game->animator, 0.0f);
	bapi_entity_set_texture(game->player, bapi_animator_get_texture(game->animator),
							bapi_animator_get_frame(game->animator));
	bapi_camera_controller_shake(game->camera_controller, state == GAME_WON ? 3.0f : 7.0f,
								 state == GAME_WON ? 0.2f : 0.35f);
}

static void collect_coins(game_t *game)
{
	bapi_collider_t picked[COIN_COUNT];
	bapi_vec2_t		position	 = bapi_entity_get_position(game->player);
	int				picked_count = bapi_collision_world_query(
		game->collision, actor_body(position, PLAYER_BODY_SIZE), COIN_LAYER, picked, COIN_COUNT);
	if (picked_count > COIN_COUNT) picked_count = COIN_COUNT;

	for (int i = 0; i < picked_count; i++) {
		for (int k = 0; k < COIN_COUNT; k++) {
			coin_t *coin = &game->coins[k];
			if (picked[i] != coin->collider || coin->collected) continue;
			coin->collected = 1;
			bapi_collider_set_layer_mask(coin->collider, 0, 0);
			game->coins_left--;
			bapi_camera_controller_shake(game->camera_controller, 2.0f, 0.12f);
			if (game->coins_left == 0) finish_game(game, GAME_WON);
		}
	}
}

static void update_game(game_t *game, float dt)
{
	if (game->state == GAME_PLAYING) {
		game->elapsed += dt;
		int			moving			= update_player(game, dt);
		bapi_vec2_t player_position = bapi_entity_get_position(game->player);
		if (moving) {
			bapi_particle_system_set_position(game->dust, player_position.x,
											  player_position.y + 8.0f);
			bapi_particle_system_emit(game->dust, 1);
		}

		bapi_world_update(game->world, dt);
		bapi_timer_manager_update(game->timers, dt);
		update_slime(game, dt);
		collect_coins(game);

		bapi_vec2_t slime_position = bapi_entity_get_position(game->slime);
		if (game->state == GAME_PLAYING &&
			bapi_rect_intersects(actor_body(player_position, PLAYER_BODY_SIZE),
								 actor_body(slime_position, SLIME_BODY_SIZE))) {
			finish_game(game, GAME_LOST);
		}
	}

	bapi_vec2_t player_position = bapi_entity_get_position(game->player);
	bapi_camera_controller_update(game->camera_controller, dt, player_position.x,
								  player_position.y);
	bapi_particle_system_update(game->dust, dt);
	bapi_tween_manager_update(game->tweens, dt);
}

static void reset_game(game_t *game)
{
	game->state			   = GAME_PLAYING;
	game->coins_left	   = COIN_COUNT;
	game->elapsed		   = 0.0f;
	game->slime_path_index = 0;
	game->repath_requested = 1;
	if (game->slime_path) {
		bapi_path_destroy(game->slime_path);
		game->slime_path = NULL;
	}

	bapi_entity_set_position(game->player, tile_center(20, 12));
	bapi_entity_set_scale(game->player, (bapi_vec2_t){SPRITE_SCALE, SPRITE_SCALE});
	bapi_entity_set_position(game->slime, tile_center(5, 5));
	for (int i = 0; i < COIN_COUNT; i++) {
		game->coins[i].collected = 0;
		bapi_collider_set_layer_mask(game->coins[i].collider, COIN_LAYER, 1u);
	}

	bapi_animator_set_bool(game->animator, "moving", 0);
	bapi_animator_play(game->animator, "idle");
	bapi_animator_update(game->animator, 0.0f);
	bapi_entity_set_texture(game->player, game->player_sheet, (bapi_rect_t){0, 0, 16, 16});

	bapi_vec2_t player_position = bapi_entity_get_position(game->player);
	bapi_camera_controller_follow(game->camera_controller, player_position.x, player_position.y);
	bapi_camera_controller_shake(game->camera_controller, 0.0f, 0.0f);
	bapi_camera_controller_snap(game->camera_controller);
}

static void draw_floor(void)
{
	bapi_fill_rect(0, 0, MAP_WIDTH * (float)TILE_SIZE, MAP_HEIGHT * (float)TILE_SIZE,
				   bapi_color(18, 22, 32, 255));
	for (int x = 0; x <= MAP_WIDTH; x++) {
		float line_x = x * (float)TILE_SIZE;
		bapi_draw_line(line_x, 0, line_x, MAP_HEIGHT * (float)TILE_SIZE,
					   bapi_color(25, 31, 44, 255));
	}
	for (int y = 0; y <= MAP_HEIGHT; y++) {
		float line_y = y * (float)TILE_SIZE;
		bapi_draw_line(0, line_y, MAP_WIDTH * (float)TILE_SIZE, line_y,
					   bapi_color(25, 31, 44, 255));
	}
}

static void draw_coin(const coin_t *coin, float pulse)
{
	if (coin->collected) return;
	bapi_fill_circle(coin->position.x, coin->position.y, 11.0f * pulse,
					 bapi_color(255, 210, 70, 35));
	bapi_fill_circle(coin->position.x, coin->position.y, 7.0f * pulse,
					 bapi_color(255, 210, 70, 255));
	bapi_draw_circle(coin->position.x, coin->position.y, 9.0f * pulse,
					 bapi_color(255, 235, 150, 220));
	bapi_fill_circle(coin->position.x - 2.0f, coin->position.y - 2.0f, 2.0f * pulse,
					 bapi_color(255, 248, 205, 255));
}

static void update_hud(game_t *game)
{
	if (!game->hud) return;
	char buffer[128];
	snprintf(buffer, sizeof(buffer), "FPS %.0f", bapi_get_fps());
	set_label(game->hud, "hud_stats", buffer);
	snprintf(buffer, sizeof(buffer), "TIME %.1f", game->elapsed);
	set_label(game->hud, "hud_time", buffer);
	snprintf(buffer, sizeof(buffer), "COINS %d / %d", COIN_COUNT - game->coins_left, COIN_COUNT);
	set_label(game->hud, "hud_coins", buffer);

	set_visible(game->hud, "result_panel", game->state != GAME_PLAYING);
	if (game->state == GAME_WON) {
		set_label(game->hud, "result_title", "ALL COINS COLLECTED!");
		snprintf(buffer, sizeof(buffer), "Finished in %.1f seconds.", game->elapsed);
		set_label(game->hud, "result_detail", buffer);
	} else if (game->state == GAME_LOST) {
		set_label(game->hud, "result_title", "CAUGHT BY THE HUNTER");
		set_label(game->hud, "result_detail", "Try a different route and keep moving.");
	}
}

static void render_game(game_t *game)
{
	bapi_set_render_color(bapi_color(10, 12, 18, 255));
	bapi_render_clear();

	bapi_camera_t camera;
	bapi_camera_controller_get_camera(game->camera_controller, &camera);
	bapi_world_set_camera(game->world, camera);
	bapi_apply_camera(&camera);
	draw_floor();
	bapi_tilemap_draw(game->map);
	bapi_particle_system_draw(game->dust);
	float pulse = bapi_tween_value(game->coin_pulse);
	for (int i = 0; i < COIN_COUNT; i++) draw_coin(&game->coins[i], pulse);
	bapi_reset_transform();
	bapi_world_draw(game->world);
	bapi_reset_transform();

	update_hud(game);
	bapi_ui_render(game->hud);
	bapi_mouse_render();
}

static int handle_events(game_t *game)
{
	bapi_event_t event;
	while (bapi_poll_event(&event)) {
		int type = bapi_event_get_type(&event);
		bapi_input_handle_event(&event);
		bapi_mouse_handle_event(&event);
		bapi_ui_update(game->hud, &event);
		if (type == BAPI_EVENT_QUIT) return 0;
		if (type == BAPI_EVENT_KEY_DOWN && bapi_event_get_key_code(&event) == KEY_ESC) return 0;
		if (type == BAPI_EVENT_WINDOW_RESIZED) {
			bapi_camera_controller_set_viewport(game->camera_controller,
												(float)bapi_event_get_window_width(&event),
												(float)bapi_event_get_window_height(&event));
		}
	}
	return 1;
}

static int init_game(game_t *game)
{
	*game = (game_t){0};
	const char *player_path =
		find_asset("assets/vertical_slice/player.png", "examples/assets/vertical_slice/player.png");
	const char *hud_path = find_asset("assets/ui/hud.xml", "examples/assets/ui/hud.xml");
	if (!player_path || !hud_path) {
		fprintf(stderr,
				"missing vertical slice assets; run from the repository or build directory\n");
		return -1;
	}

	game->player_sheet = bapi_texture_load(player_path);
	if (!game->player_sheet) return report_error("failed to load player sprite");

	bapi_rect_t idle_frame	  = {0, 0, 16, 16};
	float		idle_duration = 1.0f;
	bapi_rect_t run_frames[4] = {{0, 0, 16, 16}, {16, 0, 16, 16}, {32, 0, 16, 16}, {48, 0, 16, 16}};
	float		run_durations[4] = {0.10f, 0.10f, 0.10f, 0.10f};
	game->animator				 = bapi_animator_create();
	if (!game->animator) return report_error("failed to create animator");
	if (bapi_animator_add_clip(game->animator, "idle", game->player_sheet, &idle_frame,
							   &idle_duration, 1, 1) != 0 ||
		bapi_animator_add_clip(game->animator, "run", game->player_sheet, run_frames, run_durations,
							   4, 1) != 0 ||
		bapi_animator_add_transition(game->animator, "idle", "run", "moving", 1) != 0 ||
		bapi_animator_add_transition(game->animator, "run", "idle", "moving", 0) != 0) {
		return report_error("failed to configure animator");
	}

	game->world = bapi_world_create();
	if (!game->world) return report_error("failed to create world");
	game->player = bapi_world_spawn(game->world, "player");
	game->slime	 = bapi_world_spawn(game->world, "slime");
	if (!game->player || !game->slime) return report_error("failed to create actors");
	bapi_entity_set_scale(game->player, (bapi_vec2_t){SPRITE_SCALE, SPRITE_SCALE});
	bapi_entity_set_scale(game->slime, (bapi_vec2_t){SPRITE_SCALE, SPRITE_SCALE});
	bapi_entity_set_texture(game->slime, game->player_sheet, idle_frame);
	bapi_entity_set_tint(game->slime, bapi_color(140, 255, 160, 255));

	game->map = bapi_tilemap_create(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE, TILE_SIZE);
	if (!game->map) return report_error("failed to create tilemap");
	build_map(game->map);

	game->timers = bapi_timer_manager_create();
	if (!game->timers ||
		bapi_timer_every(game->timers, REPATH_INTERVAL, request_repath, game) < 0) {
		return report_error("failed to create timers");
	}

	game->camera_controller =
		bapi_camera_controller_create((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
	if (!game->camera_controller) return report_error("failed to create camera controller");
	bapi_camera_controller_set_bounds(
		game->camera_controller,
		(bapi_rect_t){0, 0, MAP_WIDTH * (float)TILE_SIZE, MAP_HEIGHT * (float)TILE_SIZE});
	bapi_camera_controller_set_deadzone(game->camera_controller, 140.0f, 90.0f);
	bapi_camera_controller_set_smoothing(game->camera_controller, 8.0f);

	game->collision = bapi_collision_world_create();
	if (!game->collision) return report_error("failed to create collision world");
	const bapi_vec2_t coin_positions[COIN_COUNT] = {
		{200.0f, 280.0f},
		{520.0f, 220.0f},
		{760.0f, 430.0f},
	};
	for (int i = 0; i < COIN_COUNT; i++) {
		game->coins[i].position = coin_positions[i];
		game->coins[i].collider = bapi_collision_world_add(
			game->collision, actor_body(coin_positions[i], 16.0f), COIN_LAYER, 1u, 0);
		if (!game->coins[i].collider) return report_error("failed to create coin collider");
	}

	game->tweens = bapi_tween_manager_create();
	if (!game->tweens) return report_error("failed to create tween manager");
	game->coin_pulse =
		bapi_tween_create(game->tweens, 0.75f, 1.15f, 0.7f, BAPI_EASE_IN_OUT_QUAD, 1);
	if (!game->coin_pulse) return report_error("failed to create coin animation");

	game->dust = bapi_particle_system_create(192);
	if (!game->dust) return report_error("failed to create particle system");
	bapi_particle_config_t dust_config = {
		.velocity_min  = {-45.0f, -20.0f},
		.velocity_max  = {45.0f, 10.0f},
		.color_start   = {220, 210, 190, 190},
		.color_end	   = {120, 110, 100, 0},
		.life_min	   = 0.25f,
		.life_max	   = 0.55f,
		.size_min	   = 2.0f,
		.size_max	   = 5.0f,
		.gravity	   = 60.0f,
		.emission_rate = 0.0f,
	};
	bapi_particle_system_set_config(game->dust, &dust_config);

	game->hud = bapi_ui_load_from_file(hud_path, "");
	if (!game->hud) return report_error("failed to load HUD");
	reset_game(game);
	return 0;
}

static void destroy_game(game_t *game)
{
	if (game->hud) bapi_ui_destroy(game->hud);
	if (game->slime_path) bapi_path_destroy(game->slime_path);
	if (game->timers) bapi_timer_manager_destroy(game->timers);
	if (game->dust) bapi_particle_system_destroy(game->dust);
	if (game->coin_pulse) bapi_tween_destroy(game->tweens, game->coin_pulse);
	if (game->tweens) bapi_tween_manager_destroy(game->tweens);
	if (game->collision) bapi_collision_world_destroy(game->collision);
	if (game->camera_controller) bapi_camera_controller_destroy(game->camera_controller);
	if (game->map) bapi_tilemap_destroy(game->map);
	if (game->world) bapi_world_destroy(game->world);
	if (game->animator) bapi_animator_destroy(game->animator);
	if (game->player_sheet) bapi_texture_destroy(game->player_sheet);
	*game = (game_t){0};
}

int main(void)
{
	BAPI_LOG_INIT_DEFAULT();
	if (bapi_engine_init("BridgeEngine Coin Runner", WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
		fprintf(stderr, "engine init failed: %s\n", bapi_get_last_error());
		return 1;
	}
	bapi_set_target_fps(60);
	bapi_mouse_init();
	bapi_input_init();
	bapi_text_init();

	bapi_input_action_clear();
	bapi_input_action_bind("left", 'a');
	bapi_input_action_bind("right", 'd');
	bapi_input_action_bind("up", 'w');
	bapi_input_action_bind("down", 's');
	bapi_input_action_bind("restart", 'r');

	game_t game;
	int	   result = 0;
	if (init_game(&game) != 0) {
		result = 1;
	} else {
		int running = 1;
		while (running) {
			bapi_frame_begin();
			bapi_input_update();
			running = handle_events(&game);
			if (running && bapi_input_action_pressed("restart")) reset_game(&game);

			int steps = bapi_frame_steps(FIXED_STEP, MAX_FRAME_STEPS);
			for (int i = 0; i < steps; i++) update_game(&game, FIXED_STEP);
			render_game(&game);
			bapi_frame_end();
		}
	}

	destroy_game(&game);
	bapi_text_cleanup();
	bapi_input_cleanup();
	bapi_mouse_cleanup();
	bapi_engine_quit();
	return result;
}
