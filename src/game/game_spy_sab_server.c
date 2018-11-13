#include "config.h"

#include "game_spy.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_endecode.h"
#include "game_explore.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_server.h"
#include "log.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_SAB  4

/* -------------------------------------------------------------------------- */

static void game_spy_sab_human_sabq_msg(struct game_s *g, player_id_t spy)
{
    struct turn_sabotage_s *ts = &(g->gaux->turn.sabotage);
    GAME_MSGO_EN_HDR(g, spy, GAME_MSG_ID_SABQ);
    GAME_MSGO_EN_U8(g, spy, ts->target[spy]);
    GAME_MSGO_EN_LEN(g, spy);
}

static void game_spy_sab_human_sabotage_msg_add(struct game_s *g, player_id_t pi, player_id_t spy, player_id_t target, sabotage_act_t act, player_id_t other1, player_id_t other2, uint8_t pli, uint16_t sval, bool msg_started)
{
    const planet_t *p = &(g->planet[pli]);
    if (!msg_started) {
        GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_SABOTAGE);
    }
    GAME_MSGO_EN_U8(g, pi, spy);
    GAME_MSGO_EN_U8(g, pi, target);
    GAME_MSGO_EN_U8(g, pi, act);
    GAME_MSGO_EN_U8(g, pi, other1);
    GAME_MSGO_EN_U8(g, pi, other2);
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_U16(g, pi, p->rebels);
    GAME_MSGO_EN_U16(g, pi, sval);
}

static void game_spy_sab_human_sabotage_msg_finish(struct game_s *g, player_id_t pi)
{
    GAME_MSGO_EN_U8(g, pi, 0xff);
    GAME_MSGO_EN_LEN(g, pi);
}

static bool game_spy_sab_human_next_target(struct game_s *g, player_id_t spy)
{
    struct turn_sabotage_s *ts = &(g->gaux->turn.sabotage);
    player_id_t target = ts->target[spy];
    if (target == PLAYER_NONE) {
        BOOLVEC_SET0(ts->remain, spy);
        return false;
    }
    for (; target < g->players; ++target) {
        if ((target != spy) && (g->evn.sabotage_num[target][spy] > 0)) {
            BOOLVEC_SET1(ts->answer, spy);
            BOOLVEC_SET1(ts->remain, spy);
            break;
        }
    }
    if (target >= g->players) {
        target = PLAYER_NONE;
        BOOLVEC_SET0(ts->answer, spy);
        BOOLVEC_SET0(ts->remain, spy);
    }
    ts->target[spy] = target;
    return (target != PLAYER_NONE);
}

/* -------------------------------------------------------------------------- */

