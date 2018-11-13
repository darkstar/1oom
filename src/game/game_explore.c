#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_explore.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_debug.h"
#include "game_endecode.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_shiptech.h"
#include "game_server.h"
#include "game_tech.h"
#include "game_view.h"
#include "log.h"
#include "types.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_EXPLORE  4

/* -------------------------------------------------------------------------- */

static bool game_turn_check_in_orbit(const struct game_s *g, uint8_t pli, player_id_t pi)
{
    const empiretechorbit_t *e = &(g->eto[pi]);
    const shipcount_t *os = &(e->orbit[pli].ships[0]);
    int sd_num = e->shipdesigns_num;
    for (int j = 0; j < sd_num; ++j) {
        if (os[j] != 0) {
            return true;
        }
    }
    return false;
}

static bool game_turn_check_scanner(const struct game_s *g, uint8_t pli, player_id_t pi)
{
    const empiretechorbit_t *e = &(g->eto[pi]);
    if (e->have_adv_scanner) {
        const planet_t *p = &(g->planet[pli]);
        for (int pli2 = 0; pli2 < g->galaxy_stars; ++pli2) {
            const planet_t *p2 = &(g->planet[pli2]);
            if ((p2->owner == pi) && (util_math_dist_fast(p->x, p->y, p2->x, p2->y) <= game_num_adv_scan_range)) {
                return true;
            }
        }
    }
    return false;
}

static int game_turn_check_colonize(const struct game_s *g, uint8_t pli, player_id_t pi)
{
    const planet_t *p = &(g->planet[pli]);
    const empiretechorbit_t *e = &(g->eto[pi]);
    int sd_num = e->shipdesigns_num;
    int best_colonize = 200, best_colonyship = -1;
    if ((p->owner != PLAYER_NONE) || (p->type == PLANET_TYPE_NOT_HABITABLE) || (!game_turn_check_in_orbit(g, pli, pi))) {
        return -1;
    }
    for (int j = 0; j < sd_num; ++j) {
        const shipdesign_t *sd = &(g->srd[pi].design[j]);
        int can_colonize;
        can_colonize = 200;
        for (int k = 0; k < SPECIAL_SLOT_NUM; ++k) {
            ship_special_t s;
            s = sd->special[k];
            if ((s >= SHIP_SPECIAL_STANDARD_COLONY_BASE) && (s <= SHIP_SPECIAL_RADIATED_COLONY_BASE)) {
                can_colonize = PLANET_TYPE_MINIMAL - (s - SHIP_SPECIAL_STANDARD_COLONY_BASE);
            }
        }
        if ((can_colonize < 200) && (e->orbit[pli].ships[j] > 0)) {
            if (can_colonize < best_colonize) {
                best_colonize = can_colonize;
                best_colonyship = j;
            }
            if (e->race == RACE_SILICOID) {
                best_colonize = PLANET_TYPE_RADIATED;
            }
        }
    }
    if ((best_colonize == 200) || (p->type < best_colonize)) {
        return -1;
    }
    return best_colonyship;
}

static void game_turn_explore_msg_add(const struct game_s *g, player_id_t pi, uint8_t pli, bool update_only, bool by_scanner, int colony_ship, bool msg_started)
{
    const planet_t *p = &(g->planet[pli]);
    if (!msg_started) {
        GAME_MSGO_EN_HDR(g, pi, (colony_ship < 0) ? GAME_MSG_ID_EXPLORE : GAME_MSG_ID_COLONIZEQ);
    }
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_TBL_U8(g, pi, p->name, PLANET_NAME_LEN);
    GAME_MSGO_EN_U16(g, pi, p->max_pop3);
    GAME_MSGO_EN_U16(g, pi, p->factories);
    GAME_MSGO_EN_U16(g, pi, p->missile_bases);
    GAME_MSGO_EN_U16(g, pi, p->waste);
    GAME_MSGO_EN_U8(g, pi, p->type);
    GAME_MSGO_EN_U8(g, pi, p->infogfx);
    GAME_MSGO_EN_U8(g, pi, p->growth);
    GAME_MSGO_EN_U8(g, pi, p->special);
    GAME_MSGO_EN_U8(g, pi, p->owner);
    GAME_MSGO_EN_U8(g, pi, p->unrest);
    {
        uint8_t v = (colony_ship & 7) | (by_scanner ? 0x80 : 0) | (update_only ? 0x40 : 0);
        GAME_MSGO_EN_U8(g, pi, v);
    }
}

