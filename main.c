#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>

#include "board.h"
#include "engine_uci.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *status_label;
    GtkWidget *undo_button;
    GtkWidget *new_game_button;
    GtkWidget *save_button;
    GtkWidget *load_button;
    GtkWidget *resign_button;
    GtkWidget *draw_button;
    GtkWidget *quit_button;
    GtkWidget *history_view;
    GtkWidget *history_first_button;
    GtkWidget *history_prev_button;
    GtkWidget *history_next_button;
    GtkWidget *history_latest_button;
    GtkWidget *white_clock_label;
    GtkWidget *black_clock_label;
    GtkWidget *mode_combo;
    GtkWidget *difficulty_combo;
    GtkWidget *duration_combo;
    GtkWidget *promotion_combo;
    GtkWidget *theme_combo;
    BoardState board;
    EngineUci engine;
    gboolean engine_enabled;
    gboolean waiting_for_engine;
    char move_history[512][6];
    char san_history[512][32];
    int history_len;
    char promotion_piece;
    int view_ply;
    int initial_time_ms;
    int white_time_ms;
    int black_time_ms;
    int white_time_history[513];
    int black_time_history[513];
    guint clock_timeout_id;
    gint64 last_clock_tick_us;
    int manual_result;
    char manual_reason[96];
} App;

enum {
    APP_RESULT_NONE = 0,
    APP_RESULT_WHITE_WON,
    APP_RESULT_BLACK_WON,
    APP_RESULT_DRAW
};

static void app_refresh_status(App *app, const char *fallback_message);

static int app_current_view_ply(App *app) {
    return app->view_ply >= 0 ? app->view_ply : app->history_len;
}

static gboolean app_is_viewing_latest(App *app) {
    return app_current_view_ply(app) == app->history_len;
}

static const char *app_result_token(App *app) {
    if (app->manual_result == APP_RESULT_WHITE_WON) {
        return "1-0";
    }
    if (app->manual_result == APP_RESULT_BLACK_WON) {
        return "0-1";
    }
    if (app->manual_result == APP_RESULT_DRAW) {
        return "1/2-1/2";
    }

    switch (board_get_game_state(&app->board)) {
        case KINDLE_CHESS_CHECKMATE:
            return board_white_to_move(&app->board) ? "0-1" : "1-0";
        case KINDLE_CHESS_STALEMATE:
        case KINDLE_CHESS_DRAW:
            return "1/2-1/2";
        case KINDLE_CHESS_CHECK:
        case KINDLE_CHESS_ONGOING:
        default:
            return "*";
    }
}

static gboolean app_game_is_over(App *app) {
    if (app->manual_result != APP_RESULT_NONE) {
        return TRUE;
    }
    KindleChessGameState game_state = board_get_game_state(&app->board);
    return game_state == KINDLE_CHESS_CHECKMATE ||
           game_state == KINDLE_CHESS_STALEMATE ||
           game_state == KINDLE_CHESS_DRAW;
}

static void app_update_status(App *app, const char *message) {
    gtk_label_set_text(GTK_LABEL(app->status_label), message);
}

static void app_update_clock_labels(App *app) {
    gchar *white;
    gchar *black;

    if (app->initial_time_ms <= 0) {
        white = g_strdup("White: No limit");
        black = g_strdup("Black: No limit");
    } else {
        white = g_strdup_printf("White: %02d:%02d", app->white_time_ms / 60000, (app->white_time_ms / 1000) % 60);
        black = g_strdup_printf("Black: %02d:%02d", app->black_time_ms / 60000, (app->black_time_ms / 1000) % 60);
    }
    if (app->white_clock_label != NULL) {
        gtk_label_set_text(GTK_LABEL(app->white_clock_label), white);
    }
    if (app->black_clock_label != NULL) {
        gtk_label_set_text(GTK_LABEL(app->black_clock_label), black);
    }
    g_free(white);
    g_free(black);
}

static void app_save_clock_snapshot(App *app) {
    int ply = app->history_len;

    if (ply < 0 || ply > 512) {
        return;
    }
    app->white_time_history[ply] = app->white_time_ms;
    app->black_time_history[ply] = app->black_time_ms;
}

