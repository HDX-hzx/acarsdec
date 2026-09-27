//
// Created by hdx on 2026/7/14.
//

#ifndef ACARSDEC_USRP_H
#define ACARSDEC_USRP_H

//#define USRPBUFSZ 16384
//#define USRPBUFSZ 327680
#define USRPBUFSZ 16320

#define USRP_MULT 96
#define USRP_RX_RATE 1200000.0
#define USRP_TX_RATE 1152000.0

#include <uhd.h>
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include "pkt.h"

extern pthread_cond_t tx_cond;

typedef struct
{
    char *device_args;//uhd设备参数

   // uint32_t gain;
    double rx_gain;
    double tx_gain;

     int64_t freq;

    /*
     * 实际 ACARS 信道频率，同时也是 TX 发射频率
     */
    double tx_freq;
    /*
     * USRP RX 的硬件中心频率
     * 故意避开实际 ACARS 信道
     */
    double rx_center_freq;

   // double rate;


    double rx_rate;
    double tx_rate;

    size_t rx_channel;
    size_t tx_channel;

    /*
 * 实际发送IQ样点数
 */
    size_t tx_samples;

    uhd_usrp_handle usrp; //usrp设备句柄，rx和tx共用

    //RX
    uhd_rx_streamer_handle rx_streamer;
    uhd_rx_metadata_handle rx_md;
    float complex rx_buffer[USRPBUFSZ*2];

    //TX
    uhd_tx_streamer_handle tx_streamer;
    uhd_tx_metadata_handle tx_md;
    float complex tx_buffer[SAMPLE_MAX_LEN * 2];


    uhd_tune_request_t tune_request;
    uhd_tune_result_t tune_result;

    uhd_stream_args_t rx_stream_args;
    uhd_stream_args_t tx_stream_args;


}usrp_args_t;

extern usrp_args_t usrp_dev;
extern volatile int tx_ready;
extern message_format tx_mf;
extern pthread_mutex_t tx_mutex;
void prepare_tx_packet(void);
//初始化
int preinitUsrp(char **argv,int optind);

int preinitUsrp_lib(char *freq);

int initUsrp(char **af);

//RX
int runUsrpSample(void);

void closeUsrp(void);
//TX
void usrp_transfer_data(
        usrp_args_t *args,
        const message_format *mf);

int usrp_transmit(usrp_args_t *args);


void *usrp_rx_thread(void *arg);

void *usrp_tx_thread(void *arg);


#endif //ACARSDEC_USRP_H
