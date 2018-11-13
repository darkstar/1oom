#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_election.h"
#include "boolvec.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_diplo.h"
#include "game_msg.h"
#include "game_str.h"
#include "game_tech.h"
#include "log.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_ELECTION 3

/* -------------------------------------------------------------------------- */

static void game_election_client_show(struct game_s *g, player_id_t api, struct election_s *el, int i)
{
    char vbuf[0x20];
    int votefor, n;
    player_id_t player = el->tbl_ei[i];
    bool flag_show = 0
        || IS_AI(g, player)
        || (g->gaux->local_players > 1)
        || ((g->gaux->multiplayer != GAME_MULTIPLAYER_LOCAL) && (player != api))
        ;
    if (flag_show) {
        el->str = 0;
        el->ui_delay = 2;
        ui_election_delay(el, 5);
    }
    n = el->tbl_votes[player];
    votefor = el->voted[i];
    if (n == 0) {
        votefor = 0;
    }
    if (votefor == 0) {
        sprintf(el->buf, "%s %s %s%s%s",
                game_str_el_abs1, game_str_tbl_races[g->eto[player].race], game_str_el_abs2,
                game_election_print_votes(n, vbuf), game_str_el_dots
               );
    } else {
        player_id_t pfor;
        pfor = el->candidate[votefor - 1];
        sprintf(el->buf, "%i %s %s %s, %s %s %s",
                n, (n == 1) ? game_str_el_vote : game_str_el_votes, game_str_el_for,
                g->emperor_names[pfor], game_str_el_emperor, game_str_el_ofthe,
                game_str_tbl_races[g->eto[pfor].race]
               );
        el->got_votes[votefor - 1] += n;
    }
    if (flag_show) {
        el->str = el->buf;
        el->ui_delay = 3;
        ui_election_show(el);
    }
}

/* -------------------------------------------------------------------------- */

const char *game_election_print_votes(uint16_t n, char *buf)
{
    if (n == 0) {
        sprintf(buf, "%s %s", game_str_el_no, game_str_el_votes);
    } else {
        sprintf(buf, "%i %s", n, (n == 1) ? game_str_el_vote : game_str_el_votes);
    }
    return buf;
}

int game_election_client_start(struct game_s *g, player_id_t pi)
{
    struct election_s *el = &(g->evn.election);
    memset(el, 0, sizeof(struct election_s));
    el->g = g;
    el->buf = ui_get_strbuf();
    el->cur_i = PLAYER_NONE;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0;
        SG_1OOM_DE_U16(el->total_votes);
        SG_1OOM_DE_TBL_U8(el->candidate, 2);
        SG_1OOM_DE_U8(el->num);
        LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: pi:%i tv:%i c:%i,%i n:%i", __func__, pi, el->total_votes, el->candidate[0], el->candidate[1], el->num));
        for (int i = 0; i < el->num; ++i) {
            uint8_t vpi;
            SG_1OOM_DE_U8(vpi);
            el->tbl_ei[i] = vpi;
            SG_1OOM_DE_U8(el->tbl_votes[vpi]);
            SG_1OOM_DE_U8(el->voted[i]);
            LOG_DEBUG((DEBUGLEVEL_ELECTION, " %i:%i:%i", el->tbl_ei[i], el->tbl_votes[vpi], el->voted[i]));
        }
        LOG_DEBUG((DEBUGLEVEL_ELECTION, "\n"));
    }
    el->str = game_str_el_start;
    ui_election_start(el);
    el->ui_delay = 3;
    ui_election_show(el);
    sprintf(el->buf, "%s %s %s %s %s %s %s %s %s %s",
            game_str_el_emperor, g->emperor_names[el->candidate[0]], game_str_el_ofthe, game_str_tbl_races[g->eto[el->candidate[0]].race],
            game_str_el_and,
            game_str_el_emperor, g->emperor_names[el->candidate[1]], game_str_el_ofthe, game_str_tbl_races[g->eto[el->candidate[1]].race],
            game_str_el_nomin
           );
    el->str = el->buf;
    el->ui_delay = 2;
    ui_election_show(el);
    el->flag_show_votes = true;
    for (int i = 0; (i < el->num) && (el->voted[i] != 3/*unknown*/); ++i) {
        el->cur_i = i;
        game_election_client_show(g, pi, el, i);
    }
    /* TODO setup idle hook */
    return 0;
}

