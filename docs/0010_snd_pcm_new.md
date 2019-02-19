# snd_pcm_new

* 主要是跟踪snd_pcm_new如何创建PCM设备节点的，以及操作PCM的设备节点的时候对应的操作函数在哪里；
* 驱动中component的dai的ops会被绑定到pcm的ops中；

## [`snd_soc_instantiate_card`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L1979)调用[`soc_probe_link_dais`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L2087)

```C
static int snd_soc_instantiate_card(struct snd_soc_card *card)
{
    [...省略]
    /* probe all DAI links on this card */
    for_each_comp_order(order) {
        for_each_card_rtds(card, rtd) {
            ret = soc_probe_link_dais(card, rtd, order);                // 接下来解读
            if (ret < 0) {
                dev_err(card->dev,
                    "ASoC: failed to instantiate card %d\n",
                    ret);
                goto probe_dai_err;
            }
        }
    }
    [...省略]
}
```
## [soc_probe_link_dais](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L1502)

```C
static int soc_probe_link_dais(struct snd_soc_card *card,
        struct snd_soc_pcm_runtime *rtd, int order)
{
    struct snd_soc_dai_link *dai_link = rtd->dai_link;
    struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
    struct snd_soc_rtdcom_list *rtdcom;
    struct snd_soc_component *component;
    struct snd_soc_dai *codec_dai;
    int i, ret, num;

    dev_dbg(card->dev, "ASoC: probe %s dai link %d late %d\n",
            card->name, rtd->num, order);

    /* set default power off timeout */
    rtd->pmdown_time = pmdown_time;

    ret = soc_probe_dai(cpu_dai, order);                        // 调用cpu_dai的probe函数
    if (ret)
        return ret;

    /* probe the CODEC DAI */
    for_each_rtd_codec_dai(rtd, i, codec_dai) {
        ret = soc_probe_dai(codec_dai, order);                  // 调用codec_dei的probe函数
        if (ret)
            return ret;
    }

    /* complete DAI probe during last probe */
    if (order != SND_SOC_COMP_ORDER_LAST)
        return 0;

    /* do machine specific initialization */
    if (dai_link->init) {
        ret = dai_link->init(rtd);                              // 调用dai_link的init函数
        if (ret < 0) {
            dev_err(card->dev, "ASoC: failed to init %s: %d\n",
                dai_link->name, ret);
            return ret;
        }
    }

    if (dai_link->dai_fmt)
        snd_soc_runtime_set_dai_fmt(rtd, dai_link->dai_fmt);

    ret = soc_post_component_init(rtd, dai_link->name);         // 将一个rtd（runtime dai_link）注册为一个device，加入设备链：ret = device_add(rtd->dev);
    if (ret)
        return ret;

#ifdef CONFIG_DEBUG_FS
    /* add DPCM sysfs entries */
    if (dai_link->dynamic)
        soc_dpcm_debugfs_add(rtd);
#endif

    num = rtd->num;                                                         // 获取pcm序号

    /*
     * most drivers will register their PCMs using DAI link ordering but
     * topology based drivers can use the DAI link id field to set PCM
     * device number and then use rtd + a base offset of the BEs.
     */
    for_each_rtdcom(rtd, rtdcom) {
        component = rtdcom->component;

        if (!component->driver->use_dai_pcm_id)
            continue;

        if (rtd->dai_link->no_pcm)
            num += component->driver->be_pcm_base;
        else
            num = rtd->dai_link->id;
    }

    if (cpu_dai->driver->compress_new) {                                    // 之前没有怎么关注这部分内容，暂时不管
        /* create compress_device" */
        ret = cpu_dai->driver->compress_new(rtd, num);
        if (ret < 0) {
            dev_err(card->dev, "ASoC: can't create compress %s\n",
                     dai_link->stream_name);
            return ret;
        }
    } else {

        if (!dai_link->params) {
            /* create the pcm */
            ret = soc_new_pcm(rtd, num);                                    // 接下来分析
            if (ret < 0) {
                dev_err(card->dev, "ASoC: can't create pcm %s :%d\n",
                    dai_link->stream_name, ret);
                return ret;
            }
            ret = soc_link_dai_pcm_new(&cpu_dai, 1, rtd);
            if (ret < 0)
                return ret;
            ret = soc_link_dai_pcm_new(rtd->codec_dais,
                           rtd->num_codecs, rtd);
            if (ret < 0)
                return ret;
        } else {
            INIT_DELAYED_WORK(&rtd->delayed_work,
                        codec2codec_close_delayed_work);
        }
    }

    return 0;
}
```

