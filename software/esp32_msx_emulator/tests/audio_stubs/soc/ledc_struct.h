#pragma once
#include <stdint.h>
struct AudioTestLedc
{
    struct Group
    {
        struct Channel
        {
            struct { uint32_t duty = 0; } duty;
            struct { uint32_t duty_start = 0; } conf1;
            struct { uint32_t idle_lv = 0, sig_out_en = 0, low_speed_update = 0; } conf0;
        } channel[8];
    } channel_group[1];
};
extern AudioTestLedc LEDC;
