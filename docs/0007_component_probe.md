# component probe

[soc_probe_link_components Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L2053)

主要是对component中的widget/controls/route进行处理，当然其中也将component中的dai转化成widget，这样看来route既可以连接widget，也可以连接dai widget。

## dai widget

* Codec Driver
  ```C
  static struct snd_soc_dai_driver cs42xx8_dai = {
      .playback = {
          .stream_name    = "Playback",
          .channels_min   = 1,
          .channels_max   = 8,
          .rates          = SNDRV_PCM_RATE_8000_192000,
          .formats        = CS42XX8_FORMATS,
      },
      .capture = {
          .stream_name    = "Capture",
          .channels_min   = 1,
          .rates          = SNDRV_PCM_RATE_8000_192000,
          .formats        = CS42XX8_FORMATS,
      },
      .ops = &cs42xx8_dai_ops,
  };
  ```
* Platform Driver
  ```C
    static struct snd_soc_dai_driver fsl_esai_dai = {
        .probe = fsl_esai_dai_probe,
        /* DAI capabilities */
        // struct snd_soc_pcm_stream playback;
        .playback = {
            .stream_name = "CPU-Playback",
            .channels_min = 1,
            .channels_max = 12,
            .rates = SNDRV_PCM_RATE_8000_192000,
            .formats = FSL_ESAI_FORMATS,
        },
        /* DAI capabilities */
        //struct snd_soc_pcm_stream capture;
        .capture = {
            .stream_name = "CPU-Capture",
            .channels_min = 1,
            .channels_max = 8,
            .rates = SNDRV_PCM_RATE_8000_192000,
            .formats = FSL_ESAI_FORMATS,
        },
        .ops = &fsl_esai_dai_ops,
    };
  ```
* Machine route
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

## snd_soc_register_card调用

```C
static int snd_soc_instantiate_card(struct snd_soc_card *card)
{
    [...省略]

    /* probe all components used by DAI links on this card */
    for_each_comp_order(order) {
        for_each_card_rtds(card, rtd) {
            ret = soc_probe_link_components(card, rtd, order);          // <------
            if (ret < 0) {
                dev_err(card->dev,
                    "ASoC: failed to instantiate card %d\n",
                    ret);
                goto probe_dai_err;
            }
        }
    }

    [...省略]

    return snd_soc_bind_card(card);
}
EXPORT_SYMBOL_GPL(snd_soc_register_card);
```

* static int snd_soc_instantiate_card(struct snd_soc_card *card)
  * ret = soc_probe_link_components(card, rtd, order);
    * ret = soc_probe_component(card, component);

## soc_probe_link_components代码分析

