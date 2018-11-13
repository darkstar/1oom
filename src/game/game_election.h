#ifndef INC_1OOM_GAME_ELECTION_H
#define INC_1OOM_GAME_ELECTION_H

#include "game_types.h"

struct game_s;

extern void game_election(struct game_s *g);

extern int game_election_server_votea_msg(struct game_s *g, player_id_t pi);
extern int game_election_server_councila_msg(struct game_s *g, player_id_t pi);

extern const char *game_election_print_votes(uint16_t n, char *buf);
extern int game_election_client_start(struct game_s *g, player_id_t pi);
extern int game_election_client_voted(struct game_s *g, player_id_t pi);
extern int game_election_client_voteq(struct game_s *g, player_id_t pi);
extern int game_election_client_result(struct game_s *g, player_id_t pi);
extern int game_election_client_end(struct game_s *g, player_id_t pi);

#endif
