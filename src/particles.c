/* Fixed-capacity particle pool. Colors are sampled at draw time; velocity,
 * life and size are captured per particle at emit time.
 */
#include "BridgeEngine.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	bapi_vec2_t position;
	bapi_vec2_t velocity;
	float		age;
	float		life;
	float		size;
} bapi_particle_t;

struct bapi_particle_system_internal {
	int					   capacity;
	int					   count;
	bapi_particle_t		  *particles;
	bapi_particle_config_t config;
	float				   emission_accumulator;
	uint32_t			   rng_state;
};

static uint32_t particle_next_u32(bapi_particle_system_t system)
{
	uint32_t x = system->rng_state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	system->rng_state = x;
	return x;
}

/* Uniform float in [0, 1). */
static float particle_next_float(bapi_particle_system_t system)
{
	return (float)(particle_next_u32(system) >> 8) * (1.0f / 16777216.0f);
}

static float particle_lerp(float from, float to, float t)
{
	return from + (to - from) * t;
}

static float particle_random_range(bapi_particle_system_t system, float low, float high)
{
	return particle_lerp(low, high, particle_next_float(system));
}

static uint8_t particle_lerp_channel(uint8_t from, uint8_t to, float t)
{
	float value = (float)from + ((float)to - (float)from) * t;

	if (value <= 0.0f) return 0;
	if (value >= 255.0f) return 255;
	return (uint8_t)(value + 0.5f);
}

static bapi_particle_config_t particle_default_config(void)
{
	bapi_particle_config_t config;

	memset(&config, 0, sizeof(config));
	config.color_start = (bapi_color_t){255, 255, 255, 255};
	config.color_end   = (bapi_color_t){255, 255, 255, 255};
	config.life_min	   = 1.0f;
	config.life_max	   = 1.0f;
	config.size_min	   = 2.0f;
	config.size_max	   = 2.0f;
	return config;
}

bapi_particle_system_t bapi_particle_system_create(int max_particles)
{
	bapi_particle_system_t system;

	if (max_particles <= 0) return NULL;
	system = (bapi_particle_system_t)calloc(1, sizeof(*system));
	if (!system) return NULL;
	system->particles =
		(bapi_particle_t *)calloc((size_t)max_particles, sizeof(*system->particles));
	if (!system->particles) {
		free(system);
		return NULL;
	}
	system->capacity			 = max_particles;
	system->count				 = 0;
	system->config				 = particle_default_config();
	system->emission_accumulator = 0.0f;
	system->rng_state			 = 0x12345678u;
	return system;
}

void bapi_particle_system_destroy(bapi_particle_system_t system)
{
	if (!system) return;
	free(system->particles);
	free(system);
}

void bapi_particle_system_set_config(bapi_particle_system_t		   system,
									 const bapi_particle_config_t *config)
{
	if (!system || !config) return;
	system->config = *config;
}

void bapi_particle_system_set_position(bapi_particle_system_t system, float x, float y)
{
	if (!system) return;
	system->config.position = (bapi_vec2_t){x, y};
}

void bapi_particle_system_emit(bapi_particle_system_t system, int count)
{
	int i;

	if (!system || count <= 0) return;
	if (count > system->capacity - system->count) count = system->capacity - system->count;
	for (i = 0; i < count; i++) {
		bapi_particle_t *particle = &system->particles[system->count++];

		memset(particle, 0, sizeof(*particle));
		particle->position	 = system->config.position;
		particle->velocity.x = particle_random_range(system, system->config.velocity_min.x,
													 system->config.velocity_max.x);
		particle->velocity.y = particle_random_range(system, system->config.velocity_min.y,
													 system->config.velocity_max.y);
		particle->life =
			particle_random_range(system, system->config.life_min, system->config.life_max);
		particle->size =
			particle_random_range(system, system->config.size_min, system->config.size_max);
		particle->age = 0.0f;
	}
}

void bapi_particle_system_update(bapi_particle_system_t system, float delta_time)
{
	int i;

	if (!system) return;
	if (!(delta_time > 0.0f)) delta_time = 0.0f; /* also maps NaN to 0 */

	for (i = 0; i < system->count; i++) {
		bapi_particle_t *particle = &system->particles[i];

		particle->age += delta_time;
		if (particle->age >= particle->life) {
			/* Swap-remove: the last live particle takes this slot. */
			system->particles[i] = system->particles[system->count - 1];
			system->count--;
			i--;
			continue;
		}
		particle->velocity.y += system->config.gravity * delta_time;
		particle->position.x += particle->velocity.x * delta_time;
		particle->position.y += particle->velocity.y * delta_time;
	}

	if (system->config.emission_rate > 0.0f && delta_time > 0.0f) {
		int spawn;

		system->emission_accumulator += system->config.emission_rate * delta_time;
		spawn = (int)system->emission_accumulator;
		if (spawn > 0) {
			system->emission_accumulator -= (float)spawn;
			bapi_particle_system_emit(system, spawn);
		}
	}
}

void bapi_particle_system_draw(bapi_particle_system_t system)
{
	int i;

	if (!system) return;
	for (i = 0; i < system->count; i++) {
		const bapi_particle_t *particle = &system->particles[i];
		bapi_color_t		   color;
		float				   t = 1.0f;

		if (particle->life > 0.0f) t = particle->age / particle->life;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;
		color.r =
			particle_lerp_channel(system->config.color_start.r, system->config.color_end.r, t);
		color.g =
			particle_lerp_channel(system->config.color_start.g, system->config.color_end.g, t);
		color.b =
			particle_lerp_channel(system->config.color_start.b, system->config.color_end.b, t);
		color.a =
			particle_lerp_channel(system->config.color_start.a, system->config.color_end.a, t);
		bapi_fill_rect(particle->position.x, particle->position.y, particle->size, particle->size,
					   color);
	}
}

int bapi_particle_system_count(bapi_particle_system_t system)
{
	if (!system) return 0;
	return system->count;
}
