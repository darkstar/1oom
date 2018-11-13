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

#define DEBUGLEVEL_BOMB 4

/* -------------------------------------------------------------------------- */

static uint8_t game_turn_bombard_decode(struct game_s *g, player_id_t pi, const uint8_t **data_ptr, player_id_t *owner_ptr)
{
    const uint8_t *buf = *data_ptr;
    int pos = 0;
    uint8_t pli;
    SG_1OOM_DE_U8(pli);
    if (pli != 0xff) {
        planet_t *p = &(g->planet[pli]);
        SG_1OOM_DE_U8(p->turn.bomb.bomber);
        *owner_ptr = p->owner;
        SG_1OOM_DE_U8(p->owner);
        SG_1OOM_DE_U16(p->turn.bomb.popdmg);
        SG_1OOM_DE_U16(p->turn.bomb.factdmg);
    }
    *data_ptr = buf + pos;
    return pli;
}

/* -------------------------------------------------------------------------- */

int game_turn_bomb_show(struct game_s *g, player_id_t pi)
{
    const uint8_t *data;
    data = GAME_MSGI_DPTR(g, pi);
    while (1) {
        uint8_t pli;
        player_id_t owner;
        pli = game_turn_bombard_decode(g, pi, &data, &owner);
        LOG_DEBUG((DEBUGLEVEL_BOMB, "%s: pi:%i pli:%i\n", __func__, pi, pli));
        if (pli == PLANET_NONE) {
            break;
        } else {
            const planet_t *p;
            bool flag_play_music;
            p = &(g->planet[pli]);
            flag_play_music = (pi == owner);
            ui_bomb_show(g, pi, p->turn.bomb.bomber, owner, pli, p->turn.bomb.popdmg, p->turn.bomb.factdmg, flag_play_music);
        }
    }
    return 0;
}

int game_turn_bomb_ask(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    planet_t *p;
    uint8_t pli, flag_bomb;
    uint16_t inbound;
    int pos = 0;
    SG_1OOM_DE_U8(pli);
    p = &(g->planet[pli]);
    SG_1OOM_DE_U16(p->pop);
    SG_1OOM_DE_U16(p->factories);
    SG_1OOM_DE_U16(inbound);
    game_update_visibility(g);
    LOG_DEBUG((DEBUGLEVEL_BOMB, "%s: pi:%i pli:%i\n", __func__, pi, pli));
    flag_bomb = ui_bomb_ask(g, pi, pli, inbound) ? 1 : 0;
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_BOMBA);
    GAME_MSGO_EN_U8(g, pi, pli);
    GAME_MSGO_EN_U8(g, pi, flag_bomb);
    GAME_MSGO_EN_LEN(g, pi);
    return 1;
}
