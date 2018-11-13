#ifndef INC_1OOM_GAME_AUX_H
#define INC_1OOM_GAME_AUX_H

#include "game.h"
#include "game_planet.h"
#include "types.h"

#define NUM_SHIPLOOKS   0x93

#define DIPLOMAT_D0_NUM 0x51
#define DIPLOMAT_MSG_NUM    1215
#define DIPLOMAT_MSG_LEN    0xc8

#define DIPLOMAT_MSG_PTR(ga_, r_, t_)   (&((ga_)->diplomat.msg[((t_) * 15 + (r_)) * DIPLOMAT_MSG_LEN]))
#define DIPLOMAT_MSG_GFX(p_)   ((p_)[0xc4])
#define DIPLOMAT_MSG_MUS(p_)   ((p_)[0xc6])

#define RESEARCH_DESCR_LEN  0xc3

#define RESEARCH_D0_PTR(ga_, f_, t_)   ((const uint8_t *)&((ga_)->research.d0[((f_) * 50 + (t_) - 1) * 6]))
#define RESEARCH_D0_B1(p_)   (((p_)[0] == 0xff) ? 0 : ((p_)[1]))

#define EVENTMSG_TYPE_NUM   22
#define EVENTMSG_SUB_NUM    7
#define EVENTMSG_NUM  (EVENTMSG_TYPE_NUM * EVENTMSG_SUB_NUM)
#define EVENTMSG_LEN  0xc8

#define EVENTMSG_PTR(ga_, t_, s_)   (&((ga_)->eventmsg[((t_ - 1) * 7 + (s_)) * EVENTMSG_LEN]))

#define GAME_AUX_MSGBUF_SIZE    0x10000/*FIXME*/

typedef enum {
    GAME_MULTIPLAYER_LOCAL = 0,
    GAME_MULTIPLAYER_CLIENT,
    GAME_MULTIPLAYER_SERVER
} game_multiplayer_t;

typedef enum {
    GAME_TURN_WAIT_INPUT = 0,
    GAME_TURN_MOVE,
    GAME_TURN_BATTLE,
    GAME_TURN_EXPLORE,
    GAME_TURN_BOMB,
    GAME_TURN_GROUND,
    GAME_TURN_NEWTECH,
    GAME_TURN_ESPIONAGE,
    GAME_TURN_SABOTAGE,
    GAME_TURN_ELECTION
} game_turn_phase_t;

struct game_client_s;
struct game_server_s;
struct spy_turn_s;

struct game_msg_buf_s {
    uint8_t *buf;
    int len;
    int status;
};

struct firing_s {
    uint8_t d0[12]; /* uint16_t in lbx */
    uint8_t target_x;
    uint8_t target_y;
};

/* Aux game data, not stored in saves. */
struct game_aux_s {
    struct {
        const uint8_t *d0; /*[TECH_FIELD_NUM * 50 * 6]*/
        const char *names; /* tech names, "foo\0bar\0" etc */
        const char *descr; /*[TECH_FIELD_NUM * 50 * RESEARCH_DESCR_LEN] tech descriptions */
    } research;
    struct {
        uint8_t d0[DIPLOMAT_D0_NUM];    /* uint16_t in lbx */
        const char *msg;
    } diplomat;
    struct firing_s firing[NUM_SHIPLOOKS];
    const char *eventmsg;
    uint8_t star_dist[PLANETS_MAX][PLANETS_MAX];
    player_id_t killer[PLAYER_NUM]; /* used for funeral ending */
    int local_players;
    bool flag_cheat_galaxy;
    bool flag_cheat_events;
    bool initialized;
    int savenamebuflen;
    int savebuflen;
    char *savenamebuf;
    uint8_t *savebuf;
    struct game_s *g;   /* parent game (unrestricted) */
    struct game_s *gview[PLAYER_NUM];   /* restricted viev of game */
    uint8_t *movebuf[PLAYER_NUM];
    struct game_msg_buf_s msgi[PLAYER_NUM];
    struct game_msg_buf_s msgo[PLAYER_NUM];
    game_multiplayer_t multiplayer;
    struct game_client_s *gcl;
    struct game_server_s *gsr;
    game_turn_phase_t turn_phase;
    uint16_t cl_last_msg;
    union {
        struct turn_pending_s {
            BOOLVEC_DECLARE(msg_started, PLAYER_NUM);
            BOOLVEC_DECLARE(answer, PLAYER_NUM);
            uint8_t planet[PLAYER_NUM];
            int num;
        } pending;
        struct turn_newtech_s {
            int num_choose;
            int num_frame;
            /* espionage */
            BOOLVEC_DECLARE(remain, PLAYER_NUM);
            BOOLVEC_DECLARE(answer, PLAYER_NUM);
            player_id_t target[PLAYER_NUM];
            uint8_t tech[PLAYER_NUM][TECH_FIELD_NUM];
            uint8_t flags_field[PLAYER_NUM];
            struct spy_turn_s *st;
        } newtech;
        struct turn_sabotage_s {
            BOOLVEC_DECLARE(remain, PLAYER_NUM);
            BOOLVEC_DECLARE(answer, PLAYER_NUM);
            BOOLVEC_DECLARE(frame, PLAYER_NUM);
            player_id_t target[PLAYER_NUM];
            player_id_t other[PLAYER_NUM][2];
        } sabotage;
        struct turn_election_s {
            player_id_t pending;
            uint8_t voted;
            BOOLVEC_DECLARE(answer, PLAYER_NUM);
        } election;
    } turn;
};

extern int game_aux_init(struct game_aux_s *gaux, struct game_s *g, game_multiplayer_t mp, struct game_client_s *gcl, struct game_server_s *gsr);
extern void game_aux_shutdown(struct game_aux_s *gaux);

extern uint8_t game_aux_get_firing_param_x(const struct game_aux_s *gaux, uint8_t look, uint8_t a2, bool dir);
extern uint8_t game_aux_get_firing_param_y(const struct game_aux_s *gaux, uint8_t look, uint8_t a2, bool dir);

struct game_s;
extern void game_aux_start(struct game_aux_s *gaux, struct game_s *g);

#endif
