
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>

#define HASH_SIZE 32
#define HASH_HEX 65
#define MAX_BLOCKS 5
#define MAX_TX 4
#define TX_SIZE 100
#define MAX_VALIDATORS 5
#define DIFFICULTY 2
#define REPEATS 10

typedef struct {
    char name[30];
    double stake;
} Validator;

typedef struct {
    int index;
    char transactions[MAX_TX][TX_SIZE];
    int tx_count;
    char merkle_root[HASH_HEX];
    char previous_hash[HASH_HEX];
    char hash[HASH_HEX];
    unsigned long long nonce;
    char consensus[4];  /* POW ou POS */
    char validator[30];
} Block;

Validator validators[MAX_VALIDATORS] = {
    {"Alice", 50.0},
    {"Bob", 30.0},
    {"Claire", 15.0},
    {"David", 5.0},
    {"Emma", 0.0}
};

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
    if (b->tx_count < 0 || b->tx_count > MAX_TX)
        return 0;

    char hashes[MAX_TX][HASH_HEX];

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

            snprintf(combined, sizeof(combined), "%s%s",
                     hashes[i], hashes[right]);

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

/* ---------- Proof of Work ---------- */

double mine_pow(Block *b) {
    clock_t start = clock();
    char root[HASH_HEX];

    strcpy(b->consensus, "POW");
    b->validator[0] = '\0';

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
    double total = 0.0;

    for (int i = 0; i < MAX_VALIDATORS; i++)
        if (validators[i].stake > 0)
            total += validators[i].stake;

    if (total <= 0)
        return -1;

    double choice = ((double)rand() / ((double)RAND_MAX + 1.0))
                    * total;
    double cumulative = 0.0;

    for (int i = 0; i < MAX_VALIDATORS; i++) {
        if (validators[i].stake <= 0)
            continue;

        cumulative += validators[i].stake;

        if (choice < cumulative)
            return i;
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
    strcpy(b->validator, validators[selected].name);

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
        for (int i = 0; i < MAX_VALIDATORS; i++) {
            if (strcmp(b->validator, validators[i].name) == 0
                && validators[i].stake > 0)
                return 1;
        }
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

/* ---------- Programme principal ---------- */

int main(void) {
    Block chain[MAX_BLOCKS] = {0};
    double elapsed;
    double total_pow = 0.0;
    double total_pos = 0.0;
    int count_pow = 0, count_pos = 0;

    srand((unsigned int)time(NULL));

    printf("===== BLOCKCHAIN INTEGREE =====\n");
    printf("Difficulte PoW : %d zero(s)\n", DIFFICULTY);

    for (int i = 0; i < 3; i++) {
        chain[i].index = i;
        chain[i].tx_count = 2;

        snprintf(chain[i].transactions[0], TX_SIZE,
                 "Alice->Bob:%d", 10 + i);
        snprintf(chain[i].transactions[1], TX_SIZE,
                 "Bob->Claire:%d", 5 + i);

        if (i == 0) {
            memset(chain[i].previous_hash, '0', 64);
            chain[i].previous_hash[64] = '\0';
        } else {
            strcpy(chain[i].previous_hash, chain[i - 1].hash);
        }

        /* Alterne PoW et PoS pour montrer les deux mecanismes. */
        if (i % 2 == 0) {
            elapsed = mine_pow(&chain[i]);
            total_pow += elapsed;
            count_pow++;
        } else {
            elapsed = validate_pos(&chain[i]);
            total_pos += elapsed;
            count_pos++;
        }

        if (elapsed < 0) {
            printf("Erreur pendant la creation du bloc.\n");
            return 1;
        }

        print_block(&chain[i], elapsed);
    }

    printf("\n===== VERIFICATION DE LA CHAINE =====\n");
    printf("Etat de la chaine : %s\n",
           verify_chain(chain, 3) ? "VALIDE" : "INVALIDE");

    printf("\n===== COMPARAISON DES TEMPS =====\n");
    if (count_pow > 0)
        printf("Moyenne PoW (blocs mines) : %.6f s\n",
               total_pow / count_pow);

    if (count_pos > 0)
        printf("Moyenne PoS (blocs valides): %.6f s\n",
               total_pos / count_pos);

    printf("\n===== TEST DE FALSIFICATION =====\n");
    strcpy(chain[1].transactions[0], "Alice->Bob:9999");

    printf("Apres modification : %s\n",
           verify_chain(chain, 3) ? "CHAINE VALIDE" : "CHAINE INVALIDE");

    printf("\n===== FIN DU PROGRAMME =====\n");
    return 0;
}
