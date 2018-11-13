#ifndef INC_1OOM_GAME_SERVER_H
#define INC_1OOM_GAME_SERVER_H

struct game_s;

extern int game_server_msgo_flush(struct game_s *g);
extern int game_server_msgo_flush_player(struct game_s *g, int pi);
extern int game_server_msg_send_status(struct game_s *g, int pi, int status);
extern int game_server_msg_handle(struct game_s *g, int pi);
extern int game_server_msg_wait(struct game_s *g);

#endif