## [soc_new_pcm](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-pcm.c#L3017)

```C
/* create a new pcm */
int soc_new_pcm(struct snd_soc_pcm_runtime *rtd, int num)
{
    struct snd_soc_dai *codec_dai;
    struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
    struct snd_soc_component *component;
    struct snd_soc_rtdcom_list *rtdcom;
    struct snd_pcm *pcm;
    char new_name[64];
    int ret = 0, playback = 0, capture = 0;
    int i;

    if (rtd->dai_link->dynamic || rtd->dai_link->no_pcm) {                      // 假设走else
        playback = rtd->dai_link->dpcm_playback;
        capture = rtd->dai_link->dpcm_capture;
    } else {
        for_each_rtd_codec_dai(rtd, i, codec_dai) {
            if (codec_dai->driver->playback.channels_min)                       // 从前面codec dai的设定中确实定义了
                playback = 1;
            if (codec_dai->driver->capture.channels_min)                        // 从前面codec dai的设定中确实定义了
                capture = 1;
        }

        capture = capture && cpu_dai->driver->capture.channels_min;             // capture可以是0
        playback = playback && cpu_dai->driver->playback.channels_min;          // playback可以是0
    }

    if (rtd->dai_link->playback_only) {                                         // 再次确认
        playback = 1;
        capture = 0;
    }

    if (rtd->dai_link->capture_only) {                                          // 再次确认
        playback = 0;
        capture = 1;
    }

    /* create the PCM */
    if (rtd->dai_link->no_pcm) {                                                // 暂时考虑else
        snprintf(new_name, sizeof(new_name), "(%s)",
            rtd->dai_link->stream_name);

        ret = snd_pcm_new_internal(rtd->card->snd_card, new_name, num,
                playback, capture, &pcm);
    } else {
        if (rtd->dai_link->dynamic)
            snprintf(new_name, sizeof(new_name), "%s (*)",
                rtd->dai_link->stream_name);
        else
            snprintf(new_name, sizeof(new_name), "%s %s-%d",
                rtd->dai_link->stream_name,
                (rtd->num_codecs > 1) ?                                         // 由以前文档可知，num_codecs为1
                "multicodec" : rtd->codec_dai->name, num);

        ret = snd_pcm_new(rtd->card->snd_card, new_name, num, playback,         // 接下来分子这里
            capture, &pcm);
    }
    if (ret < 0) {
        dev_err(rtd->card->dev, "ASoC: can't create pcm for %s\n",
            rtd->dai_link->name);
        return ret;
    }
    dev_dbg(rtd->card->dev, "ASoC: registered pcm #%d %s\n",num, new_name);

    /* DAPM dai link stream work */
    INIT_DELAYED_WORK(&rtd->delayed_work, close_delayed_work);

    pcm->nonatomic = rtd->dai_link->nonatomic;
    rtd->pcm = pcm;                                                             // rtd中记录pcm
    pcm->private_data = rtd;                                                    // pcm中记录rtd作为私有数据结构

    if (rtd->dai_link->no_pcm) {                                                // 考虑else
        if (playback)
            pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream->private_data = rtd;
        if (capture)
            pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream->private_data = rtd;
        goto out;
    }

    /* ASoC PCM operations */
    if (rtd->dai_link->dynamic) {                                           // 考虑else
        rtd->ops.open        = dpcm_fe_dai_open;
        rtd->ops.hw_params    = dpcm_fe_dai_hw_params;
        rtd->ops.prepare    = dpcm_fe_dai_prepare;
        rtd->ops.trigger    = dpcm_fe_dai_trigger;
        rtd->ops.hw_free    = dpcm_fe_dai_hw_free;
        rtd->ops.close        = dpcm_fe_dai_close;
        rtd->ops.pointer    = soc_pcm_pointer;
        rtd->ops.ioctl        = soc_pcm_ioctl;
    } else {
        rtd->ops.open        = soc_pcm_open;                                // 绑定rtd相关的基本操作函数
        rtd->ops.hw_params    = soc_pcm_hw_params;
        rtd->ops.prepare    = soc_pcm_prepare;
        rtd->ops.trigger    = soc_pcm_trigger;
        rtd->ops.hw_free    = soc_pcm_hw_free;
        rtd->ops.close        = soc_pcm_close;
        rtd->ops.pointer    = soc_pcm_pointer;
        rtd->ops.ioctl        = soc_pcm_ioctl;
    }

    for_each_rtdcom(rtd, rtdcom) {                                          // 将component中驱动部分的ops绑定到rtd的ops中
        const struct snd_pcm_ops *ops = rtdcom->component->driver->ops;

        if (!ops)
            continue;

        if (ops->ack)
            rtd->ops.ack        = soc_rtdcom_ack;
        if (ops->copy_user)
            rtd->ops.copy_user    = soc_rtdcom_copy_user;
        if (ops->copy_kernel)
            rtd->ops.copy_kernel    = soc_rtdcom_copy_kernel;
        if (ops->fill_silence)
            rtd->ops.fill_silence    = soc_rtdcom_fill_silence;
        if (ops->page)
            rtd->ops.page        = soc_rtdcom_page;
        if (ops->mmap)
            rtd->ops.mmap        = soc_rtdcom_mmap;
    }

    if (playback)
        snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &rtd->ops);     // 将rtd注册的ops处理函数注册到pcm playback操作流中，pcmC%iD%i%c设备节点的操作函数就在这里了。

    if (capture)
        snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &rtd->ops);      // 将rtd注册的ops处理函数注册到pcm capture操作流中，pcmC%iD%i%c设备节点的操作函数就在这里了。

    for_each_rtdcom(rtd, rtdcom) {                                      // component中可能有pcm_new阶段需要回调的函数，在这里被调用
        component = rtdcom->component;

        if (!component->driver->pcm_new)
            continue;

        ret = component->driver->pcm_new(rtd);
        if (ret < 0) {
            dev_err(component->dev,
                "ASoC: pcm constructor failed: %d\n",
                ret);
            return ret;
        }
    }

    pcm->private_free = soc_pcm_private_free;
out:
    dev_info(rtd->card->dev, "%s <-> %s mapping ok\n",
         (rtd->num_codecs > 1) ? "multicodec" : rtd->codec_dai->name,
         cpu_dai->name);
    return ret;
}

```

