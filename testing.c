//
// Created by jiaxv on 2022/10/29.
//
#include <stdio.h>


/*
 *  Copyright (c) 2015 Thierry Leconte
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
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#ifdef HAVE_LIBACARS
#include <libacars/version.h>
#endif
#include "acarsdec.h"

#define UPLINK 220
#define DOWNLINK 210

extern void build_label_filter(char *arg);

channel_t channel[MAXNBCHANNELS];
unsigned int nbch;

char *idstation = NULL;
int inmode = 0;
int verbose = 0;
int outtype = OUTTYPE_STD;
int netout = NETLOG_NONE;
int airflt = 0;
int mdly=600;
int hourly = 0;
int daily = 0;

#ifdef WITH_RTL
int gain = 1000;
int ppm = 0;
#endif
char *Rawaddr = NULL;
char *logfilename = NULL;

int test(){
    fprintf(stderr , "Hello Testing\n");

    return 0;
}

static void sighandler(int signum)
{
#ifdef DEBUG
    fprintf(stderr, "receive signal %d exiting\n", signum);
	SndWriteClose();
#endif
    exit(1);
}

int startRecv(int v , int mode, char * addr, char * ppm_i, char * index, char * freq){
    printf("%s\n", freq);
    int res, n;
    struct sigaction sigact;
    char sys_hostname[HOST_NAME_MAX+1];

    gethostname(sys_hostname, sizeof(sys_hostname));
    idstation = strndup(sys_hostname, 32);

    verbose = v;
    airflt = mode == UPLINK ? 2 : 1;
    //printf("MOOOODDDE   %d  %d\n", airflt, mode);
    Rawaddr = addr;
    netout = NETLOG_JSON;
    ppm = atoi(ppm_i);
    //printf("%d  %s %d  %s %s  \n", airflt, Rawaddr, ppm, index, freq);
    res = preinitRtl_lib(index, freq);

    res = initOutput(logfilename, Rawaddr);
    if (res) {
        fprintf(stderr, "Unable to init output\n");
        exit(res);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);

    for (n = 0; n < nbch; n++) {
        channel[n].chn = n;

        res = initMsk(&(channel[n]));
        if (res)
            break;
        res = initAcars(&(channel[n]));
        if (res)
            break;
    }

    res = runRtlSample();

    for (n = 0; n < nbch; n++){
        deinitAcars(&(channel[n]));
    }

    fprintf(stderr, "exiting ...\n");
    exit(res);
}

int he(int argc, char **argv)
{
    int c;
    int res, n;
    struct sigaction sigact;
    char sys_hostname[HOST_NAME_MAX+1];
    char *lblf=NULL;

    gethostname(sys_hostname, sizeof(sys_hostname));
    idstation = strndup(sys_hostname, 32);

    res = 0;
    while ((c = getopt(argc, argv, "HDvarfsRo:t:g:UDp:n:N:j:l:c:i:L:G:b:")) != EOF) {

        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 'o':
                outtype = atoi(optarg);
                break;
            case 't':
                mdly = atoi(optarg);
                break;
            case 'b':
                lblf=optarg;
                break;
#ifdef WITH_RTL
            case 'r':
                //res = initRtl(argv, optind);
                inmode = 3;
                break;
            case 'p':
                ppm = atoi(optarg);
                break;
            case 'g':
                gain = atoi(optarg);
                break;
#endif
            case 'n':
                Rawaddr = optarg;
                netout = NETLOG_PLANEPLOTTER;
                break;
            case 'N':
                Rawaddr = optarg;
                netout = NETLOG_NATIVE;
                break;
            case 'j':
                Rawaddr = optarg;
                netout = NETLOG_JSON;
                break;
            case 'D':
                airflt = 1;
                break;
            case 'U':
                airflt = 2;
                break;
            case 'l':
                logfilename = optarg;
                break;
            case 'H':
                hourly = 1;
                break;
            case 'i':
                free(idstation);
                idstation = strndup(optarg, 32);
                break;

            default:
                return 0;
        }
    }


    if (res) {
        fprintf(stderr, "Unable to init input\n");
        exit(res);
    }

    if(hourly && daily) {
        fprintf(stderr, "Options: -H and -D are exclusive\n");
        exit(1);
    }

    build_label_filter(lblf);

    res = initOutput(logfilename, Rawaddr);
    if (res) {
        fprintf(stderr, "Unable to init output\n");
        exit(res);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);

    for (n = 0; n < nbch; n++) {
        channel[n].chn = n;

        res = initMsk(&(channel[n]));
        if (res)
            break;
        res = initAcars(&(channel[n]));
        if (res)
            break;
    }

    if (res) {
        fprintf(stderr, "Unable to init internal decoders\n");
        exit(res);
    }

#ifdef DEBUG
    if (inmode !=2 ) {
		initSndWrite();
	}
#endif

    if (verbose)
        fprintf(stderr, "Decoding %d channels\n", nbch);

    /* main decoding  */
    switch (inmode) {
#ifdef WITH_RTL
        case 3:
            res = runRtlSample();
            break;
#endif
        default:
            res = -1;
    }

    for (n = 0; n < nbch; n++)
        deinitAcars(&(channel[n]));

    fprintf(stderr, "exiting ...\n");
    exit(res);

}