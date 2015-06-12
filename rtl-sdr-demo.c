/*
	RTL SDR Demo Application

	Chris Northcott, Jun 2015

*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <rtl-sdr.h>

uint32_t count;
char vid[256], pid[256], ser[256];

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
		fprintf(stdout, "#%d\t%s\t%s\t%d\n",
			i, vid, pid, serial);
	}
}

int main(int argc, char **argv)
{
	init_rtlsdr();	
}
