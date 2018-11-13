#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_turn.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_design.h"
#include "game_end.h"
#include "game_endecode.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_save.h"
#include "game_server.h"
#include "game_view.h"
#include "log.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_INPUT    4

/* -------------------------------------------------------------------------- */

int game_turn_server_turn_input_msg(struct game_s *g, player_id_t pi)
{
    struct game_s *gv = g->gaux->gview[pi];
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    uint16_t v16;
    uint8_t v;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_TURN_INPUT)) {
        log_fatal_and_die("%s: message id 0x%x len %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    SG_1OOM_DE_U16(v16);
    if (v16 != g->year) {
        log_error("%s: pl %i wrong year %i != %i\n", __func__, pi, v16, g->year);
        return 2;
    }
    SG_1OOM_DE_U8(v);
    if (v != pi) {
        log_error("%s: pl %i wrong pi %i != %i\n", __func__, pi, v, pi);
        return 2;
    }
    SG_1OOM_DE_U8(v);
    if (v >= g->galaxy_stars) {
        log_error("%s: pl %i wrong focus %i >= %i\n", __func__, pi, v, g->galaxy_stars);
        return 2;
    }
    gv->planet_focus_i[pi] = v;
    {
        empiretechorbit_t *e = &gv->eto[pi];
        SG_1OOM_DE_U8(v);
        if (v > 200) {
            log_error("%s: pl %i wrong tax %i > 200\n", __func__, pi, v);
            return 2;
        }
        e->tax = v;
        SG_1OOM_DE_U8(v);
        if (v > 200) {
            log_error("%s: pl %i wrong security %i > 200\n", __func__, pi, v);
            return 2;
        }
        e->security = v;
        for (int i = 0; i < g->players; ++i) {
            SG_1OOM_DE_U8(v);
            if (v > 100) {
                log_error("%s: pl %i wrong spying %i %i > 100\n", __func__, pi, i, v);
                return 2;
            }
            e->spying[i] = v;
        }
        for (int i = 0; i < g->players; ++i) {
            SG_1OOM_DE_U8(v);
            if (v > SPYMODE_SABOTAGE) {
                log_error("%s: pl %i wrong spymode %i %i > %i\n", __func__, pi, i, v, SPYMODE_SABOTAGE);
                return 2;
            }
            e->spymode[i] = v;
        }
    }
    {
        techdata_t *t = &(gv->eto[pi].tech);
        uint8_t sl[TECH_FIELD_NUM];
        v16 = 0;
        for (int i = 0; i < TECH_FIELD_NUM; ++i) {
            SG_1OOM_DE_U8(v);
            sl[i] = v;
            v16 += v & 0x7f;
        }
        if (v16 != 100) {
            log_error("%s: pl %i wrong tech slider sum %i != 100\n", __func__, pi, v16);
            return 2;
        }
        for (int i = 0; i < TECH_FIELD_NUM; ++i) {
            v = sl[i];
            if (v & 0x80) {
                t->slider_lock[i] = 1;
                v &= 0x7f;
            } else {
                t->slider_lock[i] = 0;
            }
            t->slider[i] = v;
        }
    }
    {
        gameevents_t *ev = &(gv->evn);
        SG_1OOM_DE_BV(ev->help_shown[pi], HELP_SHOWN_NUM);
        SG_1OOM_DE_BV(ev->msg_filter[pi], FINISHED_NUM);
        BOOLVEC_SET1(ev->msg_filter[pi], FINISHED_SHIP);
        SG_1OOM_DE_U8(v);
        BOOLVEC_SET(ev->gov_no_stargates, pi, (v & 0x80) != 0);
        v &= 0x7f;
        if (v >= GOVERNOR_ECO_MODE_NUM) {
            log_error("%s: pl %i wrong eco mode %i >= %i\n", __func__, pi, v, GOVERNOR_ECO_MODE_NUM);
            return 2;
        }
        ev->gov_eco_mode[pi] = v;
    }
    pos = game_save_decode_sd(buf, pos, &(gv->current_design[pi]));
    {
        int res;
        if ((res = game_design_check(g, pi, &(gv->current_design[pi]), false)) != DESIGN_ERR_NONE) {
            log_error("%s: pl %i design error %i\n", __func__, pi, res);
            return 2 + res;
        }
    }
    while (pos < len) {
        planet_t *p;
        uint8_t pli;
        SG_1OOM_DE_U8(pli);
        if (pli == 0xff) {
            break;
        }
        if (pli >= g->galaxy_stars) {
            log_error("%s: pl %i wrong planet %i >= %i\n", __func__, pi, pli, g->galaxy_stars);
            return 2;
        }
        p = &(gv->planet[pli]);
        if (p->owner != pi) {
            log_error("%s: pl %i wrong planet %i owner %i\n", __func__, pi, pli, p->owner);
            return 2;
        }
        {
            uint8_t sl[PLANET_SLIDER_NUM];
            bool have_changed = false;
            v16 = 0;
            for (int i = 0; i < PLANET_SLIDER_NUM; ++i) {
                SG_1OOM_DE_U8(v);
                sl[i] = v;
                v &= 0x7f;
                v16 += v;
                if (v != p->slider[i]) {
                    have_changed = true;
                }
            }
            /* Allow sums of over 100 if the sliders were not moved.
               The original slider autoadjust in game_update_eco_on_waste can lead to a sum over 100.
               Assume the previous values are correct.
            */
            if (have_changed && (v16 != 100)) {
                log_error("%s: pl %i wrong planet %i slider sum %i != 100\n", __func__, pi, pli, v16);
                return 2;
            }
            for (int i = 0; i < PLANET_SLIDER_NUM; ++i) {
                v = sl[i];
                if (v & 0x80) {
                    p->slider_lock[i] = 1;
                    v &= 0x7f;
                } else {
                    p->slider_lock[i] = 0;
                }
                p->slider[i] = v;
            }
        }
        SG_1OOM_DE_U8(v);
        if ((v > 6) || ((v == 6) && !gv->eto[pi].have_stargates) || ((v < 6) && (v >= gv->eto[pi].shipdesigns_num))) {
            log_error("%s: pl %i wrong planet %i buildship %i\n", __func__, pi, pli, v);
            return 2;
        }
        p->buildship = v;
        SG_1OOM_DE_U8(v);
        if ((v >= g->galaxy_stars) || (g->planet[v].owner != pi)) {
            log_error("%s: pl %i wrong planet %i reloc %i\n", __func__, pi, pli, v);
            return 2;
        }
        p->reloc = v;
        SG_1OOM_DE_U16(v16);
        if (v16 > 0) {
            if (v16 > (p->pop / 2)) { /* TODO plague, rebellion... */
                log_error("%s: pl %i wrong planet %i trans num %i\n", __func__, pi, pli, v16);
                return 2;
            }
        }
        p->trans_num = v16;
        SG_1OOM_DE_U8(v);
        if ((p->trans_num != 0) && ((v >= g->galaxy_stars) || (!game_transport_dest_ok(g, &(g->planet[v]), pi)))) {
            log_error("%s: pl %i wrong planet %i trans dest %i\n", __func__, pi, pli, v);
            return 2;
        }
        p->trans_dest = v;
        SG_1OOM_DE_BV(p->extras, PLANET_EXTRAS_NUM);
        SG_1OOM_DE_U16(p->target_bases);
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_INPUT, "%s: pi %i msg ok\n", __func__, pi));
    return 1;
}

