#include "board.h"

#include <ctype.h>
#include <string.h>

static const char *board_piece_filename(char piece) {
    switch (piece) {
        case 'P': return "whitePawn.svg";
        case 'R': return "whiteRook.svg";
        case 'N': return "whiteKnight.svg";
        case 'B': return "whiteBishop.svg";
        case 'Q': return "whiteQueen.svg";
        case 'K': return "whiteKing.svg";
        case 'p': return "blackPawn.svg";
        case 'r': return "blackRook.svg";
        case 'n': return "blackKnight.svg";
        case 'b': return "blackBishop.svg";
        case 'q': return "blackQueen.svg";
        case 'k': return "blackKing.svg";
        default: return NULL;
    }
}

static int board_piece_index(char piece) {
    switch (piece) {
        case 'P': return 0;
        case 'R': return 1;
        case 'N': return 2;
        case 'B': return 3;
        case 'Q': return 4;
        case 'K': return 5;
        case 'p': return 6;
        case 'r': return 7;
        case 'n': return 8;
        case 'b': return 9;
        case 'q': return 10;
        case 'k': return 11;
        default: return -1;
    }
}

static const char *board_find_pieces_root(void) {
    static const char *paths[] = {
        "/mnt/us/extensions/kindle-chess/share/glchess/pieces",
        "/mnt/us/extensions/gnomegames/share/glchess/pieces",
        "/home/jack/GnomeGames4Kindle/glchess/data/pieces",
        "../glchess/data/pieces",
        NULL
    };
    const char *env = g_getenv("KINDLE_CHESS_PIECES_DIR");
    int i;

    if (env != NULL && g_file_test(env, G_FILE_TEST_IS_DIR)) {
        return env;
    }
    for (i = 0; paths[i] != NULL; i++) {
        if (g_file_test(paths[i], G_FILE_TEST_IS_DIR)) {
            return paths[i];
        }
    }
    return NULL;
}

static void board_clear_piece_handles(BoardState *state) {
    int i;

    for (i = 0; i < 12; i++) {
        if (state->piece_handles[i] != NULL) {
            g_object_unref(state->piece_handles[i]);
            state->piece_handles[i] = NULL;
        }
    }
}

static void board_load_piece_handles(BoardState *state) {
    static const char pieces[] = {'P', 'R', 'N', 'B', 'Q', 'K', 'p', 'r', 'n', 'b', 'q', 'k'};
    int i;

    board_clear_piece_handles(state);
    if (state->pieces_root == NULL || state->piece_theme[0] == '\0') {
        return;
    }

    for (i = 0; i < 12; i++) {
        const char *name = board_piece_filename(pieces[i]);
        gchar *path = g_build_filename(state->pieces_root, state->piece_theme, name, NULL);
        GError *error = NULL;

        state->piece_handles[i] = rsvg_handle_new_from_file(path, &error);
        if (error != NULL) {
            g_error_free(error);
            state->piece_handles[i] = NULL;
        }
        g_free(path);
    }
}

static gboolean board_get_metrics(GtkWidget *widget,
                                  gdouble *origin_x,
                                  gdouble *origin_y,
                                  gdouble *square_size,
                                  gdouble *board_size) {
    GtkAllocation allocation;
    gdouble size;

    gtk_widget_get_allocation(widget, &allocation);
    size = allocation.width < allocation.height ? allocation.width : allocation.height;
    size -= 24.0;
    if (size <= 0.0) {
        return FALSE;
    }

    *board_size = size;
    *square_size = size / 8.0;
    *origin_x = (allocation.width - size) / 2.0;
    *origin_y = (allocation.height - size) / 2.0;
    return TRUE;
}

void board_state_init(BoardState *state) {
    memset(state, 0, sizeof(*state));
    state->selected_row = -1;
    state->selected_col = -1;
    state->pieces_root = g_strdup(board_find_pieces_root());
    kindle_chess_backend_init(&state->backend);
    board_set_piece_theme(state, "simple");
}

void board_state_clear(BoardState *state) {
    board_clear_piece_handles(state);
    g_clear_pointer(&state->pieces_root, g_free);
    kindle_chess_backend_clear(&state->backend);
}

void board_state_reset(BoardState *state) {
    state->selected_row = -1;
    state->selected_col = -1;
    kindle_chess_backend_reset(&state->backend);
}

void board_set_piece_theme(BoardState *state, const char *theme_name) {
    g_strlcpy(state->piece_theme, theme_name != NULL ? theme_name : "simple", sizeof(state->piece_theme));
    board_load_piece_handles(state);
}

