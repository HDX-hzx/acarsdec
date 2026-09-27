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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include <unistd.h>
#include <limits.h>

#include "modulator.h"
#ifdef HAVE_LIBACARS
#include <libacars/version.h>
#endif
#include "acarsdec.h"

#ifdef WITH_USRP
#include "usrp.h"
#endif



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
int gain = 80;
int ppm = 0;
#endif

#ifdef WITH_USRP
int rx_gain = 10;    // USRP：单位 dB
int tx_gain = 0;     // USRP：单位 dB
int64_t freq = 131450000;
#endif

char *Rawaddr = NULL;
char *logfilename = NULL;

static void usage(void)
{
	fprintf(stderr,
		"Acarsdec/acarsserv 3.5 Copyright (c) 2017 Thierry Leconte\n");
#ifdef HAVE_LIBACARS
	fprintf(stderr,	"(libacars %s)\n", LA_VERSION);
#endif
	fprintf(stderr,
		"\nUsage: acarsdec  [-v] [-o lv] [-t time] [-A] [-n ipaddr:port] [-l logfile [-H|-D]]");
#ifdef WITH_SNDFILE
	fprintf(stderr, " -f inputwavfile  |");
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		" [-g gain] [-p ppm] -r rtldevicenumber  f1 [f2] ... [fN]");
#endif
	fprintf(stderr, "\n\n");
	fprintf(stderr, " -v\t\t\t: verbose\n");
	fprintf(stderr,
		" -A\t\t\t: don't display uplink messages (ie : only aircraft messages)\n");
	fprintf(stderr,
		"\n -o lv\t\t\t: output format : 0 : no log, 1 : one line by msg, 2 : full (default) , 3 : monitor , 4 : msg JSON, 5 : route JSON\n");
	fprintf(stderr,
		"\n -t time\t\t: set forget time (TTL) in seconds for monitor mode (default=600s)\n");
	fprintf(stderr,
		" -l logfile\t\t: append log messages to logfile (Default : stdout).\n");
	fprintf(stderr,
		" -H\t\t\t: rotate log file once every hour\n");
	fprintf(stderr,
		" -D\t\t\t: rotate log file once every day\n");
	fprintf(stderr,
		" -n ipaddr:port\t\t: send acars messages to addr:port on UDP in planeplotter compatible format\n");
	fprintf(stderr,
		" -N ipaddr:port\t\t: send acars messages to addr:port on UDP in acarsdec native format\n");
	fprintf(stderr,
		" -j ipaddr:port\t\t: send acars messages to addr:port on UDP in acarsdec json format\n");
	fprintf(stderr,
		" -i stationid\t\t: station id used in acarsdec network format.\n");
	fprintf(stderr,
		" -b filter\t\t: filter output by label (ex: -b \"H1:Q0\" : only output messages  with label H1 or Q0)\n\n");

#ifdef WITH_SNDFILE
	fprintf(stderr,
		" -f inputwavfile\t: decode from a wav file at %d sampling rate\n",INTRATE);
#endif
#ifdef WITH_RTL
	fprintf(stderr,
		" -g gain\t\t: set rtl preamp gain in tenth of db (ie -g 90 for +9db). By default use AGC\n");
	fprintf(stderr, " -p ppm\t\t\t: set rtl ppm frequency correction\n");
	fprintf(stderr,
		" -r rtldevice f1 [f2]...[f%d]\t: decode from rtl dongle number or S/N rtldevice receiving at VHF frequencies f1 and optionally f2 to f%d in Mhz (ie : -r 0 131.525 131.725 131.825 )\n", MAXNBCHANNELS, MAXNBCHANNELS);
#endif
	fprintf(stderr,
		"\nUp to %d channels may be simultaneously decoded\n", MAXNBCHANNELS);
	exit(1);
}

static void sighandler(int signum)
{
#ifdef DEBUG
	fprintf(stderr, "receive signal %d exiting\n", signum);
	SndWriteClose();
#endif
	exit(1);
}
int main(int argc, char **argv)
{
	int c;
	int res, n;
	struct sigaction sigact;
	char sys_hostname[HOST_NAME_MAX+1];
	char *lblf=NULL;
	message_format *mf;
	mf = malloc(sizeof(message_format));
	if(mf==NULL)
	{
		fprintf(stderr,"malloc mf failed\n");
		exit(1);
	}

	gethostname(sys_hostname, sizeof(sys_hostname));
	idstation = strndup(sys_hostname, 32);

	res = 0;
	while ((c = getopt(argc, argv, "HDvarfsRo:t:g:G:u:UDp:n:N:j:l:c:i:L:b:")) != EOF) {

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

#ifdef WITH_SNDFILE
		case 'f':
		/*	printf("OPTION -f detected\n");
			printf("optarg = '%s'\n", optarg); */
			res = initSoundfile(argv, optind);
			inmode = 2;
			break; 
#endif
#ifdef WITH_RTL
		case 'r':
			//res = initRtl(argv, optind);
            res = preinitRtl_exe(argv, optind);
			inmode = 3;
			break;
		case 'p':
			ppm = atoi(optarg);
			break;

#endif
		case 'g':
#ifdef WITH_USRP
			rx_gain = atoi(optarg);
			printf("[GETOPT RX GAIN] rx_gain=%d dB\n", rx_gain);
#endif
#ifdef WITH_RTL
			gain = atoi(optarg);
#endif
			break;
		case 'G':
#ifdef WITH_USRP
			tx_gain = atoi(optarg);
			printf("[GETOPT TX GAIN] tx_gain=%d dB\n", tx_gain);
#endif
			break;


#ifdef WITH_USRP

		case 'u':

			{
				res=preinitUsrp_lib(optarg);
				inmode=4;
				break;
			}

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
			usage();
		}
	}

	//以下是新增調試
	printf("\n========== GETOPT DEBUG ==========\n");
	printf("argc = %d\n", argc);
	printf("optind = %d\n", optind);

	for (int k = 0; k < argc; k++)
	{
		printf("argv[%d] = '%s'%s\n",
			   k,
			   argv[k],
			   (k == optind) ? "  <-- optind" : "");
	}
	//上述是新增調試


	printf("After getopt gain=%d\n",gain);//调试

	if (inmode == 0) {
		fprintf(stderr, "Need at least one of -a|-f|-r|-R options\n");
		usage();
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
	printf("Decoder initialized\n");


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
#ifdef WITH_SNDFILE
	case 2:
		res = runSoundfileSample();
		break;
#endif
#ifdef WITH_RTL
	case 3:
		res = runRtlSample();
		break;
#endif
#ifdef WITH_USRP

	case 4:
		{

			pthread_t rx_thread;
			pthread_t tx_thread;



			pthread_create(
					&rx_thread,
					NULL,
					usrp_rx_thread,
					NULL);



			pthread_create(
					&tx_thread,
					NULL,
					usrp_tx_thread,
					NULL);



			while(1)
			{


				prepare_tx_packet();


				pthread_mutex_lock(
					&tx_mutex);



				tx_ready=1;


				pthread_cond_signal(
					&tx_cond);



				pthread_mutex_unlock(
					&tx_mutex);



			}


			break;

		}

#endif
	default:
		res = -1;
	}

	for (n = 0; n < nbch; n++)
		deinitAcars(&(channel[n]));

	fprintf(stderr, "exiting ...\n");
	exit(res);

}
