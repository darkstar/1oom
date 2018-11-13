#include "config.h"

#include "game_fleet.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_num.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_FLEET    4

/* -------------------------------------------------------------------------- */

int game_fleet_server_send_fleet_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    shipcount_t ships[NUM_SHIPDESIGNS];
    uint8_t from, dest;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_SEND_FLEET)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    SG_1OOM_DE_U8(from);
    SG_1OOM_DE_U8(dest);
    if (from >= g->galaxy_stars) {
        log_warning("%s: pi %i msg from %u >= %u\n", __func__, pi, from, g->galaxy_stars);
        return 2;
    }
    if (dest >= g->galaxy_stars) {
        log_warning("%s: pi %i msg dest %u >= %u\n", __func__, pi, dest, g->galaxy_stars);
        return 2;
    }
    {
        const shipcount_t *os = &(g->eto[pi].orbit[from].ships[0]);
        const bool *hrf = &(g->srd[pi].have_reserve_fuel[0]);
        bool have_reserve_fuel = true;
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            SG_1OOM_DE_U16(ships[i]);
            if (ships[i] > os[i]) {
                log_warning("%s: pi %i msg ship %i amount %u > %u\n", __func__, pi, i, ships[i], os[i]);
                return 2;
            }
            if ((ships[i] > 0) && (!hrf[i])) {
                have_reserve_fuel = false;
            }
        }
        {
            const planet_t *p = &(g->planet[dest]);
            if (!((p->within_frange[pi] == 1) || (have_reserve_fuel && (p->within_frange[pi] == 2)))) {
                log_warning("%s: pi %i msg dest %u not in range (%i, hrf:%i)\n", __func__, pi, dest, p->within_frange[pi], have_reserve_fuel);
                return 2;
            }
        }
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_FLEET, "%s: pi %i msg ok\n", __func__, pi));
    if (game_send_fleet_from_orbit(g, pi, from, dest, ships)) {
        LOG_DEBUG((DEBUGLEVEL_FLEET, "%s: pi %i send ok\n", __func__, pi));
        return 1;
    } else {
        return 2;
    }
}

int game_fleet_server_redir_fleet_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    fleet_enroute_t ri;
    uint8_t dest;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_REDIR_FLEET)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    ri.owner = pi;
    SG_1OOM_DE_U8(dest);
    if (dest >= g->galaxy_stars) {
        log_warning("%s: pi %i msg dest %u >= %u\n", __func__, pi, dest, g->galaxy_stars);
        return 2;
    }
    SG_1OOM_DE_U8(ri.dest);
    if (dest == ri.dest) {
        log_warning("%s: pi %i msg dest %u is same\n", __func__, pi, dest);
        return 1;
    }
    SG_1OOM_DE_U16(ri.x);
    SG_1OOM_DE_U16(ri.y);
    {
        const bool *hrf = &(g->srd[pi].have_reserve_fuel[0]);
        bool have_reserve_fuel = true;
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            SG_1OOM_DE_U16(ri.ships[i]);
            if ((ri.ships[i] > 0) && (!hrf[i])) {
                have_reserve_fuel = false;
            }
        }
        {
            const planet_t *p = &(g->planet[dest]);
            if (!((p->within_frange[pi] == 1) || (have_reserve_fuel && (p->within_frange[pi] == 2)))) {
                log_warning("%s: pi %i msg dest %u not in range (%i, hrf:%i)\n", __func__, pi, dest, p->within_frange[pi], have_reserve_fuel);
                return 2;
            }
        }
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        fleet_enroute_t *r = &(g->enroute[i]);
        if (1
          && (r->owner == pi) && (r->dest = ri.dest) && (r->x == ri.x) && (r->y == ri.y)
          && (r->ships[0] == ri.ships[0])
          && (r->ships[1] == ri.ships[1])
          && (r->ships[2] == ri.ships[2])
          && (r->ships[3] == ri.ships[3])
          && (r->ships[4] == ri.ships[4])
          && (r->ships[5] == ri.ships[5])
        ) {
            uint8_t from = PLANET_NONE;
            for (int j = 0; j < g->galaxy_stars; ++j) {
                const planet_t *p = &(g->planet[j]);
                if ((p->x == r->x) && (p->y == r->y)) {
                    from = j;
                    break;
                }
            }
            if ((!g->eto[pi].have_hyperspace_comm) && (from == PLANET_NONE)) {
                log_warning("%s: pi %i msg can't redirect (no hypercomm, not on planet)\n", __func__, pi);
                return 2;
            }
            if (game_num_retreat_redir_fix && r->retreat && (!g->eto[pi].have_hyperspace_comm)) {
                log_warning("%s: pi %i msg retreat fix and no hypercomm\n", __func__, pi);
                return 2;
            }
            LOG_DEBUG((DEBUGLEVEL_FLEET, "%s: pi %i msg ok\n", __func__, pi));
            game_fleet_redirect(g, r, from, dest);
            return 1;
        }
    }
    log_warning("%s: pi %i msg fleet { dest:%u, x:%i, y:%i, %i %i %i %i %i %i } not found\n", __func__, pi, ri.dest, ri.x, ri.y, ri.ships[0], ri.ships[1], ri.ships[2], ri.ships[3], ri.ships[4], ri.ships[5]);
    return 2;
}

int game_fleet_server_redir_trans_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    transport_t ri;
    uint8_t dest;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_REDIR_TRANS)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    if (!g->eto[pi].have_hyperspace_comm) {
        log_warning("%s: pi %i msg can't redirect (no hypercomm)\n", __func__, pi);
        return 2;
    }
    ri.owner = pi;
    SG_1OOM_DE_U8(dest);
    if (dest >= g->galaxy_stars) {
        log_warning("%s: pi %i msg dest %u >= %u\n", __func__, pi, dest, g->galaxy_stars);
        return 2;
    }
    if (!game_transport_dest_ok(g, &(g->planet[dest]), pi)) {
        log_warning("%s: pi %i msg dest %u is not ok\n", __func__, pi, dest);
        return 2;
    }
    SG_1OOM_DE_U8(ri.dest);
    if (dest == ri.dest) {
        log_warning("%s: pi %i msg dest %u is same\n", __func__, pi, dest);
        return 1;
    }
    SG_1OOM_DE_U16(ri.x);
    SG_1OOM_DE_U16(ri.y);
    SG_1OOM_DE_U16(ri.pop);
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    for (int i = 0; i < g->transport_num; ++i) {
        transport_t *r = &(g->transport[i]);
        if (1
          && (r->owner == pi) && (r->dest = ri.dest) && (r->x == ri.x) && (r->y == ri.y)
          && (r->pop == ri.pop)
        ) {
            LOG_DEBUG((DEBUGLEVEL_FLEET, "%s: pi %i msg ok\n", __func__, pi));
            game_transport_redirect(g, r, dest);
            return 1;
        }
    }
    log_warning("%s: pi %i msg fleet { dest:%u, x:%i, y:%i, pop:%i } not found\n", __func__, pi, ri.dest, ri.x, ri.y, ri.pop);
    return 2;
}
