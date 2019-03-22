/*
This example program makes use of the simple
sound library to generate a sine wave and write the
output to sound.wav.
For complete documentation on the library, see:
http://www.nd.edu/~dthain/courses/cse20211/fall2013/wavfile
Go ahead and modify this program for your own purposes.
*/


#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "wavfile.h"

const int NUM_SAMPLES = (WAVFILE_SAMPLES_PER_SECOND*WAVFILE_CHANNELS*2)*WAVFILE_PLAY_SECONDS;

int main()
{
	short waveform[NUM_SAMPLES];
	double frequency = 700.0;
	int volume = 32000;
	int length = NUM_SAMPLES;

	int i;
	for(i=0;i<length/WAVFILE_CHANNELS;i++) {
		double t = (double) i / WAVFILE_SAMPLES_PER_SECOND;
        if (WAVFILE_CHANNELS == 2) {
            waveform[i*2] = volume*sin(frequency*t*2*M_PI);
            waveform[i*2 + 1] = volume*sin(frequency*t*2*M_PI);
        } else {
            waveform[i] = volume*sin(frequency*t*2*M_PI);
        }
	}

	FILE * f = wavfile_open("sound.wav");
	if(!f) {
		printf("couldn't open sound.wav for writing: %s",strerror(errno));
		return 1;
	}

	wavfile_write(f,waveform,length);
	wavfile_close(f);

	return 0;
}
