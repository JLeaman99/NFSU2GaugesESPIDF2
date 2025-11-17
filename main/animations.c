#include "stdio.h"
#include "lvgl.h"


const int AVERAGE_VALUES      = 10;

int GetMovingAverage(int newValue){
    static int values[AVERAGE_VALUES] = {0};
    static int index = 0;
    static int count = 0;
    static double sum = 0;

    // Subtract the value being replaced
    sum -= values[index];

    // Insert the new value
    values[index] = new_value;
    sum += new_value;

    // Update index and count
    index = (index + 1) % AVERAGE_VALUES;
    if (count < AVERAGE_VALUES) count++;

    float avg = sum / count;

    return (int)roundf(avg * 10.0f);
}