static void app_apply_high_contrast(GtkWidget *widget) {
    GdkColor black = { 0, 0x0000, 0x0000, 0x0000 };
    GdkColor white = { 0, 0xffff, 0xffff, 0xffff };

    gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_ACTIVE, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_text(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_text(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_base(widget, GTK_STATE_SELECTED, &black);
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_bg(widget, GTK_STATE_SELECTED, &black);
}

static void app_install_kindle_style(void) {
    gtk_rc_parse_string(
        "style \"kindle_high_contrast\" {\n"
        "  fg[NORMAL] = \"#000000\"\n"
        "  fg[ACTIVE] = \"#000000\"\n"
        "  fg[PRELIGHT] = \"#ffffff\"\n"
        "  fg[SELECTED] = \"#ffffff\"\n"
        "  text[NORMAL] = \"#000000\"\n"
        "  text[ACTIVE] = \"#000000\"\n"
        "  text[SELECTED] = \"#ffffff\"\n"
        "  base[NORMAL] = \"#ffffff\"\n"
        "  base[ACTIVE] = \"#ffffff\"\n"
        "  base[SELECTED] = \"#000000\"\n"
        "  bg[NORMAL] = \"#ffffff\"\n"
        "  bg[ACTIVE] = \"#ffffff\"\n"
        "  bg[PRELIGHT] = \"#000000\"\n"
        "  bg[SELECTED] = \"#000000\"\n"
        "}\n"
        "gtk-button-images = 0\n"
        "gtk-menu-images = 0\n"
        "class \"GtkComboBox\" style \"kindle_high_contrast\"\n"
        "class \"GtkCellView\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenu\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenuItem\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkComboBox*\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkMenu*\" style \"kindle_high_contrast\"\n"
    );
}

static GtkWidget *app_create_settings_pair(GtkWidget *row, const char *label_text, GtkWidget *combo) {
    GtkWidget *pair_box;
    GtkWidget *label;

    pair_box = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(row), pair_box, TRUE, TRUE, 0);

    label = gtk_label_new(label_text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
    gtk_widget_set_size_request(label, 125, -1);
    gtk_box_pack_start(GTK_BOX(pair_box), label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(pair_box), combo, TRUE, TRUE, 0);

    return combo;
}

static void app_restore_clock_snapshot(App *app, int ply) {
    if (ply < 0) {
        ply = 0;
    }
    if (ply > app->history_len) {
        ply = app->history_len;
    }
    app->white_time_ms = app->white_time_history[ply];
    app->black_time_ms = app->black_time_history[ply];
    app_update_clock_labels(app);
}

static gboolean app_clock_tick(gpointer user_data) {
    App *app = (App *) user_data;
    gint64 now_us;
    gint elapsed_ms;
    gboolean white_to_move;

    if (app->initial_time_ms <= 0 || !app_is_viewing_latest(app) || app->waiting_for_engine || app_game_is_over(app)) {
        app->last_clock_tick_us = g_get_monotonic_time();
        return TRUE;
    }

    now_us = g_get_monotonic_time();
    elapsed_ms = (int) ((now_us - app->last_clock_tick_us) / 1000);
    if (elapsed_ms <= 0) {
        return TRUE;
    }
    app->last_clock_tick_us = now_us;

    white_to_move = board_white_to_move(&app->board);
    if (white_to_move) {
        app->white_time_ms -= elapsed_ms;
        if (app->white_time_ms <= 0) {
            app->white_time_ms = 0;
            app->manual_result = APP_RESULT_BLACK_WON;
            g_strlcpy(app->manual_reason, "White ran out of time.", sizeof(app->manual_reason));
        }
    } else {
        app->black_time_ms -= elapsed_ms;
        if (app->black_time_ms <= 0) {
            app->black_time_ms = 0;
            app->manual_result = APP_RESULT_WHITE_WON;
            g_strlcpy(app->manual_reason, "Black ran out of time.", sizeof(app->manual_reason));
        }
    }

    app_update_clock_labels(app);
    if (app->manual_result != APP_RESULT_NONE) {
        app_refresh_status(app, app->manual_reason);
    }
    return TRUE;
}

static void app_set_clock_duration(App *app, int minutes) {
    if (minutes <= 0) {
        app->initial_time_ms = 0;
        app->white_time_ms = 0;
        app->black_time_ms = 0;
    } else {
        app->initial_time_ms = minutes * 60 * 1000;
        app->white_time_ms = app->initial_time_ms;
        app->black_time_ms = app->initial_time_ms;
    }
    memset(app->white_time_history, 0, sizeof(app->white_time_history));
    memset(app->black_time_history, 0, sizeof(app->black_time_history));
    app_save_clock_snapshot(app);
    app_update_clock_labels(app);
}

static void app_update_history_view(App *app) {
    GtkTextBuffer *buffer;
    GString *text;
    int i;

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->history_view));
    text = g_string_new("");
    for (i = 0; i < app->history_len; i += 2) {
        g_string_append_printf(text, "%d. %s", (i / 2) + 1, app->san_history[i]);
        if (i + 1 < app->history_len) {
            g_string_append_printf(text, "  %s", app->san_history[i + 1]);
        }
        g_string_append_c(text, '\n');
    }
    if (text->len == 0) {
        g_string_append(text, "No moves yet.");
    }
    gtk_text_buffer_set_text(buffer, text->str, -1);
    g_string_free(text, TRUE);
}

