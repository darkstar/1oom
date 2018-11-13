#include "config.h"

#include "game_election.h"
#include "boolvec.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "log.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_ELECTION 3

/* -------------------------------------------------------------------------- */

int game_election_server_votea_msg(struct game_s *g, player_id_t pi)
{
    struct turn_election_s *te = &(g->gaux->turn.election);
    uint8_t voted;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
        SG_1OOM_DE_U8(voted);
        if (pos != len) {
            log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
        }
    }
    if (voted >= 3/*unknown*/) {
        log_warning("%s: pi %i msg voted %u >= 3\n", __func__, pi, voted);
        return 2;
    }
    if (pi != te->pending) {
        log_warning("%s: pi %i not pending %u\n", __func__, pi, te->pending);
        return 2;
    }
    LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: pi %i msg ok\n", __func__, pi));
    te->voted = voted;
    return 1;
}

int game_election_server_councila_msg(struct game_s *g, player_id_t pi)
{
    struct turn_election_s *te = &(g->gaux->turn.election);
    uint8_t refused;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
        SG_1OOM_DE_U8(refused);
        if (pos != len) {
            log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
        }
    }
    if (refused > 1) {
        log_warning("%s: pi %i msg refused %u > 1\n", __func__, pi, refused);
        return 2;
    }
    if (BOOLVEC_IS0(te->answer, pi)) {
        log_warning("%s: pi %i not answer %u\n", __func__, pi, te->answer);
        return 2;
    }
    LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: pi %i msg ok\n", __func__, pi));
    BOOLVEC_SET0(te->answer, pi);
    BOOLVEC_SET(g->refuse, pi, (refused != 0));
    return 1;
}