static void board_draw_piece(cairo_t *cr, char piece, gdouble x, gdouble y, gdouble square_size) {
    char kind = (char) toupper((unsigned char) piece);
    gboolean white = isupper((unsigned char) piece) != 0;
    gdouble cx = x + (square_size / 2.0);
    gdouble cy = y + (square_size / 2.0);
    gdouble s = square_size;

    cairo_save(cr);
    cairo_set_line_width(cr, s * 0.045);
    if (white) {
        cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
    } else {
        cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
    }

    switch (kind) {
        case 'P':
            cairo_arc(cr, cx, y + s * 0.34, s * 0.12, 0.0, 6.28318);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.12, y + s * 0.44, s * 0.24, s * 0.18);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.18, y + s * 0.64, s * 0.36, s * 0.08);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            break;
        case 'R':
            cairo_rectangle(cr, cx - s * 0.18, y + s * 0.26, s * 0.36, s * 0.38);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.22, y + s * 0.20, s * 0.08, s * 0.06);
            cairo_rectangle(cr, cx - s * 0.04, y + s * 0.20, s * 0.08, s * 0.06);
            cairo_rectangle(cr, cx + s * 0.14, y + s * 0.20, s * 0.08, s * 0.06);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.24, y + s * 0.66, s * 0.48, s * 0.08);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            break;
        case 'N':
            cairo_move_to(cr, cx - s * 0.18, y + s * 0.68);
            cairo_line_to(cr, cx - s * 0.10, y + s * 0.28);
            cairo_line_to(cr, cx + s * 0.14, y + s * 0.26);
            cairo_line_to(cr, cx + s * 0.04, y + s * 0.40);
            cairo_line_to(cr, cx + s * 0.18, y + s * 0.52);
            cairo_line_to(cr, cx + s * 0.10, y + s * 0.68);
            cairo_close_path(cr);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_arc(cr, cx + s * 0.04, y + s * 0.36, s * 0.02, 0.0, 6.28318);
            if (white) {
                cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
            } else {
                cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
            }
            cairo_fill(cr);
            break;
        case 'B':
            cairo_arc(cr, cx, y + s * 0.28, s * 0.09, 0.0, 6.28318);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_move_to(cr, cx, y + s * 0.34);
            cairo_line_to(cr, cx - s * 0.15, y + s * 0.62);
            cairo_line_to(cr, cx + s * 0.15, y + s * 0.62);
            cairo_close_path(cr);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.20, y + s * 0.66, s * 0.40, s * 0.07);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            break;
        case 'Q':
            cairo_arc(cr, cx - s * 0.12, y + s * 0.24, s * 0.05, 0.0, 6.28318);
            cairo_arc(cr, cx, y + s * 0.20, s * 0.05, 0.0, 6.28318);
            cairo_arc(cr, cx + s * 0.12, y + s * 0.24, s * 0.05, 0.0, 6.28318);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_move_to(cr, cx - s * 0.22, y + s * 0.62);
            cairo_line_to(cr, cx - s * 0.16, y + s * 0.30);
            cairo_line_to(cr, cx, y + s * 0.26);
            cairo_line_to(cr, cx + s * 0.16, y + s * 0.30);
            cairo_line_to(cr, cx + s * 0.22, y + s * 0.62);
            cairo_close_path(cr);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.24, y + s * 0.66, s * 0.48, s * 0.07);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            break;
        case 'K':
            cairo_rectangle(cr, cx - s * 0.05, y + s * 0.16, s * 0.10, s * 0.16);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.16, y + s * 0.22, s * 0.32, s * 0.08);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.16, y + s * 0.34, s * 0.32, s * 0.26);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            cairo_rectangle(cr, cx - s * 0.22, y + s * 0.66, s * 0.44, s * 0.07);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            break;
        default:
            cairo_arc(cr, cx, cy, s * 0.18, 0.0, 6.28318);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            break;
    }

    if (white) {
        cairo_set_source_rgb(cr, 0.10, 0.10, 0.10);
    } else {
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

static gboolean board_draw_piece_svg(BoardState *state, cairo_t *cr, char piece, gdouble x, gdouble y, gdouble square_size) {
    int index = board_piece_index(piece);
    RsvgHandle *handle;
    RsvgDimensionData dims;
    gdouble scale;
    gdouble draw_x;
    gdouble draw_y;

    if (index < 0) {
        return FALSE;
    }
    handle = state->piece_handles[index];
    if (handle == NULL) {
        return FALSE;
    }

    rsvg_handle_get_dimensions(handle, &dims);
    if (dims.width <= 0 || dims.height <= 0) {
        return FALSE;
    }

    scale = (square_size * 0.84) / (dims.width > dims.height ? dims.width : dims.height);
    draw_x = x + (square_size - (dims.width * scale)) / 2.0;
    draw_y = y + (square_size - (dims.height * scale)) / 2.0;

    cairo_save(cr);
    cairo_translate(cr, draw_x, draw_y);
    cairo_scale(cr, scale, scale);
    rsvg_handle_render_cairo(handle, cr);
    cairo_restore(cr);
    return TRUE;
}

gboolean board_draw(BoardState *state, GtkWidget *widget, cairo_t *cr) {
    gdouble origin_x;
    gdouble origin_y;
    gdouble square_size;
    gdouble board_size;
    int row;
    int col;

    if (!board_get_metrics(widget, &origin_x, &origin_y, &square_size, &board_size)) {
        return FALSE;
    }

    cairo_set_source_rgb(cr, 0.97, 0.97, 0.97);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.10, 0.10, 0.10);
    cairo_rectangle(cr, origin_x - 2.0, origin_y - 2.0, board_size + 4.0, board_size + 4.0);
    cairo_stroke(cr);

    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            gdouble x = origin_x + (col * square_size);
            gdouble y = origin_y + (row * square_size);
            char piece = kindle_chess_backend_piece_at(&state->backend, row, col);
            gboolean dark = ((row + col) % 2) != 0;

            if (row == state->selected_row && col == state->selected_col) {
                cairo_set_source_rgb(cr, 0.20, 0.20, 0.20);
            } else if (dark) {
                cairo_set_source_rgb(cr, 0.70, 0.70, 0.70);
            } else {
                cairo_set_source_rgb(cr, 0.90, 0.90, 0.90);
            }

            cairo_rectangle(cr, x, y, square_size, square_size);
            cairo_fill(cr);

            if (row == state->selected_row && col == state->selected_col) {
                cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
                cairo_set_line_width(cr, square_size * 0.08);
            } else {
                cairo_set_source_rgb(cr, 0.25, 0.25, 0.25);
                cairo_set_line_width(cr, 1.0);
            }
            cairo_rectangle(cr, x, y, square_size, square_size);
            cairo_stroke(cr);

            if (piece != '.') {
                if (!board_draw_piece_svg(state, cr, piece, x, y, square_size)) {
                    if (row == state->selected_row && col == state->selected_col) {
                        cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
                    } else if (isupper((unsigned char) piece)) {
                        cairo_set_source_rgb(cr, 0.05, 0.05, 0.05);
                    } else {
                        cairo_set_source_rgb(cr, 0.35, 0.35, 0.35);
                    }
                    board_draw_piece(cr, piece, x, y, square_size);
                }
            }
        }
    }

    return FALSE;
}