static void app_update_controls(App *app) {
    gboolean can_undo = app->history_len > 0 && !app->waiting_for_engine;
    gboolean can_play = !app->waiting_for_engine && !app_game_is_over(app) && app_is_viewing_latest(app);
    gboolean engine_controls = app->engine_enabled;
    int current_ply = app_current_view_ply(app);

    gtk_widget_set_sensitive(app->undo_button, can_undo);
    gtk_widget_set_sensitive(app->drawing_area, can_play);
    if (app->difficulty_combo != NULL) {
        gtk_widget_set_sensitive(app->difficulty_combo, engine_controls);
    }
    if (app->resign_button != NULL) {
        gtk_widget_set_sensitive(app->resign_button, !app_game_is_over(app) && app_is_viewing_latest(app) && app->history_len > 0);
    }
    if (app->draw_button != NULL) {
        gtk_widget_set_sensitive(app->draw_button, !app_game_is_over(app) && app_is_viewing_latest(app) && app->history_len > 0);
    }
    if (app->history_first_button != NULL) {
        gtk_widget_set_sensitive(app->history_first_button, current_ply > 0);
        gtk_widget_set_sensitive(app->history_prev_button, current_ply > 0);
        gtk_widget_set_sensitive(app->history_next_button, current_ply < app->history_len);
        gtk_widget_set_sensitive(app->history_latest_button, current_ply < app->history_len);
    }
}

static void app_push_history(App *app, const char *move, const char *san) {
    if (app->history_len >= 512) {
        return;
    }
    g_strlcpy(app->move_history[app->history_len], move, sizeof(app->move_history[app->history_len]));
    g_strlcpy(app->san_history[app->history_len],
              (san != NULL && san[0] != '\0') ? san : move,
              sizeof(app->san_history[app->history_len]));
    app->history_len++;
    app->view_ply = -1;
    app_save_clock_snapshot(app);
    app_update_history_view(app);
}

static void app_rebuild_engine_history(App *app) {
    int i;

    if (!app->engine_enabled) {
        return;
    }

    engine_uci_start_game(&app->engine);
    for (i = 0; i < app->history_len; i++) {
        engine_uci_report_move(&app->engine, app->move_history[i]);
    }
}

static void app_refresh_status(App *app, const char *fallback_message) {
    KindleChessGameState game_state = board_get_game_state(&app->board);
    gboolean white_to_move = board_white_to_move(&app->board);

    if (app->manual_result != APP_RESULT_NONE) {
        app_update_status(app, app->manual_reason[0] != '\0' ? app->manual_reason : fallback_message);
        app_update_controls(app);
        return;
    }

    if (!app_is_viewing_latest(app)) {
        gchar *message = g_strdup_printf("Viewing move %d of %d. Use Latest to resume play.", app_current_view_ply(app), app->history_len);
        app_update_status(app, message);
        g_free(message);
        app_update_controls(app);
        return;
    }

    switch (game_state) {
        case KINDLE_CHESS_CHECK:
            app_update_status(app, white_to_move ? "White to move and in check." : "Black to move and in check.");
            break;
        case KINDLE_CHESS_CHECKMATE:
            app_update_status(app, white_to_move ? "Checkmate. Black wins. Tap New Game or press u to undo." :
                                                  "Checkmate. White wins. Tap New Game or press u to undo.");
            break;
        case KINDLE_CHESS_STALEMATE:
            app_update_status(app, "Stalemate. Tap New Game or press u to undo.");
            break;
        case KINDLE_CHESS_DRAW:
            app_update_status(app, "Draw. Tap New Game or press u to undo.");
            break;
        case KINDLE_CHESS_ONGOING:
        default:
            app_update_status(app, fallback_message);
            break;
    }
    app_update_controls(app);
}

static void app_reset_game(App *app) {
    board_state_reset(&app->board);
    app->history_len = 0;
    app->view_ply = -1;
    app->manual_result = APP_RESULT_NONE;
    app->manual_reason[0] = '\0';
    memset(app->move_history, 0, sizeof(app->move_history));
    memset(app->san_history, 0, sizeof(app->san_history));
    app_set_clock_duration(app, app->initial_time_ms > 0 ? app->initial_time_ms / 60000 : 0);
    app_update_history_view(app);
    if (app->engine_enabled) {
        engine_uci_start_game(&app->engine);
    }
    app->waiting_for_engine = FALSE;
    app->last_clock_tick_us = g_get_monotonic_time();
    app_update_status(app, app->engine_enabled ? "New game started." : "New two-player game started.");
    app_update_controls(app);
    gtk_widget_queue_draw(app->drawing_area);
}

