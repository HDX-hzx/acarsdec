//
// Created by hdx on 2026/7/16.
//
#ifdef WITH_USRP

#define _GNU_SOURCE
#define RTLOUTBUFSZ 16384
#define RTLMULT 160
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <complex.h>

#include <uhd.h>

#include "acarsdec.h"
#include "usrp.h"

#include <unistd.h>

#include "modulator.h"


/*
 * USRP B220 mini采样参数
  发送端保持一致
 * rate = SAMP_RATE
 */

//#define USRPINRATE 1152000
#define USRPINRATE 1200000

/*
 * 每次接收buffer大小
 *
 * 对应RTL:
 *
 * RTLINBUFSZ
 *
 */

//#define USRPBUFSZ 16384
#define USRPBUFSZ 16320

/*
 * USRP全局变量
 */

 usrp_args_t usrp_dev;
static message_format tx_message;

static int tx_enable=0;
static int rx_enable=0;

static pthread_t rx_tid;

static pthread_t tx_tid;

static int usrp_running=1;

static message_format tx_msg;

message_format tx_mf;

pthread_mutex_t tx_mutex =
        PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tx_cond =
        PTHREAD_COND_INITIALIZER;
extern int64_t freq;
extern int rx_gain;
extern int tx_gain;

/*
 * 接收运行状态
 */

static int usrp_status = 0;



/*
 * 频率选择函数
 *
 * 与rtl.c中的chooseFc作用一致
 *
 * 多信道时选择中心频率
 *
 */

static unsigned int chooseFc(unsigned int *Fd,unsigned int nbch)
{
    int n;
    int ne;
    int Fc;


    /*
     * 冒泡排序
     */

    do {

        ne = 0;

        for(n=0;n<nbch-1;n++)
        {

            if(Fd[n]>Fd[n+1])
            {
                unsigned int t;

                t = Fd[n+1];
                Fd[n+1]=Fd[n];
                Fd[n]=t;

                ne=1;
            }

        }

    }while(ne);



    /*
     * 简化处理
     *
     * USRP带宽足够
     *
     */

    Fc=(Fd[0]+Fd[nbch-1])/2;


    return Fc;

}



/*
 * 命令行初始化
 *
 * 对应:
 *
 * preinitRtl_exe()
 *
 */


int preinitUsrp(char **argv,int optind)
{

    char *argF;

    char *freqs[MAXNBCHANNELS];


    if(argv[optind]==NULL)
    {

        fprintf(stderr,
        "Need USRP frequency after -u\n");

        exit(1);

    }


    nbch=0;


    while((argF=argv[optind])
          &&
          nbch<MAXNBCHANNELS)
    {

        freqs[nbch]=argF;

        optind++;

        nbch++;

        if(argv[optind]==NULL)
        {
            freqs[nbch]=NULL;
        }

    }



    /*
     * 初始化USRP
     */

    initUsrp(freqs);


    return 0;

}



/*
 * 库调用接口
 *
 * 对应:
 *
 * preinitRtl_lib()
 *
 */


int preinitUsrp_lib(char *freq)
{

    char *freqs[MAXNBCHANNELS];


    if(freq==NULL)
    {

        fprintf(stderr,
        "Need frequency\n");

        exit(1);

    }


    freqs[0]=freq;

    freqs[1]=NULL;


    printf("before initUsrp\n");

    initUsrp(freqs);

    printf("after initUsrp\n");


    return 0;

}






/*
 * USRP初始化
 *
 * 对应rtl.c中的initRtl()
 *
 */


