/*
	RTL SDR Demo Application

	Chris Northcott, Jun 2015

*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <rtl-sdr.h>

uint32_t count;

void init_rtlsdr()
{
	count = rtlsdr_get_device_count();
	if(count == 0)
	{
		fprintf(stderr, "Could not find any RTL-SDR devices.\n");
		exit(-1);
	}
	fprintf(stdout, "We found an RTL-SDR device!\n");
}

int main(int argc, char **argv)
{
	init_rtlsdr();	
}