static void app_rebuild_board_from_history(App *app, int ply) {
    int i;

    board_state_reset(&app->board);
    for (i = 0; i < ply; i++) {
        char san[32];

        if (!board_apply_uci_move(&app->board, app->move_history[i], san)) {
            gchar *message = g_strdup_printf("Failed to rebuild move %s", app->move_history[i]);
            app_update_status(app, message);
            g_free(message);
            break;
        }
    }
}

static void app_set_view_ply(App *app, int ply) {
    if (ply < 0) {
        ply = 0;
    }
    if (ply > app->history_len) {
        ply = app->history_len;
    }
    app->view_ply = (ply == app->history_len) ? -1 : ply;
    app_rebuild_board_from_history(app, ply);
    app_restore_clock_snapshot(app, ply);
    app->last_clock_tick_us = g_get_monotonic_time();
    gtk_widget_queue_draw(app->drawing_area);
    app_refresh_status(app, "History updated.");
}

static void app_undo_last_turn(App *app) {
    int removed;

    if (app->history_len <= 0) {
        app_refresh_status(app, "Nothing to undo.");
        return;
    }

    removed = app->engine_enabled ? 2 : 1;
    if (removed > app->history_len) {
        removed = app->history_len;
    }

    app->history_len -= removed;
    if (app->history_len < 0) {
        app->history_len = 0;
    }
    app->manual_result = APP_RESULT_NONE;
    app->manual_reason[0] = '\0';
    memset(&app->move_history[app->history_len], 0, sizeof(app->move_history) - ((size_t) app->history_len * sizeof(app->move_history[0])));
    memset(&app->san_history[app->history_len], 0, sizeof(app->san_history) - ((size_t) app->history_len * sizeof(app->san_history[0])));
    app->waiting_for_engine = FALSE;
    app_set_view_ply(app, app->history_len);
    app_rebuild_engine_history(app);
    app_update_history_view(app);
    app_refresh_status(app, "Undid last turn.");
}

static gboolean on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
    App *app = (App *) user_data;
    cairo_t *cr;

    (void) event;

    cr = gdk_cairo_create(widget->window);
    board_draw(&app->board, widget, cr);
    cairo_destroy(cr);
    return FALSE;
}

static void on_engine_move(const char *move, gpointer user_data) {
    App *app = (App *) user_data;
    char san[32];

    app->waiting_for_engine = FALSE;
    if (board_apply_uci_move(&app->board, move, san)) {
        app_push_history(app, move, san);
        engine_uci_report_move(&app->engine, move);
        app_refresh_status(app, "Engine moved. Your turn.");
        gtk_widget_queue_draw(app->drawing_area);
    } else {
        gchar *message;
        app->board.selected_row = -1;
        app->board.selected_col = -1;
        message = g_strdup_printf("Engine move failed: %s. Press r to resync.", move);
        app_update_status(app, message);
        g_free(message);
    }
    app_update_controls(app);
}

static gchar *app_build_pgn(App *app) {
    GString *pgn;
    int i;

    pgn = g_string_new("");
    g_string_append(pgn, "[Event \"Kindle GlChess\"]\n");
    g_string_append(pgn, "[Site \"Kindle\"]\n");
    g_string_append_printf(pgn, "[Result \"%s\"]\n\n", app_result_token(app));
    for (i = 0; i < app->history_len; i += 2) {
        g_string_append_printf(pgn, "%d. %s", (i / 2) + 1, app->san_history[i]);
        if (i + 1 < app->history_len) {
            g_string_append_printf(pgn, " %s", app->san_history[i + 1]);
        }
        g_string_append_c(pgn, ' ');
    }
    g_string_append(pgn, app_result_token(app));
    g_string_append_c(pgn, '\n');
    return g_string_free(pgn, FALSE);
}

static gchar *app_strip_pgn_noise(const gchar *content) {
    GString *clean = g_string_new("");
    gboolean in_brace = FALSE;
    gboolean in_semicolon = FALSE;
    gboolean in_tag = FALSE;
    const gchar *p;

    for (p = content; *p != '\0'; p++) {
        if (in_semicolon) {
            if (*p == '\n') {
                in_semicolon = FALSE;
                g_string_append_c(clean, ' ');
            }
            continue;
        }
        if (in_brace) {
            if (*p == '}') {
                in_brace = FALSE;
            }
            continue;
        }
        if (in_tag) {
            if (*p == '\n') {
                in_tag = FALSE;
                g_string_append_c(clean, ' ');
            }
            continue;
        }
        if (*p == ';') {
            in_semicolon = TRUE;
            continue;
        }
        if (*p == '{') {
            in_brace = TRUE;
            continue;
        }
        if (*p == '[' && (p == content || *(p - 1) == '\n')) {
            in_tag = TRUE;
            continue;
        }
        g_string_append_c(clean, *p);
    }

    return g_string_free(clean, FALSE);
}

