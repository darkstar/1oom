#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_election.h"
#include "boolvec.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_server.h"
#include "game_tech.h"
#include "log.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_ELECTION 3

/* -------------------------------------------------------------------------- */

static void game_election_msg_start(const struct game_s *g, player_id_t pi, const struct election_s *el)
{
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_COUNCILS);
    GAME_MSGO_EN_U16(g, pi, el->total_votes);
    GAME_MSGO_EN_TBL_U8(g, pi, el->candidate, 2);
    GAME_MSGO_EN_U8(g, pi, el->num);
    for (int i = 0; i < el->num; ++i) {
        uint8_t vpi;
        vpi = el->tbl_ei[i];
        GAME_MSGO_EN_U8(g, pi, vpi);
        GAME_MSGO_EN_U8(g, pi, el->tbl_votes[vpi]);
        GAME_MSGO_EN_U8(g, pi, el->voted[i]);
    }
    GAME_MSGO_EN_LEN(g, pi);
}

static void game_election_prepare(struct election_s *el)
{
    const struct game_s *g = el->g;
    uint16_t tbl_votes[PLAYER_NUM];
    player_id_t tbl_ei[PLAYER_NUM];
    int num = 0, total_votes = 0;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if (IS_ALIVE(g, i) && IS_AI(g, i)) {
            el->tbl_ei[num++] = i;
        }
    }
    el->num_ai = num;
    {
        bool found_human = false;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (IS_ALIVE(g, i) && IS_HUMAN(g, i)) {
                el->last_human = i;
                if (!found_human) {
                    found_human = true;
                    el->first_human = i;
                }
                el->tbl_ei[num++] = i;
            }
        }
    }
    el->num = num;
    memset(el->voted, 3/*unknown*/, sizeof(el->voted));
    memset(tbl_votes, 0, sizeof(tbl_votes));
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if ((p->owner != PLAYER_NONE) && (p->unrest != PLANET_UNREST_REBELLION)) {
            tbl_votes[p->owner] += p->pop;
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        uint16_t v;
        v = tbl_votes[i];
        v = (v / 100) + ((v % 100) != 0);
        tbl_votes[i] = v;
        total_votes += v;
    }
    el->total_votes = total_votes;
    for (player_id_t i = PLAYER_0; i < PLAYER_NUM; ++i) {
        tbl_ei[i] = i;
        el->tbl_votes[i] = tbl_votes[i];
    }
    for (int loops = 0; loops < g->players; ++loops) {
        for (player_id_t i = PLAYER_0; i < (g->players - 1); ++i) {
            uint16_t v0, v1;
            v0 = tbl_votes[i];
            v1 = tbl_votes[i + 1];
            if (v0 < v1) {
                tbl_votes[i + 1] = v0; tbl_votes[i] = v1;
                v0 = tbl_ei[i]; tbl_ei[i] = tbl_ei[i + 1]; tbl_ei[i + 1] = v0;
            }
        }
    }
    if (IS_AI(g, tbl_ei[0]) && IS_HUMAN(g, tbl_ei[1])) {
        el->candidate[0] = tbl_ei[1];
        el->candidate[1] = tbl_ei[0];
    } else {
        el->candidate[0] = tbl_ei[0];
        el->candidate[1] = tbl_ei[1];
    }
    el->flag_show_votes = false;
    el->got_votes[0] = 0;
    el->got_votes[1] = 0;
    el->cur_i = PLAYER_NONE;
    memset(el->voted, 3/*unknown*/, sizeof(el->voted));
}

static void game_election_accept(struct election_s *el)
{
    struct game_s *g = el->g;
    if (!BOOLVEC_IS_CLEAR(g->refuse, PLAYER_NUM)) {
        for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
            if (BOOLVEC_IS1(g->refuse, p1)) {
                continue;
            }
            for (player_id_t ph = el->first_human; ph <= el->last_human; ++ph) {
                if (BOOLVEC_IS0(g->refuse, ph)) {
                    continue;
                }
                game_diplo_break_trade(g, ph, p1);
                game_diplo_set_treaty(g, ph, p1, TREATY_FINAL_WAR);
            }
            for (player_id_t p2 = p1; p2 < g->players; ++p2) {
                if (BOOLVEC_IS1(g->refuse, p2)) {
                    continue;
                }
                game_diplo_set_treaty(g, p1, p2, TREATY_ALLIANCE);
            }
        }
        if (BOOLVEC_IS1(g->refuse, g->winner)) {
            if (g->winner == el->candidate[0]) {
                g->winner = el->candidate[1];
            } else {
                g->winner = el->candidate[0];
            }
        }
        game_tech_final_war_share(g);
        g->end = GAME_END_FINAL_WAR;
    }
}

/* -------------------------------------------------------------------------- */

