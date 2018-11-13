#include "config.h"

#include "game_spy.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "log.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_SAB  4

/* -------------------------------------------------------------------------- */

int game_spy_sabotage_ask(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    uint8_t target, act, planet = PLANET_NONE;
    int pos = 0;
    SG_1OOM_DE_U8(target);
    LOG_DEBUG((DEBUGLEVEL_SAB, "%s: pi:%i target:%i\n", __func__, pi, target));
    act = ui_spy_sabotage_ask(g, pi, target, &planet);
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_SABA);
    GAME_MSGO_EN_U8(g, pi, target);
    GAME_MSGO_EN_U8(g, pi, act);
    GAME_MSGO_EN_U8(g, pi, planet);
    GAME_MSGO_EN_LEN(g, pi);
    return 1;
}

int game_spy_sabotage_show(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0;
    while (1) {
        planet_t *p;
        uint8_t spy, target, act, other1, other2, other, pli;
        uint16_t sval;
        SG_1OOM_DE_U8(spy);
        if (spy == 0xff) {
            break;
        }
        SG_1OOM_DE_U8(target);
        SG_1OOM_DE_U8(act);
        SG_1OOM_DE_U8(other1);
        SG_1OOM_DE_U8(other2);
        SG_1OOM_DE_U8(pli);
        p = &(g->planet[pli]);
        SG_1OOM_DE_U16(p->rebels);
        SG_1OOM_DE_U16(sval);
        LOG_DEBUG((DEBUGLEVEL_SAB, "%s: pi:%i spy:%u target:%u act:%u o:%u,%u p:%i'%s' sval:%u\n", __func__, pi, spy, target, act, other1, other2, pli, p->name, sval));
        other = ui_spy_sabotage_done(g, pi, spy, target, act, other1, other2, pli, sval);
        if ((spy == pi) && (other2 != PLAYER_NONE) && (other != PLAYER_NONE)) {
            GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_FRAMESAB);
            GAME_MSGO_EN_U8(g, pi, target);
            GAME_MSGO_EN_U8(g, pi, other);
            GAME_MSGO_EN_LEN(g, pi);
            return 1;
        }
    }
    return 0;
}
