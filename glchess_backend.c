#include "glchess_backend.h"

#include <glib-object.h>
#include <string.h>

typedef enum  {
    COLOR_WHITE,
    COLOR_BLACK
} Color;

typedef enum  {
    PIECE_TYPE_PAWN,
    PIECE_TYPE_ROOK,
    PIECE_TYPE_KNIGHT,
    PIECE_TYPE_BISHOP,
    PIECE_TYPE_QUEEN,
    PIECE_TYPE_KING
} PieceType;

typedef enum  {
    CHECK_STATE_NONE,
    CHECK_STATE_CHECK,
    CHECK_STATE_CHECKMATE
} CheckState;

typedef enum  {
    CHESS_RULE_CHECKMATE,
    CHESS_RULE_STALEMATE,
    CHESS_RULE_FIFTY_MOVES,
    CHESS_RULE_TIMEOUT,
    CHESS_RULE_THREE_FOLD_REPETITION,
    CHESS_RULE_INSUFFICIENT_MATERIAL,
    CHESS_RULE_RESIGN,
    CHESS_RULE_ABANDONMENT,
    CHESS_RULE_DEATH
} ChessRule;

typedef enum  {
    CHESS_RESULT_IN_PROGRESS,
    CHESS_RESULT_WHITE_WON,
    CHESS_RESULT_BLACK_WON,
    CHESS_RESULT_DRAW
} ChessResult;

typedef struct _ChessPlayer ChessPlayer;
typedef struct _ChessPiece ChessPiece;
typedef struct _ChessMove ChessMove;
typedef struct _ChessStatePrivate ChessStatePrivate;
typedef struct _ChessState ChessState;
typedef struct _ChessGamePrivate ChessGamePrivate;
typedef struct _ChessGame ChessGame;

struct _ChessPlayer {
    GObject parent_instance;
    gpointer priv;
    Color color;
};

struct _ChessMove {
    GTypeInstance parent_instance;
    volatile int ref_count;
    gpointer priv;
    int number;
    ChessPiece *piece;
    ChessPiece *promotion_piece;
    ChessPiece *moved_rook;
    ChessPiece *victim;
    int r0;
    int f0;
    int r1;
    int f1;
    gboolean ambiguous_rank;
    gboolean ambiguous_file;
    CheckState check_state;
};

struct _ChessPiece {
    GTypeInstance parent_instance;
    volatile int ref_count;
    gpointer priv;
    ChessPlayer *player;
    PieceType type;
};

struct _ChessState {
    GTypeInstance parent_instance;
    volatile int ref_count;
    ChessStatePrivate *priv;
    int number;
    ChessPlayer *players[2];
    ChessPlayer *current_player;
    gboolean can_castle_kingside[2];
    gboolean can_castle_queenside[2];
    int en_passant_index;
    CheckState check_state;
    int halfmove_clock;
    ChessPiece *board[64];
    ChessMove *last_move;
};

struct _ChessGame {
    GTypeInstance parent_instance;
    volatile int ref_count;
    ChessGamePrivate *priv;
    gboolean is_started;
    ChessResult result;
    ChessRule rule;
    GList *move_stack;
};

ChessGame* chess_game_new(const gchar *fen, gchar **moves, gint moves_length1);
void chess_game_unref(gpointer instance);
void chess_game_start(ChessGame *self);
ChessPlayer* chess_game_get_current_player(ChessGame *self);
ChessState* chess_game_get_current_state(ChessGame *self);
ChessPiece* chess_game_get_piece(ChessGame *self, gint rank, gint file, gint move_number);
guint chess_game_get_n_moves(ChessGame *self);
gboolean chess_player_move_with_coords(ChessPlayer *self, gint r0, gint f0, gint r1, gint f1, gboolean apply);
gboolean chess_player_move(ChessPlayer *self, const gchar *move, gboolean apply);
void chess_player_undo(ChessPlayer *self);
ChessResult chess_state_get_result(ChessState *self, ChessRule *rule);
gunichar chess_piece_get_symbol(ChessPiece *self);
void chess_piece_unref(gpointer instance);
gchar* chess_move_get_san(ChessMove *self);
gchar* chess_move_get_engine(ChessMove *self);

#define CHESS_GAME_STANDARD_SETUP "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

static void format_move(int row0, int col0, int row1, int col1, char promotion_piece, char move_out[6]) {
    move_out[0] = (char) ('a' + col0);
    move_out[1] = (char) ('8' - row0);
    move_out[2] = (char) ('a' + col1);
    move_out[3] = (char) ('8' - row1);
    move_out[4] = promotion_piece;
    move_out[5] = '\0';
}

static gboolean is_promotion_row(int row) {
    return row == 0 || row == 7;
}

static void copy_san(ChessState *state, char san_out[32]) {
    gchar *san;

    if (san_out == NULL) {
        return;
    }
    san_out[0] = '\0';
    if (state == NULL || state->last_move == NULL) {
        return;
    }

    san = chess_move_get_san(state->last_move);
    if (san != NULL) {
        g_strlcpy(san_out, san, 32);
        g_free(san);
    }
}

static void copy_uci(ChessState *state, char uci_out[6]) {
    gchar *uci;

    if (uci_out == NULL) {
        return;
    }
    uci_out[0] = '\0';
    if (state == NULL || state->last_move == NULL) {
        return;
    }

    uci = chess_move_get_engine(state->last_move);
    if (uci != NULL) {
        g_strlcpy(uci_out, uci, 6);
        g_free(uci);
    }
}

gboolean kindle_chess_backend_init(KindleChessBackend *backend) {
    memset(backend, 0, sizeof(*backend));
    backend->game = chess_game_new(CHESS_GAME_STANDARD_SETUP, NULL, 0);
    if ((ChessGame *) backend->game == NULL) {
        return FALSE;
    }
    chess_game_start((ChessGame *) backend->game);
    return TRUE;
}

