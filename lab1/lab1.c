#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>

int **ulazniParametri;
static pthread_mutex_t print_lock;

static int nijeKraj = 1;
static int brUlaza;

double ukupnoProsjecno=0.0;
int ukupnoPromjena=0;
int ukupnoNeobradeno=0;
double globalnoMaksimalno=0.0;

static struct timespec pocetakRada;

static struct timespec *vrijemeReakcijeUlaza;
// Struktura za statistiku
typedef struct
{
    int brojPromjenaStanja;
    double prosjecnoVrijemeReakcije;
    double maksimalnoVrijemeReakcije;
    int brojNeobradjenihDogadjaja;
} Statistika;

// Funkcija za izračun vremena u milisekundama
double izracunajVrijemeUMilisekundama(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
}

// Funkcija za obradu signala SIGINT
void obradiSigInt(int sig)
{
    nijeKraj = 0;
    printf("Simulacija prekinuta na signal SIGINT\n");
}

// Funkcija dretve upravljača
void *dretvaUpravljac()
{
    int indeksPromjeneUlaza=-1;
    int zatrazenaPromjenaUlaza=-1;
    int vrijemeObrade=0;
    
    while (nijeKraj)
    {
        for (int i = 0; i < brUlaza; i++)
        {
            
            zatrazenaPromjenaUlaza =ulazniParametri[i][4];
            vrijemeObrade = ulazniParametri[i][3];
            if (zatrazenaPromjenaUlaza == 1 )
            {
                ulazniParametri[i][4]=0;
                indeksPromjeneUlaza = i;
                
                struct timespec reakcija;
                clock_gettime(CLOCK_MONOTONIC, &reakcija);
                vrijemeReakcijeUlaza[indeksPromjeneUlaza] = reakcija;
                
                struct timespec vrijemePauze;
                vrijemePauze.tv_sec = vrijemeObrade / 1000; // Sekunde
                vrijemePauze.tv_nsec = (vrijemeObrade % 1000) * 1000000; // Milisekunde u nanosekunde

                // Pauziranje rada upravljačke dretve
                nanosleep(&vrijemePauze, NULL); // Vrijeme obrade u milisekundama
                
                ulazniParametri[i][5] = 1;//oznaci kraj obrade za trenutnu dretvu
                
                zatrazenaPromjenaUlaza=-1;//osiguram da mi ne ulazi ponovno ako nije kraj obrade jos registriran
            }
            
        }
    }
    return NULL;
}