int initUsrp(char **af)
{

    int i;

    char *argF;


    unsigned int Fd[MAXNBCHANNELS];


    unsigned int Fc;


    printf("\n===== STRUCT ASSIGN TEST =====\n");

    printf("input gain = %.1f\n",
           (double)gain);

    printf("input freq = %.0f\n",
           (double)freq);      //調試

   // usrp_dev.gain=gain;
    usrp_dev.rx_gain = (double)rx_gain;
    usrp_dev.tx_gain = (double)tx_gain;


    printf("  usrp_dev.freq = %.0f\n",
           (double)usrp_dev.freq);   //ts

    usrp_dev.freq=freq;

    printf("  usrp_dev.freq = %.0f\n",
           (double)usrp_dev.freq);    //ts

   /*  usrp_dev.tune_request.target_freq=
            usrp_dev.freq;

    usrp_dev.tune_request.rf_freq_policy=
            UHD_TUNE_REQUEST_POLICY_AUTO;

    usrp_dev.tune_request.dsp_freq_policy=
            UHD_TUNE_REQUEST_POLICY_AUTO;   */

    //下述为新增  9.5
    /*
 * 实际 ACARS 信道：
 * TX 仍然工作在 131.450 MHz
 */
    usrp_dev.tx_freq =
        (double)usrp_dev.freq;

    /*
     * RX 故意偏离信道中心 100 kHz。
     *
     * 实际信道：131.450 MHz
     * RX 中心：131.550 MHz
     */
    usrp_dev.rx_center_freq =
        usrp_dev.tx_freq + 100000.0;
    printf("\n===== FREQUENCY PLAN =====\n");
    printf(
        "ACARS channel / TX = %.0f Hz\n",
        usrp_dev.tx_freq
    );

    printf(
        "RX center          = %.0f Hz\n",
        usrp_dev.rx_center_freq
    );

    printf(
        "RX channel offset  = %.0f Hz\n",
        usrp_dev.tx_freq -
        usrp_dev.rx_center_freq
    );
    //上述为新增  9.5
    uhd_tune_request_t rx_tune = {
        .target_freq =  usrp_dev.rx_center_freq,
        .rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
        .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO
    };
    uhd_tune_request_t tx_tune = {
        .target_freq = usrp_dev.tx_freq,
        .rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
        .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO
    };


    /*
     * 创建设备
     */

    usrp_dev.device_args="";


    if(uhd_usrp_make(
            &usrp_dev.usrp,
            usrp_dev.device_args)
           != UHD_ERROR_NONE)
    {
        fprintf(stderr,
                "Failed to create USRP device\n");

        return -1;
    }



    /*
     * 设置RX参数
     */


   // usrp_dev.rate=USRPINRATE;

    usrp_dev.rx_rate = USRP_RX_RATE;//新增  8.31
    usrp_dev.tx_rate = USRP_TX_RATE;

    usrp_dev.rx_channel=0;

    uhd_usrp_set_rx_rate(
            usrp_dev.usrp,
            usrp_dev.rx_rate,
            usrp_dev.rx_channel);
    double actual_rx_rate = 0.0;

    uhd_usrp_get_rx_rate(
        usrp_dev.usrp,
        usrp_dev.rx_channel,
        &actual_rx_rate
    );

    //以下爲新增調試
    printf("\n===== RX RATE DEBUG =====\n");
    printf("Requested RX rate = %.0f\n",
           (double)USRP_RX_RATE);

    printf("Actual RX rate    = %.0f\n",
           actual_rx_rate);

    printf("USRP_MULT         = %d\n",
           USRP_MULT);

    printf("DDC output rate   = %.2f\n",
           actual_rx_rate / USRP_MULT);

    printf("INTRATE           = %d\n",
           INTRATE);

    fprintf(stderr,
    "RX rate=%f\n",
    usrp_dev.rx_rate);

    /*
     * 解析频率
     *
     * 与RTL一致
     *
     */

    nbch=0;

    i=0;


    printf("\n========== FREQ DEBUG 1 ==========\n");
    printf("Before frequency parse:\n");
    printf("nbch = %d\n", nbch);
    printf("i = %d\n", i);

    if (af[i] != NULL)
        printf("af[i] = %s\n", af[i]);
    else
        printf("af[i] = NULL\n");   //ts


    while((argF=af[i])
          &&
          nbch<MAXNBCHANNELS)
    {


        Fd[nbch]=
        ((int)(1000000*atof(argF)
        +INTRATE/2)
        /INTRATE)
        *INTRATE;



        channel[nbch].chn=nbch;


        channel[nbch].Fr=
            (float)Fd[nbch];


        printf("\n========== FREQ DEBUG 2 ==========\n");
        printf("argF = %s\n", argF);
        printf("atof(argF) = %f MHz\n", atof(argF));
        printf("Fd[%d] = %d Hz\n",
               nbch,
               Fd[nbch]);

        printf("channel[%d].chn = %d\n",
               nbch,
               channel[nbch].chn);

        printf("channel[%d].Fr = %.0f Hz\n",
               nbch,
               channel[nbch].Fr);   //ts


        nbch++;

        i++;

    }



    if(nbch==0)
    {

        fprintf(stderr,
        "Need at least one frequency\n");

        return -1;

    }



    /*
     * 选择中心频率
     */


    Fc=chooseFc(Fd,nbch);
    printf("\n========== FREQ DEBUG 3 ==========\n");

    printf("After chooseFc:\n");
    printf("nbch = %d\n", nbch);
    printf("Fc = %.0f Hz\n", (double)Fc);

    for (int k = 0; k < nbch; k++)
    {
        printf(
            "Fd[%d] = %d Hz, channel[%d].Fr = %.0f Hz\n",
            k,
            Fd[k],
            k,
            channel[k].Fr
        );
    }

    /*
     * 设置RX频率
     */

    /*usrp_dev.tune_request.target_freq=
            (double)Fc;

    usrp_dev.tune_request.rf_freq_policy=
        UHD_TUNE_REQUEST_POLICY_AUTO;

    usrp_dev.tune_request.dsp_freq_policy=
            UHD_TUNE_REQUEST_POLICY_AUTO;
    uhd_tune_request_t rx_tune =
    {
        .target_freq = usrp_dev.freq,
        .rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
        .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO
        };  */
    uhd_usrp_set_rx_freq(
        usrp_dev.usrp,
        &rx_tune,
        usrp_dev.rx_channel,
        &usrp_dev.tune_result);

    //下述为新增  9.5
    uhd_usrp_set_rx_antenna(usrp_dev.usrp, "RX2", 0);
    uhd_usrp_set_tx_antenna(usrp_dev.usrp, "TX/RX", 0);
    //上述为新增  9.5

    //ts xs
    double actual_rx_freq = 0.0;
    uhd_error rx_freq_err =
        uhd_usrp_get_rx_freq(
            usrp_dev.usrp,
            usrp_dev.rx_channel,
            &actual_rx_freq
        );
    printf("\n===== RX FREQ DEBUG =====\n");
    printf("Requested RX freq = %lld Hz\n",
           (long long)usrp_dev.freq);

    printf("Actual RX freq    = %.0f Hz\n",
           actual_rx_freq);

    printf("get_rx_freq ret   = %d\n",
           (int)rx_freq_err);
    //ts sx
  //  printf("RX freq=%lf\n",usrp_dev.tune_request.target_freq);

    /*
     * 设置增益
     */
    uhd_usrp_set_rx_gain(
            usrp_dev.usrp,
            usrp_dev.rx_gain,
            usrp_dev.rx_channel,
            "");
    printf("Requested RX gain = %.1f dB\n", usrp_dev.rx_gain);

    //ts下述
    double actual_rx_gain = 0.0;

    uhd_error err_gain = uhd_usrp_get_rx_gain(
        usrp_dev.usrp,
        usrp_dev.rx_channel,
        "",
        &actual_rx_gain
    );

    printf("\n===== RX GAIN DEBUG =====\n");

    printf("Actual RX gain    = %.1f dB\n",
           actual_rx_gain);

    printf("get_rx_gain ret   = %d\n",
           (int)err_gain);
    //ts上述

    /*
     * 创建RX stream
     */


    usrp_dev.rx_stream_args.cpu_format="fc32";

    usrp_dev.rx_stream_args.otw_format="sc16";

    usrp_dev.rx_stream_args.args="";

    usrp_dev.rx_stream_args.channel_list=
            &usrp_dev.rx_channel;


    usrp_dev.rx_stream_args.n_channels=1;


    uhd_rx_streamer_make(
            &usrp_dev.rx_streamer);


    uhd_usrp_get_rx_stream(
            usrp_dev.usrp,
            &usrp_dev.rx_stream_args,
            usrp_dev.rx_streamer);

    //ts xs
    char rx_ant[128] = {0};

    uhd_usrp_get_rx_antenna(
        usrp_dev.usrp,
        usrp_dev.rx_channel,
        rx_ant,
        sizeof(rx_ant)
    );

    printf("===== RX ANTENNA DEBUG =====\n");
    printf("RX channel = %zu\n",
           usrp_dev.rx_channel);

    printf("RX antenna = %s\n",
           rx_ant);
    //ts sx

    usrp_dev.tx_channel=0;


    //TX采样率
    uhd_usrp_set_tx_rate(
    usrp_dev.usrp,
    usrp_dev.tx_rate,
     usrp_dev.tx_channel);

//ts xs
    double actual_tx_rate = 0.0;
    uhd_usrp_get_tx_rate(
    usrp_dev.usrp,
    usrp_dev.tx_channel,
    &actual_tx_rate
);
    printf("TX requested = %.0f\n", usrp_dev.tx_rate);
    printf("TX actual    = %.0f\n", actual_tx_rate);
    printf("USRP_MULT    = %d\n", USRP_MULT);
    printf("RX DDC rate  = %.2f\n",
           actual_rx_rate / (double)USRP_MULT);
    printf("INTRATE      = %d\n", INTRATE);
    //ts ss

    //TX频率
    uhd_usrp_set_tx_freq(
    usrp_dev.usrp,
    &tx_tune,
    usrp_dev.tx_channel,
    &usrp_dev.tune_result);

    //ts xs
    double actual_tx_freq = 0.0;

    uhd_error tx_freq_err =
        uhd_usrp_get_tx_freq(
            usrp_dev.usrp,
            usrp_dev.tx_channel,
            &actual_tx_freq
        );
    printf("\n===== TX FREQ DEBUG =====\n");
    printf("Requested TX freq = %lld Hz\n",
           (long long)usrp_dev.freq);

    printf("Actual TX freq    = %.0f Hz\n",
           actual_tx_freq);

    printf("get_tx_freq ret   = %d\n",
           (int)tx_freq_err);
    //ts sx


    uhd_usrp_set_tx_gain(
    usrp_dev.usrp,
    usrp_dev.tx_gain,
    usrp_dev.tx_channel,
    "");
    printf("Requested TX gain = %.1f dB\n", usrp_dev.tx_gain);


    /*
    * TX stream
*/
    usrp_dev.tx_stream_args.cpu_format="fc32";

    usrp_dev.tx_stream_args.otw_format="sc16";

    usrp_dev.tx_stream_args.args="";

    usrp_dev.tx_stream_args.channel_list=
            &usrp_dev.tx_channel;

    usrp_dev.tx_stream_args.n_channels=1;


    uhd_tx_streamer_make(
            &usrp_dev.tx_streamer);


    uhd_usrp_get_tx_stream(
            usrp_dev.usrp,
            &usrp_dev.tx_stream_args,
            usrp_dev.tx_streamer);


    //tx metadata
    uhd_tx_metadata_make(
            &usrp_dev.tx_md,
            false,
            0,
            0,
            true,
            true);

    //ts xs
    char tx_ant[128] = {0};

    uhd_usrp_get_tx_antenna(
        usrp_dev.usrp,
        usrp_dev.tx_channel,
        tx_ant,
        sizeof(tx_ant)
    );

    printf("TX channel = %zu\n",
           usrp_dev.tx_channel);

    printf("TX antenna = %s\n",
           tx_ant);
//ts sx

    /*
     *  rx metadata
     */

    uhd_rx_metadata_make(
            &usrp_dev.rx_md);


    /*
     * 为每个ACARS信道申请缓存
     *
     * 对应initRtl()
     *
     */


    for(i=0;i<nbch;i++)
    {

        channel_t *ch=&channel[i];

        ch->dm_buffer=
            malloc(RTLOUTBUFSZ*sizeof(float));

        ch->wf=
            malloc(USRP_MULT*sizeof(float complex));

        if(ch->wf==NULL)
        {
            fprintf(stderr,
            "wf malloc failed\n");

            return -1;
        }

        float AMFreq;


        AMFreq =
        (ch->Fr-(float)usrp_dev.rx_center_freq)
        /(float)usrp_dev.rx_rate
        * 2.0 * M_PI;


        for(int k=0;k<USRP_MULT;k++)
        {
            ch->wf[k]=cexpf(
                AMFreq*k*-I
            )/USRP_MULT;
        }
        printf(
            "Fc=%d ch->Fr=%d RX_RATE=%.0f AMFreq=%f\n",
            Fc,
            ch->Fr,
            USRP_RX_RATE,
            AMFreq
        );
    }

    fprintf(stderr,
    "USRP initialized\n");

    return 0;

}

