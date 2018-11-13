#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_turn.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_server.h"
#include "game_shiptech.h"
#include "game_tech.h"
#include "log.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_BOMB 4

/* -------------------------------------------------------------------------- */

static bool game_turn_bomb_damage(struct game_s *g, uint8_t pli, player_id_t attacker)
{
    planet_t *p = &(g->planet[pli]);
    const empiretechorbit_t *ea = &(g->eto[attacker]);
    const empiretechorbit_t *ed = &(g->eto[p->owner]);
    uint32_t tbl[WEAPON_NUM];
    uint8_t pshield = p->shield, antidote = ed->antidote;
    int totaldmg = 0, totalbio = 0, maxcomp = 0, complevel;
    memset(tbl, 0, sizeof(tbl));
    for (int i = 0; i < ea->shipdesigns_num; ++i) {
        const shipdesign_t *sd = &(g->srd[attacker].design[i]);
        if ((!game_num_orbital_comp_fix) || (ea->orbit[pli].ships[i] != 0)) { /* WASBUG bonus from non-existing ships */
            SETMAX(maxcomp, sd->comp);
        }
        for (int j = 0; j < (game_num_orbital_weap_4 ? WEAPON_SLOT_NUM : (WEAPON_SLOT_NUM - 1)); ++j) { /* WASBUG? last weapon not used */
            weapon_t wpnt = sd->wpnt[j];
            uint32_t v;
            v = ea->orbit[pli].ships[i] * sd->wpnn[j];
            if (v != 0) {
                int ns;
                ns = tbl_shiptech_weap[wpnt].numshots;
                if (ns == -1) {
                    ns = 30;
                }
                v *= ns;
                tbl[wpnt] += v;
            }
        }
    }
    complevel = (maxcomp - 1) * 2 + 6;
    SETRANGE(complevel, 1, 20);
    for (int i = 0; i < (game_num_orbital_weap_any ? WEAPON_NUM : WEAPON_CRYSTAL_RAY); ++i) {  /* WASBUG? excludes death ray and amoeba stream too */
        uint32_t vcur = tbl[i];
        if (vcur != 0) {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[i]);
            uint32_t v;
            int dmgmin, dmgmax;
            SETMIN(vcur, game_num_max_bomb_dmg);
            if (vcur < 10) {
                v = 1;
            } else {
                v = vcur / 10;
                vcur = 10;
            }
            dmgmin = w->damagemin;
            dmgmax = w->damagemax;
            if (dmgmin == dmgmax) {
                if (w->is_bio) {
                    for (uint32_t n = 0; n < vcur; ++n) {
                        if (rnd_1_n(20, &g->seed) <= complevel) {
                            int dmg;
                            dmg = rnd_0_nm1(dmgmin + 1, &g->seed) - antidote;
                            SETMAX(dmg, 0);
                            totalbio += v * dmg;
                            SETMIN(totalbio, game_num_max_bio_dmg);
                        }
                    }
                } else {
                    if (game_num_orbital_torpedo == (w->misstype != 0)) { /* WASBUG damage halving for torpedo affected missiles instead */
                        dmgmax /= 2;
                    }
                    dmgmax *= w->nummiss;
                    if (dmgmax > pshield) {
                        for (uint32_t n = 0; n < vcur; ++n) {
                            if (rnd_1_n(20, &g->seed) <= complevel) {
                                int dmg;
                                dmg = dmgmax - pshield;
                                totaldmg += v * dmg;
                            }
                        }
                    }
                }
            } else {
                /*dd3d*/
                if (!w->is_bomb) {
                    dmgmin /= 2;
                    dmgmax /= 2;
                }
                if (dmgmax > pshield) {
                    int dmgrange;
                    dmgrange = dmgmax - dmgmin + 1;
                    dmgmin = dmgmin - 1 - pshield;
                    vcur = (complevel * vcur * v) / 20;
                    for (uint32_t n = 0; n < vcur; ++n) {
                        int dmg;
                        dmg = rnd_1_n(dmgrange, &g->seed) + dmgmin;
                        if (dmg > 0) {
                            totaldmg += dmg;
                        }
                    }
                }
            }
        }
    }
    {
        int v, h, hr, hb;
        h = game_num_fact_hp / 5;
        hr = h * 2;
        hb = game_num_fact_hp - h;
        v = totaldmg / (rnd_1_n(hr, &g->seed) + hb);
        SETMIN(v, p->factories);
        p->turn.bomb.factdmg = v;
    }
    {
        int v, h, hr, hb;
        h = game_num_pop_hp / 5;
        hr = h * 2;
        hb = game_num_pop_hp - h;
        v = totaldmg / (rnd_1_n(hr, &g->seed) + hb);
        v += totalbio;
        SETMIN(v, p->pop);
        p->turn.bomb.popdmg = v;
    }
    {
#if 0
        int v;
        v = p->max_pop3 - totalbio;
        SETMAX(v, 10);
        p->max_pop3 = v;    /* WASBUG reduced before y/n */
#endif
        p->turn.bomb.biodmg = totalbio;
    }
    return (p->turn.bomb.factdmg > 0) || (p->turn.bomb.popdmg > 0); /* FIXME biodmg? */
}

