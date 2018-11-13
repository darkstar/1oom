#include "config.h"

#include "game_spy.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_misc.h"
#include "game_tech.h"
#include "game_techtypes.h"
#include "log.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

static uint8_t game_spy_esp_sub3_sub1(struct game_s *g, uint8_t a0, tech_field_t field, uint16_t slen, const uint8_t *src)
{
    int v = 0;
    for (int i = 0; i < slen; ++i) {
        uint8_t techi;
        const uint8_t *p;
        techi = src[i];
        p = RESEARCH_D0_PTR(g->gaux, field, techi);
        if (a0 == p[0]) {
            v = RESEARCH_D0_B1(p);
        }
    }
    return v;
}

static void game_spy_esp_sub3(struct game_s *g, struct spy_esp_s *s, tech_field_t field, int tlen, const uint8_t *trc, int slen, const uint8_t *src)
{
    for (int i = 0; i < tlen; ++i) {
        const uint8_t *p;
        uint8_t techi, b0, b1;
        bool have_tech;
        techi = trc[i];
        p = RESEARCH_D0_PTR(g->gaux, field, techi);
        b0 = p[0];
        b1 = RESEARCH_D0_B1(p);
        have_tech = false;
        for (int j = 0; j < slen; ++j) {
            if (src[j] == techi) {
                have_tech = true;
                break;
            }
        }
        if (0
          || ((b0 >= 3) && (b0 <= 6))
          || (b0 == 10) || (b0 == 18) || (b0 == 21)
          || ((b0 >= 12) && (b0 <= 16))
        ) {
            if (game_spy_esp_sub3_sub1(g, b0, field, slen, src) >= b1) {
                have_tech = true;
            }
        }
        /*632ff*/
        if ((g->eto[s->spy].race == RACE_SILICOID) && ((b0 == 5) || (b0 == 13) || (b0 == 14))) {
            have_tech = true;
        }
        if (!have_tech) {
            s->tbl_techi[field][s->tbl_num[field]++] = techi;
        }
    }
}

