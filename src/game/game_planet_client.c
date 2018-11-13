#include "config.h"

#include "game_planet.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_client.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_PLANET   4

/* -------------------------------------------------------------------------- */

bool game_planet_send_bc_client(struct game_s *g, player_id_t pi, uint8_t planet_i, uint32_t bc)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_SEND_BC);
    GAME_MSGO_EN_U8(g, pi, planet_i);
    GAME_MSGO_EN_U32(g, pi, bc);
    GAME_MSGO_EN_LEN(g, pi);
    if (game_client_msg_send_and_wait_reply(g, pi) == 1) {
        game_planet_send_bc(g, pi, planet_i, bc);
        return true;
    }
    return false;
}

bool game_planet_scrap_bases_client(struct game_s *g, player_id_t pi, uint8_t planet_i, uint16_t num)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_SCRAP_BASES);
    GAME_MSGO_EN_U8(g, pi, planet_i);
    GAME_MSGO_EN_U16(g, pi, num);
    GAME_MSGO_EN_LEN(g, pi);
    if (game_client_msg_send_and_wait_reply(g, pi) == 1) {
        game_planet_scrap_bases(g, pi, planet_i, num);
        return true;
    }
    return false;
}
