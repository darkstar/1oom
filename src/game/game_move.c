#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_move.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_planet.h"
#include "game_server.h"
#include "log.h"
#include "types.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define GAME_MOVE_EN_T(_g_, _pi_, _t_, _v_) GAME_MSGO_EN_U16(_g_, _pi_, ((_t_) << 12) | (_v_))

/* -------------------------------------------------------------------------- */

static inline void game_move_en_vis_fleet(struct game_s *g, int i, uint8_t oldv, uint8_t newv, uint8_t msg_mask)
{
    const fleet_enroute_t *r = &(g->enroute[i]);
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if ((1 << pi) & msg_mask & (oldv ^ newv)) {
            if ((1 << pi) & newv) {
                uint8_t dest;
                GAME_MOVE_EN_T(g, pi, GAME_MOVE_SHOW_FLEET, i);
                GAME_MSGO_EN_U16(g, pi, r->x);
                GAME_MSGO_EN_U16(g, pi, r->y);
                if ((r->owner == pi) || g->eto[pi].have_ia_scanner) {
                    dest = r->dest;
                } else {
                    dest = (g->planet[r->dest].x < r->x) ? PLANET_LEFT : PLANET_NONE;
                }
                GAME_MSGO_EN_U8(g, pi, dest);
            } else {
                GAME_MOVE_EN_T(g, pi, GAME_MOVE_HIDE_FLEET, i);
            }
        }
    }
}

static inline void game_move_en_vis_trans(struct game_s *g, int i, uint8_t oldv, uint8_t newv, uint8_t msg_mask)
{
    const transport_t *r = &(g->transport[i]);
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if ((1 << pi) & msg_mask & (oldv ^ newv)) {
            if ((1 << pi) & newv) {
                uint8_t dest;
                GAME_MOVE_EN_T(g, pi, GAME_MOVE_SHOW_TRANS, i);
                GAME_MSGO_EN_U16(g, pi, r->x);
                GAME_MSGO_EN_U16(g, pi, r->y);
                if ((r->owner == pi) || g->eto[pi].have_ia_scanner) {
                    dest = r->dest;
                } else {
                    dest = (g->planet[r->dest].x < r->x) ? PLANET_LEFT : PLANET_NONE;
                }
                GAME_MSGO_EN_U8(g, pi, dest);
            } else {
                GAME_MOVE_EN_T(g, pi, GAME_MOVE_HIDE_TRANS, i);
            }
        }
    }
}

