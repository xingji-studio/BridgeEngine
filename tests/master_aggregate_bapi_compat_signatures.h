#define MASTER_AGGREGATE_BAPI_SIGNATURES(X) \
	X(bapi_engine_init, int (*)(const char *, int, int)) \
	X(bapi_engine_init_ex, int (*)(const char *, int, int, bapi_renderer_backend_t)) \
	X(bapi_engine_get_renderer_backend, bapi_renderer_backend_t (*)(void)) \
	X(bapi_engine_quit, void (*)(void)) \
	X(bapi_engine_get_window, bapi_window_t (*)(void)) \
	X(bapi_engine_get_renderer, bapi_renderer_t (*)(void)) \
	X(bapi_poll_event, int (*)(bapi_event_t *)) \
	X(bapi_event_get_type, int (*)(const bapi_event_t *)) \
	X(bapi_event_get_key_code, uint8_t (*)(const bapi_event_t *)) \
	X(bapi_event_get_mouse_x, int (*)(const bapi_event_t *)) \
	X(bapi_event_get_mouse_y, int (*)(const bapi_event_t *)) \
	X(bapi_event_get_mouse_button, int (*)(const bapi_event_t *)) \
	X(bapi_event_get_motion_x, int (*)(const bapi_event_t *)) \
	X(bapi_event_get_motion_y, int (*)(const bapi_event_t *)) \
	X(bapi_event_is_mouse_button_down, int (*)(const bapi_event_t *)) \
	X(bapi_event_is_mouse_button_up, int (*)(const bapi_event_t *)) \
	X(bapi_event_is_mouse_motion, int (*)(const bapi_event_t *)) \
	X(bapi_render_clear, void (*)(void)) \
	X(bapi_render_present, void (*)(void)) \
	X(bapi_set_render_color, void (*)(bapi_color_t)) \
	X(bapi_delay, void (*)(uint32_t)) \
	X(bapi_draw_pixel, void (*)(float, float, bapi_color_t)) \
	X(bapi_draw_line, void (*)(float, float, float, float, bapi_color_t)) \
	X(bapi_draw_rect, void (*)(float, float, float, float, bapi_color_t)) \
	X(bapi_fill_rect, void (*)(float, float, float, float, bapi_color_t)) \
	X(bapi_draw_triangle, void (*)(float, float, float, float, float, float, bapi_color_t)) \
	X(bapi_draw_circle, void (*)(float, float, float, bapi_color_t)) \
	X(bapi_fill_circle, void (*)(float, float, float, bapi_color_t)) \
	X(bapi_draw_polygon, void (*)(float, float, float, int, bapi_color_t)) \
	X(bapi_fill_polygon, void (*)(float, float, float, int, bapi_color_t)) \
	X(bapi_draw_image, void (*)(const char *, float, float, float, float)) \
	X(bapi_draw_text, void (*)(const char *, float, float, float, bapi_color_t)) \
	X(bapi_get_text_size, void (*)(const char *, float, float *, float *)) \
	X(bapi_text_init, void (*)(void)) \
	X(bapi_text_cleanup, void (*)(void)) \
	X(bapi_mouse_init, void (*)(void)) \
	X(bapi_mouse_handle_event, void (*)(const bapi_event_t *)) \
	X(bapi_mouse_render, void (*)(void)) \
	X(bapi_mouse_draw_line, void (*)(float, float, float, float, bapi_color_t)) \
	X(bapi_mouse_clear, void (*)(void)) \
	X(bapi_mouse_cleanup, void (*)(void)) \
	X(bapi_color_from_hex, bapi_color_t (*)(uint32_t)) \
	X(bapi_color, bapi_color_t (*)(uint8_t, uint8_t, uint8_t, uint8_t)) \
	X(bapi_get_ticks, uint32_t (*)(void)) \
	X(bapi_audio_init, int (*)(void)) \
	X(bapi_audio_cleanup, void (*)(void)) \
	X(bapi_sound_load, bapi_sound_t (*)(const char *)) \
	X(bapi_sound_play, int (*)(bapi_sound_t)) \
	X(bapi_sound_set_volume, void (*)(bapi_sound_t, float)) \
	X(bapi_sound_set_loop, void (*)(bapi_sound_t, int)) \
	X(bapi_sound_stop, void (*)(bapi_sound_t)) \
	X(bapi_sound_update, void (*)(void)) \
	X(bapi_sound_free, void (*)(bapi_sound_t)) \
	X(bapi_video_init, int (*)(void)) \
	X(bapi_video_cleanup, void (*)(void)) \
	X(bapi_video_load, bapi_video_t (*)(const char *)) \
	X(bapi_video_free, void (*)(bapi_video_t)) \
	X(bapi_video_play, int (*)(bapi_video_t)) \
	X(bapi_video_pause, void (*)(bapi_video_t)) \
	X(bapi_video_stop, void (*)(bapi_video_t)) \
	X(bapi_video_render, void (*)(bapi_video_t, int, int, int, int)) \
	X(bapi_video_render_fit, void (*)(bapi_video_t, int, int, int, int)) \
	X(bapi_video_render_center, void (*)(bapi_video_t, int, int)) \
	X(bapi_video_set_loop, void (*)(bapi_video_t, int)) \
	X(bapi_video_set_volume, void (*)(bapi_video_t, float)) \
	X(bapi_video_is_playing, int (*)(bapi_video_t)) \
	X(bapi_video_get_size, void (*)(bapi_video_t, int *, int *)) \
	X(bapi_video_update, void (*)(void)) \
	X(bapi_log_init, bool (*)(const bapi_log_config_t *)) \
	X(bapi_log_shutdown, void (*)(void)) \
	X(bapi_log_set_level, void (*)(bapi_log_level_t)) \
	X(bapi_log_message, void (*)(bapi_log_level_t, const char *, int, const char *, const char *, ...)) \
	X(bridgeengine_get_version, const char *(*)(void)) \
	X(bridgeengine_get_version_number, int (*)(void)) \
	X(bapi_create_button, bapi_button_t *(*)(float, float, float, float, const char *, bapi_color_t, bapi_color_t, bapi_color_t, bapi_color_t, float)) \
	X(bapi_destroy_button, void (*)(bapi_button_t *)) \
	X(bapi_button_update, int (*)(bapi_button_t *, const bapi_event_t *)) \
	X(bapi_button_render, void (*)(bapi_button_t *)) \
	X(bapi_button_is_clicked, int (*)(bapi_button_t *)) \
	X(bapi_button_is_hovered, int (*)(bapi_button_t *)) \
	X(bapi_scene_create, bapi_scene_t (*)(const char *, bapi_scene_callbacks_t)) \
	X(bapi_scene_destroy, void (*)(bapi_scene_t)) \
	X(bapi_scene_get_name, const char *(*)(bapi_scene_t)) \
	X(bapi_scene_get_user_data, void *(*)(bapi_scene_t)) \
	X(bapi_scene_set_user_data, void (*)(bapi_scene_t, void *)) \
	X(bapi_scene_manager_create, bapi_scene_manager_t (*)(void)) \
	X(bapi_scene_manager_destroy, void (*)(bapi_scene_manager_t)) \
	X(bapi_scene_manager_add_scene, int (*)(bapi_scene_manager_t, bapi_scene_t)) \
	X(bapi_scene_manager_switch_scene, int (*)(bapi_scene_manager_t, const char *)) \
	X(bapi_scene_manager_get_current_scene, bapi_scene_t (*)(bapi_scene_manager_t)) \
	X(bapi_scene_manager_get_scene, bapi_scene_t (*)(bapi_scene_manager_t, const char *)) \
	X(bapi_scene_manager_update, void (*)(bapi_scene_manager_t, float)) \
	X(bapi_scene_manager_render, void (*)(bapi_scene_manager_t)) \
	X(bapi_level_create, bapi_level_t (*)(const char *, int, bapi_level_callbacks_t)) \
	X(bapi_level_destroy, void (*)(bapi_level_t)) \
	X(bapi_level_get_name, const char *(*)(bapi_level_t)) \
	X(bapi_level_get_index, int (*)(bapi_level_t)) \
	X(bapi_level_get_user_data, void *(*)(bapi_level_t)) \
	X(bapi_level_set_user_data, void (*)(bapi_level_t, void *)) \
	X(bapi_level_manager_create, bapi_level_manager_t (*)(void)) \
	X(bapi_level_manager_destroy, void (*)(bapi_level_manager_t)) \
	X(bapi_level_manager_add_level, int (*)(bapi_level_manager_t, bapi_level_t)) \
	X(bapi_level_manager_load_level, int (*)(bapi_level_manager_t, const char *)) \
	X(bapi_level_manager_load_level_by_index, int (*)(bapi_level_manager_t, int)) \
	X(bapi_level_manager_next_level, int (*)(bapi_level_manager_t)) \
	X(bapi_level_manager_previous_level, int (*)(bapi_level_manager_t)) \
	X(bapi_level_manager_get_current_level, bapi_level_t (*)(bapi_level_manager_t)) \
	X(bapi_level_manager_get_level, bapi_level_t (*)(bapi_level_manager_t, const char *)) \
	X(bapi_level_manager_get_level_by_index, bapi_level_t (*)(bapi_level_manager_t, int)) \
	X(bapi_level_manager_get_level_count, int (*)(bapi_level_manager_t)) \
	X(bapi_level_manager_update, void (*)(bapi_level_manager_t, float)) \
	X(bapi_level_manager_render, void (*)(bapi_level_manager_t)) \
	X(bapi_scene_manager_load_from_xml, bapi_scene_manager_t (*)(const char *)) \
	X(bapi_level_manager_load_from_xml, bapi_level_manager_t (*)(const char *)) \
	X(bapi_scene_manager_save_to_xml, int (*)(bapi_scene_manager_t, const char *)) \
	X(bapi_level_manager_save_to_xml, int (*)(bapi_level_manager_t, const char *)) \
	X(bapi_file_read_alloc, uint8_t *(*)(const char *, size_t *)) \
	X(bapi_pack_stream_open, bapi_pack_stream_t (*)(bapi_pack_t, const char *)) \
	X(bapi_pack_stream_read, size_t (*)(bapi_pack_stream_t, void *, size_t)) \
	X(bapi_pack_stream_seek, int64_t (*)(bapi_pack_stream_t, int64_t, int)) \
	X(bapi_pack_stream_tell, int64_t (*)(bapi_pack_stream_t)) \
	X(bapi_pack_stream_size, int64_t (*)(bapi_pack_stream_t)) \
	X(bapi_pack_stream_close, void (*)(bapi_pack_stream_t)) \
	X(bapi_audio_set_master_volume, void (*)(float)) \
	X(bapi_audio_get_master_volume, float (*)(void)) \
	X(bapi_audio_stop_all, void (*)(void)) \
	X(bapi_audio_get_active_sound_count, int (*)(void)) \
	X(bapi_music_load, bapi_music_t (*)(const char *)) \
	X(bapi_music_load_from_memory, bapi_music_t (*)(const void *, size_t)) \
	X(bapi_music_free, void (*)(bapi_music_t)) \
	X(bapi_music_play, int (*)(bapi_music_t)) \
	X(bapi_music_pause, void (*)(bapi_music_t)) \
	X(bapi_music_resume, void (*)(bapi_music_t)) \
	X(bapi_music_stop, void (*)(bapi_music_t)) \
	X(bapi_music_set_volume, void (*)(bapi_music_t, float)) \
	X(bapi_music_set_loop, void (*)(bapi_music_t, int)) \
	X(bapi_music_is_playing, int (*)(bapi_music_t)) \
	X(bapi_music_get_duration, double (*)(bapi_music_t)) \
	X(bapi_music_update, void (*)(void)) \
	X(bapi_sound_load_from_memory, bapi_sound_t (*)(const void *, size_t)) \
	X(bapi_sound_load_from_pack, bapi_sound_t (*)(bapi_pack_t, const char *)) \
	X(bapi_texture_load_from_memory, bapi_texture_t (*)(const void *, size_t)) \
	X(bapi_texture_load_from_pack, bapi_texture_t (*)(bapi_pack_t, const char *)) \
	X(bapi_video_load_from_memory, bapi_video_t (*)(const void *, size_t)) \
	X(bapi_video_load_from_pack, bapi_video_t (*)(bapi_pack_t, const char *)) \
	X(bapi_video_load_from_pack_stream, bapi_video_t (*)(bapi_pack_t, const char *)) \
	X(bapi_event_get_window_width, int (*)(const bapi_event_t *)) \
	X(bapi_event_get_window_height, int (*)(const bapi_event_t *)) \
	X(bapi_fill_triangle, void (*)(float, float, float, float, float, float, bapi_color_t)) \
	X(bapi_frame_begin, float (*)(void)) \
	X(bapi_frame_end, void (*)(void)) \
	X(bapi_get_delta_time, float (*)(void)) \
	X(bapi_get_fps, float (*)(void)) \
	X(bapi_get_elapsed_time, double (*)(void)) \
	X(bapi_get_frame_count, uint64_t (*)(void)) \
	X(bapi_set_target_fps, void (*)(int)) \
	X(bapi_set_time_scale, void (*)(float)) \
	X(bapi_set_frame_paused, void (*)(int)) \
	X(bapi_frame_is_paused, int (*)(void)) \
	X(bapi_frame_steps, int (*)(float, int)) \
	X(bapi_frame_step_alpha, float (*)(void)) \
	X(bapi_set_vsync, void (*)(int)) \
	X(bapi_get_vsync, int (*)(void)) \
	X(bapi_set_fullscreen, void (*)(int)) \
	X(bapi_get_fullscreen, int (*)(void)) \
	X(bapi_set_logical_size, void (*)(int, int)) \
	X(bapi_get_logical_size, void (*)(int *, int *)) \
	X(bapi_get_window_size, void (*)(int *, int *)) \
	X(bapi_reset_transform, void (*)(void)) \
	X(bapi_push_transform, void (*)(bapi_vec2_t, float, bapi_vec2_t)) \
	X(bapi_pop_transform, void (*)(void)) \
	X(bapi_apply_camera, void (*)(const bapi_camera_t *)) \
	X(bapi_transform_point, bapi_vec2_t (*)(bapi_vec2_t)) \
	X(bapi_transform_vector, bapi_vec2_t (*)(bapi_vec2_t)) \
	X(bapi_transform_scale_factor, float (*)(void)) \
	X(bapi_get_last_error, const char *(*)(void)) \
	X(bapi_clear_last_error, void (*)(void)) \
	X(bapi_texture_draw_default, bapi_texture_draw_t (*)(bapi_rect_t)) \
	X(bapi_texture_draw, void (*)(bapi_texture_t, const bapi_texture_draw_t *)) \
	X(bapi_sprite_batch_begin, void (*)(void)) \
	X(bapi_sprite_batch_draw, void (*)(bapi_texture_t, const bapi_texture_draw_t *)) \
	X(bapi_sprite_batch_end, void (*)(void)) \
	X(bapi_atlas_load, bapi_atlas_t (*)(const char *)) \
	X(bapi_atlas_load_with_texture, bapi_atlas_t (*)(const char *, bapi_texture_t)) \
	X(bapi_atlas_load_from_memory, bapi_atlas_t (*)(const void *, size_t, bapi_texture_t)) \
	X(bapi_atlas_destroy, void (*)(bapi_atlas_t)) \
	X(bapi_atlas_get_texture, bapi_texture_t (*)(bapi_atlas_t)) \
	X(bapi_atlas_get_region_count, int (*)(bapi_atlas_t)) \
	X(bapi_atlas_get_region, const bapi_atlas_region_t *(*)(bapi_atlas_t, int)) \
	X(bapi_atlas_find_region, const bapi_atlas_region_t *(*)(bapi_atlas_t, const char *)) \
	X(bapi_atlas_draw, void (*)(bapi_atlas_t, const char *, bapi_rect_t, int, bapi_color_t)) \
	X(bapi_entity_create, bapi_entity_t (*)(const char *)) \
	X(bapi_entity_destroy, void (*)(bapi_entity_t)) \
	X(bapi_entity_get_name, const char *(*)(bapi_entity_t)) \
	X(bapi_entity_get_user_data, void *(*)(bapi_entity_t)) \
	X(bapi_entity_set_user_data, void (*)(bapi_entity_t, void *)) \
	X(bapi_entity_set_position, void (*)(bapi_entity_t, bapi_vec2_t)) \
	X(bapi_entity_get_position, bapi_vec2_t (*)(bapi_entity_t)) \
	X(bapi_entity_set_rotation, void (*)(bapi_entity_t, float)) \
	X(bapi_entity_get_rotation, float (*)(bapi_entity_t)) \
	X(bapi_entity_set_scale, void (*)(bapi_entity_t, bapi_vec2_t)) \
	X(bapi_entity_get_scale, bapi_vec2_t (*)(bapi_entity_t)) \
	X(bapi_entity_get_world_position, bapi_vec2_t (*)(bapi_entity_t)) \
	X(bapi_entity_get_world_rotation, float (*)(bapi_entity_t)) \
	X(bapi_entity_get_world_scale, bapi_vec2_t (*)(bapi_entity_t)) \
	X(bapi_entity_add_child, int (*)(bapi_entity_t, bapi_entity_t)) \
	X(bapi_entity_remove_child, int (*)(bapi_entity_t, bapi_entity_t)) \
	X(bapi_entity_get_parent, bapi_entity_t (*)(bapi_entity_t)) \
	X(bapi_entity_get_child_count, int (*)(bapi_entity_t)) \
	X(bapi_entity_get_child, bapi_entity_t (*)(bapi_entity_t, int)) \
	X(bapi_entity_find_child, bapi_entity_t (*)(bapi_entity_t, const char *)) \
	X(bapi_entity_set_visible, void (*)(bapi_entity_t, int)) \
	X(bapi_entity_is_visible, int (*)(bapi_entity_t)) \
	X(bapi_entity_set_texture, void (*)(bapi_entity_t, bapi_texture_t, bapi_rect_t)) \
	X(bapi_entity_set_tint, void (*)(bapi_entity_t, bapi_color_t)) \
	X(bapi_entity_set_update, void (*)(bapi_entity_t, bapi_entity_update_fn)) \
	X(bapi_entity_update, void (*)(bapi_entity_t, float)) \
	X(bapi_entity_draw, void (*)(bapi_entity_t))