static void game_turn_explore_msg_finish(const struct game_s *g, player_id_t pi, bool colonize)
{
    if (!colonize) {
        GAME_MSGO_EN_U8(g, pi, 0xff);
    }
    GAME_MSGO_EN_LEN(g, pi);
}

static void game_turn_explore_do(struct game_s *g)
{
    BOOLVEC_DECLARE(msg_started, PLAYER_NUM);
    BOOLVEC_CLEAR(msg_started, PLAYER_NUM);
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (BOOLVEC_IS0(p->explored, i) && (p->turn.explore.colonize == PLAYER_NONE)) {
                bool flag_visible, by_scanner;
                flag_visible = game_turn_check_in_orbit(g, pli, i);
                by_scanner = false;
                if (!flag_visible) {
                    flag_visible = game_turn_check_scanner(g, pli, i);
                    by_scanner = true;
                }
                if (flag_visible) {
                    bool first, was_explored;
                    /* FIXME artifacts disappearing due to scanning a planet is weird */
                    first = BOOLVEC_IS_CLEAR(p->explored, PLAYER_NUM);
                    if ((p->special == PLANET_SPECIAL_ARTIFACTS) && (!by_scanner) && first) {
                        /* FIXME? AIs (last player ID) win on simultaneous explore */
                        p->artifact_looter = i;
                    }
                    was_explored = BOOLVEC_IS1(p->explored, i);
                    BOOLVEC_SET1(p->explored, i);
                    if (IS_HUMAN(g, i) && (!was_explored)) {
                        game_turn_explore_msg_add(g, i, pli, false, by_scanner, -1, BOOLVEC_IS1(msg_started, i));
                        BOOLVEC_SET1(msg_started, i);
                    }
                }
            }
        }
    }
    if (!BOOLVEC_IS_CLEAR(msg_started, PLAYER_NUM)) {
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (BOOLVEC_IS1(msg_started, i)) {
                game_turn_explore_msg_finish(g, i, false);
            }
        }
        game_server_msgo_flush(g);
    }
}

static void game_turn_colonize(struct game_s *g, uint8_t pli)
{
    const planet_t *p = &(g->planet[pli]);
    player_id_t pi = p->turn.explore.colonize;
    game_planet_colonize(g, pli, pi, p->turn.explore.colony_ship);
    if ((pli == g->evn.planet_orion_i) && game_num_news_orion) {
        g->evn.have_orion_conquer = pi + 1;
    }
}

