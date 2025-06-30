
#define m_cpu_side 1

#pragma comment(lib, "opengl32.lib")

#pragma warning(push, 0)
// #define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "SDL_mixer.h"
#pragma warning(pop)

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>
#endif // __EMSCRIPTEN__

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef near
#undef far
#endif // _WIN32

#include <stdlib.h>

#include <gl/GL.h>
#if !defined(__EMSCRIPTEN__)
#include "external/glcorearb.h"
#endif

#if defined(__EMSCRIPTEN__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT
#include "external/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert
#include "external/stb_truetype.h"
#pragma warning(pop)

#if defined(__EMSCRIPTEN__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#if defined(__EMSCRIPTEN__)
#define m_gl_single_channel GL_LUMINANCE
#else // __EMSCRIPTEN__
#define m_gl_single_channel GL_RED
#endif // __EMSCRIPTEN__


#if defined(m_debug)
#define gl(...) __VA_ARGS__; {int error = glGetError(); if(error != 0) { on_gl_error(#__VA_ARGS__, __FILE__, __LINE__, error); }}
#else // m_debug
#define gl(...) __VA_ARGS__
#endif // m_debug

#include "tklib.h"
#include "shared.h"

#include "config.h"
#include "leaderboard.h"
#include "engine.h"
#include "game.h"
#include "shader_shared.h"


#if defined(__EMSCRIPTEN__)
global constexpr b8 c_on_web = true;
#else
global constexpr b8 c_on_web = false;
#endif



global s_platform_data* g_platform_data;
global s_game* game;
global s_v2 g_mouse;
global b8 g_click;

#if defined(__EMSCRIPTEN__)
#include "leaderboard.cpp"
#endif

#include "engine.cpp"

m_dll_export void init(s_platform_data* platform_data)
{
	g_platform_data = platform_data;
	game = (s_game*)platform_data->memory;
	game->speed_index = 5;
	game->rng = make_rng(1234);
	game->reload_shaders = true;
	game->speed = 1;

	SDL_StartTextInput();

	set_state(&game->state0, e_game_state0_main_menu);
	game->do_hard_reset = true;
	set_state(&game->hard_data.state1, e_game_state1_default);

	u8* cursor = platform_data->memory + sizeof(s_game);

	{
		game->update_frame_arena = make_arena_from_memory(cursor, 10 * c_mb);
		cursor += 10 * c_mb;
	}
	{
		game->render_frame_arena = make_arena_from_memory(cursor, 500 * c_mb);
		cursor += 10 * c_mb;
	}
	{
		game->circular_arena = make_circular_arena_from_memory(cursor, 10 * c_mb);
		cursor += 10 * c_mb;
	}

	platform_data->cycle_frequency = SDL_GetPerformanceFrequency();
	platform_data->start_cycles = SDL_GetPerformanceCounter();

	platform_data->window_size.x = (int)c_world_size.x;
	platform_data->window_size.y = (int)c_world_size.y;

	g_platform_data->window = SDL_CreateWindow(
		"Loopscape", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		(int)c_world_size.x, (int)c_world_size.y, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
	);
	SDL_SetWindowPosition(g_platform_data->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(g_platform_data->window);

	#if defined(__EMSCRIPTEN__)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	#endif

	g_platform_data->gl_context = SDL_GL_CreateContext(g_platform_data->window);
	SDL_GL_SetSwapInterval(1);

	#define X(type, name) name = (type)SDL_GL_GetProcAddress(#name); assert(name);
		m_gl_funcs
	#undef X

	{
		gl(glGenBuffers(1, &game->ubo));
		gl(glBindBuffer(GL_UNIFORM_BUFFER, game->ubo));
		gl(glBufferData(GL_UNIFORM_BUFFER, sizeof(s_uniform_data), NULL, GL_DYNAMIC_DRAW));
		gl(glBindBufferBase(GL_UNIFORM_BUFFER, 0, game->ubo));
	}

	{
		constexpr float c_size = 0.5f;
		s_vertex vertex_arr[] = {
			{v3(-c_size, -c_size, 0), v3(0, -1, 0), make_color(1), v2(0, 1)},
			{v3(c_size, -c_size, 0), v3(0, -1, 0), make_color(1), v2(1, 1)},
			{v3(c_size, c_size, 0), v3(0, -1, 0), make_color(1), v2(1, 0)},
			{v3(-c_size, -c_size, 0), v3(0, -1, 0), make_color(1), v2(0, 1)},
			{v3(c_size, c_size, 0), v3(0, -1, 0), make_color(1), v2(1, 0)},
			{v3(-c_size, c_size, 0), v3(0, -1, 0), make_color(1), v2(0, 0)},
		};
		game->mesh_arr[e_mesh_quad] = make_mesh_from_vertices(vertex_arr, array_count(vertex_arr));
	}

	{
		game->mesh_arr[e_mesh_cube] = make_mesh_from_obj_file("assets/cube.obj", &game->render_frame_arena);
		game->mesh_arr[e_mesh_sphere] = make_mesh_from_obj_file("assets/sphere.obj", &game->render_frame_arena);
	}

	{
		s_mesh* mesh = &game->mesh_arr[e_mesh_line];
		mesh->vertex_count = 6;
		gl(glGenVertexArrays(1, &mesh->vao));
		gl(glBindVertexArray(mesh->vao));

		gl(glGenBuffers(1, &mesh->instance_vbo.id));
		gl(glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_vbo.id));

		u8* offset = 0;
		constexpr int stride = sizeof(float) * 9;
		int attrib_index = 0;

		gl(glVertexAttribPointer(attrib_index, 2, GL_FLOAT, GL_FALSE, stride, offset)); // line start
		gl(glEnableVertexAttribArray(attrib_index));
		gl(glVertexAttribDivisor(attrib_index, 1));
		attrib_index += 1;
		offset += sizeof(float) * 2;

		gl(glVertexAttribPointer(attrib_index, 2, GL_FLOAT, GL_FALSE, stride, offset)); // line end
		gl(glEnableVertexAttribArray(attrib_index));
		gl(glVertexAttribDivisor(attrib_index, 1));
		attrib_index += 1;
		offset += sizeof(float) * 2;

		gl(glVertexAttribPointer(attrib_index, 1, GL_FLOAT, GL_FALSE, stride, offset)); // line width
		gl(glEnableVertexAttribArray(attrib_index));
		gl(glVertexAttribDivisor(attrib_index, 1));
		attrib_index += 1;
		offset += sizeof(float) * 1;

		gl(glVertexAttribPointer(attrib_index, 4, GL_FLOAT, GL_FALSE, stride, offset)); // instance color
		gl(glEnableVertexAttribArray(attrib_index));
		gl(glVertexAttribDivisor(attrib_index, 1));
		attrib_index += 1;
		offset += sizeof(float) * 4;
	}

	for(int i = 0; i < e_sound_count; i += 1) {
		game->sound_arr[i] = load_sound_from_file(c_sound_data_arr[i].path, c_sound_data_arr[i].volume);
	}

	Mix_Music* music = Mix_LoadMUS("assets/music.mp3");
	if(music) {
		Mix_VolumeMusic(8);
		Mix_PlayMusic(music, -1);
	}

	for(int i = 0; i < e_texture_count; i += 1) {
		char* path = c_texture_path_arr[i];
		if(strlen(path) > 0) {
			game->texture_arr[i] = load_texture_from_file(path, GL_NEAREST);
		}
	}

	game->font = load_font_from_file("assets/Inconsolata-Regular.ttf", 128, &game->render_frame_arena);

	{
		u32* texture = &game->texture_arr[e_texture_light].id;
		game->light_fbo.size.x = (int)c_world_size.x;
		game->light_fbo.size.y = (int)c_world_size.y;
		gl(glGenFramebuffers(1, &game->light_fbo.id));
		bind_framebuffer(game->light_fbo.id);
		gl(glGenTextures(1, texture));
		gl(glBindTexture(GL_TEXTURE_2D, *texture));
		gl(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, game->light_fbo.size.x, game->light_fbo.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
		gl(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0));
		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		bind_framebuffer(0);
	}

	load_map();

	#if defined(__EMSCRIPTEN__)
	load_or_create_leaderboard_id();
	#endif
}

m_dll_export void init_after_recompile(s_platform_data* platform_data)
{
	g_platform_data = platform_data;
	game = (s_game*)platform_data->memory;

	#define X(type, name) name = (type)SDL_GL_GetProcAddress(#name); assert(name);
		m_gl_funcs
	#undef X
}

#if defined(__EMSCRIPTEN__)
EM_JS(int, browser_get_width, (), {
	const { width, height } = canvas.getBoundingClientRect();
	return width;
});

EM_JS(int, browser_get_height, (), {
	const { width, height } = canvas.getBoundingClientRect();
	return height;
});
#endif // __EMSCRIPTEN__

