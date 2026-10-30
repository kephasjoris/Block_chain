
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

#define MAX_TRANSACTIONS 10
#define HASH_SIZE 32
#define HASH_HEX_SIZE 65

// Calcule le hash SHA-256 d'une chaine
void sha256(const char *data, char output[HASH_HEX_SIZE]) {
    unsigned char hash[HASH_SIZE];
    const char hex[] = "0123456789abcdef";

    SHA256((const unsigned char *)data, strlen(data), hash);

    for (int i = 0; i < HASH_SIZE; i++) {
        output[i * 2] = hex[hash[i] >> 4];
        output[i * 2 + 1] = hex[hash[i] & 0x0F];
    }

    output[64] = '\0';
}

// Construit l'arbre et retourne sa racine
void merkle_root(char hashes[][HASH_HEX_SIZE], int count,
                 char root[HASH_HEX_SIZE]) {
    if (count == 0) {
        sha256("", root);
        return;
    }

    char current[MAX_TRANSACTIONS][HASH_HEX_SIZE];

    for (int i = 0; i < count; i++) {
        strcpy(current[i], hashes[i]);
    }

    while (count > 1) {
        int next_count = 0;

        for (int i = 0; i < count; i += 2) {
            int right = (i + 1 < count) ? i + 1 : i;

            char combined[HASH_HEX_SIZE * 2];
            char result[HASH_HEX_SIZE];

            strcpy(combined, current[i]);
            strcat(combined, current[right]);

            sha256(combined, result);
            strcpy(current[next_count++], result);
        }

        count = next_count;
    }

    strcpy(root, current[0]);
}

int main(void) {
    const char *transactions[] = {
        "Alice->Bob:10",
        "Bob->Claire:5",
        "David->Emma:3",
        "Emma->Alice:2"
    };

    int count = 4;
    char hashes[MAX_TRANSACTIONS][HASH_HEX_SIZE];
    char root[HASH_HEX_SIZE];

    printf("=== ARBRE DE MERKLE EN C ===\n\n");

    for (int i = 0; i < count; i++) {
        sha256(transactions[i], hashes[i]);

        printf("Transaction %d : %s\n", i + 1, transactions[i]);
        printf("Hash           : %s\n\n", hashes[i]);
    }

    merkle_root(hashes, count, root);

    printf("Racine de Merkle : %s\n", root);

    return 0;
}
