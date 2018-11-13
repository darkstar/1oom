#ifndef INC_1OOM_GAME_BOMB_H
#define INC_1OOM_GAME_BOMB_H

#include "game_types.h"

struct game_s;

extern int game_turn_bomb(struct game_s *g);
extern int game_turn_bomb_show(struct game_s *g, player_id_t pi);
extern int game_turn_bomb_ask(struct game_s *g, player_id_t pi);
extern int game_turn_server_bomb_msg(struct game_s *g, player_id_t pi);

#endif
