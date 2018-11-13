#include "config.h"

#include "game_server.h"
#include "bits.h"
#include "boolvec.h"
#include "game.h"
#include "game_aux.h"
#include "game_bomb.h"
#include "game_design.h"
#include "game_election.h"
#include "game_explore.h"
#include "game_fleet.h"
#include "game_msg.h"
#include "game_newtech.h"
#include "game_planet.h"
#include "game_spy.h"
#include "game_tech.h"
#include "game_turn.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

int game_server_msg_send_status(struct game_s *g, int pi, int status)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_CMD_STATUS);
    GAME_MSGO_EN_U16(g, pi, status);
    GAME_MSGO_EN_LEN(g, pi);
    return game_server_msgo_flush_player(g, pi);
}

int game_server_msg_handle(struct game_s *g, int pi)
{
    uint16_t id, len, dlen;
    int res;
    id = GAME_MSGI_DE_ID(g, pi);
    len = GAME_MSGI_LEN(g, pi);
    dlen = GAME_MSGI_DE_LEN(g, pi);
    if ((len < 4) || ((dlen + 4) != len)) {
        log_fatal_and_die("%s: invalid msg len %i dlen %i id 0x%x\n", __func__, len, dlen, id);
    }
    switch (id) {
        case GAME_MSG_ID_NONE:
            res = 0;
            break;
        case GAME_MSG_ID_TURN_INPUT:
            res = game_turn_server_turn_input_msg(g, pi);
            /* TODO track when all turn inputs are submitted */
            break;
        case GAME_MSG_ID_SEND_FLEET:
            res = game_fleet_server_send_fleet_msg(g, pi);
            break;
        case GAME_MSG_ID_REDIR_FLEET:
            res = game_fleet_server_redir_fleet_msg(g, pi);
            break;
        case GAME_MSG_ID_REDIR_TRANS:
            res = game_fleet_server_redir_trans_msg(g, pi);
            break;
        case GAME_MSG_ID_SEND_BC:
            res = game_planet_server_send_bc_msg(g, pi);
            break;
        case GAME_MSG_ID_SCRAP_BASES:
            res = game_planet_server_scrap_bases_msg(g, pi);
            break;
        case GAME_MSG_ID_SCRAP_SHIP:
            res = game_design_server_scrap_msg(g, pi);
            break;
        case GAME_MSG_ID_DESIGN_SHIP:
            res = game_design_server_add_msg(g, pi);
            break;
        case GAME_MSG_ID_COLONIZEA:
            res = game_turn_server_colonize_msg(g, pi);
            break;
        case GAME_MSG_ID_BOMBA:
            res = game_turn_server_bomb_msg(g, pi);
            break;  /* TODO or goto skip_handled? */
        case GAME_MSG_ID_NEXTTECH:
            res = game_turn_newtech_next_msg(g, pi);
            break;
        case GAME_MSG_ID_FRAMEESP:
            res = game_turn_newtech_frame_msg(g, pi);
            break;
        case GAME_MSG_ID_STEALA:
            res = game_spy_server_steal_msg(g, pi);
            break;  /* TODO or goto skip_handled? */
        case GAME_MSG_ID_SABA:
            res = game_spy_server_saba_msg(g, pi);
            break;  /* TODO or goto skip_handled? */
        case GAME_MSG_ID_FRAMESAB:
            res = game_spy_server_framesab_msg(g, pi);
            break;
        case GAME_MSG_ID_VOTEA:
            res = game_election_server_votea_msg(g, pi);
            break;
        case GAME_MSG_ID_COUNCILA:
            res = game_election_server_councila_msg(g, pi);
            break;
        default:
            log_warning("%s: unknown msg id 0x%x\n", __func__, id);
            res = -1;
            break;
    }
    GAME_MSGI_HANDLED(g, pi);
    if (res > 0) {
        res = game_server_msg_send_status(g, pi, res);
    }
    return res;
}
