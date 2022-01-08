/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-12-02     RT-Thread    first version
 */

#include <rtthread.h>
#include <stdio.h>
#include "lora_24.h"
#include "led.h"
#include "manager.h"
#include "gps.h"
#include "motor_pwm.h"
#include "compass.h"
#include "energy_adc.h"
#include "relay.h"
#include "back_home.h"
int main(void)
{
    init_relay();
    init_led();
    thread_pwm();
    thread_gps();
    thread_lora_24();
    thread_manager();
    thread_compass();
    thread_energy();

    return RT_EOK;
}
