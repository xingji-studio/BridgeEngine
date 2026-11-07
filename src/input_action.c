#include "BridgeEngine.h"

#include <string.h>

#define ACTION_MAX			64
#define ACTION_BINDINGS_MAX 8
#define ACTION_NAME_MAX		63

typedef enum {
	ACTION_BIND_KEY	  = 0,
	ACTION_BIND_MOUSE = 1,
} action_bind_kind_t;

typedef struct {
	int kind;
	int code;
} action_binding_t;

typedef struct {
	char			 name[ACTION_NAME_MAX + 1];
	action_binding_t bindings[ACTION_BINDINGS_MAX];
	int				 binding_count;
} action_entry_t;

static action_entry_t g_actions[ACTION_MAX];
static int			  g_action_count;

static action_entry_t *action_find(const char *name)
{
	if (name == NULL || name[0] == '\0') return NULL;
	for (int i = 0; i < g_action_count; i++) {
		if (strcmp(g_actions[i].name, name) == 0) return &g_actions[i];
	}
	return NULL;
}

static action_entry_t *action_find_or_create(const char *name)
{
	action_entry_t *action = action_find(name);
	if (action != NULL) return action;
	if (g_action_count >= ACTION_MAX) return NULL;

	action = &g_actions[g_action_count];
	memset(action, 0, sizeof(*action));
	strncpy(action->name, name, ACTION_NAME_MAX);
	action->name[ACTION_NAME_MAX] = '\0';
	g_action_count++;
	return action;
}

static int action_add_binding(action_entry_t *action, int kind, int code)
{
	for (int i = 0; i < action->binding_count; i++) {
		if (action->bindings[i].kind == kind && action->bindings[i].code == code) {
			return 0; /* already bound: binding twice is harmless */
		}
	}
	if (action->binding_count >= ACTION_BINDINGS_MAX) return -1;

	action->bindings[action->binding_count].kind = kind;
	action->bindings[action->binding_count].code = code;
	action->binding_count++;
	return 0;
}

static int action_query(const char *name, int mode)
{
	action_entry_t *action = action_find(name);
	if (action == NULL) return 0;

	for (int i = 0; i < action->binding_count; i++) {
		const action_binding_t *binding = &action->bindings[i];
		int						active	= 0;
		switch (binding->kind) {
		case ACTION_BIND_KEY:
			if (mode == 0)
				active = bapi_is_key_down((uint8_t)binding->code);
			else if (mode == 1)
				active = bapi_is_key_pressed((uint8_t)binding->code);
			else
				active = bapi_is_key_released((uint8_t)binding->code);
			break;
		case ACTION_BIND_MOUSE:
			if (mode == 0)
				active = bapi_is_mouse_button_down(binding->code);
			else if (mode == 1)
				active = bapi_is_mouse_button_pressed(binding->code);
			else
				active = bapi_is_mouse_button_released(binding->code);
			break;
		default:
			break;
		}
		if (active) return 1;
	}
	return 0;
}

int bapi_input_action_clear(void)
{
	memset(g_actions, 0, sizeof(g_actions));
	g_action_count = 0;
	return 0;
}

int bapi_input_action_bind(const char *name, uint8_t key)
{
	if (name == NULL || name[0] == '\0' || key == 0) return -1;

	action_entry_t *action = action_find_or_create(name);
	if (action == NULL) return -1;
	return action_add_binding(action, ACTION_BIND_KEY, (int)key);
}

int bapi_input_action_bind_mouse(const char *name, int button)
{
	if (name == NULL || name[0] == '\0') return -1;
	if (button < 0 || button >= 31) return -1;

	action_entry_t *action = action_find_or_create(name);
	if (action == NULL) return -1;
	return action_add_binding(action, ACTION_BIND_MOUSE, button);
}

int bapi_input_action_down(const char *name)
{
	return action_query(name, 0);
}

int bapi_input_action_pressed(const char *name)
{
	return action_query(name, 1);
}

int bapi_input_action_released(const char *name)
{
	return action_query(name, 2);
}
