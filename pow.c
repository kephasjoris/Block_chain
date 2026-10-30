
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>

#define HASH_SIZE 32
#define HASH_HEX 65
#define MAX_BLOCKS 5

typedef struct {
    int index;
    char data[256];
    char previous_hash[HASH_HEX];
    char hash[HASH_HEX];
    unsigned long long nonce;
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

    snprintf(input, sizeof(input), "%d|%s|%s|%llu",
             block->index, block->data,
             block->previous_hash, block->nonce);

    sha256(input, block->hash);
}

int valid_hash(const char *hash, int difficulty) {
    for (int i = 0; i < difficulty; i++) {
        if (hash[i] != '0')
            return 0;
    }
    return 1;
}

double mine_block(Block *block, int difficulty) {
    clock_t start = clock();

    block->nonce = 0;

    do {
        calculate_hash(block);

        if (valid_hash(block->hash, difficulty))
            break;

        block->nonce++;
    } while (1);

    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

int main(void) {
    Block chain[MAX_BLOCKS];
    int difficulties[] = {1, 2, 3, 4};

    printf("===== EXERCICE 2 : PROOF OF WORK =====\n\n");

    for (int d = 0; d < 4; d++) {
        int difficulty = difficulties[d];

        chain[0].index = 0;
        strcpy(chain[0].data, "Bloc Genesis");
        strcpy(chain[0].previous_hash, "0");

        double elapsed = mine_block(&chain[0], difficulty);

        printf("Difficulte : %d zero(s)\n", difficulty);
        printf("Nonce      : %llu\n", chain[0].nonce);
        printf("Hash       : %s\n", chain[0].hash);
        printf("Temps      : %.6f seconde(s)\n\n", elapsed);
    }

    printf("===== TEST DE CHAINE DE BLOCS =====\n");

    strcpy(chain[0].data, "Bloc Genesis");
    chain[0].index = 0;
    strcpy(chain[0].previous_hash,
           "0000000000000000000000000000000000000000000000000000000000000000");

    mine_block(&chain[0], 2);

    for (int i = 1; i < 3; i++) {
        chain[i].index = i;
        snprintf(chain[i].data, sizeof(chain[i].data),
                 "Transaction du bloc %d", i);

        strcpy(chain[i].previous_hash, chain[i - 1].hash);

        double elapsed = mine_block(&chain[i], 2);

        printf("\nBloc %d mine\n", i);
        printf("Donnees       : %s\n", chain[i].data);
        printf("Hash precedent: %s\n", chain[i].previous_hash);
        printf("Hash          : %s\n", chain[i].hash);
        printf("Nonce         : %llu\n", chain[i].nonce);
        printf("Temps         : %.6f seconde(s)\n", elapsed);
    }

    return 0;
}