static void usrp_callback(float complex *usrp_buff,
                          uint32_t nsamples)
{
    /*
        * 保存每个 channel 上一次 callback
        * 尚未凑满 USRP_MULT 的状态
        */
    static int decim_pos[MAXNBCHANNELS] = {0};

    static float complex decim_acc[MAXNBCHANNELS] = {0};


    for(int n = 0; n < nbch; n++)
    {
        channel_t *ch = &channel[n];

        int m = 0;

        /*
         * 注意：
         * 不再使用
         *
         * while(i + USRP_MULT <= nsamples)
         *
         * 因为那样会把尾部余数直接丢掉。
         */
        for(uint32_t i = 0; i < nsamples; i++)
        {
            int pos = decim_pos[n];

            /*
             * DDC / 积分
             */
            decim_acc[n] +=
                usrp_buff[i] * ch->wf[pos];

            pos++;


            /*
             * 收满 USRP_MULT=96 个输入样点
             */
            if(pos >= USRP_MULT)
            {
                if(m < RTLOUTBUFSZ)
                {
                    ch->dm_buffer[m++] =
                        cabsf(decim_acc[n]);
                }

                /*
                 * 开始下一组96个样点
                 */
                pos = 0;
                decim_acc[n] = 0.0f + 0.0f * I;
            }

            decim_pos[n] = pos;
        }

/*
    int n;

    for(n = 0; n < nbch; n++)
    {

        channel_t *ch=&channel[n];

        int i=0;
        int m=0;


        float complex D;

        float complex *wf;


        wf=ch->wf;

        while(i+USRP_MULT <= nsamples)
        {
            int ind;

            D=0;


            for(ind=0;
                ind<USRP_MULT;
                ind++)
            {

                D += usrp_buff[i]
                     *
                     wf[ind];

                i++;

            }


            if(m < RTLOUTBUFSZ)
            {
                ch->dm_buffer[m++]=cabsf(D);
            }
            else
            {
                break;
            }

        }   */

     /*   //DDC输出调试
        static unsigned long ddc_debug_count = 0;
        ddc_debug_count++;

        if(ddc_debug_count % 50 == 0)
        {
            float dm_min = 0.0f;
            float dm_max = 0.0f;
            float dm_avg = 0.0f;

            if(m > 0)
            {
                dm_min = ch->dm_buffer[0];

                for(int k = 0; k < m; k++)
                {
                    float v = ch->dm_buffer[k];

                    if(v < dm_min)
                        dm_min = v;

                    if(v > dm_max)
                        dm_max = v;

                    dm_avg += v;
                }

                dm_avg /= (float)m;
            }

            printf(
                "[DDC] ch=%d "
                "nsamples=%u "
                "m=%d "
                "remain=%d "
                "min=%f max=%f avg=%f\n",
                ch->chn,
                nsamples,
                m,
                decim_pos[n],
                dm_min,
                dm_max,
                dm_avg
            );

            fflush(stdout);
        }  */
//上述爲增加調試內容
        static FILE *fp = NULL;
        static int capture_started = 0;
        static int capture_done = 0;
        static int saved_samples = 0;

#define CAPTURE_SAMPLES 2500
#define CAPTURE_THRESHOLD 0.002f

        if(m > 0 && !capture_done)
        {
            float dm_avg_capture = 0.0f;

            for(int k = 0; k < m; k++)
                dm_avg_capture += ch->dm_buffer[k];

            dm_avg_capture /= (float)m;

            /*
             * 检测到发送能量后才开始记录
             */
            if(!capture_started &&
               dm_avg_capture > CAPTURE_THRESHOLD)
            {
                fp = fopen("/tmp/acars_ddc_tx.txt", "w");

                if(fp == NULL)
                {
                    perror("fopen");
                }
                else
                {
                    capture_started = 1;

                    printf(
                        "\n===== TX DDC CAPTURE START =====\n"
                        "trigger avg = %.6f\n",
                        dm_avg_capture
                    );
                }
            }

            /*
             * 触发以后连续保存，不再判断能量，
             * 避免一个数据包中间能量下降导致断裂
             */
            if(capture_started && fp != NULL)
            {
                for(int k = 0;
                    k < m && saved_samples < CAPTURE_SAMPLES;
                    k++)
                {
                    fprintf(
                        fp,
                        "%.9f\n",
                        ch->dm_buffer[k]
                    );

                    saved_samples++;
                }

                if(saved_samples >= CAPTURE_SAMPLES)
                {
                    fclose(fp);
                    fp = NULL;

                    capture_done = 1;

                    printf(
                        "===== TX DDC CAPTURE COMPLETE =====\n"
                        "samples = %d\n"
                        "file = /tmp/acars_ddc_tx.txt\n",
                        saved_samples
                    );
                }
            }
        }
        //调试：捕捉DDC

        if(m>0)
        {
            demodMSK(ch,m);//msk解调
        }

    }

}

