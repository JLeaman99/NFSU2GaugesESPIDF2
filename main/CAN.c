#include "CAN.h"

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


//CAN VARIABLES
#define CAN_TX_GPIO (gpio_num_t)3
#define CAN_RX_GPIO (gpio_num_t)2
#define RX_TASK_PRIO            8

static VehicleDataRT vehicle_data = {0, 0, GEAR_NEUTRAL};
static SemaphoreHandle_t vehicle_data_mutex = NULL;


 #pragma region CAN Setup
bool InitialMessage(void){
    twai_message_t message;
if (twai_receive(&message, pdMS_TO_TICKS(3000)) == ESP_OK) {
    printf("Message received\n");\
    return true;
} else {
    printf("Failed to receive message\n");
    return false;
}
}

bool CANSetup(void){
    vehicle_data_mutex = xSemaphoreCreateMutex(); //Create a mutex for vehicle data access

    //Serial.begin(115200);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL); // Set to no-acknowledgment mode for testing
    g_config.mode = TWAI_MODE_NORMAL; // Set to normal mode for standard operation

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); //Potentially TWAI_TIMING_CONFIG_500KBITS
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); //May need to be changed to reduce data overload

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        printf("Driver installed\n");
    } else {
        printf("Failed to install driver\n");
        return false;
    }

    //Start TWAI driver
    if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return false;
    }

    uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA |  TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_BUS_OFF | TWAI_ALERT_TX_FAILED | TWAI_ALERT_RX_QUEUE_FULL| TWAI_ALERT_BUS_RECOVERED;
     if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
        printf("Alerts configured\n");
    } else {   
        printf("Failed to configure alerts\n");
        return false;
    }
    // if(InitialMessage() == true){
    //         return true;
    //     }
    return true;

}

void ReceiveMessage(void *arg){
    int counter =0;
    while(1){
        printf("Waiting for message...\n");
 // Check if alert happened
  uint32_t alerts_triggered;
  twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(1000));
  twai_status_info_t twaistatus;
  twai_get_status_info(&twaistatus);

  // Handle alerts
  if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
   printf("Alert: TWAI controller has become error passive.");
  }
  if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
    printf("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
    printf("Bus error count: %" PRIu32 "\n", twaistatus.bus_error_count);
  }
  if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
    printf("Alert: The RX queue is full causing a received frame to be lost.");
    printf("RX buffered: %" PRIu32 "\t", twaistatus.msgs_to_rx);
    printf("RX missed: %" PRIu32 "\t", twaistatus.rx_missed_count);
    printf("RX overrun %" PRIu32 "\n", twaistatus.rx_overrun_count);
  }    
  if (alerts_triggered & TWAI_ALERT_BUS_OFF) {
    printf("Alert: Bus-Off state, initiating recovery\n");
    twai_initiate_recovery(); // Required before driver can transmit again
}
if (alerts_triggered & TWAI_ALERT_BUS_RECOVERED) {
    printf("Alert: Bus has recovered, restarting driver\n");
    twai_start(); // Must call twai_start() again after recovery completes
}    
//Wait for message to be received
twai_message_t message;
while(twai_receive(&message,0) == ESP_OK){
//Process received message
if (message.extd) {
    printf("Message is in Extended Format\n");
} else {
    printf("Message is in Standard Format\n");
}
printf("Message Number: %d\n", counter++);
printf("ID is %d\n", message.identifier);
if (!(message.rtr)) {
    printf("DLC = %d\n",message.data_length_code);
    for (int i = 0; i < message.data_length_code; i++) {
        printf("Data byte %d = %d\n", i, message.data[i]);
        }
    }

    // if (message.identifier == 0xAAAA) {
    //     if (vehicle_data_mutex != NULL) {
    //         if (xSemaphoreTake(vehicle_data_mutex, pdMS_TO_TICKS(100))) {
    //             vehicle_data.speed_kph = message.data[0];
    //             vehicle_data.rpm = message.data[1] << 8 | message.data[2];
    //             vehicle_data.gear = (Gear)message.data[3];
    //             xSemaphoreGive(vehicle_data_mutex);
    //         } else {
    //             printf("Failed to take mutex for vehicle data update\n");
    //         }
    //     } else {
    //         printf("Vehicle data mutex is NULL\n");
    //     }
    // }

    
}
// esp_err_t err = twai_receive(&message,0);

// if (err == ESP_OK) {
//     printf("Message received\n");
    
// } else if(err == ESP_ERR_TIMEOUT) {
//     printf("Message Timeout\n");
//     continue; // Skip processing if no message is received
//     }
//      else if(err == ESP_ERR_INVALID_ARG) {
//     printf("Invalid argument\n");
//     continue; // Skip processing if no message is received
//     }
//      else if(err == ESP_ERR_INVALID_STATE) {
//     printf("Invalid state\n");
//     continue; // Skip processing if no message is received
//     }
//      else if(err == ESP_FAIL) {
//     printf("Failed to receive message\n");
//     continue; // Skip processing if no message is received
//     }




}
}

uint32_t getNextCanId() {
  static uint8_t index = 0;
  uint32_t baseId = 0x100;
  uint32_t id = baseId + index;
  index = (index + 1) % 10;  // Cycle back to 0 after 9
  return id;
}

void SendMessage(void *arg)
{
    while(1){
twai_message_t message;
message.identifier = 0xAAAA;
message.extd = 1;
message.data_length_code = 4;
for (int i = 0; i < 4; i++) {
    message.data[i] = i;
}

esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(100));
//Queue message for transmission
if (err == ESP_OK) {
    printf("Message queued for transmission\n");
} else if (err == ESP_ERR_TIMEOUT) {    
    printf("Failed on Timeout\n");
}
else if (err == ESP_ERR_INVALID_ARG) {
    printf("Invalid argument\n");
}
else if (err == ESP_FAIL) {
    printf("Driver not installed or stopped\n");
}
else if (err == ESP_ERR_INVALID_STATE) {
    printf("Driver not started or bus-off\n");
    twai_initiate_recovery(); // Required before driver can transmit again
}
vTaskDelay(pdMS_TO_TICKS(100));

}

}

#pragma endregion
