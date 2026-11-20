#include "atelier2.h"
#include <stdio.h>
#include <stdlib.h>

static int ask(const char *message,int low,int high){
    char line[80],extra;
    int value;
    for(;;){
        printf("%s",message);
        if(!fgets(line,sizeof(line),stdin))return low;
        if(sscanf(line," %d %c",&value,&extra)==1 && value>=low && value<=high)return value;
        printf("[!] Entrer un nombre entre %d et %d.\n",low,high);
    }
}
static void bar(const char *label,double pct){
    int count=(int)(pct/5+.5);
    if(count>20)count=20;
    printf(" %-12s [",label);
    for(int i=0;i<20;i++)putchar(i<count?'#':'.');
    printf("] %5.1f %%\n",pct);
}
static void show(const char *label,Summary s){
    double n=s.trials;
    printf("\n+------------------------------------------+\n| %-40s |\n+------------------------------------------+\n",label);
    bar("Reussite",100*s.successes/n);
    bar("Detection",100*s.detected/n);
    printf(" Pertes %.2f | Cout %.2f | Orphelins %.2f (moyennes/essai)\n",
       s.loss/n,s.overhead/n,s.orphans/n);
    printf(" TP=%d FP=%d FN=%d TN=%d | Delai=%.2f tours (alertes uniquement)\n",
       s.tp,s.fp,s.fn,s.tn,s.detected?s.detection_sum/s.detected:0);

}
static void csv_line(FILE *f,Config c,const char *mode,Summary s,
                     const BlockchainSnapshot *snap, int attacker){
    double n=s.trials;
    fprintf(f,"%d,%s,%d,%d,%d,%d,%d,%d,%u,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%.4f",
       c.attack,mode,s.trials,c.rounds,c.power,c.nodes,c.latency,c.threshold,c.seed,c.protection,
       100*s.successes/n,100*s.detected/n,s.loss/n,s.overhead/n,s.orphans/n,s.reward/n,
       s.tp,s.fp,s.fn,s.tn,s.detected?s.detection_sum/s.detected:0);
    /* Tracabilite du mode lie aux donnees reelles (colonnes facultatives). */
    if (snap && attacker >= 0)
        fprintf(f,",COMPTE,%s,%d,%.16s\n", snap->miners[attacker].name,
                snap->blocks,snap->tip_hash);
    else fputs(",MANUEL,,0,\n", f);
}
static void run_experiment(int scenario, const BlockchainSnapshot *snap, int attacker){
    Config cfg={0};
    cfg.rounds=ask("Tours (10-200) : ",10,200);
    int trials=ask("Essais par scenario (1-1000) : ",1,1000);
    if (snap && attacker >= 0) {
        int total = 0;
        for (int i = 0; i < snap->miner_count; i++) total += snap->miners[i].power;
        /* 1-95 : bornes acceptees par le modele statistique existant. */
        cfg.power = (int)(100.0 * snap->miners[attacker].power / total + 0.5);
        printf("[DONNEES] Mineur adverse : %s, puissance %d/%d => %d %%\n",
               snap->miners[attacker].name, snap->miners[attacker].power, total, cfg.power);
    } else {
        cfg.power=ask("Effort / puissance adversaire (1-95 %) : ",1,95);
    }
    puts("[INFO] Les noeuds P2P restent abstraits : nombre a saisir independamment des comptes.");
    cfg.nodes=ask("Nombre de noeuds honnetes simules (5-100) : ",5,100);
    cfg.latency=ask("Latence simulee (0-100) : ",0,100);
    cfg.threshold=ask("Seuil de detection (20-80 %) : ",20,80);
    cfg.seed=(unsigned)ask("Graine aleatoire (0-1000000) : ",0,1000000);
    FILE *f=fopen("resultats_atelier2.csv","w");
    if(!f){puts("[ERREUR] Impossible d'ouvrir resultats_atelier2.csv");return;}
    fputs("attaque,mode,essais,tours,puissance,noeuds,latence,seuil,seed,defense,reussite_pct,alertes_pct,pertes_moyennes,surcout_moyen,orphelins_moyens,recompense_moyenne,TP,FP,FN,TN,delai_detection_tours,source_puissance,mineur_adverse,blocs_snapshot,hash_snapshot\n",f);
    /* Baseline identique pour chaque situation; reprend la meme graine. */
    cfg.attack=REF;cfg.protection=0;
    Summary reference=sim_evaluate(cfg,trials);
    show("REFERENCE (sans adversaire)",reference);
    csv_line(f,cfg,"NORMAL",reference,snap,attacker);
    for(int i=(scenario==5?1:scenario);i<=(scenario==5?4:scenario);i++){
        cfg.attack=(Attack)i;cfg.protection=0;
        Summary attacked=sim_evaluate(cfg,trials);
        show(sim_attack_name(cfg.attack),attacked);
        csv_line(f,cfg,"ATTAQUE",attacked,snap,attacker);
        cfg.protection=1;
        Summary protected=sim_evaluate(cfg,trials);
        show("AVEC CONTRE-MESURE",protected);
        csv_line(f,cfg,"PROTEGE",protected,snap,attacker);
    }
    fclose(f);
    puts("\n[OK] CSV : resultats_atelier2.csv (dossier courant)");
    puts("[INFO] Reexecuter les memes parametres et la meme graine reproduit les chiffres.");
    puts("[LIMITE] Les essais sont pedagogiques : pas de sockets P2P, de vrais UTXO ni de chronometrage reseau.");
    puts("[LIMITE] Les pertes et couts sont des unites abstraites; les modeles d'attaque et de defense sont simplifies.");
    puts("[NOTE] Precision globale calculable en regroupant TP/FP; rappel = TP/(TP+FN).");
}
/* Relie les parametres des experiences a une photographie du registre.
   Aucun changement sur chain.dat / comptes.dat / pending.dat. */
