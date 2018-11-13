#include "config.h"

#include <stdio.h>

#include "game_transport.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_str.h"
#include "log.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_TRANSPORT    4

/* -------------------------------------------------------------------------- */

int game_turn_transport_destroyed_show(struct game_s *g, player_id_t pi)
{
    char *strbuf = ui_get_strbuf();
    const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
    int pos = 0;
    while (1) {
        planet_t *p;
        uint8_t pli, owner;
        SG_1OOM_DE_U8(pli);
        if (pli == 0xff) {
            break;
        }
        SG_1OOM_DE_U8(owner);
        p = &(g->planet[pli]);
        LOG_DEBUG((DEBUGLEVEL_TRANSPORT, "%s: pi:%i p:%i'%s' owner:p:%i,t:%i\n", __func__, pi, pli, p->name, p->owner, owner));
        if (p->owner == PLAYER_NONE) {
            sprintf(strbuf, "%s %s %s", game_str_sm_trbdb1, p->name, game_str_sm_trbdb2);
            ui_turn_msg(g, pi, strbuf);
        } else {
            const char *s;
            if (IS_HUMAN(g, owner)) {
                s = game_str_sb_your;
            } else {
                s = game_str_tbl_race[g->eto[owner].race];
            }
            sprintf(strbuf, "%s %s %s %s", s, game_str_sm_traad1, p->name, game_str_sm_traad2);
            ui_turn_msg(g, pi, strbuf);
        }
    }
    return 0;
}