m_dll_export void do_game(s_platform_data* platform_data)
{
	g_platform_data = platform_data;
	game = (s_game*)platform_data->memory;

	f64 delta64 = get_seconds() - game->time_before;
	game->time_before = get_seconds();

	{
		#if defined(__EMSCRIPTEN__)
		int width = browser_get_width();
		int height = browser_get_height();
		g_platform_data->window_size.x = width;
		g_platform_data->window_size.y = height;
		if(g_platform_data->prev_window_size.x != width || g_platform_data->prev_window_size.y != height) {
			set_window_size(width, height);
			g_platform_data->window_resized = true;
		}
		g_platform_data->prev_window_size.x = width;
		g_platform_data->prev_window_size.y = height;
		#endif // __EMSCRIPTEN__
	}

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		handle state start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	{
		b8 state_changed = handle_state(&game->state0, game->render_time);
		if(state_changed) {
			game->accumulator += c_update_delay;
		}
	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		handle state end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	input();
	float game_speed = c_game_speed_arr[game->speed_index] * game->speed;
	game->accumulator += delta64 * game_speed;
	#if defined(__EMSCRIPTEN__)
	game->accumulator = at_most(game->accumulator, 1.0);
	#else
	#endif
	while(game->accumulator >= c_update_delay) {
		game->accumulator -= c_update_delay;
		update();
	}
	float interp_dt = (float)(game->accumulator / c_update_delay);
	render(interp_dt, (float)delta64);
}

func void input()
{

	game->char_events.count = 0;
	game->key_events.count = 0;

	u8* keyboard_state = (u8*)SDL_GetKeyboardState(null);

	if(game->input.handled) {
		game->input = zero;
	}
	game->input.left = game->input.left || keyboard_state[SDL_SCANCODE_A];
	game->input.right = game->input.right || keyboard_state[SDL_SCANCODE_D];


	for(int i = 0; i < c_max_keys; i += 1) {
		game->input_arr[i].half_transition_count = 0;
	}

	g_click = false;
	{
		int x;
		int y;
		SDL_GetMouseState(&x, &y);
		g_mouse = v2(x, y);
		s_rect letterbox = do_letterbox(v2(g_platform_data->window_size), c_world_size);
		g_mouse.x = range_lerp(g_mouse.x, letterbox.x, letterbox.x + letterbox.size.x, 0, c_world_size.x);
		g_mouse.y = range_lerp(g_mouse.y, letterbox.y, letterbox.y + letterbox.size.y, 0, c_world_size.y);
	}

	s_hard_game_data* hard_data = &game->hard_data;
	s_soft_game_data* soft_data = &hard_data->soft_data;

	e_game_state0 state0 = (e_game_state0)get_state(&game->state0);
	e_game_state1 state1 = (e_game_state1)get_state(&hard_data->state1);

	s_player* player = &soft_data->player;

	SDL_Event event;
	while(SDL_PollEvent(&event) != 0) {
		switch(event.type) {
			case SDL_QUIT: {
				g_platform_data->quit = true;
			} break;

			case SDL_WINDOWEVENT: {
				if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					int width = event.window.data1;
					int height = event.window.data2;
					g_platform_data->window_size.x = width;
					g_platform_data->window_size.y = height;
					g_platform_data->window_resized = true;
				}
			} break;

			case SDL_KEYDOWN:
			case SDL_KEYUP: {
				int key = event.key.keysym.sym;
				if(key == SDLK_LSHIFT) {
					key = c_left_shift;
				}
				b8 is_down = event.type == SDL_KEYDOWN;
				handle_key_event(key, is_down, event.key.repeat != 0);

				if(key < c_max_keys) {
					game->input_arr[key].is_down = is_down;
					game->input_arr[key].half_transition_count += 1;
				}
				if(event.type == SDL_KEYDOWN) {
					if(key == SDLK_r && event.key.repeat == 0) {
						if(event.key.keysym.mod & KMOD_LCTRL) {
							game->do_hard_reset = true;
						}
						else {
							start_restart(player->pos);
						}
					}
					else if(key == SDLK_SPACE && event.key.repeat == 0) {
						soft_data->want_to_jump_timestamp = game->update_time;
					}
					else if(key == SDLK_f && event.key.repeat == 0) {
						soft_data->want_to_teleport_timestamp = game->update_time;
					}
					else if(key == SDLK_ESCAPE && event.key.repeat == 0) {
						if(state0 == e_game_state0_play && state1 == e_game_state1_default) {
							add_state(&game->state0, e_game_state0_pause);
						}
						else if(state0 == e_game_state0_pause) {
							pop_state_transition(&game->state0, game->render_time, c_transition_time);
						}
					}
					#if defined(m_debug)
					else if(key == SDLK_s && event.key.repeat == 0 && event.key.keysym.mod & KMOD_LCTRL) {
						if(state0 == e_game_state0_play && state1 == e_game_state1_editor) {
							save_map("map.map");
							save_map("map_backup.map");
						}
					}
					else if(key == SDLK_l && event.key.repeat == 0 && event.key.keysym.mod & KMOD_LCTRL) {
						if(state0 == e_game_state0_play && state1 == e_game_state1_editor) {
							load_map();
						}
					}
					else if(key == SDLK_KP_PLUS) {
						game->speed_index = circular_index(game->speed_index + 1, array_count(c_game_speed_arr));
						printf("Game speed: %f\n", c_game_speed_arr[game->speed_index]);
					}
					else if(key == SDLK_KP_MINUS) {
						game->speed_index = circular_index(game->speed_index - 1, array_count(c_game_speed_arr));
						printf("Game speed: %f\n", c_game_speed_arr[game->speed_index]);
					}
					else if(key == SDLK_F1) {
						if(state1 == e_game_state1_default) {
							add_state(&hard_data->state1, e_game_state1_editor);
							game->editor.cam_pos = (soft_data->player.pos - c_world_center);
							game->editor.zoom = 1;
						}
						else if(state1 == e_game_state1_editor) {
							pop_state(&hard_data->state1);
						}
					}
					else if(key == SDLK_g && event.key.repeat == 0) {
						if(state0 == e_game_state0_play) {
							s_v2 target_pos = zero;
							b8 do_teleport = false;
							if(state1 == e_game_state1_default) {
								s_m4 view = get_player_view_matrix(player->pos);
								s_m4 inverse_view = m4_inverse(view);
								s_v2 temp_mouse = v2_multiply_m4(g_mouse, m4_inverse(view));
								s_v2i index = v2i(
									floorfi(temp_mouse.x / c_tile_size),
									floorfi(temp_mouse.y / c_tile_size)
								);
								target_pos = c_tile_size_v * index;
								do_teleport = true;
							}
							else if(state1 == e_game_state1_editor) {

								s_m4 view = get_editor_view_matrix();
								s_m4 inverse_view = m4_inverse(view);
								s_v2 temp_mouse = v2_multiply_m4(g_mouse, m4_inverse(view));
								s_v2i index = v2i(
									floorfi(temp_mouse.x / c_tile_size),
									floorfi(temp_mouse.y / c_tile_size)
								);
								target_pos = c_tile_size_v * index + c_player_size_v * 0.5f;

								do_teleport = true;
							}
							if(do_teleport) {
								game->last_debug_teleport_pos = target_pos;
								player->pos = target_pos;
								player->prev_pos = player->pos;
								player->vel.y = 0;
							}
						}
					}
					else if(key == SDLK_j && event.key.repeat == 0) {
						player->pos = game->last_debug_teleport_pos;
						player->prev_pos = player->pos;
						player->vel.y = 0;
					}
					else if(key == SDLK_h && event.key.repeat == 0) {
						if(state0 == e_game_state0_play && state1 == e_game_state1_default) {
							game->freeze_loop = !game->freeze_loop;
						}
					}
					#endif // m_debug

					for(int i = 1; i < e_tile_type_count; i += 1) {
						int check_key = SDLK_1 + (i - 1);
						if(i == e_tile_type_count - 1) {
							check_key = SDLK_0;
						}
						if(key == check_key && event.key.repeat == 0) {
							if(state0 == e_game_state0_play && state1 == e_game_state1_editor) {
								game->editor.curr_tile = (e_tile_type)i;
							}
						}
					}
				}

				// @Note(tkap, 28/06/2025): Key up
				else {
					if(key == SDLK_SPACE) {
						soft_data->stop_jump_timestamp = game->update_time;
					}
				}
			} break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == 1) {
					g_click = true;
					soft_data->want_to_teleport_timestamp = game->update_time;
				}
				if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == 3) {
					soft_data->want_to_super_speed_timestamp = game->update_time;
				}
				// int key = sdl_key_to_windows_key(event.button.button);
				// b8 is_down = event.type == SDL_MOUSEBUTTONDOWN;
				// handle_key_event(key, is_down, false);

				if(event.button.button == 1) {
					int key = c_left_button;
					game->input_arr[key].is_down = event.type == SDL_MOUSEBUTTONDOWN;
					game->input_arr[key].half_transition_count += 1;
				}

				else if(event.button.button == 3) {
					int key = c_right_button;
					game->input_arr[key].is_down = event.type == SDL_MOUSEBUTTONDOWN;
					game->input_arr[key].half_transition_count += 1;
				}

			} break;

			case SDL_TEXTINPUT: {
				char c = event.text.text[0];
				game->char_events.add((char)c);
			} break;

			case SDL_MOUSEWHEEL: {
				// float movement = (float)event.wheel.y;
				// g_platform_data->input.wheel_movement = movement / 120;
				if(state0 == e_game_state0_play && state1 == e_game_state1_editor) {
					if(is_key_down(c_left_shift)) {
						int movement = event.wheel.y > 0 ? -1 : 1;
						game->editor.curr_tile = (e_tile_type)circular_index(game->editor.curr_tile + movement, e_tile_type_count);
						if(game->editor.curr_tile == e_tile_type_none) {
							game->editor.curr_tile = (e_tile_type)circular_index(game->editor.curr_tile + movement, e_tile_type_count);
						}
					}
					else {
						float movement = event.wheel.y > 0 ? 1.1f : 0.9f;
						game->editor.zoom *= movement;
					}
				}
			} break;
		}
	}
}

func void update()
{
	game->update_frame_arena.used = 0;

	e_game_state0 state0 = (e_game_state0)get_state(&game->state0);

	s_hard_game_data* hard_data = &game->hard_data;
	s_soft_game_data* soft_data = &hard_data->soft_data;

	switch(state0) {
		case e_game_state0_play: {
		} break;

		default: {}
	}

	if(game->do_hard_reset) {
		game->do_hard_reset = false;
		memset(hard_data, 0, sizeof(*hard_data));

		game->editor = zero;
		game->editor.zoom = 1;
		game->editor.curr_tile = e_tile_type_block;

		game->update_time = 0;

		set_state(&hard_data->state1, e_game_state1_default);

		game->do_soft_reset = true;

	}

	if(game->do_soft_reset) {
		game->do_soft_reset = false;

		memset(soft_data, 0, sizeof(*soft_data));

		soft_data->run_start_timestamp = game->update_time;

		entity_manager_reset(&soft_data->emitter_a_arr);

		{
			s_player* player = &soft_data->player;
			player->pos = get_player_spawn_pos();
			player->prev_pos = player->pos;
			player->last_x_dir = 1;
		}

	}

	s_player* player = &soft_data->player;
	player->prev_pos = player->pos;

	e_game_state1 state1 = (e_game_state1)get_state(&hard_data->state1);

	b8 restarting = soft_data->start_restart_timestamp > 0;
	b8 do_player_input = state0 == e_game_state0_play && state1 == e_game_state1_default;
	do_player_input = do_player_input && !restarting;

	if(do_player_input) {

		if(
			check_action(game->update_time, soft_data->want_to_super_speed_timestamp, 0.0f) && soft_data->super_speed_timestamp == 0 &&
			has_upgrade(e_upgrade_super_speed)
		) {
			do_screen_shake(50);
			play_sound(e_sound_super_speed);
			soft_data->super_speed_timestamp = game->update_time;
		}

		float speed = get_player_movement_speed();
		if(!check_action(game->update_time, player->on_ground_timestamp, (float)c_update_delay * 2)) {
			speed *= 1.25f;
		}
		if(are_we_in_super_speed()) {
			speed *= c_super_speed_multiplier;
		}
		s_v2 movement = zero;
		if(game->input.left) {
			movement.x -= speed;
		}

		if(game->input.right) {
			movement.x += speed;
		}
		if(movement.x == 0) {
			player->start_run_timestamp = 0;
		}
		else if(player->start_run_timestamp == 0) {
			player->start_run_timestamp = game->update_time;
		}
		if(movement.x > 0) {
			player->last_x_dir = 1;
		}
		else if(movement.x < 0) {
			player->last_x_dir = -1;
		}

		player->vel.y += c_gravity;
		if(player->vel.y >= c_max_gravity) {
			player->vel.y = c_max_gravity;
		}

		int max_player_jumps = get_max_player_jumps();
		for(int jump_i = player->jumps_done; jump_i < max_player_jumps; jump_i += 1) {
			if(
				check_action(game->update_time, soft_data->want_to_jump_timestamp, 0.1f) &&
				(check_action(game->update_time, player->on_ground_timestamp, 0.15f) || jump_i > 0)
			) {
				soft_data->want_to_jump_timestamp = 0;
				float strength = c_jump_strength;
				if(jump_i > 0) {
					strength *= 0.9f;
				}
				player->vel.y = -strength;
				player->jumps_done = jump_i + 1;
				player->jumping = true;
				if(jump_i == 0) {
					play_sound(e_sound_jump1);
				}
				else {
					play_sound(e_sound_jump2);
				}
				player->jump_timestamp = game->update_time;
				player->jump_x_dir = movement.x;
				break;
			}
		}

		if(check_action(game->update_time, soft_data->stop_jump_timestamp, 0.0f) && player->jumping) {
			soft_data->stop_jump_timestamp = 0;
			player->vel.y *= 0.5f;
		}

		movement += player->vel;

		if(player->vel.y > 0) {
			player->jumping = false;
		}

		player->last_x_vel = movement.x;

		do_player_move(0, movement.x, player);
		do_player_move(1, movement.y, player);

		{
			s_v2 teleport_pos = player->pos;
			teleport_pos.x += c_teleport_distance * player->last_x_dir;;
			if(
				check_action(game->update_time, soft_data->want_to_teleport_timestamp, 0.0f) &&
				has_upgrade(e_upgrade_teleport) && can_we_teleport_without_getting_stuck(teleport_pos) && !check_action(game->update_time, soft_data->teleport_timestamp, c_teleport_cooldown)
			) {
				soft_data->teleport_timestamp = game->update_time;
				player->pos = teleport_pos;
				player->prev_pos = player->pos;
				play_sound(e_sound_teleport);

				{
					s_particle_emitter_a a = zero;
					a.pos.xy = player->pos;
					a.particle_duration = 0.5f;
					a.radius = 10;
					a.shrink = 0.5f;
					a.dir = v3(1, 1, 0);
					a.dir_rand = v3(1, 1, 0);
					a.speed = 200;
					a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0x497A85), 1.0f), .percent = 0});
					a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0x3E244B), 1.0f), .percent = 0.2f});
					s_particle_emitter_b b = zero;
					b.duration = 0;
					b.particle_count = 200;
					b.spawn_type = e_emitter_spawn_type_circle;
					b.spawn_data.x = c_player_size_v.y * 0.5f;
					add_emitter(a, b);
				}
			}
		}
	}

	if(!soft_data->curr_ghost.pos_arr.is_full()) {
		soft_data->curr_ghost.pos_arr.add(player->pos);
	}

	game->update_time += (float)c_update_delay;
	hard_data->update_count += 1;
	soft_data->update_count += 1;

	game->input.handled = true;
}

