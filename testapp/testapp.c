#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#define TAG "Test App"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

typedef enum {
	EventTypeTick,
	EventTypeInput,
} EventType;

typedef struct {
	EventType type;
	InputEvent input;
} GameEvent;

typedef struct {
	FuriMutex* mutex;

	bool ok;
	bool right;
	bool left;
	bool up;
	bool down;
	bool back;

	int32_t x;
	int32_t y;

	char* message;
} GameState;

void drawCallback(Canvas* canvas, void* context) {
	furi_assert(context);

	const GameState* gameState = context;
	if (!gameState) {
		return;
	}

	furi_mutex_acquire(gameState->mutex, FuriWaitForever);

	canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2 + gameState->x, SCREEN_HEIGHT / 2 + gameState->y, AlignCenter, AlignCenter, gameState->message);

	furi_mutex_release(gameState->mutex);
}

void inputCallback(InputEvent* inputEvent, FuriMessageQueue* eventQueue) {
	furi_assert(eventQueue);

	GameEvent event = {
		.type = EventTypeInput,
		.input = *inputEvent
	};
	furi_message_queue_put(eventQueue, &event, FuriWaitForever);
}

void timerCallback(FuriMessageQueue* eventQueue) {
	furi_assert(eventQueue);

	GameEvent gameEvent = {
		.type = EventTypeTick
	};
	furi_message_queue_put(eventQueue, &gameEvent, 0);
}

int32_t testapp() {
	FuriMessageQueue* eventQueue = furi_message_queue_alloc(8, sizeof(GameEvent));

	GameState gameState = {
		.mutex = furi_mutex_alloc(FuriMutexTypeNormal),
		.ok = false,
		.right = false,
		.left = false,
		.up = false,
		.down = false,
		.back = false,
		.x = 0,
		.y = 0
	};

	if (!gameState.mutex) {
		FURI_LOG_E(TAG, "Cannot create mutex\n\r");
		return 255;
	}

	ViewPort* viewPort = view_port_alloc();
	view_port_draw_callback_set(viewPort, drawCallback, &gameState);
	view_port_input_callback_set(viewPort, inputCallback, eventQueue);

	FuriTimer* timer = furi_timer_alloc(timerCallback, FuriTimerTypePeriodic, eventQueue);
	furi_timer_start(timer, furi_kernel_get_tick_frequency() / 12);

	Gui* gui = furi_record_open(RECORD_GUI);
	gui_add_view_port(gui, viewPort, GuiLayerFullscreen);

	GameEvent gameEvent;
	while (!gameState.back) {
		FuriStatus eventStatus = furi_message_queue_get(eventQueue, &gameEvent, 100);
		furi_mutex_acquire(gameState.mutex, FuriWaitForever);

		if (eventStatus == FuriStatusOk) {
			if (gameEvent.type == EventTypeTick) {
				gameState.x += gameState.right - gameState.left;
				gameState.y += gameState.down - gameState.up;
			} else if (gameEvent.type == EventTypeInput) {
				bool keyState = false;
				switch (gameEvent.input.type) {
				case InputTypePress:
				case InputTypeShort:
				case InputTypeLong:
				case InputTypeRepeat:
					keyState = true;
					break;
				case InputTypeRelease:
				case InputTypeMAX:
					keyState = false;
					break;
				}
				switch (gameEvent.input.key) {
				case InputKeyOk:
					gameState.ok = keyState;
					gameState.message = "ok";
					break;
				case InputKeyRight:
					gameState.right = keyState;
					gameState.message = "right";
					break;
				case InputKeyLeft:
					gameState.left = keyState;
					gameState.message = "left";
					break;
				case InputKeyUp:
					gameState.up = keyState;
					gameState.message = "up";
					break;
				case InputKeyDown:
					gameState.down = keyState;
					gameState.message = "down";
					break;
				case InputKeyBack:
					gameState.back = keyState;
					gameState.message = "back";
					break;
				case InputKeyMAX:
					break;
				}
			}
		} else {
			// event timeout?
		}

		view_port_update(viewPort);
		furi_mutex_release(gameState.mutex);
	}

	furi_timer_free(timer);
	furi_message_queue_free(eventQueue);
	view_port_enabled_set(viewPort, false);
	gui_remove_view_port(gui, viewPort);
	view_port_free(viewPort);
	furi_record_close(RECORD_GUI);
	furi_mutex_free(gameState.mutex);

	return 0;
}