void game_spy_sab_human(struct game_s *g)
{
    /* 1. Find target for each player
       2. If any targets, send sabotage ask to players and wait for answers
       3.1. Msg handler (sab): (don't) do sabotage and send done message, get next target if no framing
       3.2. Msg handler (frame): handle framing, get next target
       4. Wait for answers, go to 2 until all targets and messages handled
    */
    struct turn_sabotage_s *ts = &(g->gaux->turn.sabotage);
    BOOLVEC_CLEAR(ts->remain, PLAYER_NUM);
    BOOLVEC_CLEAR(ts->answer, PLAYER_NUM);
    BOOLVEC_CLEAR(ts->frame, PLAYER_NUM);
    g->gaux->turn_phase = GAME_TURN_SABOTAGE;
    for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
        if (IS_AI(g, spy) || (!IS_ALIVE(g, spy))) {
            ts->target[spy] = PLAYER_NONE;
        } else {
            ts->target[spy] = PLAYER_0;
            if (game_spy_sab_human_next_target(g, spy)) {
                game_spy_sab_human_sabq_msg(g, spy);
            }
        }
    }
    if (!BOOLVEC_IS_CLEAR(ts->remain, PLAYER_NUM)) {
        if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) { /* handle spies in series */
            for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
                game_server_msgo_flush_player(g, spy);
                while (BOOLVEC_IS1(ts->remain, spy)) {
                    game_server_msg_wait(g);
                    if (BOOLVEC_IS0(ts->answer, spy) && game_spy_sab_human_next_target(g, spy)) {
                        game_spy_sab_human_sabq_msg(g, spy);
                        game_server_msgo_flush_player(g, spy);
                    }
                }
            }
        } else { /* concurrent handling */
            game_server_msgo_flush(g);
            while (!BOOLVEC_IS_CLEAR(ts->remain, PLAYER_NUM)) {
                game_server_msg_wait(g);
                for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
                    if (BOOLVEC_IS0(ts->answer, spy) && game_spy_sab_human_next_target(g, spy)) {
                        game_spy_sab_human_sabq_msg(g, spy);
                        game_server_msgo_flush_player(g, spy);
                    }
                }
            }
        }
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (IS_AI(g, pi)) {
            continue;
        }
        for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
            int snum;
            snum = g->evn.sabotage_num[pi][spy];
            if ((pi != spy) && (snum > 0)) {
                sabotage_act_t act;
                act = g->evn.sabotage_act[pi][spy];
                if ((act == SABOTAGE_ACT_FACT) || (act == SABOTAGE_ACT_BASES)) {
                    uint8_t planet;
                    int spy2;
                    planet = g->evn.sabotage_planet[pi][spy];
                    spy2 = g->evn.sabotage_spy[pi][spy];
                    if (spy2 == -1) {
                        spy2 = PLAYER_NONE;
                    }
                    game_spy_sab_human_sabotage_msg_add(g, pi, spy2, pi, act, PLAYER_NONE, PLAYER_NONE, planet, snum, BOOLVEC_IS1(ts->answer, pi));
                    BOOLVEC_SET1(ts->answer, pi);
                }
            }
        }
        if (BOOLVEC_IS1(ts->answer, pi)) {
            game_spy_sab_human_sabotage_msg_finish(g, pi);
        }
    }
    if (!BOOLVEC_IS_CLEAR(ts->answer, PLAYER_NUM)) {
        game_server_msgo_flush(g);
    }
}

