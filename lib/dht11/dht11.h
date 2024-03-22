#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"

//Un enum pour definir les valeurs que peut prendre le statut de de relevé de donnés.
typedef enum dht11_status dht11_status;
enum dht11_status {
    DHT11_CHECKSUM_ERROR = -2,
    DHT11_TIMEOUT_ERROR = -1,
    DHT11_OK = 0,
};
//Un struct permettant de definir le relevé de donnés
typedef struct dht11_mesures dht11_mesures;
struct dht11_mesures{
    dht11_status status;
    int temperature;
    int humidity;
};

//Les fonctions que nous allons utiliser pour intéragir avec le capteur DHT11 :
void dht11_initialisation(gpio_num_t);
dht11_mesures dht11_lecture(void);

#endif DHT11_H