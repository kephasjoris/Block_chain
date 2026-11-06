#include "common.h"
#include "src/atelier2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>

typedef struct {
    char name[30];
    double stake;
} Validator;

typedef struct {
    int index;
    char transactions[MAX_TRANSACTIONS][TX_SIZE];
    int tx_count;
    char merkle_root[HASH_HEX];
    char previous_hash[HASH_HEX];
    char hash[HASH_HEX];
    unsigned long long nonce;
    char consensus[4];  /* POW ou POS */
    char validator[30];
} Block;

/* Les anciens validateurs servent seulement a verifier les blocs PoS historiques. */
static const Validator legacy_validators[MAX_VALIDATORS] = INITIAL_VALIDATORS;

/* Le registre vivant est le meme pour les comptes, mises PoS et mineurs PoW. */
#define MAX_ACCOUNTS 50
#define MAX_PENDING MAX_TRANSACTIONS
#define ACCOUNTS_FILE "comptes.dat"
#define CHAIN_FILE "chain.dat"
typedef struct {
    char name[30];
    int initial_balance;
    int stake;          /* Unites bloquees pour PoS */
    int mining_power;   /* Poids pedagogique pour PoW, 0 = non mineur */
} Account;
static Account accounts[MAX_ACCOUNTS];
static int account_count = 0;
static char pending[MAX_PENDING][TX_SIZE];
static int pending_count = 0;


/* ---------- SHA-256 ---------- */

void sha256(const char *data, char output[HASH_HEX]) {
    unsigned char digest[HASH_SIZE];
    const char hex[] = "0123456789abcdef";

    SHA256((const unsigned char *)data, strlen(data), digest);

    for (int i = 0; i < HASH_SIZE; i++) {
        output[2 * i] = hex[digest[i] >> 4];
        output[2 * i + 1] = hex[digest[i] & 15];
    }

    output[64] = '\0';
}

/* ---------- Arbre de Merkle ---------- */

int compute_merkle(Block *b, char root[HASH_HEX]) {
    if (b->tx_count < 0 || b->tx_count > MAX_TRANSACTIONS)
        return 0;

    char hashes[MAX_TRANSACTIONS][HASH_HEX];

    if (b->tx_count == 0) {
        sha256("", root);
        return 1;
    }

    for (int i = 0; i < b->tx_count; i++)
        sha256(b->transactions[i], hashes[i]);

    int count = b->tx_count;

    while (count > 1) {
        int next_count = 0;

        for (int i = 0; i < count; i += 2) {
            int right = (i + 1 < count) ? i + 1 : i;
            char combined[HASH_HEX * 2];
            char result[HASH_HEX];

            memcpy(combined, hashes[i], HASH_HEX - 1);
            memcpy(combined + HASH_HEX - 1, hashes[right], HASH_HEX - 1);
            combined[2 * (HASH_HEX - 1)] = '\0';

            sha256(combined, result);
            strcpy(hashes[next_count++], result);
        }

        count = next_count;
    }

    strcpy(root, hashes[0]);
    return 1;
}

/* ---------- Hash du bloc ---------- */

void calculate_hash(Block *b, char output[HASH_HEX]) {
    char input[1024];

    snprintf(input, sizeof(input), "%d|%s|%s|%s|%s|%llu",
             b->index, b->previous_hash, b->merkle_root,
             b->consensus, b->validator, b->nonce);

    sha256(input, output);
}

int valid_hash(const char *hash, int difficulty) {
    if (difficulty < 0 || difficulty > 64)
        return 0;

    for (int i = 0; i < difficulty; i++)
        if (hash[i] != '0')
            return 0;

    return 1;
}

int select_miner(void); /* fonction definie plus bas */

/* ---------- Proof of Work ---------- */