func void render(float interp_dt, float delta)
{
	game->render_frame_arena.used = 0;

	if(!game->music_volume_clean) {
		game->music_volume_clean = true;
		if(game->disable_music) {
			Mix_VolumeMusic(0);
		}
		else {
			Mix_VolumeMusic(8);
		}
	}

	#if defined(_WIN32)
	while(g_platform_data->hot_read_index[1] < g_platform_data->hot_write_index) {
		char* path = g_platform_data->hot_file_arr[g_platform_data->hot_read_index[1] % c_max_hot_files];
		if(strstr(path, ".shader")) {
			game->reload_shaders = true;
		}
		g_platform_data->hot_read_index[1] += 1;
	}
	#endif // _WIN32

	if(game->reload_shaders) {
		game->reload_shaders = false;

		for(int i = 0; i < e_shader_count; i += 1) {
			s_shader shader = load_shader_from_file(c_shader_path_arr[i], &game->render_frame_arena);
			if(shader.id > 0) {
				if(game->shader_arr[i].id > 0) {
					gl(glDeleteProgram(game->shader_arr[i].id));
				}
				game->shader_arr[i] = shader;

				#if defined(m_debug)
				printf("Loaded %s\n", c_shader_path_arr[i]);
				#endif // m_debug
			}
		}
	}

	for(int i = 0; i < e_shader_count; i += 1) {
		for(int j = 0; j < e_texture_count; j += 1) {
			for(int k = 0; k < e_mesh_count; k += 1) {
				game->render_group_index_arr[i][j][k] = -1;
			}
		}
	}
	game->render_group_arr.count = 0;
	memset(game->render_instance_count, 0, sizeof(game->render_instance_count));

	s_hard_game_data* hard_data = &game->hard_data;
	s_soft_game_data* soft_data = &hard_data->soft_data;

	s_m4 light_projection = make_orthographic(-50, 50, -50, 50, -50, 50);
	s_v3 sun_pos = v3(0, -10, 10);
	s_v3 sun_dir = v3_normalized(v3(1, 1, -1));
	s_m4 ortho = make_orthographic(0, c_world_size.x, c_world_size.y, 0, -1, 1);
	s_m4 perspective = make_perspective(60.0f, c_world_size.x / c_world_size.y, 0.01f, 100.0f);

	game->speed = 1;

	bind_framebuffer(0);
	clear_framebuffer_depth(0);
	clear_framebuffer_color(0, v4(0.0f, 0, 0, 0));

	e_game_state0 state0 = (e_game_state0)get_state(&game->state0);


	switch(state0) {

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		main menu start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		case e_game_state0_main_menu: {
			game->speed = 0;

			draw_background(zero, ortho);

			if(do_button(S("Play"), wxy(0.5f, 0.5f), true) || is_key_pressed(SDLK_RETURN, true)) {
				add_state_transition(&game->state0, e_game_state0_play, game->render_time, c_transition_time);
				game->do_hard_reset = true;
			}

			if(do_button(S("Leaderboard"), wxy(0.5f, 0.6f), true)) {
				#if defined(__EMSCRIPTEN__)
				get_leaderboard(c_leaderboard_id);
				#endif
				add_state_transition(&game->state0, e_game_state0_leaderboard, game->render_time, c_transition_time);
			}

			if(do_button(S("Options"), wxy(0.5f, 0.7f), true)) {
				add_state_transition(&game->state0, e_game_state0_options, game->render_time, c_transition_time);
			}

			draw_text(c_game_name, wxy(0.5f, 0.2f), 128, make_color(1), true, &game->font);
			draw_text(S("www.twitch.tv/Tkap1"), wxy(0.5f, 0.3f), 32, make_color(0.6f), true, &game->font);

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}

		} break;
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		main menu end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		pause menu start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		case e_game_state0_pause: {
			game->speed = 0;

			draw_background(zero, ortho);

			if(do_button(S("Resume"), wxy(0.5f, 0.5f), true) || is_key_pressed(SDLK_RETURN, true)) {
				pop_state_transition(&game->state0, game->render_time, c_transition_time);
			}

			if(do_button(S("Leaderboard"), wxy(0.5f, 0.6f), true)) {
				#if defined(__EMSCRIPTEN__)
				get_leaderboard(c_leaderboard_id);
				#endif
				add_state_transition(&game->state0, e_game_state0_leaderboard, game->render_time, c_transition_time);
			}

			if(do_button(S("Options"), wxy(0.5f, 0.7f), true)) {
				add_state_transition(&game->state0, e_game_state0_options, game->render_time, c_transition_time);
			}

			draw_text(c_game_name, wxy(0.5f, 0.2f), 128, make_color(1), true, &game->font);
			draw_text(S("www.twitch.tv/Tkap1"), wxy(0.5f, 0.3f), 32, make_color(0.6f), true, &game->font);

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}

		} break;
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		pause menu end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		leaderboard start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		case e_game_state0_leaderboard: {
			game->speed = 0;
			draw_background(zero, ortho);
			do_leaderboard();

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}

		} break;

		case e_game_state0_win_leaderboard: {
			game->speed = 0;
			draw_background(zero, ortho);
			do_leaderboard();

			{
				s_time_format data = update_count_to_time_format(game->update_count_at_win_time);
				s_len_str text = format_text("%02i:%02i.%i", data.minutes, data.seconds, data.milliseconds);
				draw_text(text, c_world_center * v2(1.0f, 0.2f), 64, make_color(1), true, &game->font);

				draw_text(S("Press R to restart..."), c_world_center * v2(1.0f, 0.4f), sin_range(48, 60, game->render_time * 8.0f), make_color(0.66f), true, &game->font);
			}

			b8 want_to_reset = is_key_pressed(SDLK_r, true);
			if(
				do_button(S("Restart"), c_world_size * v2(0.87f, 0.82f), true)
				|| is_key_pressed(SDLK_ESCAPE, true) || want_to_reset
			) {
				pop_state_transition(&game->state0, game->render_time, c_transition_time);
				game->do_hard_reset = true;
			}

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}

		} break;
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		leaderboard end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		options start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		case e_game_state0_options: {
			game->speed = 0;
			draw_background(zero, ortho);

			s_v2 pos = wxy(0.5f, 0.2f);
			s_v2 button_size = v2(600, 48);

			{
				s_len_str text = format_text("Sound: %s", game->turn_off_all_sounds ? "Off" : "On");
				do_bool_button_ex(text, pos, button_size, true, &game->turn_off_all_sounds);
				pos.y += 80;
			}

			{
				s_len_str text = format_text("Music: %s", game->disable_music ? "Off" : "On");
				b8 result = do_bool_button_ex(text, pos, button_size, true, &game->disable_music);
				if(result) {
					game->music_volume_clean = false;
				}
				pos.y += 80;
			}

			{
				s_len_str text = format_text("Replay ghosts: %s", game->hide_ghosts ? "Off" : "On");
				do_bool_button_ex(text, pos, button_size, true, &game->hide_ghosts);
				pos.y += 80;
			}

			{
				s_len_str text = format_text("Background: %s", game->hide_background ? "Off" : "On");
				do_bool_button_ex(text, pos, button_size, true, &game->hide_background);
				pos.y += 80;
			}

			{
				s_len_str text = format_text("Screen shake: %s", game->disable_screen_shake ? "Off" : "On");
				do_bool_button_ex(text, pos, button_size, true, &game->disable_screen_shake);
				pos.y += 80;
			}

			{
				s_len_str text = format_text("Show timer: %s", game->hide_timer ? "Off" : "On");
				do_bool_button_ex(text, pos, button_size, true, &game->hide_timer);
				pos.y += 80;
			}

			{
				s_len_str text = format_text("Dim player when out of jumps: %s", game->dim_player_when_out_of_jumps ? "On" : "Off");
				do_bool_button_ex(text, pos, button_size, true, &game->dim_player_when_out_of_jumps);
				pos.y += 80;
			}

			b8 escape = is_key_pressed(SDLK_ESCAPE, true);
			if(do_button(S("Back"), wxy(0.87f, 0.92f), true) || escape) {
				pop_state_transition(&game->state0, game->render_time, c_transition_time);
			}

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}
		} break;
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		options end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		play start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		case e_game_state0_play: {
			game->speed = 1;

			handle_state(&hard_data->state1, game->render_time);
			s_player* player = &soft_data->player;

			{
				b8 restarting = soft_data->start_restart_timestamp > 0;
				if(restarting) {
					float time = update_time_to_render_time(game->update_time, interp_dt);
					s_time_data data = get_time_data(time, soft_data->start_restart_timestamp, 0.4f);
					data.percent = at_most(1.0f, data.percent);
					player->prev_pos = player->pos;
					player->pos = lerp_v2(soft_data->player_pos_when_restart_started, get_player_spawn_pos(), data.percent);
					if(data.percent >= 1) {
						game->do_soft_reset = true;
					}
				}
			}

			s_m4 view = zero;

			s_v2 player_pos = lerp_v2(player->prev_pos, player->pos, interp_dt);

			e_game_state1 state1 = (e_game_state1)get_state(&hard_data->state1);
			b8 do_editor = false;
			b8 do_game = false;
			switch(state1) {
				case e_game_state1_default: {

					do_game = true;

					view = get_player_view_matrix(player_pos);

				} break;

				// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		map editor start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
				case e_game_state1_editor: {

					do_editor = true;

					s_v2 movement = zero;
					if(is_key_down(SDLK_a)) {
						movement.x -= 1;
					}
					if(is_key_down(SDLK_d)) {
						movement.x += 1;
					}
					if(is_key_down(SDLK_w)) {
						movement.y -= 1;
					}
					if(is_key_down(SDLK_s)) {
						movement.y += 1;
					}
					movement = v2_normalized(movement);
					game->editor.cam_pos += movement * 20 / game->editor.zoom;

					view = get_editor_view_matrix();

					s_v2 temp_mouse = v2_multiply_m4(g_mouse, m4_inverse(view));
					s_v2i index = v2i(
						floorfi(temp_mouse.x / c_tile_size),
						floorfi(temp_mouse.y / c_tile_size)
					);

					if(is_key_down(c_left_button) && is_valid_2d_index(index, c_max_tiles, c_max_tiles)) {
						game->map.tile_arr[index.y][index.x] = game->editor.curr_tile;
						if(game->editor.curr_tile == e_tile_type_spawn) {
							game->map.spawn_tile_index = index;
						}
					}

					if(is_key_down(c_right_button) && is_valid_2d_index(index, c_max_tiles, c_max_tiles)) {
						game->map.tile_arr[index.y][index.x] = e_tile_type_none;
					}


				} break;
				// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		map editor end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
			}

			s_m4 inverse_view = m4_inverse(view);
			s_v2 edge_topleft = v2_multiply_m4(v2(0, 0), inverse_view);
			s_v2 edge_bottomright = v2_multiply_m4(c_world_size, inverse_view);

			s_v2i topleft_index = v2i(floorfi(edge_topleft.x / c_tile_size), floorfi(edge_topleft.y / c_tile_size));
			topleft_index.x = at_least(0, topleft_index.x - 1);
			topleft_index.y = at_least(0, topleft_index.y - 1);
			s_v2i bottomright_index = v2i(ceilfi(edge_bottomright.x / c_tile_size), ceilfi(edge_bottomright.y / c_tile_size));
			bottomright_index.x = at_most(c_max_tiles, bottomright_index.x + 1);
			bottomright_index.y = at_most(c_max_tiles, bottomright_index.y + 1);

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		draw tiles start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			for(int y = topleft_index.y; y < bottomright_index.y; y += 1) {
				for(int x = topleft_index.x; x < bottomright_index.x; x += 1) {
					e_tile_type tile = game->map.tile_arr[y][x];
					s_v4 color = make_color(1);
					b8 consumed = hard_data->consumed_tile_arr[y][x];
					if(state1 != e_game_state1_editor) {
						if(soft_data->broken_tile_arr[y][x]) {
							tile = e_tile_type_none;
						}
						else if(consumed) {
							color = make_color(0.25f);
						}
					}
					if(tile != e_tile_type_none) {
						s_v2 pos = v2(x * c_tile_size, y * c_tile_size);
						s_v2i atlas_index = get_tile_atlas_index(tile);
						s_v2 size = c_tile_size_v;
						if(!consumed && tile_is_upgrade(tile)) {
							float t = update_time_to_render_time(game->update_time, interp_dt);
							pos += get_upgrade_offset(interp_dt);
							s_v2 extra_size = v2(sinf2(t * 4.26f) * 8);
							size += extra_size;
							pos -= extra_size * 0.5f;

							if(!soft_data->added_particle_emitter_arr[y][x]) {
								soft_data->added_particle_emitter_arr[y][x] = true;
								s_particle_emitter_a a = zero;
								a.pos.xy = pos + size * 0.5f;
								a.particle_duration = 1.5f;
								a.radius = 6;
								a.shrink = 0.5f;
								a.dir = v3(0.5f, -1, 0);
								a.dir_rand = v3(1, 0, 0);
								a.speed = 50;
								a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0x137BA2), 0.5f), .percent = 0});
								s_particle_emitter_b b = zero;
								b.duration = -1;
								b.particles_per_second = 50;
								b.particle_count = 1;
								b.spawn_type = e_emitter_spawn_type_circle;
								b.spawn_data.x = c_tile_size_v.x * 0.5f;
								b.pause_after_update = true;
								soft_data->particle_emitter_index_arr[y][x] = add_emitter(a, b);
							}
							else if(!consumed) {
								s_particle_emitter_b* b = &soft_data->emitter_b_arr[soft_data->particle_emitter_index_arr[y][x]];
								b->paused = false;
							}
						}
						if(tile == e_tile_type_spawn) {
							s_instance_data data = zero;
							data.model = m4_translate(v3(pos + c_tile_size_v * 0.5f, 0));
							data.model = m4_multiply(data.model, m4_scale(v3(size * 4, 1)));
							data.color = make_color(0.2f, 1.0f, 0.2f);
							add_to_render_group(data, e_shader_portal, e_texture_white, e_mesh_quad);
						}
						else if(tile == e_tile_type_goal) {
							s_instance_data data = zero;
							data.model = m4_translate(v3(pos + c_tile_size_v * 0.5f, 0));
							data.model = m4_multiply(data.model, m4_scale(v3(size * 4, 1)));
							data.color = make_color(1.0f, 0.2f, 0.2f);
							add_to_render_group(data, e_shader_portal, e_texture_white, e_mesh_quad);
						}
						else {
							draw_atlas_topleft(pos, size, atlas_index, color);
						}
					}
				}
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		draw tiles end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.view = view;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}


			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		draw ghosts start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			if(!game->hide_ghosts) {
				int ghost_count = at_most(c_max_ghosts, hard_data->ghost_count);
				for(int ghost_i = 0; ghost_i < ghost_count; ghost_i += 1) {
					s_ghost* ghost = &hard_data->ghost_arr[ghost_i];
					int update_count = at_most(ghost->pos_arr.count - 1, soft_data->update_count);
					int prev_pos_index = at_most(ghost->pos_arr.count - 1, soft_data->update_count - 1);
					s_v2 pos = lerp_v2(ghost->pos_arr[prev_pos_index], ghost->pos_arr[update_count], interp_dt);

					s_draw_player dp = get_player_draw_data();
					draw_player(pos, 0, dp, make_color(0.5f));
				}
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		draw ghosts end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		draw player start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{
				b8 immune = are_we_immune();
				b8 blink = false;
				if(immune) {
					float passed = game->update_time - soft_data->used_shield_timestamp;
					blink = fmodf(passed, 0.5f) <= 0.25f;
				}
				if(!blink) {
					float time = update_time_to_render_time(game->update_time, interp_dt);
					float angle = 0;
					s_time_data jump_time_data = get_time_data(time, player->jump_timestamp, 0.4f);
					if(player->jump_timestamp > 0 && jump_time_data.passed <= 0.4f) {
						float target_angle = 0;
						if(player->jump_x_dir < 0) {
							target_angle = -c_pi * 2;
						}
						else if(player->jump_x_dir > 0) {
							target_angle = c_pi * 2;
						}
						angle = lerp(0, target_angle, powf(jump_time_data.percent, 0.75f));
					}
					s_draw_player dp0 = get_player_draw_data();
					s_time_data run_time_data = get_time_data(time, player->start_run_timestamp, 0.4f);
					if(player->start_run_timestamp > 0) {
						float s = (float)sign_as_int(player->last_x_dir);
						float x = cosf(run_time_data.passed * 16 * s) * 3;
						float y = sinf(run_time_data.passed * 16 * s + 0.5f) * 3;
						dp0.left_foot_offset += v2(x, y);
						dp0.right_foot_offset += v2(x, y);
					}
					dp0.head_offset.x += player->last_x_vel * 0.5f;
					dp0.head_offset.y -= player->vel.y;
					dp0.left_foot_offset.y -= player->vel.y;
					dp0.right_foot_offset.y -= player->vel.y;
					player->left_foot_offset = lerp_v2(player->left_foot_offset, dp0.left_foot_offset, delta * 30);
					player->right_foot_offset = lerp_v2(player->right_foot_offset, dp0.right_foot_offset, delta * 30);
					player->head_offset = lerp_v2(player->head_offset, dp0.head_offset, delta * 30);

					s_draw_player dp1 = zero;
					dp1.head_offset = player->head_offset;
					dp1.left_foot_offset = player->left_foot_offset;
					dp1.right_foot_offset = player->right_foot_offset;

					s_v4 color = make_color(1);
					if(game->dim_player_when_out_of_jumps && player->jumps_done >= get_max_player_jumps()) {
						color = make_color(0.5f);
					}

					draw_player(player_pos, angle, dp1, color);
				}
				if(has_upgrade(e_upgrade_teleport)) {
					s_v2 teleport_pos = player_pos;
					teleport_pos.x += player->last_x_dir * c_teleport_distance;
					s_v4 color = make_color(0.0f, 0.5f, 0.0f);
					s_time_data time_data = get_time_data(game->update_time, soft_data->teleport_timestamp, c_teleport_cooldown);
					if(!can_we_teleport_without_getting_stuck(teleport_pos) || time_data.percent < 1) {
						color = make_color(0.5f, 0.0f, 0.0f);
					}
					s_v2 indicator_size = lerp_v2(zero, c_player_size_v * v2(1.0f, 1.0f), at_most(1.0f, time_data.percent));
					draw_rect(teleport_pos, indicator_size, color);
				}
				if(are_we_in_super_speed()) {
					if(soft_data->super_speed_emitter_index.valid) {
						s_particle_emitter_a* emitter = &soft_data->emitter_a_arr.data[soft_data->super_speed_emitter_index.value];
						emitter->pos.xy = player_pos;
					}
					else {
						s_particle_emitter_a a = zero;
						a.pos.xy = player_pos;
						a.particle_duration = 0.5f;
						a.radius = 10;
						a.shrink = 0.5f;
						a.dir = v3(0.5f, -1, 0);
						a.dir_rand = v3(1, 0, 0);
						a.speed = 200;
						a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0xFFFFFF), 1.0f), .percent = 0});
						a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0xC7B94D), 1.0f), .percent = 0.2f});
						a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0xD8503B), 1.0f), .percent = 0.5f});
						a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0x0), 1.0f), .percent = 0.9f});
						s_particle_emitter_b b = zero;
						b.duration = -1;
						b.particle_count = 50;
						b.particles_per_second = 100;
						b.spawn_type = e_emitter_spawn_type_rect_center;
						b.spawn_data.xy = c_player_size_v;
						soft_data->super_speed_emitter_index = maybe(add_emitter(a, b));
					}
				}
				else {
					if(soft_data->super_speed_emitter_index.valid) {
						s_particle_emitter_b* emitter = &soft_data->emitter_b_arr[soft_data->super_speed_emitter_index.value];
						emitter->duration = 0;
						soft_data->super_speed_emitter_index = zero;
					}
				}
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		draw player end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.view = view;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}

			constexpr float font_size = 48;
			s_v2 ui_pos = v2(4, 4);

			if(do_editor) {
				s_v2 pos = ui_pos;
				s_v2 size = v2(64);
				for_enum(tile_i, e_tile_type) {
					if(tile_i == e_tile_type_none) { continue; }
					s_v2i atlas_index = get_tile_atlas_index(tile_i);
					s_v4 color = make_color(0.5f);
					if(game->editor.curr_tile == tile_i) {
						color = make_color(1);
					}
					draw_atlas_topleft(pos, size, atlas_index, color);
					pos.x += size.x;
				}
				ui_pos.y += size.y;
			}

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}

			draw_background(player_pos, ortho);

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		multiplicative light start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{
				clear_framebuffer_color(game->light_fbo.id, make_color(0.5f));
				draw_light(player_pos, 512, make_color(1.0f));
				{
					s_render_flush_data data = make_render_flush_data(zero, zero);
					data.projection = ortho;
					data.view = view;
					data.blend_mode = e_blend_mode_additive;
					data.depth_mode = e_depth_mode_no_read_no_write;
					data.fbo = game->light_fbo;
					render_flush(data, true);
				}

				draw_texture_screen(c_world_center, c_world_size, make_color(1), e_texture_light, e_shader_flat, v2(0, 1), v2(1, 0));
				{
					s_render_flush_data data = make_render_flush_data(zero, zero);
					data.projection = ortho;
					data.blend_mode = e_blend_mode_multiply;
					data.depth_mode = e_depth_mode_no_read_no_write;
					render_flush(data, true);
				}
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		multiplicative light end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		additive light start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{
				clear_framebuffer_color(game->light_fbo.id, make_color(0.0f));

				topleft_index.x = at_least(0, topleft_index.x - 3);
				topleft_index.y = at_least(0, topleft_index.y - 3);
				bottomright_index.x = at_most(c_max_tiles, bottomright_index.x + 3);
				bottomright_index.y = at_most(c_max_tiles, bottomright_index.y + 3);

				for(int y = topleft_index.y; y < bottomright_index.y; y += 1) {
					for(int x = topleft_index.x; x < bottomright_index.x; x += 1) {
						e_tile_type tile = game->map.tile_arr[y][x];
						b8 consumed = hard_data->consumed_tile_arr[y][x];
						s_v2 size = c_tile_size_v;
						s_v2 pos = c_tile_size_v * v2i(x, y) + size * 0.5f;
						if(tile == e_tile_type_spike) {
							draw_light(pos, 96, make_color(0.1f, 0, 0));
						}
						else if(tile_is_upgrade(tile) && !consumed) {
							pos += get_upgrade_offset(interp_dt);
							s_v4 color = hex_to_rgb(0x137BA2);
							color.r += 0.1f;
							color.g += 0.1f;
							color.b += 0.3f;
							color = multiply_rgb(color, 0.5f);
							draw_light(pos, 128, color);
						}
					}
				}

				{
					s_render_flush_data data = make_render_flush_data(zero, zero);
					data.projection = ortho;
					data.view = view;
					data.blend_mode = e_blend_mode_additive;
					data.depth_mode = e_depth_mode_no_read_no_write;
					data.fbo = game->light_fbo;
					render_flush(data, true);
				}

				draw_texture_screen(c_world_center, c_world_size, make_color(1), e_texture_light, e_shader_flat, v2(0, 1), v2(1, 0));
				{
					s_render_flush_data data = make_render_flush_data(zero, zero);
					data.projection = ortho;
					data.blend_mode = e_blend_mode_additive;
					data.depth_mode = e_depth_mode_no_read_no_write;
					render_flush(data, true);
				}
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		additive light end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^



			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		particles start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			update_particles(delta);
			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.view = view;
				data.blend_mode = e_blend_mode_additive;
				data.depth_mode = e_depth_mode_no_read_no_write;
				render_flush(data, true);
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		particles end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			{
				constexpr float temp_font_size = 32;
				draw_text(S("A/D: Move"), c_tile_size_v * v2i(104, 517), temp_font_size, make_color(1), false, &game->font);
				draw_text(S("Space: Jump (hold to jump higher!)"), c_tile_size_v * v2i(104, 518), temp_font_size, make_color(1), false, &game->font);
				draw_text(S("R: Next loop (can save time!)"), c_tile_size_v * v2i(104, 519), temp_font_size, make_color(1), false, &game->font);
				draw_text(S("CTRL+R: Full restart"), c_tile_size_v * v2i(104, 520), temp_font_size, make_color(1), false, &game->font);
				draw_text(S("Hard ->"), c_tile_size_v * v2i(573, 562), temp_font_size * 2, make_color(1), false, &game->font);
				draw_text(S("Long ->"), c_tile_size_v * v2i(573, 574), temp_font_size * 2, make_color(1), false, &game->font);

				draw_text(S("You move 25% faster in the air!"), c_tile_size_v * v2i(151, 517), temp_font_size, make_color(1), false, &game->font);

				if(has_upgrade(e_upgrade_teleport)) {
					draw_text(S("Left click or F: Teleport (0.5s cooldown!)"), c_tile_size_v * v2i(125, 517), temp_font_size, make_color(1), false, &game->font);
				}
				if(has_upgrade(e_upgrade_super_speed)) {
					draw_text(format_text("Right click: Super speed (%.0fs duration)", c_super_speed_duration), c_tile_size_v * v2i(125, 519), temp_font_size, make_color(1), false, &game->font);
					draw_text(S("(only 1 use per loop!)"), c_tile_size_v * v2i(125, 520), temp_font_size, make_color(1), false, &game->font);
				}

				// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		timed messages start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
				{
					foreach_ptr(msg_i, msg, soft_data->timed_msg_arr) {
						float time = update_time_to_render_time(game->update_time, interp_dt);
						s_time_data data = get_time_data(time, msg->spawn_timestamp, 3.0f);
						s_v4 color = hsv_to_rgb(data.passed * 500, 1, 1);
						color.a = powf(1.0f - data.percent, 2);
						draw_text(builder_to_len_str(&msg->builder), msg->pos, 40, color, true, &game->font);
						msg->pos.y -= 100 * delta;

						if(data.percent >= 1) {
							soft_data->timed_msg_arr.remove_and_swap(msg_i);
							msg_i -= 1;
						}
					}
				}
				// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		timed messages end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

				{
					s_render_flush_data data = make_render_flush_data(zero, zero);
					data.projection = ortho;
					data.view = view;
					data.blend_mode = e_blend_mode_normal;
					data.depth_mode = e_depth_mode_no_read_yes_write;
					render_flush(data, true);
				}
			}

			if(do_editor) {

				s_v2 temp_mouse = v2_multiply_m4(g_mouse, inverse_view);
				s_v2i index = v2i(
					floorfi(temp_mouse.x / c_tile_size),
					floorfi(temp_mouse.y / c_tile_size)
				);

				s_len_str a = format_text("$$FFFFFF%i$., ", index.x);
				if(index.x < 0 || index.x >= c_max_tiles) {
					a = format_text("$$FF0000%i$., ", index.x);
				}
				s_len_str b = format_text("$$FFFFFF%i$.", index.y);
				if(index.y < 0 || index.y >= c_max_tiles) {
					b = format_text("$$FF0000%i$.", index.y);
				}
				s_len_str text = format_text("%.*s%.*s", a.count, a.str, b.count, b.str);
				draw_text(text, ui_pos, font_size, make_color(1), false, &game->font);
				ui_pos.y += font_size;
			}

			if(do_game && !game->hide_timer) {
				int update_count = get_update_count_with_upgrades(hard_data->update_count);
				s_time_format format = update_count_to_time_format(update_count);
				s_len_str text = format_text("%02d:%02d.%i", format.minutes, format.seconds, format.milliseconds);
				draw_text(text, ui_pos, font_size, make_color(1), false, &game->font);
				ui_pos.y += font_size;
			}

			if(do_game) {
				float passed = game->update_time - soft_data->run_start_timestamp;
				float loop_time = get_max_loop_time();
				float time_left = loop_time - passed;
				time_left = at_least(0.0f, time_left);
				s_len_str text = format_text("%i", ceilfi(time_left));
				draw_text(text, wxy(0.5f, 0.05f), font_size, make_color(1), true, &game->font);
				// ui_pos.y += font_size;
				if(time_left <= 0 && !game->freeze_loop) {
					start_restart(player->pos);
				}
			}

			{
				s_render_flush_data data = make_render_flush_data(zero, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_yes_write;
				render_flush(data, true);
			}





			#if 0
			{
				s_render_flush_data data = make_render_flush_data(cam_pos, zero);
				data.projection = perspective;
				data.view = view;
				data.light_projection = light_projection;
				data.light_view = light_view;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_no_write;
				render_flush(data, true);
			}

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		post start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{
				draw_texture_screen(c_world_center, c_world_size, make_color(1), e_texture_white, e_shader_post, zero, zero);
				s_render_flush_data data = make_render_flush_data(cam_pos, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_no_write;
				render_flush(data, true);
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		post end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			{
				s_v2 pos = v2(4);
				constexpr float font_size = 48;

				if(!game->hide_timer) {
					s_time_format format = update_count_to_time_format(hard_data->update_count);
					s_len_str text = format_text("%02d:%02d.%i", format.minutes, format.seconds, format.milliseconds);
					draw_text(text, pos, font_size, make_color(1), false, &game->font);
					pos.y += font_size;
				}

				#if defined(m_debug)
				{
					s_len_str text = format_text("FPS: %i", roundfi(1.0f / delta));
					draw_text(text, pos, font_size, make_color(1), false, &game->font);
					pos.y += font_size;
				}
				#endif // m_debug

				pos.y += font_size * 0.5f;
			}

			{
				s_render_flush_data data = make_render_flush_data(cam_pos, zero);
				data.projection = ortho;
				data.blend_mode = e_blend_mode_normal;
				data.depth_mode = e_depth_mode_no_read_no_write;
				render_flush(data, true);
			}

			#endif

		} break;
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		play end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		case e_game_state0_input_name: {
			game->speed = 0;
			draw_background(zero, ortho);
			s_input_name_state* state = &game->input_name_state;
			float font_size = 36;
			s_v2 pos = c_world_size * v2(0.5f, 0.4f);

			int count_before = state->name.str.count;
			b8 submitted = handle_string_input(&state->name, game->render_time);
			int count_after = state->name.str.count;
			if(count_before != count_after) {
				play_sound(e_sound_key);
			}
			if(submitted) {
				b8 can_submit = true;
				if(state->name.str.count < 2) {
					can_submit = false;
					cstr_into_builder(&state->error_str, "Name must have at least 2 characters!");
				}
				if(can_submit) {
					state->error_str.count = 0;
					#if defined(__EMSCRIPTEN__)
					set_leaderboard_name(builder_to_len_str(&state->name.str));
					#endif
					game->leaderboard_nice_name = state->name.str;
				}
			}

			draw_text(S("Victory!"), c_world_size * v2(0.5f, 0.1f), font_size, make_color(1), true, &game->font);
			draw_text(S("Enter your name"), c_world_size * v2(0.5f, 0.2f), font_size, make_color(1), true, &game->font);
			if(state->error_str.count > 0) {
				draw_text(builder_to_len_str(&state->error_str), c_world_size * v2(0.5f, 0.3f), font_size, hex_to_rgb(0xD77870), true, &game->font);
			}

			if(state->name.str.count > 0) {
				draw_text(builder_to_len_str(&state->name.str), pos, font_size, make_color(1), true, &game->font);
			}

			s_v2 full_text_size = get_text_size(builder_to_len_str(&state->name.str), &game->font, font_size);
			s_v2 partial_text_size = get_text_size_with_count(builder_to_len_str(&state->name.str), &game->font, font_size, state->name.cursor.value, 0);
			s_v2 cursor_pos = v2(
				-full_text_size.x * 0.5f + pos.x + partial_text_size.x,
				pos.y - font_size * 0.5f
			);

			s_v2 cursor_size = v2(15.0f, font_size);
			float t = game->render_time - max(state->name.last_action_time, state->name.last_edit_time);
			b8 blink = false;
			constexpr float c_blink_rate = 0.75f;
			if(t > 0.75f && fmodf(t, c_blink_rate) >= c_blink_rate / 2) {
				blink = true;
			}
			float t2 = clamp(game->render_time - state->name.last_edit_time, 0.0f, 1.0f);
			s_v4 color = lerp_color(hex_to_rgb(0xffdddd), multiply_rgb_clamped(hex_to_rgb(0xABC28F), 0.8f), 1 - powf(1 - t2, 3));
			float extra_height = ease_out_elastic2_advanced(t2, 0, 0.75f, 20, 0);
			cursor_size.y += extra_height;

			if(!state->name.visual_pos_initialized) {
				state->name.visual_pos_initialized = true;
				state->name.cursor_visual_pos = cursor_pos;
			}
			else {
				state->name.cursor_visual_pos = lerp_snap(state->name.cursor_visual_pos, cursor_pos, delta * 20);
			}

			if(!blink) {
				draw_rect_topleft(state->name.cursor_visual_pos - v2(0.0f, extra_height / 2), cursor_size, color);
			}

			s_render_flush_data data = make_render_flush_data(zero, zero);
			data.projection = ortho;
			data.blend_mode = e_blend_mode_normal;
			data.depth_mode = e_depth_mode_no_read_no_write;
			render_flush(data, true);

		} break;
	}

	s_state_transition transition = get_state_transition(&game->state0, game->render_time);
	if(transition.active) {
		{
			float alpha = 0;
			if(transition.time_data.percent <= 0.5f) {
				alpha = transition.time_data.percent * 2;
			}
			else {
				alpha = transition.time_data.inv_percent * 2;
			}
			s_instance_data data = zero;
			data.model = fullscreen_m4();
			data.color = make_color(0.0f, 0, 0, alpha);
			add_to_render_group(data, e_shader_flat, e_texture_white, e_mesh_quad);
		}

		{
			s_render_flush_data data = make_render_flush_data(zero, zero);
			data.projection = ortho;
			data.blend_mode = e_blend_mode_normal;
			data.depth_mode = e_depth_mode_no_read_no_write;
			render_flush(data, true);
		}
	}

	SDL_GL_SwapWindow(g_platform_data->window);

	game->render_time += delta;
}

func f64 get_seconds()
{
	u64 now =	SDL_GetPerformanceCounter();
	return (now - g_platform_data->start_cycles) / (f64)g_platform_data->cycle_frequency;
}

func void on_gl_error(const char* expr, char* file, int line, int error)
{
	#define m_gl_errors \
	X(GL_INVALID_ENUM, "GL_INVALID_ENUM") \
	X(GL_INVALID_VALUE, "GL_INVALID_VALUE") \
	X(GL_INVALID_OPERATION, "GL_INVALID_OPERATION") \
	X(GL_STACK_OVERFLOW, "GL_STACK_OVERFLOW") \
	X(GL_STACK_UNDERFLOW, "GL_STACK_UNDERFLOW") \
	X(GL_OUT_OF_MEMORY, "GL_STACK_OUT_OF_MEMORY") \
	X(GL_INVALID_FRAMEBUFFER_OPERATION, "GL_STACK_INVALID_FRAME_BUFFER_OPERATION")

	const char* error_str;
	#define X(a, b) case a: { error_str = b; } break;
	switch(error) {
		m_gl_errors
		default: {
			error_str = "unknown error";
		} break;
	}
	#undef X
	#undef m_gl_errors

	printf("GL ERROR: %s - %i (%s)\n", expr, error, error_str);
	printf("  %s(%i)\n", file, line);
	printf("--------\n");

	#if defined(_WIN32)
	__debugbreak();
	#else
	__builtin_trap();
	#endif
}

func void draw_rect(s_v2 pos, s_v2 size, s_v4 color)
{
	s_instance_data data = zero;
	data.model = m4_translate(v3(pos, 0));
	data.model = m4_multiply(data.model, m4_scale(v3(size, 1)));
	data.color = color;

	add_to_render_group(data, e_shader_flat, e_texture_white, e_mesh_quad);
}

func void draw_rect_topleft(s_v2 pos, s_v2 size, s_v4 color)
{
	pos += size * 0.5f;
	draw_rect(pos, size, color);
}

func void draw_texture_screen(s_v2 pos, s_v2 size, s_v4 color, e_texture texture_id, e_shader shader_id, s_v2 uv_min, s_v2 uv_max)
{
	s_instance_data data = zero;
	data.model = m4_translate(v3(pos, 0));
	data.model = m4_multiply(data.model, m4_scale(v3(size, 1)));
	data.color = color;
	data.uv_min = uv_min;
	data.uv_max = uv_max;

	add_to_render_group(data, shader_id, texture_id, e_mesh_quad);
}

func void draw_mesh(e_mesh mesh_id, s_m4 model, s_v4 color, e_shader shader_id)
{
	s_instance_data data = zero;
	data.model = model;
	data.color = color;
	add_to_render_group(data, shader_id, e_texture_white, mesh_id);
}

func void draw_mesh(e_mesh mesh_id, s_v3 pos, s_v3 size, s_v4 color, e_shader shader_id)
{
	s_m4 model = m4_translate(pos);
	model = m4_multiply(model, m4_scale(size));
	draw_mesh(mesh_id, model, color, shader_id);
}

func void bind_framebuffer(u32 fbo)
{
	if(game->curr_fbo != fbo) {
		gl(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
		game->curr_fbo = fbo;
	}
}

func void clear_framebuffer_color(u32 fbo, s_v4 color)
{
	bind_framebuffer(fbo);
	glClearColor(color.x, color.y, color.z, color.w);
	glClear(GL_COLOR_BUFFER_BIT);
}

func void clear_framebuffer_depth(u32 fbo)
{
	bind_framebuffer(fbo);
	set_depth_mode(e_depth_mode_read_and_write);
	glClear(GL_DEPTH_BUFFER_BIT);
}

func void render_flush(s_render_flush_data data, b8 reset_render_count)
{
	bind_framebuffer(data.fbo.id);

	if(data.fbo.id == 0) {
		s_rect letterbox = do_letterbox(v2(g_platform_data->window_size), c_world_size);
		glViewport((int)letterbox.x, (int)letterbox.y, (int)letterbox.w, (int)letterbox.h);
	}
	else {
		glViewport(0, 0, data.fbo.size.x, data.fbo.size.y);
	}

	set_cull_mode(data.cull_mode);
	set_depth_mode(data.depth_mode);
	set_blend_mode(data.blend_mode);

	{
		gl(glBindBuffer(GL_UNIFORM_BUFFER, game->ubo));
		s_uniform_data uniform_data = zero;
		uniform_data.view = data.view;
		uniform_data.projection = data.projection;
		uniform_data.light_view = data.light_view;
		uniform_data.light_projection = data.light_projection;
		uniform_data.render_time = game->render_time;
		uniform_data.cam_pos = data.cam_pos;
		uniform_data.mouse = g_mouse;
		uniform_data.player_pos = data.player_pos;
		gl(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(s_uniform_data), &uniform_data));
	}

	foreach_val(group_i, group, game->render_group_arr) {
		s_mesh* mesh = &game->mesh_arr[group.mesh_id];
		int* instance_count = &game->render_instance_count[group.shader_id][group.texture_id][group.mesh_id];
		assert(*instance_count > 0);
		void* instance_data = game->render_instance_arr[group.shader_id][group.texture_id][group.mesh_id];

		gl(glUseProgram(game->shader_arr[group.shader_id].id));

		int in_texture_loc = glGetUniformLocation(game->shader_arr[group.shader_id].id, "in_texture");
		int noise_loc = glGetUniformLocation(game->shader_arr[group.shader_id].id, "noise");
		if(in_texture_loc >= 0) {
			glUniform1i(in_texture_loc, 0);
			glActiveTexture(GL_TEXTURE0);
			gl(glBindTexture(GL_TEXTURE_2D, game->texture_arr[group.texture_id].id));
		}
		if(noise_loc >= 0) {
			glUniform1i(noise_loc, 2);
			glActiveTexture(GL_TEXTURE2);
			gl(glBindTexture(GL_TEXTURE_2D, game->texture_arr[e_texture_noise].id));
		}

		gl(glBindVertexArray(mesh->vao));
		gl(glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_vbo.id));
		gl(glBufferSubData(GL_ARRAY_BUFFER, 0, group.element_size * *instance_count, instance_data));
		gl(glDrawArraysInstanced(GL_TRIANGLES, 0, mesh->vertex_count, *instance_count));
		if(reset_render_count) {
			game->render_group_arr.remove_and_swap(group_i);
			game->render_group_index_arr[group.shader_id][group.texture_id][group.mesh_id] = -1;
			group_i -= 1;
			*instance_count = 0;
		}
	}
}

template <typename t>
func void add_to_render_group(t data, e_shader shader_id, e_texture texture_id, e_mesh mesh_id)
{
	s_render_group render_group = zero;
	render_group.shader_id = shader_id;
	render_group.texture_id = texture_id;
	render_group.mesh_id = mesh_id;
	render_group.element_size = sizeof(t);

	s_mesh* mesh = &game->mesh_arr[render_group.mesh_id];

	int* render_group_index = &game->render_group_index_arr[render_group.shader_id][render_group.texture_id][render_group.mesh_id];
	if(*render_group_index < 0) {
		game->render_group_arr.add(render_group);
		*render_group_index = game->render_group_arr.count - 1;
	}
	int* count = &game->render_instance_count[render_group.shader_id][render_group.texture_id][render_group.mesh_id];
	int* max_elements = &game->render_instance_max_elements[render_group.shader_id][render_group.texture_id][render_group.mesh_id];
	t* ptr = (t*)game->render_instance_arr[render_group.shader_id][render_group.texture_id][render_group.mesh_id];
	b8 expand = *max_elements <= *count;
	b8 get_new_ptr = *count <= 0 || expand;
	int new_max_elements = *max_elements;
	if(expand) {
		if(new_max_elements <= 0) {
			new_max_elements = 64;
		}
		else {
			new_max_elements *= 2;
		}
		if(new_max_elements > mesh->instance_vbo.max_elements) {
			gl(glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_vbo.id));
			gl(glBufferData(GL_ARRAY_BUFFER, sizeof(t) * new_max_elements, null, GL_DYNAMIC_DRAW));
			mesh->instance_vbo.max_elements = new_max_elements;
		}
	}
	if(get_new_ptr) {
		t* temp = (t*)arena_alloc(&game->render_frame_arena, sizeof(t) * new_max_elements);
		if(*count > 0) {
			memcpy(temp, ptr, *count * sizeof(t));
		}
		game->render_instance_arr[render_group.shader_id][render_group.texture_id][render_group.mesh_id] = (void*)temp;
		ptr = temp;
	}
	*max_elements = new_max_elements;
	*(ptr + *count) = data;
	*count += 1;
}

