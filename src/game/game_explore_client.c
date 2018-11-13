#include "config.h"

#include "game_explore.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_num.h"
#include "log.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_EXPLORE  4

/* -------------------------------------------------------------------------- */

static uint8_t game_turn_explore_decode(struct game_s *g, player_id_t pi, const uint8_t **data_ptr, uint8_t *exploreinfoptr)
{
    const uint8_t *buf = *data_ptr;
    int pos = 0;
    uint8_t pli;
    SG_1OOM_DE_U8(pli);
    if (pli != 0xff) {
        planet_t *p = &(g->planet[pli]);
        SG_1OOM_DE_TBL_U8(p->name, PLANET_NAME_LEN);
        SG_1OOM_DE_U16(p->max_pop3);
        SG_1OOM_DE_U16(p->factories);
        SG_1OOM_DE_U16(p->missile_bases);
        SG_1OOM_DE_U16(p->waste);
        SG_1OOM_DE_U8(p->type);
        SG_1OOM_DE_U8(p->infogfx);
        SG_1OOM_DE_U8(p->growth);
        SG_1OOM_DE_U8(p->special);
        SG_1OOM_DE_U8(p->owner);
        SG_1OOM_DE_U8(p->unrest);
        {
            uint8_t v;
            SG_1OOM_DE_U8(v);
            *exploreinfoptr = v;
        }
        BOOLVEC_SET1(p->explored, pi);
    }
    *data_ptr = buf + pos;
    return pli;
}

/* -------------------------------------------------------------------------- */

int game_turn_explore_show(struct game_s *g, player_id_t pi)
{
    const uint8_t *p;
    p = GAME_MSGI_DPTR(g, pi);
    while (1) {
        uint8_t pli, exploreinfo = 0;
        pli = game_turn_explore_decode(g, pi, &p, &exploreinfo);
        if (pli == PLANET_NONE) {
            break;
        } else {
            game_update_visibility(g);
            if ((exploreinfo & 0x40) == 0) {
                ui_explore(g, pi, pli, (exploreinfo & 0x80) != 0, false);
            }
        }
    }
    return 0;
}

int game_turn_colonize_ask(struct game_s *g, player_id_t pi)
{
    const uint8_t *data_in = GAME_MSGI_DPTR(g, pi);
    uint8_t pli, exploreinfo = 0, flag_do_colonize;
    pli = game_turn_explore_decode(g, pi, &data_in, &exploreinfo);
    game_update_visibility(g);
    flag_do_colonize = ui_explore(g, pi, pli, false, true) ? 1 : 0;
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_COLONIZEA);
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_U8(g, pi, flag_do_colonize);
    GAME_MSGO_EN_TBL_U8(g, pi, g->planet[pli].name, PLANET_NAME_LEN);
    GAME_MSGO_EN_LEN(g, pi);
    if (flag_do_colonize) {
        game_planet_colonize(g, pli, pi, exploreinfo & 7);
    }
    return 1;
}
