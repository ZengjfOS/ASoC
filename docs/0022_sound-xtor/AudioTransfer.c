/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Copyright (C) 2012-2016 Freescale Semiconductor, Inc. */
/* Copyright 2017-2018 NXP */

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <sound/asound.h>

#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <audio_effects/effect_aec.h>

#include "audio_hardware.h"
#include "config_wm8962.h"
#include "config_wm8958.h"
#include "config_hdmi.h"
#include "config_nullcard.h"
#include "config_spdif.h"
#include "config_cs42888.h"
#include "config_saf775d.h"
#include "config_wm8960.h"
#include "config_sii902x.h"
#include "config_rpmsg.h"
#include "config_wm8524.h"
#include "config_cdnhdmi.h"
#include "config_ak4458.h"
#include "config_ak5558.h"
#include "control.h"
#include "pcm_ext.h"
#include "config_xtor.h"
#include "config_ak4497.h"
#include "config_sgtl5000.h"
#include "config_xtor_pico.h"
#include "config_rt5645.h"
#include "config_micfil.h"

/* ALSA ports for IMX */
#define PORT_MM     0
#define PORT_MM2_UL 0

/*align the definition in kernel for hdmi audio*/
#define HDMI_PERIOD_SIZE       192
#define PLAYBACK_HDMI_PERIOD_COUNT      8

#define ESAI_PERIOD_SIZE       1024
#define PLAYBACK_ESAI_PERIOD_COUNT      2

#define DSD_PERIOD_SIZE       1024
#define PLAYBACK_DSD_PERIOD_COUNT       8

/* number of frames per short period (low latency) */
/* align other card with hdmi, same latency*/
#define SHORT_PERIOD_SIZE       384
/* number of short periods in a long period (low power) */
#define LONG_PERIOD_MULTIPLIER  2
/* number of frames per long period (low power) */
#define LONG_PERIOD_SIZE        192
/* number of periods for low power playback */
#define PLAYBACK_LONG_PERIOD_COUNT  8
/* number of periods for capture */
#define CAPTURE_PERIOD_SIZE  512
/* number of periods for capture */
#define CAPTURE_PERIOD_COUNT 16
/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000

#define DEFAULT_OUT_SAMPLING_RATE 48000

/* sampling rate when using MM low power port */
#define MM_LOW_POWER_SAMPLING_RATE  48000
/* sampling rate when using MM full power port */
#define MM_FULL_POWER_SAMPLING_RATE 48000

#define DSD64_SAMPLING_RATE 2822400
#define DSD_RATE_TO_PCM_RATE 32
// DSD pcm param: 2 channel, 32 bit
#define DSD_FRAMESIZE_BYTES 8

#define LPA_PERIOD_SIZE 192
#define LPA_PERIOD_COUNT 8

// Limit LPA max latency to 300ms
#define LPA_LATENCY_MS 300

#define MM_USB_AUDIO_IN_RATE   16000

#define SCO_RATE 16000
/* audio input device for hfp */
#define SCO_IN_DEVICE AUDIO_DEVICE_IN_BUILTIN_MIC

/* product-specific defines */
#define PRODUCT_DEVICE_PROPERTY "ro.product.device"
#define PRODUCT_NAME_PROPERTY   "ro.product.name"
#define PRODUCT_DEVICE_IMX      "imx"
#define PRODUCT_DEVICE_AUTO     "sabreauto"
#define SUPPORT_CARD_NUM        19

#define IMX8_BOARD_NAME "imx8"
#define IMX7_BOARD_NAME "imx7"
#define DEFAULT_ERROR_NAME_str "0"

#undef NDEBUG 

bool running = true;
struct imx_audio_device adevInst;
struct imx_audio_device *adev = &adevInst;


/*"null_card" must be in the end of this array*/
struct audio_card *audio_card_list[SUPPORT_CARD_NUM] = {
    &wm8958_card,
    &xtor_card,
    &wm8962_card,
    &hdmi_card,
    /* &usbaudio_card, */
    &spdif_card,
    &cs42888_card,
    &saf775d_card,
    &wm8960_card,
    &sii902x_card,
    &rpmsg_card,
    &wm8524_card,
    &cdnhdmi_card,
    &ak4458_card,
    &ak5558_card,
    &ak4497_card,
    &sgtl5000_card,
    &xtor_pico_card,
    &rt5645_card,
    &micfil_card,
    &null_card,
};