static gboolean app_token_is_move_number(const gchar *token) {
    const gchar *p = token;
    gboolean saw_digit = FALSE;

    while (g_ascii_isdigit(*p)) {
        saw_digit = TRUE;
        p++;
    }
    if (!saw_digit) {
        return FALSE;
    }
    while (*p == '.') {
        p++;
    }
    return *p == '\0';
}

static gboolean app_load_pgn_file(App *app, const gchar *filename, GError **error) {
    gchar *content = NULL;
    gchar *clean = NULL;
    gchar **tokens = NULL;
    int i;

    if (!g_file_get_contents(filename, &content, NULL, error)) {
        return FALSE;
    }

    app_reset_game(app);
    app->waiting_for_engine = FALSE;
    app->manual_result = APP_RESULT_NONE;
    app->manual_reason[0] = '\0';
    clean = app_strip_pgn_noise(content);
    tokens = g_strsplit_set(clean, " \t\r\n", -1);
    app->history_len = 0;
    memset(app->move_history, 0, sizeof(app->move_history));
    memset(app->san_history, 0, sizeof(app->san_history));

    for (i = 0; tokens[i] != NULL; i++) {
        char san[32];
        char uci[6];
        const gchar *token = tokens[i];

        if (token[0] == '\0' || app_token_is_move_number(token)) {
            continue;
        }
        if (g_str_equal(token, "1-0") || g_str_equal(token, "0-1") || g_str_equal(token, "1/2-1/2") || g_str_equal(token, "*")) {
            break;
        }
        if (!kindle_chess_backend_apply_move_text(&app->board.backend, token, san, uci)) {
            g_set_error(error, g_quark_from_static_string("kindle-chess-pgn"), 1, "Failed to load move: %s", token);
            g_strfreev(tokens);
            g_free(clean);
            g_free(content);
            return FALSE;
        }
        app_push_history(app, uci, san);
    }

    app_set_view_ply(app, app->history_len);
    app_rebuild_engine_history(app);
    g_strfreev(tokens);
    g_free(clean);
    g_free(content);
    return TRUE;
}

static gboolean on_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    App *app = (App *) user_data;
    char move[6];
    char san[32];

    if (event->button != 1) {
        return FALSE;
    }

    if (app->waiting_for_engine) {
        app_update_status(app, "Waiting for engine...");
        return TRUE;
    }

    if (app_game_is_over(app)) {
        app_refresh_status(app, "Game over. Tap New Game or press u to undo.");
        return TRUE;
    }

    if (board_handle_click(&app->board, widget, event->x, event->y, app->promotion_piece, move, san)) {
        if (move[0] != '\0') {
            app_push_history(app, move, san);
            if (app_game_is_over(app)) {
                app_refresh_status(app, "Game finished.");
            } else if (app->engine_enabled) {
                engine_uci_report_move(&app->engine, move);
                app->waiting_for_engine = TRUE;
                app_update_status(app, "Your move sent. Engine thinking...");
                engine_uci_request_move(&app->engine);
            } else {
                app_refresh_status(app, "Moved piece.");
            }
        } else {
            app_update_status(app, "Tap a piece, then tap a destination.");
        }
        gtk_widget_queue_draw(widget);
    }

    app_update_controls(app);
    return TRUE;
}

static void on_difficulty_changed(GtkComboBox *combo, gpointer user_data) {
    App *app = (App *) user_data;
    gint active = gtk_combo_box_get_active(combo);

    if (active < 0 || !app->engine_enabled) {
        return;
    }
    engine_uci_set_difficulty(&app->engine, active);
    app_refresh_status(app, "Engine difficulty updated.");
}

static void on_duration_changed(GtkComboBox *combo, gpointer user_data) {
    App *app = (App *) user_data;
    gint active = gtk_combo_box_get_active(combo);
    int minutes = 5;

    if (active < 0) {
        return;
    }
    switch (active) {
        case 0: minutes = 0; break;
        case 1: minutes = 1; break;
        case 2: minutes = 5; break;
        case 3: minutes = 10; break;
        case 4: minutes = 15; break;
        default: break;
    }
    app_set_clock_duration(app, minutes);
    app_update_status(app, "Clock duration updated.");
}

static void on_mode_changed(GtkComboBox *combo, gpointer user_data) {
    App *app = (App *) user_data;
    gint active = gtk_combo_box_get_active(combo);

    if (active < 0) {
        return;
    }
    app->engine_enabled = (active != 0);
    app->waiting_for_engine = FALSE;
    app->view_ply = -1;

    if (app->engine_enabled) {
        engine_uci_start_game(&app->engine);
        app_refresh_status(app, "Human vs Engine mode.");
    } else {
        app_refresh_status(app, "Human vs Human mode.");
    }

    app_update_controls(app);
}

