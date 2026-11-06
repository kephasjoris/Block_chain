#include "../atelier2.h"
/* Pool de minage : un participant remet des parts (shares) mais cache
   certains blocs valides. Detection a partir des ecarts shares/blocs. */
Trial attack_withholding(const Config *c) {
    Trial r={0};int shares=0, hidden=0, valid=0;
    for(int turn=1;turn<=c->rounds;turn++) {
        if(sim_chance(c->power/100.0)) {shares++;if(sim_chance(.65))hidden++;else valid++;}
        else valid++;
        if(shares>=5 && hidden*100>=shares*c->threshold) sim_alert(&r,turn);
        if(c->protection) r.overhead+=.25;
    }
    int unblocked=hidden;
    if(c->protection && r.alerted) unblocked=hidden/3; /* exclusion simulee */
    r.success=(shares>0 && unblocked*100>=shares*30);
    r.loss=unblocked;
    r.reward=valid;
    return r;
}