void kindle_chess_backend_clear(KindleChessBackend *backend) {
    if (backend->game != NULL) {
        chess_game_unref((ChessGame *) backend->game);
        backend->game = NULL;
    }
}

void kindle_chess_backend_reset(KindleChessBackend *backend) {
    kindle_chess_backend_clear(backend);
    kindle_chess_backend_init(backend);
}

gboolean kindle_chess_backend_white_to_move(KindleChessBackend *backend) {
    ChessState *state = chess_game_get_current_state((ChessGame *) backend->game);
    return state->current_player->color == COLOR_WHITE;
}

char kindle_chess_backend_piece_at(KindleChessBackend *backend, int row, int col) {
    ChessPiece *piece = chess_game_get_piece((ChessGame *) backend->game, 7 - row, col, -1);
    char symbol = '.';

    if (piece != NULL) {
        switch (piece->type) {
            case PIECE_TYPE_PAWN:
                symbol = 'p';
                break;
            case PIECE_TYPE_ROOK:
                symbol = 'r';
                break;
            case PIECE_TYPE_KNIGHT:
                symbol = 'n';
                break;
            case PIECE_TYPE_BISHOP:
                symbol = 'b';
                break;
            case PIECE_TYPE_QUEEN:
                symbol = 'q';
                break;
            case PIECE_TYPE_KING:
            default:
                symbol = 'k';
                break;
        }
        if (piece->player->color == COLOR_WHITE) {
            symbol = (char) g_ascii_toupper(symbol);
        }
        chess_piece_unref(piece);
    }
    return symbol;
}

gboolean kindle_chess_backend_try_move(KindleChessBackend *backend,
                                       int row0,
                                       int col0,
                                       int row1,
                                       int col1,
                                       char promotion_piece,
                                       char move_out[6],
                                       char san_out[32]) {
    ChessPlayer *player = chess_game_get_current_player((ChessGame *) backend->game);
    char piece;
    char promo = '\0';
    ChessState *state;

    piece = kindle_chess_backend_piece_at(backend, row0, col0);
    if ((piece == 'P' || piece == 'p') && is_promotion_row(row1)) {
        promo = (char) g_ascii_tolower(promotion_piece != '\0' ? promotion_piece : 'q');
    }

    format_move(row0, col0, row1, col1, promo, move_out);
    if (!chess_player_move(player, move_out, TRUE)) {
        move_out[0] = '\0';
        if (san_out != NULL) {
            san_out[0] = '\0';
        }
        return FALSE;
    }

    state = chess_game_get_current_state((ChessGame *) backend->game);
    copy_san(state, san_out);
    return TRUE;
}

gboolean kindle_chess_backend_apply_uci(KindleChessBackend *backend, const char *move, char san_out[32]) {
    ChessPlayer *player = chess_game_get_current_player((ChessGame *) backend->game);
    gboolean ok = chess_player_move(player, move, TRUE);

    if (ok) {
        copy_san(chess_game_get_current_state((ChessGame *) backend->game), san_out);
    } else if (san_out != NULL) {
        san_out[0] = '\0';
    }
    return ok;
}

gboolean kindle_chess_backend_apply_move_text(KindleChessBackend *backend, const char *move_text, char san_out[32], char uci_out[6]) {
    ChessPlayer *player = chess_game_get_current_player((ChessGame *) backend->game);
    gboolean ok = chess_player_move(player, move_text, TRUE);

    if (ok) {
        ChessState *state = chess_game_get_current_state((ChessGame *) backend->game);
        copy_san(state, san_out);
        copy_uci(state, uci_out);
    } else {
        if (san_out != NULL) {
            san_out[0] = '\0';
        }
        if (uci_out != NULL) {
            uci_out[0] = '\0';
        }
    }
    return ok;
}

void kindle_chess_backend_undo(KindleChessBackend *backend) {
    ChessPlayer *player = chess_game_get_current_player((ChessGame *) backend->game);
    chess_player_undo(player);
}

KindleChessGameState kindle_chess_backend_state(KindleChessBackend *backend) {
    ChessRule rule = CHESS_RULE_STALEMATE;
    ChessState *state = chess_game_get_current_state((ChessGame *) backend->game);
    ChessResult result = chess_state_get_result(state, &rule);

    if (result == CHESS_RESULT_IN_PROGRESS) {
        return state->check_state == CHECK_STATE_CHECK ? KINDLE_CHESS_CHECK : KINDLE_CHESS_ONGOING;
    }
    if (rule == CHESS_RULE_CHECKMATE) {
        return KINDLE_CHESS_CHECKMATE;
    }
    if (rule == CHESS_RULE_STALEMATE) {
        return KINDLE_CHESS_STALEMATE;
    }
    return KINDLE_CHESS_DRAW;
}

gboolean kindle_chess_backend_get_last_san(KindleChessBackend *backend, char san_out[32]) {
    ChessState *state = chess_game_get_current_state((ChessGame *) backend->game);

    if (state == NULL || state->last_move == NULL) {
        if (san_out != NULL) {
            san_out[0] = '\0';
        }
        return FALSE;
    }
    copy_san(state, san_out);
    return san_out != NULL && san_out[0] != '\0';
}

gboolean kindle_chess_backend_get_last_uci(KindleChessBackend *backend, char uci_out[6]) {
    ChessState *state = chess_game_get_current_state((ChessGame *) backend->game);

    if (state == NULL || state->last_move == NULL) {
        if (uci_out != NULL) {
            uci_out[0] = '\0';
        }
        return FALSE;
    }
    copy_uci(state, uci_out);
    return uci_out != NULL && uci_out[0] != '\0';
}