static void run_snapshot(const BlockchainSnapshot *s) {
    puts("\n========= PHOTOGRAPHIE DE LA BLOCKCHAIN =========");
    printf("Comptes : %d | Blocs confirmes : %d | Transactions confirmees : %d\n",
           s->accounts, s->blocks, s->transactions);
    printf("Validateurs PoS avec mise : %d (non utilises pour ces attaques PoW/P2P)\n", s->pos_validators);
    printf("Dernier hash : %.32s...\n", s->tip_hash);
    if (s->blocks == 0) puts("[NOTE] Aucun bloc valide : les simulations restent possibles, mais sans ancrage historique.");
    if (s->miner_count < 2) {
        puts("[INFO] Il faut au moins deux mineurs PoW avec une puissance > 0.");
        puts("Ajoute-les dans le menu principal > 9, puis reviens ici.");
        return;
    }
    int total = 0;
    for (int i = 0; i < s->miner_count; i++) total += s->miners[i].power;
    puts("Mineurs PoW enregistres :");
    for (int i = 0; i < s->miner_count; i++)
        printf("  %d. %-20s puissance=%d (%.1f %%)\n", i+1,
               s->miners[i].name, s->miners[i].power, 100.0*s->miners[i].power/total);
    int index = ask("Numero du mineur a simuler comme attaquant : ",1,s->miner_count)-1;
    int percent = (int)(100.0*s->miners[index].power/total+0.5);
    if (percent < 1 || percent > 95) {
        printf("[INFO] Part calculee : %d %%, hors limites du moteur (1-95 %%).\n", percent);
        puts("Modifie les puissances PoW dans le menu 9 pour obtenir une part valide.");
        return;
    }
    puts("1. Attaque 51 % / double depense");
    puts("2. Selfish Mining");
    puts("3. Block Withholding");
    puts("4. Sybil (modele P2P abstrait)");
    puts("5. Comparer les quatre attaques");
    int attack = ask("Scenario : ",1,5);
    puts("[LIMITE] La puissance vient des comptes, mais les attaques demeurent statistiques.");
    puts("[LIMITE] Aucune reorganisation effective ni debit de compte. Donnees .dat inchangees.");
    run_experiment(attack,s,index);
}
void atelier2_menu(const BlockchainSnapshot *snapshot){
    for(;;){
        puts("\n+================================================+\n|       ATELIER 2 - ATTAQUES BLOCKCHAIN          |\n+================================================+");
        puts("| 1. Attaque 51 % et double depense               |\n| 2. Selfish Mining                               |\n| 3. Block Withholding                            |\n| 4. Sybil Attack (P2P simule)                    |\n| 5. Comparer les 4 attaques                      |\n| 6. Attaque avec mineurs PoW enregistres         |\n| 0. Retour au menu principal                     |\n+================================================+");
        int choice=ask("Votre choix : ",0,6);
        if(choice==0)return;
        if (choice == 6) run_snapshot(snapshot);
        else run_experiment(choice,NULL,-1);
    }
}