/* -------------------------------------------------------------------------- */

static bool game_turn_bomb_possible(const struct game_s *g, uint8_t pli, player_id_t attacker)
{
    const empiretechorbit_t *ea = &(g->eto[attacker]);
    bool flag_fleet = false, flag_treaty = false;
    player_id_t owner = g->planet[pli].owner;
    if ((owner == PLAYER_NONE) || (attacker == owner)) {
        return false;
    }
    for (int j = 0; j < ea->shipdesigns_num; ++j) {
        if (ea->orbit[pli].ships[j] > 0) {
            flag_fleet = true;
            break;
        }
    }
    if ((ea->treaty[owner] == TREATY_ALLIANCE) || ((ea->treaty[owner] == TREATY_NONAGGRESSION) && IS_AI(g, attacker))) {
        flag_treaty = true;
    }
    return flag_fleet && (!flag_treaty);
}

static int game_turn_bomb_inbound(const struct game_s *g, uint8_t pli, player_id_t attacker)
{
    int pop_inbound = 0;
    for (int j = 0; j < g->transport_num; ++j) {
        const transport_t *r = &(g->transport[j]);
        if ((r->owner == attacker) && (r->dest == pli)) {
            pop_inbound += r->pop;
        }
    }
    return pop_inbound;
}

static void game_turn_bombard_msg_add(const struct game_s *g, player_id_t pi, player_id_t att, uint8_t pli, bool msg_started)
{
    const planet_t *p = &(g->planet[pli]);
    if (!msg_started) {
        GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_BOMBARD);
    }
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_U8(g, pi, att);
    GAME_MSGO_EN_U8(g, pi, p->owner);
    GAME_MSGO_EN_U16(g, pi, p->turn.bomb.popdmg);
    GAME_MSGO_EN_U16(g, pi, p->turn.bomb.factdmg);
}

static inline void game_turn_bombard_msg_finish(const struct game_s *g, player_id_t pi)
{
    GAME_MSGO_EN_U8(g, pi, 0xff);
    GAME_MSGO_EN_LEN(g, pi);
}

static void game_turn_bombard_msg_first(const struct game_s *g, player_id_t pi, uint8_t pli)
{
    int len = GAME_MSGO_LEN(g, pi);
    uint8_t *data = GAME_MSGO_DPTR(g, pi);
    if (len == 0) { /* FIXME should not happen */
        game_turn_bombard_msg_add(g, pi, pi, pli, false);
    } else {
        len -= 4;
        memmove(&(data[7/*msglen*/]), data, len);
        g->gaux->msgo[pi].len = 4;
        game_turn_bombard_msg_add(g, pi, pi, pli, true);
        g->gaux->msgo[pi].len = len + 7/*msglen*/;
    }
}

static void game_turn_bombard_msg_flush(struct game_s *g, BOOLVEC_PTRPARAMI(msg_started))
{
    if (!BOOLVEC_IS_CLEAR(msg_started, PLAYER_NUM)) {
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            if (BOOLVEC_IS1(msg_started, pi)) {
                game_turn_bombard_msg_finish(g, pi);
            }
        }
        BOOLVEC_CLEAR(msg_started, PLAYER_NUM);
        game_server_msgo_flush(g);
    }
}

