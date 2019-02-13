# ASoC Machine Driver

* 主要是获取设备中定义的route并初始化card；
* 用预定好的route、wiget初始化card；
* [初始化dai_link](0005_soc_init_dai_link.md)；

## 参考文档

* [Android音频驱动-ASOC之PCM Device创建](https://blog.csdn.net/Vincentywj/article/details/77676583)

## ASoC audio设备树匹配

* [CS42888声卡设备树示例](0001_DTS.md#audio-dts)
* [fsl,imx-audio-cs42888 fsl_asoc_card_driver](https://github.com/torvalds/linux/blob/master/sound/soc/fsl/fsl-asoc-card.c#L698)
  ```C
  static const struct of_device_id fsl_asoc_card_dt_ids[] = {
      { .compatible = "fsl,imx-audio-ac97", },
      { .compatible = "fsl,imx-audio-cs42888", },
      { .compatible = "fsl,imx-audio-cs427x", },
      { .compatible = "fsl,imx-audio-sgtl5000", },
      { .compatible = "fsl,imx-audio-wm8962", },
      { .compatible = "fsl,imx-audio-wm8960", },
      {}
  };
  MODULE_DEVICE_TABLE(of, fsl_asoc_card_dt_ids);
  
  static struct platform_driver fsl_asoc_card_driver = {
      .probe = fsl_asoc_card_probe,
      .driver = {
          .name = "fsl-asoc-card",
          .pm = &snd_soc_pm_ops,
          .of_match_table = fsl_asoc_card_dt_ids,
      },
  };
  module_platform_driver(fsl_asoc_card_driver);
  ```

## Machine route/widget

* of_dapm_routes
  ```
  audio-routing =
      "Line Out Jack", "AOUT1L",
      "Line Out Jack", "AOUT1R",
      "Line Out Jack", "AOUT2L",
      "Line Out Jack", "AOUT2R",
      "Line Out Jack", "AOUT3L",
      "Line Out Jack", "AOUT3R",
      "Line Out Jack", "AOUT4L",
      "Line Out Jack", "AOUT4R",
      "AIN1L", "Line In Jack",
      "AIN1R", "Line In Jack",
      "AIN2L", "Line In Jack",
      "AIN2R", "Line In Jack";
  ```
* dapm_routes
  ```C
  static const struct snd_soc_dapm_route audio_map[] = {
      /* 1st half -- Normal DAPM routes */
      {"Playback",  NULL, "CPU-Playback"},
      {"CPU-Capture",  NULL, "Capture"},
      /* 2nd half -- ASRC DAPM routes */
      {"CPU-Playback",  NULL, "ASRC-Playback"},
      {"ASRC-Capture",  NULL, "CPU-Capture"},
  };
  ```
* dapm_widgets
  ```C
  /* Add all possible widgets into here without being redundant */
  static const struct snd_soc_dapm_widget fsl_asoc_card_dapm_widgets[] = {
      /**
       * #define SND_SOC_DAPM_LINE(wname, wevent) \
       * {    .id = snd_soc_dapm_line, .name = wname, .kcontrol_news = NULL, \
       *     .num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
       *     .event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD}
       */
      SND_SOC_DAPM_LINE("Line Out Jack", NULL),
      SND_SOC_DAPM_LINE("Line In Jack", NULL),
      SND_SOC_DAPM_HP("Headphone Jack", NULL),
      SND_SOC_DAPM_SPK("Ext Spk", NULL),
      SND_SOC_DAPM_MIC("Mic Jack", NULL),
      SND_SOC_DAPM_MIC("AMIC", NULL),
      SND_SOC_DAPM_MIC("DMIC", NULL),
  };
  ```
* audio-routing解析
  * fsl_asoc_card_probe
    ```C
    static int fsl_asoc_card_probe(struct platform_device *pdev)
    {
        [...省略]
        priv->card.dapm_routes = fsl_asoc_card_is_ac97(priv) ?
                   audio_map_ac97 : audio_map;
        priv->card.late_probe = fsl_asoc_card_late_probe;                                 // late_probe函数
        priv->card.num_dapm_routes = ARRAY_SIZE(audio_map);
        priv->card.dapm_widgets = fsl_asoc_card_dapm_widgets;
        priv->card.num_dapm_widgets = ARRAY_SIZE(fsl_asoc_card_dapm_widgets);
        [...省略]
        ret = snd_soc_of_parse_audio_routing(&priv->card, "audio-routing");               // 对设备树的audio-routing进行提取
        if (ret) {
            dev_err(&pdev->dev, "failed to parse audio-routing: %d\n", ret);
            goto asrc_fail;
        }
        [...省略]
    }
    ```
  * [snd_soc_of_parse_audio_routing(&priv->card, "audio-routing")](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L3529)

## fsl_asoc_card_probe解析

```C
static int fsl_asoc_card_probe(struct platform_device *pdev)
{
    struct device_node *cpu_np, *codec_np, *asrc_np;
    struct device_node *np = pdev->dev.of_node;
    struct platform_device *asrc_pdev = NULL;
    struct platform_device *cpu_pdev;
    struct fsl_asoc_card_priv *priv;                                        // 这个结构体需要看一下
    struct i2c_client *codec_dev;
    const char *codec_dai_name;
    u32 width;
    int ret;

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    cpu_np = of_parse_phandle(np, "audio-cpu", 0);                          // 获取esai设备树
    /* Give a chance to old DT binding */
    if (!cpu_np)
        cpu_np = of_parse_phandle(np, "ssi-controller", 0);
    if (!cpu_np) {
        dev_err(&pdev->dev, "CPU phandle missing or invalid\n");
        ret = -EINVAL;
        goto fail;
    }

    cpu_pdev = of_find_device_by_node(cpu_np);                              // 根据设备树找到平台设备
    if (!cpu_pdev) {
        dev_err(&pdev->dev, "failed to find CPU DAI device\n");
        ret = -EINVAL;
        goto fail;
    }

    codec_np = of_parse_phandle(np, "audio-codec", 0);                      // 获取cs42888设备树
    if (codec_np)
        codec_dev = of_find_i2c_device_by_node(codec_np);                   // 获取i2c平台设备
    else
        codec_dev = NULL;

    asrc_np = of_parse_phandle(np, "audio-asrc", 0);                        // 获取asrc设备树
    if (asrc_np)
        asrc_pdev = of_find_device_by_node(asrc_np);                        // 获取asrc平台设备

    /* Get the MCLK rate only, and leave it controlled by CODEC drivers */
    if (codec_dev) {
        struct clk *codec_clk = clk_get(&codec_dev->dev, NULL);

        if (!IS_ERR(codec_clk)) {
            priv->codec_priv.mclk_freq = clk_get_rate(codec_clk);
            clk_put(codec_clk);
        }
    }

    /* Default sample rate and format, will be updated in hw_params() */
    priv->sample_rate = 44100;                                              // 默认采样率
    priv->sample_format = SNDRV_PCM_FORMAT_S16_LE;                          // 采样深度

    /* Assign a default DAI format, and allow each card to overwrite it */
    priv->dai_fmt = DAI_FMT_BASE;                                           // DAI格式

    /* Diversify the card configurations */
    if (of_device_is_compatible(np, "fsl,imx-audio-cs42888")) {             // 我们跟的就是他了
        codec_dai_name = "cs42888";                                         // 获取到了codec dai name
        priv->card.set_bias_level = NULL;
        priv->cpu_priv.sysclk_freq[TX] = priv->codec_priv.mclk_freq;
        priv->cpu_priv.sysclk_freq[RX] = priv->codec_priv.mclk_freq;
        priv->cpu_priv.sysclk_dir[TX] = SND_SOC_CLOCK_OUT;
        priv->cpu_priv.sysclk_dir[RX] = SND_SOC_CLOCK_OUT;
        priv->cpu_priv.slot_width = 32;
        priv->dai_fmt |= SND_SOC_DAIFMT_CBS_CFS;

    [...省略]

    }

    if (!fsl_asoc_card_is_ac97(priv) && !codec_dev) {
        dev_err(&pdev->dev, "failed to find codec device\n");
        ret = -EINVAL;
        goto asrc_fail;
    }

    /* Common settings for corresponding Freescale CPU DAI driver */
    if (of_node_name_eq(cpu_np, "ssi")) {
        /* Only SSI needs to configure AUDMUX */
        ret = fsl_asoc_card_audmux_init(np, priv);
        if (ret) {
            dev_err(&pdev->dev, "failed to init audmux\n");
            goto asrc_fail;
        }
    } else if (of_node_name_eq(cpu_np, "esai")) {                           // 接口用的是esai
        priv->cpu_priv.sysclk_id[1] = ESAI_HCKT_EXTAL;
        priv->cpu_priv.sysclk_id[0] = ESAI_HCKR_EXTAL;
    } else if (of_node_name_eq(cpu_np, "sai")) {
        priv->cpu_priv.sysclk_id[1] = FSL_SAI_CLK_MAST1;
        priv->cpu_priv.sysclk_id[0] = FSL_SAI_CLK_MAST1;
    }

    snprintf(priv->name, sizeof(priv->name), "%s-audio",
         fsl_asoc_card_is_ac97(priv) ? "ac97" :
         codec_dev->name);                                                  // codec_dev->name = cs42888

    /* Initialize sound card */
    priv->pdev = pdev;
    priv->card.dev = &pdev->dev;
    priv->card.name = priv->name;                                               // cs42888-audio
    priv->card.dai_link = priv->dai_link;                                       // 声卡的dai_link和priv的dai_link是一样的，后面主要操作priv的dai_link
    /**
     * static const struct snd_soc_dapm_route audio_map[] = {
     *     /* 1st half -- Normal DAPM routes */
     *     {"Playback",  NULL, "CPU-Playback"},
     *     {"CPU-Capture",  NULL, "Capture"},
     *     /* 2nd half -- ASRC DAPM routes */
     *     {"CPU-Playback",  NULL, "ASRC-Playback"},
     *     {"ASRC-Capture",  NULL, "CPU-Capture"},
     * };
     */
    priv->card.dapm_routes = fsl_asoc_card_is_ac97(priv) ?
                 audio_map_ac97 : audio_map;
    priv->card.late_probe = fsl_asoc_card_late_probe;
    priv->card.num_dapm_routes = ARRAY_SIZE(audio_map);
    /**
     * /* Add all possible widgets into here without being redundant */
     * static const struct snd_soc_dapm_widget fsl_asoc_card_dapm_widgets[] = {
     *     /**
     *      * #define SND_SOC_DAPM_LINE(wname, wevent) \
     *      * {    .id = snd_soc_dapm_line, .name = wname, .kcontrol_news = NULL, \
     *      *     .num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
     *      *     .event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD}
     *      */
     *     SND_SOC_DAPM_LINE("Line Out Jack", NULL),
     *     SND_SOC_DAPM_LINE("Line In Jack", NULL),
     *     SND_SOC_DAPM_HP("Headphone Jack", NULL),
     *     SND_SOC_DAPM_SPK("Ext Spk", NULL),
     *     SND_SOC_DAPM_MIC("Mic Jack", NULL),
     *     SND_SOC_DAPM_MIC("AMIC", NULL),
     *     SND_SOC_DAPM_MIC("DMIC", NULL),
     * };
     */
    priv->card.dapm_widgets = fsl_asoc_card_dapm_widgets;
    priv->card.num_dapm_widgets = ARRAY_SIZE(fsl_asoc_card_dapm_widgets);

    /* Drop the second half of DAPM routes -- ASRC */
    if (!asrc_pdev)
        priv->card.num_dapm_routes /= 2;

    /**
     * static struct snd_soc_dai_link fsl_asoc_card_dai[] = {
     *     /* Default ASoC DAI Link*/
     *     {
     *         .name = "HiFi",
     *         .stream_name = "HiFi",
     *         .ops = &fsl_asoc_card_ops,
     *     },
     *     /* DPCM Link between Front-End and Back-End (Optional) */
     *     {
     *         .name = "HiFi-ASRC-FE",
     *         .stream_name = "HiFi-ASRC-FE",
     *         .codec_name = "snd-soc-dummy",
     *         .codec_dai_name = "snd-soc-dummy-dai",
     *         .dpcm_playback = 1,
     *         .dpcm_capture = 1,
     *         .dynamic = 1,
     *     },
     *     {
     *         .name = "HiFi-ASRC-BE",
     *         .stream_name = "HiFi-ASRC-BE",
     *         .platform_name = "snd-soc-dummy",
     *         .be_hw_params_fixup = be_hw_params_fixup,
     *         .ops = &fsl_asoc_card_ops,
     *         .dpcm_playback = 1,
     *         .dpcm_capture = 1,
     *         .no_pcm = 1,
     *     },
     * };
     */
    memcpy(priv->dai_link, fsl_asoc_card_dai,
           sizeof(struct snd_soc_dai_link) * ARRAY_SIZE(priv->dai_link));

    /**
     * audio-routing =
     *     "Line Out Jack", "AOUT1L",
     *     "Line Out Jack", "AOUT1R",
     *     "Line Out Jack", "AOUT2L",
     *     "Line Out Jack", "AOUT2R",
     *     "Line Out Jack", "AOUT3L",
     *     "Line Out Jack", "AOUT3R",
     *     "Line Out Jack", "AOUT4L",
     *     "Line Out Jack", "AOUT4R",
     *     "AIN1L", "Line In Jack",
     *     "AIN1R", "Line In Jack",
     *     "AIN2L", "Line In Jack",
     *     "AIN2R", "Line In Jack";
     *
     * card->num_of_dapm_routes = num_routes;           // 设备树定义的route数量
     * card->of_dapm_routes = routes;                   // 设备树定义的route
     */
    ret = snd_soc_of_parse_audio_routing(&priv->card, "audio-routing");
    if (ret) {
        dev_err(&pdev->dev, "failed to parse audio-routing: %d\n", ret);
        goto asrc_fail;
    }

    /* Normal DAI Link */                                                   // 这里一定要注意priv->dai_link和后面priv->card->dai_link指针指向同一个地方
    priv->dai_link[0].cpu_of_node = cpu_np;                                 // dai_link[0]cpu设备树节点为esai设备树
    priv->dai_link[0].codec_dai_name = codec_dai_name;                      // dai_link[0]的codec DAI名字

    if (!fsl_asoc_card_is_ac97(priv))
        priv->dai_link[0].codec_of_node = codec_np;                         // dai_link[0]codec设备树节点为cs42888设备树
    else {
        u32 idx;

        ret = of_property_read_u32(cpu_np, "cell-index", &idx);
        if (ret) {
            dev_err(&pdev->dev,
                "cannot get CPU index property\n");
            goto asrc_fail;
        }

        priv->dai_link[0].codec_name =
                devm_kasprintf(&pdev->dev, GFP_KERNEL,
                           "ac97-codec.%u",
                           (unsigned int)idx);
        if (!priv->dai_link[0].codec_name) {
            ret = -ENOMEM;
            goto asrc_fail;
        }
    }

    priv->dai_link[0].platform_of_node = cpu_np;                        // dai_link[0]平台设备树节点为esai设备树
    priv->dai_link[0].dai_fmt = priv->dai_fmt;                          // dai_link[0]帧格式
    priv->card.num_links = 1;                                           // 当前支持的dai link的个数

    if (asrc_pdev) {
        /* DPCM DAI Links only if ASRC exsits */
        priv->dai_link[1].cpu_of_node = asrc_np;                        // dai_link[1]cpu设备树节点为asrc设备树
        priv->dai_link[1].platform_of_node = asrc_np;                   // dai_link[1]平台设备树节点为asrc设备树
        priv->dai_link[2].codec_dai_name = codec_dai_name;              // dai_link[0]的codec DAI名字
        priv->dai_link[2].codec_of_node = codec_np;                     // dai_link[0]codec设备树节点为cs42888设备树
        priv->dai_link[2].codec_name =
                priv->dai_link[0].codec_name;                           // 这个目前没有赋值
        priv->dai_link[2].cpu_of_node = cpu_np;                         // dai_link[1]cpu设备树节点为asrc设备树
        priv->dai_link[2].dai_fmt = priv->dai_fmt;                      // dai_link[0]帧格式
        priv->card.num_links = 3;                                       // 当前支持的dai link的个数

        ret = of_property_read_u32(asrc_np, "fsl,asrc-rate",
                       &priv->asrc_rate);
        if (ret) {
            dev_err(&pdev->dev, "failed to get output rate\n");
            ret = -EINVAL;
            goto asrc_fail;
        }

        ret = of_property_read_u32(asrc_np, "fsl,asrc-width", &width);
        if (ret) {
            dev_err(&pdev->dev, "failed to get output rate\n");
            ret = -EINVAL;
            goto asrc_fail;
        }

        if (width == 24)
            priv->asrc_format = SNDRV_PCM_FORMAT_S24_LE;
        else
            priv->asrc_format = SNDRV_PCM_FORMAT_S16_LE;
    }

    /* Finish card registering */
    platform_set_drvdata(pdev, priv);                                       // dev->driver_data = data;
    snd_soc_card_set_drvdata(&priv->card, priv);                            // card->drvdata = data;

    ret = devm_snd_soc_register_card(&pdev->dev, &priv->card);
    if (ret && ret != -EPROBE_DEFER)
        dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);

asrc_fail:
    of_node_put(asrc_np);
    of_node_put(codec_np);
fail:
    of_node_put(cpu_np);

    return ret;
}
```

## dai_link数组结构展开

[dai_link被解析成`struct snd_soc_dai_link_component`代码分析](0005_dai_link_component.md#soc_init_dai_link代码分析)

```C
static struct snd_soc_dai_link fsl_asoc_card_dai[] = {
    /* Default ASoC DAI Link*/
    {
        .name = "HiFi",
        .stream_name = "HiFi",
        .ops = &fsl_asoc_card_ops,
        .cpu_of_node = cpu_np,
        .codec_dai_name = codec_dai_name,
        .codec_of_node = codec_np,
        .platform_of_node = cpu_np,
        .dai_fmt = priv->dai_fmt,
    },
    /* DPCM Link between Front-End and Back-End (Optional) */
    {
        .name = "HiFi-ASRC-FE",
        .stream_name = "HiFi-ASRC-FE",
        .codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
        .dpcm_playback = 1,
        .dpcm_capture = 1,
        .dynamic = 1,
        .cpu_of_node = asrc_np,
        .platform_of_node = asrc_np,
    },
    {
        .name = "HiFi-ASRC-BE",
        .stream_name = "HiFi-ASRC-BE",
        .platform_name = "snd-soc-dummy",
        .be_hw_params_fixup = be_hw_params_fixup,
        .ops = &fsl_asoc_card_ops,
        .dpcm_playback = 1,
        .dpcm_capture = 1,
        .no_pcm = 1,
        .codec_dai_name = codec_dai_name,
        .codec_of_node = codec_np,
        .cpu_of_node = cpu_np,
        .dai_fmt = priv->dai_fmt,
    },
};
```

## 声卡注册流程

```
* static int fsl_asoc_card_probe(struct platform_device *pdev)
  * codec_dai_name = "cs42888";
  * priv->card.dai_link = priv->dai_link;                                   // 这个很重要，因为后面没有重复初始化了
  * memcpy(priv->dai_link, fsl_asoc_card_dai, sizeof(struct snd_soc_dai_link) * ARRAY_SIZE(priv->dai_link));
  * priv->dai_link[0].codec_of_node = codec_np;
  * priv->dai_link[0].codec_dai_name = codec_dai_name;
  * priv->card.num_links = 1;
  * if (asrc_pdev) 
    * priv->dai_link[2].codec_dai_name = codec_dai_name;
    * priv->dai_link[2].codec_of_node = codec_np;
    * priv->card.num_links = 3;
  * ret = devm_snd_soc_register_card(&pdev->dev, &priv->card);
    * ret = snd_soc_register_card(card);
      * ret = soc_init_dai_link(card, link);
        * ret = snd_soc_init_platform(card, link);                                // 初始化平台数据
          * dai_link->platform      = platform;
          * dai_link->legacy_platform = 1;
          * platform->name          = dai_link->platform_name;
          * platform->of_node      = dai_link->platform_of_node;
          * platform->dai_name      = NULL;
        * ret = snd_soc_init_multicodec(card, link);                              // 初始化codec数据
          * dai_link->num_codecs = 1;
          * dai_link->codecs[0].name = dai_link->codec_name;
          * dai_link->codecs[0].of_node = dai_link->codec_of_node;
          * dai_link->codecs[0].dai_name = dai_link->codec_dai_name;
      * return snd_soc_bind_card(card);
        * static int snd_soc_instantiate_card(struct snd_soc_card *card)
          * ret = soc_bind_dai_link(card, dai_link);
            * if (soc_is_dai_link_bound(card, dai_link))                          // 如果已经绑定了就不用再绑定
            * rtd = soc_new_pcm_runtime(card, dai_link);
            * cpu_dai_component.name = dai_link->cpu_name;
            * cpu_dai_component.of_node = dai_link->cpu_of_node;                  // 使用of_node进行匹配
            * rtd->cpu_dai = snd_soc_find_dai(&cpu_dai_component);                // 查找能够匹配的of_node匹配上的DAI，不是名字
              * for_each_component(component) 
                * list_for_each_entry(component, &component_list, list)
                  * if (!snd_soc_is_matching_component(dlc, component))
                    * if (dlc->of_node && component_of_node != dlc->of_node) return 0;          // of_node进行匹配
                    * if (dlc->name && strcmp(component->name, dlc->name)) return 0;
            * snd_soc_rtdcom_add(rtd, rtd->cpu_dai->component);                                 // esai dai
            * snd_soc_rtdcom_add(rtd, codec_dais[i]->component);                                // codec dai
            * snd_soc_rtdcom_add(rtd, component);                                               // platform
          * ret = soc_probe_link_components(card, rtd, order);                                  // 调用component的probe函数
            * ret = soc_probe_component(card, component);
              * if (!strcmp(component->name, "snd-soc-dummy")) return 0;
              * ret = component->driver->probe(component);
              * ret = component->init(component);
          * ret = soc_probe_link_dais(card, rtd, order);
            * ret = soc_new_pcm(rtd, num);                                        // <--------到这里创建出PCM
              * ret = snd_pcm_new(rtd->card->snd_card, new_name, num, playback, capture, &pcm);
                * return _snd_pcm_new(card, id, device, playback_count, capture_count, false, rpcm);
                  * static struct snd_device_ops ops = { .dev_free = snd_pcm_dev_free, .dev_register = snd_pcm_dev_register, .dev_disconnect = snd_pcm_dev_disconnect, };
                    * snd_pcm_dev_register                                                    // 这个回调函数会在声卡的注册阶段创建pcm设备节点/dev/snd/pcmCxxDxxp和/dev/snd/pcmCxxDxxc
                      * err = snd_pcm_add(pcm);                                               // 将pcm加入到全局变量snd_pcm_devices，之后会创建/proc/asound/pcm文件
                      * err = snd_register_device(devtype, pcm->card, pcm->device, &snd_pcm_f_ops[cidx], pcm, &pcm->streams[cidx].dev);
                  * err = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK, playback_count);
                  * err = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, capture_count);
                  * err = snd_device_new(card, SNDRV_DEV_PCM, pcm, internal ? &internal_ops : &ops);
```