/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-12-02     DUAN       the first version
 */
#include "manager.h"

static const int  draw_time = 10 ; //抽水时间
//全局变量
u_int relay_status=1;//定时器状态
u_int row = 50;// 横向摇杆接受值0-99
u_int col = 50;// 纵向摇杆接受值0-99
u_int b_draw = 0;// 水泵控制接受值0初始值 1  开启 2关闭
u_int led_status = 0;    //   当前状态灯编码  0 绿色  1 黄色  2 红色  3 蜂鸣
u_int current_theta=0; //当前角
float_t dump_energy = 0;  //  当前剩余电量
float_t back_home_erergy = 15;  //  返航电量阈值
int current_status=1;  // 船当前状态  1  运行（绿）  2 返航（红） 3 抽水（黄）
double lng=0.0;  // 经度
double lat=0.0;   // 纬度
double target_longitude=0;  // 目标经度
double target_latitude=0;   // 目标纬度
int b_back_home=0;// 是否在返航

//声明
u_int is_current_forward=0; // 当前是否前进需要保持航向角度
u_int keep_theta=0; // 需要保持航向角度
u_int error_theta=0; //偏差角
u_int PWM_Increment=0;//增量
u_int left_pwm = 1500;//左电机应该输出值
u_int right_pwm = 1500;//左电机应该输出值
u_int remote_forward_pwm = 1500;//遥控直行输出
u_int remote_steer_pwm = 1500;//遥控转弯输出

static rt_thread_t t_manager = RT_NULL;

void manager(void *parameter)
{
    int temp_forward_pwm=0;
    int forward_pwm=0;
    int steer_pwm=0;
    int max_control=500;
    while(1)
    {
        // 控制抽水泵
        if (b_draw==1&&relay_status==1)//开启
        {
            set_relay(true);
            set_relay_timing(true);
            current_status=3;
        }
        else if (b_draw==2)//关闭
        {
            relay_status=1;
            set_relay(false);
            delete_time();
            current_status=1;
        }
        else if (b_draw==3)//返航
        {
            b_back_home=1;
            current_status=2;
        }
        else if (b_draw==4)//停止返航
        {
            b_back_home=0;
            current_status=1;
        }
        // 控制电机输出 将摇杆值0-99转换到1000-2000
        remote_forward_pwm = (100 - col) * 10 + 1000;//2000-1000
        remote_steer_pwm = row * 10 + 1000;//1000-2000
        //左高右低-电机前进
        left_pwm = 1500 + (remote_forward_pwm - 1500) + (int)(remote_steer_pwm - 1500)/2;
        right_pwm = 1500 - (remote_forward_pwm - 1500) + (int)(remote_steer_pwm - 1500)/2;
        //判断是否需要保持航向
        if (col<20&&row<60&&row>40)
        {
            is_current_forward=1;
            if (current_theta!=0 && keep_theta==0)
            {
                keep_theta=current_theta;
            }
        }
        else {
            is_current_forward=0;
            keep_theta=0;
        }
        //纯P调节走直线
        if(is_current_forward==1)
        {
            rt_kprintf("current_theta %d keep_theta %d\n",current_theta,keep_theta);
            error_theta = keep_theta-current_theta;//偏差
            error_theta = (int)(0.6*(float)error_theta);
            steer_pwm = (int)((1.0 / (1.0 + exp (-0.02 * error_theta)) - 0.5) * 1000);
            forward_pwm = (int)((1.0 / (1.0 + exp (-0.2 * 10)) - 0.5) * 1000);

            if (forward_pwm + abs(steer_pwm) >= max_control)
            {
                temp_forward_pwm = forward_pwm;
                forward_pwm = (int)(max_control * ((float)temp_forward_pwm) / (temp_forward_pwm + abs(steer_pwm)));
                steer_pwm = (int)(max_control * ((float)steer_pwm / (temp_forward_pwm + abs(steer_pwm))));
            }
            left_pwm = 1500 + forward_pwm - steer_pwm;
            right_pwm = 1500 - forward_pwm - steer_pwm;
        }
        //电量少且不在抽水状态才返航  或者  按下返航按键
        if(((dump_energy<=back_home_erergy)&&(b_draw!=1))||((b_back_home==1)&&(b_draw!=1)))
        {
            if((remote_forward_pwm<1700&&remote_forward_pwm>1300)&&(remote_steer_pwm<1700&&remote_steer_pwm>1300))//不动遥控器时才能返航，遥控器最牛
            {
                if((int)target_longitude<10||(int)target_latitude<10)
                {
                    rt_kprintf("error\n");
                }
                else {
                    back_home_pid(lng,lat,target_longitude,target_latitude,&left_pwm ,&right_pwm);//返航
                }
            }
        }
//        rt_kprintf("curren mode :%d M2:%d\r\n",left_pwm,right_pwm);
        /*防止溢出*/
        if(left_pwm>1990) left_pwm=1990;
        if(left_pwm<1010) left_pwm=1010;
        if(right_pwm>1990) right_pwm=1990;
        if(right_pwm<1010) right_pwm=1010;
        set_pwm(left_pwm,right_pwm);//设置目标输出PWM
        /*设置状态灯*/
        if (current_status!=1)
        {
            control_out_status_led(current_status);
            control_status_led(current_status);
        }
        else {
            control_out_status_led(1);
            control_status_led(1);
        }
        rt_thread_mdelay(50);
    }
}

/* 线程 */
int thread_manager(void)
{
    /* 创建线程 */
    t_manager = rt_thread_create("thread_manager",
                                  manager,
                                  RT_NULL,
                                  2048,
                                  20,
                                  5);

    /* 如果获得线程控制块，启动这个线程 */
    if (t_manager != RT_NULL)
        rt_thread_startup(t_manager);
    return 0;
}