static void game_turn_bombard_msg_flush_player(struct game_s *g, player_id_t pi, BOOLVEC_PTRPARAMI(msg_started))
{
    if (BOOLVEC_IS1(msg_started, pi)) {
        BOOLVEC_SET0(msg_started, pi);
        game_turn_bombard_msg_finish(g, pi);
        game_server_msgo_flush_player(g, pi);
    }
}

static void game_turn_bombq_msg_add(const struct game_s *g, player_id_t pi, uint8_t pli, uint16_t inbound)
{
    const planet_t *p = &(g->planet[pli]);
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_BOMBQ);
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_U16(g, pi, p->pop);
    GAME_MSGO_EN_U16(g, pi, p->factories);
    GAME_MSGO_EN_U16(g, pi, inbound);
    GAME_MSGO_EN_LEN(g, pi);
}

static void game_turn_bomb_do(struct game_s *g, uint8_t pli, player_id_t attacker, BOOLVEC_PTRPARAMI(msg_started))
{
    planet_t *p = &(g->planet[pli]);
    player_id_t owner = p->owner;
    p->pop -= p->turn.bomb.popdmg;
    p->factories -= p->turn.bomb.factdmg;
    if (p->turn.bomb.biodmg) {
        int v;
        v = p->max_pop3 - p->turn.bomb.biodmg;
        SETMAX(v, 10);
        p->max_pop3 = v;
    }
    SUBSAT0(p->rebels, p->turn.bomb.popdmg / 2 + 1);
    if (p->pop == 0) {
        game_planet_destroy(g, pli, attacker);
    }
    if (IS_HUMAN(g, attacker)) {
        if (BOOLVEC_IS1(msg_started, attacker)) {
            /* put attacker message first so after clicking bomb the results for that planet are shown next */
            game_turn_bombard_msg_first(g, attacker, pli);
        } else {
            game_turn_bombard_msg_add(g, attacker, attacker, pli, false);
            BOOLVEC_SET1(msg_started, attacker);
        }
    }
    if (IS_HUMAN(g, owner)) {
        game_turn_bombard_msg_add(g, owner, attacker, pli, BOOLVEC_IS1(msg_started, owner));
        BOOLVEC_SET1(msg_started, owner);
    }
    if (IS_AI(g, owner)) {
        game_ai->bombed(g, owner, attacker, pli, p->turn.bomb.popdmg, p->turn.bomb.factdmg, p->turn.bomb.biodmg);
    }
}

static void game_turn_bomb_cond(struct game_s *g, player_id_t pi, uint8_t pli, bool flag_bomb)
{
    struct turn_pending_s *tp = &(g->gaux->turn.pending);
    planet_t *p = &(g->planet[pli]);
    --tp->num;
    BOOLVEC_SET0(tp->answer, pi);
    if (flag_bomb) {
        game_turn_bomb_do(g, pli, pi, tp->msg_started);
    }
    p->turn.bomb.bomber = PLAYER_NONE;
    if (p->owner != PLAYER_NONE) {
        /* find next player to bomb this planet */
        for (player_id_t i = pi + 1; i < g->players; ++i) {
            if (game_turn_bomb_possible(g, pli, i) && game_turn_bomb_damage(g, pli, i)) {
                if (IS_AI(g, i)) {
                    if (game_ai->bomb(g, i, pli, game_turn_bomb_inbound(g, pli, i))) {
                        game_turn_bomb_do(g, pli, i, tp->msg_started);
                    }
                } else {
                    p->turn.bomb.bomber = i;
                    if (tp->planet[i] == PLANET_NONE) {
                        tp->planet[i] = pli;
                        ++tp->num;
                    }
                    break;
                }
            }
        }
    }
    if (flag_bomb) {
        BOOLVEC_SET0(tp->msg_started, pi);
        game_turn_bombard_msg_finish(g, pi);
        game_server_msgo_flush_player(g, pi);
    }
    /* search next planet to bomb for player */
    for (uint8_t pli2 = 0; pli2 < g->galaxy_stars; ++pli2) {
        if (g->planet[pli2].turn.bomb.bomber == pi) {
            tp->planet[pi] = pli2;
            ++tp->num;
            break;
        }
    }
}

