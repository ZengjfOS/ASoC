# ASoC Platform Driver

主要是注册DAI，没有注册route、widget、control，最终填充 [snd_soc_component主要域](0002_ASoC_Codec_Class_Driver.md#snd_soc_component主要域)；

## 参考文档

* [ASoC Platform Driver](https://www.kernel.org/doc/html/v4.10/sound/soc/platform.html)

## Audio DMA Driver

https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/fsl/fsl_esai.c#L915

## SoC DAI esai Driver

* fsl_esai_component
  ```C
  static const struct snd_soc_component_driver fsl_esai_component = {
      .name        = "fsl-esai",
  };
  ```
* fsl_esai_dai
  ```C
  static const struct snd_soc_dai_ops fsl_esai_dai_ops = {
      .startup = fsl_esai_startup,
      .shutdown = fsl_esai_shutdown,
      .trigger = fsl_esai_trigger,
      .hw_params = fsl_esai_hw_params,
      .set_sysclk = fsl_esai_set_dai_sysclk,
      .set_fmt = fsl_esai_set_dai_fmt,
      .set_tdm_slot = fsl_esai_set_dai_tdm_slot,
  };

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
      .ops = &fsl_esai_dai_ops,                                 // 这里的ops会转移到生成的pcm的ops
  };
  ```

## cs42xx8_probe解析

[fsl_esai_probe source code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/fsl/fsl_esai.c#L796)

```C
static int fsl_esai_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    struct fsl_esai *esai_priv;
    struct resource *res;
    const __be32 *iprop;
    void __iomem *regs;
    int irq, ret;

    esai_priv = devm_kzalloc(&pdev->dev, sizeof(*esai_priv), GFP_KERNEL);
    if (!esai_priv)
        return -ENOMEM;

    esai_priv->pdev = pdev;
    snprintf(esai_priv->name, sizeof(esai_priv->name), "%pOFn", np);

    /* Get the addresses and IRQ */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);

    esai_priv->regmap = devm_regmap_init_mmio_clk(&pdev->dev,
            "core", regs, &fsl_esai_regmap_config);
    if (IS_ERR(esai_priv->regmap)) {
        dev_err(&pdev->dev, "failed to init regmap: %ld\n",
                PTR_ERR(esai_priv->regmap));
        return PTR_ERR(esai_priv->regmap);
    }

    esai_priv->coreclk = devm_clk_get(&pdev->dev, "core");
    if (IS_ERR(esai_priv->coreclk)) {
        dev_err(&pdev->dev, "failed to get core clock: %ld\n",
                PTR_ERR(esai_priv->coreclk));
        return PTR_ERR(esai_priv->coreclk);
    }

    esai_priv->extalclk = devm_clk_get(&pdev->dev, "extal");
    if (IS_ERR(esai_priv->extalclk))
        dev_warn(&pdev->dev, "failed to get extal clock: %ld\n",
                PTR_ERR(esai_priv->extalclk));

    esai_priv->fsysclk = devm_clk_get(&pdev->dev, "fsys");
    if (IS_ERR(esai_priv->fsysclk))
        dev_warn(&pdev->dev, "failed to get fsys clock: %ld\n",
                PTR_ERR(esai_priv->fsysclk));

    esai_priv->spbaclk = devm_clk_get(&pdev->dev, "spba");
    if (IS_ERR(esai_priv->spbaclk))
        dev_warn(&pdev->dev, "failed to get spba clock: %ld\n",
                PTR_ERR(esai_priv->spbaclk));

    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        dev_err(&pdev->dev, "no irq for node %s\n", pdev->name);
        return irq;
    }

    ret = devm_request_irq(&pdev->dev, irq, esai_isr, 0,
                   esai_priv->name, esai_priv);
    if (ret) {
        dev_err(&pdev->dev, "failed to claim irq %u\n", irq);
        return ret;
    }

    /* Set a default slot number */
    esai_priv->slots = 2;

    /* Set a default master/slave state */
    esai_priv->slave_mode = true;

    /* Determine the FIFO depth */
    iprop = of_get_property(np, "fsl,fifo-depth", NULL);
    if (iprop)
        esai_priv->fifo_depth = be32_to_cpup(iprop);
    else
        esai_priv->fifo_depth = 64;

    esai_priv->dma_params_tx.maxburst = 16;
    esai_priv->dma_params_rx.maxburst = 16;
    esai_priv->dma_params_tx.addr = res->start + REG_ESAI_ETDR;
    esai_priv->dma_params_rx.addr = res->start + REG_ESAI_ERDR;

    esai_priv->synchronous =
        of_property_read_bool(np, "fsl,esai-synchronous");

    /* Implement full symmetry for synchronous mode */
    if (esai_priv->synchronous) {
        fsl_esai_dai.symmetric_rates = 1;
        fsl_esai_dai.symmetric_channels = 1;
        fsl_esai_dai.symmetric_samplebits = 1;
    }

    dev_set_drvdata(&pdev->dev, esai_priv);

    /* Reset ESAI unit */
    ret = regmap_write(esai_priv->regmap, REG_ESAI_ECR, ESAI_ECR_ERST);
    if (ret) {
        dev_err(&pdev->dev, "failed to reset ESAI: %d\n", ret);
        return ret;
    }

    /*
     * We need to enable ESAI so as to access some of its registers.
     * Otherwise, we would fail to dump regmap from user space.
     */
    ret = regmap_write(esai_priv->regmap, REG_ESAI_ECR, ESAI_ECR_ESAIEN);
    if (ret) {
        dev_err(&pdev->dev, "failed to enable ESAI: %d\n", ret);
        return ret;
    }

    ret = devm_snd_soc_register_component(&pdev->dev, &fsl_esai_component,              // 注册component
                          &fsl_esai_dai, 1);
    if (ret) {
        dev_err(&pdev->dev, "failed to register DAI: %d\n", ret);
        return ret;
    }

    ret = imx_pcm_dma_init(pdev, IMX_ESAI_DMABUF_SIZE);
    if (ret)
        dev_err(&pdev->dev, "failed to init imx pcm dma: %d\n", ret);

    return ret;
}
```

## ESAI、DMA component注册流程：

```
* static int fsl_esai_probe(struct platform_device *pdev)
  * ret = devm_snd_soc_register_component(&pdev->dev, &fsl_esai_component, &fsl_esai_dai, 1);
    * ret = snd_soc_register_component(dev, cmpnt_drv, dai_drv, num_dai);
      * component = devm_kzalloc(dev, sizeof(*component), GFP_KERNEL);
      * return snd_soc_add_component(dev, component, component_driver, dai_drv, num_dai);
        * ret = snd_soc_component_initialize(component, component_driver, dev);
          * component->name = fmt_single_name(dev, &component->id);
            ```
            static char *fmt_single_name(struct device *dev, int *id)
            {
                [...省略]
                strlcpy(name, dev_name(dev), NAME_SIZE);
          
                /* are we a "%s.%d" name (platform and SPI components) */
                found = strstr(name, dev->driver->name);
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
        * ret = snd_soc_register_dais(component, dai_drv, num_dai);
        * snd_soc_component_add(component);
          * list_add(&component->list, &component_list);
            * static LIST_HEAD(component_list);
        * snd_soc_try_rebind_card();
          * if (!snd_soc_bind_card(card))
            * ret = snd_soc_instantiate_card(card);
  * ret = imx_pcm_dma_init(pdev, IMX_ESAI_DMABUF_SIZE);                                               // 申请DMA
    * config = devm_kzalloc(&pdev->dev, sizeof(struct snd_dmaengine_pcm_config), GFP_KERNEL);
    * *config = imx_dmaengine_pcm_config;
    * return devm_snd_dmaengine_pcm_register(&pdev->dev, config, SND_DMAENGINE_PCM_FLAG_COMPAT);
      * ret = dmaengine_pcm_request_chan_of(pcm, dev, config);
      * if (config && config->process) ret = snd_soc_add_component(dev, &pcm->component, &dmaengine_pcm_component_process, NULL, 0);
      * else ret = snd_soc_add_component(dev, &pcm->component, &dmaengine_pcm_component, NULL, 0);
```