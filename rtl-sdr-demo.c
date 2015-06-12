/*
	RTL SDR Demo Application

	Chris Northcott, Jun 2015

*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <rtl-sdr.h>

#define USE_DEVICE 0

uint32_t count;
char vid[256], pid[256], ser[256];
rtlsdr_dev_t *device;

void init_rtlsdr()
{
	count = rtlsdr_get_device_count();
	if(count == 0)
	{
		fprintf(stderr, "Could not find any RTL-SDR devices.\n");
		exit(-1);
	}
	fprintf(stdout, "We found %d RTL-SDR devices.\n", count);

	/*
		Get info about the attached device(s).
	*/
	for(int i = 0; i < count; i++)
	{
		rtlsdr_get_device_usb_strings(i, vid, pid, ser);
		char selected = (i == USE_DEVICE)? '*' : ' ';
		fprintf(stdout, "#%d%c\t%s\t%s\t%d\n",
			i, selected, vid, pid, ser);
	}

	if(rtlsdr_open(&device, USE_DEVICE) < 0)
	{
		fprintf(stderr, "Couldn't open RTLSDR #%d: %s (%d)\n",
			USE_DEVICE, strerror(errno), errno);
		exit(-1);
	}
}

int main(int argc, char **argv)
{
	init_rtlsdr();	
}
