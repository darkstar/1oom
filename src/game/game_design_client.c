#include "config.h"

#include "game_design.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_client.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_save.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_DESIGN   4

/* -------------------------------------------------------------------------- */

bool game_design_scrap_client(struct game_s *g, player_id_t pi, int shipi, bool flag_for_new)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_SCRAP_SHIP);
    GAME_MSGO_EN_U8(g, pi, shipi);
    GAME_MSGO_EN_U8(g, pi, flag_for_new ? 1 : 0);
    GAME_MSGO_EN_LEN(g, pi);
    if (game_client_msg_send_and_wait_reply(g, pi) == 1) {
        game_design_scrap(g, pi, shipi, flag_for_new);
        return true;
    }
    return false;
}

bool game_design_add_client(struct game_s *g, player_id_t pi, const shipdesign_t *sd)
{
    uint8_t *buf = GAME_MSGO_DPTR(g, pi);
    int pos = 0;
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_DESIGN_SHIP);
    pos = game_save_encode_sd(buf, pos, sd);
    GAME_MSGO_ADD_LEN(g, pi, pos);
    GAME_MSGO_EN_LEN(g, pi);
    if (game_client_msg_send_and_wait_reply(g, pi) == 1) {
        game_design_add(g, pi, sd, true);
        return true;
    }
    return false;
}
