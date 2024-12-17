#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#define INTERRUPT_PERIOD_MS (100U)
#define CLOCK CLOCK_MONOTONIC


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


int zadA[] = {1, 2, 3};
int zadB[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
int zadC[] = {19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38};
int *zad[] = {zadA, zadB, zadC};
int zadCount[] = {3, 15, 20};
int red[] = {0, 1, -1, 0, 1, -1, 0, 1, 2, -1}; //za svaki prekid svakih 100 ms
int ind[] = {0, 0, 0};
int t = 0;

int **ulazniParametri;

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
static int nijeKraj = 1;
static int brUlaza;



double ukupnoProsjecno=0.0;
int ukupnoPromjena=0;
int ukupnoNeobradeno=0;
int ukupnoDuplih=0;
double globalnoMaksimalno=0.0;
timer_t timer;
volatile int zahtjevZaObradom=0;
volatile int zahtjevajucaDretva=-1;

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

static struct _upravljac{
    volatile int brojPerioda;     // Broj perioda koji su prošli
    volatile int aktivanUlaz;     // Indeks aktivnog ulaza u obradi
    volatile int periodiBezPrekoracenja;
    volatile int koristenaDvaPerioda; // Broj perioda koje je trenutni ulaz potrošio
} upravljac;

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

int dajIduci(){
    int sljedeciZadatak = 0;
    int tipZadatka = red[t];
    
    t = (t+1) % 10;
  
    if (tipZadatka != -1){
        
        sljedeciZadatak = zad[tipZadatka][ind[tipZadatka]];
        ind[tipZadatka] = (ind[tipZadatka] + 1) % zadCount[tipZadatka];

    }
    return sljedeciZadatak-1; 
}

// Funkcija za obradu signala SIGINT
void obradiSigInt(int sig)
{
    nijeKraj = 0;
    
    printf("Simulacija prekinuta na signal SIGINT\n");
}

void stvoriPeriodickiPrekid(int signum, unsigned long period_ms)
{
    struct sigevent event;
    struct itimerspec period;

    event.sigev_notify = SIGEV_SIGNAL;
    event.sigev_signo = signum;

    CALL(STOP, timer_create, CLOCK, &event, &timer);

    milisekundePretvorba(period_ms, &period.it_value);
    milisekundePretvorba(period_ms, &period.it_interval);

    CALL(STOP, timer_settime, timer, 0, &period, NULL);
}

void onemoguciPeriodickiPrekid()
{
    struct itimerspec period = {
    .it_value = {0, 0},
    .it_interval = {0, 0}
};

    int result = timer_settime(timer, 0, &period, NULL);
    if (result != 0) {
        perror("timer_settime");
        printf("errno: %d (%s)\n", errno, strerror(errno));
    }
    CALL(STOP, timer_settime, timer, 0, &period, NULL);
}

static void pokreniPeriodickiPrekid(void (*interrupt_cb)(int))
{
    signal(SIGALRM, interrupt_cb);
    stvoriPeriodickiPrekid(SIGALRM, INTERRUPT_PERIOD_MS);
}



timer_t timer;


// Funkcija za ulaznu dretvu
void *ulaznaDretva(void *arg)
{
    int *param = (int *)arg;
    int id = param[0];
    char msg[100];  // Osiguraj dovoljno prostora za string
    
    if (param[7] == 0){
        sprintf(msg, "dretva %d je popuna i gasim je", id);
        printajVrijeme(msg);
        return NULL;
    }
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
       
        
        stat.brojPromjenaStanja++;
        pthread_mutex_lock(&print_mutex);
        zahtjevZaObradom = 1;
        zahtjevajucaDretva = id;
        pthread_mutex_unlock(&print_mutex);
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
            zahtjevajucaDretva = -1;
        }
        
        trenutakPrvePojave += perioda;
        
    }
    ukupnoProsjecno+=stat.prosjecnoVrijemeReakcije;
    ukupnoPromjena+=stat.brojPromjenaStanja;
    ukupnoNeobradeno+=stat.brojNeobradjenihDogadjaja;
    ukupnoDuplih += ulazniParametri[id][8];
    if(stat.maksimalnoVrijemeReakcije > globalnoMaksimalno){
        globalnoMaksimalno = stat.maksimalnoVrijemeReakcije;
    }
    // Ispis statistike za ovu dretvu
    printf("Statistika za ulaz %d:\n", id);
    printf("  Broj promjena stanja: %d\n", stat.brojPromjenaStanja);
    printf("  Prosječno vrijeme reakcije: %.8f ms\n", stat.prosjecnoVrijemeReakcije);
    printf("  Maksimalno vrijeme reakcije: %.8f ms\n", stat.maksimalnoVrijemeReakcije);
    printf("  Broj neobrađenih događaja: %d\n", stat.brojNeobradjenihDogadjaja);
    printf("  Broj duplih perioda: %d\n", ulazniParametri[id][8]);
    printf("\n");
    return NULL;
}