void game_election(struct game_s *g)
{
    /* 1. For all AIs, vote
       2. Send empires, candidates, votes to players
       3. For all non-AIs, ask and wait for vote and send to players
       4. Send election results
       5. If got winner, wait for refusals
       6. Send election end
    */
    struct election_s *el = &(g->evn.election);
    struct turn_election_s *te = &(g->gaux->turn.election);
    uint8_t winner;
    te->pending = PLAYER_NONE;
    te->voted = 3/*unknown*/;
    BOOLVEC_CLEAR(te->answer, PLAYER_NUM);
    el->g = g;
    game_election_prepare(el);
    LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: tv:%i c:%i,%i n:%i", __func__, el->total_votes, el->candidate[0], el->candidate[1], el->num));
    for (int i = 0; i < el->num_ai; ++i) {
        int votefor, n;
        player_id_t player;
        player = el->tbl_ei[i];
        n = el->tbl_votes[player];
        votefor = game_ai->vote(el, player);
        LOG_DEBUG((DEBUGLEVEL_ELECTION, " %i:%i:%i", player, n, votefor));
        if (n == 0) {
            votefor = 0;
        }
        el->voted[i] = votefor;
        if (votefor == 0) {
            g->evn.voted[player] = PLAYER_NONE;
            game_diplo_act(g, -6, player, el->candidate[1], 0, 0, 0);
            game_diplo_act(g, -6, player, el->candidate[0], 0, 0, 0);
        } else {
            player_id_t pfor, pnot;
            pfor = el->candidate[votefor - 1];
            pnot = el->candidate[(votefor - 1) ^ 1];
            el->got_votes[votefor - 1] += n;
            g->evn.voted[player] = pfor;
            game_diplo_act(g, 24, player, pfor, 0, 0, 0);
            game_diplo_act(g, -12, player, pnot, 0, 0, 0);
        }
    }
    LOG_DEBUG((DEBUGLEVEL_ELECTION, "\n"));
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (1
          && IS_HUMAN(g, pi) && IS_ALIVE(g, pi)
          && ((g->gaux->multiplayer != GAME_MULTIPLAYER_LOCAL) || (pi == el->first_human))
        ) {
            game_election_msg_start(g, pi, el);
        }
    }
    game_server_msgo_flush(g);
    for (int i = el->num_ai; i < el->num;) {
        player_id_t player;
        el->cur_i = i;
        player = el->tbl_ei[i];
        if (te->pending != player) {
            te->pending = player;
            te->voted = 3/*unknown*/;
            /* send q */
            GAME_MSGO_EN_HDR(g, player, GAME_MSG_ID_VOTEQ);
            GAME_MSGO_EN_LEN(g, player);
            game_server_msgo_flush_player(g, player);
        } else if (te->voted != 3/*unknown*/) {
            int votefor, n;
            votefor = te->voted;
            te->voted = 3/*unknown*/;
            te->pending = PLAYER_NONE;
            n = el->tbl_votes[player];
            if (n == 0) {
                votefor = 0;
            }
            if (votefor == 0) {
                g->evn.voted[player] = PLAYER_NONE;
                game_diplo_act(g, -6, player, el->candidate[1], 0, 0, 0);
                game_diplo_act(g, -6, player, el->candidate[0], 0, 0, 0);
            } else {
                player_id_t pfor, pnot;
                pfor = el->candidate[votefor - 1];
                pnot = el->candidate[(votefor - 1) ^ 1];
                el->got_votes[votefor - 1] += n;
                g->evn.voted[player] = pfor;
                if (el->candidate[0] == player) {
                    game_diplo_act(g, 24, player, pfor, 0, 0, 0);
                    game_diplo_act(g, -12, player, pnot, 0, 0, 0);
                } else {
                    game_diplo_act(g, 24, player, pfor, 80, 0, pfor);
                    game_diplo_act(g, -12, player, pnot, 79, 0, pfor);
                }
            }
            /* send voted */
            for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
                if (1
                  && IS_HUMAN(g, pi) && IS_ALIVE(g, pi)
                  && ((g->gaux->multiplayer != GAME_MULTIPLAYER_LOCAL) || (pi == el->first_human))
                ) {
                    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_VOTED);
                    GAME_MSGO_EN_U8(g, pi, i);
                    GAME_MSGO_EN_U8(g, pi, votefor);
                    GAME_MSGO_EN_LEN(g, pi);
                }
            }
            game_server_msgo_flush(g);
            /* next player */
            ++i;
        } else {
            game_server_msg_wait(g);
        }
    }
    for (winner = 0; winner < 2; ++winner) {
        if ((((el->total_votes + 1) * 2) / 3) <= el->got_votes[winner]) {
            break;
        }
    }
    if (winner < 2) {
        g->winner = el->candidate[winner];
        g->end = GAME_END_WON_GOOD;
        BOOLVEC_CLEAR(g->refuse, PLAYER_NUM);
        ++winner;
    } else {
        winner = 0;
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi) && IS_ALIVE(g, pi)) {
            GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_COUNCILR);
            GAME_MSGO_EN_U8(g, pi, winner);
            GAME_MSGO_EN_LEN(g, pi);
            if (winner) {
                BOOLVEC_SET1(te->answer, pi);
            }
        }
    }
    game_server_msgo_flush(g);
    if (g->end != GAME_END_NONE) {
        while (!BOOLVEC_IS_CLEAR(te->answer, PLAYER_NUM)) {
            game_server_msg_wait(g);
        }
        game_election_accept(el);
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (1
          && IS_HUMAN(g, pi) && IS_ALIVE(g, pi)
          && ((g->gaux->multiplayer != GAME_MULTIPLAYER_LOCAL) || (pi == el->first_human))
        ) {
            uint8_t refused = BOOLVEC_IS_CLEAR(g->refuse, PLAYER_NUM) ? 0 : 1;
            GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_COUNCILE);
            GAME_MSGO_EN_U8(g, pi, winner);
            GAME_MSGO_EN_U8(g, pi, refused);
            GAME_MSGO_EN_LEN(g, pi);
        }
    }
    game_server_msgo_flush(g);
}
