#include "config.h"

#include "game_client.h"
#include "bits.h"
#include "boolvec.h"
#include "game.h"
#include "game_aux.h"
#include "game_bomb.h"
#include "game_election.h"
#include "game_explore.h"
#include "game_ground.h"
#include "game_misc.h"
#include "game_move.h"
#include "game_msg.h"
#include "game_newtech.h"
#include "game_save.h"
#include "game_server.h"
#include "game_spy.h"
#include "game_tech.h"
#include "game_transport.h"
#include "game_turn.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_CLIENT   4

/* -------------------------------------------------------------------------- */

int game_client_msg_send(struct game_s *g, int pi)
{
    if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) {
        int len;
        if (g->gaux->msgi[pi].len != 0) {
            LOG_DEBUG((0, "BUG: %s overriding player %i msgi 0x%x len %i with msgo 0x%x len %i\n", __func__, pi, GET_LE_16(g->gaux->msgi[pi].buf), g->gaux->msgi[pi].len, GET_LE_16(g->gaux->msgo[pi].buf), g->gaux->msgo[pi].len));
        }
        g->gaux->msgi[pi].len = len = g->gaux->msgo[pi].len;
        memcpy(g->gaux->msgi[pi].buf, g->gaux->msgo[pi].buf, len);
        g->gaux->msgo[pi].len = 0;
        game_server_msg_handle(g->gaux->g, pi);
        return 0;
    } else {
        return -1;  /* FIXME */
    }
}

int game_client_msg_recv(struct game_s *g, int pi)
{
    if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) {
        if (g->gaux->msgi[pi].len != 0) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return -1;  /* FIXME */
    }
}

int game_client_msg_send_and_wait_reply(struct game_s *g, int pi)
{
#define MAX_LOOPS   10000
    int res, loops;
    g->gaux->msgi[pi].status = 0;
    res = game_client_msg_send(g, pi);
    if (res < 0) {
        return res;
    }
    loops = 0;
    while ((g->gaux->msgi[pi].status == 0) && (++loops < MAX_LOOPS)) {
        /* TODO ui tick, ... */
    }
    if (loops < MAX_LOOPS) {
        return g->gaux->msgi[pi].status;
    } else {
        return -1;
    }
#undef MAX_LOOPS
}

