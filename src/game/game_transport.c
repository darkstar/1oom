#include "config.h"

#include "game_transport.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_design.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_server.h"
#include "game_shiptech.h"
#include "game_tech.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static inline void game_turn_transport_msg_add(const struct game_s *g, player_id_t pi, uint8_t pli, player_id_t owner, bool msg_started)
{
    if (!msg_started) {
        GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_TRANSSHOT);
    }
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_U8(g, pi, owner);
}

static inline void game_turn_transport_msg_finish(const struct game_s *g, player_id_t pi, bool msg_started)
{
    if (msg_started) {
        GAME_MSGO_EN_U8(g, pi, 0xff);
        GAME_MSGO_EN_LEN(g, pi);
    }
}

static int game_turn_transport_send_msgs(struct game_s *g)
{
    BOOLVEC_DECLARE(msg_started, PLAYER_NUM);
    BOOLVEC_CLEAR(msg_started, PLAYER_NUM);
    for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
        const planet_t *p = &(g->planet[pli]);
        player_id_t powner;
        bool is_human;
        powner = p->owner;
        is_human = (powner != PLAYER_NONE) && IS_HUMAN(g, powner);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            if (BOOLVEC_IS1(p->turn.transport.destroyed, pi)) {
                if (is_human) {
                    game_turn_transport_msg_add(g, powner, pli, pi, BOOLVEC_IS1(msg_started, powner));
                    BOOLVEC_SET1(msg_started, powner);
                }
                if ((pi != powner) && IS_HUMAN(g, pi)) {
                    game_turn_transport_msg_add(g, pi, pli, pi, BOOLVEC_IS1(msg_started, pi));
                    BOOLVEC_SET1(msg_started, pi);
                }
            }
        }
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        game_turn_transport_msg_finish(g, pi, BOOLVEC_IS1(msg_started, pi));
    }
    if (!BOOLVEC_IS_CLEAR(msg_started, PLAYER_NUM)) {
        game_server_msgo_flush(g);
    }
    return 0;
}

