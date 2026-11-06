#include "atelier2.h"
#include <stdlib.h>
#include <string.h>
/* Pseudo-aleatoire : srand(seed) est reinitialise pour chaque scenario.
   Utilise pour la reproductibilite, PAS pour une cryptographie. */
double sim_rand(void){return rand()/((double)RAND_MAX+1);}
int sim_chance(double p){return sim_rand()<p;}
void sim_alert(Trial *t,int round){if(!t->alerted){t->alerted=1;t->detection_round=round;}}
const char *sim_attack_name(Attack a) {
    switch(a){case REF:return "Reference";case ATTACK51:return "51% / Double depense";
    case SELFISH:return "Selfish Mining";case WITHHOLD:return "Block Withholding";
    case SYBIL:return "Sybil Attack";default:return "Inconnu";}
}
Trial attack_reference(const Config *c){
    Trial r={0};
    for(int turn=1;turn<=c->rounds;turn++) {
        if(sim_chance(c->latency/1500.0))r.orphans++;
        if(r.orphans>=2)sim_alert(&r,turn);
        if(c->protection)r.overhead+=.10;
    }
    return r;
}
static Trial execute(const Config *cfg){
    switch(cfg->attack){case ATTACK51:return attack_51(cfg);
    case SELFISH:return attack_selfish(cfg);case WITHHOLD:return attack_withholding(cfg);
    case SYBIL:return attack_sybil(cfg);default:return attack_reference(cfg);}
}
Summary sim_evaluate(Config cfg,int repetitions){
    Summary s={0};s.trials=repetitions;
    srand(cfg.seed);
    for(int i=0;i<repetitions;i++){
        Trial t=execute(&cfg);
        s.successes+=t.success;s.detected+=t.alerted;
        if(cfg.attack==REF){if(t.alerted)s.fp++;else s.tn++;}
        else {if(t.alerted)s.tp++;else s.fn++;}
        s.loss+=t.loss;s.overhead+=t.overhead;s.orphans+=t.orphans;
        s.reward+=t.reward;s.detection_sum+=t.detection_round;
    }
    return s;
}