static void obradiUlaz(int signum){
    (void) signum;
    printajVrijeme("Upravljac: Periodicki prekid zapoceo");
    if (zahtjevZaObradom == 1 && upravljac.aktivanUlaz==zahtjevajucaDretva && zahtjevajucaDretva != -1){
        //obrada nije dovrsena 
        if(upravljac.brojPerioda == 1 && upravljac.periodiBezPrekoracenja <= 10){
            
            ulazniParametri[upravljac.aktivanUlaz][8]++;
            
            upravljac.periodiBezPrekoracenja=0;
            upravljac.brojPerioda=2;
            return;
        }
        else{            
            
            zahtjevZaObradom = 0;
            
            ulazniParametri[upravljac.aktivanUlaz][6] = 1; //obrada ulaza je prekinuta
            ulazniParametri[upravljac.aktivanUlaz][9]++;
            upravljac.aktivanUlaz=-1;
            printajVrijeme("Upravljac: Obrada prekinuta.");
        }
    
    }
    
    int sljedeciUlaz = dajIduci();
   
    if (zahtjevajucaDretva == -1 || sljedeciUlaz == -1 || ulazniParametri[sljedeciUlaz][7] == 0 && sljedeciUlaz != -1){
        printajVrijeme("Upravljac: Ovo je prazan period.");
        
        return ;
    }
    //zahtjevZaObradom = 1;
    if (sljedeciUlaz == zahtjevajucaDretva){
       
        struct timespec reakcija;
        clock_gettime(CLOCK_MONOTONIC, &reakcija);
        vrijemeReakcijeUlaza[sljedeciUlaz] = reakcija;
    }
    //naznaci trenutno aktivni ulaz
    upravljac.aktivanUlaz = sljedeciUlaz;                                                                            
    int potrebnoZaObradu=ulazniParametri[upravljac.aktivanUlaz][3];
    upravljac.brojPerioda=1;
    while (potrebnoZaObradu > 0 && ulazniParametri[sljedeciUlaz][6] == 0 && upravljac.aktivanUlaz==zahtjevajucaDretva){//dok nije gotov i dok nije prekinut
        //obraduje se dretva
        time_utils_delay_for(5);
        potrebnoZaObradu -= 5;
    }
    if (upravljac.aktivanUlaz==zahtjevajucaDretva && ulazniParametri[sljedeciUlaz][6] == 0 && potrebnoZaObradu <= 0 ){
        //obrada nije prekinuta nego zavrsena
        
        ulazniParametri[upravljac.aktivanUlaz][5] =1 ; //obrada je gotova
        
        upravljac.aktivanUlaz=-1;

        if(upravljac.brojPerioda == 1){
            upravljac.periodiBezPrekoracenja++;
        }
    }else{
        //izadi iz funkcije
        
        ulazniParametri[upravljac.aktivanUlaz][6] = 1;        
    }

}


// Funkcija dretve upravljača
int pokreniUpravljaca()
{
    //pokreni periodicki signal
    upravljac.brojPerioda=0;
    upravljac.aktivanUlaz=-1;
    upravljac.periodiBezPrekoracenja=0;
    upravljac.koristenaDvaPerioda=0;

    pthread_t *ulazneDretve = malloc(brUlaza * sizeof(pthread_t));
    // Pokretanje ulaznih dretvi
    for (int i = 0; i < brUlaza; i++)
    {
        
        if (pthread_create(&ulazneDretve[i], NULL, ulaznaDretva, ulazniParametri[i]) != 0)
        {
            perror("Neuspješno kreiranje ulazne dretve");
            return 1;
        }
    }
    //spavaj 10 ms
    time_utils_delay_for(20);
    pokreniPeriodickiPrekid(obradiUlaz);


    while (nijeKraj) ;

    // Onemoguci periodicki signal
    onemoguciPeriodickiPrekid();

    for (int i = 0; i < brUlaza; i++)
    {
        pthread_join(ulazneDretve[i], NULL);
        free(ulazniParametri[i]);
    }
    free(ulazneDretve);
}