int game_turn_server_send_state(struct game_s *g)
{
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi) && IS_ALIVE(g, pi)) {
            int len;
            game_view_make(g, g->gaux->gview[pi], pi);
            GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_GAME_DATA);
            len = game_save_encode(GAME_MSGO_DPTR(g, pi), GAME_AUX_MSGBUF_SIZE - 4, g->gaux->gview[pi], 0);
            if (len < 0) {
                log_fatal_and_die("BUG: %s: buffet too small\n");
                return -1;
            }
            GAME_MSGO_ADD_LEN(g, pi, len);
            GAME_MSGO_EN_LEN(g, pi);
        }
    }
    game_server_msgo_flush(g);
    return 0;
}

int game_turn_server_send_end(struct game_s *g, struct game_end_s *ge, const uint8_t *old_home)
{
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi) && (IS_ALIVE(g, pi) || (old_home[pi] != PLANET_NONE))) {
            uint8_t winner, end;
            winner = g->winner;
            end = ge->type;
            switch (end) {
                case GAME_END_WON_GOOD:
                    if (pi != g->winner) {
                        end = GAME_END_LOST_EXILE;
                    }
                    break;
                case GAME_END_WON_TYRANT:
                    if (pi != g->winner) {
                        end = GAME_END_LOST_FUNERAL;
                    }
                    break;
                default:
                    if (!IS_ALIVE(g, pi)) {
                        end = GAME_END_LOST_FUNERAL;
                        if (winner == PLAYER_NONE) {
                            winner = g->gaux->killer[pi];
                        }
                    }
                    break;
            }
            GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_END);
            GAME_MSGO_EN_U8(g, pi, winner);
            GAME_MSGO_EN_U8(g, pi, end);
            GAME_MSGO_EN_LEN(g, pi);
        }
    }
    game_server_msgo_flush(g);
    return 0;
}
