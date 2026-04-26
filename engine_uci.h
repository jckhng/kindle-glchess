#ifndef KINDLE_CHESS_ENGINE_UCI_H
#define KINDLE_CHESS_ENGINE_UCI_H

#include <glib.h>

typedef struct _EngineUci EngineUci;
typedef void (*EngineUciMoveCallback)(const char *move, gpointer user_data);

struct _EngineUci {
    gchar *binary;
    GPid pid;
    gint stdin_fd;
    GIOChannel *stdout_channel;
    guint stdout_watch_id;
    GString *buffer;
    GString *moves;
    gint movetime_ms;
    gint skill_level;
    gboolean limit_strength;
    gint elo;
    gboolean ready;
    gboolean waiting_for_move;
    EngineUciMoveCallback move_callback;
    gpointer move_callback_data;
};

void engine_uci_init(EngineUci *engine, const char *binary);
gboolean engine_uci_start(EngineUci *engine, GError **error);
void engine_uci_set_move_callback(EngineUci *engine, EngineUciMoveCallback callback, gpointer user_data);
void engine_uci_set_difficulty(EngineUci *engine, gint preset);
void engine_uci_start_game(EngineUci *engine);
void engine_uci_report_move(EngineUci *engine, const char *move);
void engine_uci_request_move(EngineUci *engine);
void engine_uci_stop(EngineUci *engine);
void engine_uci_clear(EngineUci *engine);

#endif
