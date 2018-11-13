#include "config.h"

#include "game_ground.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_server.h"
#include "game_shiptech.h"
#include "game_spy.h"
#include "game_tech.h"
#include "log.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

static void game_ground_resolve_init(struct game_s *g, struct ground_s *gr)
{
    gr->seed = g->seed;
    gr->fact = g->planet[gr->planet_i].factories;
    for (int i = 0; i < 2; ++i) {
        const empiretechorbit_t *e = &(g->eto[gr->s[i].player]);
        const shipresearch_t *srd = &(g->srd[gr->s[i].player]);
        const uint8_t *rc;
        uint8_t bestarmor, bestsuit, bestshield, bestweap, besti;
        gr->s[i].human = IS_HUMAN(g, gr->s[i].player);
        gr->s[i].force = 0;
        bestarmor = 0;
        bestsuit = besti = 0;
        rc = &(srd->researchcompleted[TECH_FIELD_CONSTRUCTION][0]);
        for (int j = 0; j < e->tech.completed[TECH_FIELD_CONSTRUCTION]; ++j) {
            const uint8_t *r;
            r = RESEARCH_D0_PTR(g->gaux, TECH_FIELD_CONSTRUCTION, rc[j]);
            if (r[0] == 7) {
                bestarmor = r[1];
            } else if (r[0] == 0xf) {
                bestsuit = r[1];
                besti = rc[j];
            }
        }
        gr->s[i].force += bestarmor * 5 + bestsuit * 10;
        gr->s[i].armori = bestarmor;
        gr->s[i].suiti = besti;
        bestshield = besti = 0;
        rc = &(srd->researchcompleted[TECH_FIELD_FORCE_FIELD][0]);
        for (int j = 0; j < e->tech.completed[TECH_FIELD_FORCE_FIELD]; ++j) {
            const uint8_t *r;
            r = RESEARCH_D0_PTR(g->gaux, TECH_FIELD_FORCE_FIELD, rc[j]);
            if (r[0] == 0x10) {
                bestshield = r[1];
                besti = rc[j];
            }
        }
        gr->s[i].shieldi = besti;
        if (bestshield != 0) {
            gr->s[i].force += bestshield * 10;
        }
        bestweap = besti = 0;
        rc = &(srd->researchcompleted[TECH_FIELD_WEAPON][0]);
        for (int j = 0; j < e->tech.completed[TECH_FIELD_WEAPON]; ++j) {
            const uint8_t *r;
            r = RESEARCH_D0_PTR(g->gaux, TECH_FIELD_WEAPON, rc[j]);
            if (r[0] == 0x15) {
                bestweap = r[1];
                besti = rc[j];
            }
        }
        gr->s[i].weapi = besti;
        if (bestweap != 0) {
            if (bestweap > 2) {
                ++bestweap;
            }
            gr->s[i].force += bestweap * 5;
        }
        if (e->race == RACE_BULRATHI) {
            gr->s[i].force += 25;
        }
    }
    gr->s[gr->flag_swap ? 0 : 1].force += 5;
    for (int i = 0; i < TECH_SPY_MAX; ++i) {
        gr->got[i].tech = 0;
        gr->got[i].field = 0;
    }
}

static void game_ground_encode_first(struct game_s *g, player_id_t pi)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_GROUND);
}