func s_shader load_shader_from_file(char* file, s_linear_arena* arena)
{
	b8 delete_shaders[2] = {true, true};
	char* src = (char*)read_file(file, arena);
	assert(src);

	u32 shader_arr[] = {glCreateShader(GL_VERTEX_SHADER), glCreateShader(GL_FRAGMENT_SHADER)};

	#if defined(__EMSCRIPTEN__)
	const char* header = "#version 300 es\nprecision highp float;\n";
	#else
	const char* header = "#version 330 core\n";
	#endif

	char* shared_src = (char*)try_really_hard_to_read_file("src/shader_shared.h", arena);
	assert(shared_src);

	for(int i = 0; i < 2; i += 1) {
		const char* src_arr[] = {header, "", "", shared_src, src};
		if(i == 0) {
			src_arr[1] = "#define m_vertex 1\n";
			src_arr[2] = "#define shared_var out\n";
		}
		else {
			src_arr[1] = "#define m_fragment 1\n";
			src_arr[2] = "#define shared_var in\n";
		}
		gl(glShaderSource(shader_arr[i], array_count(src_arr), (const GLchar * const *)src_arr, null));
		gl(glCompileShader(shader_arr[i]));

		int compile_success;
		char info_log[1024];
		gl(glGetShaderiv(shader_arr[i], GL_COMPILE_STATUS, &compile_success));

		if(!compile_success) {
			gl(glGetShaderInfoLog(shader_arr[i], sizeof(info_log), null, info_log));
			printf("Failed to compile shader: %s\n%s", file, info_log);
			delete_shaders[i] = false;
		}
	}

	b8 detach_shaders = delete_shaders[0] && delete_shaders[1];

	u32 program = 0;
	if(delete_shaders[0] && delete_shaders[1]) {
		program = gl(glCreateProgram());
		gl(glAttachShader(program, shader_arr[0]));
		gl(glAttachShader(program, shader_arr[1]));
		gl(glLinkProgram(program));

		int linked = 0;
		gl(glGetProgramiv(program, GL_LINK_STATUS, &linked));
		if(!linked) {
			char info_log[1024] = zero;
			gl(glGetProgramInfoLog(program, sizeof(info_log), null, info_log));
			printf("FAILED TO LINK: %s\n", info_log);
		}
	}

	if(detach_shaders) {
		gl(glDetachShader(program, shader_arr[0]));
		gl(glDetachShader(program, shader_arr[1]));
	}

	if(delete_shaders[0]) {
		gl(glDeleteShader(shader_arr[0]));
	}
	if(delete_shaders[1]) {
		gl(glDeleteShader(shader_arr[1]));
	}

	s_shader result = zero;
	result.id = program;
	return result;
}

