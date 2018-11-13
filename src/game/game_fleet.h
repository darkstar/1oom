#ifndef INC_1OOM_GAME_FLEET_H
#define INC_1OOM_GAME_FLEET_H

#include "game_types.h"
#include "types.h"

struct game_s;
struct planet_s;
struct fleet_enroute_s;
struct transport_s;

#define FLEET_SPEED_STARGATE    35

extern bool game_send_fleet_from_orbit(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS]);
extern bool game_send_fleet_from_orbit_client(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS]);
extern bool game_send_fleet_retreat(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS]);
extern bool game_send_fleet_reloc(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, uint8_t si, shipcount_t shipnum);
extern bool game_send_transport(struct game_s *g, struct planet_s *p);
extern void game_remove_empty_fleets(struct game_s *g);
extern void game_remove_player_fleets(struct game_s *g, player_id_t owner);
extern bool game_fleet_any_dest_player(const struct game_s *g, player_id_t owner, player_id_t target);
extern void game_fleet_unrefuel(struct game_s *g);
extern uint8_t game_fleet_get_speed(const struct game_s *g, const struct fleet_enroute_s *r, uint8_t pfrom, uint8_t pto);
extern void game_fleet_redirect(struct game_s *g, struct fleet_enroute_s *r, uint8_t pfrom, uint8_t pto);
extern bool game_fleet_redirect_client(struct game_s *g, struct fleet_enroute_s *r, uint8_t pfrom, uint8_t pto);
extern void game_transport_redirect(struct game_s *g, struct transport_s *r, uint8_t pto);
extern bool game_transport_redirect_client(struct game_s *g, struct transport_s *r, uint8_t pto);
extern int game_fleet_server_send_fleet_msg(struct game_s *g, player_id_t pi);
extern int game_fleet_server_redir_fleet_msg(struct game_s *g, player_id_t pi);
extern int game_fleet_server_redir_trans_msg(struct game_s *g, player_id_t pi);

#endif
