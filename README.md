# Blockchain pédagogique en C — Ateliers 1 & 2

Plateforme **en terminal**, développée en **C** avec **OpenSSL (SHA-256)** et utilisable dans **VS Code** sous Windows (MSYS2 UCRT64). Le projet reprend l'Atelier 1 (Merkle, PoW, PoS, blocs) et l'Atelier 2 (simulation d'attaques, détection, contre-mesures, évaluation). Il comprend un **laboratoire d'intégrité sur copie mémoire**.

> **Cadre du projet :** simulateur pédagogique, et non blockchain décentralisée destinée à une utilisation réelle. Les identités sont des noms simples (sans clés/signatures), les puissances PoW sont des poids déclarés, et les attaques sont principalement des modèles probabilistes.

## 1. Structure du projet

```text
Atelier2_Blockchain_Donnees_Reelles/
├── blockchain.c                 # Programme principal, menus, comptes, blocs, persistance, labo d'intégrité
├── common.h                     # Constantes partagées
├── merkle.c                     # Exercice autonome : arbre de Merkle
├── pow.c                        # Exercice autonome : Proof of Work
├── pos.c                        # Exercice autonome : Proof of Stake
├── src/
│   ├── atelier2.h               # Types et déclarations des simulations
│   ├── menu_atelier2.c          # Menu de simulation, résultats et export CSV
│   ├── experiences.c            # Moteur d'expériences reproductibles
│   └── attaques/
│       ├── attaque51.c          # Attaque 51 % / double dépense symbolique
│       ├── selfish.c            # Selfish Mining
│       ├── withholding.c        # Block Withholding
│       └── sybil.c              # Sybil Attack
├── .vscode/
│   └── tasks.json               # Compilation multi-fichiers pour VS Code
└── output/                      # Destination de l'exécutable ; données si lancement depuis ce dossier
```

Les fichiers `merkle.c`, `pow.c`, `pos.c` servent à présenter séparément les exercices de l'Atelier 1. **Ne les ajoutez pas** à la compilation de `blockchain.exe` car ils possèdent leur propre `main()`.

## 2. Installation et compilation

### Windows / VS Code

**Prérequis :** GCC MSYS2 UCRT64 et bibliothèque OpenSSL UCRT64 disponibles sous `C:\msys64\ucrt64` ; VS Code.

1. Extraire le ZIP et ouvrir **le dossier qui contient `blockchain.c`** dans VS Code.
2. Appuyer sur **Ctrl + Shift + B** et choisir **Compiler Blockchain + Atelier 2**.
3. Lancer, depuis le dossier du projet, dans PowerShell :

```powershell
.\output\blockchain.exe
```

Si vous préférez exécuter depuis `output` :

```powershell
cd output
.\blockchain.exe
```

**Attention :** les fichiers `.dat` et `resultats_atelier2.csv` sont lus/écrits dans le **répertoire courant d'exécution**, qui peut donc être le dossier du projet **ou** `output`. Toujours lancer depuis le même dossier pour retrouver les mêmes données.

Le bouton « exécuter le fichier C » peut ne compiler que `blockchain.c` et provoquer `undefined reference to atelier2_menu` : utiliser la tâche de compilation multi-fichiers.

### Linux

```bash
mkdir -p output
gcc -std=c11 -Wall -Wextra blockchain.c src/*.c src/attaques/*.c -o output/blockchain -lcrypto
./output/blockchain
```

## 3. Navigation dans les menus

```text
BLOCKCHAIN — MENU PRINCIPAL
1. Comptes et soldes
2. Transactions
3. Blocs et integrite
4. Mineurs PoW et validateurs PoS
5. Atelier 2 - Simulations d'attaques
0. Quitter
```

| Chemin | Action |
|---|---|
| `1 > 1` | Créer un compte avec un solde initial fictif |
| `1 > 2` | Afficher solde total, mise bloquée et solde disponible |
| `1 > 3` | Ajouter une **recharge fictive en attente** |
| `2 > 1` | Ajouter un transfert en attente (expéditeur, destinataire, montant) |
| `2 > 2` | Consulter les transactions en attente |
| `3 > 1` | Créer et valider un bloc PoW ou PoS |
| `3 > 2` | Afficher les blocs et leurs hash |
| `3 > 3` | Vérifier l'intégrité et la cohérence des soldes |
| `3 > 4` | Ouvrir le laboratoire de falsification/restauration **sur copie** |
| `4 > 1` | Gérer les mineurs et validateurs (puis les options internes 1 à 4) |
| `5 > 1` | Accéder au menu des quatre attaques et aux comparaisons |

Dans les sous-menus, `0` revient en arrière ; `0` à l'accueil ferme le programme.

## 4. Comptes, transactions, participants et blocs

### Comptes et transferts

Un compte est identifié par un nom unique composé de lettres. Il possède un solde total et, éventuellement, une mise PoS bloquée. Le solde **disponible = solde total − mise PoS**.

Exemple :

1. Créer `Joris` avec 20 unités et `Demba` avec 10 unités (`1 > 1`).
2. Saisir un transfert (`2 > 1`) : expéditeur `Joris`, destinataire `Demba`, montant `4`.
3. La transaction est placée **en attente** et enregistrée sous la forme `Joris->Demba:4` ; elle ne change pas encore les soldes confirmés.
4. Enregistrer au moins un mineur PoW actif ou un validateur PoS actif (ci-dessous).
5. Choisir `3 > 1`, puis PoW (`1`) ou PoS (`2`). Après validation, les soldes confirmés deviennent **Joris : 16 ; Demba : 14**.

La recharge (`1 > 3`) est une **émission de crédits de test** notée `SYSTEM->Joris:12`. Elle n'a d'effet qu'après validation d'un bloc. `SYSTEM` n'est pas un compte utilisateur et cette émission n'est pas soumise à une autorisation cryptographique : **ne pas assimiler cela à une monnaie réelle**.

### Mineurs PoW

Depuis `4 > 1 > 4`, choisir un compte puis lui attribuer une puissance relative PoW (0 à 1000). Exemple : `Joris=60`, `Demba=40` => poids de sélection respectifs **60 %** et **40 %**. Ces nombres ne sont **pas** des hash/s mesurés ; ils servent à simuler le choix du mineur. Une valeur de `0` signifie que le compte n'est pas actif comme mineur. Au moins un mineur positif est nécessaire pour créer un bloc PoW.

### Validateurs PoS

Depuis `4 > 1 > 2`, bloquer une mise sur un compte existant (ex. `Alice=40`). Le montant immobilisé reste inclus dans le solde total mais n'est plus disponible pour les transferts. `4 > 1 > 3` permet de retirer une mise. Au moins un validateur avec mise positive est nécessaire pour créer un bloc PoS. Les noms historiques de `common.h` ne créent **pas** de comptes ; pour les nouveaux blocs, les participants proviennent des comptes enregistrés.

### Sauvegarde

| Fichier | Rôle |
|---|---|
| `comptes.dat` | Comptes, soldes initiaux, mises PoS et puissances PoW (format ACC2 ; lecture des anciens ACC1) |
| `chain.dat` | Blocs validés et transactions confirmées |
| `pending.dat` | Transactions et recharges en attente |

Au démarrage, le programme recharge les données et recalcule les soldes à partir de la chaîne. **Sauvegarder les trois `.dat` avant de changer de version**, et ne pas mélanger les fichiers de deux répertoires d'exécution.

## 5. Laboratoire d'intégrité : falsifier et restaurer

Chemin : **`3 > 4`**. Ce laboratoire copie les blocs **en RAM** et ne modifie jamais les fichiers `.dat`.

1. Créer d'abord au moins un bloc confirmé contenant une transaction.
2. Ouvrir `3 > 4`, puis choisir `1. Modifier une transaction de la copie`.
3. Sélectionner le numéro de bloc (à partir de `0`) et de transaction (à partir de `1`).
4. Remplacer par exemple `Joris->Demba:4` par `Joris->Demba:40`.
5. Choisir `2` : le contrôle doit montrer une **incohérence de Merkle/hash**. La copie est invalide.
6. Choisir `4` : les blocs d'origine sont recopiés dans la copie ; l'intégrité redevient valide.
7. Quitter le laboratoire et vérifier `3 > 3` : la blockchain originale reste valide.

**À comprendre :** la restauration remet les **données d'origine**. Elle ne re-mine pas les blocs falsifiés, ne recalcule pas une nouvelle chaîne concurrente et ne modifie pas les soldes. Cohérence des hash et légitimité d'une transaction sont deux propriétés distinctes.

## 6. Atelier 2 : attaques et contre-mesures

Chemin : **`5 > 1`**, puis :

| Option | Attaque | Objectif étudié | Détection / défense simplifiées |
|---|---|---|---|
| `1` | 51 % et double dépense | Une branche concurrente domine la branche honnête ; double dépense **symbolique** | Surveiller la réorganisation et les anomalies de chaîne |
| `2` | Selfish Mining | Retenir puis publier stratégiquement des blocs pour augmenter l'avantage de minage | Surveiller les blocs orphelins et les comportements de publication |
| `3` | Block Withholding | Retenir des blocs valides dans une pool | Repérer les anomalies de blocs soumis / récompenses |
| `4` | Sybil Attack | Multiplier les identités malveillantes dans un réseau P2P simulé | Surveiller la concentration et limiter les identités/connexions suspectes |
| `5` | Comparaison complète | Exécuter les quatre attaques avec et sans protection | Exporter les métriques dans un CSV |
| `6` | Mode avec mineurs enregistrés | Déduire la puissance adverse des poids PoW des comptes existants | Conserver une photographie informative de la chaîne |

Les simulations de l'Atelier 2 **ne falsifient pas les blocs sauvegardés**, ne débitent pas les comptes et ne créent pas de vrais sockets réseau. Les nœuds honnêtes P2P simulés sont **indépendants des comptes** : sept comptes n'impliquent pas sept nœuds.

### Paramètres d'expérimentation

- **Tours** : 10 à 200 étapes par essai.
- **Essais** : 1 à 1000 répétitions par scénario.
- **Puissance / effort adverse** : 1 à 95 % ; son sens exact dépend de l'attaque. L'option 6 la calcule depuis les mineurs enregistrés.
- **Nœuds honnêtes** : 5 à 100 entités P2P abstraites, sans création de comptes.
- **Latence** : indice abstrait 0 à 100, **pas des millisecondes**.
- **Seuil de détection** : 20 à 80 %, paramètre du détecteur du modèle.
- **Seed** : graine aléatoire entière 0 à 1 000 000 ; à paramètres et environnement équivalents, elle permet de reproduire les tirages pseudo-aléatoires.

Exemple de campagne : **40 tours, 100 essais, puissance 55 %, 20 nœuds, latence 20, seuil 40, seed 42**, puis ne changer qu'un paramètre à la fois.

### Résultats CSV

Chaque simulation écrit `resultats_atelier2.csv` dans le **répertoire courant**. **Le CSV précédent est écrasé** au lancement suivant : le renommer ou le copier avant une nouvelle expérience.

Les modes sont `NORMAL`, `ATTAQUE`, `PROTEGE`. Les principales colonnes :

| Colonne | Interprétation |
|---|---|
| `reussite_pct` | Pourcentage d'essais où le critère de réussite de l'attaque est satisfait |
| `alertes_pct` | Pourcentage d'essais ayant déclenché une alerte (**ce n'est pas la précision**) |
| `pertes_moyennes` | Pertes fictives moyennes par essai, en unités abstraites |
| `surcout_moyen` | Coût fictif moyen de la défense |
| `orphelins_moyens` | Moyenne de blocs orphelins selon le modèle |
| `recompense_moyenne` | Récompense abstraite moyenne du modèle |
| `TP / FP / FN / TN` | Vrais positifs / faux positifs / faux négatifs / vrais négatifs |
| `delai_detection_tours` | Délai moyen en tours **parmi les essais ayant déclenché une alerte** |
| `source_puissance` | `MANUEL` ou `COMPTE` |
| `mineur_adverse` | Mineur sélectionné en mode lié aux comptes |
| `blocs_snapshot`, `hash_snapshot` | Contexte de la blockchain au moment de l'expérience |