// Funkcija za ulaznu dretvu
void *ulaznaDretva(void *arg)
{
    
    int *param = (int *)arg;
    int id = param[0];
    
    int trenutakPrvePojave = param[1];
    int perioda = param[2];
    int vrijemeObrade = param[3];
    Statistika stat = {0, 0.0, 0.0, 0}; // Inicijalizacija statistike za ovu dretvu
    struct timespec trenutnoVrijeme;
    clock_gettime(CLOCK_MONOTONIC, &trenutnoVrijeme);
    

    while (nijeKraj)
    {
        
        while (izracunajVrijemeUMilisekundama(pocetakRada, trenutnoVrijeme) < trenutakPrvePojave)//cekanje trenutka prve pojave -> oduzimam vrijeme pocetka od trenutnog da dobijem oblik pogodan za usporedbu
        {
            clock_gettime(CLOCK_MONOTONIC, &trenutnoVrijeme);
        }
        
        
        
        ulazniParametri[id][4] = 1; //promjena ulaza
        
        
        struct timespec trenutakPromjeneStanja;//zabiljezi trenutak promjene stanja ulaza
        clock_gettime(CLOCK_MONOTONIC, &trenutakPromjeneStanja);

        while (!ulazniParametri[id][5] && (izracunajVrijemeUMilisekundama(pocetakRada, trenutnoVrijeme) < trenutakPrvePojave + perioda))
        {
            usleep(10000);
            clock_gettime(CLOCK_MONOTONIC, &trenutnoVrijeme);
        }

        if ((vrijemeReakcijeUlaza[id].tv_sec != 0 || vrijemeReakcijeUlaza[id].tv_nsec != 0) && nijeKraj)//odgovor primljen od upravljaca
        {
            printf("%d: Tocno vrijeme kad je prepoznata promjena : tv_sec = %ld, tv_nsec = %ld\n", 
               id, vrijemeReakcijeUlaza[id].tv_sec, vrijemeReakcijeUlaza[id].tv_nsec);
            printf("%d: Tocno vrijeme kad je promjena postavljena: tv_sec = %ld, tv_nsec = %ld\n", 
               id, trenutakPromjeneStanja.tv_sec, trenutakPromjeneStanja.tv_nsec);
            double vrijemeReakcije = izracunajVrijemeUMilisekundama(trenutakPromjeneStanja, vrijemeReakcijeUlaza[id]);
            printf("Vrijeme reakcije za dretvu %d: %.8f ms\n", id,vrijemeReakcije);
            if(ulazniParametri[id][5]==1){//obrada upravljaca gotova

                stat.brojPromjenaStanja++;
                stat.prosjecnoVrijemeReakcije = ((stat.prosjecnoVrijemeReakcije * (stat.brojPromjenaStanja - 1)) + vrijemeReakcije) / stat.brojPromjenaStanja;
                if (vrijemeReakcije > stat.maksimalnoVrijemeReakcije)
                {
                    stat.maksimalnoVrijemeReakcije = vrijemeReakcije;
                }
                ulazniParametri[id][5]=0;
                
            }else
            {
                stat.brojNeobradjenihDogadjaja++;
            }
        }
        //ulazniParametri[id][4] = 0;
        
        trenutakPrvePojave += perioda;
    }
    ukupnoProsjecno+=stat.prosjecnoVrijemeReakcije;
    ukupnoPromjena+=stat.brojPromjenaStanja;
    ukupnoNeobradeno+=stat.brojNeobradjenihDogadjaja;
    if(stat.maksimalnoVrijemeReakcije > globalnoMaksimalno){
        globalnoMaksimalno = stat.maksimalnoVrijemeReakcije;
    }
    // Ispis statistike za ovu dretvu
    printf("Statistika za ulaz %d:\n", id);
    printf("  Broj promjena stanja: %d\n", stat.brojPromjenaStanja);
    printf("  Prosječno vrijeme reakcije: %.8f ms\n", stat.prosjecnoVrijemeReakcije);
    printf("  Maksimalno vrijeme reakcije: %.8f ms\n", stat.maksimalnoVrijemeReakcije);
    printf("  Broj neobrađenih događaja: %d\n", stat.brojNeobradjenihDogadjaja);
    printf("\n");
    return NULL;
}