double mine_pow(Block *b) {
    clock_t start = clock();
    char root[HASH_HEX];

    strcpy(b->consensus, "POW");
    int miner = select_miner();
    if (miner < 0) return -1.0;
    strcpy(b->validator, accounts[miner].name);

    if (!compute_merkle(b, root))
        return -1.0;

    strcpy(b->merkle_root, root);
    b->nonce = 0;

    do {
        calculate_hash(b, b->hash);

        if (valid_hash(b->hash, DIFFICULTY))
            break;

        b->nonce++;
    } while (1);

    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

/* ---------- Proof of Stake ---------- */

int select_validator(void) {
    unsigned long long total = 0;
    for (int i = 0; i < account_count; i++)
        if (accounts[i].stake > 0) total += (unsigned)accounts[i].stake;
    if (!total) return -1;
    /* Tirage pondere, pedagogique (rand n'est pas une source securisee). */
    double draw = ((double)rand() / ((double)RAND_MAX + 1.0)) * (double)total;
    double cumul = 0;
    for (int i = 0; i < account_count; i++) {
        cumul += accounts[i].stake;
        if (accounts[i].stake > 0 && draw < cumul) return i;
    }
    return -1;
}

int select_miner(void) {
    unsigned long long total = 0;
    for (int i = 0; i < account_count; i++)
        total += (unsigned)accounts[i].mining_power;
    if (!total) return -1;
    double draw = ((double)rand() / ((double)RAND_MAX + 1.0)) * (double)total;
    double cumul = 0;
    for (int i = 0; i < account_count; i++) {
        cumul += accounts[i].mining_power;
        if (accounts[i].mining_power > 0 && draw < cumul) return i;
    }
    return -1;
}

double validate_pos(Block *b) {
    clock_t start = clock();
    char root[HASH_HEX];

    int selected = select_validator();

    if (selected < 0)
        return -1.0;

    strcpy(b->consensus, "POS");
    strcpy(b->validator, accounts[selected].name);

    if (!compute_merkle(b, root))
        return -1.0;

    strcpy(b->merkle_root, root);
    b->nonce = 0;

    /* En PoS, pas de recherche de nonce par minage. */
    calculate_hash(b, b->hash);

    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

/* ---------- Verification d'un bloc ---------- */

int verify_block(Block *b) {
    char root[HASH_HEX];
    char calculated[HASH_HEX];

    if (!compute_merkle(b, root))
        return 0;

    if (strcmp(root, b->merkle_root) != 0)
        return 0;

    calculate_hash(b, calculated);

    if (strcmp(calculated, b->hash) != 0)
        return 0;

    if (strcmp(b->consensus, "POW") == 0)
        return valid_hash(b->hash, DIFFICULTY);

    if (strcmp(b->consensus, "POS") == 0) {
        /* Un bloc historique doit rester verifiable apres changement de stake.
           ATTENTION : cette verification ne prouve pas l'election PoS ancienne. */
        for (int i = 0; i < account_count; i++)
            if (!strcmp(b->validator, accounts[i].name)) return 1;
        for (int i = 0; i < MAX_VALIDATORS; i++)
            if (!strcmp(b->validator, legacy_validators[i].name)) return 1;
        return 0;
    }

    return 0;
}

/* ---------- Verification de la chaine ---------- */

int verify_chain(Block chain[], int count) {
    char zero_hash[HASH_HEX];
    memset(zero_hash, '0', 64);
    zero_hash[64] = '\0';

    for (int i = 0; i < count; i++) {
        if (!verify_block(&chain[i]))
            return 0;

        if (chain[i].index != i)
            return 0;

        if (i == 0) {
            if (strcmp(chain[i].previous_hash, zero_hash) != 0)
                return 0;
        } else {
            if (strcmp(chain[i].previous_hash,
                       chain[i - 1].hash) != 0)
                return 0;
        }
    }

    return 1;
}

/* ---------- Affichage ---------- */

void print_block(Block *b, double elapsed) {
    printf("\nBloc %d | Consensus : %s\n",
           b->index, b->consensus);

    for (int i = 0; i < b->tx_count; i++)
        printf("Transaction %d : %s\n",
               i + 1, b->transactions[i]);

    printf("Racine Merkle : %s\n", b->merkle_root);
    printf("Hash precedent: %s\n", b->previous_hash);
    printf("Hash du bloc  : %s\n", b->hash);
    printf("Nonce         : %llu\n", b->nonce);

    if (strcmp(b->consensus, "POS") == 0)
        printf("Validateur    : %s\n", b->validator);

    printf("Temps         : %.6f s\n", elapsed);
    printf("Bloc valide   : %s\n",
           verify_block(b) ? "OUI" : "NON");
}

/* ---------- Comptes, transactions en attente et sauvegarde ---------- */

int read_int(const char *prompt) {
    char line[64]; int n; char extra;
    printf("%s", prompt);
    if (!fgets(line, sizeof(line), stdin) || sscanf(line, " %d %c", &n, &extra) != 1) return -1;
    return n;
}

void read_line(const char *prompt, char *dest, size_t capacity) {
    printf("%s", prompt);
    if (!fgets(dest, (int)capacity, stdin)) { dest[0] = '\0'; return; }
    size_t n = strcspn(dest, "\n");
    if (dest[n] == '\n') dest[n] = '\0';
    else { int ch; while ((ch = getchar()) != '\n' && ch != EOF) {} }
}

int find_account(const char *name) {
    for (int i = 0; i < account_count; i++)
        if (!strcmp(accounts[i].name, name)) return i;
    return -1;
}

/* Format : emetteur->destinataire:montant.
   SYSTEM est une source fictive de credits pour les RECHARGES pedagogiques :
   ce n'est pas un compte et cela cree de nouvelles unites, apres validation d'un bloc.
   Le format des anciennes transactions reste compatible avec chain.dat / pending.dat. */
int parse_transaction(const char *line, int *from, int *to, int *amount) {
    char a[30], b[30], canonical[TX_SIZE]; int value, n = 0;
    if (sscanf(line, "%29[A-Za-z]->%29[A-Za-z]:%d%n", a, b, &value, &n) != 3 ||
        line[n] != '\0' || value <= 0 || !strcmp(a, b)) return 0;
    snprintf(canonical, sizeof(canonical), "%s->%s:%d", a, b, value);
    if (strcmp(canonical, line)) return 0;
    *from = !strcmp(a, "SYSTEM") ? -1 : find_account(a);
    *to = find_account(b); *amount = value;
    return (*from >= 0 || !strcmp(a, "SYSTEM")) && *to >= 0 &&
           strcmp(b, "SYSTEM") != 0;
}

/* Soldes obtenus en rejouant les blocs puis, au besoin, les transactions en attente. */
int calculate_balances(Block chain[], int count, int include_pending, int balances[]) {
    for (int i = 0; i < account_count; i++) balances[i] = accounts[i].initial_balance;
    for (int i = 0; i < count + (include_pending ? 1 : 0); i++) {
        int n = i == count ? pending_count : chain[i].tx_count;
        for (int j = 0; j < n; j++) {
            const char *tx = i == count ? pending[j] : chain[i].transactions[j];
            int from, to, amount;
            if (!parse_transaction(tx, &from, &to, &amount) ||
                (from >= 0 && balances[from] - accounts[from].stake < amount) ||
                balances[to] > 1000000000 - amount) return 0;
            if (from >= 0) balances[from] -= amount;
            balances[to] += amount;
        }
    }
    /* Les stakes restent dans le patrimoine, mais indisponibles au transfert. */
    for (int i = 0; i < account_count; i++)
        if (balances[i] < accounts[i].stake) return 0;
    return 1;
}

/* ACC2 : meme fichier que les anciens comptes ACC1, enrichi des roles.
   Le chargement ACC1 migre automatiquement EN MEMOIRE ; la prochaine
   sauvegarde ecrit ACC2. Conserver une copie de comptes.dat avant migration. */
typedef struct { char name[30]; int initial_balance; } LegacyAccount;
int save_accounts(void) {
    FILE *f = fopen(ACCOUNTS_FILE, "wb"); if (!f) return 0;
    int ok = fwrite("ACC2", 1, 4, f) == 4 &&
        fwrite(&account_count, sizeof(account_count), 1, f) == 1 &&
        fwrite(accounts, sizeof(Account), account_count, f) == (size_t)account_count;
    return fclose(f) == 0 && ok;
}
int load_accounts(void) {
    FILE *f = fopen(ACCOUNTS_FILE, "rb"); if (!f) return 1;
    char magic[4]; int n = 0;
    int ok = fread(magic, 1, 4, f) == 4 &&
        (!memcmp(magic, "ACC1", 4) || !memcmp(magic, "ACC2", 4)) &&
        fread(&n, sizeof(n), 1, f) == 1 && n >= 0 && n <= MAX_ACCOUNTS;
    int old = ok && !memcmp(magic, "ACC1", 4);
    if (ok) {
        if (old) {
            LegacyAccount old_accounts[MAX_ACCOUNTS];
            ok = fread(old_accounts, sizeof(LegacyAccount), n, f) == (size_t)n;
            if (ok) for (int i = 0; i < n; i++) {
                memset(&accounts[i], 0, sizeof(Account));
                memcpy(accounts[i].name, old_accounts[i].name, sizeof(old_accounts[i].name));
                accounts[i].initial_balance = old_accounts[i].initial_balance;
            }
        } else ok = fread(accounts, sizeof(Account), n, f) == (size_t)n;
    }
    if (ok && fgetc(f) != EOF) ok = 0;
    fclose(f);
    if (ok) for (int i = 0; i < n; i++) {
        if (!memchr(accounts[i].name, 0, sizeof(accounts[i].name)) ||
            accounts[i].initial_balance < 0 ||
            accounts[i].stake < 0 || accounts[i].mining_power < 0) ok = 0;
        for (int j = 0; j < i; j++)
            if (!strcmp(accounts[i].name, accounts[j].name)) ok = 0;
    }
    if (ok) account_count = n;
    return ok;
}

int save_chain(Block chain[], int count) {
    FILE *f = fopen(CHAIN_FILE, "wb"); if (!f) return 0;
    int ok = fwrite("BLC1", 1, 4, f) == 4 &&
        fwrite(&count, sizeof(count), 1, f) == 1 &&
        fwrite(chain, sizeof(Block), count, f) == (size_t)count;
    return fclose(f) == 0 && ok;
}

int load_chain(Block chain[], int *count) {
    FILE *f = fopen(CHAIN_FILE, "rb"); if (!f) return 1;
    char magic[4]; int n;
    int ok = fread(magic, 1, 4, f) == 4 && !memcmp(magic, "BLC1", 4) &&
        fread(&n, sizeof(n), 1, f) == 1 && n >= 0 && n <= MAX_BLOCKS;
    if (ok) ok = fread(chain, sizeof(Block), n, f) == (size_t)n;
    if (fgetc(f) != EOF) ok = 0;
    fclose(f);
    if (ok) {
        for (int i = 0; i < n; i++) {
            Block *b = &chain[i];
            if (b->tx_count < 1 || b->tx_count > MAX_TRANSACTIONS ||
                !memchr(b->consensus, 0, sizeof(b->consensus)) ||
                !memchr(b->validator, 0, sizeof(b->validator)) ||
                !memchr(b->hash, 0, sizeof(b->hash)) ||
                !memchr(b->previous_hash, 0, sizeof(b->previous_hash)) ||
                !memchr(b->merkle_root, 0, sizeof(b->merkle_root))) ok = 0;
            if (b->tx_count >= 1 && b->tx_count <= MAX_TRANSACTIONS)
                for (int j = 0; j < b->tx_count; j++)
                    if (!memchr(b->transactions[j], 0, TX_SIZE)) ok = 0;
        }
        if (ok && !verify_chain(chain, n)) ok = 0;
        int balances[MAX_ACCOUNTS];
        if (ok && !calculate_balances(chain, n, 0, balances)) ok = 0;
    }
    if (ok) *count = n;
    return ok;
}

int save_pending(void) {
    FILE *f = fopen("pending.dat", "wb"); if (!f) return 0;
    int ok = fwrite("PND1", 1, 4, f) == 4 &&
        fwrite(&pending_count, sizeof(pending_count), 1, f) == 1 &&
        fwrite(pending, TX_SIZE, pending_count, f) == (size_t)pending_count;
    return fclose(f) == 0 && ok;
}

int load_pending(Block chain[], int count) {
    FILE *f = fopen("pending.dat", "rb"); if (!f) return 1;
    char magic[4]; int n;
    int ok = fread(magic, 1, 4, f) == 4 && !memcmp(magic, "PND1", 4) &&
        fread(&n, sizeof(n), 1, f) == 1 && n >= 0 && n <= MAX_PENDING;
    if (ok) ok = fread(pending, TX_SIZE, n, f) == (size_t)n;
    if (fgetc(f) != EOF) ok = 0;
    fclose(f);
    if (ok) for (int i = 0; i < n; i++)
        if (!memchr(pending[i], 0, TX_SIZE)) ok = 0;
    if (ok) {
        pending_count = n;
        int balances[MAX_ACCOUNTS];
        ok = calculate_balances(chain, count, 1, balances);
    }
    return ok;
}

void show_chain(Block chain[], int count) {
    printf("\n================ BLOCKCHAIN ================\n");
    if (!count) printf("Aucun bloc valide pour le moment.\n");
    for (int i = 0; i < count; i++) {
        Block *b = &chain[i];
        printf("+--------------------------------------------+\n");
        printf("| BLOC %-3d  | %-3s | %-8s             |\n", b->index, b->consensus,
               verify_block(b) ? "OK" : "ALTERE");
        printf("+--------------------------------------------+\n");
        for (int j = 0; j < b->tx_count; j++) printf("  %s\n", b->transactions[j]);
        printf("  Hash      : %.16s...\n", b->hash);
        printf("  Precedent : %.16s...\n", b->previous_hash);
        if (!strcmp(b->consensus, "POS")) printf("  Validateur: %s\n", b->validator);
        if (i + 1 < count) printf("                     |\n                     v\n");
    }
    printf("============================================\n");
    printf("Integrite : %s\n", verify_chain(chain, count) ? "VALIDE" : "INVALIDE");
}


/* ---------- Laboratoire d'integrite : travail sur une COPIE uniquement ---------- */
/*
 * Une falsification modifie une transaction mais NE recalcule NI Merkle NI hash.
 * La copie devient donc invalide sans toucher chain.dat, pending.dat et comptes.dat.
 * Restaurer copie les blocs authentiques depuis la chaine originale.
 */
static void integrity_lab(const Block original[], int count) {
    if (count == 0) {
        puts("[INFO] Creez d'abord au moins un bloc valide.");
        return;
    }
    if (!verify_chain((Block *)original, count)) {
        puts("[REFUS] Chaine originale invalide : laboratoire indisponible.");
        return;
    }
    Block *demo = malloc((size_t)count * sizeof *demo);
    if (!demo) { puts("[ERREUR] Memoire insuffisante."); return; }
    memcpy(demo, original, (size_t)count * sizeof *demo);
    for (;;) {
        puts("\n====== LABORATOIRE D'INTEGRITE (COPIE RAM) ======");
        puts("1. Modifier une transaction de la copie");
        puts("2. Verifier la copie et localiser les erreurs");
        puts("3. Afficher la copie de la blockchain");
        puts("4. Restaurer les donnees originales de la copie");
        puts("0. Retour sans aucune modification des fichiers .dat");
        int choice = read_int("Votre choix : ");
        if (choice == 0) break;
        if (choice == 1) {
            for (int i = 0; i < count; i++)
                printf("Bloc %d : %d transaction(s)\n", i, demo[i].tx_count);
            int block_index = read_int("Numero du bloc : ");
            if (block_index < 0 || block_index >= count) {
                puts("[REFUS] Bloc inexistant."); continue;
            }
            for (int j = 0; j < demo[block_index].tx_count; j++)
                printf("  %d. %s\n", j + 1, demo[block_index].transactions[j]);
            int tx_number = read_int("Numero de la transaction (a partir de 1) : ");
            if (tx_number < 1 || tx_number > demo[block_index].tx_count) {
                puts("[REFUS] Transaction inexistante."); continue;
            }
            char replacement[TX_SIZE];
            read_line("Nouvelle valeur (ex: Joris->Demba:40) : ",
                      replacement, sizeof replacement);
            if (!replacement[0]) { puts("[REFUS] Saisie vide."); continue; }
            strcpy(demo[block_index].transactions[tx_number - 1], replacement);
            printf("[TEST] Bloc %d modifie uniquement dans la copie.\n", block_index);
            puts("[INFO] Racine Merkle et hash volontairement NON recalcules.");
            printf("[RESULTAT] Integrite copie : %s\n",
                   verify_chain(demo, count) ? "VALIDE" : "INVALIDE");
        } else if (choice == 2) {
            for (int i = 0; i < count; i++) {
                int block_ok = verify_block(&demo[i]);
                int link_ok = (i == 0) ?
                    strcmp(demo[i].previous_hash, original[i].previous_hash) == 0 :
                    strcmp(demo[i].previous_hash, demo[i-1].hash) == 0;
                printf("Bloc %-3d | hash/Merkle : %-8s | chainage : %s\n",
                       i, block_ok ? "OK" : "INVALIDE",
                       link_ok ? "OK" : "INVALIDE");
            }
            printf("[RESULTAT] Copie : %s | Original : %s\n",
                   verify_chain(demo, count) ? "VALIDE" : "INVALIDE",
                   verify_chain((Block *)original, count) ? "VALIDE" : "INVALIDE");
        } else if (choice == 3) {
            show_chain(demo, count);
        } else if (choice == 4) {
            memcpy(demo, original, (size_t)count * sizeof *demo);
            printf("[OK] Copie restauree depuis les blocs originaux : %s\n",
                   verify_chain(demo, count) ? "VALIDE" : "INVALIDE");
            puts("[INFO] Aucun minage, aucune nouvelle validation, aucune sauvegarde.");
        } else {
            puts("Choix inconnu.");
        }
    }
    free(demo);
    puts("[OK] Laboratoire ferme. La blockchain enregistree est inchangee.");
}

/* Gestion simplifiee des participants. Les roles sont dans Account. */
void participants_menu(Block chain[], int count) {
    for (;;) {
        puts("\n====== PARTICIPANTS PoS / PoW ======");
        puts("1. Voir les comptes, mises et puissances");
        puts("2. Bloquer une mise PoS");
        puts("3. Retirer une mise PoS");
        puts("4. Definir la puissance de minage PoW");
        puts("0. Retour");
        int choice = read_int("Votre choix : ");
        if (!choice) return;
        int balances[MAX_ACCOUNTS];
        if (!calculate_balances(chain, count, 1, balances)) {
            puts("[ERREUR] Etat des comptes incoherent."); return;
        }
        if (choice == 1) {
            puts("\nNom                 Solde total    Mise PoS   Disponible   PoW");
            for (int i = 0; i < account_count; i++)
                printf("%-20s %11d %11d %12d %5d\n", accounts[i].name,
                       balances[i], accounts[i].stake, balances[i]-accounts[i].stake,
                       accounts[i].mining_power);
            continue;
        }
        if (choice < 2 || choice > 4) { puts("Choix invalide."); continue; }
        char name[30]; read_line("Compte concerne : ", name, sizeof(name));
        int i = find_account(name);
        if (i < 0) { puts("Compte inexistant : le creer d'abord."); continue; }
        int amount;
        if (choice == 2) {
            amount = read_int("Mise supplementaire (>0) : ");
            if (amount <= 0 || amount > balances[i]-accounts[i].stake ||
                accounts[i].stake > 1000000000 - amount) {
                puts("Mise invalide ou fonds insuffisants."); continue;
            }
            accounts[i].stake += amount;
        } else if (choice == 3) {
            amount = read_int("Mise a retirer (>0) : ");
            if (amount <= 0 || amount > accounts[i].stake) {
                puts("Mise invalide."); continue;
            }
            /* Retrait immediat : simplification pedagogique. */
            accounts[i].stake -= amount;
        } else {
            amount = read_int("Puissance relative PoW (0-1000) : ");
            if (amount < 0 || amount > 1000) { puts("Puissance invalide."); continue; }
            accounts[i].mining_power = amount;
        }
        if (!save_accounts()) puts("[ERREUR] Sauvegarde impossible.");
        else puts("[OK] Role mis a jour dans comptes.dat (ACC2).");
    }
}

/* ---------- Atelier 2 : simulation pedagogique d'une attaque 51 % ---------- */
/* Les deux branches sont privees a la simulation : aucun fichier .dat n'est modifie. */
/* Fournit uniquement une photographie en lecture seule au laboratoire. */
static BlockchainSnapshot make_snapshot(const Block chain[], int count) {
    BlockchainSnapshot s = {0};
    s.accounts = account_count;
    s.blocks = count;
    if (count) strcpy(s.tip_hash, chain[count - 1].hash);
    else strcpy(s.tip_hash, "(chaine vide)");
    for (int i = 0; i < count; i++) s.transactions += chain[i].tx_count;
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].stake > 0) s.pos_validators++;
        if (accounts[i].mining_power > 0 && s.miner_count < SIM_MAX_MINERS) {
            SimMiner *m = &s.miners[s.miner_count++];
            strcpy(m->name, accounts[i].name);
            m->power = accounts[i].mining_power;
        }
    }
    return s;
}