**Précision** = `TP / (TP + FP)` ; **rappel** = `TP / (TP + FN)` (si les dénominateurs sont non nuls). Un taux de détection élevé sur quelques essais ne prouve pas l'efficacité réelle d'un dispositif.

## 7. Limites et bonnes pratiques

- Les comptes ne sont pas authentifiés par des clés privées ou des signatures numériques.
- Le PoS simplifié n'inclut pas de *slashing*, période de déblocage, finalité distribuée ni récompenses réalistes.
- La puissance PoW est un **poids de sélection** et ne proportionne pas le travail SHA-256 réellement effectué.
- Les attaques sont des **modèles pédagogiques** : pas de réorganisation effective du registre de comptes, pas de vrais UTXO ni de propagation P2P réelle.
- Les contre-mesures ne garantissent pas la sécurité d'une blockchain réelle.
- Pour un rapport scientifique : comparer `NORMAL`, `ATTAQUE` et `PROTEGE`, utiliser plusieurs valeurs de puissance/latence/nœuds, plusieurs graines et un nombre suffisant d'essais ; conserver les CSV de chaque campagne.

## 8. Démonstration conseillée

1. Créer deux comptes, attribuer une puissance PoW à un mineur, saisir un transfert et créer un bloc.
2. Afficher les soldes et vérifier l'intégrité.
3. Dans le laboratoire d'intégrité, falsifier une transaction **dans la copie**, constater l'invalidité, puis restaurer.
4. Exécuter les quatre attaques, comparer les indicateurs sans/avec défense et sauvegarder le CSV.
5. Présenter les résultats du CSV sous forme de tableaux et exposer clairement les limites du simulateur.

---

**Projet universitaire — Blockchain / Ateliers 1 et 2.** Ce README décrit la version avec **menu réorganisé et laboratoire d'intégrité** ; il ne décrit pas l'ancien prototype Mini Twitter, qui n'est pas inclus dans cette archive.