int main(int argc, char *argv[])
{
   
    int ulaz[][4] = {
        {1000, 0, 30, 1}, {1000, 400, 30, 1}, {1000, 700, 30, 1},
        { 5000,  100, 50, 1}, {5000,  500, 50, 1}, {5000,  800, 50, 1},
        /* { 5000, 1100, 50, 1}, {5000, 1500, 50, 1}, {5000, 1800, 50, 1}, //b4-b6
        { 5000, 2100, 50, 1}, {5000, 2500, 50, 1}, {5000, 2800, 50, 1}, //b7-b9
        { 5000, 3100, 50, 1}, {5000, 3500, 50, 1}, {5000, 3800, 50, 1}, //b9-b12
        { 5000, 4100, 50, 1}, {5000, 4500, 50, 1}, {5000, 4800, 50, 1}, //b13-b15

        //samo jedan od ovih 20 redaka svake sekunde
        {20000,   900,  50, 1},  //c1
        {20000,  1900, 150, 1},  //c2
        {20000,  2900,  50, 1},  //c3
        {20000,  3900, 150, 1},  //c4
        {20000,  4900,  50, 1},  //c5
        {20000,  5900, 150, 1},  //c6
        {20000,  6900,  50, 1},  //c7
        {20000,  7900, 150, 1},  //c8
        {20000,  8900,  50, 1},  //c9
        {20000,  9900, 150, 1},  //c10
        {20000, 10900,  50, 1},  //c11
        {20000, 11900, 150, 1},  //c12
        {20000, 12900,  50, 1},  //c13
        {20000, 13900, 150, 1},  //c14
        {20000, 14900,  50, 1},  //c15
        {20000, 15900, 150, 1},  //c16
        {20000, 16900,  50, 1},  //c17
        {20000, 17900, 150, 0},  //c18-samo popunjavanje tablice
        {20000, 18900, 150, 0},  //c19-samo popunjavanje tablice
        {20000, 19900, 150, 0}   //c20-samo popunjavanje tablice */
    };
    brUlaza = sizeof(ulaz) / sizeof(ulaz[0]);
    //globalno polje u koje se spremaju vremena reakcija za svaku pojedinu dretvu
    vrijemeReakcijeUlaza = malloc(brUlaza * sizeof(struct timespec));
    memset(vrijemeReakcijeUlaza, 0, brUlaza * sizeof(struct timespec));
    struct sigaction act;
    act.sa_handler = obradiSigInt;
    sigaction(SIGINT, &act, NULL);

    pthread_t upravljac;
    pthread_t *ulazneDretve = malloc(brUlaza * sizeof(pthread_t));
    //globalno polje koje sadrži odgovarajuće varijable potrebne za ispravan rad
    ulazniParametri = malloc(brUlaza * sizeof(int *));
    for (int i = 0; i < brUlaza; i++)
    {
        ulazniParametri[i] = malloc(6 * sizeof(int));
        ulazniParametri[i][0] = i;               // ID dretve
        ulazniParametri[i][1] = ulaz[i][1];      // Početno vrijeme
        ulazniParametri[i][2] = ulaz[i][0];      // Perioda događaja
        ulazniParametri[i][3] = ulaz[i][2];      // Vrijeme obrade
        ulazniParametri[i][4] = 0;              //Zatrazio promjenu
        ulazniParametri[i][5] = 0;              //je li obrada gotova
    }

    // Pokretanje upravljačke dretve
    if (pthread_create(&upravljac, NULL, dretvaUpravljac, NULL) != 0)
    {
        perror("Neuspješno kreiranje upravljačke dretve");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &pocetakRada); // Početno vrijeme simulacije

    // Pokretanje ulaznih dretvi
    for (int i = 0; i < brUlaza; i++)
    {
        if (pthread_create(&ulazneDretve[i], NULL, ulaznaDretva, ulazniParametri[i]) != 0)
        {
            perror("Neuspješno kreiranje ulazne dretve");
            return 1;
        }
    }

    // Čekanje završetka ulaznih dretvi
    for (int i = 0; i < brUlaza; i++)
    {
        pthread_join(ulazneDretve[i], NULL);
        free(ulazniParametri[i]);
    }

    // Čekanje završetka upravljačke dretve i oslobadanje zauzete mermorije
    pthread_join(upravljac, NULL);
    free(vrijemeReakcijeUlaza);
    free(ulazneDretve);
    free(ulazniParametri);

    printf("Statistika ukupno:\n");
    printf("  Broj promjena stanja: %d\n", ukupnoPromjena);
    printf("  Prosječno vrijeme reakcije: %.8f ms\n", ukupnoProsjecno/ukupnoPromjena);
    printf("  Maksimalno vrijeme reakcije: %.8f ms\n", globalnoMaksimalno);
    printf("  Broj neobrađenih događaja: %d\n", ukupnoNeobradeno);
    printf("\n");
    return 0;
}