int main(int argc, char *argv[])
{
    
    int ulaz[][4] = {
        
        {1000, 100, 30, 1}, {1000, 500, 30, 1}, {1000, 800, 30, 1},
        {5000, 200, 50, 1}, {5000, 600, 50, 1}, {5000, 900, 50, 1},
        {5000, 1200, 50, 1}, {5000, 1600, 50, 1}, {5000, 1900, 50, 1}, //b4-b6
        {5000, 2200, 50, 1}, {5000, 2600, 50, 1}, {5000, 2900, 50, 1}, //b7-b9
        {5000, 3200, 50, 1}, {5000, 3600, 50, 1}, {5000, 3900, 50, 1}, //b9-b12
        {5000, 4200, 50, 1}, {5000, 4600, 50, 1}, {5000, 4900, 50, 1}, //b13-b15

        //samo jedan od ovih 20 redaka svake sekunde
        {20000, 1000, 50, 1},  //c1
        {20000, 2000, 150, 1},  //c2
        {20000, 3000, 50, 1},  //c3
        {20000, 4000, 150, 1},  //c4
        {20000, 5000, 50, 1},  //c5
        {20000, 6000, 150, 1},  //c6
        {20000, 7000, 50, 1},  //c7
        {20000, 8000, 150, 1},  //c8
        {20000, 9000, 50, 1},  //c9
        {20000, 10000, 150, 1},  //c10
        {20000, 11000, 50, 1},  //c11
        {20000, 12000, 150, 1},  //c12
        {20000, 13000, 50, 1},  //c13
        {20000, 14000, 150, 1},  //c14
        {20000, 15000, 50, 1},  //c15
        {20000, 16000, 150, 1},  //c16
        {20000, 17000, 50, 1},  //c17
        {20000, 18000, 150, 0},  //c18-samo popunjavanje tablice
        {20000, 19000, 150, 0},  //c19-samo popunjavanje tablice
        {20000, 20000, 150, 0}

        
    };


    brUlaza = sizeof(ulaz) / sizeof(ulaz[0]);
    //globalno polje u koje se spremaju vremena reakcija za svaku pojedinu dretvu
    vrijemeReakcijeUlaza = malloc(brUlaza * sizeof(struct timespec));
    memset(vrijemeReakcijeUlaza, 0, brUlaza * sizeof(struct timespec));
    struct sigaction act;
    act.sa_handler = obradiSigInt;
    sigaction(SIGINT, &act, NULL);

    pthread_t upravljac;
    printf("haha\n");
    //globalno polje koje sadrži odgovarajuće varijable potrebne za ispravan rad
    ulazniParametri = malloc(brUlaza * sizeof(int *));
    for (int i = 0; i < brUlaza; i++)
    {
        ulazniParametri[i] = malloc(10 * sizeof(int));
        ulazniParametri[i][0] = i;               // ID dretve
        ulazniParametri[i][1] = ulaz[i][1];      // Početno vrijeme
        ulazniParametri[i][2] = ulaz[i][0];      // Perioda događaja
        ulazniParametri[i][3] = ulaz[i][2];      // Vrijeme obrade
        ulazniParametri[i][4] = 0;              //Zatrazio promjenu
        ulazniParametri[i][5] = 0;              //je li obrada gotova
        ulazniParametri[i][6] = 0;              //je li obrada prekinuta
        ulazniParametri[i][7] = ulaz[i][3];     //je li popuna
        ulazniParametri[i][8] = 0;     //kolko drugih perioda
        ulazniParametri[i][9] = 0;     //kolko puta prekinut
    }
   
    //u lab 2 upravljac nije dretva 
    //tu pozovi upravljaca
    clock_gettime(CLOCK_MONOTONIC, &pocetakRada); // Početno vrijeme simulacije
    char msg[100];
    unsigned long ms = pretvoriUmilisekunde(&pocetakRada);
    sprintf(msg, "POČETNO VRIJEME SIMULACIJE=%lu", ms);
    printajVrijeme(msg);
    
    pokreniUpravljaca();

    // Čekanje završetka upravljačke dretve i oslobadanje zauzete mermorije
    
    free(vrijemeReakcijeUlaza);
    
    free(ulazniParametri);

    printf("Statistika ukupno:\n");
    printf("  Broj promjena stanja: %d\n", ukupnoPromjena);
    printf("  Prosječno vrijeme reakcije: %.8f ms\n", ukupnoProsjecno/ukupnoPromjena);
    printf("  Maksimalno vrijeme reakcije: %.8f ms\n", globalnoMaksimalno);
    printf("  Broj neobrađenih događaja: %d\n", ukupnoNeobradeno);
    printf("  Broj duplih perioda: %d\n", ukupnoDuplih);
    printf("\n");
    return 0;
}