int game_spy_server_saba_msg(struct game_s *g, player_id_t pi)
{
    struct turn_sabotage_s *ts = &(g->gaux->turn.sabotage);
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    uint8_t target, act, pli;
    SG_1OOM_DE_U8(target);
    if (target >= g->players) {
        log_warning("%s: pi %i msg target %u >= %u\n", __func__, pi, target, g->players);
        return 2;
    }
    if (target != ts->target[pi]) {
        log_warning("%s: pi %i msg target %u != %u\n", __func__, pi, target, ts->target[pi]);
        return 2;
    }
    SG_1OOM_DE_U8(act);
    if (act > SABOTAGE_ACT_REVOLT) {
        log_warning("%s: pi %i msg invalid act %u\n", __func__, pi, act);
        return 2;
    }
    SG_1OOM_DE_U8(pli);
    if (act != SABOTAGE_ACT_NONE) {
        if ((pli >= g->galaxy_stars) || (g->planet[pli].owner != target)) {
            log_warning("%s: pi %i msg invalid planet %u\n", __func__, pi, pli);
            return 2;
        }
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    if (BOOLVEC_IS1(ts->frame, pi)) {
        log_warning("%s: pi %i msg during frame\n", __func__, pi);
        return 2;
    }
    LOG_DEBUG((DEBUGLEVEL_SAB, "%s: pi %i msg ok\n", __func__, pi));
    GAME_MSGI_HANDLED(g, pi);
    BOOLVEC_SET0(ts->answer, pi);
    game_server_msg_send_status(g, pi, 1);
    if (act != SABOTAGE_ACT_NONE) {
        planet_t *p = &(g->planet[pli]);
        player_id_t other1, other2;
        int snum = g->evn.sabotage_num[target][pi];
        g->evn.sabotage_planet[target][pi] = pli;
        g->evn.sabotage_act[target][pi] = act;
        switch (act) {
            case SABOTAGE_ACT_FACT:
                SETMIN(snum, p->factories);
                p->factories -= snum;
                break;
            case SABOTAGE_ACT_BASES:
                {
                    int n;
                    n = 0;
                    for (int i = 0; i < snum; ++i) {
                        if (!rnd_0_nm1(4, &g->seed)) {
                            ++n;
                        }
                    }
                    snum = n;
                }
                SETMIN(snum, p->missile_bases);
                p->missile_bases -= snum;
                break;
            case SABOTAGE_ACT_REVOLT:
                snum = (p->pop * snum) / 200;
                if (p->pop < snum) {
                    snum = p->pop - p->rebels;
                }
                SETMAX(snum, 1);
                p->rebels += snum;
                if (p->rebels >= (p->pop / 2)) {
                    p->unrest = PLANET_UNREST_REBELLION;
                    p->unrest_reported = false;
                    p->rebels = p->pop / 2;
                }
                break;
            case SABOTAGE_ACT_NONE:
            default:
                break;
        }
        other1 = PLAYER_NONE;
        other2 = PLAYER_NONE;
        if (act != SABOTAGE_ACT_REVOLT) {
            if (g->evn.spied_spy[target][pi] != -1) {
                game_diplo_act(g, -g->evn.spied_spy[target][pi], pi, target, 6, pli, act - 1/*MOO1 uses -1..2*/);
            } else if ((snum != 0) && (act > SABOTAGE_ACT_FACT)) { /* FIXME BUG? no framing for factory sabotage */
                const empiretechorbit_t *et;
                et = &(g->eto[target]);
                for (int i = 0; (i < g->players) && (other2 == PLAYER_NONE); ++i) {
                    if ((i != pi) && BOOLVEC_IS1(et->contact, i)) {
                        if (other1 == PLAYER_NONE) {
                            other1 = i;
                        } else {
                            other2 = i;
                        }
                    }
                }
                if (other2 == PLAYER_NONE) {
                    other1 = PLAYER_NONE;
                }
            }
        }
        ts->other[pi][0] = other1;
        ts->other[pi][1] = other2;
        BOOLVEC_SET1(p->explored, pi);
        g->seen[pi][pli].owner = p->owner;
        g->seen[pi][pli].pop = p->pop;
        g->seen[pi][pli].bases = p->missile_bases;
        g->seen[pi][pli].factories = p->factories;
        game_turn_explore_send_single(g, pi, pli);
        if (other2 != PLAYER_NONE) {
            BOOLVEC_SET1(ts->frame, pi);
            BOOLVEC_SET1(ts->answer, pi);
        } else {
            if (++ts->target[pi] >= g->players) {
                ts->target[pi] = PLAYER_NONE;
                BOOLVEC_SET0(ts->remain, pi);
            }
        }
        game_spy_sab_human_sabotage_msg_add(g, pi, pi, target, act, other1, other2, pli, snum, false);
        game_spy_sab_human_sabotage_msg_finish(g, pi);
        game_server_msgo_flush_player(g, pi);
    } else {
        if (++ts->target[pi] >= g->players) {
            ts->target[pi] = PLAYER_NONE;
            BOOLVEC_SET0(ts->remain, pi);
        }
    }
    return 0;
}

int game_spy_server_framesab_msg(struct game_s *g, player_id_t pi)
{
    struct turn_sabotage_s *ts = &(g->gaux->turn.sabotage);
    uint8_t target, victim;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
        SG_1OOM_DE_U8(target);
        SG_1OOM_DE_U8(victim);
        if (pos != len) {
            log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
        }
    }
    if (BOOLVEC_IS0(ts->frame, pi)) {
        log_warning("%s: pi %i msg not during frame\n", __func__, pi);
        return 2;
    }
    if (target != ts->target[pi]) {
        log_warning("%s: pi %i msg target %u != %u\n", __func__, pi, target, ts->target[pi]);
        return 2;
    }
    if (victim != PLAYER_NONE) {
        if ((victim == ts->other[pi][0]) || (victim == ts->other[pi][1])) {
            int v;
            g->evn.sabotage_spy[target][pi] = victim;
            v = -(rnd_1_n(12, &g->seed) + rnd_1_n(12, &g->seed));
            game_diplo_act(g, v, victim, target, 7, g->evn.sabotage_planet[target][pi], g->evn.sabotage_act[target][pi] - 1/*MOO1 uses -1..2*/);
        } else {
            log_warning("%s: pi %i msg target %u invalid victim %u\n", __func__, pi, target, victim);
            return 2;
        }
    }
    LOG_DEBUG((DEBUGLEVEL_SAB, "%s: pi %i msg ok\n", __func__, pi));
    BOOLVEC_SET0(ts->frame, pi);
    BOOLVEC_SET0(ts->answer, pi);
    if (++ts->target[pi] >= g->players) {
        ts->target[pi] = PLAYER_NONE;
        BOOLVEC_SET0(ts->remain, pi);
    }
    return 1;
}
