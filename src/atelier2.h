#ifndef ATELIER2_H
#define ATELIER2_H
/* Les simulations sont independantes de la blockchain sauvegardee. */
typedef enum { REF=0, ATTACK51=1, SELFISH=2, WITHHOLD=3, SYBIL=4 } Attack;
typedef struct {
    Attack attack;
    int rounds, power, nodes, latency, threshold, protection;
    unsigned seed;
} Config;
typedef struct {
    int success, alerted, detection_round;
    double loss, overhead, orphans, reward;
} Trial;
typedef struct {
    int trials, successes, tp, fp, fn, tn, detected;
    double loss, overhead, orphans, reward, detection_sum;
} Summary;
/* Simulateurs : meme signature, un fichier source par attaque. */
Trial attack_51(const Config *c);
Trial attack_selfish(const Config *c);
Trial attack_withholding(const Config *c);
Trial attack_sybil(const Config *c);
Trial attack_reference(const Config *c);
/* Fonctions communes et menu. */
double sim_rand(void);
int sim_chance(double probability);
void sim_alert(Trial *t, int round);
Summary sim_evaluate(Config cfg, int repetitions);
/* Copie en lecture seule des metadonnees de la blockchain principale.
   Les transactions et les soldes ne sont jamais modifies par les attaques. */
#define SIM_MAX_MINERS 50
typedef struct {
    char name[30];
    int power;
} SimMiner;
typedef struct {
    int accounts, blocks, transactions, pos_validators;
    int miner_count;
    char tip_hash[65];
    SimMiner miners[SIM_MAX_MINERS];
} BlockchainSnapshot;
void atelier2_menu(const BlockchainSnapshot *snapshot);
const char *sim_attack_name(Attack attack);
#endif