struct pcm_config pcm_config_mm_out = {
    .channels = 2,
    .rate = MM_FULL_POWER_SAMPLING_RATE,
    .period_size = LONG_PERIOD_SIZE,
    .period_count = PLAYBACK_LONG_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .avail_min = 0,
};

int lpa_enable = 0;

struct pcm_config pcm_config_sco_out = {
    .channels = 2,
    .rate = SCO_RATE,
    .period_size = LONG_PERIOD_SIZE,
    .period_count = PLAYBACK_LONG_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .avail_min = 0,
};

struct pcm_config pcm_config_sco_in = {
    .channels = 2,
    .rate = SCO_RATE,
    .period_size = CAPTURE_PERIOD_SIZE,
    .period_count = CAPTURE_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .avail_min = 0,
};


/**
 * NOTE: when multiple mutexes have to be acquired, always respect the following order:
 *        hw device > in stream > out stream
 */
extern int pcm_state(struct pcm *pcm);

/* The enable flag when 0 makes the assumption that enums are disabled by
 * "Off" and integers/booleans by 0 */
static int set_route_by_array(struct mixer *mixer, struct route_setting *route,
                              int enable)
{
    struct mixer_ctl *ctl = NULL;
    unsigned int i, j;

    if(!mixer) return 0;
    if(!route) return 0;
    /* Go through the route array and set each value */
    i = 0;
    while (route[i].ctl_name) {
        ctl = mixer_get_ctl_by_name(mixer, route[i].ctl_name);
        if (!ctl)
            return -EINVAL;

        if (route[i].strval) {
            if (enable)
                mixer_ctl_set_enum_by_string(ctl, route[i].strval);
            else
                mixer_ctl_set_enum_by_string(ctl, "Off");
        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
                if (enable)
                    mixer_ctl_set_value(ctl, j, route[i].intval);
                else
                    mixer_ctl_set_value(ctl, j, 0);
            }
        }
        i++;
    }

    return 0;
}

static int get_card_for_name(const char *name, int *card_index)
{
    int i;
    int card = -1;

    if (!name) {
        ALOGE("%s: Invalid audio device or card name", __func__);
        return -1;
    }

    for (i = 0; i < MAX_AUDIO_CARD_NUM; i++) {
        ALOGE("%s: check value: %d, current card name: %s,  name: %s", __func__, strcmp(audio_card_list[i]->name, name), audio_card_list[i]->name, name);
        if (strcmp(audio_card_list[i]->name, name) == 0) {
              card = audio_card_list[i]->card;
              break;
        }
    }

    if (card_index)
        *card_index = i;

    ALOGD("%s: name: %s, card: %d", __func__, name, card);
    return card;
}

static int get_card_for_device(struct imx_audio_device *adev, int device, unsigned int flag, int *card_index)
{
    int i;
    int card = -1;

    if (flag == PCM_OUT ) {
        for(i = 0; i < MAX_AUDIO_CARD_NUM; i++) {
            if(audio_card_list[i]->supported_out_devices & device) {
                  card = audio_card_list[i]->card;
                  break;
            }
        }
    } else {
        for(i = 0; i < MAX_AUDIO_CARD_NUM; i++) {
            if(adev->card_list[i]->supported_in_devices & device & ~AUDIO_DEVICE_BIT_IN) {
                  card = adev->card_list[i]->card;
                  break;
            }
        }
    }
    if (card_index != NULL)
        *card_index = i;
    return card;
}

static int pcm_write_wrapper(struct pcm *pcm, const void * buffer, size_t bytes, int flags)
{
    int ret = 0;

    if(flags & PCM_MMAP)
         ret = pcm_mmap_write(pcm, (void *)buffer, bytes);
    else
         ret = pcm_write(pcm, (void *)buffer, bytes);

    if(ret !=0) {
         ALOGW("ret %d, pcm write %zu error %s", ret, bytes, pcm_get_error(pcm));
        if (lpa_enable == 1)
            return ret;

         switch(pcm_state(pcm)) {
              case PCM_STATE_SETUP:
              case PCM_STATE_XRUN:
                   ret = pcm_prepare(pcm);
			  	   ALOGW("ret %d, pcm_prepare\n",ret);
                   if(ret != 0) return ret;
                   break;
              default:
                   return ret;
         }

         if(flags & PCM_MMAP)
            ret = pcm_mmap_write(pcm, (void *)buffer, bytes);
         else
            ret = pcm_write(pcm, (void *)buffer, bytes);
    }

    return ret;
}

