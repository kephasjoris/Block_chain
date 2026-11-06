#include "../atelier2.h"
/* Publication strategique de blocs prives. Le nombre de blocs abandonnes
   sert d'indicateur de detection. La defense refuse les branches privees
   lorsqu'un comportement suspect est constate (modele simplifie). */
Trial attack_selfish(const Config *c) {
    Trial r={0};int hidden=0, honest=0, attacker_rewards=0, orphan=0;
    for(int turn=1;turn<=c->rounds;turn++) {
        if(sim_chance(c->power/100.0)) hidden++;
        else {
            honest++;
            if(hidden>=2){attacker_rewards+=hidden;orphan++;hidden=0;}
            else if(hidden==1){if(sim_chance(0.50+c->latency/400.0)){attacker_rewards++;orphan++;}hidden=0;}
        }
        if(hidden>=3) {attacker_rewards+=hidden;orphan++;hidden=0;}
        if(orphan>=2 || hidden>=3) sim_alert(&r,turn);
        if(c->protection){r.overhead+=0.15;if(r.alerted) {hidden=0;attacker_rewards=(int)(attacker_rewards*0.8);}}
    }
    r.orphans=orphan;
    r.reward=100.0*attacker_rewards/(attacker_rewards+honest+1);
    r.success=(r.reward>c->power+5);
    r.loss=r.success?attacker_rewards:0;
    return r;
}