static int game_spy_esp_get_value(struct game_s *g, struct spy_esp_s *s, tech_field_t field, uint8_t techi)
{
    empiretechorbit_t *es = &(g->eto[s->spy]);
    const shipresearch_t *srds = &(g->srd[s->spy]);
    uint8_t b0 = RESEARCH_D0_PTR(g->gaux, field, techi)[0], maxb1 = 0, maxti = 0;
    int v;
    if (b0 & 0x80) {
        return 0;
    }
    for (int i = 0; i < es->tech.completed[field]; ++i) {
        const uint8_t *p;
        uint8_t ti;
        ti = srds->researchcompleted[field][i];
        p = RESEARCH_D0_PTR(g->gaux, field, ti);
        if ((p[0] == b0) && (p[1] > maxb1)) {
             maxb1 = p[1];
             maxti = ti;
        }
    }
#if 0
    /* BUG v is uninitialized and overwritten anyway */
    /*62e63*/
    switch (field) {
        case TECH_FIELD_COMPUTER:
            if (es->trait2 == TRAIT2_INDUSTRIALIST) {
                v += v / 6;
            }
            break;
        case TECH_FIELD_CONSTRUCTION:
            if (es->trait2 == TRAIT2_INDUSTRIALIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_FORCE_FIELD:
            if (es->trait2 == TRAIT2_MILITARIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_PLANETOLOGY:
            if (es->trait2 == TRAIT2_ECOLOGIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_PROPULSION:
            if (es->trait2 == TRAIT2_EXPANSIONIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_WEAPON:
            if (es->trait2 == TRAIT2_MILITARIST) {
                v += v / 4;
            }
            break;
        default:
            break;
    }
#endif
    /*62f24*/
    v = techi * techi;
    if (b0 == 0) {
        v *= 10;
    }
    if (b0 == 3) {
        v *= 6;
    }
    if (b0 == 5) {
        v *= 3;
    }
    if (b0 == 14) {
        v *= 3;
    }
    if (maxti > techi) {
        v /= 4;
    }
    if (maxti == techi) {
        v = 0;
    }
    if ((es->race == RACE_SILICOID) && ((b0 == 5) || (b0 == 13) || (b0 == 14))) {
        v = 0;
    }
    if ((field == TECH_FIELD_WEAPON) && (techi == TECH_WEAP_DEATH_RAY)) {
        v = 30000;
    }
    return v;
}

static player_id_t game_spy_frame_random(struct game_s *g, player_id_t spy, player_id_t target)
{
    const empiretechorbit_t *et = &(g->eto[target]);
    player_id_t tbl_scapegoat[PLAYER_NUM];
    int n = 0;
    for (player_id_t i = 0; i < PLAYER_NUM; ++i) {
        if ((i != spy) && (i != target) && BOOLVEC_IS1(et->contact, i)) {
            tbl_scapegoat[n++] = i;
        }
    }
    return (n == 0) ? PLAYER_NONE : tbl_scapegoat[rnd_0_nm1(n, &g->seed)];
}

static void game_spy_espionage(struct game_s *g, player_id_t spy, player_id_t target, bool flag_frame, int spies, bool flag_any_caught, struct spy_turn_s *st)
{
    struct spy_esp_s s[1];
    empiretechorbit_t *et = &(g->eto[target]);
    int spied = spies, rmax = 0, tmax = 0;
    for (int i = 0; i < spies; ++i) {
        int r;
        r = rnd_1_n(100, &g->seed);
        SETMAX(rmax, r);
    }
    for (int i = 0; i < TECH_FIELD_NUM; ++i) {
        SETMAX(tmax, et->tech.percent[i]);
    }
    rmax = (rmax * tmax) / 100;
    if (spies > 0) {
        s->target = target;
        s->spy = spy;
        game_spy_esp_sub1(g, s, 0, 0);
        /*81f0a*/
        game_spy_esp_sub5(s, rmax);
        SETMIN(spied, s->tnum);
        if (s->tnum > 0) {
            g->evn.stolen_spy[target][spy] = flag_any_caught ? spy : PLAYER_NONE;
            if (IS_HUMAN(g, spy)) {
                /*81fd3*/
                int v = 0;
                st->tbl_rmax[target][spy] = rmax;
                g->evn.spied_num[target][spy] = spied;
                g->evn.spied_spy[target][spy] = flag_frame ? -1 : spy;
                for (int i = 0; i < 5; ++i) {
                    v += rnd_1_n(12, &g->seed);
                }
                if ((!flag_frame) && flag_any_caught) {
                    g->evn.spied_spy[target][spy] = v;
                }
            } else if (IS_HUMAN(g, target)) {
                /*81f35*/
                g->evn.stolen_field[target][spy] = s->tbl_field[0];
                g->evn.stolen_tech[target][spy] = s->tbl_tech2[0];
                if (flag_frame) {
                    g->evn.stolen_spy[target][spy] = game_spy_frame_random(g, spy, target);
                }
                /*81fa7*/
                game_tech_get_new(g, spy, s->tbl_field[0], s->tbl_tech2[0], TECHSOURCE_AI_SPY, 0, PLAYER_NONE, false);
            } else {
                /*8207f*/
                game_tech_get_new(g, spy, s->tbl_field[0], s->tbl_tech2[0], TECHSOURCE_AI_SPY, 0, PLAYER_NONE, false);
                if (flag_frame && (rnd_0_nm1(2, &g->seed) == 0)) {
                    player_id_t scapegoat[PLAYER_NUM];
                    player_id_t pi;
                    int n = 0;
                    for (pi = PLAYER_0; pi < g->players; ++pi) {
                        if ((pi != target) && IS_HUMAN(g, pi) && IS_ALIVE(g, pi) && BOOLVEC_IS1(et->contact, pi)) {
                            scapegoat[n++] = pi;
                        }
                    }
                    if (n > 0) {
                        pi = scapegoat[(n > 1) ? rnd_0_nm1(n, &g->seed) : 0];
                        game_diplo_act(g, -(rnd_1_n(20, &g->seed) + 20), pi, target, 5, 0, s->tbl_field[0]);
                    }
                }
            }
            /*820e6*/
        }
    }
}

static void game_spy_sabotage(struct game_s *g, player_id_t spy, player_id_t target, bool flag_frame, int spies, bool flag_any_caught)
{
    int rcaught, snum = 0, pl;
    bool flag_bases;
    rcaught = flag_any_caught ? (rnd_1_n(20, &g->seed) + 20) : 0;
    {
        int num;
        num = ((g->eto[spy].tech.percent[TECH_FIELD_WEAPON] + 9) * spies) / 10;
        for (int i = 0; i < num; ++i) {
            snum += rnd_1_n(5, &g->seed);
        }
    }
    flag_bases = (rnd_0_nm1(2, &g->seed) != 0);
    pl = game_planet_get_random(g, target); /* WASBUG? used a function that returned 0 on no planets */
    if ((snum > 0) && (pl != PLANET_NONE)) {
        planet_t *p = &(g->planet[pl]);
        g->evn.sabotage_spy[target][spy] = rcaught ? spy : PLAYER_NONE;
        if (IS_HUMAN(g, spy)) {
            g->evn.sabotage_num[target][spy] = snum;
            g->evn.sabotage_spy[target][spy] = flag_frame ? -1 : rcaught;
        } else if (IS_HUMAN(g, target)) {
            if (rnd_0_nm1(4, &g->seed) == 0) {
                g->evn.sabotage_act[target][spy] = SABOTAGE_ACT_REVOLT;
                snum = (p->pop * (snum / 2)) / 100;
                SETMAX(snum, 1);
                SETMIN(snum, p->pop);
                p->rebels += snum;
                SETMIN(p->rebels, p->pop);
                if (p->rebels >= (p->pop / 2)) {
                    p->unrest = PLANET_UNREST_REBELLION;
                    p->unrest_reported = false;
                    p->rebels = p->pop;
                }
            } else {
                if (!flag_bases) {
                    SETMIN(snum, p->factories);
                } else {
                    snum /= 5;
                    SETMIN(snum, p->missile_bases);
                }
                if (snum > 0) {
                    g->evn.sabotage_act[target][spy] = flag_bases ? SABOTAGE_ACT_BASES : SABOTAGE_ACT_FACT;
                    g->evn.sabotage_planet[target][spy] = pl;
                    g->evn.sabotage_num[target][spy] = snum;
                    if (flag_frame) {
                        g->evn.sabotage_spy[target][spy] = game_spy_frame_random(g, spy, target);
                    }
                    if (flag_bases) {
                        p->missile_bases -= snum;
                    } else {
                        p->factories -= snum;
                    }
                }
            }
        } else {
            if (!flag_bases) {
                g->evn.sabotage_act[target][spy] = SABOTAGE_ACT_FACT;
                SUBSAT0(p->factories, snum);
            } else {
                g->evn.sabotage_act[target][spy] = SABOTAGE_ACT_BASES;
                snum /= 5;
                SUBSAT0(p->missile_bases, snum);
            }
            if (!flag_frame) {
                game_diplo_act(g, -rcaught, spy, target, 6, pl, flag_bases);
            } else {
                player_id_t p2 = game_spy_frame_random(g, spy, target);
                if (p2 != PLAYER_NONE) {
                    int r = -(rnd_1_n(16, &g->seed) + rnd_1_n(16, &g->seed));
                    game_diplo_act(g, -r, p2, target, 7, pl, flag_bases);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

int game_spy_esp_sub1(struct game_s *g, struct spy_esp_s *s, int a4, int a6)
{
    s->tnum = 0;
    game_spy_esp_sub2(g, s, a6);
    for (int loops = 0; (loops < 500) && (s->tnum < TECH_SPY_MAX); ++loops) {
        tech_field_t field;
        field = rnd_0_nm1(TECH_FIELD_NUM, &g->seed);
        if (s->tbl_num[field] > 0) {
            int value;
            bool have_tech;
            uint8_t techi;
            techi = s->tbl_techi[field][rnd_0_nm1(s->tbl_num[field], &g->seed)];
            have_tech = false;
            for (int i = 0; i < s->tnum; ++i) {
                /*63495*/
                if ((s->tbl_field[i] == field) && (s->tbl_tech2[i] == techi)) {
                    have_tech = true;
                }
            }
            value = game_spy_esp_get_value(g, s, field, techi);
            if ((value == 0) || (value < a4)) {
                have_tech = true;
            }
            if (!have_tech) {
                int i;
                i = s->tnum;
                s->tbl_field[i] = field;
                s->tbl_tech2[i] = techi;
                s->tbl_value[i] = value;
                s->tnum = i + 1;
            }
        }
    }
    return s->tnum;
}

int game_spy_esp_sub2(struct game_s *g, struct spy_esp_s *s, int a4)
{
    const empiretechorbit_t *es = &(g->eto[s->spy]);
    const empiretechorbit_t *et = &(g->eto[s->target]);
    const shipresearch_t *srds = &(g->srd[s->spy]);
    const shipresearch_t *srdt = &(g->srd[s->target]);
    int sum = 0;
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        s->tbl_num[f] = 0;
    }
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        game_spy_esp_sub3(g, s, f, et->tech.completed[f] - a4, srdt->researchcompleted[f], es->tech.completed[f], srds->researchcompleted[f]);
    }
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        sum += s->tbl_num[f];
    }
    return sum;
}

void game_spy_esp_sub5(struct spy_esp_s *s, int r)
{
    for (int i = 0; i < s->tnum; ++i) {
        if (s->tbl_tech2[i] > r) {
            --s->tnum;
            for (int j = 0; j < s->tnum; ++j) {
                s->tbl_field[j] = s->tbl_field[j + 1];
                s->tbl_tech2[j] = s->tbl_tech2[j + 1];
            }
            --i;
        }
    }
}

void game_spy_build(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        int spycost_base;
        spycost_base = e->tech.percent[TECH_FIELD_COMPUTER] * 2 + 25;
        if (e->race == RACE_DARLOK) {
            spycost_base /= 2;
        }
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if ((i != j) && (e->spying[j] != 0)) {
                int spyfund, spycost;
                spyfund = (e->total_production_bc * e->spying[j]) / 1000 + e->spyfund[j];
                spycost = spycost_base; /* WASBUG MOO1 does not reset spycost between target players */
                while (spyfund >= spycost) {
                    ++e->spies[j];
                    spyfund -= spycost;
                    spycost *= 2;
                }
                e->spyfund[j] = spyfund;
            }
        }
    }
}

void game_spy_report(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if ((i != j) && (e->spies[j] > 0)) {
                uint16_t *ntbl = &(g->eto[j].tech.completed[0]);
                shipresearch_t *srd = &(g->srd[j]);
                e->spyreportyear[j] = g->year;
                for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
                    e->spyreportfield[j][f] = srd->researchcompleted[f][ntbl[f] - 1];
                }
            }
        }
    }
}

void game_spy_turn(struct game_s *g, struct spy_turn_s *st)
{
    bool have_planet[PLAYER_NUM];
    int comptech1[PLAYER_NUM], comptech2[PLAYER_NUM];
    memset(st, 0, sizeof(*st));
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        have_planet[i] = false;
        for (int j = 0; j < g->galaxy_stars; ++j) {
            if (g->planet[j].owner == i) {
                have_planet[i] = true;
                break;
            }
        }
        comptech1[i] = e->tech.percent[TECH_FIELD_COMPUTER];
        comptech2[i] = e->tech.percent[TECH_FIELD_COMPUTER];
        if (e->race == RACE_DARLOK) {
            comptech1[i] += 20;
            comptech2[i] += 30;
        }
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            g->evn.spies_caught[j][i] = 0;
            g->evn.spies_caught[i][j] = 0;
            g->evn.stolen_field[j][i] = 0;
            g->evn.stolen_field[i][j] = 0;
            g->evn.stolen_tech[j][i] = 0;
            g->evn.stolen_tech[i][j] = 0;
            g->evn.stolen_spy[j][i] = 0;
            g->evn.stolen_spy[i][j] = 0;
            g->evn.spied_num[j][i] = 0;
            g->evn.spied_num[i][j] = 0;
            g->evn.spied_spy[j][i] = 0;
            g->evn.spied_spy[i][j] = 0;
            g->evn.sabotage_act[j][i] = SABOTAGE_ACT_NONE;
            g->evn.sabotage_act[i][j] = SABOTAGE_ACT_NONE;
            g->evn.sabotage_num[j][i] = 0;
            g->evn.sabotage_num[i][j] = 0;
            g->evn.sabotage_spy[j][i] = 0;
            g->evn.sabotage_spy[i][j] = 0;
        }
    }
    /*8af1*/
    for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
        empiretechorbit_t *es = &(g->eto[spy]);
        for (player_id_t target = PLAYER_0; target < g->players; ++target) {
            empiretechorbit_t *et = &(g->eto[target]);
            int spies, dt1, dt2, numcaught, numsuccess, numfail;
            bool flag_frame, flag_any_caught;
            if ((spy == target) || (!have_planet[spy]) || (!have_planet[target])) {
                continue;
            }
            spies = es->spies[target];
            dt1 = comptech2[spy] - comptech1[target];
            dt2 = comptech1[target] - comptech2[spy];
            SETMAX(dt1, 0);
            SETMAX(dt2, 0);
            dt2 += et->security / 5;
            if (es->spymode[target] == SPYMODE_HIDE) {
                dt1 += 30;
            }
            numcaught = numsuccess = numfail = 0;
            flag_frame = flag_any_caught = false;
            if (es->spymode[target] == SPYMODE_HIDE) {
                for (int i = 0; i < spies; ++i) {
                    if ((rnd_1_n(100, &g->seed) + dt2) > 85) {
                        ++numcaught;
                    }
                }
                spies = 0;
            } else {
                /*8c30*/
                for (int i = 0; i < spies; ++i) {
                    int r;
                    r = rnd_1_n(100, &g->seed) + dt2;
                    if (r > 99) {
                        numcaught = spies;
                        numfail = spies;
                        flag_any_caught = true;
                        break;
                    } else if (r > 70) {
                        ++numcaught;
                        ++numfail;
                        flag_any_caught = true;
                    } else if (r > 50) {
                        ++numfail;
                        flag_any_caught = true;
                    } else if (r > 50) { /* BUG never true */
                        flag_any_caught = true;
                    }
                }
            }
            /*8c9e*/
            SUBSAT0(spies, numfail);
            SETMIN(numcaught, es->spies[target]);
            es->spies[target] -= numcaught;
            {
                int r;
                r = rnd_1_n(100, &g->seed) + dt1;
                if (r > 84) {
                    numsuccess = spies;
                }
                if (r > 100) {
                    flag_frame = true;
                }
            }
            g->evn.spies_caught[target][spy] = numcaught;
            if (es->spymode[target] == SPYMODE_ESPIONAGE) {
                game_spy_espionage(g, spy, target, flag_frame, numsuccess, flag_any_caught, st);
            } else if (es->spymode[target] == SPYMODE_SABOTAGE) {
                game_spy_sabotage(g, spy, target, flag_frame, numsuccess, flag_any_caught);
            }
        }
    }
}
