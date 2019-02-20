# widget dapm-control

## 参考文档

* [ALSA声卡驱动中的DAPM详解之五：建立widget之间的连接关系](https://blog.csdn.net/DroidPhone/article/details/14052861)

## 简要说明

* snd_soc_dapm_new_controls  
  实际上，这个函数只是创建widget的第一步，它为每个widget分配内存，初始化必要的字段，然后把这些widget挂在代表声卡的snd_soc_card的widgets链表字段中。要使widget之间具备连接能力，我们还需要第二个函数：
* snd_soc_dapm_new_widgets  
  这个函数会根据widget的信息，创建widget所需要的dapm kcontrol，这些dapm kcontol的状态变化，代表着音频路径的变化，从而影响着各个widget的电源状态。看到函数的名称可能会迷惑一下，实际上，snd_soc_dapm_new_controls的作用更多地是创建widget，而snd_soc_dapm_new_widget的作用则更多地是创建widget所包含的kcontrol。

## 调用流程

* [widget注册流程参考链接](0008_widget-control-route_Add_To_Card.md#widget)
* snd_soc_dapm_new_widgets
  ```C
  static int snd_soc_instantiate_card(struct snd_soc_card *card)
  {
      [...省略]
      snd_soc_dapm_new_widgets(card);                       // <-----
  
      ret = snd_card_register(card->snd_card);
      if (ret < 0) {
          dev_err(card->dev, "ASoC: failed to register soundcard %d\n",
                  ret);
          goto probe_aux_dev_err;
      }
      [...省略]
  }
  ```
* snd_soc_dapm_new_widgets
  ```C
  int snd_soc_dapm_new_widgets(struct snd_soc_card *card)
  {
      struct snd_soc_dapm_widget *w;
      unsigned int val;
  
      mutex_lock_nested(&card->dapm_mutex, SND_SOC_DAPM_CLASS_INIT);
  
      list_for_each_entry(w, &card->widgets, list)
      {
          if (w->new)
              continue;
  
          if (w->num_kcontrols) {
              w->kcontrols = kcalloc(w->num_kcontrols,
                          sizeof(struct snd_kcontrol *),
                          GFP_KERNEL);
              if (!w->kcontrols) {
                  mutex_unlock(&card->dapm_mutex);
                  return -ENOMEM;
              }
          }
  
          switch(w->id) {                               // 根据widget id创建不同的mixer/mux/gpa/dai_link
          case snd_soc_dapm_switch:
          case snd_soc_dapm_mixer:
          case snd_soc_dapm_mixer_named_ctl:
              dapm_new_mixer(w);
              break;
          case snd_soc_dapm_mux:
          case snd_soc_dapm_demux:
              dapm_new_mux(w);
              break;
          case snd_soc_dapm_pga:
          case snd_soc_dapm_out_drv:
              dapm_new_pga(w);
              break;
          case snd_soc_dapm_dai_link:
              dapm_new_dai_link(w);
              break;
          default:
              break;
          }
  
          /* Read the initial power state from the device */
          if (w->reg >= 0) {
              soc_dapm_read(w->dapm, w->reg, &val);
              val = val >> w->shift;
              val &= w->mask;
              if (val == w->on_val)
                  w->power = 1;
          }
  
          w->new = 1;
  
          dapm_mark_dirty(w, "new widget");
          dapm_debugfs_add_widget(w);
      }
  
      dapm_power_widgets(card, SND_SOC_DAPM_STREAM_NOP);    //通过dapm_power_widgets函数，统一处理所有位于dapm_dirty链表上的widget的状态改变：
      mutex_unlock(&card->dapm_mutex);
      return 0;
  }
  ```