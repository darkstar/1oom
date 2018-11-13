#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_newtech.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_endecode.h"
#include "game_msg.h"
#include "game_server.h"
#include "game_tech.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_NEWTECH  4

/* -------------------------------------------------------------------------- */

static void game_turn_newtech_msg(const struct game_s *g, player_id_t pi)
{
    const newtechs_t *nts = &(g->evn.newtech[pi]);
    GAME_MSGO_EN_HDR(g, pi, GAME_MSG_ID_NEWTECH);
    GAME_MSGO_EN_U8(g, pi, nts->num);
    for (int i = 0; i < nts->num; ++i) {
        const newtech_t *nt = &(nts->d[i]);
        GAME_MSGO_EN_U8(g, pi, nt->field);
        GAME_MSGO_EN_U8(g, pi, nt->tech);
        GAME_MSGO_EN_U8(g, pi, nt->source);
        GAME_MSGO_EN_U8(g, pi, nt->v06);
        GAME_MSGO_EN_U8(g, pi, nt->stolen_from);
        GAME_MSGO_EN_U8(g, pi, nt->frame ? 1 : 0);
        GAME_MSGO_EN_U8(g, pi, nt->other1);
        GAME_MSGO_EN_U8(g, pi, nt->other2);
    }
    for (tech_field_t field = 0; field < TECH_FIELD_NUM; ++field) {
        const nexttech_t *xt = &(nts->next[field]);
        GAME_MSGO_EN_U8(g, pi, xt->num);
        GAME_MSGO_EN_TBL_U8(g, pi, xt->tech, TECH_NEXT_MAX);
    }
    GAME_MSGO_EN_LEN(g, pi);
}

/* -------------------------------------------------------------------------- */

int game_turn_newtech_send_single(struct game_s *g, player_id_t pi)
{
    struct turn_newtech_s *tn = &(g->gaux->turn.newtech);
    uint8_t can_choose = game_tech_finish_new(g, pi);
    bool need_answer = false;
    if (can_choose) {
        ++tn->num_choose;
        need_answer = true;
    }
    if (g->evn.newtech[pi].d[0].frame) {
        ++tn->num_frame;
        need_answer = true;
    }
    game_turn_newtech_msg(g, pi);
    game_server_msgo_flush_player(g, pi);
    return need_answer ? 1 : 0;
}

int game_turn_newtech(struct game_s *g)
{
    /* 1. Finish newtechs for all human players
       2. Send newtech messages
       3. Wait for new tech / frame messages until all done
    */
    struct turn_newtech_s *tn = &(g->gaux->turn.newtech);
    BOOLVEC_DECLARE(msg_started, PLAYER_NUM);
    BOOLVEC_CLEAR(msg_started, PLAYER_NUM);
    tn->num_choose = 0;
    tn->num_frame = 0;
    g->gaux->turn_phase = GAME_TURN_NEWTECH;
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        uint8_t can_choose;
        if (IS_AI(g, pi) || (!IS_ALIVE(g, pi))) {
            continue;
        }
        can_choose = game_tech_finish_new(g, pi);
        for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
            if (can_choose & (1 << f)) {
                ++tn->num_choose;
            }
        }
        if (g->evn.newtech[pi].num != 0) {
            BOOLVEC_SET1(msg_started, pi);
        }
        for (int i = 0; i < g->evn.newtech[pi].num; ++i) {
            if (g->evn.newtech[pi].d[i].frame) {
                ++tn->num_frame;
            }
        }
        if (can_choose || (g->evn.newtech[pi].num != 0)) {
            BOOLVEC_SET1(msg_started, pi);
            game_turn_newtech_msg(g, pi);
        }
    }
    if (BOOLVEC_IS_CLEAR(msg_started, PLAYER_NUM)) {
        return 0;
    }
    game_server_msgo_flush(g);
    while (tn->num_choose || tn->num_frame) {
        game_server_msg_wait(g);
        if (g->gaux->multiplayer == GAME_MULTIPLAYER_LOCAL) {
            log_fatal_and_die("BUG: %s: nc:%i nf:%i\n", __func__, tn->num_choose, tn->num_frame);
        }
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        g->evn.newtech[pi].num = 0;
    }
    return 0;
}

int game_turn_newtech_next_msg(struct game_s *g, player_id_t pi)
{
    nexttech_t *xt;
    uint8_t field, tech;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
        SG_1OOM_DE_U8(field);
        SG_1OOM_DE_U8(tech);
        if (pos != len) {
            log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
        }
    }
    if (field >= TECH_FIELD_NUM) {
        log_warning("%s: pi %i msg field %u >= %u\n", __func__, pi, field, TECH_FIELD_NUM);
        return 2;
    }
    xt = &(g->evn.newtech[pi].next[field]);
    if (xt->num == 0) {
        log_warning("%s: pi %i msg field %u num == 0\n", __func__, pi, field);
        return 2;
    }
    for (int i = 0; i < xt->num; ++i) {
        if (xt->tech[i] == tech) {
            LOG_DEBUG((DEBUGLEVEL_NEWTECH, "%s: pi %i msg ok\n", __func__, pi));
            game_tech_start_next(g, pi, field, tech);
            xt->num = 0;
            --g->gaux->turn.newtech.num_choose;
            BOOLVEC_SET0(g->gaux->turn.newtech.answer, pi);
            return 1;
        }
    }
    log_warning("%s: pi %i msg field %u tech %u not in table\n", __func__, pi, field, tech);
    return 2;
}

int game_turn_newtech_frame_msg(struct game_s *g, player_id_t pi)
{
    newtechs_t *nts = &(g->evn.newtech[pi]);
    newtech_t *nt;
    uint8_t newtech_i, victim;
    {
        const uint8_t *buf = GAME_MSGI_DPTR(g, pi);
        int pos = 0, len = GAME_MSGI_DE_LEN(g, pi);
        SG_1OOM_DE_U8(newtech_i);
        SG_1OOM_DE_U8(victim);
        if (pos != len) {
            log_warning("%s: pl %i len %i != %i\n", __func__, pi, pos, len);
        }
    }
    if (newtech_i >= nts->num) {
        log_warning("%s: pi %i msg newtech_i %u >= %u\n", __func__, pi, newtech_i, nts->num);
        return 2;
    }
    nt = &(g->evn.newtech[pi].d[newtech_i]);
    if (!nt->frame) {
        log_warning("%s: pi %i msg newtech_i %u no frame\n", __func__, pi, newtech_i);
        return 2;
    }
    if (victim != PLAYER_NONE) {
        if ((victim == nt->other1) || (victim == nt->other2)) {
            g->evn.stolen_spy[nt->stolen_from][pi] = victim;
            game_diplo_esp_frame(g, victim, nt->stolen_from);
        } else {
            log_warning("%s: pi %i msg newtech_i %u invalid victim %u\n", __func__, pi, newtech_i, victim);
            return 2;
        }
    }
    LOG_DEBUG((DEBUGLEVEL_NEWTECH, "%s: pi %i msg ok\n", __func__, pi));
    nt->frame = false;
    --g->gaux->turn.newtech.num_frame;
    if (nts->next[nt->field].num == 0) {
        BOOLVEC_SET0(g->gaux->turn.newtech.answer, pi);
    }
    return 1;
}
