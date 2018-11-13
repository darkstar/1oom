#ifndef INC_1OOM_GAME_VIEW_H
#define INC_1OOM_GAME_VIEW_H

#include "game_types.h"

struct game_s;

extern void game_view_update_from(struct game_s *g, const struct game_s *gv, player_id_t pi);
extern void game_view_update_all(struct game_s *g);
extern void game_view_make(const struct game_s *g, struct game_s *gv, player_id_t pi);
extern void game_view_make_all(struct game_s *g);

#endif
