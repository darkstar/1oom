#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_view.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

void game_view_update_from(struct game_s *g, const struct game_s *gv, player_id_t pi)
{
    {
        empiretechorbit_t *e = &(g->eto[pi]);
        const empiretechorbit_t *ev = &(gv->eto[pi]);
        e->tax = ev->tax;
        e->security = ev->security;
        for (int j = 0; j < g->players; ++j) {
            e->spying[j] = ev->spying[j];
            e->spymode[j] = ev->spymode[j];
        }
        for (int j = 0; j < TECH_FIELD_NUM; ++j) {
            e->tech.slider[j] = e->tech.slider[j];
            e->tech.slider_lock[j] = e->tech.slider_lock[j];
        }
    }
    {
        gameevents_t *e = &(g->evn);
        const gameevents_t *ev = &(gv->evn);
        e->gov_eco_mode[pi] = ev->gov_eco_mode[pi];
        BOOLVEC_SET(e->gov_no_stargates, pi, BOOLVEC_IS1(ev->gov_no_stargates, pi));
        BOOLVEC_TBL_COPY1(e->help_shown, ev->help_shown[pi], pi, HELP_SHOWN_NUM);
        BOOLVEC_TBL_COPY1(e->msg_filter, ev->msg_filter[pi], pi, FINISHED_NUM);
    }
    g->planet_focus_i[pi] = gv->planet_focus_i[pi];
    g->current_design[pi] = gv->current_design[pi];
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if (p->owner == pi) {
            const planet_t *pv = &(gv->planet[i]);
            p->buildship = pv->buildship;
            for (int j = 0; j < PLANET_SLIDER_NUM; ++j) {
                p->slider[j] = pv->slider[j];
                p->slider_lock[j] = pv->slider_lock[j];
            }
            p->reloc = pv->reloc;
            p->trans_dest = pv->trans_dest;
            p->trans_num = pv->trans_num;
            p->target_bases = pv->target_bases;
            BOOLVEC_COPY(p->extras, pv->extras, PLANET_EXTRAS_NUM);
        }
    }
}

void game_view_update_all(struct game_s *g)
{
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi)) { /* TODO is server or !remote */
            game_view_update_from(g, g->gaux->gview[pi], pi);
        }
    }
}

void game_view_make(const struct game_s *g, struct game_s *gv, player_id_t pi)
{
    /* TODO censor secrets */
    struct game_aux_s *gaux = gv->gaux;
    memcpy(gv, g, sizeof(struct game_s));
    gv->gaux = gaux;
    gv->active_player = pi;
}

void game_view_make_all(struct game_s *g)
{
    for (player_id_t pi = 0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi)) { /* TODO is server or !remote */
            game_view_make(g, g->gaux->gview[pi], pi);
        }
    }
}
