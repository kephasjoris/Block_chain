#include "../atelier2.h"
/* Identites P2P fictives, sans veritables connexions reseau.
   La defense plafonne le nombre d'identites acceptees par source. */
Trial attack_sybil(const Config *c) {
    Trial r={0};int fake=0;
    for(int turn=1;turn<=c->rounds;turn++) {
        if(sim_chance(c->power/100.0)) fake++;
        if(fake>=c->nodes*c->threshold/100) sim_alert(&r,turn);
        if(c->protection) r.overhead+=.1;
    }
    int accepted=(c->protection && fake>c->nodes/4)?c->nodes/4:fake;
    r.success=(accepted>c->nodes);
    r.loss=r.success?accepted:0;
    return r;
}
