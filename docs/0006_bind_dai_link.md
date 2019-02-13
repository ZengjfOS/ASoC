# bind dai_link

[soc_bind_dai_link Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L862)

* 一个dai_link创建一个pcm runtime(rtd: runtime dai)；
* 这部分相当于去检查当前的dai_link中需要的CPU/Platform/Codec component是否注册了，只有注册了的才能通过`snd_soc_rtdcom_add`添加到rtd中。

## snd_soc_register_card调用

```C
static int snd_soc_instantiate_card(struct snd_soc_card *card)
{
    [...省略]

    /* bind DAIs */
    for_each_card_prelinks(card, i, dai_link) {
        ret = soc_bind_dai_link(card, dai_link);        // <------
        if (ret != 0)
            goto base_error;
    }

    [...省略]

    return snd_soc_bind_card(card);
}
EXPORT_SYMBOL_GPL(snd_soc_register_card);
```

## soc_bind_dai_link代码分析

* [soc_bind_dai_link Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L862)
* [i.MX6 soc dai_link添加link-component数组结构展开](0005_dai_link_component.md#dai_link添加link-component数组结构展开)

```C
static int soc_bind_dai_link(struct snd_soc_card *card,
    struct snd_soc_dai_link *dai_link)
{
    struct snd_soc_pcm_runtime *rtd;
    struct snd_soc_dai_link_component *codecs = dai_link->codecs;
    struct snd_soc_dai_link_component cpu_dai_component;
    struct snd_soc_component *component;
    struct snd_soc_dai **codec_dais;
    int i;

    if (dai_link->ignore)
        return 0;

    dev_dbg(card->dev, "ASoC: binding %s\n", dai_link->name);

    if (soc_is_dai_link_bound(card, dai_link)) {                            // 检查当前的dai_link是否已经绑定了
        dev_dbg(card->dev, "ASoC: dai link %s already bound\n",
            dai_link->name);
        return 0;
    }

    rtd = soc_new_pcm_runtime(card, dai_link);                              // 每一个dai_link对应一个pcm runtime(rtd: runtime dai)
    if (!rtd)
        return -ENOMEM;

    cpu_dai_component.name = dai_link->cpu_name;
    cpu_dai_component.of_node = dai_link->cpu_of_node;
    cpu_dai_component.dai_name = dai_link->cpu_dai_name;
    rtd->cpu_dai = snd_soc_find_dai(&cpu_dai_component);                    // 根据dai_link->cpu_of_node查找CPU DAI，其实就是esai
    if (!rtd->cpu_dai) {
        dev_info(card->dev, "ASoC: CPU DAI %s not registered\n",
             dai_link->cpu_dai_name);
        goto _err_defer;
    }
    snd_soc_rtdcom_add(rtd, rtd->cpu_dai->component);                       // 添加CPU DAI的component到rtd

    rtd->num_codecs = dai_link->num_codecs;

    /* Find CODEC from registered CODECs */
    /* we can use for_each_rtd_codec_dai() after this */
    codec_dais = rtd->codec_dais;
    for (i = 0; i < rtd->num_codecs; i++) {
        codec_dais[i] = snd_soc_find_dai(&codecs[i]);                       // 根据codecs信息获取codec DAI
        if (!codec_dais[i]) {
            dev_err(card->dev, "ASoC: CODEC DAI %s not registered\n",
                codecs[i].dai_name);
            goto _err_defer;
        }
        snd_soc_rtdcom_add(rtd, codec_dais[i]->component);                  // 添加Codec DAI的component到rtd
    }

    /* Single codec links expect codec and codec_dai in runtime data */
    rtd->codec_dai = codec_dais[0];

    /* find one from the set of registered platforms */
    for_each_component(component) {
        if (!snd_soc_is_matching_component(dai_link->platform,
                           component))
            continue;

        snd_soc_rtdcom_add(rtd, component);                                 // 添加platform到rtd中
    }

    soc_add_pcm_runtime(card, rtd);                                         // 将rtd添加到声卡中
    return 0;

_err_defer:
    soc_free_pcm_runtime(rtd);
    return -EPROBE_DEFER;
}
```