func b8 do_button(s_len_str text, s_v2 pos, b8 centered)
{
	s_v2 size = v2(256, 48);
	b8 result = do_button_ex(text, pos, size, centered);
	return result;
}

func b8 do_button_ex(s_len_str text, s_v2 pos, s_v2 size, b8 centered)
{
	b8 result = false;
	if(!centered) {
		pos += size * 0.5f;
	}

	b8 hovered = mouse_vs_rect_center(g_mouse, pos, size);
	s_v4 color = make_color(0.25f);
	if(hovered) {
		size += v2(8);
		if(!centered) {
			pos += v2(8) * 0.5f;
		}
		color = make_color(0.5f);
		if(g_click) {
			result = true;
			play_sound(e_sound_click);
		}
	}

	{
		s_instance_data data = zero;
		data.model = m4_translate(v3(pos, 0));
		data.model = m4_multiply(data.model, m4_scale(v3(size, 1)));
		data.color = color,
		add_to_render_group(data, e_shader_button, e_texture_white, e_mesh_quad);
	}

	draw_text(text, pos, 32.0f, make_color(1), true, &game->font);

	return result;
}

func b8 do_bool_button(s_len_str text, s_v2 pos, b8 centered, b8* out)
{
	s_v2 size = v2(256, 48);
	b8 result = do_bool_button_ex(text, pos, size, centered, out);
	return result;
}

