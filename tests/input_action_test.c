#include "BridgeEngine.h"
#include "internal/platform/platform.h"

#include <stdio.h>
#include <string.h>

/* input.c only needs the platform pointer; the mock platform queries no mouse
 * state, so a NULL platform is enough for synthetic events. */
const plat_interface_t *plat_get(void)
{
	return NULL;
}

/* Minimal event accessors (input.c uses only these). init.c is not linked. */
int bapi_event_get_type(const bapi_event_t *event)
{
	return event != NULL ? event->type : BAPI_EVENT_UNKNOWN;
}

uint8_t bapi_event_get_key_code(const bapi_event_t *event)
{
	return event != NULL ? (uint8_t)event->data.key.key : 0;
}

int bapi_event_get_mouse_button(const bapi_event_t *event)
{
	return event != NULL ? event->data.button.button : 0;
}

int bapi_event_get_motion_x(const bapi_event_t *event)
{
	return event != NULL ? (int)event->data.motion.x : 0;
}

int bapi_event_get_motion_y(const bapi_event_t *event)
{
	return event != NULL ? (int)event->data.motion.y : 0;
}

static void send_key(uint8_t key, int down)
{
	bapi_event_t event;
	memset(&event, 0, sizeof(event));
	event.type		   = down ? BAPI_EVENT_KEY_DOWN : BAPI_EVENT_KEY_UP;
	event.data.key.key = key;
	bapi_input_handle_event(&event);
}

static void send_mouse(int button, int down)
{
	bapi_event_t event;
	memset(&event, 0, sizeof(event));
	event.type				 = down ? BAPI_EVENT_MOUSE_BUTTON_DOWN : BAPI_EVENT_MOUSE_BUTTON_UP;
	event.data.button.button = button;
	bapi_input_handle_event(&event);
}

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static int test_bindings_and_states(void)
{
	bapi_input_init();
	CHECK(bapi_input_action_clear() == 0);

	/* Invalid binds. */
	CHECK(bapi_input_action_bind(NULL, KEY_SPACE) == -1);
	CHECK(bapi_input_action_bind("", KEY_SPACE) == -1);
	CHECK(bapi_input_action_bind("jump", 0) == -1);
	CHECK(bapi_input_action_bind_mouse(NULL, 0) == -1);
	CHECK(bapi_input_action_bind_mouse("fire", -1) == -1);
	CHECK(bapi_input_action_bind_mouse("fire", 31) == -1);
	CHECK(bapi_input_action_bind_mouse("fire", 30) == 0);

	/* Multiple keyboard/mouse bindings on one action. */
	CHECK(bapi_input_action_bind("jump", KEY_SPACE) == 0);
	CHECK(bapi_input_action_bind("jump", 'w') == 0);
	CHECK(bapi_input_action_bind_mouse("fire", 0) == 0);
	CHECK(bapi_input_action_bind_mouse("fire", 2) == 0);

	/* Nothing held yet. */
	CHECK(bapi_input_action_down("jump") == 0);
	CHECK(bapi_input_action_pressed("jump") == 0);
	CHECK(bapi_input_action_released("jump") == 0);
	CHECK(bapi_input_action_down("fire") == 0);

	/* First frame: space goes down. */
	send_key(KEY_SPACE, 1);
	CHECK(bapi_input_action_down("jump") == 1);
	CHECK(bapi_input_action_pressed("jump") == 1);
	CHECK(bapi_input_action_released("jump") == 0);
	bapi_input_update();
	CHECK(bapi_input_action_pressed("jump") == 0);
	CHECK(bapi_input_action_down("jump") == 1);

	/* Second binding fires independently. */
	send_key('w', 1);
	CHECK(bapi_input_action_pressed("jump") == 1);
	bapi_input_update();

	/* "any binding" semantics: releasing space counts even while W is held. */
	send_key(KEY_SPACE, 0);
	CHECK(bapi_input_action_released("jump") == 1);
	CHECK(bapi_input_action_down("jump") == 1);
	bapi_input_update();
	CHECK(bapi_input_action_released("jump") == 0);

	send_key('w', 0);
	CHECK(bapi_input_action_released("jump") == 1);
	bapi_input_update();
	CHECK(bapi_input_action_down("jump") == 0);
	CHECK(bapi_input_action_released("jump") == 0);

	/* Mouse binding. */
	send_mouse(0, 1);
	CHECK(bapi_input_action_down("fire") == 1);
	CHECK(bapi_input_action_pressed("fire") == 1);
	bapi_input_update();
	CHECK(bapi_input_action_pressed("fire") == 0);
	CHECK(bapi_input_action_down("fire") == 1);

	/* Second mouse binding also drives the action. */
	send_mouse(0, 0);
	send_mouse(2, 1);
	CHECK(bapi_input_action_released("fire") == 1);
	CHECK(bapi_input_action_down("fire") == 1);
	bapi_input_update();
	CHECK(bapi_input_action_released("fire") == 0);

	send_mouse(2, 0);
	CHECK(bapi_input_action_released("fire") == 1);
	bapi_input_update();
	CHECK(bapi_input_action_down("fire") == 0);

	/* Unknown actions are always false, never invalid. */
	CHECK(bapi_input_action_down("missing") == 0);
	CHECK(bapi_input_action_pressed("missing") == 0);
	CHECK(bapi_input_action_released("missing") == 0);

	/* clear removes every binding. */
	CHECK(bapi_input_action_clear() == 0);
	send_key(KEY_SPACE, 1);
	CHECK(bapi_input_action_down("jump") == 0);
	CHECK(bapi_input_action_bind("jump", 'a') == 0);
	send_key('a', 1);
	CHECK(bapi_input_action_down("jump") == 1);

	bapi_input_cleanup();
	return 0;
}

static int test_capacity(void)
{
	bapi_input_init();
	CHECK(bapi_input_action_clear() == 0);

	/* 64 named actions fit; the 65th is rejected. */
	char name[32];
	for (int i = 0; i < 64; i++) {
		snprintf(name, sizeof(name), "action_%d", i);
		CHECK(bapi_input_action_bind(name, (uint8_t)(i + 1)) == 0);
	}
	CHECK(bapi_input_action_bind("action_overflow", 'a') == -1);

	CHECK(bapi_input_action_clear() == 0);

	/* 8 bindings per action; duplicates do not consume a slot. */
	for (int i = 0; i < 8; i++) {
		CHECK(bapi_input_action_bind("multi", (uint8_t)(i + 1)) == 0);
	}
	CHECK(bapi_input_action_bind("multi", (uint8_t)1) == 0);
	CHECK(bapi_input_action_bind("multi", (uint8_t)9) == -1);

	CHECK(bapi_input_action_clear() == 0);
	bapi_input_cleanup();
	return 0;
}

int main(void)
{
	if (test_bindings_and_states() != 0) return 1;
	if (test_capacity() != 0) return 1;

	printf("input_action_test: ok\n");
	return 0;
}