static int game_turn_transport_shoot(struct game_s *g, uint8_t planet_i, player_id_t rowner, uint8_t speed, player_id_t attacker, int bases, weapon_t basewpnt)
{
    const planet_t *p = &(g->planet[planet_i]);
    const empiretechorbit_t *ea = &(g->eto[attacker]);
    const empiretechorbit_t *ed = &(g->eto[rowner]);
    int totaldmg = 0, complevel, killed;
    uint8_t bestcomp = 0, bestarmor = 0;
    uint32_t tbl[WEAPON_NUM];
    memset(tbl, 0, sizeof(tbl));
    tbl[basewpnt] = bases * 24;
    for (int i = 0; i < ea->shipdesigns_num; ++i) {
        const shipdesign_t *sd = &(g->srd[attacker].design[i]);
        uint8_t comp;
        comp = sd->comp;
        if (comp > speed) { /* FIXME BUG ? */
            bestcomp = comp;
        }
        for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
            weapon_t wpnt = sd->wpnt[j];
            uint32_t v;
            v = ea->orbit[planet_i].ships[i] * sd->wpnn[j];
            if (v != 0) {
                const struct shiptech_weap_s *w;
                int ns;
                w = &(tbl_shiptech_weap[wpnt]);
                ns = w->numshots;
                if (ns == -1) {
                    ns = 4;
                }
                v *= ns;
                if (w->nummiss != 0) {
                    v *= w->nummiss;
                }
                SETMIN(v, game_num_max_trans_dmg);
                if (w->is_bomb || w->is_bio) {
                    v = 0;
                }
                if (w->misstype != 0) {
                    v /= 2;
                }
                tbl[wpnt] += (v * 2) / speed;
            }
        }
    }
    if (bases > 0) {
        bestcomp = game_get_best_comp(g, rowner, ed->tech.percent[TECH_FIELD_COMPUTER]);  /* FIXME should be ea ? */
    }
    complevel = (bestcomp - speed) * 2 + 12;
    SETRANGE(complevel, 1, 20);
    for (int i = 0; i < (game_num_orbital_weap_any ? WEAPON_NUM : WEAPON_CRYSTAL_RAY); ++i) {  /* WASBUG? excludes death ray and amoeba stream too */
        uint32_t vcur = tbl[i];
        if (vcur != 0) {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[i]);
            int dmgmin, dmgmax;
            dmgmin = w->damagemin;
            dmgmax = w->damagemax;
            if (dmgmin == dmgmax) {
                for (uint32_t n = 0; n < vcur; ++n) {
                    if (rnd_1_n(20, &g->seed) <= complevel) {
                        totaldmg += dmgmax;
                    }
                }
            } else {
                /*7c47d*/
                int dmgrange;
                dmgrange = dmgmax - dmgmin + 1;
                --dmgmin;
                vcur = (complevel * vcur) / 20;
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
    /*7c521*/
    {
        const uint8_t *rc = &(g->srd[rowner].researchcompleted[TECH_FIELD_CONSTRUCTION][0]);
        for (int i = 0; i < ed->tech.completed[TECH_FIELD_CONSTRUCTION]; ++i) {
            const uint8_t *r;
            r = RESEARCH_D0_PTR(g->gaux, TECH_FIELD_CONSTRUCTION, rc[i]);
            if (r[0] == 7) {
                bestarmor = r[1];
            }
        }
    }
    killed = totaldmg / ((bestarmor + 1) * 15);
    SETMIN(killed, 1000);
    for (monster_id_t i = MONSTER_CRYSTAL; i <= MONSTER_AMOEBA; ++i) {
        monster_t *m;
        m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
        if (m->exists && (m->killer == PLAYER_NONE) && (m->x == p->x) && (m->y == p->y)){
            killed = 1000;
        }
    }
    return killed;
}

/* -------------------------------------------------------------------------- */

int game_turn_transport(struct game_s *g)
{
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            p->inbound[i] = 0;
            p->total_inbound[i] = 0;
        }
        BOOLVEC_CLEAR(p->turn.transport.destroyed, PLAYER_NUM);
    }
    for (int i = 0; i < g->transport_num; ++i) {
        transport_t *r = &(g->transport[i]);
        planet_t *p;
        uint8_t dest;
        dest = r->dest;
        p = &(g->planet[dest]);
        if ((r->x == p->x) && (r->y == p->y)) {
            int pop2, pop3;
            player_id_t owner;
            owner = r->owner;
            pop2 = pop3 = r->pop;
            for (int j = 0; j < g->players; ++j) {
                treaty_t t;
                if (j == owner) {
                    continue;
                }
                t = g->eto[owner].treaty[j];
                if ((j == p->owner) || (t == TREATY_NONE) || (t >= TREATY_WAR)) {
                    empiretechorbit_t *e;
                    e = &(g->eto[j]);
                    if (j == p->owner) {
                        pop3 -= game_turn_transport_shoot(g, dest, owner, r->speed, j, p->missile_bases, e->base_weapon);
                    } else {
                        /*e102*/
                        bool any_ships;
                        any_ships = false;
                        for (int k = 0; k < e->shipdesigns_num; ++k) {
                            if (e->orbit[dest].ships[k] > 0) {
                                any_ships = true;
                                break;
                            }
                        }
                        if (any_ships) {
                            pop3 -= game_turn_transport_shoot(g, dest, owner, r->speed, j, 0, WEAPON_NONE);
                        }
                    }
                }
            }
            if (g->evn.have_guardian && (dest == g->evn.planet_orion_i)) {
                pop3 = 0;
            }
            for (monster_id_t i = MONSTER_CRYSTAL; i <= MONSTER_AMOEBA; ++i) {
                monster_t *m;
                m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
                if (m->exists && /*(m->killer == PLAYER_NONE) &&*/ (m->x == p->x) && (m->y == p->y)) { /* FIXME dead monster kills transports ? */
                    pop3 = 0;
                }
            }
            SETMAX(pop3, 0);
            if (g->eto[owner].have_combat_transporter) {
                int n;
                n = pop2 - pop3;
                if (game_num_combat_trans_fix) {    /* do as OSG says: 50% chance, 25% if interdictor */
                    int c;
                    c = g->eto[p->owner].have_sub_space_int ? 4 : 2;
                    for (int j = 0; j < n; ++j) {
                        if (!rnd_0_nm1(c, &g->seed)) {
                            ++pop3;
                        }
                    }
                } else if (g->eto[p->owner].have_sub_space_int) {   /* WASBUG transporters only work when planet owner has interdictor */
                    for (int j = 0; j < n; ++j) {
                        if (!rnd_0_nm1(4, &g->seed)) {
                            ++pop3;
                        } else if (!rnd_0_nm1(2, &g->seed)) {
                            ++pop3;
                        }
                    }
                }
            }
            /*e2a4*/
            if (pop3 <= 0) {
                if (IS_HUMAN(g, owner) || (p->owner == PLAYER_NONE) || IS_HUMAN(g, p->owner)) {
                    BOOLVEC_SET1(p->turn.transport.destroyed, owner);
                }
            } else {
                /*e3fe*/
#if 0
                if ((p->owner == PLAYER_NONE) || (p->pop == 0)) { /* never true (tested above) */
                    /* ignored */
                } else
#endif
                if (p->owner == owner) {
                    if (p->unrest == PLANET_UNREST_REBELLION) {
                        ADDSATT(p->inbound[owner], pop3, game_num_max_inbound);
                        p->total_inbound[owner] += pop2;
                    } else {
                        ADDSATT(p->pop, pop3, p->max_pop3);
                    }
                } else {
                    /*e5a6*/
                    if (g->eto[owner].treaty[p->owner] != TREATY_ALLIANCE) {
                        ADDSATT(p->inbound[owner], pop3, game_num_max_inbound);
                        p->total_inbound[owner] += pop2;
                    }
                }
            }
            /*e639*/
            util_table_remove_item_any_order(i, g->transport, sizeof(transport_t), g->transport_num);
            --g->transport_num;
            --i;
        }
    }
    return game_turn_transport_send_msgs(g);
}