static void game_turn_bomb_concurrent(struct game_s *g, uint8_t plimin, uint8_t plimax)
{
    /* Concurrent multiplayer bombardment:
       1. Let AIs bomb planets unless player with lower ID has not bombed
       2. Send bombardment messages
       3. Send bomb questions, wait bomb answers
       4.1. Msg handler: (don't) bomb and send result to bomber
       4.2. Msg handler: If colony left, search next bomber for planet; if AI then let it bomb)
       4.3. Msg handler: Search next planet the player can bomb
       5. Send remaining bombardment messages
    */
    struct turn_pending_s *tp = &(g->gaux->turn.pending);
    BOOLVEC_CLEAR(tp->answer, PLAYER_NUM);

    LOG_DEBUG((DEBUGLEVEL_BOMB, "%s: pli:%i-%i\n", __func__, plimin, plimax));
    while (plimin <= plimax) {
        BOOLVEC_CLEAR(tp->msg_started, PLAYER_NUM);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            tp->planet[pi] = PLANET_NONE;
        }
        tp->num = 0;
        for (uint8_t pli = plimin; pli <= plimax; ++pli) {
            planet_t *p = &(g->planet[pli]);
            if ((p->owner == PLAYER_NONE) || (p->turn.bomb.bomber == PLAYER_NONE)) {
                if (plimin == pli) {
                    ++plimin;
                }
                if ((plimax == pli) && (plimax > 0)) {
                    --plimax;
                }
                continue;
            }
            for (player_id_t pi = p->turn.bomb.bomber; pi != PLAYER_NONE;) {
                if (game_turn_bomb_damage(g, pli, pi)) {
                    if (IS_AI(g, pi)) {
                        if (game_ai->bomb(g, pi, pli, game_turn_bomb_inbound(g, pli, pi))) {
                            game_turn_bomb_do(g, pli, pi, tp->msg_started);
                        }
                    } else {
                        if (tp->planet[pi] == PLANET_NONE) {
                            tp->planet[pi] = pli;
                            ++tp->num;
                        }
                        break;
                    }
                }
                while ((++pi < g->players) && (!game_turn_bomb_possible(g, pli, pi))) {
                }
                if (pi >= g->players) {
                    pi = PLAYER_NONE;
                    if (plimin == pli) {
                        ++plimin;
                    }
                    if ((plimax == pli) && (plimax > 0)) {
                        --plimax;
                    }
                }
                p->turn.bomb.bomber = pi;
            }
        }
        game_turn_bombard_msg_flush(g, tp->msg_started);
        if (tp->num == 0) {
            return;
        }
        while (tp->num) {
            for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
                uint8_t pli;
                pli = tp->planet[pi];
                if ((pli != PLANET_NONE) && BOOLVEC_IS0(tp->answer, pi)) {
                    BOOLVEC_SET1(tp->answer, pi);
                    game_turn_bombq_msg_add(g, pi, pli, game_turn_bomb_inbound(g, pli, pi));
                }
            }
            game_server_msgo_flush(g);
            game_server_msg_wait(g);
        }
        game_turn_bombard_msg_flush(g, tp->msg_started);
    }
}