## [`snd_pcm_new`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/core/pcm.c#L837)调用[`_snd_pcm_new`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/core/pcm.c#L767)

```C
static int _snd_pcm_new(struct snd_card *card, const char *id, int device,
        int playback_count, int capture_count, bool internal,
        struct snd_pcm **rpcm)
{
    struct snd_pcm *pcm;
    int err;
    static struct snd_device_ops ops = {
        .dev_free = snd_pcm_dev_free,
        .dev_register =    snd_pcm_dev_register,
        .dev_disconnect = snd_pcm_dev_disconnect,
    };
    static struct snd_device_ops internal_ops = {
        .dev_free = snd_pcm_dev_free,
    };

    if (snd_BUG_ON(!card))
        return -ENXIO;
    if (rpcm)
        *rpcm = NULL;
    pcm = kzalloc(sizeof(*pcm), GFP_KERNEL);                                    // 申请pcm内存
    if (!pcm)
        return -ENOMEM;
    pcm->card = card;
    pcm->device = device;
    pcm->internal = internal;
    mutex_init(&pcm->open_mutex);
    init_waitqueue_head(&pcm->open_wait);
    INIT_LIST_HEAD(&pcm->list);
    if (id)
        strlcpy(pcm->id, id, sizeof(pcm->id));

    // 设备节点的生成就在这里了
    // dev_set_name(&pstr->dev, "pcmC%iD%i%c", pcm->card->number, pcm->device,
    //          stream == SNDRV_PCM_STREAM_PLAYBACK ? 'p' : 'c');
    err = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK,                    // 创建Playback流
                 playback_count);
    if (err < 0)
        goto free_pcm;

    err = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, capture_count);     // 创建Capture流
    if (err < 0)
        goto free_pcm;

    err = snd_device_new(card, SNDRV_DEV_PCM, pcm,
                 internal ? &internal_ops : &ops);
    if (err < 0)
        goto free_pcm;

    if (rpcm)
        *rpcm = pcm;
    return 0;

free_pcm:
    snd_pcm_free(pcm);
    return err;
}
```