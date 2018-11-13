#include "config.h"

#include "game_spy.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_endecode.h"
#include "game_newtech.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_tech.h"
#include "game_server.h"
#include "log.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_ESP  4

/* -------------------------------------------------------------------------- */

static void game_spy_esp_human_stealq_msg(struct game_s *g, player_id_t spy)
{
    struct turn_newtech_s *tn = &(g->gaux->turn.newtech);
    GAME_MSGO_EN_HDR(g, spy, GAME_MSG_ID_STEALQ);
    GAME_MSGO_EN_U8(g, spy, tn->target[spy]);
    GAME_MSGO_EN_U8(g, spy, tn->flags_field[spy]);
    GAME_MSGO_EN_LEN(g, spy);
}

static void game_spy_esp_human_stolen_msg_add(struct game_s *g, player_id_t pi, uint16_t v, bool msg_started)
{
    if (!msg_started) {
        GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_STOLEN);
    }
    GAME_MSGO_EN_U8(g, pi, (v >> 12) & 7);
    GAME_MSGO_EN_U8(g, pi, (v >> 8) & 7);
    GAME_MSGO_EN_U8(g, pi, v & 0xff);
}

static void game_spy_esp_human_stolen_msg_finish(struct game_s *g, player_id_t pi)
{
    GAME_MSGO_EN_U8(g, pi, 0xff);
    GAME_MSGO_EN_LEN(g, pi);
}

static bool game_spy_esp_human_next_target(struct game_s *g, player_id_t spy)
{
    struct turn_newtech_s *tn = &(g->gaux->turn.newtech);
    struct spy_esp_s s[1];
    player_id_t target = tn->target[spy];
    g->evn.newtech[spy].num = 0;
    if (target == PLAYER_NONE) {
        BOOLVEC_SET0(tn->remain, spy);
        return false;
    }
    s->spy = spy;
    for (; target < g->players; ++target) {
        if ((target != spy) && (g->evn.spied_num[target][spy] > 0)) {
            uint8_t flags_field;
            s->target = target;
            flags_field = 0;
            for (int i = 0; i < TECH_FIELD_NUM; ++i) {
                tn->tech[spy][i] = 0;
            }
            for (int loops = 0; loops < 5; ++loops) {
                int num;
                num = game_spy_esp_sub1(g, s, 0, 0);
                game_spy_esp_sub5(s, tn->st->tbl_rmax[target][spy]);
                for (int i = 0; i < num; ++i) {
                    tech_field_t field;
                    field = s->tbl_field[i];
                    if (tn->tech[spy][field] == 0) {
                        tn->tech[spy][field] = s->tbl_tech2[i];
                        flags_field |= (1 << field);
                    }
                }
            }
            tn->flags_field[spy] = flags_field;
            g->evn.newtech[spy].num = 0;
            if (flags_field != 0) {
                BOOLVEC_SET1(tn->answer, spy);
                BOOLVEC_SET1(tn->remain, spy);
                break;
            }
        }
    }
    if (target >= g->players) {
        target = PLAYER_NONE;
        BOOLVEC_SET0(tn->answer, spy);
        BOOLVEC_SET0(tn->remain, spy);
    }
    tn->target[spy] = target;
    return (target != PLAYER_NONE);
}

/* -------------------------------------------------------------------------- */