static void on_promotion_changed(GtkComboBox *combo, gpointer user_data) {
    App *app = (App *) user_data;
    gint active = gtk_combo_box_get_active(combo);

    switch (active) {
        case 1:
            app->promotion_piece = 'r';
            break;
        case 2:
            app->promotion_piece = 'b';
            break;
        case 3:
            app->promotion_piece = 'n';
            break;
        case 0:
        default:
            app->promotion_piece = 'q';
            break;
    }
    app_update_status(app, "Promotion choice updated.");
}

static void on_theme_changed(GtkComboBox *combo, gpointer user_data) {
    App *app = (App *) user_data;
    gint active = gtk_combo_box_get_active(combo);

    if (active < 0) {
        return;
    }
    board_set_piece_theme(&app->board, active == 1 ? "fancy" : "simple");
    gtk_widget_queue_draw(app->drawing_area);
}

static void on_history_first_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    (void) widget;
    app_set_view_ply(app, 0);
}

static void on_history_prev_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    (void) widget;
    app_set_view_ply(app, app_current_view_ply(app) - 1);
}

static void on_history_next_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    (void) widget;
    app_set_view_ply(app, app_current_view_ply(app) + 1);
}

static void on_history_latest_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    (void) widget;
    app_set_view_ply(app, app->history_len);
}

static void on_resign_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    (void) widget;

    if (app_game_is_over(app)) {
        return;
    }
    if (board_white_to_move(&app->board)) {
        app->manual_result = APP_RESULT_BLACK_WON;
        g_strlcpy(app->manual_reason, "White resigned. Black wins.", sizeof(app->manual_reason));
    } else {
        app->manual_result = APP_RESULT_WHITE_WON;
        g_strlcpy(app->manual_reason, "Black resigned. White wins.", sizeof(app->manual_reason));
    }
    app_refresh_status(app, app->manual_reason);
}

static void on_draw_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    (void) widget;

    if (app_game_is_over(app)) {
        return;
    }
    app->manual_result = APP_RESULT_DRAW;
    g_strlcpy(app->manual_reason, "Draw agreed.", sizeof(app->manual_reason));
    app_refresh_status(app, app->manual_reason);
}

static void on_save_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    const gchar *filename = "/mnt/us/documents/kindle-glchess.pgn";
    gchar *pgn;
    GError *error = NULL;

    (void) widget;

    pgn = app_build_pgn(app);
    if (g_file_set_contents(filename, pgn, -1, &error)) {
        app_update_status(app, "Saved to /mnt/us/documents/kindle-glchess.pgn");
    } else {
        gchar *message = g_strdup_printf("Save failed: %s", error->message);
        app_update_status(app, message);
        g_free(message);
        g_clear_error(&error);
    }
    g_free(pgn);
}

static void on_load_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;
    const gchar *filename = "/mnt/us/documents/kindle-glchess.pgn";
    GError *error = NULL;

    (void) widget;

    if (app_load_pgn_file(app, filename, &error)) {
        app_update_status(app, "Loaded /mnt/us/documents/kindle-glchess.pgn");
    } else {
        gchar *message = g_strdup_printf("Load failed: %s", error != NULL ? error->message : "unknown error");
        app_update_status(app, message);
        g_free(message);
        g_clear_error(&error);
    }
}

static void on_new_game_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;

    (void) widget;
    app_reset_game(app);
}

static void on_undo_clicked(GtkWidget *widget, gpointer user_data) {
    App *app = (App *) user_data;

    (void) widget;
    app_undo_last_turn(app);
}

static void on_quit_clicked(GtkWidget *widget, gpointer user_data) {
    (void) widget;
    (void) user_data;
    gtk_main_quit();
}

static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    App *app = (App *) user_data;

    (void) widget;

    if (event->keyval == GDK_Escape || event->keyval == GDK_q) {
        gtk_main_quit();
        return TRUE;
    }

    if (event->keyval == GDK_r) {
        app_reset_game(app);
        return TRUE;
    }

    if (event->keyval == GDK_u) {
        app_undo_last_turn(app);
        return TRUE;
    }

    return FALSE;
}

