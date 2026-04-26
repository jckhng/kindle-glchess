#ifndef KINDLE_CHESS_GLCHESS_BACKEND_H
#define KINDLE_CHESS_GLCHESS_BACKEND_H

#include <glib.h>

typedef struct _KindleChessBackend {
    gpointer game;
} KindleChessBackend;

typedef enum {
    KINDLE_CHESS_ONGOING,
    KINDLE_CHESS_CHECK,
    KINDLE_CHESS_CHECKMATE,
    KINDLE_CHESS_STALEMATE,
    KINDLE_CHESS_DRAW
} KindleChessGameState;

gboolean kindle_chess_backend_init(KindleChessBackend *backend);
void kindle_chess_backend_clear(KindleChessBackend *backend);
void kindle_chess_backend_reset(KindleChessBackend *backend);
gboolean kindle_chess_backend_white_to_move(KindleChessBackend *backend);
char kindle_chess_backend_piece_at(KindleChessBackend *backend, int row, int col);
gboolean kindle_chess_backend_try_move(KindleChessBackend *backend,
                                       int row0,
                                       int col0,
                                       int row1,
                                       int col1,
                                       char promotion_piece,
                                       char move_out[6],
                                       char san_out[32]);
gboolean kindle_chess_backend_apply_uci(KindleChessBackend *backend, const char *move, char san_out[32]);
gboolean kindle_chess_backend_apply_move_text(KindleChessBackend *backend, const char *move_text, char san_out[32], char uci_out[6]);
void kindle_chess_backend_undo(KindleChessBackend *backend);
KindleChessGameState kindle_chess_backend_state(KindleChessBackend *backend);
gboolean kindle_chess_backend_get_last_san(KindleChessBackend *backend, char san_out[32]);
gboolean kindle_chess_backend_get_last_uci(KindleChessBackend *backend, char uci_out[6]);

#endif
