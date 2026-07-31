#include "drivers/sensor.h"
#include <string.h>

void SensorBuffer_Init(Sensor_Buffer_t *sb)
{
    memset(&sb->data, 0, sizeof(sb->data));
    sb->data_ready = false;
    sb->data_valid = false;
    sb->resolution = 16;
}

void SensorBuffer_NotifyISR(Sensor_Buffer_t *sb)
{
    sb->data_ready = true;
}

void SensorBuffer_Collect(Sensor_Buffer_t *sb, VL53L8CX_Configuration *p_dev)
{
    uint8_t isReady, res;

    if (vl53l8cx_check_data_ready(p_dev, &isReady) != VL53L8CX_STATUS_OK || !isReady) {
        return;
    }

    vl53l8cx_get_ranging_data(p_dev, &sb->data);
    vl53l8cx_get_resolution(p_dev, &res);
    sb->resolution = res;
    sb->data_valid = true;
}

uint32_t SensorBuffer_PackResponse(Sensor_Buffer_t *sb, uint8_t *raw, uint8_t id)
{
    if (!sb->data_valid) return 0;

    VL53L8CX_ResultsData *p = &sb->data;
    uint8_t nz              = sb->resolution;
    uint8_t is_8x8          = (nz == 64);

    sb->data_valid = false;

#ifdef RESPONSE_MODE_FULL
    if (is_8x8) {
        Frame_8x8_Full_t frame;
        frame.silicon_temp_degc = p->silicon_temp_degc;
        for (uint8_t i = 0; i < 64; i++) {
#ifndef VL53L8CX_DISABLE_NB_TARGET_DETECTED
            frame.zones[i].nb_target_detected = p->nb_target_detected[i];
#endif
#ifndef VL53L8CX_DISABLE_SIGNAL_PER_SPAD
            frame.zones[i].signal_per_spad = p->signal_per_spad[i];
#endif
#ifndef VL53L8CX_DISABLE_RANGE_SIGMA_MM
            frame.zones[i].range_sigma_mm = p->range_sigma_mm[i];
#endif
#ifndef VL53L8CX_DISABLE_DISTANCE_MM
            frame.zones[i].distance_mm = p->distance_mm[i];
#endif
#ifndef VL53L8CX_DISABLE_REFLECTANCE_PERCENT
            frame.zones[i].reflectance = p->reflectance[i];
#endif
#ifndef VL53L8CX_DISABLE_TARGET_STATUS
            frame.zones[i].target_status = p->target_status[i];
#endif
        }
        return Frame_Pack_8x8_Full(&frame, raw, id);
    } else {
        Frame_4x4_Full_t frame;
        frame.silicon_temp_degc = p->silicon_temp_degc;
        for (uint8_t i = 0; i < 16; i++) {
#ifndef VL53L8CX_DISABLE_NB_TARGET_DETECTED
            frame.zones[i].nb_target_detected = p->nb_target_detected[i];
#endif
#ifndef VL53L8CX_DISABLE_SIGNAL_PER_SPAD
            frame.zones[i].signal_per_spad = p->signal_per_spad[i];
#endif
#ifndef VL53L8CX_DISABLE_RANGE_SIGMA_MM
            frame.zones[i].range_sigma_mm = p->range_sigma_mm[i];
#endif
#ifndef VL53L8CX_DISABLE_DISTANCE_MM
            frame.zones[i].distance_mm = p->distance_mm[i];
#endif
#ifndef VL53L8CX_DISABLE_REFLECTANCE_PERCENT
            frame.zones[i].reflectance = p->reflectance[i];
#endif
#ifndef VL53L8CX_DISABLE_TARGET_STATUS
            frame.zones[i].target_status = p->target_status[i];
#endif
        }
        return Frame_Pack_4x4_Full(&frame, raw, id);
    }
#else /* RESPONSE_SLIM */
    if (is_8x8) {
        Frame_8x8_Slim_t frame;
#ifndef VL53L8CX_DISABLE_DISTANCE_MM
        memcpy(frame.distance, p->distance_mm, sizeof(frame.distance));
#else
        memset(frame.distance, 0, sizeof(frame.distance));
#endif
        return Frame_Pack_8x8_Slim(&frame, raw, id);
    } else {
        Frame_4x4_Slim_t frame;
#ifndef VL53L8CX_DISABLE_DISTANCE_MM
        memcpy(frame.distance, p->distance_mm, sizeof(frame.distance));
#else
        memset(frame.distance, 0, sizeof(frame.distance));
#endif
        return Frame_Pack_4x4_Slim(&frame, raw, id);
    }
#endif
}