int main(void) {
    Block chain[MAX_BLOCKS] = {0}; int count = 0;
    srand((unsigned)time(NULL));
    if (!load_accounts() || !load_chain(chain, &count) || !load_pending(chain, count)) {
        printf("[ERREUR] Donnees sauvegardees invalides. Rien n'a ete modifie.\n");
        return 1;
    }
    printf("[OK] %d comptes, %d blocs et %d transactions en attente charges.\n", account_count, count, pending_count);
    /* Navigation par rubriques : les operations existantes gardent leur code.
       0 dans une rubrique revient au menu principal ; 0 a l'accueil quitte. */
    int section = 0;
    for (;;) {
        int choice = -1;
        if (section == 0) {
            puts("\n+=============================================+");
            puts("|          BLOCKCHAIN - MENU PRINCIPAL        |");
            puts("+=============================================+");
            puts("| 1. Comptes et soldes                        |");
            puts("| 2. Transactions                             |");
            puts("| 3. Blocs et integrite                       |");
            puts("| 4. Mineurs PoW et validateurs PoS           |");
            puts("| 5. Atelier 2 - Simulations d'attaques       |");
            puts("| 0. Quitter                                  |");
            puts("+=============================================+");
            int selected = read_int("Votre choix : ");
            if (selected == 0) break;
            if (selected < 1 || selected > 5) { puts("Choix inconnu."); continue; }
            section = selected;
            continue;
        }
        if (section == 1) {
            puts("\n====== COMPTES ET SOLDES ======");
            puts("1. Creer un compte");
            puts("2. Afficher les soldes");
            puts("3. Recharger un compte (credits fictifs)");
            puts("0. Retour au menu principal");
        } else if (section == 2) {
            puts("\n====== TRANSACTIONS ======");
            puts("1. Ajouter un transfert en attente");
            puts("2. Voir les transactions en attente");
            puts("0. Retour au menu principal");
        } else if (section == 3) {
            puts("\n====== BLOCS ET INTEGRITE ======");
            puts("1. Creer et valider un bloc (PoW/PoS)");
            puts("2. Afficher la blockchain");
            puts("3. Verifier l'integrite");
            puts("4. Tester falsification et restauration (copie)");
            puts("0. Retour au menu principal");
        } else if (section == 4) {
            puts("\n====== PARTICIPANTS ======");
            puts("1. Gerer les mineurs et validateurs");
            puts("0. Retour au menu principal");
        } else {
            puts("\n====== ATELIER 2 ======");
            puts("1. Simuler et comparer les attaques");
            puts("0. Retour au menu principal");
        }
        int selected = read_int("Votre choix : ");
        if (selected == 0) { section = 0; continue; }
        /* Traduction rubrique -> operation historique, sans dupliquer la logique. */
        if (section == 1) {
            const int actions[] = {0, 1, 2, 10};
            if (selected >= 1 && selected <= 3) choice = actions[selected];
        } else if (section == 2) {
            const int actions[] = {0, 3, 7};
            if (selected >= 1 && selected <= 2) choice = actions[selected];
        } else if (section == 3) {
            const int actions[] = {0, 4, 5, 6, 11};
            if (selected >= 1 && selected <= 4) choice = actions[selected];
        } else if (section == 4 && selected == 1) choice = 9;
        else if (section == 5 && selected == 1) choice = 8;
        if (choice < 0) { puts("Choix inconnu."); continue; }
        if (choice == 1) {
            if (account_count >= MAX_ACCOUNTS) { puts("Limite de comptes atteinte."); continue; }
            char name[30]; read_line("Nom (lettres uniquement) : ", name, sizeof(name));
            int valid = name[0] != '\0';
            for (int i = 0; name[i]; i++)
                if (!((name[i] >= 'A' && name[i] <= 'Z') ||
                      (name[i] >= 'a' && name[i] <= 'z'))) valid = 0;
            if (!valid || !strcmp(name, "SYSTEM") || find_account(name) >= 0) { puts("Nom invalide ou existant."); continue; }
            int amount = read_int("Solde initial fictif (0 a 1000000) : ");
            if (amount < 0 || amount > 1000000) { puts("Montant invalide."); continue; }
            Account *a = &accounts[account_count++];
            memset(a, 0, sizeof(*a)); strcpy(a->name, name); a->initial_balance = amount;
            if (!save_accounts()) { account_count--; puts("Erreur de sauvegarde."); continue; }
            printf("[OK] Compte %s cree.\n", name);
        } else if (choice == 2) {
            int balances[MAX_ACCOUNTS];
            if (!calculate_balances(chain, count, 0, balances)) { puts("Donnees invalides."); continue; }
            printf("\n+----------------------+------------+------------+------------+\n");
            printf("| Compte               | Solde total| Mise bloq. | Disponible |\n");
            printf("+----------------------+------------+------------+------------+\n");
            for (int i = 0; i < account_count; i++)
                printf("| %-20s | %10d | %10d | %10d |\n", accounts[i].name,
                       balances[i], accounts[i].stake, balances[i]-accounts[i].stake);
            puts("+----------------------+------------+------------+------------+");
        } else if (choice == 3) {
            if (pending_count >= MAX_PENDING) { puts("File d'attente pleine."); continue; }
            /* Les trois champs sont demandes separement pour eviter les fautes de syntaxe. */
            char sender[30], receiver[30], line[TX_SIZE];
            read_line("Expediteur (compte) : ", sender, sizeof(sender));
            read_line("Destinataire (compte) : ", receiver, sizeof(receiver));
            int requested = read_int("Montant a envoyer (>0) : ");
            if (requested <= 0 || requested > 1000000 || !strcmp(sender, "SYSTEM")) {
                puts("[REFUS] Expediteur ou montant invalide."); continue;
            }
            snprintf(line, sizeof(line), "%s->%s:%d", sender, receiver, requested);
            int from, to, amount, balances[MAX_ACCOUNTS];
            if (!parse_transaction(line, &from, &to, &amount) || from < 0 ||
                !calculate_balances(chain, count, 1, balances) ||
                balances[from] - accounts[from].stake < amount ||
                balances[to] > 1000000000 - amount) {
                puts("[REFUS] Comptes ou solde disponible insuffisant."); continue;
            }
            strcpy(pending[pending_count++], line);
            if (!save_pending()) { pending_count--; puts("Erreur sauvegarde attente."); continue; }
            puts("[OK] Transfert en attente. Les soldes changeront apres validation d'un bloc.");
        } else if (choice == 4) {
            if (!pending_count) { puts("Aucune transaction en attente."); continue; }
            if (count >= MAX_BLOCKS) { puts("Limite de blocs atteinte."); continue; }
            if (!verify_chain(chain, count)) { puts("Chaine invalide."); continue; }
            int method = read_int("Consensus (1=PoW, 2=PoS) : ");
            if (method != 1 && method != 2) { puts("Choix invalide."); continue; }
            if (method == 1 && select_miner() < 0) {
                puts("[REFUS] Aucun mineur PoW actif. Participants > Definir la puissance de minage PoW.");
                continue;
            }
            if (method == 2 && select_validator() < 0) {
                puts("[REFUS] Aucun validateur PoS actif. Participants > Bloquer une mise PoS.");
                continue;
            }
            Block b = {0}; b.index = count; b.tx_count = pending_count;
            for (int i = 0; i < pending_count; i++) strcpy(b.transactions[i], pending[i]);
            if (count) strcpy(b.previous_hash, chain[count - 1].hash);
            else { memset(b.previous_hash, '0', 64); b.previous_hash[64] = '\0'; }
            double elapsed = method == 1 ? mine_pow(&b) : validate_pos(&b);
            if (elapsed < 0) { puts("Echec validation."); continue; }
            chain[count] = b;
            if (!save_chain(chain, count + 1)) { puts("[ERREUR] Bloc non sauvegarde."); continue; }
            count++; pending_count = 0;
            if (!save_pending()) puts("[ATTENTION] Impossible de vider le fichier des transactions en attente.");
            printf("[OK] Bloc %d valide et sauvegarde (%.6f s). Participant : %s\n",
                   b.index, elapsed, b.validator);
        } else if (choice == 5) show_chain(chain, count);
        else if (choice == 6) {
            int balances[MAX_ACCOUNTS];
            printf("Integrite : %s\n", verify_chain(chain, count) &&
                calculate_balances(chain, count, 0, balances) ? "VALIDE" : "INVALIDE");
        } else if (choice == 7) {
            printf("\nTransactions en attente (%d) :\n", pending_count);
            for (int i = 0; i < pending_count; i++) printf(" %d. %s\n", i + 1, pending[i]);
        } else if (choice == 8) {
            BlockchainSnapshot snapshot = make_snapshot(chain, count);
            atelier2_menu(&snapshot);
        }
        else if (choice == 9) participants_menu(chain, count);
        else if (choice == 11) integrity_lab(chain, count);
        else if (choice == 10) {
            /* Recharge pedagogique tracable dans le journal des blocs.
               Elle ne modifie PAS le solde tant que le bloc n'est pas valide. */
            if (pending_count >= MAX_PENDING) { puts("File d'attente pleine."); continue; }
            char receiver[30], tx[TX_SIZE];
            read_line("Compte a recharger : ", receiver, sizeof(receiver));
            int i = find_account(receiver);
            if (i < 0) { puts("[REFUS] Compte inexistant."); continue; }
            int amount = read_int("Montant de credits fictifs (1 a 1000000) : ");
            int balances[MAX_ACCOUNTS];
            if (amount < 1 || amount > 1000000 ||
                !calculate_balances(chain, count, 1, balances) ||
                balances[i] > 1000000000 - amount) {
                puts("[REFUS] Montant invalide ou solde maximal atteint."); continue;
            }
            snprintf(tx, sizeof(tx), "SYSTEM->%s:%d", receiver, amount);
            strcpy(pending[pending_count++], tx);
            if (!save_pending()) { pending_count--; puts("[ERREUR] Sauvegarde impossible."); continue; }
            puts("[OK] Recharge fictive en attente. Aller dans Blocs et integrite > Creer et valider un bloc.");
            puts("[INFO] SYSTEM cree des unites : simulation, pas une vraie cryptomonnaie.");
        }
        else puts("Choix inconnu.");
    }
    if (pending_count) puts("Transactions en attente conservees pour le prochain demarrage.");
    return 0;
}
