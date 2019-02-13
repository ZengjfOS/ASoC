# widget-control-route Add To Card

主要分析前面提到的widget/control/route是怎么添加到Card上的；

* component->driver->dapm_widgets添加：([dai widget类似](0007_component_probe.md#dai-widget)): [snd_soc_dapm_new_controls Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-dapm.c#L3592)
* component->driver->controls添加：[snd_soc_add_component_controls Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L2383)
* component->driver->dapm_routes添加：[snd_soc_dapm_add_routes Source Code](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-dapm.c#L2896)

## widget

* [Codec CS42888 widgets 示例](0002_ASoC_Codec_Class_Driver.md#dapm_widgets)
  ```C
  /**
   * #define SND_SOC_DAPM_DAC(wname, stname, wreg, wshift, winvert) \
   * {    .id = snd_soc_dapm_dac, .name = wname, .sname = stname, \
   *     SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert) }
   */
  SND_SOC_DAPM_DAC("DAC1", "Playback", CS42XX8_PWRCTL, 1, 1),
  ```
* [snd_soc_dapm_new_control_unlocked分析](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-dapm.c#L3421)
  ```C
  struct snd_soc_dapm_widget *
  snd_soc_dapm_new_control_unlocked(struct snd_soc_dapm_context *dapm,
               const struct snd_soc_dapm_widget *widget)
  {
      enum snd_soc_dapm_direction dir;
      struct snd_soc_dapm_widget *w;
      const char *prefix;
      int ret;
  
      if ((w = dapm_cnew_widget(widget)) == NULL)                         // 创建新的widget
          return ERR_PTR(-ENOMEM);
  
      switch (w->id) {                                                    // 第一次通过id来识别widget类型
      case snd_soc_dapm_regulator_supply:
          w->regulator = devm_regulator_get(dapm->dev, w->name);
          if (IS_ERR(w->regulator)) {
              ret = PTR_ERR(w->regulator);
              goto request_failed;
          }
  
          if (w->on_val & SND_SOC_DAPM_REGULATOR_BYPASS) {
              ret = regulator_allow_bypass(w->regulator, true);
              if (ret != 0)
                  dev_warn(dapm->dev,
                       "ASoC: Failed to bypass %s: %d\n",
                       w->name, ret);
          }
          break;
      case snd_soc_dapm_pinctrl:
          w->pinctrl = devm_pinctrl_get(dapm->dev);
          if (IS_ERR(w->pinctrl)) {
              ret = PTR_ERR(w->pinctrl);
              goto request_failed;
          }
          break;
      case snd_soc_dapm_clock_supply:
          w->clk = devm_clk_get(dapm->dev, w->name);
          if (IS_ERR(w->clk)) {
              ret = PTR_ERR(w->clk);
              goto request_failed;
          }
          break;
      default:
          break;
      }
  
      prefix = soc_dapm_prefix(dapm);                                         // 之前没有注意到这个prefix，暂时不管
      if (prefix)
          w->name = kasprintf(GFP_KERNEL, "%s %s", prefix, widget->name);
      else
          w->name = kstrdup_const(widget->name, GFP_KERNEL);
      if (w->name == NULL) {
          kfree(w);
          return ERR_PTR(-ENOMEM);
      }
  
      switch (w->id) {                                                        // 第二次通过id来识别不同的widget
      case snd_soc_dapm_mic:
          w->is_ep = SND_SOC_DAPM_EP_SOURCE;
          w->power_check = dapm_generic_check_power;
          break;
      case snd_soc_dapm_input:
          if (!dapm->card->fully_routed)
              w->is_ep = SND_SOC_DAPM_EP_SOURCE;
          w->power_check = dapm_generic_check_power;
          break;
      case snd_soc_dapm_spk:
      case snd_soc_dapm_hp:
          w->is_ep = SND_SOC_DAPM_EP_SINK;
          w->power_check = dapm_generic_check_power;
          break;
      case snd_soc_dapm_output:
          if (!dapm->card->fully_routed)
              w->is_ep = SND_SOC_DAPM_EP_SINK;
          w->power_check = dapm_generic_check_power;
          break;
      case snd_soc_dapm_vmid:
      case snd_soc_dapm_siggen:
          w->is_ep = SND_SOC_DAPM_EP_SOURCE;
          w->power_check = dapm_always_on_check_power;
          break;
      case snd_soc_dapm_sink:
          w->is_ep = SND_SOC_DAPM_EP_SINK;
          w->power_check = dapm_always_on_check_power;
          break;
  
      case snd_soc_dapm_mux:
      case snd_soc_dapm_demux:
      case snd_soc_dapm_switch:
      case snd_soc_dapm_mixer:
      case snd_soc_dapm_mixer_named_ctl:
      case snd_soc_dapm_adc:
      case snd_soc_dapm_aif_out:
      case snd_soc_dapm_dac:                                                  // 这个就是前面的示例目标了
      case snd_soc_dapm_aif_in:
      case snd_soc_dapm_pga:
      case snd_soc_dapm_out_drv:
      case snd_soc_dapm_micbias:
      case snd_soc_dapm_line:
      case snd_soc_dapm_dai_link:
      case snd_soc_dapm_dai_out:
      case snd_soc_dapm_dai_in:
          w->power_check = dapm_generic_check_power;
          break;
      case snd_soc_dapm_supply:
      case snd_soc_dapm_regulator_supply:
      case snd_soc_dapm_pinctrl:
      case snd_soc_dapm_clock_supply:
      case snd_soc_dapm_kcontrol:
          w->is_supply = 1;
          w->power_check = dapm_supply_check_power;
          break;
      default:
          w->power_check = dapm_always_on_check_power;
          break;
      }
  
      w->dapm = dapm;
      INIT_LIST_HEAD(&w->list);
      INIT_LIST_HEAD(&w->dirty);
      list_add_tail(&w->list, &dapm->card->widgets);                          // 添加到card->widgets中去
  
      snd_soc_dapm_for_each_direction(dir) {
          INIT_LIST_HEAD(&w->edges[dir]);
          w->endpoints[dir] = -1;
      }
  
      /* machine layer sets up unconnected pins and insertions */
      w->connected = 1;
      return w;
  
  request_failed:
      if (ret != -EPROBE_DEFER)
          dev_err(dapm->dev, "ASoC: Failed to request %s: %d\n",
              w->name, ret);
  
      return ERR_PTR(ret);
  }
  ```

## control

* [Codec CS42888 controls 示例](0002_ASoC_Codec_Class_Driver.md#controls)
  ```C
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
  ```
* [snd_soc_add_controls分析](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L2337)
  ```C
  static int snd_soc_add_controls(struct snd_card *card, struct device *dev,
      const struct snd_kcontrol_new *controls, int num_controls,
      const char *prefix, void *data)
  {
      int err, i;
  
      for (i = 0; i < num_controls; i++) {
          const struct snd_kcontrol_new *control = &controls[i];
  
          err = snd_ctl_add(card, snd_soc_cnew(control, data,               // <--------创建并添加
                               control->name, prefix));
          if (err < 0) {
              dev_err(dev, "ASoC: Failed to add %s: %d\n",
                  control->name, err);
              return err;
          }
      }
  
      return 0;
  }
  ```
* [snd_soc_cnew](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-core.c#L2305)：通过`kcontrol = snd_ctl_new1(&template, data);`创建一个`kcontrol`；
* [`snd_ctl_add`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/core/control.c#L441)调用[`snd_ctl_add_replace`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/core/control.c#L404)再调用[`__snd_ctl_add_replace`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/core/control.c#L356)进行[`CTL_ADD_EXCLUSIVE`](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/core/control.c#L352)添加替换，`kcontrol`保存在`card->controls`；
  ```C
  /* add/replace a new kcontrol object; call with card->controls_rwsem locked */
  static int __snd_ctl_add_replace(struct snd_card *card,
                   struct snd_kcontrol *kcontrol,
                   enum snd_ctl_add_mode mode)
  {
      struct snd_ctl_elem_id id;
      unsigned int idx;
      unsigned int count;
      struct snd_kcontrol *old;
      int err;
  
      id = kcontrol->id;
      if (id.index > UINT_MAX - kcontrol->count)
          return -EINVAL;
  
      old = snd_ctl_find_id(card, &id);                                 // kcontrol保存在card->controls，查找当前的id是否存在
      if (!old) {
          if (mode == CTL_REPLACE)
              return -EINVAL;
      } else {
          if (mode == CTL_ADD_EXCLUSIVE) {
              dev_err(card->dev,
                  "control %i:%i:%i:%s:%i is already present\n",
                  id.iface, id.device, id.subdevice, id.name,
                  id.index);
              return -EBUSY;
          }
  
          err = snd_ctl_remove(card, old);
          if (err < 0)
              return err;
      }
  
      if (snd_ctl_find_hole(card, kcontrol->count) < 0)
          return -ENOMEM;
  
      list_add_tail(&kcontrol->list, &card->controls);                  // 将当前的kcontrol添加到card->controls中去
      card->controls_count += kcontrol->count;
      kcontrol->id.numid = card->last_numid + 1;
      card->last_numid += kcontrol->count;
  
      id = kcontrol->id;
      count = kcontrol->count;
      for (idx = 0; idx < count; idx++, id.index++, id.numid++)
          snd_ctl_notify(card, SNDRV_CTL_EVENT_MASK_ADD, &id);
  
      return 0;
  }
  ```

## route

* [Codec CS42888 routes 示例](0002_ASoC_Codec_Class_Driver.md#dapm_routes)
  ```C
  /* Playback */
  { "AOUT1L", NULL, "DAC1" },
  { "AOUT1R", NULL, "DAC1" },
  { "DAC1", NULL, "PWR" },
  ```
* [snd_soc_dapm_add_route分析](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-dapm.c#L2734)
  ```C
  static int snd_soc_dapm_add_route(struct snd_soc_dapm_context *dapm,
                    const struct snd_soc_dapm_route *route)
  {
      struct snd_soc_dapm_widget *wsource = NULL, *wsink = NULL, *w;
      struct snd_soc_dapm_widget *wtsource = NULL, *wtsink = NULL;
      const char *sink;
      const char *source;
      char prefixed_sink[80];
      char prefixed_source[80];
      const char *prefix;
      int ret;
  
      prefix = soc_dapm_prefix(dapm);                                       // 没关注这部分，暂时不管
      if (prefix) {
          snprintf(prefixed_sink, sizeof(prefixed_sink), "%s %s",
               prefix, route->sink);
          sink = prefixed_sink;
          snprintf(prefixed_source, sizeof(prefixed_source), "%s %s",
               prefix, route->source);
          source = prefixed_source;
      } else {                                                              // 假装走的是这条线路
          sink = route->sink;
          source = route->source;
      }
  
      wsource = dapm_wcache_lookup(&dapm->path_source_cache, source);       // 查找card->widgets，比较w->name和source是否一致
      wsink = dapm_wcache_lookup(&dapm->path_sink_cache, sink);             // 查找card->widgets，比较w->name和sink是否一致
  
      if (wsink && wsource)                                                 // 两者如果都存在，那就是找到了
          goto skip_search;
  
      /*
       * find src and dest widgets over all widgets but favor a widget from
       * current DAPM context
       */
      list_for_each_entry(w, &dapm->card->widgets, list) {
          if (!wsink && !(strcmp(w->name, sink))) {
              wtsink = w;
              if (w->dapm == dapm) {
                  wsink = w;
                  if (wsource)
                      break;
              }
              continue;
          }
          if (!wsource && !(strcmp(w->name, source))) {
              wtsource = w;
              if (w->dapm == dapm) {
                  wsource = w;
                  if (wsink)
                      break;
              }
          }
      }
      /* use widget from another DAPM context if not found from this */
      if (!wsink)
          wsink = wtsink;
      if (!wsource)
          wsource = wtsource;
  
      if (wsource == NULL) {
          dev_err(dapm->dev, "ASoC: no source widget found for %s\n",
              route->source);
          return -ENODEV;
      }
      if (wsink == NULL) {
          dev_err(dapm->dev, "ASoC: no sink widget found for %s\n",
              route->sink);
          return -ENODEV;
      }
  
  skip_search:
      dapm_wcache_update(&dapm->path_sink_cache, wsink);                            // wcache->widget = w; 设定sink widget
      dapm_wcache_update(&dapm->path_source_cache, wsource);                        // wcache->widget = w; 设定source widget
  
      ret = snd_soc_dapm_add_path(dapm, wsource, wsink, route->control,             // 添加route，下面分析这个函数
          route->connected);
      if (ret)
          goto err;
  
      return 0;
  err:
      dev_warn(dapm->dev, "ASoC: no dapm match for %s --> %s --> %s\n",
           source, route->control, sink);
      return ret;
  }
  ```
* [snd_soc_dapm_add_path分析](https://elixir.bootlin.com/linux/v5.0-rc6/source/sound/soc/soc-dapm.c#L2632)
  ```C
  static int snd_soc_dapm_add_path(struct snd_soc_dapm_context *dapm,
      struct snd_soc_dapm_widget *wsource, struct snd_soc_dapm_widget *wsink,
      const char *control,
      int (*connected)(struct snd_soc_dapm_widget *source,
               struct snd_soc_dapm_widget *sink))
  {
      struct snd_soc_dapm_widget *widgets[2];
      enum snd_soc_dapm_direction dir;
      struct snd_soc_dapm_path *path;
      int ret;
  
      if (wsink->is_supply && !wsource->is_supply) {
          dev_err(dapm->dev,
              "Connecting non-supply widget to supply widget is not supported (%s -> %s)\n",
              wsource->name, wsink->name);
          return -EINVAL;
      }
  
      if (connected && !wsource->is_supply) {                                               // route不一定要由connected
          dev_err(dapm->dev,
              "connected() callback only supported for supply widgets (%s -> %s)\n",
              wsource->name, wsink->name);
          return -EINVAL;
      }
  
      if (wsource->is_supply && control) {
          dev_err(dapm->dev,
              "Conditional paths are not supported for supply widgets (%s -> [%s] -> %s)\n",
              wsource->name, control, wsink->name);
          return -EINVAL;
      }
  
      ret = snd_soc_dapm_check_dynamic_path(dapm, wsource, wsink, control);         // 检查path的dynamic
      if (ret)
          return ret;
  
      path = kzalloc(sizeof(struct snd_soc_dapm_path), GFP_KERNEL);                 // 申请path结构体
      if (!path)
          return -ENOMEM;

      /**
       * enum snd_soc_dapm_direction {
       *     SND_SOC_DAPM_DIR_IN,
       *     SND_SOC_DAPM_DIR_OUT
       * };
       */  
      path->node[SND_SOC_DAPM_DIR_IN] = wsource;                                    // 设置输入源
      path->node[SND_SOC_DAPM_DIR_OUT] = wsink;                                     // 设置输出目标
      widgets[SND_SOC_DAPM_DIR_IN] = wsource;
      widgets[SND_SOC_DAPM_DIR_OUT] = wsink;
  
      path->connected = connected;                                                  // 设置path的connected
      INIT_LIST_HEAD(&path->list);                                                  // 初始化path的list
      INIT_LIST_HEAD(&path->list_kcontrol);                                         // 初始化path的list_kcontrol
  
      if (wsource->is_supply || wsink->is_supply)
          path->is_supply = 1;
  
      /* connect static paths */
      if (control == NULL) {                                            // 如果control不存在，那么仅仅是设定path的connect = 1 
          path->connect = 1;
      } else {                                                          // control存在，那么就需要将source/sink添加到path中去
          switch (wsource->id) {
          case snd_soc_dapm_demux:
              ret = dapm_connect_mux(dapm, path, control, wsource);
              if (ret)
                  goto err;
              break;
          default:
              break;
          }
  
          switch (wsink->id) {
          case snd_soc_dapm_mux:
              ret = dapm_connect_mux(dapm, path, control, wsink);
              if (ret != 0)
                  goto err;
              break;
          case snd_soc_dapm_switch:
          case snd_soc_dapm_mixer:
          case snd_soc_dapm_mixer_named_ctl:
              ret = dapm_connect_mixer(dapm, path, control);
              if (ret != 0)
                  goto err;
              break;
          default:
              break;
          }
      }
  
      /**
       * #define snd_soc_dapm_for_each_direction(dir) \
       *     for ((dir) = SND_SOC_DAPM_DIR_IN; (dir) <= SND_SOC_DAPM_DIR_OUT; \
       *         (dir)++)
       * 
       */
      list_add(&path->list, &dapm->card->paths);                        // 将path添加到card的paths中去，主要是分析到这里
      snd_soc_dapm_for_each_direction(dir)
          list_add(&path->list_node[dir], &widgets[dir]->edges[dir]);   // 将path和wideget链接起来，目前不是很清楚这里用来做什么
  
      snd_soc_dapm_for_each_direction(dir) {
          dapm_update_widget_flags(widgets[dir]);
          dapm_mark_dirty(widgets[dir], "Route added");
      }
  
      if (dapm->card->instantiated && path->connect)
          dapm_path_invalidate(path);
  
      return 0;
  err:
      kfree(path);
      return ret;
  }
  ```