int main(int argc, char **argv) {
    App app;
    GtkWidget *vbox;
    GtkWidget *content_hbox;
    GtkWidget *sidebar_vbox;
    GtkWidget *controls_box;
    GtkWidget *settings_box_top;
    GtkWidget *settings_box_middle;
    GtkWidget *settings_box_bottom;
    GtkWidget *board_frame;
    GtkWidget *history_frame;
    GtkWidget *history_scroll;
    GtkWidget *history_nav_box;
    GtkWidget *label;
    const gchar *engine_path;
    GError *error = NULL;

    gtk_init(&argc, &argv);
    app_install_kindle_style();
    memset(&app, 0, sizeof(app));

    board_state_init(&app.board);
    app.promotion_piece = 'q';
    app.view_ply = -1;
    app.initial_time_ms = 5 * 60 * 1000;
    app_set_clock_duration(&app, 5);

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "L:A_N:application_ID:kindlechess_PC:N_O:URL");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 600, 800);
    gtk_container_set_border_width(GTK_CONTAINER(app.window), 8);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(app.window), vbox);

    label = gtk_label_new("Kindle GlChess");
    gtk_misc_set_alignment(GTK_MISC(label), 0.5f, 0.5f);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    app.status_label = gtk_label_new("Tap a white piece, then tap a destination. Press r to reset.");
    gtk_box_pack_start(GTK_BOX(vbox), app.status_label, FALSE, FALSE, 0);

    controls_box = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), controls_box, FALSE, FALSE, 0);

    app.new_game_button = gtk_button_new_with_label("New Game");
    gtk_box_pack_start(GTK_BOX(controls_box), app.new_game_button, FALSE, FALSE, 0);

    app.undo_button = gtk_button_new_with_label("Undo");
    gtk_box_pack_start(GTK_BOX(controls_box), app.undo_button, FALSE, FALSE, 0);

    app.save_button = gtk_button_new_with_label("Save");
    gtk_box_pack_start(GTK_BOX(controls_box), app.save_button, FALSE, FALSE, 0);

    app.load_button = gtk_button_new_with_label("Load");
    gtk_box_pack_start(GTK_BOX(controls_box), app.load_button, FALSE, FALSE, 0);

    app.resign_button = gtk_button_new_with_label("Resign");
    gtk_box_pack_start(GTK_BOX(controls_box), app.resign_button, FALSE, FALSE, 0);

    app.draw_button = gtk_button_new_with_label("Draw");
    gtk_box_pack_start(GTK_BOX(controls_box), app.draw_button, FALSE, FALSE, 0);

    app.quit_button = gtk_button_new_with_label("Quit");
    gtk_box_pack_start(GTK_BOX(controls_box), app.quit_button, FALSE, FALSE, 0);

    settings_box_top = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), settings_box_top, FALSE, FALSE, 0);

    app.mode_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.mode_combo), "2 Players");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.mode_combo), "Vs Engine");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.mode_combo), 1);
    app_apply_high_contrast(app.mode_combo);
    app_create_settings_pair(settings_box_top, "Mode", app.mode_combo);

    app.difficulty_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.difficulty_combo), "Easy");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.difficulty_combo), "Medium");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.difficulty_combo), "Hard");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.difficulty_combo), 1);
    app_apply_high_contrast(app.difficulty_combo);
    app_create_settings_pair(settings_box_top, "Level", app.difficulty_combo);

    settings_box_middle = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), settings_box_middle, FALSE, FALSE, 0);

    app.duration_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.duration_combo), "No limit");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.duration_combo), "1 min");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.duration_combo), "5 min");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.duration_combo), "10 min");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.duration_combo), "15 min");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.duration_combo), 2);
    app_apply_high_contrast(app.duration_combo);
    app_create_settings_pair(settings_box_middle, "Time", app.duration_combo);

    app.promotion_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.promotion_combo), "Queen");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.promotion_combo), "Rook");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.promotion_combo), "Bishop");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.promotion_combo), "Knight");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.promotion_combo), 0);
    app_apply_high_contrast(app.promotion_combo);
    app_create_settings_pair(settings_box_middle, "Promotion", app.promotion_combo);

    settings_box_bottom = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), settings_box_bottom, FALSE, FALSE, 0);

    app.theme_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.theme_combo), "Simple");
    gtk_combo_box_append_text(GTK_COMBO_BOX(app.theme_combo), "Fancy");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.theme_combo), 0);
    app_apply_high_contrast(app.theme_combo);
    app_create_settings_pair(settings_box_bottom, "Theme", app.theme_combo);

    content_hbox = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), content_hbox, TRUE, TRUE, 0);

    board_frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(content_hbox), board_frame, TRUE, TRUE, 0);

    app.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.drawing_area, 400, 400);
    gtk_container_add(GTK_CONTAINER(board_frame), app.drawing_area);

    sidebar_vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(content_hbox), sidebar_vbox, FALSE, TRUE, 0);

    app.white_clock_label = gtk_label_new("White: 05:00");
    gtk_box_pack_start(GTK_BOX(sidebar_vbox), app.white_clock_label, FALSE, FALSE, 0);
    app.black_clock_label = gtk_label_new("Black: 05:00");
    gtk_box_pack_start(GTK_BOX(sidebar_vbox), app.black_clock_label, FALSE, FALSE, 0);

    history_frame = gtk_frame_new("Moves");
    gtk_box_pack_start(GTK_BOX(sidebar_vbox), history_frame, TRUE, TRUE, 0);

    history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(history_scroll), GTK_SHADOW_IN);
    gtk_widget_set_size_request(history_scroll, 220, 320);
    gtk_container_add(GTK_CONTAINER(history_frame), history_scroll);

    app.history_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.history_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.history_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.history_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(history_scroll), app.history_view);

    history_nav_box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar_vbox), history_nav_box, FALSE, FALSE, 0);

    app.history_first_button = gtk_button_new_with_label("|<");
    gtk_box_pack_start(GTK_BOX(history_nav_box), app.history_first_button, TRUE, TRUE, 0);
    app.history_prev_button = gtk_button_new_with_label("<");
    gtk_box_pack_start(GTK_BOX(history_nav_box), app.history_prev_button, TRUE, TRUE, 0);
    app.history_next_button = gtk_button_new_with_label(">");
    gtk_box_pack_start(GTK_BOX(history_nav_box), app.history_next_button, TRUE, TRUE, 0);
    app.history_latest_button = gtk_button_new_with_label(">|");
    gtk_box_pack_start(GTK_BOX(history_nav_box), app.history_latest_button, TRUE, TRUE, 0);

    gtk_widget_add_events(app.drawing_area, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app.window, "key-press-event", G_CALLBACK(on_key_press_event), &app);
    g_signal_connect(app.drawing_area, "expose-event", G_CALLBACK(on_expose_event), &app);
    g_signal_connect(app.drawing_area, "button-press-event", G_CALLBACK(on_button_press_event), &app);
    g_signal_connect(app.new_game_button, "clicked", G_CALLBACK(on_new_game_clicked), &app);
    g_signal_connect(app.undo_button, "clicked", G_CALLBACK(on_undo_clicked), &app);
    g_signal_connect(app.save_button, "clicked", G_CALLBACK(on_save_clicked), &app);
    g_signal_connect(app.load_button, "clicked", G_CALLBACK(on_load_clicked), &app);
    g_signal_connect(app.resign_button, "clicked", G_CALLBACK(on_resign_clicked), &app);
    g_signal_connect(app.draw_button, "clicked", G_CALLBACK(on_draw_clicked), &app);
    g_signal_connect(app.quit_button, "clicked", G_CALLBACK(on_quit_clicked), &app);
    g_signal_connect(app.history_first_button, "clicked", G_CALLBACK(on_history_first_clicked), &app);
    g_signal_connect(app.history_prev_button, "clicked", G_CALLBACK(on_history_prev_clicked), &app);
    g_signal_connect(app.history_next_button, "clicked", G_CALLBACK(on_history_next_clicked), &app);
    g_signal_connect(app.history_latest_button, "clicked", G_CALLBACK(on_history_latest_clicked), &app);
    g_signal_connect(app.mode_combo, "changed", G_CALLBACK(on_mode_changed), &app);
    g_signal_connect(app.difficulty_combo, "changed", G_CALLBACK(on_difficulty_changed), &app);
    g_signal_connect(app.duration_combo, "changed", G_CALLBACK(on_duration_changed), &app);
    g_signal_connect(app.promotion_combo, "changed", G_CALLBACK(on_promotion_changed), &app);
    g_signal_connect(app.theme_combo, "changed", G_CALLBACK(on_theme_changed), &app);

    engine_path = g_getenv("KINDLE_CHESS_ENGINE");
    if (engine_path == NULL || engine_path[0] == '\0') {
        engine_path = "/mnt/us/extensions/gnomegames/bin/stockfish.sh";
    }
    engine_uci_init(&app.engine, engine_path);
    engine_uci_set_move_callback(&app.engine, on_engine_move, &app);
    if (engine_uci_start(&app.engine, &error)) {
        app.engine_enabled = TRUE;
        engine_uci_start_game(&app.engine);
    } else {
        gchar *message = g_strdup_printf("Engine disabled: %s", error != NULL ? error->message : "start failed");
        app_update_status(&app, message);
        g_free(message);
        g_clear_error(&error);
    }

    app_update_history_view(&app);
    app_update_controls(&app);
    app_update_clock_labels(&app);
    app.clock_timeout_id = g_timeout_add(200, app_clock_tick, &app);
    app.last_clock_tick_us = g_get_monotonic_time();
    gtk_widget_show_all(app.window);
    gtk_window_present(GTK_WINDOW(app.window));

    gtk_main();
    if (app.clock_timeout_id != 0) {
        g_source_remove(app.clock_timeout_id);
    }
    board_state_clear(&app.board);
    engine_uci_clear(&app.engine);
    return 0;
}
