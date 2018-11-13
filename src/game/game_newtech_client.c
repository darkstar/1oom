#include "config.h"

#include "game_newtech.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_client.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_tech.h"
#include "log.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_NEWTECH  4

/* -------------------------------------------------------------------------- */

static bool game_turn_newtech_decode(struct game_s *g, player_id_t pi)
{
    newtechs_t *nts = &(g->evn.newtech[pi]);
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0;
    SG_1OOM_DE_U8(nts->num);
    for (int i = 0; i < nts->num; ++i) {
        newtech_t *nt = &(nts->d[i]);
        SG_1OOM_DE_U8(nt->field);
        SG_1OOM_DE_U8(nt->tech);
        SG_1OOM_DE_U8(nt->source);
        SG_1OOM_DE_U8(nt->v06);
        SG_1OOM_DE_U8(nt->stolen_from);
        {
            uint8_t v;
            SG_1OOM_DE_U8(v);
            nt->frame = (v != 0);
        }
        SG_1OOM_DE_U8(nt->other1);
        SG_1OOM_DE_U8(nt->other2);
    }
    for (tech_field_t field = 0; field < TECH_FIELD_NUM; ++field) {
        nexttech_t *xt = &(nts->next[field]);
        SG_1OOM_DE_U8(xt->num);
        SG_1OOM_DE_TBL_U8(xt->tech, TECH_NEXT_MAX);
    }
    GAME_MSGI_HANDLED(g, pi);
    return true;
}

/* -------------------------------------------------------------------------- */

int game_turn_newtech_show(struct game_s *g, player_id_t pi)
{
    game_turn_newtech_decode(g, pi);
    ui_newtech(g, pi);
    g->evn.newtech[pi].num = 0;
    return 0;
}

int game_turn_newtech_choose_next(struct game_s *g, player_id_t pi, tech_field_t field, uint8_t tech)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_NEXTTECH);
    GAME_MSGO_EN_U8(g, pi, field);
    GAME_MSGO_EN_U8(g, pi, tech);
    GAME_MSGO_EN_LEN(g, pi);
    game_tech_start_next(g, pi, field, tech);
    return game_client_msg_send(g, pi);
}

int game_turn_newtech_frame(struct game_s *g, player_id_t pi, uint8_t newtech_i, player_id_t victim)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_FRAMEESP);
    GAME_MSGO_EN_U8(g, pi, newtech_i);
    GAME_MSGO_EN_U8(g, pi, victim);
    GAME_MSGO_EN_LEN(g, pi);
    return game_client_msg_send(g, pi);
}