static gboolean board_piece_matches_turn(BoardState *state, char piece) {
    if (piece == '.') {
        return FALSE;
    }
    if (board_white_to_move(state)) {
        return isupper((unsigned char) piece) != 0;
    }
    return islower((unsigned char) piece) != 0;
}

gboolean board_handle_click(BoardState *state,
                            GtkWidget *widget,
                            gdouble x,
                            gdouble y,
                            char promotion_piece,
                            char move_out[6],
                            char san_out[32]) {
    gdouble origin_x;
    gdouble origin_y;
    gdouble square_size;
    gdouble board_size;
    int row;
    int col;
    char piece;

    if (!board_get_metrics(widget, &origin_x, &origin_y, &square_size, &board_size)) {
        return FALSE;
    }

    if (x < origin_x || y < origin_y || x >= origin_x + board_size || y >= origin_y + board_size) {
        return FALSE;
    }

    col = (int) ((x - origin_x) / square_size);
    row = (int) ((y - origin_y) / square_size);
    if (row < 0 || row > 7 || col < 0 || col > 7) {
        return FALSE;
    }

    piece = kindle_chess_backend_piece_at(&state->backend, row, col);

    if (state->selected_row == -1 || state->selected_col == -1) {
        if (board_piece_matches_turn(state, piece)) {
            state->selected_row = row;
            state->selected_col = col;
            move_out[0] = '\0';
            if (san_out != NULL) {
                san_out[0] = '\0';
            }
            return TRUE;
        }
        return FALSE;
    }

    if (state->selected_row == row && state->selected_col == col) {
        state->selected_row = -1;
        state->selected_col = -1;
        move_out[0] = '\0';
        if (san_out != NULL) {
            san_out[0] = '\0';
        }
        return TRUE;
    }

    if (kindle_chess_backend_try_move(&state->backend,
                                      state->selected_row,
                                      state->selected_col,
                                      row,
                                      col,
                                      promotion_piece,
                                      move_out,
                                      san_out)) {
        state->selected_row = -1;
        state->selected_col = -1;
        return TRUE;
    }

    if (board_piece_matches_turn(state, piece)) {
        state->selected_row = row;
        state->selected_col = col;
        move_out[0] = '\0';
        if (san_out != NULL) {
            san_out[0] = '\0';
        }
        return TRUE;
    }

    return FALSE;
}

gboolean board_apply_uci_move(BoardState *state, const char *move, char san_out[32]) {
    state->selected_row = -1;
    state->selected_col = -1;
    return kindle_chess_backend_apply_uci(&state->backend, move, san_out);
}

KindleChessGameState board_get_game_state(BoardState *state) {
    return kindle_chess_backend_state(&state->backend);
}

gboolean board_white_to_move(BoardState *state) {
    return kindle_chess_backend_white_to_move(&state->backend);
}

void board_undo(BoardState *state) {
    state->selected_row = -1;
    state->selected_col = -1;
    kindle_chess_backend_undo(&state->backend);
}
