#ifndef INC_1OOM_GAME_NEWTECH_H
#define INC_1OOM_GAME_NEWTECH_H

#include "game_types.h"

struct game_s;

extern int game_turn_newtech_send_single(struct game_s *g, player_id_t pi);
extern int game_turn_newtech(struct game_s *g);
extern int game_turn_newtech_next_msg(struct game_s *g, player_id_t pi);
extern int game_turn_newtech_frame_msg(struct game_s *g, player_id_t pi);

extern int game_turn_newtech_show(struct game_s *g, player_id_t pi);
extern int game_turn_newtech_choose_next(struct game_s *g, player_id_t pi, tech_field_t field, uint8_t tech);
extern int game_turn_newtech_frame(struct game_s *g, player_id_t pi, uint8_t newtech_i, player_id_t victim);

#endif