static void game_turn_colonize_cond(struct game_s *g, player_id_t pi, uint8_t pli, bool flag_colonize)
{
    struct turn_pending_s *tp = &(g->gaux->turn.pending);
    --tp->num;
    BOOLVEC_SET0(tp->answer, pi);
    if (flag_colonize) {
        game_turn_colonize(g, pli);
    } else {
        planet_t *p = &(g->planet[pli]);
        p->turn.explore.colonize = PLAYER_NONE;
        BOOLVEC_SET1(p->explored, pi);
        for (player_id_t i = pi + 1; i < g->players; ++i) {
            int colshipi;
            colshipi = game_turn_check_colonize(g, pli, i);
            if (colshipi >= 0) {
                p->turn.explore.colonize = i;
                p->turn.explore.colony_ship = colshipi;
                ++tp->num;
                break;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

int game_turn_explore(struct game_s *g)
{
    /* 1. Check which planets need "colonize?" question(s)
       2. Send explore messages for all the other planets
       3. Send colonize questions
       4. Wait colonize answers (goto 3 if "no" and colony ships from other players in orbit)
       5. Send explore messages for colonized planets
    */
    struct turn_pending_s *tp = &(g->gaux->turn.pending);
    uint8_t plimin = PLANET_NONE, plimax = PLANET_NONE;
    BOOLVEC_DECLARE(msg_started, PLAYER_NUM);
    BOOLVEC_CLEAR(msg_started, PLAYER_NUM);
    BOOLVEC_CLEAR(tp->answer, PLAYER_NUM);
    tp->num = 0;

    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        p->artifact_looter = PLAYER_NONE;
        p->turn.explore.colonize = PLAYER_NONE;
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            int colshipi;
            colshipi = game_turn_check_colonize(g, pli, pi);
            if (colshipi >= 0) {
                p->turn.explore.colonize = pi;
                p->turn.explore.colony_ship = colshipi;
                ++tp->num;
                plimax = pli;
                if (plimin == PLANET_NONE) {
                    plimin = pli;
                }
                break;
            }
        }
    }

    game_turn_explore_do(g);

    while (tp->num) {
        for (int pli = plimin; pli <= plimax; ++pli) {
            planet_t *p = &(g->planet[pli]);
            player_id_t pi;
            pi = p->turn.explore.colonize;
            if ((pi != PLAYER_NONE) && BOOLVEC_IS0(tp->answer, pi)) {
                if (IS_HUMAN(g, pi)) {
                    BOOLVEC_SET1(tp->answer, pi);
                    tp->planet[pi] = pli;
                    game_turn_explore_msg_add(g, pi, pli, false, false, p->turn.explore.colony_ship, false);
                    game_turn_explore_msg_finish(g, pi, false);
                } else {
                    game_turn_colonize(g, pli);
                    --tp->num;
                }
            }
        }
        game_server_msgo_flush(g);
        game_server_msg_wait(g);
    }

    game_turn_explore_do(g);
    return 0;
}

int game_turn_explore_send_single(struct game_s *g, player_id_t pi, uint8_t pli)
{
    game_turn_explore_msg_add(g, pi, pli, true, false, -1, false);
    game_turn_explore_msg_finish(g, pi, false);
    game_server_msgo_flush_player(g, pi);
    return 0;
}

int game_turn_server_colonize_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    planet_t *p;
    uint8_t planet_i, flag_colonize;
    SG_1OOM_DE_U8(planet_i);
    if (planet_i >= g->galaxy_stars) {
        log_warning("%s: pi %i msg planet %u >= %u\n", __func__, pi, planet_i, g->galaxy_stars);
        return 2;
    }
    if (planet_i != g->gaux->turn.pending.planet[pi]) {
        log_warning("%s: pi %i msg planet %u != %u\n", __func__, pi, planet_i, g->gaux->turn.pending.planet[pi]);
        return 2;
    }
    SG_1OOM_DE_U8(flag_colonize);
    if (flag_colonize > 1) {
        log_warning("%s: pi %i msg colonize %u > 1\n", __func__, pi, flag_colonize);
        return 2;
    }
    p = &(g->planet[planet_i]);
    {
        char name[PLANET_NAME_LEN];
        SG_1OOM_DE_TBL_U8(name, PLANET_NAME_LEN);
        if (name[0] != '\0') {
            name[PLANET_NAME_LEN - 1] = '\0';
            util_trim_whitespace(name);
            if (name[0] != '\0') {
                for (int i = 0; i < PLANET_NAME_LEN - 1; ++i) {
                    char c;
                    c = name[i];
                    if (c == 0) {
                        break;
                    }
                    if ((c < 0x20) || (c > 0x7e)) {
                        log_warning("%s: pi %i msg invalid char %i\n", __func__, pi, c);
                        return 2;
                    }
                }
                memcpy(p->name, name, PLANET_NAME_LEN - 1);
            }
        }
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_EXPLORE, "%s: pi %i msg ok\n", __func__, pi));
    game_turn_colonize_cond(g, pi, planet_i, flag_colonize == 1);
    g->gaux->turn.pending.planet[pi] = PLANET_NONE;
    return 1;
}
