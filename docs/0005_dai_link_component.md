# dai_link component

[soc_init_dai_link Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L1086)

```C
struct snd_soc_dai_link {
    [...省略]
    struct snd_soc_dai_link_component *codecs;
    [...省略]
    struct snd_soc_dai_link_component *platform;
    [...省略]
}
```

如上可以，`struct snd_soc_dai_link`将codec和platform连接在了一起，这也就是link的意思了；

## snd_soc_register_card调用

```C
int snd_soc_register_card(struct snd_soc_card *card)
{
    [...省略]

    mutex_lock(&client_mutex);
    for_each_card_prelinks(card, i, link) {

        ret = soc_init_dai_link(card, link);                        // <---- snd_soc_register_card调用
        if (ret) {
            dev_err(card->dev, "ASoC: failed to init link %s\n",
                link->name);
            mutex_unlock(&client_mutex);
            return ret;
        }
    }
    mutex_unlock(&client_mutex);

    [...省略]

    return snd_soc_bind_card(card);
}
EXPORT_SYMBOL_GPL(snd_soc_register_card);
```

## soc_init_dai_link代码分析

* [soc_init_dai_link Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L1086)
* [i.MX6 soc dai_link数组结构展开](0004_ASoC_Machine_Driver.md#dai_link数组结构展开)

```C
static int soc_init_dai_link(struct snd_soc_card *card,
                 struct snd_soc_dai_link *link)
{
    int i, ret;
    struct snd_soc_dai_link_component *codec;

    ret = snd_soc_init_platform(card, link);                            // 创建platform component
    if (ret) {
        dev_err(card->dev, "ASoC: failed to init multiplatform\n");
        return ret;
    }

    ret = snd_soc_init_multicodec(card, link);                          // 创建codec component
    if (ret) {
        dev_err(card->dev, "ASoC: failed to init multicodec\n");
        return ret;
    }

    for_each_link_codecs(link, i, codec) {                              // codec名称和设备树不用同时设定，DAI名字要设定
        /*
         * Codec must be specified by 1 of name or OF node,
         * not both or neither.
         */
        if (!!codec->name ==
            !!codec->of_node) {
            dev_err(card->dev, "ASoC: Neither/both codec name/of_node are set for %s\n",
                link->name);
            return -EINVAL;
        }
        /* Codec DAI name must be specified */

        /**
         * priv->dai_link[0].codec_dai_name = "cs42888"
         * priv->dai_link[1].codec_dai_name = "snd-soc-dummy-dai"
         * priv->dai_link[2].codec_dai_name = "cs42888"
         */
        if (!codec->dai_name) {
            dev_err(card->dev, "ASoC: codec_dai_name not set for %s\n",
                link->name);
            return -EINVAL;
        }
    }

    /*
     * Platform may be specified by either name or OF node, but
     * can be left unspecified, and a dummy platform will be used.
     * platform的名字和设备树节点只能设定一个
     */
    if (link->platform->name && link->platform->of_node) {
        dev_err(card->dev,
            "ASoC: Both platform name/of_node are set for %s\n",
            link->name);
        return -EINVAL;
    }

    /*
     * Defer card registartion if platform dai component is not added to
     * component list.
     */
    if ((link->platform->of_node || link->platform->name) &&
        !soc_find_component(link->platform->of_node, link->platform->name))
        return -EPROBE_DEFER;

    /*
     * CPU device may be specified by either name or OF node, but
     * can be left unspecified, and will be matched based on DAI
     * name alone..
     * cpu的名字和设备树节点只能设定一个
     */
    if (link->cpu_name && link->cpu_of_node) {
        dev_err(card->dev,
            "ASoC: Neither/both cpu name/of_node are set for %s\n",
            link->name);
        return -EINVAL;
    }

    /*
     * Defer card registartion if cpu dai component is not added to
     * component list.
     */
    if ((link->cpu_of_node || link->cpu_name) &&
        !soc_find_component(link->cpu_of_node, link->cpu_name))
        return -EPROBE_DEFER;

    /*
     * At least one of CPU DAI name or CPU device name/node must be
     * specified
     */
    if (!link->cpu_dai_name &&
        !(link->cpu_name || link->cpu_of_node)) {
        dev_err(card->dev,
            "ASoC: Neither cpu_dai_name nor cpu_name/of_node are set for %s\n",
            link->name);
        return -EINVAL;
    }

    return 0;
}
```
* platform snd_soc_dai_link_component
  ```C
  static int snd_soc_init_platform(struct snd_soc_card *card,
                   struct snd_soc_dai_link *dai_link)
  {
      struct snd_soc_dai_link_component *platform = dai_link->platform;
  
      /*
       * FIXME
       *
       * this function should be removed in the future
       */
      /* convert Legacy platform link */
      if (!platform || dai_link->legacy_platform) {
          platform = devm_kzalloc(card->dev,
                  sizeof(struct snd_soc_dai_link_component),
                  GFP_KERNEL);
          if (!platform)
              return -ENOMEM;
  
          dai_link->platform      = platform;
          dai_link->legacy_platform = 1;
          platform->name          = dai_link->platform_name;
          platform->of_node      = dai_link->platform_of_node;
          platform->dai_name      = NULL;
      }
  
      /* if there's no platform we match on the empty platform */
      if (!platform->name &&
          !platform->of_node)
          platform->name = "snd-soc-dummy";
  
      return 0;
  }
  ```
* codec snd_soc_dai_link_component
  ```C
  static int snd_soc_init_multicodec(struct snd_soc_card *card,
                     struct snd_soc_dai_link *dai_link)
  {
      /* Legacy codec/codec_dai link is a single entry in multicodec */
      if (dai_link->codec_name || dai_link->codec_of_node ||
          dai_link->codec_dai_name) {
          dai_link->num_codecs = 1;
  
          dai_link->codecs = devm_kzalloc(card->dev,
                  sizeof(struct snd_soc_dai_link_component),
                  GFP_KERNEL);
          if (!dai_link->codecs)
              return -ENOMEM;
  
          dai_link->codecs[0].name = dai_link->codec_name;
          dai_link->codecs[0].of_node = dai_link->codec_of_node;
          dai_link->codecs[0].dai_name = dai_link->codec_dai_name;
      }
  
      if (!dai_link->codecs) {
          dev_err(card->dev, "ASoC: DAI link has no CODECs\n");
          return -EINVAL;
      }
  
      return 0;
  }
  ```

## dai_link添加link-component数组结构展开

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
        .dai_link->legacy_platform = 1,
        .platform = {
            .of_node = cpu_np,
        },
        .codecs = [{
            .of_node = codec_np,
            .dai_name = codec_dai_name,
        },],
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
        .dai_link->legacy_platform = 1,
        .platform = {
            .of_node = asrc_np,
        },
        .codecs = [{
            .dai_name = "snd-soc-dummy-dai",
        },],
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
        .dai_link->legacy_platform = 1,
        .platform = {
            .name = "snd-soc-dummy",
        },
        .codecs = [{
            .of_node = codec_np,
            .dai_name = codec_dai_name,
        },],
    },
};
```