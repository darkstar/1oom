#include "config.h"

#include "game_design.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_num.h"
#include "game_save.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_DESIGN   4

/* -------------------------------------------------------------------------- */

int game_design_server_scrap_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    const empiretechorbit_t *e = &(g->eto[pi]);
    uint8_t shipi, v;
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_SCRAP_SHIP)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    SG_1OOM_DE_U8(shipi);
    if (shipi >= e->shipdesigns_num) {
        log_warning("%s: pi %i msg shipi %u >= %u\n", __func__, pi, shipi, e->shipdesigns_num);
        return 2;
    }
    if (e->shipdesigns_num <= 1) {
        log_warning("%s: pi %i msg can't scrap last design\n", __func__, pi);
        return 2;
    }
    SG_1OOM_DE_U8(v);
    if (v > 1) {
        log_warning("%s: pi %i msg flag %u > 1\n", __func__, pi, v);
        return 2;
    }
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    LOG_DEBUG((DEBUGLEVEL_DESIGN, "%s: pi %i msg ok\n", __func__, pi));
    game_design_scrap(g, pi, shipi, v == 1);
    return 1;
}

int game_design_server_add_msg(struct game_s *g, player_id_t pi)
{
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int res, pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
    shipdesign_t sd[1];
    if ((!GAME_MSGI_LEN(g, pi)) || (GAME_MSGI_DE_ID(g, pi) != GAME_MSG_ID_DESIGN_SHIP)) {
        log_fatal_and_die("%s: message id 0x%x len %i dlen %i\n", __func__, GAME_MSGI_DE_ID(g, pi), GAME_MSGI_LEN(g, pi));
        return -1;
    }
    {
        int sdnum = g->eto[pi].shipdesigns_num;
        if (sdnum >= NUM_SHIPDESIGNS) {
            log_warning("%s: pi %i msg designs %u >= %u\n", __func__, pi, sdnum, NUM_SHIPDESIGNS);
            return 2;
        }
    }
    pos = game_save_decode_sd(buf, pos, sd);
    if (pos != len) {
        log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
    }
    if ((res = game_design_check(g, pi, sd, true)) == DESIGN_ERR_NONE) {
        LOG_DEBUG((DEBUGLEVEL_DESIGN, "%s: pi %i msg ok\n", __func__, pi));
        game_design_add(g, pi, sd, true);
        return 1;
    } else {
        log_warning("%s: pi %i msg design not ok, error %i\n", __func__, pi, res);
        return 2 + res;
    }
}
