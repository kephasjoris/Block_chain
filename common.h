#ifndef BLOCKCHAIN_COMMON_H
#define BLOCKCHAIN_COMMON_H

/* Parametres communs aux demonstrations de l'atelier 1. */
#define HASH_SIZE 32
#define HASH_HEX 65                 /* 64 chiffres hexadecimaux + fin de chaine */
#define MAX_BLOCKS 100
#define MAX_TRANSACTIONS 10
#define TX_SIZE 100
#define MAX_VALIDATORS 5
#define DIFFICULTY 2
#define REPEATS 10

/* Jeu de mises de reference, partage entre pos.c et blockchain.c :
   Alice 50, Bob 30, Claire 15, David 4, Emma 1 (total 100). */
#define INITIAL_VALIDATORS { {"Alice", 50.0}, {"Bob", 30.0}, {"Claire", 15.0}, {"David", 4.0}, {"Emma", 1.0} }

#endif