static void game_ground_encode_start(struct game_s *g, player_id_t pi, const struct ground_s *gr)
{
    GAME_MSGO_EN_U8(g, pi, gr->planet_i);
    GAME_MSGO_EN_U8(g, pi, gr->s[0].player);
    GAME_MSGO_EN_U8(g, pi, gr->s[1].player);
    GAME_MSGO_EN_U8(g, pi, gr->flag_swap | (gr->flag_rebel ? 2 : 0));
    GAME_MSGO_EN_U32(g, pi, gr->seed);
    GAME_MSGO_EN_U16(g, pi, gr->inbound);
    GAME_MSGO_EN_U16(g, pi, gr->total_inbound);
    GAME_MSGO_EN_U16(g, pi, gr->fact);
    for (int i = 0; i < 2; ++i) {
        GAME_MSGO_EN_U16(g, pi, gr->s[i].force);
        GAME_MSGO_EN_U16(g, pi, gr->s[i].pop1);
        GAME_MSGO_EN_U8(g, pi, gr->s[i].armori);
        GAME_MSGO_EN_U8(g, pi, gr->s[i].suiti);
        GAME_MSGO_EN_U8(g, pi, gr->s[i].shieldi);
        GAME_MSGO_EN_U8(g, pi, gr->s[i].weapi);
    }
}

static void game_ground_encode_post(struct game_s *g, player_id_t pi, const struct ground_s *gr)
{
    GAME_MSGO_EN_U16(g, pi, gr->s[0].pop1);
    GAME_MSGO_EN_U16(g, pi, gr->s[1].pop1);
    for (int i = 0; i < TECH_SPY_MAX; ++i) {
        GAME_MSGO_EN_U8(g, pi, gr->got[i].tech);
        GAME_MSGO_EN_U8(g, pi, gr->got[i].field);
    }
}

static void game_ground_encode_end(struct game_s *g, player_id_t pi)
{
    GAME_MSGO_EN_U8(g, pi, 0xff);
    GAME_MSGO_EN_LEN(g, pi);
}

static void game_ground_finish(struct game_s *g, struct ground_s *gr)
{
    planet_t *p = &(g->planet[gr->planet_i]);
    struct spy_esp_s s[1];
    g->seed = gr->seed;
    gr->techchance = 0;
    if (gr->flag_swap) {
        int t;
        t = gr->s[0].pop1; gr->s[0].pop1 = gr->s[1].pop1; gr->s[1].pop1 = t;
        t = gr->s[0].player; gr->s[0].player = gr->s[1].player; gr->s[1].player = t;
    }
    if (gr->s[0].pop1 > 0) {
        if (gr->flag_rebel) {
            p->unrest = PLANET_UNREST_RESOLVED;
            p->pop = p->pop - p->rebels + gr->s[0].pop1;
            p->rebels = 0;
        } else {
            int fact, chance, num;
            fact = gr->fact;
            chance = 0;
            for (int i = 0; i < fact; ++i) {
                if (!rnd_0_nm1(50, &g->seed)) {
                    ++chance;
                }
            }
            gr->techchance = chance;
            s->target = gr->s[1].player;
            s->spy = gr->s[0].player;
            num = game_spy_esp_sub1(g, s, 0, 0);
            SETMIN(num, chance);
            s->tnum = num;
            for (int i = 0; i < num; ++i) {
                gr->got[i].tech = s->tbl_tech2[i];
                gr->got[i].field = s->tbl_field[i];
                game_tech_get_new(g, gr->s[0].player, s->tbl_field[i], s->tbl_tech2[i], TECHSOURCE_FOUND, gr->planet_i, gr->s[1].player, false);
            }
            gr->techchance = num;
            game_planet_destroy(g, gr->planet_i, gr->s[0].player);
            p->owner = gr->s[0].player;
            p->pop = gr->s[0].pop1;
            p->bc_to_refit = 0;
            p->pop_oper_fact = 1;
        }
    } else {
        if (gr->flag_rebel) {
            p->rebels = gr->s[1].pop1;
            if (p->rebels == 0) {
                p->unrest = PLANET_UNREST_RESOLVED;
            }
        } else {
            p->pop = gr->s[1].pop1;
        }
    }
    SETMIN(p->pop, p->max_pop3);
    if (gr->flag_swap) {
        int t;
        t = gr->s[0].pop1; gr->s[0].pop1 = gr->s[1].pop1; gr->s[1].pop1 = t;
        t = gr->s[0].player; gr->s[0].player = gr->s[1].player; gr->s[1].player = t;
    }
}