func b8 do_bool_button_ex(s_len_str text, s_v2 pos, s_v2 size, b8 centered, b8* out)
{
	assert(out);
	b8 result = false;
	if(do_button_ex(text, pos, size, centered)) {
		result = true;
		*out = !(*out);
	}
	return result;
}

func b8 is_key_pressed(int key, b8 consume)
{
	b8 result = false;
	if(key < c_max_keys) {
		result = game->input_arr[key].half_transition_count > 1 || (game->input_arr[key].half_transition_count > 0 && game->input_arr[key].is_down);
	}
	if(result && consume) {
		game->input_arr[key].half_transition_count = 0;
		game->input_arr[key].is_down = false;
	}
	return result;
}

func b8 is_key_down(int key)
{
	b8 result = false;
	if(key < c_max_keys) {
		result = game->input_arr[key].half_transition_count > 1 || game->input_arr[key].is_down;
	}
	return result;
}

template <int n>
func void cstr_into_builder(s_str_builder<n>* builder, char* str)
{
	assert(str);

	int len = (int)strlen(str);
	assert(len <= n);
	memcpy(builder->str, str, len);
	builder->count = len;
}

template <int n>
func s_len_str builder_to_len_str(s_str_builder<n>* builder)
{
	s_len_str result = zero;
	result.str = builder->str;
	result.count = builder->count;
	return result;
}

template <int n>
func b8 handle_string_input(s_input_str<n>* str, float time)
{
	b8 result = false;
	if(!str->cursor.valid) {
		str->cursor = maybe(0);
	}
	foreach_val(c_i, c, game->char_events) {
		if(is_alpha_numeric(c) || c == '_') {
			if(!is_builder_full(&str->str)) {
				builder_insert_char(&str->str, str->cursor.value, c);
				str->cursor.value += 1;
				str->last_edit_time = time;
				str->last_action_time = str->last_edit_time;
			}
		}
	}

	foreach_val(event_i, event, game->key_events) {
		if(!event.went_down) { continue; }
		if(event.key == SDLK_RETURN) {
			result = true;
			str->last_action_time = time;
		}
		else if(event.key == SDLK_ESCAPE) {
			str->cursor.value = 0;
			str->str.count = 0;
			str->str.str[0] = 0;
			str->last_edit_time = time;
			str->last_action_time = str->last_edit_time;
		}
		else if(event.key == SDLK_BACKSPACE) {
			if(str->cursor.value > 0) {
				str->cursor.value -= 1;
				builder_remove_char_at(&str->str, str->cursor.value);
				str->last_edit_time = time;
				str->last_action_time = str->last_edit_time;
			}
		}
	}
	return result;
}

