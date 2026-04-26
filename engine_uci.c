#include "engine_uci.h"

#include <string.h>
#include <unistd.h>
#include <signal.h>

static void engine_uci_write_line(EngineUci *engine, const char *line) {
    gchar *message;
    gsize length;

    if (engine->stdin_fd < 0 || line == NULL) {
        return;
    }

    g_printerr("[uci->] %s\n", line);
    message = g_strdup_printf("%s\n", line);
    length = strlen(message);
    if (write(engine->stdin_fd, message, length) < 0) {
        /* Ignore short-lived engine write failures in prototype mode. */
    }
    g_free(message);
}

static void engine_uci_free_gstring(gpointer data) {
    if (data != NULL) {
        g_string_free((GString *) data, TRUE);
    }
}

static void engine_uci_configure(EngineUci *engine) {
    gchar *line;

    engine_uci_write_line(engine, "setoption name Ponder value false");
    engine_uci_write_line(engine, engine->limit_strength ? "setoption name UCI_LimitStrength value true"
                                                         : "setoption name UCI_LimitStrength value false");
    line = g_strdup_printf("setoption name Skill Level value %d", engine->skill_level);
    engine_uci_write_line(engine, line);
    g_free(line);
    if (engine->limit_strength) {
        line = g_strdup_printf("setoption name UCI_Elo value %d", engine->elo);
        engine_uci_write_line(engine, line);
        g_free(line);
    }
    engine_uci_write_line(engine, "isready");
}

static void engine_uci_set_strength_values(EngineUci *engine, gint preset) {
    switch (preset) {
        case 0:
            engine->movetime_ms = 200;
            engine->skill_level = 1;
            engine->limit_strength = TRUE;
            engine->elo = 1350;
            break;
        case 1:
            engine->movetime_ms = 450;
            engine->skill_level = 8;
            engine->limit_strength = TRUE;
            engine->elo = 1700;
            break;
        case 2:
        default:
            engine->movetime_ms = 900;
            engine->skill_level = 20;
            engine->limit_strength = FALSE;
            engine->elo = 2200;
            break;
    }
}

static void engine_uci_process_line(EngineUci *engine, const char *line) {
    g_printerr("[uci<-] %s\n", line);

    if (g_str_has_prefix(line, "uciok")) {
        engine_uci_configure(engine);
        return;
    }

    if (g_str_has_prefix(line, "readyok")) {
        engine->ready = TRUE;
        return;
    }

    if (g_str_has_prefix(line, "bestmove ")) {
        gchar **tokens = g_strsplit(line, " ", 0);
        engine->waiting_for_move = FALSE;
        if (tokens[1] != NULL && engine->move_callback != NULL) {
            g_printerr("[uci!!] bestmove token=%s\n", tokens[1]);
            engine->move_callback(tokens[1], engine->move_callback_data);
        }
        g_strfreev(tokens);
    }
}

static gboolean engine_uci_read_cb(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    EngineUci *engine = (EngineUci *) user_data;
    gchar chunk[512];
    gsize bytes_read = 0;
    GIOStatus status;

    (void) condition;

    status = g_io_channel_read_chars(source, chunk, sizeof(chunk) - 1, &bytes_read, NULL);
    if (status == G_IO_STATUS_EOF || bytes_read == 0) {
        return FALSE;
    }
    if (status != G_IO_STATUS_NORMAL) {
        return TRUE;
    }

    chunk[bytes_read] = '\0';
    g_string_append_len(engine->buffer, chunk, bytes_read);

    while (TRUE) {
        gchar *newline = strchr(engine->buffer->str, '\n');
        gchar *line;
        if (newline == NULL) {
            break;
        }

        *newline = '\0';
        line = g_strdup(engine->buffer->str);
        g_string_erase(engine->buffer, 0, (newline - engine->buffer->str) + 1);
        g_strstrip(line);
        if (line[0] != '\0') {
            engine_uci_process_line(engine, line);
        }
        g_free(line);
    }

    return TRUE;
}