static void game_turn_bomb_serial(struct game_s *g, uint8_t plimin, uint8_t plimax)
{
    /* Serial multiplayer bombardment:
       0. For each player {
       1. Let AIs bomb planets unless player with lower ID has not bombed
       2. Send bombardment messages to player
       3. For each planet ask player for bomb/no
       4. } Send remaining bombardment messages
    */
    struct turn_pending_s *tp = &(g->gaux->turn.pending);
    BOOLVEC_CLEAR(tp->answer, PLAYER_NUM);
    BOOLVEC_CLEAR(tp->msg_started, PLAYER_NUM);
    LOG_DEBUG((DEBUGLEVEL_BOMB, "%s: pli:%i-%i\n", __func__, plimin, plimax));
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
         tp->planet[pi] = PLANET_NONE;
    }
    for (player_id_t api = PLAYER_0; (api < g->players) && (plimin <= plimax); ++api) {
        if (IS_AI(g, api)) {
            continue;
        }
        tp->num = 0;
        for (uint8_t pli = plimin; pli <= plimax; ++pli) {
            planet_t *p = &(g->planet[pli]);
            if ((p->owner == PLAYER_NONE) || (p->turn.bomb.bomber == PLAYER_NONE)) {
                if (plimin == pli) {
                    ++plimin;
                }
                if ((plimax == pli) && (plimax > 0)) {
                    --plimax;
                }
                continue;
            }
            for (player_id_t pi = p->turn.bomb.bomber; pi != PLAYER_NONE;) {
                if (game_turn_bomb_damage(g, pli, pi)) {
                    if (IS_AI(g, pi)) {
                        if (game_ai->bomb(g, pi, pli, game_turn_bomb_inbound(g, pli, pi))) {
                            game_turn_bomb_do(g, pli, pi, tp->msg_started);
                        }
                    } else {
                        if (pi == api) {
                            ++tp->num;
                        }
                        break;
                    }
                }
                while ((++pi < g->players) && (!game_turn_bomb_possible(g, pli, pi))) {
                }
                if (pi >= g->players) {
                    pi = PLAYER_NONE;
                    if (plimin == pli) {
                        ++plimin;
                    }
                    if ((plimax == pli) && (plimax > 0)) {
                        --plimax;
                    }
                }
                p->turn.bomb.bomber = pi;
            }
        }
        game_turn_bombard_msg_flush_player(g, api, tp->msg_started);
        for (uint8_t pli = plimin; (pli <= plimax) && (tp->num > 0); ++pli) {
            const planet_t *p = &(g->planet[pli]);
            if (p->turn.bomb.bomber == api) {
                BOOLVEC_SET1(tp->answer, api);
                tp->planet[api] = pli;
                game_turn_bombq_msg_add(g, api, pli, game_turn_bomb_inbound(g, pli, api));
                game_server_msgo_flush_player(g, api);
                game_server_msg_wait(g);
            }
        }
    }
    game_turn_bombard_msg_flush(g, tp->msg_started);
}

/* -------------------------------------------------------------------------- */

void game_turn_bomb(struct game_s *g)
{
    uint8_t plimin = PLANET_NONE, plimax = PLANET_NONE;
    for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        p->turn.bomb.bomber = PLAYER_NONE;
        if (p->owner != PLAYER_NONE) {
            for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
                if (game_turn_bomb_possible(g, pli, pi)) {
                    p->turn.bomb.bomber = pi;
                    plimax = pli;
                    if (plimin == PLANET_NONE) {
                        plimin = pli;
                    }
                    break;
                }
            }
        }
    }
    LOG_DEBUG((DEBUGLEVEL_BOMB, "%s: pli:%i-%i mp:%i,%i\n", __func__, plimin, plimax, g->gaux->multiplayer, GAME_MULTIPLAYER_LOCAL));
    if (plimax == PLANET_NONE) {
        return;
    }
    if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) {
        game_turn_bomb_serial(g, plimin, plimax);
    } else {
        game_turn_bomb_concurrent(g, plimin, plimax);
    }
}

int game_turn_server_bomb_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    uint8_t planet_i, flag_bomb;
    SG_1OOM_DE_U8(planet_i);
    if (planet_i >= g->galaxy_stars) {
        log_warning("%s: pi %i msg planet %u >= %u\n", __func__, pi, planet_i, g->galaxy_stars);
        return 2;
    }
    if (planet_i != g->gaux->turn.pending.planet[pi]) {
        log_warning("%s: pi %i msg planet %u != %u\n", __func__, pi, planet_i, g->gaux->turn.pending.planet[pi]);
        return 2;
    }
    SG_1OOM_DE_U8(flag_bomb);
    if (flag_bomb > 1) {
        log_warning("%s: pi %i msg bomb %u > 1\n", __func__, pi, flag_bomb);
        return 2;
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_BOMB, "%s: pi %i msg ok\n", __func__, pi));
    GAME_MSGI_HANDLED(g, pi);
    game_turn_bomb_cond(g, pi, planet_i, flag_bomb == 1);
    g->gaux->turn.pending.planet[pi] = PLANET_NONE;
    return 1;
}