int game_client_msg_handle(struct game_s *g, int pi)
{
    uint16_t id, len, dlen;
    int res;
    len = GAME_MSGI_LEN(g, pi);
    if (len < 4) {
        LOG_DEBUG((0, "BUG: %s: no msg for pi %i\n", __func__, pi));
        return 0;
    }
    id = GAME_MSGI_DE_ID(g, pi);
    dlen = GAME_MSGI_DE_LEN(g, pi);
    LOG_DEBUG((DEBUGLEVEL_CLIENT, "%s: got msg id 0x%0x len %i dlen %i\n", __func__, id, len, dlen));
    if ((dlen + 4) != len) {
        log_fatal_and_die("%s: invalid msg len %i dlen %i id 0x%x\n", __func__, len, dlen, id);
    }
    switch (id) {
        case GAME_MSG_ID_NONE:
            res = 0;
            break;
        case GAME_MSG_ID_CMD_STATUS:
            g->gaux->msgi[pi].status = GET_LE_16(GAME_MSGI_DPTR(g, pi));
            res = 0;
            break;
        case GAME_MSG_ID_GAME_DATA:
            res = game_save_decode(GAME_MSGI_DPTR(g, pi), dlen, g, 0);
            game_update_production(g);
            game_update_tech_util(g);
            game_update_within_range(g);
            game_update_visibility(g);
            game_update_have_reserve_fuel(g);
            break;
        case GAME_MSG_ID_TURN_MOVE:
            res = game_turn_move_show(g, pi);
            break;
        case GAME_MSG_ID_EXPLORE:
            res = game_turn_explore_show(g, pi);
            break;
        case GAME_MSG_ID_COLONIZEQ:
            res = game_turn_colonize_ask(g, pi);
            break;
        case GAME_MSG_ID_GROUND:
            res = game_turn_ground_show(g, pi);
            break;
        case GAME_MSG_ID_BOMBARD:
            res = game_turn_bomb_show(g, pi);
            break;
        case GAME_MSG_ID_BOMBQ:
            res = game_turn_bomb_ask(g, pi);
            break;
        case GAME_MSG_ID_NEWTECH:
            res = game_turn_newtech_show(g, pi);
            goto skip_handled;
        case GAME_MSG_ID_STEALQ:
            res = game_spy_steal_ask(g, pi);
            break;
        case GAME_MSG_ID_STOLEN:
            res = game_spy_stolen_show(g, pi);
            break;
        case GAME_MSG_ID_SABQ:
            res = game_spy_sabotage_ask(g, pi);
            break;
        case GAME_MSG_ID_SABOTAGE:
            res = game_spy_sabotage_show(g, pi);
            break;
        case GAME_MSG_ID_TRANSSHOT:
            res = game_turn_transport_destroyed_show(g, pi);
            break;
        case GAME_MSG_ID_COUNCILS:
            res = game_election_client_start(g, pi);
            break;
        case GAME_MSG_ID_VOTED:
            res = game_election_client_voted(g, pi);
            break;
        case GAME_MSG_ID_VOTEQ:
            res = game_election_client_voteq(g, pi);
            break;
        case GAME_MSG_ID_COUNCILR:
            res = game_election_client_result(g, pi);
            break;
        case GAME_MSG_ID_COUNCILE:
            res = game_election_client_end(g, pi);
            break;
        case GAME_MSG_ID_END:
            res = game_turn_client_end(g, pi);
            break;
        default:
            log_warning("%s: unknown msg id 0x%x\n", __func__, id);
            res = -1;
            break;
    }
    GAME_MSGI_HANDLED(g, pi);
skip_handled:
    g->gaux->cl_last_msg = id;
    LOG_DEBUG((DEBUGLEVEL_CLIENT, "%s: msg id 0x%0x handled\n", __func__, id));
    if (res == 1) {
        game_client_msg_send(g, pi);
    }
    return res;
}

/* -------------------------------------------------------------------------- */
/* game server message passing for non-networked play */

int game_server_msgo_flush_player(struct game_s *g, int pi)
{
    if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) {
        if (IS_HUMAN(g, pi) && IS_ALIVE(g, pi)) {
            int len;
            if (g->gaux->msgi[pi].len != 0) {
                LOG_DEBUG((0, "BUG: %s overriding player %i msgi 0x%x len %i with msgo 0x%x len %i\n", __func__, pi, GET_LE_16(g->gaux->msgi[pi].buf), g->gaux->msgi[pi].len, GET_LE_16(g->gaux->msgo[pi].buf), g->gaux->msgo[pi].len));
            }
            g->gaux->msgi[pi].len = len = g->gaux->msgo[pi].len;
            memcpy(g->gaux->msgi[pi].buf, g->gaux->msgo[pi].buf, len);
            g->gaux->msgo[pi].len = 0;
            if (len != 0) {
                game_client_msg_handle(g->gaux->gview[pi], pi);
            }
        }
        return 1;
    } else {
        LOG_DEBUG((0, "BUG: %s called in multiplayer mode %i\n", __func__, g->gaux->multiplayer));
        return -1;
    }
}

int game_server_msgo_flush(struct game_s *g)
{
    if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) {
        for (player_id_t pi = 0; pi < g->players; ++pi) {
            game_server_msgo_flush_player(g, pi);
        }
        return 1;
    } else {
        LOG_DEBUG((0, "BUG: %s called in multiplayer mode %i\n", __func__, g->gaux->multiplayer));
        return -1;
    }
}

int game_server_msg_wait(struct game_s *g)
{
    return 0;
}