void game_spy_esp_human(struct game_s *g, struct spy_turn_s *st)
{
    /* 1. Find target for each player
       2. If any targets, send field select to players and wait for answers
       3.1. Msg handler (esp): (don't) steal tech and send newtech messages
       3.2. Msg handler (newtech): handle next/frame messages
       4. Wait for answers, go to 2 until all targets and messages handled
    */
    struct turn_newtech_s *tn = &(g->gaux->turn.newtech);
    BOOLVEC_CLEAR(tn->remain, PLAYER_NUM);
    BOOLVEC_CLEAR(tn->answer, PLAYER_NUM);
    tn->num_choose = 0;
    tn->num_frame = 0;
    tn->st = st;
    g->gaux->turn_phase = GAME_TURN_ESPIONAGE;
    for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
        g->evn.newtech[spy].num = 0;
        tn->flags_field[spy] = 0;
        if (IS_AI(g, spy) || (!IS_ALIVE(g, spy))) {
            tn->target[spy] = PLAYER_NONE;
        } else {
            tn->target[spy] = PLAYER_0;
            if (game_spy_esp_human_next_target(g, spy)) {
                game_spy_esp_human_stealq_msg(g, spy);
            }
        }
    }
    if (!BOOLVEC_IS_CLEAR(tn->remain, PLAYER_NUM)) {
        if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) { /* handle spies in series */
            for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
                while (BOOLVEC_IS1(tn->remain, spy)) {
                    game_server_msgo_flush_player(g, spy);
                    game_server_msg_wait(g);
                    if (game_spy_esp_human_next_target(g, spy)) {
                        game_spy_esp_human_stealq_msg(g, spy);
                    }
                }
            }
        } else { /* concurrent handling */
            game_server_msgo_flush(g);
            while (!BOOLVEC_IS_CLEAR(tn->remain, PLAYER_NUM)) {
                game_server_msg_wait(g);
                for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
                    if (BOOLVEC_IS0(tn->answer, spy) && game_spy_esp_human_next_target(g, spy)) {
                        game_spy_esp_human_stealq_msg(g, spy);
                        game_server_msgo_flush_player(g, spy);
                    }
                }
            }
        }
    }
    for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
        g->evn.newtech[spy].num = 0;
    }
    /* Send stolen messages to affected players */
    for (player_id_t player = PLAYER_0; player < g->players; ++player) {
        uint16_t tbl[PLAYER_NUM];
        int n;
        if (IS_AI(g, player)) {
            continue;
        }
        n = 0;
        for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
            uint8_t tech;
            tech = g->evn.stolen_tech[player][spy];
            if ((spy != player) && (tech != 0)) {
                tbl[n++] = ((((uint16_t)g->evn.stolen_spy[player][spy])) << 12) | ((((uint16_t)g->evn.stolen_field[player][spy])) << 8) | tech;
            }
        }
        for (int loops = 0; loops < n; ++loops) {
            for (int i = 0; i < n - 1; ++i) {
                uint16_t v0, v1;
                v0 = tbl[i];
                v1 = tbl[i + 1];
                if (v0 > v1) {
                    tbl[i + 1] = v0;
                    tbl[i] = v1;
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            game_spy_esp_human_stolen_msg_add(g, player, tbl[i], i > 0);
        }
        if (n > 0) {
            game_spy_esp_human_stolen_msg_finish(g, player);
            BOOLVEC_SET1(tn->answer, player);
        }
    }
    if (!BOOLVEC_IS_CLEAR(tn->answer, PLAYER_NUM)) {
        game_server_msgo_flush(g);
    }
}

int game_spy_server_steal_msg(struct game_s *g, player_id_t pi)
{
    struct turn_newtech_s *tn = &(g->gaux->turn.newtech);
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    uint8_t target, field;
    SG_1OOM_DE_U8(target);
    if (target >= g->players) {
        log_warning("%s: pi %i msg target %u >= %u\n", __func__, pi, target, g->players);
        return 2;
    }
    if (target != tn->target[pi]) {
        log_warning("%s: pi %i msg target %u != %u\n", __func__, pi, target, tn->target[pi]);
        return 2;
    }
    SG_1OOM_DE_U8(field);
    if ((field > TECH_FIELD_NUM) || ((field < TECH_FIELD_NUM) && ((tn->flags_field[pi] & (1 << field)) == 0))) {
        log_warning("%s: pi %i msg invalid field %u\n", __func__, pi, field);
        return 2;
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_ESP, "%s: pi %i msg ok\n", __func__, pi));
    GAME_MSGI_HANDLED(g, pi);
    game_server_msg_send_status(g, pi, 1);
    tn->flags_field[pi] = 0;
    if (++tn->target[pi] >= g->players) {
        tn->target[pi] = PLAYER_NONE;
    }
    BOOLVEC_SET0(tn->answer, pi);
    if ((field >= 0) && (field < TECH_FIELD_NUM)) {
        bool framed;
        uint8_t planet;
        planet = game_planet_get_random(g, target);
        framed = (g->evn.spied_spy[target][pi] == -1);
        if (!framed) {
            game_diplo_act(g, -g->evn.spied_spy[target][pi], pi, target, 4, 0, target);
        }
        g->evn.newtech[pi].num = 0;
        game_tech_get_new(g, pi, field, tn->tech[pi][field], TECHSOURCE_SPY, planet, target, framed);
        if (game_turn_newtech_send_single(g, pi) == 1) {
            BOOLVEC_SET1(tn->answer, pi);
        }
    }
    return 0;
}
