# sound-xtor

## 设备树

### xtor-audio

```
[...省略]
sound-xtor {
    compatible = "fsl,imx-audio-xtor";
    model = "xtor-audio";
    cpu-dai = <&sai0>;
    status = "okay";
};
[...省略]
```

### sai0

```
[...省略]
sai0: sai@59040000 {
    compatible = "fsl,imx8qm-sai";
    reg = <0x0 0x59040000 0x0 0x10000>;
    interrupts = <GIC_SPI 314 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&clk IMX8QM_AUD_SAI_0_IPG>,
        <&clk IMX8QM_CLK_DUMMY>,
        <&clk IMX8QM_AUD_SAI_0_MCLK>,
        <&clk IMX8QM_CLK_DUMMY>,
        <&clk IMX8QM_CLK_DUMMY>;
    clock-names = "bus", "mclk0", "mclk1", "mclk2", "mclk3";
    dma-names = "rx", "tx";
    dmas = <&edma2 12 0 1>, <&edma2 13 0 0>;
    status = "disabled";
    power-domains = <&pd_sai0>;
};

[...省略]

&sai0 {
    assigned-clocks = <&clk IMX8QM_AUD_PLL0_DIV>,
            <&clk IMX8QM_AUD_ACM_AUD_PLL_CLK0_DIV>,
            <&clk IMX8QM_AUD_ACM_AUD_REC_CLK0_DIV>,
            <&clk IMX8QM_AUD_SAI_0_MCLK>;
    assigned-clock-rates = <786432000>, <49152000>, <12288000>, <49152000>;
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_sai0>;
    status = "okay";
};

[...省略]

pinctrl_sai0: sai0grp {
    fsl,pins = <
        SC_P_SPI0_CS1_AUD_SAI0_TXC      0x0600004c
        SC_P_SPI2_CS1_AUD_SAI0_TXFS     0x0600004c
        SC_P_SAI1_RXFS_AUD_SAI0_RXD     0x0600004c
        SC_P_SAI1_RXC_AUD_SAI0_TXD      0x0600006c
    >;
};
[...省略]
```

引脚配置说明：vendor/nxp-opensource/kernel_imx/Documentation/devicetree/bindings/pinctrl

## 主要修改函数

```
static int imx_xtor_hw_params(struct snd_pcm_substream *substream,
                     struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct imx_xtor_data *data = snd_soc_card_get_drvdata(rtd->card);
    bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
    struct cpu_priv *cpu_priv = &data->cpu_priv;
    struct device *dev = rtd->card->dev;
    u32 channels = params_channels(params);
    unsigned int fmt = SND_SOC_DAI_FORMAT_I2S | SND_SOC_DAIFMT_NB_NF;
    int ret, dir;

    /* For playback the XTOR is slave, and for record is master */
    //fmt |= tx ? SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;
    //dir = tx ? SND_SOC_CLOCK_OUT : SND_SOC_CLOCK_IN;
    fmt |= SND_SOC_DAIFMT_CBM_CFM;
    dir = SND_SOC_CLOCK_IN;
    
    /* set cpu DAI configuration */
    ret = snd_soc_dai_set_fmt(rtd->cpu_dai, fmt);
    if (ret) {
        dev_err(dev, "failed to set cpu dai fmt: %d\n", ret);
        return ret;
    }

    /* Specific configurations of DAIs starts from here */
    ret = snd_soc_dai_set_sysclk(rtd->cpu_dai, cpu_priv->sysclk_id[tx],
                     0, dir);
    if (ret) {
        dev_err(dev, "failed to set cpu sysclk: %d\n", ret);
        return ret;
    }

    ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, BIT(channels) - 1,
        // BIT(channels) - 1, cpu_priv->slots, params_width(params));
        BIT(channels) - 1, 0, params_width(params));
    if (ret) {
        dev_err(dev, "failed to set cpu dai tdm slot: %d\n", ret);
        return ret;
    }

    return 0;
}
```