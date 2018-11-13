#include "config.h"

#include "game_planet.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_num.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_PLANET   4

/* -------------------------------------------------------------------------- */

int game_planet_server_send_bc_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    uint32_t bc;
    uint8_t planet_i;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_SEND_BC)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    SG_1OOM_DE_U8(planet_i);
    if (planet_i >= g->galaxy_stars) {
        log_warning("%s: pi %i msg planet %u >= %u\n", __func__, pi, planet_i, g->galaxy_stars);
        return 2;
    }
    if (g->planet[planet_i].owner != pi) {
        log_warning("%s: pi %i msg planet owner %u is not player\n", __func__, pi, planet_i, g->planet[planet_i].owner);
        return 2;
    }
    SG_1OOM_DE_U32(bc);
    if (bc > g->eto[pi].reserve_bc) {
        log_warning("%s: pi %i msg bc %u > %u reserve\n", __func__, pi, bc, g->eto[pi].reserve_bc);
        return 2;
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_PLANET, "%s: pi %i msg ok\n", __func__, pi));
    game_planet_send_bc(g, pi, planet_i, bc);
    return 1;
}

int game_planet_server_scrap_bases_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    planet_t *p;
    uint16_t num;
    uint8_t planet_i;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_SCRAP_BASES)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    SG_1OOM_DE_U8(planet_i);
    if (planet_i >= g->galaxy_stars) {
        log_warning("%s: pi %i msg planet %u >= %u\n", __func__, pi, planet_i, g->galaxy_stars);
        return 2;
    }
    p = &(g->planet[planet_i]);
    if (p->owner != pi) {
        log_warning("%s: pi %i msg planet owner %u is not player\n", __func__, pi, planet_i, p->owner);
        return 2;
    }
    SG_1OOM_DE_U16(num);
    if (num > p->missile_bases) {
        log_warning("%s: pi %i msg num %u > %u bases\n", __func__, pi, num, p->missile_bases);
        return 2;
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_PLANET, "%s: pi %i msg ok\n", __func__, pi));
    game_planet_scrap_bases(g, pi, planet_i, num);
    return 1;
}
