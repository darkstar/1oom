#ifndef INC_1OOM_GAME_TURN_H
#define INC_1OOM_GAME_TURN_H

#include "game_end.h"
#include "types.h"

struct game_s;

extern struct game_end_s game_turn_process(struct game_s *g);
extern struct game_end_s game_turn_client_next(struct game_s *g);
extern int game_turn_client_end(struct game_s *g, player_id_t pi);
extern int game_turn_server_turn_input_msg(struct game_s *g, player_id_t pi);
extern int game_turn_server_send_state(struct game_s *g);
extern int game_turn_server_send_end(struct game_s *g, struct game_end_s *ge, const uint8_t *old_home);

#endif