func void handle_key_event(int key, b8 is_down, b8 is_repeat)
{
	if(is_down) {
		game->any_key_pressed = true;
	}
	if(key < c_max_keys) {
		if(!is_repeat) {
			game->any_key_pressed = true;
		}

		{
			s_key_event key_event = {};
			key_event.went_down = is_down;
			key_event.key = key;
			// key_event.modifiers |= e_input_modifier_ctrl * is_key_down(&g_platform_data.input, c_key_left_ctrl);
			game->key_events.add(key_event);
		}
	}
}

func void do_leaderboard()
{
	b8 escape = is_key_pressed(SDLK_ESCAPE, true);
	if(do_button(S("Back"), wxy(0.87f, 0.92f), true) || escape) {
		s_maybe<int> prev = get_previous_non_temporary_state(&game->state0);
		if(prev.valid && prev.value == e_game_state0_pause) {
			pop_state_transition(&game->state0, game->render_time, c_transition_time);
		}
		else {
			add_state_transition(&game->state0, e_game_state0_main_menu, game->render_time, c_transition_time);
			clear_state(&game->state0);
		}
	}

	{
		if(!game->leaderboard_received) {
			draw_text(S("Getting leaderboard..."), c_world_center, 48, make_color(0.66f), true, &game->font);
		}
		else if(game->leaderboard_arr.count <= 0) {
			draw_text(S("No scores yet :("), c_world_center, 48, make_color(0.66f), true, &game->font);
		}

		constexpr int c_max_visible_entries = 10;
		s_v2 pos = c_world_center * v2(1.0f, 0.7f);
		for(int entry_i = 0; entry_i < at_most(c_max_visible_entries + 1, game->leaderboard_arr.count); entry_i += 1) {
			s_leaderboard_entry entry = game->leaderboard_arr[entry_i];
			s_time_format data = update_count_to_time_format(entry.time);
			s_v4 color = make_color(0.8f);
			int rank_number = entry_i + 1;
			if(entry_i == c_max_visible_entries || builder_equals(&game->leaderboard_public_uid, &entry.internal_name)) {
				color = hex_to_rgb(0xD3A861);
				rank_number = entry.rank;
			}
			char* name = entry.internal_name.str;
			if(entry.nice_name.count > 0) {
				name = entry.nice_name.str;
			}
			draw_text(format_text("%i %s", rank_number, name), v2(c_world_size.x * 0.1f, pos.y - 24), 32, color, false, &game->font);
			s_len_str text = format_text("%02i:%02i.%i", data.minutes, data.seconds, data.milliseconds);
			draw_text(text, v2(c_world_size.x * 0.5f, pos.y - 24), 32, color, false, &game->font);
			pos.y += 48;
		}
	}
}

func s_v2 get_rect_normal_of_closest_edge(s_v2 p, s_v2 center, s_v2 size)
{
	s_v2 edge_arr[] = {
		v2(center.x - size.x * 0.5f, center.y),
		v2(center.x + size.x * 0.5f, center.y),
		v2(center.x, center.y - size.y * 0.5f),
		v2(center.x, center.y + size.y * 0.5f),
	};
	s_v2 normal_arr[] = {
		v2(-1, 0),
		v2(1, 0),
		v2(0, -1),
		v2(0, 1),
	};
	float closest_dist[2] = {9999999, 9999999};
	int closest_index[2] = {0, 0};

	for(int i = 0; i < 4; i += 1) {
		float dist;
		if(i <= 1) {
			dist = fabsf(p.x - edge_arr[i].x);
		}
		else {
			dist = fabsf(p.y - edge_arr[i].y);
		}
		if(dist < closest_dist[0]) {
			closest_dist[0] = dist;
			closest_index[0] = i;
		}
		else if(dist < closest_dist[1]) {
			closest_dist[1] = dist;
			closest_index[1] = i;
		}
	}
	s_v2 result = normal_arr[closest_index[0]];

	// @Note(tkap, 19/04/2025): Probably breaks for very thin rectangles
	if(fabsf(closest_dist[0] - closest_dist[1]) <= 0.01f) {
		result += normal_arr[closest_index[1]];
		result = v2_normalized(result);
	}
	return result;
}

func void load_map()
{
	u8* data = read_file("map.map", &game->update_frame_arena);
	if(!data) { return; }
	u8* cursor = data;
	int version = buffer_read<int>(&cursor);
	game->map.spawn_tile_index = buffer_read<s_v2i>(&cursor);
	assert(version == 1);
	memcpy(game->map.tile_arr, cursor, sizeof(game->map.tile_arr));
}

func void save_map(char* path)
{
	u8* buffer = arena_alloc(&game->update_frame_arena, 5 * c_mb);
	u8* cursor = buffer;
	buffer_write(&cursor, c_map_version);
	buffer_write(&cursor, game->map.spawn_tile_index);
	buffer_memcpy(&cursor, game->map.tile_arr, sizeof(game->map.tile_arr));
	int size = (int)(cursor - buffer);
	assert(size <= 5 * c_mb);
	write_file(path, buffer, size);
}

func b8 is_valid_2d_index(s_v2i index, int x_count, int y_count)
{
	b8 result = index.x >= 0 && index.x < x_count && index.y >= 0 && index.y < y_count;
	return result;
}

func b8 check_action(float curr_time, float timestamp, float grace)
{
	float passed = curr_time - timestamp;
	b8 result = passed <= grace && timestamp > 0;
	return result;
}

