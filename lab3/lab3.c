#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#define INTERRUPT_PERIOD_MS (100U)
#define CLOCK CLOCK_MONOTONIC

#define SIMULATOR_THREAD_PRIORITY (99)

#define WARN 0
#define STOP 1
#define CALL(ACT,FUNC,...)      \
do {                            \
    if (FUNC(__VA_ARGS__)) {    \
        perror(#FUNC);          \
        if (ACT == STOP)        \
            exit(1);            \
    }                           \
} while(0)

int **ulazniParametri;

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
static int nijeKraj = 1;
static int brUlaza;



double ukupnoProsjecno=0.0;
int ukupnoPromjena=0;
int ukupnoNeobradeno=0;
int ukupnoDuplih=0;
int ukupnoNedozvoljenihDuplih=0;
double globalnoMaksimalno=0.0;
timer_t timer;


static struct timespec pocetakRada;

static struct timespec *vrijemeReakcijeUlaza;
// Struktura za statistiku
typedef struct
{
    int brojPromjenaStanja;
    double prosjecnoVrijemeReakcije;
    double maksimalnoVrijemeReakcije;
    int brojNeobradjenihDogadjaja;
    int brojDuplihPerioda;
} Statistika;

static void timespec_sub(struct timespec *A, struct timespec *B)
{
    A->tv_sec -= B->tv_sec;
    A->tv_nsec -= B->tv_nsec;
    if ( A->tv_nsec < 0 ) {
        A->tv_sec--;
        A->tv_nsec += 1000000000;
    }
}

static unsigned long pretvoriUmilisekunde(struct timespec *t)
{
    return t->tv_sec * 1000 + t->tv_nsec / 1000000;
}

void milisekundePretvorba(unsigned long ms, struct timespec *t)
{
    t->tv_sec = ms / 1000;
    t->tv_nsec = (ms % 1000) * 1000000;
}
void time_utils_delay_for(unsigned long ms)
{
    struct timespec t;
    milisekundePretvorba(ms, &t);
    clock_nanosleep(CLOCK, 0, &t, NULL);
}

// Funkcija za izračun vremena u milisekundama
double izracunajVrijemeUMilisekundama(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
}


void printajVrijeme(const char *msg)
{
    struct timespec t;
    pthread_mutex_lock(&print_mutex);
    clock_gettime(CLOCK, &t);
    timespec_sub(&t, &pocetakRada);
    unsigned long ms = pretvoriUmilisekunde(&t);
    printf("[%lu] %s\n", ms, msg);
    pthread_mutex_unlock(&print_mutex);
}

unsigned long dohvati_vrijeme_ms()
{
    struct timespec t;
    clock_gettime(CLOCK, &t);
    timespec_sub(&t, &pocetakRada);
    return pretvoriUmilisekunde(&t);
}

static unsigned long broj_iteracija_10_ms;
static void trosi_10_ms()
{
    for (unsigned long i = 0; i < broj_iteracija_10_ms; i++)
    {
        asm volatile("" ::: "memory");
    }
}

static void odredi_broj_iteracija_10_ms()
{
    broj_iteracija_10_ms = 1000000;
    bool run = true;
    unsigned long t0;
    unsigned long t1;

    while (run)
    {
        t0 = dohvati_vrijeme_ms();
        trosi_10_ms();
        t1 = dohvati_vrijeme_ms();
        if (t1 - t0 >= 10)
        {
            run = false;
        }
        else
        {
            broj_iteracija_10_ms *= 10;
        }

    }
    
    broj_iteracija_10_ms = broj_iteracija_10_ms * (10. / (t1 - t0));
}

void inicijaliziraj_vrijeme()
{
    clock_gettime(CLOCK, &pocetakRada);
    odredi_broj_iteracija_10_ms();
    
    // Ponovno postavi pocetno vrijeme jer je racunanje iteracija za
    // kasnjenje 10 ms trajalo neko vrijeme, pa da ne bude pomaka
    // na pocetku simulacije
    clock_gettime(CLOCK, &pocetakRada);
}

void simuliraj_x_ms(int ms)
{
    for (unsigned long i = 0; i < ms / 10; i++)
    {
        trosi_10_ms();
    }
}

// Funkcija za obradu signala SIGINT
void obradiSigInt(int sig)
{
    nijeKraj = 0;
    
    printf("Simulacija prekinuta na signal SIGINT\n");
}

// Funkcija za ulaznu dretvu
void *ulaznaDretva(void *arg)
{
    int *param = (int *)arg;
    int id = param[0];
    char msg[100];  // Osiguraj dovoljno prostora za string
    int trenutakPrvePojave = param[1];
    int perioda = param[2];
    int vrijemeObrade = param[3];
    Statistika stat = {0, 0.0, 0.0, 0,0}; // Inicijalizacija statistike za ovu dretvu
    struct timespec trenutnoVrijeme;
    clock_gettime(CLOCK_MONOTONIC, &trenutnoVrijeme);
    

    while (nijeKraj)
    {
        
        while (izracunajVrijemeUMilisekundama(pocetakRada, trenutnoVrijeme) < trenutakPrvePojave && nijeKraj)//cekanje trenutka prve pojave -> oduzimam vrijeme pocetka od trenutnog da dobijem oblik pogodan za usporedbu
        {
            clock_gettime(CLOCK_MONOTONIC, &trenutnoVrijeme);
        }            
        ulazniParametri[id][4] =  1; //promjena ulaza    
        sprintf(msg, "ulaz %d naznacio promjenu", id);
        printajVrijeme(msg);
        stat.brojPromjenaStanja++;
        struct timespec trenutakPromjeneStanja;//zabiljezi trenutak promjene stanja ulaza
        clock_gettime(CLOCK_MONOTONIC, &trenutakPromjeneStanja);

        while (!ulazniParametri[id][5] && (izracunajVrijemeUMilisekundama(pocetakRada, trenutnoVrijeme) < trenutakPrvePojave + perioda))
        {
            
            time_utils_delay_for(10);
            clock_gettime(CLOCK_MONOTONIC, &trenutnoVrijeme);
        }

        if ( (vrijemeReakcijeUlaza[id].tv_sec != 0 || vrijemeReakcijeUlaza[id].tv_nsec != 0) && nijeKraj)//odgovor primljen od upravljaca
        {
            if(ulazniParametri[id][5]==1){//obrada upravljaca gotova                
                double vrijemeReakcije = izracunajVrijemeUMilisekundama(trenutakPromjeneStanja, vrijemeReakcijeUlaza[id]);
                
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



// Funkcija dretve upravljača
void *dretvaUpravljac(void *p)
{
    int id = *(int *)p;
    printf ( "Upravljac s id-em %d\n", id );
    
    
    int prvaPojavaUlazneDretve = ulazniParametri[id][1] + 5;
    while (nijeKraj)
    {       
        printajVrijeme("prije sleepa");
        time_utils_delay_for(prvaPojavaUlazneDretve);
        printajVrijeme("pposlje sleepa");
        int zatrazenaPromjenaUlaza =ulazniParametri[id][4];
        int vrijemeObrade = ulazniParametri[id][3];
        if (zatrazenaPromjenaUlaza == 1 )
        {
            char msg[100];
            
            sprintf(msg, "upravljac %d prepoznao da je ulaz %d aktivan", id, ulazniParametri[id][0]);
            printajVrijeme(msg);
            ulazniParametri[id][4]=0;
            
            
            struct timespec reakcija;
            clock_gettime(CLOCK_MONOTONIC, &reakcija);
            vrijemeReakcijeUlaza[id] = reakcija;
            simuliraj_x_ms(vrijemeObrade);
            

            // Pauziranje rada upravljačke dretve
             // Vrijeme obrade u milisekundama
            
            ulazniParametri[id][5] = 1;//oznaci kraj obrade za trenutnu dretvu
            
        }
        prvaPojavaUlazneDretve += ulazniParametri[id][2];
            
        
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    
    int ulaz[][4] = {
        
        {1000, 100, 30, 1}, {1000, 500, 30, 1}, {1000, 800, 30, 1},
        {5000, 200, 150, 1}, {5000, 600, 50, 1}, {5000, 900, 50, 1},
    
    };

    brUlaza = sizeof(ulaz) / sizeof(ulaz[0]);
    //globalno polje u koje se spremaju vremena reakcija za svaku pojedinu dretvu
    vrijemeReakcijeUlaza = malloc(brUlaza * sizeof(struct timespec));
    memset(vrijemeReakcijeUlaza, 0, brUlaza * sizeof(struct timespec));
    struct sigaction act;
    act.sa_handler = obradiSigInt;
    sigaction(SIGINT, &act, NULL);

    pthread_t *upravljackeDretve = malloc(brUlaza * sizeof(pthread_t));
    pthread_t *ulazneDretve = malloc(brUlaza * sizeof(pthread_t));
    //globalno polje koje sadrži odgovarajuće varijable potrebne za ispravan rad
    ulazniParametri = malloc(brUlaza * sizeof(int *));
    for (int i = 0; i < brUlaza; i++)
    {
        ulazniParametri[i] = malloc(7 * sizeof(int));
        ulazniParametri[i][0] = i;               // ID dretve
        ulazniParametri[i][1] = ulaz[i][1];      // Početno vrijeme
        ulazniParametri[i][2] = ulaz[i][0];      // Perioda događaja
        ulazniParametri[i][3] = ulaz[i][2];      // Vrijeme obrade
        ulazniParametri[i][4] = 0;              //Zatrazio promjenu
        ulazniParametri[i][5] = 0;              //je li obrada gotova
        ulazniParametri[i][6] = 0;              //je li obrada prekinuta
        
    }
   
    inicijaliziraj_vrijeme();
    printf("broj_iteracija_10_ms = %lu\n",broj_iteracija_10_ms);
    char msg[100];
    unsigned long ms = pretvoriUmilisekunde(&pocetakRada);
    sprintf(msg, "POČETNO VRIJEME SIMULACIJE=%lu", ms);
    printajVrijeme(msg);
    
    // Pokretanje ulaznih dretvi
    pthread_attr_t attr;
    CALL(STOP, pthread_attr_init, &attr);
    CALL(STOP, pthread_attr_setinheritsched, &attr, PTHREAD_EXPLICIT_SCHED);
    CALL(STOP, pthread_attr_setschedpolicy, &attr, SCHED_FIFO);
    struct sched_param param;
    param.sched_priority = SIMULATOR_THREAD_PRIORITY;
    CALL(STOP, pthread_attr_setschedparam, &attr, &param);

    for (int i = 0; i < brUlaza; i++)
    {
        if (pthread_create(&ulazneDretve[i], &attr, ulaznaDretva, ulazniParametri[i]) != 0)
        {
            perror("Neuspješno kreiranje ulazne dretve");
            return 1;
        }
    }
    int max_priority = sched_get_priority_max(SCHED_FIFO);
    printf("Maksimalni prioritet za SCHED_FIFO je %d\n", max_priority);
    int id[brUlaza];
    for (int i = 0; i < brUlaza; i++)
    {
        id[i] = i;
        /* if (pthread_create(&upravljackeDretve[i], NULL, dretvaUpravljac, (void *) &id[i]) != 0)
        {
            perror("Neuspješno kreiranje upravljacke dretve");
            return 1;
        } */
        struct sched_param param;
        
        param.sched_priority = max_priority - (ulazniParametri[id[i]][0])-1;
        CALL(STOP, pthread_attr_setschedparam, &attr, &param);
        CALL(STOP, pthread_create, &upravljackeDretve[i], &attr, dretvaUpravljac, (void *) &id[i]);
    }

    // Čekanje završetka ulaznih dretvi
    for (int i = 0; i < brUlaza; i++)
    {
        pthread_join(ulazneDretve[i], NULL);
        pthread_join(upravljackeDretve[i], NULL);    
        free(ulazniParametri[i]);
    }

    // Čekanje završetka upravljačke dretve i oslobadanje zauzete mermorije
    free(vrijemeReakcijeUlaza);
    
    //free(ulazniParametri);

    printf("Statistika ukupno:\n");
    printf("  Broj promjena stanja: %d\n", ukupnoPromjena);
    printf("  Prosječno vrijeme reakcije: %.8f ms\n", ukupnoProsjecno/ukupnoPromjena);
    printf("  Maksimalno vrijeme reakcije: %.8f ms\n", globalnoMaksimalno);
    printf("  Broj neobrađenih događaja: %d\n", ukupnoNeobradeno);
    printf("  Broj duplih perioda: %d\n", ukupnoDuplih);
    printf("\n");
    return 0;
}
