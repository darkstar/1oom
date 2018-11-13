#include "config.h"

#include "game_move.h"
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

#define DEBUGLEVEL_MOVE 5

/* -------------------------------------------------------------------------- */

int game_turn_move_show(struct game_s *g, player_id_t pi)
{
    void *ctx;
    bool flag_more;
    const uint8_t *p, *q;
    ctx = ui_gmap_basic_init(g, g->gaux->local_players > 1);
    game_update_visibility(g);
    flag_more = true;
    ui_gmap_basic_start_player(ctx, pi);
    p = GAME_MSGI_PTR(g, pi)->buf;
    q = p;
    p += 4;
    while (flag_more) {
        uint16_t v, i;
        v = GET_LE_16(p);
        p += 2;
        i = v & GAME_MOVE_INDEX_MASK;
        switch (v >> 12) {
            case GAME_MOVE_FRAME_START:
                LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x frame %i\n", __func__, v >> 12, i));
                ui_gmap_basic_start_frame(ctx, pi);
                break;
            case GAME_MOVE_FRAME_END:
                LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x more %i\n", __func__, v >> 12, i));
                ui_gmap_basic_draw_frame(ctx, pi);
                ui_gmap_basic_finish_frame(ctx, pi);
                flag_more = (i != 0);
                break;
            case GAME_MOVE_HIDE_FLEET:
                LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x hide fleet %i\n", __func__, v >> 12, i));
                {
                    fleet_enroute_t *r;
                    r = &(g->enroute[i]);
                    BOOLVEC_SET0(r->visible, pi);
                }
                break;
            case GAME_MOVE_HIDE_TRANS:
                LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x hide trans %i\n", __func__, v >> 12, i));
                {
                    transport_t *r;
                    r = &(g->transport[i]);
                    BOOLVEC_SET0(r->visible, pi);
                }
                break;
            case GAME_MOVE_SHOW_FLEET:
                {
                    fleet_enroute_t *r;
                    uint16_t x, y;
                    uint8_t dest;
                    x = GET_LE_16(p);
                    p += 2;
                    y = GET_LE_16(p);
                    p += 2;
                    dest = *p++;
                    LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x show fleet %i x:%i y:%i d:%i\n", __func__, v >> 12, i, x, y, dest));
                    r = &(g->enroute[i]);
                    r->x = x;
                    r->y = y;
                    r->dest = dest;
                    BOOLVEC_SET1(r->visible, pi);
                }
                break;
            case GAME_MOVE_SHOW_TRANS:
                {
                    uint16_t x, y;
                    uint8_t dest;
                    x = GET_LE_16(p);
                    p += 2;
                    y = GET_LE_16(p);
                    p += 2;
                    dest = *p++;
                    LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x show trans %i x:%i y:%i d:%i\n", __func__, v >> 12, i, x, y, dest));
                    if (i < TRANSPORT_MAX) {
                        transport_t *r;
                        r = &(g->transport[i]);
                        r->x = x;
                        r->y = y;
                        r->dest = dest;
                        BOOLVEC_SET1(r->visible, pi);
                    } else if (i == GAME_MOVE_INDEX_AMOEBA) {
                        monster_t *m = &(g->evn.amoeba);
                        m->x = x;
                        m->y = y;
                    } else if (i == GAME_MOVE_INDEX_CRYSTAL) {
                        monster_t *m = &(g->evn.crystal);
                        m->x = x;
                        m->y = y;
                    }
                }
                break;
            case GAME_MOVE_MOVE_FLEET:
                {
                    int8_t dx, dy;
                    dx = *p++;
                    dy = *p++;
                    LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x move fleet %i dx:%i dy:%i\n", __func__, v >> 12, i, dx, dy));
                    if (i < FLEET_ENROUTE_MAX) {
                        fleet_enroute_t *r;
                        r = &(g->enroute[i]);
                        r->x += dx;
                        r->y += dy;
                    } else {
                        log_fatal_and_die("%s: invalid fleet index %i\n", __func__, i);
                    }
                }
                break;
            case GAME_MOVE_MOVE_TRANS:
                {
                    int8_t dx, dy;
                    dx = *p++;
                    dy = *p++;
                    LOG_DEBUG((DEBUGLEVEL_MOVE, "%s: act 0x%x move trans %i dx:%i dy:%i\n", __func__, v >> 12, i, dx, dy));
                    if (i < TRANSPORT_MAX) {
                        transport_t *r;
                        r = &(g->transport[i]);
                        r->x += dx;
                        r->y += dy;
                    } else if (i == GAME_MOVE_INDEX_AMOEBA) {
                        monster_t *m = &(g->evn.amoeba);
                        m->x += dx;
                        m->y += dy;
                    } else if (i == GAME_MOVE_INDEX_CRYSTAL) {
                        monster_t *m = &(g->evn.crystal);
                        m->x += dx;
                        m->y += dy;
                    } else {
                        log_fatal_and_die("%s: invalid trans index %i\n", __func__, i);
                    }
                }
                break;
            default:
                log_fatal_and_die("%s: invalid act 0x%x i %i, pos %i / %i\n", __func__, v >> 12, i, p - q, GAME_MSGI_PTR(g, pi)->len);
                break;
        }
    }
    ui_gmap_basic_shutdown(ctx);
    return 0;
}
