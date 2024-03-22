#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "rom/ets_sys.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

//#define DEBUG_LOG

/* Définition des MACROS */
#define DHT11_DEMARRAGE_DELAIS          (1000)
#define DHT11_PERIODE_ACTUALISATION     (2*1000*1000)     // (us) On va utiliser est_timer.h qui fonctionne en micro-secondes.
#define DHT11_TEMPS_DEPART_NIVEAU_BAS   (20*1000)        // Pour une durée de 2ms
#define DHT11_TEMPS_DEPART_NIVEAU_HAUT  (40)           // Pour une durée de 40us

/* Variables privées */
static gpio_num_t dht11_gpio; //Pourquoi ? Pour ne pas repasser en argument la valeur de la pin pour chaque fonction qui va l'utiliser. Plus sime de faire une variable static qui va etre utilisé dans toutes les fonction ci-apres.
static int64_t temps_derniere_lecture = -2000000;
static dht11_mesures derniere_lecture;

/* Afin d'y voir plus clair dans les sequences d'envoie et de recption de données,
 * nous allons definir plusieurs fonctions*/

/* Prototypes des Fonctions Privées */
static int dht11_attendre_avant_timeout(uint16_t usecondes, int niveau); //Permet d'attendre x usecondes au niveau haut/bas avant de retourner un timeout
static void dht11_envoie_signal_depart(void);
static int dht11_verif_reponse(void);
static dht11_mesures dht11_erreur_timeout(void);
static dht11_mesures dht11_erreur_checksum(void);
static dht11_status dht11_verif_checksum(uint8_t *data);
#ifdef DEBUG_LOG
    static void afficherBits(uint8_t *data, size_t taille);
#endif



/* Définition des fonctions publiques */
void dht11_initialisation(gpio_num_t gpio_num){
    /* Ici on veut juste patienter pour que le capteur puisse boot convenablement */
    vTaskDelay(DHT11_DEMARRAGE_DELAIS/portTICK_PERIOD_MS);
    dht11_gpio = gpio_num;
    #ifdef DEBUG_LOG
            ESP_LOGI("DEBUG", "%d", dht11_gpio);
    #endif
}

dht11_mesures dht11_lecture(void){
    /* DHT11 prend jusqu'à 2 secondes (pour etre large mais c'est en theorie ~1,004 secondes)
     * pour donner une nouvelle valeur actualisée de la temp/humi. Si on appele la fonction 
     * "lecture" trop tôt, on retourne tout simplement l'ancienne valeur de la lecture. */
    #ifdef DEBUG_LOG
        ESP_LOGI("DEBUG", "Enter dht11_lecture()");
    #endif
    if ((esp_timer_get_time() - DHT11_PERIODE_ACTUALISATION) < temps_derniere_lecture){
        #ifdef DEBUG_LOG
            ESP_LOGI("DEBUG", "Trop rapide");
        #endif
        return derniere_lecture;
    }

    temps_derniere_lecture = esp_timer_get_time(); 

    /* Data = 8bit integral RH data + 8bit decimal RH data + 8bit integral T data 
     * + 8bit decimal T data + 8bit check-sum.
     * => Soit 5 octects pour chaque lecture */

    uint8_t data[5]={0,0,0,0,0};

    // On déclanche la sequence de démarrage de la lecture
    dht11_envoie_signal_depart();

    if (dht11_verif_reponse() == DHT11_TIMEOUT_ERROR){
        #ifdef DEBUG_LOG
            ESP_LOGI("DEBUG", "Verif reponse timeout");
        #endif
        return derniere_lecture = dht11_erreur_timeout();
    }
    /* On construit le pointeur data avec les elements lus depuis le DHT11 bit par bit */
    #ifdef DEBUG_LOG
            ESP_LOGI("DEBUG", "Avant boncle construction data");
    #endif
    for(int i=0; i<40; i++){
        if(dht11_attendre_avant_timeout(50,0)==DHT11_TIMEOUT_ERROR){
            #ifdef DEBUG_LOG
                ESP_LOGI("DEBUG", "DHT11_TIMEOUT_ 50");
            #endif
            return derniere_lecture = dht11_erreur_timeout();
        }
        if(dht11_attendre_avant_timeout(70,1)>28){
            data[i/8] |= (1<<(7-(i%8)));
        }
    }
    #ifdef DEBUG_LOG
            ESP_LOGI("DEBUG", "Apres construction data");
    #endif
    /* On rappel que le checksum permet de verifier la coherence et donc la bonne lecture des
     * bits faite par la boncle precedente : l'octet de checksum (data[4]) vaut la somme des 
     * 4 octets precendents */
    if (dht11_verif_checksum(data)!=DHT11_CHECKSUM_ERROR){
        /* Comme toutes les verification sont bonnes, nous modifions les valeurs de la structure 
         * 'dht11_mesures derniere_mesure' avec les données mise-à-jour. */
        derniere_lecture.temperature = data[2];
        derniere_lecture.humidity = data[0];
        derniere_lecture.status = DHT11_OK;
        #ifdef DEBUG_LOG
            afficherBits(data, 5); 
        #endif
    }
    else{
        #ifdef DEBUG_LOG
            ESP_LOGI("DEBUG", "ERREUR CHECKSUM");
        #endif
        return derniere_lecture = dht11_erreur_checksum();
    }

    return derniere_lecture;
}   