FILE* outputPcmSampleFile_rx_be;
FILE* outputPcmSampleFile_rx_af;
char outputFilename_rx_be[50] = "/data/misc/bluedroid/output_sample_rx_be.pcm";
char outputFilename_rx_af[50] = "/data/misc/bluedroid/output_sample_rx_af.pcm";

static void* sco_rx_task(void *arg)
{
    int ret = 0;
    uint32_t size = 0;
    uint32_t frames = 0;
    uint32_t out_frames = 0;
    uint32_t out_size = 0;
    uint8_t *buffer = NULL;
    struct pcm *out_pcm = NULL;
    struct imx_audio_device *adev = (struct imx_audio_device *)arg;
    int flag = 0;
    unsigned int card = 0;
    struct pcm_config play_config;
    struct pcm *pcm_play;
    uint8_t *out_buffer = NULL;

    if(adev == NULL)
        return NULL;

    frames = pcm_config_sco_in.period_size;
    size = pcm_frames_to_bytes(adev->pcm_sco_rx, frames);
    buffer = (uint8_t *)malloc(size);
    if(buffer == NULL) {
        ALOGE("sco_rx_task, malloc %d bytes failed", size);
        return NULL;
    }

    ALOGI("enter sco_rx_task, pcm_sco_rx frames %d, szie %d", frames, size);

    play_config = pcm_config_sco_out;
    play_config.rate = 48000;
    play_config.period_size = pcm_config_sco_out.period_size * adev->cap_config.rate / pcm_config_sco_out.rate;
    ALOGW("rx_task open mic, card %d, port %d", 0, 0);
    ALOGW("rate %d, channel %d, period_size 0x%x",
        play_config.rate, play_config.channels, play_config.period_size);

    pcm_play = pcm_open(0, 0, PCM_OUT, &play_config);
    if (pcm_play && !pcm_is_ready(pcm_play)) {
        ret = -1;
        ALOGE("cannot open pcm_cap: %s", pcm_get_error(pcm_play));
        goto exit;
    }

    ALOGW("rx_task get out_buffer");
    out_frames = play_config.period_size * 2;
    out_size = pcm_frames_to_bytes(adev->pcm_sco_tx, out_frames);
    out_buffer = (uint8_t *)malloc(out_size);
    if(out_buffer == NULL) {
        free(buffer);
        ALOGE("sco_tx_task, malloc out_buffer %d bytes failed", out_size);
        return NULL;
    }

	outputPcmSampleFile_rx_be = fopen(outputFilename_rx_be, "ab");
	outputPcmSampleFile_rx_af = fopen(outputFilename_rx_af, "ab");

    while(adev->b_sco_rx_running) {
        ret = pcm_read(adev->pcm_sco_rx, buffer, size);
        if(ret) {
            ALOGE("sco_rx_task, pcm_read ret %d, size %d, %s",
                ret, size, pcm_get_error(adev->pcm_sco_rx));
            usleep(2000);
            continue;
        }

        ALOGD("sco_rx_task, resample_from_input");

        /*
        frames = pcm_config_sco_in.period_size;
        adev->rsmpl_sco_rx->resample_from_input(adev->rsmpl_sco_rx,
                                                (int16_t *)buffer,
                                                (size_t *)&frames,
                                                (int16_t *)out_buffer,
                                                (size_t *)&out_frames);
        */

        ALOGD("sco_rx_task, resample_from_input, in frames %d, %d, out_frames %zu, %d",
            pcm_config_sco_in.period_size, frames,
            pcm_config_mm_out.period_size * 2, out_frames);
        usleep(2000);

        /*
        out_size = pcm_frames_to_bytes(pcm_play, out_frames);
        ret = pcm_write_wrapper(pcm_play, out_buffer, out_size, flag);
        if(ret) {
            ALOGE("sco_rx_task, pcm_write ret %d, size %d, %s",
                ret, out_size, pcm_get_error(out_pcm));
            usleep(2000);
        }
        */
    }

	if (outputPcmSampleFile_rx_be) {
	    fclose(outputPcmSampleFile_rx_be);
	}
	if (outputPcmSampleFile_rx_af) {
        fclose(outputPcmSampleFile_rx_af);
    }

    free(out_buffer);
exit:
    free(buffer);
    ALOGI("leave sco_rx_task");

    return NULL;
}

