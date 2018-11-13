#include "config.h"

#include "game_turn.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_client.h"
#include "game_end.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_save.h"
#include "log.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_TURN 3

/* -------------------------------------------------------------------------- */

int game_turn_client_make_input_msg(struct game_s *g)
{
    player_id_t pi = g->active_player;
    uint8_t *buf = GAME_MSGO_DPTR(g, pi);
    int pos = 0;
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_TURN_INPUT);
    SG_1OOM_EN_U16(g->year);
    SG_1OOM_EN_U8(pi);
    SG_1OOM_EN_U8(g->planet_focus_i[pi]);
    {
        const empiretechorbit_t *e = &g->eto[pi];
        SG_1OOM_EN_U8(e->tax);
        SG_1OOM_EN_U8(e->security);
        SG_1OOM_EN_TBL_U8(e->spying, g->players);
        SG_1OOM_EN_TBL_U8(e->spymode, g->players);
        for (int i = 0; i < TECH_FIELD_NUM; ++i) {
            uint8_t v;
            v = e->tech.slider[i];
            if (e->tech.slider_lock[i]) {
                v |= 0x80;
            }
            SG_1OOM_EN_U8(v);
        }
    }
    {
        const gameevents_t *ev = &(g->evn);
        SG_1OOM_EN_BV(ev->help_shown[pi], HELP_SHOWN_NUM);
        SG_1OOM_EN_BV(ev->msg_filter[pi], FINISHED_NUM);
        {
            uint8_t v;
            v = ev->gov_eco_mode[pi];
            if (BOOLVEC_IS1(ev->gov_no_stargates, pi)) {
                v |= 0x80;
            }
            SG_1OOM_EN_U8(v);
        }
    }
    pos = game_save_encode_sd(buf, pos, &(g->current_design[pi]));
    for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
        const planet_t *p = &(g->planet[pli]);
        if (p->owner == pi) {
            SG_1OOM_EN_U8(pli);
            for (int i = 0; i < PLANET_SLIDER_NUM; ++i) {
                uint8_t v;
                v = p->slider[i];
                if (p->slider_lock[i]) {
                    v |= 0x80;
                }
                SG_1OOM_EN_U8(v);
            }
            SG_1OOM_EN_U8(p->buildship);
            SG_1OOM_EN_U8(p->reloc);
            SG_1OOM_EN_U16(p->trans_num);
            SG_1OOM_EN_U8(p->trans_dest);
            SG_1OOM_EN_BV(p->extras, PLANET_EXTRAS_NUM);
            SG_1OOM_EN_U16(p->target_bases);
        }
    }
    SG_1OOM_EN_U8(0xff);
    GAME_MSGO_ADD_LEN(g, pi, pos);
    GAME_MSGO_EN_LEN(g, pi);
    return 0;
}

struct game_end_s game_turn_client_next(struct game_s *g)
{
    struct game_end_s game_end;
    game_end.type = GAME_END_NONE;
    game_turn_client_make_input_msg(g);
    game_client_msg_send(g, g->active_player);
    if ((g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) && (g->gaux->local_players > 1)) {
        for (player_id_t pi = g->active_player + 1; pi < g->players; ++pi) {
            if (IS_HUMAN(g, pi) && IS_ALIVE(g, pi)) {
                return game_end;
            }
        }
    }
    /* TODO if mp == client
    bool done = false;
    ui_next_turn_start(g);
    while (!done) {
        int msgtype;
        msgtype = game_msg_recv(g);
        if (msgtype > 0) {
            switch (msgtype) {
                ...
            }
            game_msg_release(g);
        } else if (msgtype < 0) {
            error
        } else {
            ui_next_turn_wait(g);
        }
    }
    ui_next_turn_end(g);
    */
    game_end.type = g->end;
    /* TODO fill out rest of game_end */
    return game_end;
}

int game_turn_client_end(struct game_s *g, player_id_t pi)
{
    game_end_type_t end;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0;
        SG_1OOM_DE_U8(g->winner);
        SG_1OOM_DE_U8(end);
    }
    LOG_DEBUG((DEBUGLEVEL_TURN, "%s: w:%u e:%u ge:%u", __func__, g->winner, end, g->end));
    if ((g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) && (g->gaux->local_players > 1)) {
        switch (end) {
            case GAME_END_LOST_EXILE:
                if (IS_HUMAN(g, g->winner)) {
                    end = g->end;
                }
                break;
            case GAME_END_LOST_FUNERAL:
                for (player_id_t i = 0; i < g->players; ++i) {
                    if (IS_HUMAN(g, i) && IS_ALIVE(g, i)) {
                        end = g->end;
                    }
                }
                break;
            default:
                break;
        }
    }
    g->end = end;
    LOG_DEBUG((DEBUGLEVEL_TURN, "->%u\n", end));
    return 0;
}