int game_election_client_voted(struct game_s *g, player_id_t pi)
{
    struct election_s *el = &(g->evn.election);
    uint8_t i, voted;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0;
        SG_1OOM_DE_U8(i);
        SG_1OOM_DE_U8(voted);
        LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: pi:%i i:%u v:%u\n", __func__, pi, i, voted));
    }
    el->voted[i] = voted;
    el->cur_i = i;
    game_election_client_show(g, pi, el, i);
    return 0;
}

int game_election_client_voteq(struct game_s *g, player_id_t pi)
{
    struct election_s *el = &(g->evn.election);
    char vbuf[0x20];
    uint8_t votefor, n;
    el->str = 0;
    el->ui_delay = 2;
    ui_election_delay(el, 5);
    n = el->tbl_votes[pi];
    if ((g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) && (g->gaux->local_players > 1)) {
        for (int i = 0; i < el->num; ++i) {
            if (el->tbl_ei[i] == pi) {
                el->cur_i = i;
                break;
            }
        }
        sprintf(el->buf, "%s (%s%s", g->emperor_names[pi], game_election_print_votes(n, vbuf), game_str_el_dots);
    } else {
        el->cur_i = PLAYER_NONE;
        sprintf(el->buf, "%s%s%s", game_str_el_your, game_election_print_votes(n, vbuf), game_str_el_dots);
    }
    el->str = el->buf;
    votefor = ui_election_vote(el, pi);
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_VOTEA);
    GAME_MSGO_EN_U8(g, pi, votefor);
    GAME_MSGO_EN_LEN(g, pi);
    return 1;
}

int game_election_client_result(struct game_s *g, player_id_t pi)
{
    struct election_s *el = &(g->evn.election);
    uint8_t winner;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0;
        SG_1OOM_DE_U8(winner);
        LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: pi:%i w:%u\n", __func__, pi, winner));
    }
    if (winner) {
        g->winner = el->candidate[winner - 1];
        sprintf(el->buf, "%s %i %s %s %s %s %s",
                game_str_el_chose1, g->year + YEAR_BASE, game_str_el_chose1,
                g->emperor_names[g->winner], game_str_el_ofthe, game_str_tbl_races[g->eto[g->winner].race],
                game_str_el_chose3 /* WASBUG MOO1 has the last period missing if second candidate won */
               );
        el->str = el->buf;
        el->cur_i = PLAYER_NONE;
        if (IS_AI(g, g->winner) || (g->gaux->local_players > 1)) {
            for (int i = 0; i < (el->num + 1); ++i) {
                if (el->tbl_ei[i] == g->winner) {
                    el->cur_i = i;
                    break;
                }
            }
        }
    } else {
        el->str = game_str_el_neither;
        el->cur_i = PLAYER_NONE;
    }
    if (!el->flag_results_shown) {
        el->flag_results_shown = true;
        el->ui_delay = 3;
        ui_election_show(el);
    }
    if (!winner) {
        return 0;
    } else {
        uint8_t refuse;
        if ((g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) && (g->gaux->local_players > 1)) {
            sprintf(el->buf, "(%s) %s", g->emperor_names[pi], game_str_el_accept);
            el->str = el->buf;
        } else {
            el->str = game_str_el_accept;
        }
        refuse = ui_election_accept(el, pi) ? 0 : 1;
        GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_COUNCILA);
        GAME_MSGO_EN_U8(g, pi, refuse);
        GAME_MSGO_EN_LEN(g, pi);
        return 1;
    }
}

int game_election_client_end(struct game_s *g, player_id_t pi)
{
    struct election_s *el = &(g->evn.election);
    uint8_t winner, refused;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0;
        SG_1OOM_DE_U8(winner);
        SG_1OOM_DE_U8(refused);
        LOG_DEBUG((DEBUGLEVEL_ELECTION, "%s: pi:%i w:%u r:%u\n", __func__, pi, winner, refused));
    }
    if (winner && refused) {
        int pos;
        pos = sprintf(el->buf, "%s", game_str_el_sobeit);
        if (g->winner == pi) {
            g->winner = el->candidate[(winner - 1) ^ 1];
            sprintf(&el->buf[pos], " %s %s %s", game_str_el_emperor, g->emperor_names[g->winner], game_str_el_isnow);
        }
        el->str = el->buf;
        for (int i = 0; i < el->num; ++i) {
            if (el->tbl_ei[i] == g->winner) {
                el->cur_i = i;
                break;
            }
        }
        el->ui_delay = 3;
        ui_election_show(el);
    }
    ui_election_end(el);
    /* TODO remove idle hook */
    return 0;
}