FILE* outputPcmSampleFile_tx_be;
FILE* outputPcmSampleFile_tx_af;
char outputFilename_tx_be[50] = "/data/misc/bluedroid/output_sample_tx_be.pcm";
char outputFilename_tx_af[50] = "/data/misc/bluedroid/output_sample_tx_af.pcm";

static void* sco_tx_task(void *arg)
{
    int ret = 0;
    uint32_t size = 0;
    uint32_t frames = 0;
    uint8_t *buffer = NULL;
    uint32_t out_frames = 0;
    uint32_t out_size = 0;
    uint8_t *out_buffer = NULL;
    int i = 0;

    struct imx_audio_device *adev = (struct imx_audio_device *)arg;
    if(adev == NULL)
        return NULL;

    frames = adev->cap_config.period_size;
    size = pcm_frames_to_bytes(adev->pcm_cap , frames);
    buffer = (uint8_t *)malloc(size);
    if(buffer == NULL) {
        ALOGE("sco_tx_task, malloc %d bytes failed", size);
        return NULL;
    }

    ALOGI("enter sco_tx_task, pcm_cap frames %d, szie %d", frames, size);

    out_frames = pcm_config_sco_out.period_size * 2;
    out_size = pcm_frames_to_bytes(adev->pcm_sco_tx, out_frames);
    out_buffer = (uint8_t *)malloc(out_size);
    if(out_buffer == NULL) {
        free(buffer);
        ALOGE("sco_tx_task, malloc out_buffer %d bytes failed", out_size);
        return NULL;
    }

	outputPcmSampleFile_tx_be = fopen(outputFilename_tx_be, "ab");
	outputPcmSampleFile_tx_af = fopen(outputFilename_tx_af, "ab");

    while(adev->b_sco_tx_running) {
        ret = pcm_read(adev->pcm_cap, buffer, size);
        if(ret) {
            ALOGI("sco_tx_task, pcm_read ret %d, size %d, %s",
                ret, size, pcm_get_error(adev->pcm_cap));
            continue;
        }

		if (outputPcmSampleFile_tx_be) {
   			 fwrite((buffer), 1, (size_t)size, outputPcmSampleFile_tx_be);
			 ALOGI("outputPcmSampleFile_tx_be write Ok\n");
  		}	

        frames = adev->cap_config.period_size;
        out_frames = pcm_config_sco_out.period_size * 2;
        adev->rsmpl_sco_tx->resample_from_input(adev->rsmpl_sco_tx,
                                                (int16_t *)buffer,
                                                (size_t *)&frames,
                                                (int16_t *)out_buffer,
                                                (size_t *)&out_frames);

        ALOGD("sco_tx_task, resample_from_input, in frames %d, %d, out_frames %d, %d",
            adev->cap_config.period_size, frames,
            pcm_config_sco_out.period_size * 2, out_frames);

        for(i = 0; i < out_frames; i++) {
            *((int16_t *)out_buffer + (i * 2)) = *((int16_t *)out_buffer + (i * 2 + 1));
        }

        out_size = pcm_frames_to_bytes(adev->pcm_sco_tx, out_frames);
        ret = pcm_write(adev->pcm_sco_tx, out_buffer, out_size);
        if(ret) {
            ALOGE("sco_tx_task, pcm_write ret %d, size %d, %s",
                ret, out_size, pcm_get_error(adev->pcm_sco_tx));
        }

		if (outputPcmSampleFile_tx_af) {
   			 fwrite((out_buffer), 1, (size_t)out_size, outputPcmSampleFile_tx_af);
			 ALOGI("outputPcmSampleFile_tx_af write Ok\n");
  		}
    }

    free(buffer);
    free(out_buffer);
    ALOGI("leave sco_tx_task");

	if (outputPcmSampleFile_tx_be) {
	  fclose(outputPcmSampleFile_tx_be);
	}
	if (outputPcmSampleFile_tx_af) {
    	fclose(outputPcmSampleFile_tx_af);
    }

    return NULL;
}

