# ASoC Codec Class Driver

Codec probe一般来说是获取、填充component、dai driver数据，数据一般都是标准结构，提供这些数据就完成了大部分工作：
* [struct snd_soc_component_driver](https://elixir.bootlin.com/linux/v5.0-rc6/source/include/sound/soc.h#L755)：主要定义route和widget
* [struct snd_soc_dai_driver](https://elixir.bootlin.com/linux/v5.0-rc6/source/include/sound/soc-dai.h#L254)：主要定义dai
* [struct snd_soc_component](https://elixir.bootlin.com/linux/v5.0-rc6/source/include/sound/soc.h#L821)

## snd_soc_component主要域

Codec `struct snd_soc_component_driver` / `struct snd_soc_dai_driver` 最终生成的component主要是填充以下两个域；

```C
struct snd_soc_component {
    [...省略]
    const struct snd_soc_component_driver *driver;      // Codec对应的component的driver

    struct list_head dai_list;                          // codec对应的component的dai list，一般是一个dai，当然可以有很多个
    [...省略]
};
```


## 参考文档

* [ASoC Codec Class Driver](https://www.kernel.org/doc/html/v4.11/sound/soc/codec.html)
* [CS42888 Codec Driver](https://github.com/torvalds/linux/blob/master/sound/soc/codecs/cs42xx8-i2c.c)
* [CS42888 108 dB, 192 kHz 4-in, 8-out Multi-channel Codec](https://www.cirrus.com/products/cs42888/)
* [linux ALSA & ASOC (3) — widget 、route](https://blog.csdn.net/u012719256/article/details/77507479)
* [ALSA声卡驱动的DAPM（一）-DPAM详解](https://cloud.tencent.com/developer/article/1078002)

## 简要说明

Each codec class driver must provide the following features:-

* [Codec DAI and PCM configuration](#Codec-DAI-and-PCM-configuration)
* [Codec control IO - using RegMap API](#Codec-control-IO)
* [Mixers and audio controls](#Mixers-and-audio-controls)
* Codec audio operations
* DAPM description.
* DAPM event handler.

Optionally, codec drivers can also provide:-

* DAC Digital mute control.

## Codec DAI and PCM configuration

```C
static const struct snd_soc_dai_ops cs42xx8_dai_ops = {
    .set_fmt        = cs42xx8_set_dai_fmt,
    .set_sysclk     = cs42xx8_set_dai_sysclk,
    .hw_params      = cs42xx8_hw_params,
    .digital_mute   = cs42xx8_digital_mute,
};

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
    .ops = &cs42xx8_dai_ops,                                    // 这里的ops会转移到生成的pcm的ops
};
```

## Codec control IO

```C
static const struct reg_default cs42xx8_reg[] = {
    { 0x02, 0x00 },   /* Power Control */
    { 0x03, 0xF0 },   /* Functional Mode */
    { 0x04, 0x46 },   /* Interface Formats */
    { 0x05, 0x00 },   /* ADC Control & DAC De-Emphasis */
    { 0x06, 0x10 },   /* Transition Control */
    { 0x07, 0x00 },   /* DAC Channel Mute */
    { 0x08, 0x00 },   /* Volume Control AOUT1 */
    { 0x09, 0x00 },   /* Volume Control AOUT2 */
    { 0x0a, 0x00 },   /* Volume Control AOUT3 */
    { 0x0b, 0x00 },   /* Volume Control AOUT4 */
    { 0x0c, 0x00 },   /* Volume Control AOUT5 */
    { 0x0d, 0x00 },   /* Volume Control AOUT6 */
    { 0x0e, 0x00 },   /* Volume Control AOUT7 */
    { 0x0f, 0x00 },   /* Volume Control AOUT8 */
    { 0x10, 0x00 },   /* DAC Channel Invert */
    { 0x11, 0x00 },   /* Volume Control AIN1 */
    { 0x12, 0x00 },   /* Volume Control AIN2 */
    { 0x13, 0x00 },   /* Volume Control AIN3 */
    { 0x14, 0x00 },   /* Volume Control AIN4 */
    { 0x15, 0x00 },   /* Volume Control AIN5 */
    { 0x16, 0x00 },   /* Volume Control AIN6 */
    { 0x17, 0x00 },   /* ADC Channel Invert */
    { 0x18, 0x00 },   /* Status Control */
    { 0x1a, 0x00 },   /* Status Mask */
    { 0x1b, 0x00 },   /* MUTEC Pin Control */
};

[...省略]

const struct regmap_config cs42xx8_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .max_register = CS42XX8_LASTREG,
    .reg_defaults = cs42xx8_reg,
    .num_reg_defaults = ARRAY_SIZE(cs42xx8_reg),
    .volatile_reg = cs42xx8_volatile_register,
    .writeable_reg = cs42xx8_writeable_register,
    .cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL_GPL(cs42xx8_regmap_config);
```

## Mixers and audio controls

```C
static const struct snd_soc_component_driver cs42xx8_driver = {
    .probe              = cs42xx8_component_probe,
    .controls           = cs42xx8_snd_controls,
    .num_controls       = ARRAY_SIZE(cs42xx8_snd_controls),
    .dapm_widgets       = cs42xx8_dapm_widgets,
    .num_dapm_widgets   = ARRAY_SIZE(cs42xx8_dapm_widgets),
    .dapm_routes        = cs42xx8_dapm_routes,
    .num_dapm_routes    = ARRAY_SIZE(cs42xx8_dapm_routes),
    .use_pmdown_time    = 1,
    .endianness         = 1,
    .non_legacy_dai_naming    = 1,
};
```

* [controls](#controls)
* [dapm_widgets](#dapm_widgets)
* [dapm_routes](#dapm_routes)

### controls

```C
static const struct snd_kcontrol_new cs42xx8_snd_controls[] = {
    /**
     * #define SOC_DOUBLE_R_TLV(xname, reg_left, reg_right, xshift, xmax, xinvert, tlv_array) \
     * {    .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname),\
     *     .access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |\
     *          SNDRV_CTL_ELEM_ACCESS_READWRITE,\
     *     .tlv.p = (tlv_array), \
     *     .info = snd_soc_info_volsw, \
     *     .get = snd_soc_get_volsw, .put = snd_soc_put_volsw, \
     *     .private_value = SOC_DOUBLE_R_VALUE(reg_left, reg_right, xshift, \
     *                         xmax, xinvert) }
     */
    SOC_DOUBLE_R_TLV("DAC1 Playback Volume", CS42XX8_VOLAOUT1,
             CS42XX8_VOLAOUT2, 0, 0xff, 1, dac_tlv),
    SOC_DOUBLE_R_TLV("DAC2 Playback Volume", CS42XX8_VOLAOUT3,
             CS42XX8_VOLAOUT4, 0, 0xff, 1, dac_tlv),
    SOC_DOUBLE_R_TLV("DAC3 Playback Volume", CS42XX8_VOLAOUT5,
             CS42XX8_VOLAOUT6, 0, 0xff, 1, dac_tlv),
    SOC_DOUBLE_R_TLV("DAC4 Playback Volume", CS42XX8_VOLAOUT7,
             CS42XX8_VOLAOUT8, 0, 0xff, 1, dac_tlv),
    SOC_DOUBLE_R_S_TLV("ADC1 Capture Volume", CS42XX8_VOLAIN1,
               CS42XX8_VOLAIN2, 0, -0x80, 0x30, 7, 0, adc_tlv),
    SOC_DOUBLE_R_S_TLV("ADC2 Capture Volume", CS42XX8_VOLAIN3,
               CS42XX8_VOLAIN4, 0, -0x80, 0x30, 7, 0, adc_tlv),
    /**
     * #define SOC_DOUBLE(xname, reg, shift_left, shift_right, max, invert) \
     * {    .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname),\
     *     .info = snd_soc_info_volsw, .get = snd_soc_get_volsw, \
     *     .put = snd_soc_put_volsw, \
     *     .private_value = SOC_DOUBLE_VALUE(reg, shift_left, shift_right, \
     *                       max, invert, 0) }
     */
    SOC_DOUBLE("DAC1 Invert Switch", CS42XX8_DACINV, 0, 1, 1, 0),
    SOC_DOUBLE("DAC2 Invert Switch", CS42XX8_DACINV, 2, 3, 1, 0),
    SOC_DOUBLE("DAC3 Invert Switch", CS42XX8_DACINV, 4, 5, 1, 0),
    SOC_DOUBLE("DAC4 Invert Switch", CS42XX8_DACINV, 6, 7, 1, 0),
    SOC_DOUBLE("ADC1 Invert Switch", CS42XX8_ADCINV, 0, 1, 1, 0),
    SOC_DOUBLE("ADC2 Invert Switch", CS42XX8_ADCINV, 2, 3, 1, 0),
    /**
     * #define SOC_SINGLE(xname, reg, shift, max, invert) \
     * {    .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
     *     .info = snd_soc_info_volsw, .get = snd_soc_get_volsw,\
     *     .put = snd_soc_put_volsw, \
     *     .private_value = SOC_SINGLE_VALUE(reg, shift, max, invert, 0) }
     */
    SOC_SINGLE("ADC High-Pass Filter Switch", CS42XX8_ADCCTL, 7, 1, 1),
    SOC_SINGLE("DAC De-emphasis Switch", CS42XX8_ADCCTL, 5, 1, 0),
    /**
     * #define SOC_ENUM(xname, xenum) \
     * {    .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,\
     *     .info = snd_soc_info_enum_double, \
     *     .get = snd_soc_get_enum_double, .put = snd_soc_put_enum_double, \
     *     .private_value = (unsigned long)&xenum }
     */
    SOC_ENUM("ADC1 Single Ended Mode Switch", adc1_single_enum),
    SOC_ENUM("ADC2 Single Ended Mode Switch", adc2_single_enum),
    SOC_SINGLE("DAC Single Volume Control Switch", CS42XX8_TXCTL, 7, 1, 0),
    SOC_ENUM("DAC Soft Ramp & Zero Cross Control Switch", dac_szc_enum),
    SOC_SINGLE("DAC Auto Mute Switch", CS42XX8_TXCTL, 4, 1, 0),
    SOC_SINGLE("Mute ADC Serial Port Switch", CS42XX8_TXCTL, 3, 1, 0),
    SOC_SINGLE("ADC Single Volume Control Switch", CS42XX8_TXCTL, 2, 1, 0),
    SOC_ENUM("ADC Soft Ramp & Zero Cross Control Switch", adc_szc_enum),
};
```

### dapm_widgets

```C
static const struct snd_soc_dapm_widget cs42xx8_dapm_widgets[] = {
    /**
     * #define SND_SOC_DAPM_DAC(wname, stname, wreg, wshift, winvert) \
     * {    .id = snd_soc_dapm_dac, .name = wname, .sname = stname, \
     *     SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert) }
     */
    SND_SOC_DAPM_DAC("DAC1", "Playback", CS42XX8_PWRCTL, 1, 1),
    SND_SOC_DAPM_DAC("DAC2", "Playback", CS42XX8_PWRCTL, 2, 1),
    SND_SOC_DAPM_DAC("DAC3", "Playback", CS42XX8_PWRCTL, 3, 1),
    SND_SOC_DAPM_DAC("DAC4", "Playback", CS42XX8_PWRCTL, 4, 1),

    /**
     * #define SND_SOC_DAPM_OUTPUT(wname) \
     * {    .id = snd_soc_dapm_output, .name = wname, .kcontrol_news = NULL, \
     *     .num_kcontrols = 0, .reg = SND_SOC_NOPM }
     */
    SND_SOC_DAPM_OUTPUT("AOUT1L"),
    SND_SOC_DAPM_OUTPUT("AOUT1R"),
    SND_SOC_DAPM_OUTPUT("AOUT2L"),
    SND_SOC_DAPM_OUTPUT("AOUT2R"),
    SND_SOC_DAPM_OUTPUT("AOUT3L"),
    SND_SOC_DAPM_OUTPUT("AOUT3R"),
    SND_SOC_DAPM_OUTPUT("AOUT4L"),
    SND_SOC_DAPM_OUTPUT("AOUT4R"),

    /**
     * #define SND_SOC_DAPM_ADC(wname, stname, wreg, wshift, winvert) \
     * {    .id = snd_soc_dapm_adc, .name = wname, .sname = stname, \
     *     SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert), }
     */
    SND_SOC_DAPM_ADC("ADC1", "Capture", CS42XX8_PWRCTL, 5, 1),
    SND_SOC_DAPM_ADC("ADC2", "Capture", CS42XX8_PWRCTL, 6, 1),

    /**
     * #define SND_SOC_DAPM_INPUT(wname) \
     * {    .id = snd_soc_dapm_input, .name = wname, .kcontrol_news = NULL, \
     *     .num_kcontrols = 0, .reg = SND_SOC_NOPM }
     */
    SND_SOC_DAPM_INPUT("AIN1L"),
    SND_SOC_DAPM_INPUT("AIN1R"),
    SND_SOC_DAPM_INPUT("AIN2L"),
    SND_SOC_DAPM_INPUT("AIN2R"),

    /**
     * #define SND_SOC_DAPM_SUPPLY(wname, wreg, wshift, winvert, wevent, wflags) \
     * {    .id = snd_soc_dapm_supply, .name = wname, \
     *     SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert), \
     *     .event = wevent, .event_flags = wflags}
     */
    SND_SOC_DAPM_SUPPLY("PWR", CS42XX8_PWRCTL, 0, 1, NULL, 0),
};
```

### dapm_routes

```C
static const struct snd_soc_dapm_route cs42xx8_dapm_routes[] = {
    /* Playback */
    { "AOUT1L", NULL, "DAC1" },
    { "AOUT1R", NULL, "DAC1" },
    { "DAC1", NULL, "PWR" },

    { "AOUT2L", NULL, "DAC2" },
    { "AOUT2R", NULL, "DAC2" },
    { "DAC2", NULL, "PWR" },

    { "AOUT3L", NULL, "DAC3" },
    { "AOUT3R", NULL, "DAC3" },
    { "DAC3", NULL, "PWR" },

    { "AOUT4L", NULL, "DAC4" },
    { "AOUT4R", NULL, "DAC4" },
    { "DAC4", NULL, "PWR" },

    /* Capture */
    { "ADC1", NULL, "AIN1L" },
    { "ADC1", NULL, "AIN1R" },
    { "ADC1", NULL, "PWR" },

    { "ADC2", NULL, "AIN2L" },
    { "ADC2", NULL, "AIN2R" },
    { "ADC2", NULL, "PWR" },
};
```

## cs42xx8_probe解析

[cs42xx8_probe source code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/codecs/cs42xx8.c#L442)

```C
int cs42xx8_probe(struct device *dev, struct regmap *regmap)
{
    const struct of_device_id *of_id;
    struct cs42xx8_priv *cs42xx8;
    int ret, val, i;

    if (IS_ERR(regmap)) {
        ret = PTR_ERR(regmap);
        dev_err(dev, "failed to allocate regmap: %d\n", ret);
        return ret;
    }

    cs42xx8 = devm_kzalloc(dev, sizeof(*cs42xx8), GFP_KERNEL);
    if (cs42xx8 == NULL)
        return -ENOMEM;

    cs42xx8->regmap = regmap;
    dev_set_drvdata(dev, cs42xx8);                                              // dev->driver_data = data;

    /**
     * const struct of_device_id cs42xx8_of_match[] = {
     *     { .compatible = "cirrus,cs42448", .data = &cs42448_data, },
     *     { .compatible = "cirrus,cs42888", .data = &cs42888_data, },
     *     { /* sentinel */ }
     * };
     *
     * const struct cs42xx8_driver_data cs42888_data = {
     *     .name = "cs42888",
     *     .num_adcs = 2,
     * };
     */
    of_id = of_match_device(cs42xx8_of_match, dev);
    if (of_id)
        cs42xx8->drvdata = of_id->data;                                         // .name = "cs42888", .num_adcs = 2

    if (!cs42xx8->drvdata) {
        dev_err(dev, "failed to find driver data\n");
        return -EINVAL;
    }

    cs42xx8->clk = devm_clk_get(dev, "mclk");
    if (IS_ERR(cs42xx8->clk)) {
        dev_err(dev, "failed to get the clock: %ld\n",
                PTR_ERR(cs42xx8->clk));
        return -EINVAL;
    }

    cs42xx8->sysclk = clk_get_rate(cs42xx8->clk);

    /**
     * #define CS42XX8_NUM_SUPPLIES 4
     * static const char *const cs42xx8_supply_names[CS42XX8_NUM_SUPPLIES] = {
     *     "VA",
     *     "VD",
     *     "VLS",
     *     "VLC",
     * };
     */
    for (i = 0; i < ARRAY_SIZE(cs42xx8->supplies); i++)
        cs42xx8->supplies[i].supply = cs42xx8_supply_names[i];

    ret = devm_regulator_bulk_get(dev,
            ARRAY_SIZE(cs42xx8->supplies), cs42xx8->supplies);
    if (ret) {
        dev_err(dev, "failed to request supplies: %d\n", ret);
        return ret;
    }

    ret = regulator_bulk_enable(ARRAY_SIZE(cs42xx8->supplies),
                    cs42xx8->supplies);
    if (ret) {
        dev_err(dev, "failed to enable supplies: %d\n", ret);
        return ret;
    }

    /* Make sure hardware reset done */
    msleep(5);

    /* Validate the chip ID */
    ret = regmap_read(cs42xx8->regmap, CS42XX8_CHIPID, &val);
    if (ret < 0) {
        dev_err(dev, "failed to get device ID, ret = %d", ret);
        goto err_enable;
    }

    /* The top four bits of the chip ID should be 0000 */
    if (((val & CS42XX8_CHIPID_CHIP_ID_MASK) >> 4) != 0x00) {
        dev_err(dev, "unmatched chip ID: %d\n",
            (val & CS42XX8_CHIPID_CHIP_ID_MASK) >> 4);
        ret = -EINVAL;
        goto err_enable;
    }

    dev_info(dev, "found device, revision %X\n",
            val & CS42XX8_CHIPID_REV_ID_MASK);

    cs42xx8_dai.name = cs42xx8->drvdata->name;                                      // .name = "cs42888", .num_adcs = 2

    /* Each adc supports stereo input */
    cs42xx8_dai.capture.channels_max = cs42xx8->drvdata->num_adcs * 2;              // .name = "cs42888", .num_adcs = 2

    ret = devm_snd_soc_register_component(dev, &cs42xx8_driver, &cs42xx8_dai, 1);
    if (ret) {
        dev_err(dev, "failed to register component:%d\n", ret);
        goto err_enable;
    }

    regcache_cache_only(cs42xx8->regmap, true);

err_enable:
    regulator_bulk_disable(ARRAY_SIZE(cs42xx8->supplies),
                   cs42xx8->supplies);

    return ret;
}
EXPORT_SYMBOL_GPL(cs42xx8_probe);
```

## CS42xx8 component注册流程：

```
* static int cs42xx8_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
  * int cs42xx8_probe(struct device *dev, struct regmap *regmap)                            // 填充struct snd_soc_component_driver、struct snd_soc_dai_driver
    * ret = devm_snd_soc_register_component(dev, &cs42xx8_driver, &cs42xx8_dai, 1);
      * ret = snd_soc_register_component(dev, cmpnt_drv, dai_drv, num_dai);
        * component = devm_kzalloc(dev, sizeof(*component), GFP_KERNEL);                    // 创建struct snd_soc_component
        * return snd_soc_add_component(dev, component, component_driver, dai_drv, num_dai);
          * ret = snd_soc_component_initialize(component, component_driver, dev);           // 使用struct snd_soc_component_driver填充struct snd_soc_component
          * ret = snd_soc_register_dais(component, dai_drv, num_dai);                       // 使用struct snd_soc_dai_driver填充struct snd_soc_component
          * snd_soc_component_add(component);
            * list_add(&component->list, &component_list);                                  // 添加到内核component_list链表中
              * static LIST_HEAD(component_list);
          * snd_soc_try_rebind_card();                                                      // 这里貌似会重新初始化所有的声卡
            * if (!snd_soc_bind_card(card))
              * ret = snd_soc_instantiate_card(card);
```
