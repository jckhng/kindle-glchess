#include <glib-object.h>

typedef struct _ChessClock ChessClock;
typedef enum {
    COLOR_WHITE,
    COLOR_BLACK
} Color;

GType chess_clock_get_type(void) {
    return G_TYPE_OBJECT;
}

void chess_clock_set_active_color(ChessClock *self, Color value) {
    (void) self;
    (void) value;
}

void chess_clock_start(ChessClock *self) {
    (void) self;
}