int runUsrpSample(void)
{

    size_t num_rx = 0;

    /*
     * 接收buffer
     */

    float complex *buff;

    buff = usrp_dev.rx_buffer;


    usrp_status=1;


    fprintf(stderr,
            "Start USRP RX...\n");

    uhd_stream_cmd_t stream_cmd;


    stream_cmd.stream_mode =
            UHD_STREAM_MODE_START_CONTINUOUS;

    stream_cmd.num_samps=0;

    stream_cmd.stream_now=true;

    uhd_rx_streamer_issue_stream_cmd(
            usrp_dev.rx_streamer,
            &stream_cmd);

    while(usrp_status)
    {

        void *buffs[1];

        buffs[0]=(void *)buff;

        /*
         * UHD接收
         */

        int ret;

        ret = uhd_rx_streamer_recv(
                usrp_dev.rx_streamer,
                buffs,
                USRPBUFSZ,
                &usrp_dev.rx_md,
                3.0,
                false,
                &num_rx);

      /*  printf("RX samples=%ld\n", num_rx); */


        if(ret!=UHD_ERROR_NONE)
        {

            fprintf(stderr,
            "USRP receive error\n");

            break;

        }

//新IQ调试
    /*    if(num_rx > 0)
        {
            static int rx_debug_count = 0;
            rx_debug_count++;

            if(rx_debug_count % 50 == 0)
            {
                float max_amp = 0.0f;
                float avg_amp = 0.0f;

                float max_i = 0.0f;
                float max_q = 0.0f;

                size_t near_full = 0;

                for(size_t k = 0; k < num_rx; k++)
                {

                    float i_val = fabsf(crealf(buff[k]));
                    float q_val = fabsf(cimagf(buff[k]));

                    float amp = cabsf(buff[k]);

                    if(i_val > max_i)
                        max_i = i_val;

                    if(q_val > max_q)
                        max_q = q_val;

                    if(amp > max_amp)
                        max_amp = amp;

                    avg_amp += amp;


                    if(i_val > 0.95f || q_val > 0.95f)
                        near_full++;
                }

                avg_amp /= (float)num_rx;

                printf(
                    "[RX RAW] samples=%zu "
                    "maxAmp=%.4f "
                    "avgAmp=%.4f "
                    "maxI=%.4f "
                    "maxQ=%.4f "
                    "nearFull=%zu/%zu (%.2f%%)\n",
                    num_rx,
                    max_amp,
                    avg_amp,
                    max_i,
                    max_q,
                    near_full,
                    num_rx,
                    100.0 * (double)near_full / (double)num_rx
                );

                fflush(stdout);
            }

            usrp_callback(buff, num_rx);
        }  */

       if(num_rx>0)
        {

            usrp_callback(
                buff,
                num_rx);

        }

    }

    return 0;

}