func void do_player_move(int movement_index, float movement, s_player* player)
{
	assert(movement_index == 0 || movement_index == 1);
	s_hard_game_data* hard_data = &game->hard_data;
	s_soft_game_data* soft_data = &game->hard_data.soft_data;
	float temp_movement = movement;
	float s = (float)sign_as_int(temp_movement);
	while(fabsf(temp_movement) > 0) {
		float to_move = at_most(1.0f, fabsf(temp_movement));
		player->pos.all[movement_index] += to_move * s;
		s_v2i player_index = v2i(floorfi(player->pos.x / c_tile_size), floorfi(player->pos.y / c_tile_size));
		b8 collided = false;
		for(int y = -1; y <= 1; y += 1) {
			for(int x = -1; x <= 1; x += 1) {
				s_v2i index = player_index + v2i(x, y);
				if(!is_valid_2d_index(index, c_max_tiles, c_max_tiles)) { continue; }
				e_tile_type tile_type = game->map.tile_arr[index.y][index.x];
				if(soft_data->broken_tile_arr[index.y][index.x] || hard_data->consumed_tile_arr[index.y][index.x]) {
					tile_type = e_tile_type_none;
				}
				if(tile_type == e_tile_type_none) { continue; }
				s_v2 tile_pos = v2(index.x * c_tile_size, index.y * c_tile_size);
				s_v2 tile_size = c_tile_size_v;
				if(tile_type == e_tile_type_spike) {
					tile_size.x -= 4;
					tile_size.y -= 4;
				}
				b8 collides = false;
				if(tile_type == e_tile_type_platform) {
					tile_size.y = 1;
					s_v2 bottom = player->pos;
					bottom.y += c_player_size_v.y * 0.5f - 1;
					collides = rect_vs_rect_center(bottom, v2(c_player_size_v.x, 2.0f), tile_pos + tile_size * 0.5f, tile_size);
				}
				else {
					if(tile_is_upgrade(tile_type)) {
						tile_size += v2(10);
					}
					collides = rect_vs_rect_center(player->pos, c_player_size_v, tile_pos + c_tile_size_v * 0.5f, tile_size);
				}
				if(collides) {
					b8 blocks_movement = tile_type == e_tile_type_block ||
						(tile_type == e_tile_type_breakable && !has_upgrade(e_upgrade_break_tiles)) ||
						(tile_type == e_tile_type_platform && s > 0 && movement_index == 1)
					;
					if(tile_is_upgrade(tile_type)) {
						hard_data->consumed_tile_arr[index.y][index.x] = true;
						play_sound(e_sound_clap);
						do_screen_shake(30);
						soft_data->collected_upgrade_this_run = true;

						{
							s_particle_emitter_a a = zero;
							a.pos.xy = tile_pos + c_tile_size_v * 0.5f;
							a.particle_duration = 1.0f;
							a.radius = 10;
							a.shrink = 0.5f;
							a.dir = v3(0.5f, -1, 0);
							a.dir_rand = v3(1, 0, 0);
							a.speed = 150;
							a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0x137BA2), 0.5f), .percent = 0});
							s_particle_emitter_b b = zero;
							b.duration = 0;
							b.particle_count = 100;
							b.spawn_type = e_emitter_spawn_type_circle;
							b.spawn_data.x = c_tile_size_v.x * 0.5f;
							add_emitter(a, b);
						}
					}
					if(tile_type == e_tile_type_upgrade_jump) {
						hard_data->upgrade_arr[e_upgrade_one_more_jump] += 1;
						add_timed_msg(S("One more jump!"), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_speed) {
						hard_data->upgrade_arr[e_upgrade_speed] += 1;
						add_timed_msg(format_text("+%.0f%% speed!", c_increased_movement_speed_per_upgrade * 100), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_more_loop_time) {
						hard_data->upgrade_arr[e_upgrade_more_loop_time] += 1;
						add_timed_msg(format_text("+%.0fs loop time!", c_extra_loop_time_per_upgrade), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_less_run_time) {
						hard_data->upgrade_arr[e_upgrade_less_run_time] += 1;
						add_timed_msg(format_text("-%is total run time!", c_seconds_saved_per_upgrade), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_anti_spike) {
						hard_data->upgrade_arr[e_upgrade_anti_spike] += 1;
						add_timed_msg(S("Immune to spikes! (once per loop)"), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_teleport) {
						hard_data->upgrade_arr[e_upgrade_teleport] += 1;
						add_timed_msg(S("Teleport!!! (left click or F)"), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_break_tiles) {
						hard_data->upgrade_arr[e_upgrade_break_tiles] += 1;
						add_timed_msg(S("Can break some blocks!"), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_upgrade_super_speed) {
						hard_data->upgrade_arr[e_upgrade_super_speed] += 1;
						add_timed_msg(S("SUPER SPEED!!! (right click, once per loop)"), tile_pos + c_tile_size_v * 0.5f);
					}
					else if(tile_type == e_tile_type_goal) {
						if(game->leaderboard_nice_name.count <= 0 && c_on_web) {
							add_temporary_state_transition(&game->state0, e_game_state0_input_name, game->render_time, c_transition_time);
						}
						else {
							add_state_transition(&game->state0, e_game_state0_win_leaderboard, game->render_time, c_transition_time);
							int update_count = get_update_count_with_upgrades(hard_data->update_count);
							game->update_count_at_win_time = update_count;
							#if defined(__EMSCRIPTEN__)
							submit_leaderboard_score(update_count, c_leaderboard_id);
							#endif
						}
					}
					else if(tile_type == e_tile_type_spike) {
						// @Note(tkap, 28/06/2025): We are immune, do nothing
						if(are_we_immune()) {

						}
						// @Note(tkap, 28/06/2025): Activate immunity
						else if(has_upgrade(e_upgrade_anti_spike) && soft_data->used_shield_timestamp == 0) {
							soft_data->used_shield_timestamp = game->update_time;
							play_sound(e_sound_shield);
						}
						// @Note(tkap, 28/06/2025): Die
						else {
							start_restart(player->pos);
						}
					}
					else if(tile_type == e_tile_type_breakable && has_upgrade(e_upgrade_break_tiles)) {
						soft_data->broken_tile_arr[index.y][index.x] = true;
						do_screen_shake(10);
						play_sound(e_sound_break);

						{
							s_particle_emitter_a a = zero;
							a.pos.xy = tile_pos + c_tile_size_v * 0.5f;
							a.particle_duration = 2.0f;
							a.radius = 10;
							a.shrink = 0.5f;
							a.dir = v3(0.5f, -1, 0);
							a.dir_rand = v3(1, 0, 0);
							a.speed = 100;
							a.gravity = 5;
							a.color_arr.add({.color = multiply_rgb(hex_to_rgb(0x394a50), 0.5f), .percent = 0});
							s_particle_emitter_b b = zero;
							b.duration = 0;
							b.particle_count = 50;
							b.spawn_type = e_emitter_spawn_type_rect_center;
							b.spawn_data.xy = c_tile_size_v;
							add_emitter(a, b);
						}
					}
					else if(!collided && blocks_movement) {
						collided = true;
						player->pos.all[movement_index] -= to_move * s;
						if(movement_index == 1) {
							if(s > 0) {
								player->jumps_done = 0;
								player->jumping = false;
								player->on_ground_timestamp = game->update_time;
								if(player->vel.y >= 7) {
									play_sound(e_sound_land);
								}
							}
							player->vel.y = 0;
						}
					}
				}
			}
		}
		temp_movement -= to_move * s;
		if(collided) { break; }
	}
}

func b8 can_we_teleport_without_getting_stuck(s_v2 teleport_pos)
{
	s_soft_game_data* soft_data = &game->hard_data.soft_data;
	s_v2i player_index = v2i(floorfi(teleport_pos.x / c_tile_size), floorfi(teleport_pos.y / c_tile_size));

	b8 allow_teleport = true;
	for(int y = -1; y <= 1; y += 1) {
		for(int x = -1; x <= 1; x += 1) {
			s_v2i index = player_index + v2i(x, y);
			if(!is_valid_2d_index(index, c_max_tiles, c_max_tiles)) { continue; }
			e_tile_type tile_type = game->map.tile_arr[index.y][index.x];
			if(soft_data->broken_tile_arr[index.y][index.x]) {
				tile_type = e_tile_type_none;
			}
			if(tile_type == e_tile_type_none) { continue; }
			s_v2 tile_pos = v2(index.x * c_tile_size, index.y * c_tile_size);
			s_v2 tile_size = c_tile_size_v;
			if(tile_type == e_tile_type_spike) {
				tile_size.x -= 4;
				tile_size.y -= 4;
			}
			if(rect_vs_rect_center(teleport_pos, c_player_size_v, tile_pos + c_tile_size_v * 0.5f, tile_size)) {
				allow_teleport = allow_teleport && tile_type != e_tile_type_block;
				if(tile_type == e_tile_type_breakable && !has_upgrade(e_upgrade_break_tiles)) {
					allow_teleport = false;
				}
			}
			if(!allow_teleport) { break; }
		}
	}
	return allow_teleport;
}

func void draw_atlas(s_v2 pos, s_v2 size, s_v2i index, s_v4 color)
{
	draw_atlas_ex(pos, size, index, color, 0);
}

func void draw_atlas_ex(s_v2 pos, s_v2 size, s_v2i index, s_v4 color, float rotation)
{
	s_instance_data data = zero;
	data.model = m4_translate(v3(pos, 0));
	if(rotation != 0) {
		data.model *= m4_rotate(rotation, v3(0, 0, 1));
	}
	data.model *= m4_scale(v3(size, 1));
	data.color = color;
	data.uv_min.x = index.x * c_atlas_sprite_size / (float)c_atlas_size;
	data.uv_max.x = data.uv_min.x + c_atlas_sprite_size / (float)c_atlas_size;
	data.uv_min.y = index.y * c_atlas_sprite_size / (float)c_atlas_size;
	data.uv_max.y = data.uv_min.y + c_atlas_sprite_size / (float)c_atlas_size;

	add_to_render_group(data, e_shader_flat, e_texture_atlas, e_mesh_quad);
}

func void draw_atlas_topleft(s_v2 pos, s_v2 size, s_v2i index, s_v4 color)
{
	pos += size * 0.5f;
	draw_atlas(pos, size, index, color);
}

func b8 has_upgrade(e_upgrade id)
{
	s_hard_game_data* hard_data = &game->hard_data;
	b8 result = hard_data->upgrade_arr[id] > 0;
	return result;
}

func s_v2i get_tile_atlas_index(e_tile_type tile)
{
	s_v2i result = v2i(-1, -1);
	switch(tile) {
		xcase e_tile_type_block: { result = v2i(0, 0); }
		xcase e_tile_type_breakable: { result = v2i(2, 0); }
		xcase e_tile_type_upgrade_jump: { result = v2i(4, 0); }
		xcase e_tile_type_spike: { result = v2i(6, 0); }
		xcase e_tile_type_upgrade_speed: { result = v2i(8, 0); }
		xcase e_tile_type_upgrade_anti_spike: { result = v2i(10, 0); }
		xcase e_tile_type_upgrade_more_loop_time: { result = v2i(8, 2); }
		xcase e_tile_type_upgrade_less_run_time: { result = v2i(12, 0); }
		xcase e_tile_type_spawn: { result = v2i(0, 2); }
		xcase e_tile_type_goal: { result = v2i(0, 2); }
		xcase e_tile_type_upgrade_teleport: { result = v2i(2, 2); }
		xcase e_tile_type_upgrade_break_tiles: { result = v2i(4, 2); }
		xcase e_tile_type_upgrade_super_speed: { result = v2i(14, 0); }
		xcase e_tile_type_platform: { result = v2i(6, 2); }

		break; invalid_default_case;
	}
	return result;
}

func int get_max_player_jumps()
{
	int result = 1;
	result += game->hard_data.upgrade_arr[e_upgrade_one_more_jump];
	return result;
}

func float get_player_movement_speed()
{
	float result = c_movement_speed;
	result *= 1 + game->hard_data.upgrade_arr[e_upgrade_speed] * c_increased_movement_speed_per_upgrade;
	return result;
}

func int get_update_count_with_upgrades(int update_count)
{
	update_count -= game->hard_data.upgrade_arr[e_upgrade_less_run_time] * c_seconds_saved_per_upgrade * c_updates_per_second;
	update_count = at_least(0, update_count);
	return update_count;
}

func b8 are_we_immune()
{
	b8 result = check_action(game->update_time, game->hard_data.soft_data.used_shield_timestamp, 3.0f);
	return result;
}

func float get_max_loop_time()
{
	float result = c_loop_time + game->hard_data.upgrade_arr[e_upgrade_more_loop_time] * c_extra_loop_time_per_upgrade;
	return result;
}

func b8 tile_is_upgrade(e_tile_type type)
{
	b8 result = false;
	switch(type) {
		xcase e_tile_type_upgrade_jump: { result = true; }
		xcase e_tile_type_upgrade_speed: { result = true; }
		xcase e_tile_type_upgrade_anti_spike: { result = true; }
		xcase e_tile_type_upgrade_less_run_time: { result = true; }
		xcase e_tile_type_upgrade_teleport: { result = true; }
		xcase e_tile_type_upgrade_break_tiles: { result = true; }
		xcase e_tile_type_upgrade_super_speed: { result = true; }
		xcase e_tile_type_upgrade_more_loop_time: { result = true; }
		break; default: { result = false; }
	}
	return result;
}

func void draw_circle(s_v2 pos, float radius, s_v4 color)
{
	s_instance_data data = zero;
	data.model = m4_translate(v3(pos, 0));
	data.model = m4_multiply(data.model, m4_scale(v3(radius * 2, radius * 2, 1)));
	data.color = color;

	add_to_render_group(data, e_shader_circle, e_texture_white, e_mesh_quad);
}

func void draw_light(s_v2 pos, float radius, s_v4 color)
{
	s_instance_data data = zero;
	data.model = m4_translate(v3(pos, 0));
	data.model = m4_multiply(data.model, m4_scale(v3(radius * 2, radius * 2, 1)));
	data.color = color;

	add_to_render_group(data, e_shader_light, e_texture_white, e_mesh_quad);
}

func s_v2 get_upgrade_offset(float interp_dt)
{
	float t = update_time_to_render_time(game->update_time, interp_dt);
	s_v2 result = v2(0.0f, sinf(t * 3.14f) * 3);
	return result;
}

func s_v2 get_player_spawn_pos()
{
	s_v2 result = v2(
		game->map.spawn_tile_index.x * c_tile_size + c_tile_size * 0.5f,
		game->map.spawn_tile_index.y * c_tile_size + c_tile_size * 0.5f
	);
	return result;
}

func void start_restart(s_v2 pos)
{
	s_hard_game_data* hard_data = &game->hard_data;
	s_soft_game_data* soft_data = &game->hard_data.soft_data;
	b8 restarting = soft_data->start_restart_timestamp > 0;
	if(!restarting) {
		soft_data->player_pos_when_restart_started = pos;
		soft_data->start_restart_timestamp = game->update_time;
		play_sound(e_sound_restart);

		if(soft_data->collected_upgrade_this_run) {
			int index = hard_data->ghost_count % c_max_ghosts;
			hard_data->ghost_arr[index] = soft_data->curr_ghost;
			hard_data->ghost_count += 1;
		}
	}
}

func void do_screen_shake(float intensity)
{
	s_soft_game_data* soft_data = &game->hard_data.soft_data;
	soft_data->start_screen_shake_timestamp = game->render_time;
	soft_data->shake_intensity = intensity;
}

func s_m4 get_player_view_matrix(s_v2 player_pos)
{
	s_soft_game_data* soft_data = &game->hard_data.soft_data;
	s_m4 result = m4_translate(v3(-(player_pos.x - c_world_center.x), -(player_pos.y - c_world_center.y), 0.0f));
	if(!game->disable_screen_shake && check_action(game->render_time, soft_data->start_screen_shake_timestamp, c_shake_duration)) {
		s_v3 offset = zero;
		s_time_data data = get_time_data(game->render_time, soft_data->start_screen_shake_timestamp, c_shake_duration);
		float t = at_least(0.0f, data.inv_percent);
		offset.x = randf32_11(&game->rng) * soft_data->shake_intensity * t;
		offset.y = randf32_11(&game->rng) * soft_data->shake_intensity * t;
		result *= m4_translate(offset);
	}
	return result;
}

func s_m4 get_editor_view_matrix()
{
	s_m4 result = m4_scale(v3(game->editor.zoom, game->editor.zoom, 1.0f));
	result *= m4_translate(v3(game->editor.cam_pos * -1, 0));
	return result;
}

func b8 are_we_in_super_speed()
{
	b8 result = check_action(game->update_time, game->hard_data.soft_data.super_speed_timestamp, c_super_speed_duration);
	return result;
}

func void draw_player(s_v2 pos, float angle, s_draw_player dp, s_v4 color)
{
	s_v2 left_foot_offset = v2_rotated(dp.left_foot_offset, angle);
	s_v2 right_foot_offset = v2_rotated(dp.right_foot_offset, angle);
	s_v2 head_offset = v2_rotated(dp.head_offset, angle);
	s_v2 offset = orbit_around_2d(pos, 2, angle);
	s_v2 size = c_player_size_v * v2(1.25f, 1.15f);
	draw_atlas_ex(offset, size, v2i(12, 2), multiply_rgb(color, 0.7f), angle);
	draw_atlas_ex(offset + head_offset, v2(size.x * 1.1f), v2i(10, 2), color, angle);
	draw_atlas_ex(offset + left_foot_offset, v2(size.x), v2i(14, 2), color, angle);
	draw_atlas_ex(offset + right_foot_offset, v2(size.x), v2i(14, 2), color, angle);
}

func s_draw_player get_player_draw_data()
{
	s_draw_player dp = zero;
	dp.head_offset = v2(0.0f, sinf(game->render_time * 10) * 1.5f);
	dp.left_foot_offset = v2(0, 5);
	dp.right_foot_offset = v2(10, 5);
	return dp;
}

func void add_timed_msg(s_len_str str, s_v2 pos)
{
	s_timed_msg msg = zero;
	msg.pos = pos;
	msg.spawn_timestamp = game->update_time;
	str_into_builder(&msg.builder, str);
	game->hard_data.soft_data.timed_msg_arr.add_if_not_full(msg);
}

func void draw_background(s_v2 player_pos, s_m4 ortho)
{
	if(game->hide_background) { return; }

	{
		s_instance_data data = zero;
		data.model = m4_translate(v3(c_world_center, 0));
		data.model = m4_multiply(data.model, m4_scale(v3(c_world_size, 1)));
		data.color = make_color(1);
		add_to_render_group(data, e_shader_background, e_texture_white, e_mesh_quad);
	}

	{
		s_render_flush_data data = make_render_flush_data(zero, v3(player_pos, 0.0f));
		data.projection = ortho;
		data.blend_mode = e_blend_mode_normal;
		data.depth_mode = e_depth_mode_read_no_write;
		render_flush(data, true);
	}
}