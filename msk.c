/*
 *  Copyright (c) 2017 Thierry Leconte
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "acarsdec.h"

pthread_mutex_t chmtx;
pthread_cond_t chprcd,chcscd;
int chmsk,tmsk;

#define FLEN ((INTRATE/1200)+1)
static float h[2*FLEN];

int initMsk(channel_t * ch)
{

    int i;

    ch->MskPhi = ch->MskClk = 0;
    ch->MskS = 0;

    ch->MskDf = 0;

    ch->idx = 0;
    ch->inb = calloc(FLEN, sizeof(float complex));

    for (i = 0; i < FLEN; i++) {
        if(ch->chn==0)  {
            h[i] = h[i+FLEN]= cosf(2.0*M_PI*600.0/INTRATE*(i-FLEN/2));
        }

        ch->inb[i] = 0;
    }

    return 0;
}

static inline void putbit(float v, channel_t * ch, int n)
{
    /*
    int bit;

    if (v > 0)
        bit = 1;
    else
        bit = 0;

    static unsigned long bit_cnt = 0;

    bit_cnt++;

    if (bit_cnt <= 300)
    {
        printf("%d", bit);

        if (bit_cnt % 8 == 0)
            printf(" ");

        if (bit_cnt % 64 == 0)
            printf("\n");

        fflush(stdout);
    }



    ch->outbits >>= 1;

    if (bit)
        ch->outbits |= 0x80;


    ch->nbits--;



    if (ch->nbits <= 0)
    {
        decodeAcars(ch, n);
    }
*/

    //下屬爲最初代碼
    ch->outbits >>= 1;
    if (v > 0) {
        ch->outbits |= 0x80;
    }
    ch->nbits--;
    if (ch->nbits <= 0){
        decodeAcars(ch, n);
    }

}

const float PLLG=38e-4;
const float PLLC=0.52;
void demodMSK(channel_t *ch,int len)
{
    /* MSK demod */
    int n;
    int idx=ch->idx;
    double p=ch->MskPhi;


    for(n=0;n<len;n++) {
        float in;
        double s;
        float complex v;
        int j,o;

        /* VCO */
        s = 1800.0/INTRATE*2.0*M_PI + ch->MskDf;


        p+=s;
        if (p >= 2.0*M_PI) p -= 2.0*M_PI;

        /* mixer */
        in = ch->dm_buffer[n];
#ifdef DEBUG
        if(ch->chn==1) SndWrite(&in);
#endif
        ch->inb[idx] = in * cexp(-p*I);
        idx=(idx+1)%FLEN;


        /* bit clock */
        ch->MskClk+=s;
        if (ch->MskClk >=3*M_PI/2.0) {
            double dphi;
            float vo,lvl;

            ch->MskClk -= 3*M_PI/2.0;

            /* matched filter */
            o=FLEN-idx;
            v=0;
            for (j = 0; j < FLEN; j++,o++) {
                v += h[o]*ch->inb[j];
            }
            /* normalize */
            lvl=cabsf(v);
            v/=lvl+1e-6;
            ch->Msklvl = 0.99 * ch->Msklvl + 0.01*lvl/5.2;

            if(ch->MskS&1) {
                vo=cimagf(v);
                if(vo>=0) dphi=-crealf(v); else dphi=crealf(v);
            } else {
                vo=crealf(v);
                if(vo>=0) dphi=cimagf(v); else dphi=-cimagf(v);
            }

         /*  //下述为调试代码
            static unsigned long decision_count = 0;

            static unsigned long strong_bits = 0;
            static unsigned long weak_bits = 0;

            static float abs_vo_sum = 0.0f;
            static float abs_vo_min = 999.0f;
            static float abs_vo_max = 0.0f;

            float abs_vo = fabsf(vo);

            decision_count++;

            abs_vo_sum += abs_vo;

            if(abs_vo < abs_vo_min)
                abs_vo_min = abs_vo;

            if(abs_vo > abs_vo_max)
                abs_vo_max = abs_vo;


            if(abs_vo >= 0.5f)
                strong_bits++;
            else
                weak_bits++;

            if(decision_count % 500 == 0)
            {
                printf(
                    "[MSK QUALITY] "
                    "bits=%lu "
                    "avg|vo|=%.3f "
                    "min=%.3f "
                    "max=%.3f "
                    "strong=%.1f%% "
                    "weak=%.1f%% "
                    "lvl=%.6f "
                    "MskDf=%.6f\n",
                    decision_count,
                    abs_vo_sum / 500.0f,
                    abs_vo_min,
                    abs_vo_max,
                    100.0f * strong_bits / 500.0f,
                    100.0f * weak_bits / 500.0f,
                    lvl,
                    ch->MskDf
                );

                abs_vo_sum = 0.0f;
                abs_vo_min = 999.0f;
                abs_vo_max = 0.0f;

                strong_bits = 0;
                weak_bits = 0;
            }  */

           /*  //调试代码
            static unsigned long demod_bit_count = 0;

            demod_bit_count++;

            if(demod_bit_count % 500 == 0)
            {
                printf(
                    "[MSK] bits=%lu "
                    "vo=%f lvl=%f "
                    "MskDf=%f\n",
                    demod_bit_count,
                    vo,
                    lvl,
                    ch->MskDf
                );
            }
            //上述为调试代码 */

            if(ch->MskS&2) {
                putbit(-vo, ch, n);
            } else {
                putbit(vo, ch, n);
            }
            ch->MskS++;

            /* PLL filter */
            ch->MskDf=PLLC*ch->MskDf+(1.0-PLLC)*PLLG*dphi;
        }
    }

    ch->idx=idx;
    ch->MskPhi=p;

}

