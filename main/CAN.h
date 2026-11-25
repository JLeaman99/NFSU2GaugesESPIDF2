#pragma once
//CAN imports
#include "driver/twai.h"
//#include "esp_twai.h"
#include "driver/gpio.h"


#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GEAR_REVERSE = -1,
    GEAR_NEUTRAL =  0,
    GEAR_1       =  1,
    GEAR_2       =  2,
    GEAR_3       =  3,
    GEAR_4       =  4,
    GEAR_5       =  5,
    GEAR_6       =  6,
} Gear;


typedef struct {
    uint16_t speed_kph;
    uint16_t rpm;
    Gear gear;

} VehicleDataRT;





bool InitialMessage(void);
bool CANSetup(void);
void ReceiveMessage(void *arg);
void SendMessage(void *arg);

#ifdef __cplusplus
}
#endif