#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_ground.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_str.h"
#include "game_tech.h"
#include "log.h"
#include "types.h"
#include "ui.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static void game_ground_show_init(struct game_s *g, struct ground_s *gr)
{
    for (int i = 0; i < 2; ++i) {
        char strbuf[0x40];
        uint8_t besti;
        gr->s[i].human = IS_HUMAN(g, gr->s[i].player);
        gr->s[i].pop2 = gr->s[i].pop1;
        gr->s[i].strnum = 1;
        strcpy(strbuf, *tbl_shiptech_armor[gr->s[i].armori * 2].nameptr);
        util_str_tolower(&strbuf[1]);
        sprintf(gr->s[i].str[0], "%s ", strbuf);
        besti = gr->s[i].suiti;
        if (besti == 0) {
            strcat(gr->s[i].str[0], game_str_gr_carmor);
        } else {
            game_tech_get_name(g->gaux, TECH_FIELD_CONSTRUCTION, besti, strbuf);
            strcat(gr->s[i].str[0], strbuf);
        }
        besti = gr->s[i].shieldi;
        if (besti != 0) {
            game_tech_get_name(g->gaux, TECH_FIELD_FORCE_FIELD, besti, gr->s[i].str[1]);
            gr->s[i].strnum = 2;
        }
        besti = gr->s[i].weapi;
        if (besti != 0) {
            game_tech_get_name(g->gaux, TECH_FIELD_WEAPON, besti, gr->s[i].str[gr->s[i].strnum++]);
        }
    }
}

static int game_ground_decode(struct ground_s *gr, const uint8_t *buf, int pos)
{
    {
        uint8_t v;
        SG_1OOM_DE_U8(v);
        if (v == 0xff) {
            return 0;
        }
        gr->planet_i = v;
    }
    SG_1OOM_DE_U8(gr->s[0].player);
    SG_1OOM_DE_U8(gr->s[1].player);
    {
        uint8_t v;
        SG_1OOM_DE_U8(v);
        gr->flag_swap = (v & 1) != 0;
        gr->flag_rebel = (v & 2) != 0;
    }
    SG_1OOM_DE_U32(gr->seed);
    SG_1OOM_DE_U16(gr->inbound);
    SG_1OOM_DE_U16(gr->total_inbound);
    SG_1OOM_DE_U16(gr->fact);
    for (int i = 0; i < 2; ++i) {
        SG_1OOM_DE_U16(gr->s[i].force);
        SG_1OOM_DE_U16(gr->s[i].pop1);
        SG_1OOM_DE_U8(gr->s[i].armori);
        SG_1OOM_DE_U8(gr->s[i].suiti);
        SG_1OOM_DE_U8(gr->s[i].shieldi);
        SG_1OOM_DE_U8(gr->s[i].weapi);
    }
    SG_1OOM_DE_DUMMY(4);  /* 2 x pop1 at end; we ignore it and just run the battle */
    {
        int num = 0;
        for (int i = 0; i < TECH_SPY_MAX; ++i) {
            uint8_t t;
            SG_1OOM_DE_U8(t);
            gr->got[i].tech = t;
            if (t != 0) {
                ++num;
            }
            SG_1OOM_DE_U8(gr->got[i].field);
        }
        gr->techchance = num;
    }
    return pos;
}

/* -------------------------------------------------------------------------- */

int game_turn_ground_show(struct game_s *g, player_id_t pi)
{
    struct ground_s gr[1];
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0;
    while ((pos = game_ground_decode(gr, buf, pos)) > 0) {
        game_ground_show_init(g, gr);
        if (0 /* do not show player-player battles twice on local multiplayer */
          || (g->gaux->multiplayer != GAME_MULTIPLAYER_LOCAL)
          || (!gr->s[0].human) || (!gr->s[1].human)
          || (pi == MIN(gr->s[0].player, gr->s[1].player))
        ) {
            ui_ground(g, gr);
        }
    }
    return 0;
}
