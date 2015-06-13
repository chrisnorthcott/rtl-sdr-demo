/*
	RTL SDR Demo Application

	Chris Northcott, Jun 2015

*/

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <rtl-sdr.h>
#include <signal.h>
#include <pthread.h>

#define USAGE(x) \
	fprintf(stderr,x); \
	fprintf(stderr,"\n");

uint32_t count;
char vid[256], pid[256], ser[256];
rtlsdr_dev_t *device;

bool agc = false;
float gain = 0;
int *validgainvalues;
int gaincount;
short ppm = 0;
int frequency = 100000000;
int samplerate = 1024000;
int opt;
int deviceindex = 0;

uint8_t *databuffer;
pthread_t reader_thread;
pthread_cond_t buffer_ready;
pthread_mutex_t buffer_lock;
struct sigaction sa;

void usage()
{
	USAGE("-d <n>: \tselect device index");
	USAGE("-a:\t\tturn on tuner AGC");
	USAGE("-g <dB>: \tset gain");
	USAGE("-p <ppm>: \tset frequency correction");
	USAGE("-f <Hz>: \tset initial frequency");
	USAGE("-S <Hz>: \tset sample rate");
	exit(-1);
}

static void sighandler(int signal)
{
	fprintf(stderr, "We get signal! ");
	if(signal == SIGKILL || signal == SIGTERM)
	{
		fprintf(stderr, "Someone set up us the bomb!");
	}
	rtlsdr_cancel_async(device);
	fprintf(stderr, "\n");
	exit(1);
}

/*
	We're called by rtlsdr_read_async when it's done.

	Some multithreaded craziness from this point in, so hold on tight.
*/

//Some variables for demonstration purposes - we will remove these in Branch 06
int _pkts;
long int _datalen;

static void reader_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	_datalen += len;
	fprintf(stderr, "\r%d packets, %ldMiB", ++_pkts, (_datalen / 1000) / 1000);
}

/*
	We have to define a main loop in the main thread, otherwise
	it will exit prematurely pulling out libusb from under librtlsdr.

	We don't want to spend 3 hours wondering debugging cb_transfer_status
	error messages. I didn't. Honest. Nuh-uh. Why would I do that?
*/

static void main_thread_func()
{
	while(1)
	{
		//process samples
	}
}

/*
	Acquisition thread entry point.
*/
void *reader_thread_func(void *arg)
{
	while(1)
	{
		//Lock the buffer_lock mutex while we are writing to the
		//  buffer.
		pthread_mutex_lock(&buffer_lock);

		//Read asynchronously from the device. Once complete,
		//  librtlsdr will call reader_callback
		rtlsdr_read_async(device, reader_callback, NULL, 0, 0); 

		//Signal to any threads that might be subscribed via
		//  pthread_cond_wait (#06) that the buffer is ready for reading.
		pthread_cond_signal(&buffer_ready);

		//We are done with the buffer, unlock the mutex.
		pthread_mutex_unlock(&buffer_lock);
	}
	return NULL;
}

/*
	Given a user-supplied gain value (eg "10.5"), find the
	nearest valid gain value.

	Gain value is actual-dB value * 10.
*/
int to_gain_value(int requested)
{
	/* Make sure validgainvalues has been populated. */
	if(validgainvalues == NULL)
	{
		return 0;
	}

	/* Check for gainvalues that are too high. */
	if(requested > validgainvalues[gaincount - 1])
	{
		return validgainvalues[gaincount - 1];
	}
	
	/* Start at 0 and work our way up.
		As soon as requested < gainval we have our valid
		gain value. */
	for(int i = 0; i < gaincount; i++)
	{
		if(requested < validgainvalues[i])
		{
			return validgainvalues[i];
		}
	}

	return 0;
}

void init_acquisition()
{
	pthread_create(&reader_thread, NULL, reader_thread_func, NULL);
}

void init_rtlsdr()
{
	count = rtlsdr_get_device_count();
	if(count == 0)
	{
		fprintf(stderr, "Could not find any RTL-SDR devices.\n");
		exit(-1);
	}
	fprintf(stdout, "We found %d RTL-SDR device(s).\n", count);

	/*
		Get info about the attached device(s).
	*/
	for(int i = 0; i < count; i++)
	{
		rtlsdr_get_device_usb_strings(i, vid, pid, ser);
		char selected = (i == deviceindex)? '*' : ' ';
		fprintf(stdout, "#%d%c\t%s\t%s\t%s\n",
			i, selected, vid, pid, ser);
	}

	/*
		Open the device selected by the user.
	*/
	if(rtlsdr_open(&device, deviceindex) < 0)
	{
		fprintf(stderr, "Couldn't open RTLSDR #%d: %s (%d)\n",
			deviceindex, strerror(errno), errno);
		exit(-1);
	}

	/* 
		Get valid gain values as reported by the device.
	*/
	gaincount = rtlsdr_get_tuner_gains(device, NULL);
	validgainvalues = malloc(sizeof(int) * gaincount);
	rtlsdr_get_tuner_gains(device, validgainvalues);
	fprintf(stdout, "Valid gain values: \n");
	for(int i = 0; i < gaincount; i++)
	{
		fprintf(stdout, "%.2fdB\n", (float)(validgainvalues[i] / 10.0f));
	}

	/*
		Set up gain and turn on baseband (tuner) AGC if required.
	*/
	if(agc)
	{
		fprintf(stdout, "Turning on tuner AGC\n");
		rtlsdr_set_agc_mode(device, 1);
	}else{
		fprintf(stdout, "Setting gain to %.2fdB (actual: %.2fdB)\n", 
			gain, (float)to_gain_value(gain * 10) / 10);
		rtlsdr_set_tuner_gain(device, to_gain_value(gain * 10));
	}

	/*
		Set PPM error.
	*/
	fprintf(stdout, "Setting PPM error to %d ppm\n", ppm);
	rtlsdr_set_freq_correction(device, ppm);
	
	/*
		Set sample rate.
	*/
	fprintf(stdout, "Setting sample rate to %d Hz\n", samplerate);
	if(rtlsdr_set_sample_rate(device, samplerate) == -EINVAL)
	{
		fprintf(stderr, "Invalid sample rate chosen.\n");
		exit(-1);
	}

	/*
		Set the centre frequency on startup.
	*/
	fprintf(stdout, "Setting startup frequency to %d Hz\n", frequency);
	rtlsdr_set_center_freq(device, frequency);

	/* 
		Reset the device with the new options.
		
		We can now read samples off with rtlsdr_read_async
	*/
	rtlsdr_reset_buffer(device);
	
}

int main(int argc, char **argv)
{
	/*
		Parse command line options
	*/

	while((opt = getopt(argc, argv, "d:ag:p:f:S:")) != -1)
	{
		switch(opt)
		{
			case 'd':	deviceindex = atoi(optarg);
					break;
			case 'a':	agc = true;
					break;
			case 'g':	gain = atof(optarg);
					break;
			case 'p':	ppm = atoi(optarg);
					break;
			case 'f':	frequency = atoi(optarg);
					break;
			case 'S':	samplerate = atoi(optarg);
					break;
			default:
					usage();
		}
	}

	pthread_mutex_init(&buffer_lock, NULL);
	pthread_cond_init(&buffer_ready, NULL);

        sa.sa_handler = sighandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        sigaction(SIGPIPE, &sa, NULL);

	init_rtlsdr();	
	init_acquisition();
	main_thread_func();

	rtlsdr_close(device);
}