* [soc_probe_link_components Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L2053)
* [i.MX6 soc dai_link添加link-component数组结构展开](0005_dai_link_component.md#dai_link添加link-component数组结构展开)
* [snd_soc_component主要域](0002_ASoC_Codec_Class_Driver.md#snd_soc_component主要域)

```C
static int soc_probe_component(struct snd_soc_card *card,
    struct snd_soc_component *component)
{
    struct snd_soc_dapm_context *dapm =
            snd_soc_component_get_dapm(component);
    struct snd_soc_dai *dai;
    int ret;

    if (!strcmp(component->name, "snd-soc-dummy"))              // component->name看下面小结《snd_soc_component_initialize分析》，这个snd-soc-dummy在dai_link注册的地方多次出现
        return 0;

    if (component->card) {                                      // 检查component的card归属
        if (component->card != card) {
            dev_err(component->dev,
                "Trying to bind component to card \"%s\" but is already bound to card \"%s\"\n",
                card->name, component->card->name);
            return -ENODEV;
        }
        return 0;
    }

    if (!try_module_get(component->dev->driver->owner))
        return -ENODEV;

    component->card = card;
    dapm->card = card;
    soc_set_name_prefix(card, component);

    soc_init_component_debugfs(component);

    if (component->driver->dapm_widgets) {                          // 注册component driver中的widget
        ret = snd_soc_dapm_new_controls(dapm,
                    component->driver->dapm_widgets,
                    component->driver->num_dapm_widgets);

        if (ret != 0) {
            dev_err(component->dev,
                "Failed to create new controls %d\n", ret);
            goto err_probe;
        }
    }

    for_each_component_dais(component, dai) {
        ret = snd_soc_dapm_new_dai_widgets(dapm, dai);              // 将dai(snd_soc_dai_driver) playback/capture转换成dai widget
        if (ret != 0) {
            dev_err(component->dev,
                "Failed to create DAI widgets %d\n", ret);
            goto err_probe;
        }
    }

    if (component->driver->probe) {
        ret = component->driver->probe(component);                  // 执行driver预设的probe函数
        if (ret < 0) {
            dev_err(component->dev,
                "ASoC: failed to probe component %d\n", ret);
            goto err_probe;
        }

        WARN(dapm->idle_bias_off &&
            dapm->bias_level != SND_SOC_BIAS_OFF,
            "codec %s can not start from non-off bias with idle_bias_off==1\n",
            component->name);
    }

    /* machine specific init */
    if (component->init) {                                          // 如果存在init程序，那就执行init程序
        ret = component->init(component);
        if (ret < 0) {
            dev_err(component->dev,
                "Failed to do machine specific init %d\n", ret);
            goto err_probe;
        }
    }

    if (component->driver->controls)
        snd_soc_add_component_controls(component,                   // 添加驱动中预置的controls
                           component->driver->controls,
                           component->driver->num_controls);
    if (component->driver->dapm_routes)                             // 添加驱动中预置的routes
        snd_soc_dapm_add_routes(dapm,
                    component->driver->dapm_routes,
                    component->driver->num_dapm_routes);

    list_add(&dapm->list, &card->dapm_list);
    /* see for_each_card_components */
    list_add(&component->card_list, &card->component_dev_list);

    return 0;

err_probe:
    soc_cleanup_component_debugfs(component);
    component->card = NULL;
    module_put(component->dev->driver->owner);

    return ret;
}
```

## snd_soc_component_initialize分析

```C
static int snd_soc_component_initialize(struct snd_soc_component *component,
    const struct snd_soc_component_driver *driver, struct device *dev)
{
    struct snd_soc_dapm_context *dapm;

    component->name = fmt_single_name(dev, &component->id);                     // <--- component名称来自这里
    if (!component->name) {
        dev_err(dev, "ASoC: Failed to allocate name\n");
        return -ENOMEM;
    }

    component->dev = dev;
    component->driver = driver;

    dapm = snd_soc_component_get_dapm(component);
    dapm->dev = dev;
    dapm->component = component;
    dapm->bias_level = SND_SOC_BIAS_OFF;
    dapm->idle_bias_off = !driver->idle_bias_on;
    dapm->suspend_bias_off = driver->suspend_bias_off;
    if (driver->seq_notifier)
        dapm->seq_notifier = snd_soc_component_seq_notifier;
    if (driver->stream_event)
        dapm->stream_event = snd_soc_component_stream_event;
    if (driver->set_bias_level)
        dapm->set_bias_level = snd_soc_component_set_bias_level;

    INIT_LIST_HEAD(&component->dai_list);
    mutex_init(&component->io_mutex);

    return 0;
}
```
* fmt_single_name
  ```C
  static char *fmt_single_name(struct device *dev, int *id)
  {
      char *found, name[NAME_SIZE];
      int id1, id2;
  
      if (dev_name(dev) == NULL)
          return NULL;
  
      strlcpy(name, dev_name(dev), NAME_SIZE);                                    // 提取设备名称
  
      /* are we a "%s.%d" name (platform and SPI components) */
      found = strstr(name, dev->driver->name);                                    // 查找字符串，这个应该找不到的样子
      if (found) {
          /* get ID */
          if (sscanf(&found[strlen(dev->driver->name)], ".%d", id) == 1) {
  
              /* discard ID from name if ID == -1 */
              if (*id == -1)
                  found[strlen(dev->driver->name)] = '\0';
          }
  
      } else {
          /* I2C component devices are named "bus-addr" */
          if (sscanf(name, "%x-%x", &id1, &id2) == 2) {
              char tmp[NAME_SIZE];
  
              /* create unique ID number from I2C addr and bus */
              *id = ((id1 & 0xffff) << 16) + id2;
  
              /* sanitize component name for DAI link creation */
              snprintf(tmp, NAME_SIZE, "%s.%s", dev->driver->name,
                   name);
              strlcpy(name, tmp, NAME_SIZE);
          } else
              *id = 0;
      }
  
      return kstrdup(name, GFP_KERNEL);
  }
  ```