#ifdef DEBUG_LOG
    static void afficherBits(uint8_t *data, size_t taille) {
        for (size_t i = 0; i < taille; i++) {
            for (int j = 7; j >= 0; j--) {
                ESP_LOGI("DEBUG", "%u", (data[i] >> j) & 1);
            }
            ESP_LOGI("DEBUG", " ");
        }
        ESP_LOGI("DEBUG", "\n");
    }
#endif

static int dht11_attendre_avant_timeout(uint16_t usecondes, int niveau){
    int compteur_usecondes = 0;
    while(gpio_get_level(dht11_gpio)==niveau){
        if (compteur_usecondes++ > usecondes){
            return DHT11_TIMEOUT_ERROR;
        }
        ets_delay_us(1); // Permet d'attendre 1us.
    }
    return compteur_usecondes;
}

static void dht11_envoie_signal_depart(void){
    /* Séquence à faire pour signaler au DHT11 une mesure */
    #ifdef DEBUG_LOG
        ESP_LOGI("DEBUG", "Envoie signal depart");
    #endif
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 0);
    ets_delay_us(DHT11_TEMPS_DEPART_NIVEAU_BAS);
    gpio_set_level(dht11_gpio, 1);
    ets_delay_us(DHT11_TEMPS_DEPART_NIVEAU_HAUT);
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);
    #ifdef DEBUG_LOG
        ESP_LOGI("DEBUG", "Signal depart ENVOYÉ");
    #endif
}

static int dht11_verif_reponse(void){
    if (dht11_attendre_avant_timeout(80, 0)==DHT11_TIMEOUT_ERROR){
        return DHT11_TIMEOUT_ERROR;
    }
    if (dht11_attendre_avant_timeout(80,1)==DHT11_TIMEOUT_ERROR){
        return DHT11_TIMEOUT_ERROR;
    }
    return DHT11_OK;
}

 static dht11_mesures dht11_erreur_timeout(void){
    dht11_mesures erreur_timeout = {DHT11_TIMEOUT_ERROR, -100, -100};
    return erreur_timeout;
 }

 static dht11_mesures dht11_erreur_checksum(void){
    dht11_mesures erreur_checksum = {DHT11_CHECKSUM_ERROR, -200, -200};
    return erreur_checksum;
 }

 static dht11_status dht11_verif_checksum(uint8_t *data){
    dht11_status checksum_status = DHT11_OK;
    if (data[4]!=(data[0]+data[1]+data[2]+data[3])){
        checksum_status = DHT11_CHECKSUM_ERROR;
    }
    return checksum_status;
 }

