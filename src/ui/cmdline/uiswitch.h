#ifndef INC_1OOM_UISWITCH_H
#define INC_1OOM_UISWITCH_H

#include "game_types.h"
#include "types.h"

struct game_s;

extern bool ui_switch_1_opts(const struct game_s *g, player_id_t pi);
extern void ui_switch_1(const struct game_s *g, player_id_t pi);
extern bool ui_switch_2(const struct game_s *g, player_id_t pi1, player_id_t pi2);
extern void ui_switch_all(const struct game_s *g);
extern void ui_switch_wait(const struct game_s *g);

#endif
