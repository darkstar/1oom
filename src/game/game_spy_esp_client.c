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

#define DEBUGLEVEL_ESP  4

/* -------------------------------------------------------------------------- */

int game_spy_steal_ask(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    uint8_t target, flags_field;
    int pos = 0, field;
    SG_1OOM_DE_U8(target);
    SG_1OOM_DE_U8(flags_field);
    LOG_DEBUG((DEBUGLEVEL_ESP, "%s: pi:%i target:%i fields:%x\n", __func__, pi, target, flags_field));
    field = ui_spy_steal(g, pi, target, flags_field);
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_STEALA);
    GAME_MSGO_EN_U8(g, pi, target);
    GAME_MSGO_EN_U8(g, pi, field);
    GAME_MSGO_EN_LEN(g, pi);
    return 1;
}

int game_spy_stolen_show(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0;
    while (1) {
        uint8_t spy, field, tech;
        SG_1OOM_DE_U8(spy);
        if (spy == 0xff) {
            break;
        }
        SG_1OOM_DE_U8(field);
        SG_1OOM_DE_U8(tech);
        LOG_DEBUG((DEBUGLEVEL_ESP, "%s: pi:%i spy:%u field:%u tech:%u\n", __func__, pi, spy, field, tech));
        ui_spy_stolen(g, pi, spy, field, tech);
    }
    return 0;
}
