#ifndef INC_1OOM_GAME_CLIENT_H
#define INC_1OOM_GAME_CLIENT_H

struct game_s;

extern int game_client_msg_send(struct game_s *g, int pi);
extern int game_client_msg_recv(struct game_s *g, int pi);
extern int game_client_msg_send_and_wait_reply(struct game_s *g, int pi);
extern int game_client_msg_handle(struct game_s *g, int pi);

#endif