static int sco_release_resource()
{
    if(NULL == adev)
        return -1;

    // release rx resource
    if(adev->tid_sco_rx) {
        adev->b_sco_rx_running = false;
        pthread_join(adev->tid_sco_rx, NULL);
    }

    if(adev->pcm_sco_rx) {
        pcm_close(adev->pcm_sco_rx);
        adev->pcm_sco_rx = NULL;
    }

    if(adev->rsmpl_sco_rx) {
        release_resampler(adev->rsmpl_sco_rx);
        adev->rsmpl_sco_rx = NULL;
    }

    // release tx resource
    if(adev->tid_sco_tx) {
        adev->b_sco_tx_running = false;
        pthread_join(adev->tid_sco_tx, NULL);
    }

    if(adev->pcm_sco_tx) {
        pcm_close(adev->pcm_sco_tx);
        adev->pcm_sco_tx = NULL;
    }

    if(adev->rsmpl_sco_tx) {
        release_resampler(adev->rsmpl_sco_tx);
        adev->rsmpl_sco_tx = NULL;
    }

    if(adev->pcm_cap){
        pcm_close(adev->pcm_cap);
        adev->pcm_cap = NULL;
    }

    return 0;
}

static int sco_task_create()
{
    int ret = 0;
    unsigned int port = 0;
    unsigned int card = 0;
    pthread_t tid_sco_rx = 0;
    pthread_t tid_sco_tx = 0;
    pthread_attr_t attr;
    struct sched_param schParam;

    if(NULL == adev)
        return -1;

    /*=============== create rx task ===============*/
    ALOGI("prepare bt rx task");
    //open sco card for read
    card = get_card_for_name(BT_SAI_CARD_NAME, NULL);
    pcm_config_sco_in.period_size =  pcm_config_mm_out.period_size * pcm_config_sco_in.rate / pcm_config_mm_out.rate;
    pcm_config_sco_in.period_count = pcm_config_mm_out.period_count;

    ALOGI("set pcm_config_sco_in.period_size to %d", pcm_config_sco_in.period_size);
    ALOGI("open sco for read, card %d, port %d", card, port);
    ALOGI("rate %d, channel %d, period_size 0x%x, period_count %d",
        pcm_config_sco_in.rate, pcm_config_sco_in.channels,
        pcm_config_sco_in.period_size, pcm_config_sco_in.period_count);

    adev->pcm_sco_rx = pcm_open(1, 0, PCM_IN, &pcm_config_sco_in);
    ALOGI("after pcm open, rate %d, channel %d, period_size 0x%x, period_count %d",
        pcm_config_sco_in.rate, pcm_config_sco_in.channels,
        pcm_config_sco_in.period_size, pcm_config_sco_in.period_count);

    if (adev->pcm_sco_rx && !pcm_is_ready(adev->pcm_sco_rx)) {
        ret = -1;
        ALOGE("cannot open pcm_sco_rx: %s", pcm_get_error(adev->pcm_sco_rx));
        goto error;
    }

    //create resampler
    ret = create_resampler(pcm_config_sco_in.rate,
                           pcm_config_mm_out.rate,
                           2,
                           RESAMPLER_QUALITY_DEFAULT,
                           NULL,
                           &adev->rsmpl_sco_rx);
    if(ret) {
        ALOGI("create_resampler rsmpl_sco_rx failed, ret %d", ret);
        goto error;
    }

    ALOGI("create_resampler rsmpl_sco_rx, in rate %d, out rate %d",
        pcm_config_sco_in.rate, pcm_config_mm_out.rate);

    //create rx task, use real time thread.
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    schParam.sched_priority = 3;
    pthread_attr_setschedparam(&attr, &schParam);

    adev->b_sco_rx_running = true;
    ret = pthread_create(&tid_sco_rx, &attr, sco_rx_task, (void *)adev);
    if(ret) {
        goto error;
    }
    adev->tid_sco_rx = tid_sco_rx;
    ALOGI("sco_rx_task create ret %d, tid_sco_rx %ld", ret, tid_sco_rx);

    /*=============== create tx task ===============*/
    ALOGI("prepare bt tx task");
    //open sco card for write
    card = get_card_for_name(BT_SAI_CARD_NAME, NULL);
    ALOGI("open sco for write, card %d, port %d", card, port);
    ALOGI("rate %d, channel %d, period_size 0x%x",
        pcm_config_sco_out.rate, pcm_config_sco_out.channels, pcm_config_sco_out.period_size);

    adev->pcm_sco_tx = pcm_open(1, 0, PCM_OUT, &pcm_config_sco_out);
    if (adev->pcm_sco_tx && !pcm_is_ready(adev->pcm_sco_tx)) {
        ret = -1;
        ALOGE("cannot open pcm_sco_tx: %s", pcm_get_error(adev->pcm_sco_tx));
        goto error;
    }

    card = get_card_for_device(adev, SCO_IN_DEVICE, PCM_IN, NULL);
    adev->cap_config = pcm_config_sco_out;
    adev->cap_config.rate = 48000;
    adev->cap_config.period_size = pcm_config_sco_out.period_size * adev->cap_config.rate / pcm_config_sco_out.rate;
    ALOGW(" open mic, card %d, port %d", card, port);
    ALOGW("rate %d, channel %d, period_size 0x%x",
        adev->cap_config.rate, adev->cap_config.channels, adev->cap_config.period_size);

    adev->pcm_cap = pcm_open(card, port, PCM_IN, &adev->cap_config);
    if (adev->pcm_cap && !pcm_is_ready(adev->pcm_cap)) {
        ret = -1;
        ALOGE("cannot open pcm_cap: %s", pcm_get_error(adev->pcm_cap));
        goto error;
    }

    //create resampler
    ret = create_resampler(adev->cap_config.rate,
                         pcm_config_sco_out.rate,
                         2,
                         RESAMPLER_QUALITY_DEFAULT,
                         NULL,
                         &adev->rsmpl_sco_tx);
    if(ret) {
      ALOGI("create_resampler rsmpl_sco_tx failed, ret %d", ret);
      goto error;
    }

    ALOGI("create_resampler rsmpl_sco_tx, in rate %d, out rate %d",
        adev->cap_config.rate, pcm_config_sco_out.rate);

    //create tx task, use real time thread.
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    schParam.sched_priority = 3;
    pthread_attr_setschedparam(&attr, &schParam);

    adev->b_sco_tx_running = true;
    /*
    ret = pthread_create(&tid_sco_tx, &attr, sco_tx_task, (void *)adev);
    if(ret) {
        goto error;
    }
    adev->tid_sco_tx = tid_sco_tx;
    */
    ALOGI("sco_tx_task create ret %d, tid_sco_tx %ld", ret, tid_sco_tx);

    return 0;

error:
    sco_release_resource();
    return ret;
}

static int sco_task_destroy()
{
    int ret;

    ALOGI("enter sco_task_destroy");
    ret = sco_release_resource();
    ALOGI("leave sco_task_destroy");

    return ret;
}

void sigint_handler(int sig)
{
    ALOGI("%s: receive signal", __func__);
    if (sig == SIGINT){
        running = false;
    }
}


int main()
{
    int ret;
    int rate = 8000;
    int k = 0;

    signal(SIGINT, sigint_handler); 

    /*second card maybe null*/
    while (k < MAX_AUDIO_CARD_NUM) {
        adev->card_list[k]  = audio_card_list[SUPPORT_CARD_NUM-1];
        k++;
    }
    ALOGD("%s: enter", __func__);


    ALOGI("hfp_set_sampling_rate, %d", rate);
    pcm_config_sco_in.rate = rate;
    pcm_config_sco_out.rate = rate;

	ret = sco_task_create();
	ALOGI("sco_task_create, ret %d", ret);

	while(running){
		sleep(1);
	}

	ret = sco_task_destroy();
	ALOGI("sco_task_destroy, ret %d", ret);


    return 0;
}