void engine_uci_init(EngineUci *engine, const char *binary) {
    memset(engine, 0, sizeof(*engine));
    engine->binary = g_strdup(binary);
    engine->stdin_fd = -1;
    engine->buffer = g_string_new("");
    engine->moves = g_string_new("");
    engine_uci_set_strength_values(engine, 1);
}

gboolean engine_uci_start(EngineUci *engine, GError **error) {
    gchar *argv[2];
    gint stdout_fd = -1;
    gint stderr_fd = -1;

    argv[0] = engine->binary;
    argv[1] = NULL;

    if (!g_spawn_async_with_pipes(NULL,
                                  argv,
                                  NULL,
                                  G_SPAWN_SEARCH_PATH,
                                  NULL,
                                  NULL,
                                  &engine->pid,
                                  &engine->stdin_fd,
                                  &stdout_fd,
                                  &stderr_fd,
                                  error)) {
        return FALSE;
    }

    if (stderr_fd >= 0) {
        close(stderr_fd);
    }

    engine->stdout_channel = g_io_channel_unix_new(stdout_fd);
    g_io_channel_set_encoding(engine->stdout_channel, NULL, NULL);
    g_io_channel_set_flags(engine->stdout_channel, G_IO_FLAG_NONBLOCK, NULL);
    engine->stdout_watch_id = g_io_add_watch(engine->stdout_channel, G_IO_IN | G_IO_HUP, engine_uci_read_cb, engine);

    engine_uci_write_line(engine, "uci");
    return TRUE;
}

void engine_uci_set_move_callback(EngineUci *engine, EngineUciMoveCallback callback, gpointer user_data) {
    engine->move_callback = callback;
    engine->move_callback_data = user_data;
}

void engine_uci_set_difficulty(EngineUci *engine, gint preset) {
    engine_uci_set_strength_values(engine, preset);
    if (engine->stdin_fd >= 0) {
        engine_uci_configure(engine);
    }
}

void engine_uci_start_game(EngineUci *engine) {
    g_string_assign(engine->moves, "");
    engine_uci_write_line(engine, "ucinewgame");
    engine_uci_write_line(engine, "isready");
}

void engine_uci_report_move(EngineUci *engine, const char *move) {
    if (move == NULL || move[0] == '\0') {
        return;
    }

    if (engine->moves->len > 0) {
        g_string_append_c(engine->moves, ' ');
    }
    g_string_append(engine->moves, move);
}

void engine_uci_request_move(EngineUci *engine) {
    gchar *position;
    gchar *go_line;

    if (!engine->ready || engine->waiting_for_move) {
        return;
    }

    if (engine->moves->len > 0) {
        position = g_strdup_printf("position startpos moves %s", engine->moves->str);
    } else {
        position = g_strdup("position startpos");
    }
    engine_uci_write_line(engine, position);
    g_free(position);

    engine->waiting_for_move = TRUE;
    go_line = g_strdup_printf("go movetime %d", engine->movetime_ms);
    engine_uci_write_line(engine, go_line);
    g_free(go_line);
}

void engine_uci_stop(EngineUci *engine) {
    if (engine->stdin_fd >= 0) {
        engine_uci_write_line(engine, "quit");
        close(engine->stdin_fd);
        engine->stdin_fd = -1;
    }

    if (engine->stdout_watch_id != 0) {
        g_source_remove(engine->stdout_watch_id);
        engine->stdout_watch_id = 0;
    }

    if (engine->stdout_channel != NULL) {
        g_io_channel_shutdown(engine->stdout_channel, TRUE, NULL);
        g_io_channel_unref(engine->stdout_channel);
        engine->stdout_channel = NULL;
    }

    if (engine->pid != 0) {
        kill(engine->pid, SIGTERM);
        g_spawn_close_pid(engine->pid);
        engine->pid = 0;
    }
}

void engine_uci_clear(EngineUci *engine) {
    engine_uci_stop(engine);
    g_clear_pointer(&engine->buffer, engine_uci_free_gstring);
    g_clear_pointer(&engine->moves, engine_uci_free_gstring);
    g_clear_pointer(&engine->binary, g_free);
}