/* -------------------------------------------------------------------------- */

void game_ground_kill(struct ground_s *gr)
{
    int v1, v2, death;
    v1 = rnd_1_n(100, &gr->seed) + gr->s[0].force;
    v2 = rnd_1_n(100, &gr->seed) + gr->s[1].force;
    if (v1 <= v2) {
        death = 0;
    } else {
        death = 1;
    }
    gr->death = death;
    --gr->s[death].pop1;
}

int game_turn_ground_resolve_all(struct game_s *g)
{
    struct ground_s gr[1];
    BOOLVEC_DECLARE(msg_started, PLAYER_NUM);
    BOOLVEC_CLEAR(msg_started, PLAYER_NUM);
    gr->seed = g->seed;
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        player_id_t powner;
        powner = p->owner;
        for (player_id_t i = 0; i < g->players; ++i) {
            if ((i != powner) || (p->unrest == PLANET_UNREST_REBELLION)) {
                if ((powner != PLAYER_NONE) && (p->inbound[i] > 0)) {
                    int pop_planet;
                    gr->flag_rebel = (p->unrest == PLANET_UNREST_REBELLION) && IS_HUMAN(g, i) && (p->owner == i);
                    gr->inbound = p->inbound[i];
                    gr->total_inbound = p->total_inbound[i];
                    gr->s[0].pop2 = gr->s[0].pop1 = p->inbound[i];
                    pop_planet = gr->s[1].pop2 = gr->s[1].pop1 = gr->flag_rebel ? p->rebels : p->pop;
                    gr->s[0].player = i;
                    gr->s[1].player = powner;
                    gr->planet_i = pli;
                    if (IS_HUMAN(g, i) || IS_HUMAN(g, powner)) {
                        int t;
                        gr->flag_swap = true;
                        t = gr->s[0].pop2; gr->s[0].pop2 = gr->s[1].pop2; gr->s[1].pop2 = t;
                        t = gr->s[0].pop1; gr->s[0].pop1 = gr->s[1].pop1; gr->s[1].pop1 = t;
                        t = gr->s[0].player; gr->s[0].player = gr->s[1].player; gr->s[1].player = t;
                    } else {
                        gr->flag_swap = false;
                    }
                    game_ground_resolve_init(g, gr);
                    if ((gr->s[0].pop1 != 0) && (gr->s[1].pop1 != 0)) {
                        for (int k = 0; k < 2; ++k) {
                            player_id_t pi;
                            pi = gr->s[k].player;
                            if (gr->s[k].human && ((k == 0) || (gr->s[0].player != gr->s[1].player))) {
                                if (BOOLVEC_IS0(msg_started, pi)) {
                                    BOOLVEC_SET1(msg_started, pi);
                                    game_ground_encode_first(g, pi);
                                }
                                game_ground_encode_start(g, pi, gr);
                            }
                        }
                        while ((gr->s[0].pop1 != 0) && (gr->s[1].pop1 != 0)) {
                            game_ground_kill(gr);
                        }
                        game_ground_finish(g, gr);
                        for (int k = 0; k < 2; ++k) {
                            if (gr->s[k].human && ((k == 0) || (gr->s[0].player != gr->s[1].player))) {
                                game_ground_encode_post(g, gr->s[k].player, gr);
                            }
                        }
                        pop_planet -= gr->s[1].pop1;
                        SETMAX(pop_planet, 1);
                        game_ai->ground(g, gr->s[1].player, gr->s[0].player, pli, pop_planet, (p->owner != powner));
                    }
                }
                powner = p->owner;
            }
        }
    }
    if (!BOOLVEC_IS_CLEAR(msg_started, PLAYER_NUM)) {
        for (player_id_t pi = 0; pi < g->players; ++pi) {
            if (BOOLVEC_IS1(msg_started, pi)) {
                game_ground_encode_end(g, pi);
            }
        }
        game_server_msgo_flush(g);
    }
    return 0;
}