static inline void game_move_en_dxdy(struct game_s *g, bool is_fleet, int i, int dx, int dy, uint8_t vis, uint8_t msg_mask)
{
    if ((dx < -128) || (dx > 127) || (dy < -128) || (dy > 127)) {
        if (is_fleet) {
            fleet_enroute_t *r = &(g->enroute[i]);
            r->x += dx;
            r->y += dy;
            game_move_en_vis_fleet(g, i, 0, r->visible[0], msg_mask);
        } else if (i < TRANSPORT_MAX) {
            transport_t *r = &(g->transport[i]);
            r->x += dx;
            r->y += dy;
            game_move_en_vis_trans(g, i, 0, r->visible[0], msg_mask);
        } else {
            log_fatal_and_die("BUG: %s: transport %i\n", __func__, i);
        }
    } else {
        uint16_t id = ((is_fleet ? GAME_MOVE_MOVE_FLEET : GAME_MOVE_MOVE_TRANS) << 12) | i;
        for (player_id_t pi = 0; pi < g->players; ++pi) {
            if ((1 << pi) & msg_mask & vis) {
                GAME_MSGO_EN_U16(g, pi, id);
                GAME_MSGO_EN_U8(g, pi, dx);
                GAME_MSGO_EN_U8(g, pi, dy);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

int game_turn_move_ships(struct game_s *g)
{
    bool flag_more = true;
    int rng_steps = 0, rng_countdown = -1;
    BOOLVEC_TBL_DECLARE(vis_fleet, FLEET_ENROUTE_MAX, PLAYER_NUM);
    BOOLVEC_TBL_DECLARE(vis_trans, TRANSPORT_MAX, PLAYER_NUM);
    BOOLVEC_DECLARE(build_msg, PLAYER_NUM);
    game_update_visibility(g);
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        BOOLVEC_TBL_COPY1(vis_fleet, r->visible, i, PLAYER_NUM);
    }
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        BOOLVEC_TBL_COPY1(vis_trans, r->visible, i, PLAYER_NUM);
    }
    BOOLVEC_CLEAR(build_msg, PLAYER_NUM);
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi) && IS_ALIVE(g, pi)) {
            BOOLVEC_SET1(build_msg, pi);
            GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_TURN_MOVE);
        }
    }
    for (int frame = 0; (frame < MOVE_FRAMES_MAX) && flag_more; ++frame) {
        bool odd_frame;
        for (player_id_t pi = 0; pi < g->players; ++pi) {
            if (BOOLVEC_IS1(build_msg, pi)) {
                GAME_MOVE_EN_T(g, pi, GAME_MOVE_FRAME_START, frame);
            }
        }
        game_update_visibility(g);
        for (int i = 0; i < g->enroute_num; ++i) {
            const fleet_enroute_t *r = &(g->enroute[i]);
            uint8_t oldv, newv;
            oldv = vis_fleet[i][0];
            newv = r->visible[0];
            vis_fleet[i][0] = newv;
            if ((oldv ^ newv) & build_msg[0]) {
                game_move_en_vis_fleet(g, i, oldv, newv, build_msg[0]);
            }
        }
        for (int i = 0; i < g->transport_num; ++i) {
            const transport_t *r = &(g->transport[i]);
            uint8_t oldv, newv;
            oldv = vis_trans[i][0];
            newv = r->visible[0];
            vis_trans[i][0] = newv;
            if ((oldv ^ newv) & build_msg[0]) {
                game_move_en_vis_trans(g, i, oldv, newv, build_msg[0]);
            }
        }
        odd_frame = frame & 1;
        flag_more = false;
        for (int i = 0; i < g->enroute_num; ++i) {
            fleet_enroute_t *r = &(g->enroute[i]);
            if ((r->speed * 2) > frame) {
                bool in_nebula;
                int x, y;
                x = r->x;
                y = r->y;
                in_nebula = (odd_frame || (frame == 0)) ? false : game_xy_is_in_nebula(g, x, y);
                if (odd_frame || (frame == 0) || (!in_nebula)) {
                    int x1, y1;
                    const planet_t *p;
                    p = &(g->planet[r->dest]);
                    x1 = p->x;
                    y1 = p->y;
                    if (r->speed == FLEET_SPEED_STARGATE) {
                        x = x1;
                        y = y1;
                    } else {
                        flag_more = true;
                        util_math_go_line_dist(&x, &y, x1, y1, odd_frame ? 6 : 5);
                    }
                    if ((r->x != x) || (r->y != y)) {
                        game_move_en_dxdy(g, true, i, x - r->x, y - r->y, r->visible[0], build_msg[0]);
                    }
                    r->x = x;
                    r->y = y;
                } else if (in_nebula) {
                    flag_more = true;   /* WASBUG MOO1 stopped nebula movement early if no transports or faster ships enroute */
                }
            }
        }
        for (int i = 0; i < g->transport_num; ++i) {
            transport_t *r = &(g->transport[i]);
            if ((r->speed * 2) > frame) {
                bool in_nebula;
                int x, y;
                x = r->x;
                y = r->y;
                in_nebula = (!odd_frame) ? false : game_xy_is_in_nebula(g, x, y);
                if ((!odd_frame) || (!in_nebula)) {
                    int x1, y1;
                    const planet_t *p;
                    p = &(g->planet[r->dest]);
                    x1 = p->x;
                    y1 = p->y;
                    if (r->speed == FLEET_SPEED_STARGATE) {
                        x = x1;
                        y = y1;
                    } else {
                        flag_more = true;
                        util_math_go_line_dist(&x, &y, x1, y1, odd_frame ? 6 : 5);
                    }
                    if ((r->x != x) || (r->y != y)) {
                        game_move_en_dxdy(g, false, i, x - r->x, y - r->y, r->visible[0], build_msg[0]);
                    }
                    r->x = x;
                    r->y = y;
                } else if (in_nebula) {
                    flag_more = true;   /* WASBUG MOO1 stopped nebula movement early if no fleets or faster ships enroute */
                }
            }
        }
        for (int i = 0; i < 2; ++i) {
            monster_t *m;
            m = (i == 0) ? &(g->evn.crystal) : &(g->evn.amoeba);
            if (m->exists && (m->counter <= 0)) {
                int x, y, x1, y1;
                const planet_t *p;
                p = &(g->planet[m->dest]);
                x1 = p->x;
                y1 = p->y;
                x = m->x;
                y = m->y;
                if (((x != x1) || (y != y1)) && (frame < 2)) {
                    util_math_go_line_dist(&x, &y, x1, y1, odd_frame ? 6 : 5);
                    if ((m->x != x) || (m->y != y)) {
                        game_move_en_dxdy(g, false, (i == 0) ? GAME_MOVE_INDEX_CRYSTAL : GAME_MOVE_INDEX_AMOEBA, x - m->x, y - m->y, 0xff, build_msg[0]);
                    }
                    m->x = x;
                    m->y = y;
                }
            }
        }
        for (player_id_t pi = 0; pi < g->players; ++pi) {
            if (BOOLVEC_IS1(build_msg, pi)) {
                GAME_MOVE_EN_T(g, pi, GAME_MOVE_FRAME_END, (frame < (MOVE_FRAMES_MAX - 1)) && flag_more);
            }
        }
        if (rng_countdown < 0) {
            ++rng_steps;
            rng_countdown = 3;
        } else {
            --rng_countdown;
        }
    }
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if (BOOLVEC_IS1(build_msg, pi)) {
            GAME_MSGO_EN_LEN(g, pi);
        }
    }
    game_server_msgo_flush(g);
    return rng_steps;
}

void game_turn_move_to_orbit(struct game_s *g)
{
    for (int i = 0; i < g->enroute_num; ++i) {
        fleet_enroute_t *r = &(g->enroute[i]);
        const planet_t *p;
        p = &(g->planet[r->dest]);
        if ((r->x == p->x) && (r->y == p->y)) {
            fleet_orbit_t *o;
            o = &(g->eto[r->owner].orbit[r->dest]);
            for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
                uint32_t s;
                s = o->ships[j] + r->ships[j];
                SETMIN(s, game_num_limit_ships);
                o->ships[j] = s;
            }
            util_table_remove_item_any_order(i, g->enroute, sizeof(fleet_enroute_t), g->enroute_num);
            --g->enroute_num;
            --i;
        }
    }
    game_update_visibility(g);
}