int usrp_transmit(usrp_args_t *args)
{
    printf("enter usrp_transmit\n");

/*
    printf("tx metadata=%p\n",
            args->tx_md);


    printf("tx samples=%ld\n",
            args->tx_samples);
*/

    float tx_max = 0.0f;
    size_t i;
    for (i = 0; i < args->tx_samples; i++)
    {
        float i_value;
        float q_value;
        float amplitude;

        i_value = args->tx_buffer[2 * i];
        q_value = args->tx_buffer[2 * i + 1];

        amplitude = sqrtf(
                i_value * i_value +
                q_value * q_value);

        if (amplitude > tx_max)
        {
            tx_max = amplitude;
        }
    }
    //printf("TX IQ max=%f\n", tx_max);


    if(args == NULL)
    {
        fprintf(stderr, "USRP args is NULL\n");
        return -1;
    }

    if(args->tx_samples == 0)
    {
        fprintf(stderr, "No TX samples available\n");
        return -1;
    }


    size_t num_samps_sent = 0;


    /*
     * UHD要求的buffer数组
     */
    const void *buffs[1];

    buffs[0] = args->tx_buffer;


    /*
     * 只发送实际IQ长度
     */
    printf(
    "TX send length=%ld\n",
    args->tx_samples);

    uhd_error err = uhd_tx_streamer_send(
        args->tx_streamer,
        buffs,
        args->tx_samples,
        &args->tx_md,
        3.0,
        &num_samps_sent);
    printf("TX sent %zu / %zu, err=%d\n",
           num_samps_sent, args->tx_samples, (int)err);

   /* uhd_tx_streamer_send(
            args->tx_streamer,
            buffs,
            args->tx_samples,
            &args->tx_md,
            3.0,
            &num_samps_sent);  */

  /*  printf(
        "TX sent %zu / %zu samples\n",
        num_samps_sent,
        args->tx_samples
    );
    printf(
"TX freq=%ld rate=%f gain=%d\n",
args->freq,
args->rate,
args->gain);
    printf(
"RX freq=%ld rate=%f\n",
usrp_dev.freq,
usrp_dev.rate); */
    return 0;
}


