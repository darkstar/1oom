#ifndef INC_1OOM_GAME_TRANSPORT_H
#define INC_1OOM_GAME_TRANSPORT_H

#include "game_types.h"

struct game_s;

extern int game_turn_transport(struct game_s *g);
extern int game_turn_transport_destroyed_show(struct game_s *g, player_id_t pi);

#endif
