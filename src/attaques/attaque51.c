#include "../atelier2.h"
/* Deux branches PoW de meme difficulte : une victoire = un bloc decouvert.
   La double depense est symbolisee par deux transactions depensant UTXO001.
   La defense attend 3 confirmations supplementaires AVANT l'acceptation.
   Ce n'est pas une garantie contre une majorite de puissance adverse. */
Trial attack_51(const Config *c) {
    Trial r={0};
    int honest=0, adversary=0, confirmed=0;
    int needed=c->protection ? 5 : 2;
    for (int turn=1;turn<=c->rounds;turn++) {
        if (sim_chance(c->power/100.0)) adversary++;
        else honest++;
        if (honest>=needed) confirmed=1;
        /* Une reorganisation apres confirmation est notre critere de succes. */
        if (adversary>honest && honest>=2) sim_alert(&r,turn);
        if (confirmed && adversary>honest) {r.success=1;r.loss=10;break;}
        if (c->protection) r.overhead+=0.2;
    }
    r.orphans=(honest<adversary)?honest:adversary;
    return r;
}
