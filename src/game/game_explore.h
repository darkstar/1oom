#ifndef INC_1OOM_GAME_EXPLORE_H
#define INC_1OOM_GAME_EXPLORE_H

#include "game_types.h"

struct game_s;

extern int game_turn_explore(struct game_s *g);
extern int game_turn_explore_send_single(struct game_s *g, player_id_t pi, uint8_t pli);
extern int game_turn_explore_show(struct game_s *g, player_id_t pi);
extern int game_turn_colonize_ask(struct game_s *g, player_id_t pi);
extern int game_turn_server_colonize_msg(struct game_s *g, player_id_t pi);

#endif
