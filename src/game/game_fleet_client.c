#include "config.h"

#include "game_fleet.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_client.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_FLEET    4

/* -------------------------------------------------------------------------- */

bool game_send_fleet_from_orbit_client(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS])
{
    uint8_t *buf = GAME_MSGO_DPTR(g, owner);
    int pos = 0;
    if (from == dest) {
        return false;
    }
    GAME_MSGO_EN_HDR(g, owner, GAME_MSG_ID_SEND_FLEET);
    SG_1OOM_EN_U8(from);
    SG_1OOM_EN_U8(dest);
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        SG_1OOM_EN_U16(ships[i]);
    }
    GAME_MSGO_ADD_LEN(g, owner, pos);
    GAME_MSGO_EN_LEN(g, owner);
    if (game_client_msg_send_and_wait_reply(g, owner) == 1) {
        game_send_fleet_from_orbit(g, owner, from, dest, ships);
        return true;
    }
    return false;
}

bool game_fleet_redirect_client(struct game_s *g, struct fleet_enroute_s *r, uint8_t pfrom, uint8_t pto)
{
    player_id_t pi = r->owner;
    uint8_t *buf = GAME_MSGO_DPTR(g, pi);
    int pos = 0;
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_REDIR_FLEET);
    SG_1OOM_EN_U8(pto);
    SG_1OOM_EN_U8(r->dest);
    SG_1OOM_EN_U16(r->x);
    SG_1OOM_EN_U16(r->y);
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        SG_1OOM_EN_U16(r->ships[i]);
    }
    GAME_MSGO_ADD_LEN(g, pi, pos);
    GAME_MSGO_EN_LEN(g, pi);
    if (game_client_msg_send_and_wait_reply(g, pi) == 1) {
        game_fleet_redirect(g, r, pfrom, pto);
        return true;
    }
    return false;
}

bool game_transport_redirect_client(struct game_s *g, struct transport_s *r, uint8_t pto)
{
    player_id_t pi = r->owner;
    uint8_t *buf = GAME_MSGO_DPTR(g, pi);
    int pos = 0;
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_REDIR_TRANS);
    SG_1OOM_EN_U8(pto);
    SG_1OOM_EN_U8(r->dest);
    SG_1OOM_EN_U16(r->x);
    SG_1OOM_EN_U16(r->y);
    SG_1OOM_EN_U16(r->pop);
    GAME_MSGO_ADD_LEN(g, pi, pos);
    GAME_MSGO_EN_LEN(g, pi);
    if (game_client_msg_send_and_wait_reply(g, pi) == 1) {
        game_transport_redirect(g, r, pto);
        return true;
    }
    return false;
}
