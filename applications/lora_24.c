/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-12-02     DUAN       the first version
 */
#include <lora_24.h>
#include <manager.h>


#define THREAD_PRIORITY         10
#define THREAD_STACK_SIZE       2048
#define THREAD_TIMESLICE        5

static rt_thread_t thread_lora_rec = RT_NULL;
static rt_thread_t thread_lora_send = RT_NULL;

#define LORA_UART_NAME       "uart2"    /* 串口设备名称 */
static rt_device_t lora_serial;                /* 串口设备句柄 */
struct serial_configure lora_config = RT_SERIAL_CONFIG_DEFAULT;  /* 初始化配置参数 */

struct rt_semaphore lora_sem;  // 信号量


rt_err_t rx_callback(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&lora_sem);
    return RT_EOK;
}

void init_lora()
{
    /* step1：查找串口设备 */
    lora_serial = rt_device_find(LORA_UART_NAME);
    /* step2：修改串口配置参数 */
    lora_config.baud_rate = BAUD_RATE_115200;        //修改波特率为 9600
    lora_config.data_bits = DATA_BITS_8;           //数据位 8
    lora_config.stop_bits = STOP_BITS_1;           //停止位 1
    lora_config.bufsz     = 128;                   //修改缓冲区 buff size 为 128
    lora_config.parity    = PARITY_NONE;           //无奇偶校验位

    /* step3：控制串口设备。通过控制接口传入命令控制字，与控制参数 */
    rt_device_control(lora_serial, RT_DEVICE_CTRL_CONFIG, &lora_config);

    /* step4：打开串口设备。以中断接收及轮询发送模式打开串口设备 */
    rt_device_open(lora_serial, RT_DEVICE_FLAG_INT_RX);
    /* 初始化信号量 */
    rt_sem_init(&lora_sem, "lora_sem", 0, RT_IPC_FLAG_FIFO);
    rt_device_set_rx_indicate(lora_serial, rx_callback);
}



/* 线程 的入口函数 */
static void lora_send(void *parameter)
{
    char str[] = "RT-ok!\r\n";
    while (1)
    {
        if (lng>100.0) {
            rt_device_write(lora_serial, 0, str, (sizeof(str) - 1));  //向指定串口发送数据
        }
        rt_thread_mdelay(300);
    }
}

static void lora_rec(void *parameter)
{
    char lora_char;//接收字节
    char lora_buffer[32]={0};//摇杆数据
    int lora_buffer_index=0;//索引
    char lora_draw_buffer[32]={0};//抽水数据
    int lora_draw_buffer_index=0;//索引
    char row_s[5]={0};
    char col_s[5]={0};
    char draw_s[2]={0};
    while (1)
    {
        while(rt_device_read(lora_serial,0, &lora_char, 1) != 1)
        {
            /* 阻塞等待接收信号量，等到信号量释放后再次读取数据 */
           rt_sem_take(&lora_sem, RT_WAITING_FOREVER);
        }

//接收A。。。Z数据，遥控
        if(lora_char=='A')
        {
            lora_buffer[lora_buffer_index]=lora_char;
            lora_buffer_index+=1;
        }
        else if (lora_buffer_index !=0) {
            lora_buffer[lora_buffer_index]=lora_char;
            lora_buffer_index+=1;
        }
        //如果接收到完整数据后分割字符串处理
        if(lora_char=='Z'&&lora_buffer_index!=0)
        {
            lora_buffer[lora_buffer_index] = '\0';
            sscanf(lora_buffer,"A%[0-9],%[0-9]Z,",col_s,row_s);
            row = atoi(row_s);//转弯
            col = atoi(col_s);//直行
            // 控制电机输出 将摇杆值0-99转换到1000-2000
            if ((row>45) && (row<55))row=50;//转弯
            if ((col>45) && (col<55))col=50;//直行
//          rt_kprintf("row %d col %d\r\n",row,col);
            lora_buffer_index =0;
        }
//接收B。。。Z数据，抽水
        if (lora_char == 'B')
        {
             lora_draw_buffer[lora_draw_buffer_index] = lora_char;
             lora_draw_buffer_index+=1;
        }
        else if (lora_draw_buffer_index != 0) {
              lora_draw_buffer[lora_draw_buffer_index] = lora_char;
              lora_draw_buffer_index+=1;
        }
              // 接收到完整数据 后分隔字符串处理
        if (lora_char == 'Z' && lora_draw_buffer_index!=0) {
              lora_draw_buffer[lora_draw_buffer_index] = '\0';
              sscanf(lora_draw_buffer,"B%[0-9]Z,",draw_s);
              b_draw=atoi(draw_s);
              rt_kprintf("b_draw %d \n",b_draw);
              lora_draw_buffer_index =0;
        }

    }
}

/* 线程 */
int thread_lora_24(void)
{
    init_lora();
    /* 创建线程 */
    thread_lora_rec = rt_thread_create("lora_rec",
            lora_rec, RT_NULL,
                            THREAD_STACK_SIZE,
                            1, THREAD_TIMESLICE);
    /* 如果获得线程控制块，启动这个线程 */
    if (thread_lora_rec != RT_NULL)
        rt_thread_startup(thread_lora_rec);

    /* 创建线程 */
    thread_lora_send = rt_thread_create("lora_send",
        lora_send, RT_NULL,
                                1025,
                                5, THREAD_TIMESLICE);
    /* 如果获得线程控制块，启动这个线程 */
    if (thread_lora_send != RT_NULL)
        rt_thread_startup(thread_lora_send);

    return 0;
}

/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(thread_lora_24, thread_lora_24);