void closeUsrp(void)
{


    usrp_status=0;



    if(usrp_dev.rx_streamer)
    {

        uhd_rx_streamer_free(
            &usrp_dev.rx_streamer);

    }



    if(usrp_dev.rx_md)
    {

        uhd_rx_metadata_free(
            &usrp_dev.rx_md);

    }



    if(usrp_dev.usrp)
    {

        uhd_usrp_free(
            &usrp_dev.usrp);

    }



    fprintf(stderr,
    "USRP closed\n");

}



void usrp_transfer_data(usrp_args_t *args, const message_format *mf) {


    int i;

    args->tx_samples =
        mf->complex_length;

    for(i=0;i<args->tx_samples;i++)
    {

        float re;
        float im;

        re =
        mf->out_r[i]/128.0f;


        im =
        mf->out_i[i]/128.0f;


        args->tx_buffer[i]
            =
                re + im * I;

    }


    printf(
        "TX IQ samples = %ld\n",
        args->tx_samples
    );

}

//TX线程

volatile int tx_ready=0;
void *usrp_tx_thread(void *arg)
{


    printf("TX thread start\n");

    while(1)
    {


        pthread_mutex_lock(&tx_mutex);


        while(tx_ready==0)
        {
            pthread_cond_wait(
                &tx_cond,
                &tx_mutex);
        }


        tx_ready=0;


        pthread_mutex_unlock(&tx_mutex);

 //原代码
        for(int i=0;i<100;i++)
        {

            usrp_transmit(&usrp_dev);

            sleep(1);

        }


        usrp_dev.tx_samples=0;

        printf("TX finished\n");
       /* while (1)
        {
            if (usrp_dev.tx_samples > 0)
            {
                usrp_transmit(&usrp_dev);
            }

            usleep(1000);
        }  */
           /* usrp_transmit(&usrp_dev);
            printf("TX finished\n"); */
    }

    return NULL;
}

void prepare_tx_packet()
{




    memset(&tx_mf,0,sizeof(tx_mf));


    generate_pkt(&tx_mf);


    merge_elements(&tx_mf);


    modulate(&tx_mf);

    pthread_mutex_lock(&tx_mutex);


    usrp_transfer_data(
            &usrp_dev,
            &tx_mf);


    tx_ready=1;


    pthread_mutex_unlock(&tx_mutex);


}

//RX线程
void *usrp_rx_thread(void *arg)
{
    printf("RX thread start\n");

    int ret;
    ret = runUsrpSample();

    printf("RX thread exit ret=%d\n",ret);

    return NULL;
}


#endif