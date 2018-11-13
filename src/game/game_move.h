#ifndef INC_1OOM_GAME_MOVE_H
#define INC_1OOM_GAME_MOVE_H

#include "game_types.h"

struct game_s;

#define GAME_MOVE_FRAME_START   0x8/*:12 frame_num*/
#define GAME_MOVE_FRAME_END     0x9/*:12 flag_more*/
#define GAME_MOVE_HIDE_FLEET    0xa/*:12 fleet_index*/
#define GAME_MOVE_HIDE_TRANS    0xb/*:12 trans_index*/
#define GAME_MOVE_SHOW_FLEET    0xc/*:12 fleet_index, u16 x, u16 y, u8 dest*/
#define GAME_MOVE_SHOW_TRANS    0xd/*:12 trans_index, u16 x, u16 y, u8 dest*/
#define GAME_MOVE_MOVE_FLEET    0xe/*:12 fleet_index, s8 dx, s8 dy*/
#define GAME_MOVE_MOVE_TRANS    0xf/*:12 trans_index, s8 dx, s8 dy*/
#define GAME_MOVE_INDEX_MASK    0xfff
#define GAME_MOVE_INDEX_AMOEBA  0xfff
#define GAME_MOVE_INDEX_CRYSTAL 0xffe

extern int game_turn_move_ships(struct game_s *g);
extern int game_turn_move_show(struct game_s *g, player_id_t pi);
extern void game_turn_move_to_orbit(struct game_s *g);

#endif
