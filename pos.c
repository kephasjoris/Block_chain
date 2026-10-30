
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>

#define HASH_SIZE 32
#define HASH_HEX 65
#define MAX_VALIDATORS 5

typedef struct {
    char name[30];
    double stake;
} Validator;

typedef struct {
    int index;
    char data[256];
    char previous_hash[HASH_HEX];
    char hash[HASH_HEX];
    int validator_index;
} Block;

void sha256(const char *data, char output[HASH_HEX]) {
    unsigned char digest[HASH_SIZE];
    const char hex[] = "0123456789abcdef";

    SHA256((const unsigned char *)data, strlen(data), digest);

    for (int i = 0; i < HASH_SIZE; i++) {
        output[2 * i] = hex[digest[i] >> 4];
        output[2 * i + 1] = hex[digest[i] & 0x0F];
    }

    output[64] = '\0';
}

void calculate_hash(Block *block) {
    char input[512];

    snprintf(input, sizeof(input), "%d|%s|%s|%d",
             block->index,
             block->data,
             block->previous_hash,
             block->validator_index);

    sha256(input, block->hash);
}

/* Sélection aléatoire pondérée par la mise.
   Plus la mise est élevée, plus la probabilité de sélection est grande. */
int select_validator(Validator validators[], int count) {
    double total_stake = 0.0;

    for (int i = 0; i < count; i++) {
        if (validators[i].stake > 0)
            total_stake += validators[i].stake;
    }

    if (total_stake <= 0)
        return -1;

    double choice = ((double)rand() / ((double)RAND_MAX + 1.0))
                    * total_stake;

    double cumulative = 0.0;

    for (int i = 0; i < count; i++) {
        if (validators[i].stake <= 0)
            continue;

        cumulative += validators[i].stake;

        if (choice < cumulative)
            return i;
    }

    return -1;
}

/* Valide un bloc avec le validateur sélectionné. */
double validate_block_pos(Block *block,
                          Validator validators[],
                          int count) {
    clock_t start = clock();

    int selected = select_validator(validators, count);

    if (selected == -1) {
        printf("Erreur : aucune mise positive disponible.\n");
        return -1.0;
    }

    block->validator_index = selected;
    calculate_hash(block);

    clock_t end = clock();

    printf("Validateur selectionne : %s\n",
           validators[selected].name);
    printf("Mise du validateur     : %.2f\n",
           validators[selected].stake);
    printf("Hash du bloc           : %s\n", block->hash);

    return (double)(end - start) / CLOCKS_PER_SEC;
}

int main(void) {
    Validator validators[MAX_VALIDATORS] = {
        {"Alice", 50.0},
        {"Bob", 30.0},
        {"Claire", 15.0},
        {"David", 4.0},
        {"Emma", 1.0}
    };

    Block block = {0};

    srand((unsigned int)time(NULL));

    block.index = 1;
    strcpy(block.data, "Alice->Bob:10");
    strcpy(block.previous_hash,
           "0000000000000000000000000000000000000000000000000000000000000000");

    printf("===== EXERCICE 3 : PROOF OF STAKE =====\n\n");

    printf("Liste des validateurs :\n");

    for (int i = 0; i < MAX_VALIDATORS; i++) {
        printf("- %-10s | Mise : %6.2f\n",
               validators[i].name, validators[i].stake);
    }

    printf("\nValidation du bloc :\n");

    double elapsed = validate_block_pos(
        &block, validators, MAX_VALIDATORS
    );

    if (elapsed >= 0.0) {
        printf("Temps de validation PoS : %.6f seconde(s)\n",
               elapsed);
        printf("Statut : bloc valide par le validateur selectionne.\n");
    }

    printf("\nNote : cette implementation est une simulation pedagogique.\n");
    printf("Elle ne represente pas un protocole PoS securise de production.\n");

    return 0;
}
