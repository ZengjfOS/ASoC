# Control Device

* 主要是知道声卡控制设备是在如何注册；
* 最终声卡控制设备的回调函数是：[snd_ctl_f_ops](https://elixir.bootlin.com/linux/latest/source/sound/core/control.c#L1829)

## 参考文档

* [Linux ALSA声卡驱动之四：Control设备的创建](https://blog.csdn.net/droidphone/article/details/6409983)

## Control被创建的函数调用

[Codec controls](0002_ASoC_Codec_Class_Driver.md#controls)


* snd_soc_instantiate_card
  ```C
  static int snd_soc_instantiate_card(struct snd_soc_card *card)
  {
      [...省略]
      /* card bind complete so register a sound card */
      ret = snd_card_new(card->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,         // <----
              card->owner, 0, &card->snd_card);
      if (ret < 0) {
          dev_err(card->dev,
              "ASoC: can't create sound card for card %s: %d\n",
              card->name, ret);
          goto base_error;
      }
      [...省略]
  }
  ```
* snd_card_new
  ```C
  int snd_card_new(struct device *parent, int idx, const char *xid,
              struct module *module, int extra_size,
              struct snd_card **card_ret)
  {
      [...省略]
      snprintf(card->irq_descr, sizeof(card->irq_descr), "%s:%s",
           dev_driver_string(card->dev), dev_name(&card->card_dev));
  
      /* the control interface cannot be accessed from the user space until */
      /* snd_cards_bitmask and snd_cards are set with snd_card_register */
      err = snd_ctl_create(card);                                                   // <-----
      if (err < 0) {
          dev_err(parent, "unable to register control minors\n");
          goto __error;
      }
      err = snd_info_card_create(card);
      if (err < 0) {
          dev_err(parent, "unable to create card info\n");
          goto __error_ctl;
      }
      *card_ret = card;
      return 0;
      [...省略]
  }
  ```
* snd_ctl_create
  ```C
  int snd_ctl_create(struct snd_card *card)
  {
      static struct snd_device_ops ops = {
          .dev_free = snd_ctl_dev_free,
          .dev_register =    snd_ctl_dev_register,                          // <-----
          .dev_disconnect = snd_ctl_dev_disconnect,
      };
      int err;
  
      if (snd_BUG_ON(!card))
          return -ENXIO;
      if (snd_BUG_ON(card->number < 0 || card->number >= SNDRV_CARDS))
          return -ENXIO;
  
      snd_device_initialize(&card->ctl_dev, card);
      dev_set_name(&card->ctl_dev, "controlC%d", card->number);             // 设备节点名称
  
      err = snd_device_new(card, SNDRV_DEV_CONTROL, card, &ops);            // ops中的snd_ctl_dev_register接口，实际会被下边的snd_card_register接口调用到，snd_ctl_dev_register接口调用snd_register_device，传递snd_ctl_f_ops，这个ops就是实际使用到的control设备的文件操作.
      if (err < 0)
          put_device(&card->ctl_dev);
      return err;
  }
  ```
* snd_ctl_dev_register
  ```C
  static int snd_ctl_dev_register(struct snd_device *device)
  {
      struct snd_card *card = device->device_data;
  
      return snd_register_device(SNDRV_DEVICE_TYPE_CONTROL, card, -1,       // <-----
                     &snd_ctl_f_ops, card, &card->ctl_dev);
  }
  ```
* snd_ctl_f_ops
  ```C
  static const struct file_operations snd_ctl_f_ops =
  {
      .owner =    THIS_MODULE,
      .read =        snd_ctl_read,
      .open =        snd_ctl_open,
      .release =    snd_ctl_release,
      .llseek =    no_llseek,
      .poll =        snd_ctl_poll,
      .unlocked_ioctl =    snd_ctl_ioctl,
      .compat_ioctl =    snd_ctl_ioctl_compat,
      .fasync =    snd_ctl_fasync,
  };
  ```