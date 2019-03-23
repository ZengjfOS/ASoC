# Android Bluetooth HFP

## 参考文档

* [audioflinger学习笔记](https://www.xuebuyuan.com/684621.html)
* [Bluetooth sco audio是什么？](https://blog.csdn.net/lizzywu/article/details/8107502)

蓝牙基带技术支持两种连接方式：  
* 面向连接（SCO）方式：主要用于话音传输；
* 无连接(ACL)方式：主要用于分组数据传输。

## Android Audio HAL

### 获取HAL Logcat信息

`logcat -s audio_hw_primary`

### Source Code

* `vendor/nxp-opensource/imx/alsa/tinyalsa_hal.c`
  ```C
  static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
  {
      struct imx_audio_device *adev = (struct imx_audio_device *)dev;
      struct str_parms *parms;
      char *str;
      char value[32];
      int ret;
      int status = 0;
  
      ALOGD("%s: enter: %s", __func__, kvpairs);
  
      parms = str_parms_create_str(kvpairs);
      ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_TTY_MODE, value, sizeof(value));
      if (ret >= 0) {
          int tty_mode;
  
          if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
              tty_mode = TTY_MODE_OFF;
          else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
              tty_mode = TTY_MODE_VCO;
          else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
              tty_mode = TTY_MODE_HCO;
          else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
              tty_mode = TTY_MODE_FULL;
          else {
              status = -EINVAL;
              goto done;
          }
  
          pthread_mutex_lock(&adev->lock);
          if (tty_mode != adev->tty_mode) {
              adev->tty_mode = tty_mode;
              if (adev->mode == AUDIO_MODE_IN_CALL)
                  select_output_device(adev);
          }
          pthread_mutex_unlock(&adev->lock);
      }
  
      ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_NREC, value, sizeof(value));
      if (ret >= 0) {
          if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
              adev->bluetooth_nrec = true;
          else
              adev->bluetooth_nrec = false;
      }
  
      ret = str_parms_get_str(parms, "screen_state", value, sizeof(value));
      if (ret >= 0) {
          if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
              adev->low_power = false;
          else
              adev->low_power = true;
      }
  
      ret = str_parms_get_str(parms, "hfp_set_sampling_rate", value, sizeof(value));
      if (ret >= 0) {
          int rate = atoi(value);
          ALOGI("hfp_set_sampling_rate, %d", rate);
          pcm_config_sco_in.rate = rate;                                                    // 蓝牙语音声卡采样率
          pcm_config_sco_out.rate = rate;                                                   // 蓝牙语音声卡采样率
      }
  
      ret = str_parms_get_str(parms, "hfp_enable", value, sizeof(value));
      if (ret >= 0) {
          if(0 == strcmp(value, "true")) {
              ret = sco_task_create(adev);                                                  // 处理任务
              ALOGI("sco_task_create, ret %d", ret);
          } else {
              ret = sco_task_destroy(adev);
              ALOGI("sco_task_destroy, ret %d", ret);
          }
      }
  
  done:
      str_parms_destroy(parms);
      ALOGD("%s: exit with code(%d)", __func__, status);
      return status;
  }
  ```

## hfpclient

* 源代码：`packages/apps/Bluetooth/src/com/android/bluetooth/hfpclient`
* 状态监听事件处理源代码：`packages/apps/Bluetooth/src/com/android/bluetooth/hfpclient/HeadsetClientStateMachine.java`
  ```Java
  // in Connected state
  private void processAudioEvent(int state, BluetoothDevice device) {
      // message from old device
      if (!mCurrentDevice.equals(device)) {
          Log.e(TAG, "Audio changed on disconnected device: " + device);
          return;
      }
  
      switch (state) {
          case HeadsetClientHalConstants.AUDIO_STATE_CONNECTED_MSBC:
              mAudioWbs = true;
              // fall through
          case HeadsetClientHalConstants.AUDIO_STATE_CONNECTED:
              // Audio state is split in two parts, the audio focus is maintained by the
              // entity exercising this service (typically the Telecom stack) and audio
              // routing is handled by the bluetooth stack itself. The only reason to do so is
              // because Bluetooth SCO connection from the HF role is not entirely supported
              // for routing and volume purposes.
              // NOTE: All calls here are routed via the setParameters which changes the
              // routing at the Audio HAL level.
  
              if (mService.isScoRouted()) {
                  StackEvent event =
                          new StackEvent(StackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED);
                  event.valueInt = state;
                  event.device = device;
                  sendMessageDelayed(StackEvent.STACK_EVENT, event, ROUTING_DELAY_MS);
                  break;
              }
  
              mAudioState = BluetoothHeadsetClient.STATE_AUDIO_CONNECTED;
  
              // We need to set the volume after switching into HFP mode as some Audio HALs
              // reset the volume to a known-default on mode switch.
              final int amVol = mAudioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
              final int hfVol = amToHfVol(amVol);
  
              if (DBG) {
                  Log.d(TAG, "hfp_enable=true mAudioWbs is " + mAudioWbs);
              }
              if (mAudioWbs) {
                  if (DBG) {
                      Log.d(TAG, "Setting sampling rate as 16000");
                  }
                  mAudioManager.setParameters("hfp_set_sampling_rate=16000");
              } else {
                  if (DBG) {
                      Log.d(TAG, "Setting sampling rate as 8000");
                  }
                  mAudioManager.setParameters("hfp_set_sampling_rate=8000");
              }
              if (DBG) {
                  Log.d(TAG, "hf_volume " + hfVol);
              }
              routeHfpAudio(true);
              mAudioFocusRequest = requestAudioFocus();
              mAudioManager.setParameters("hfp_volume=" + hfVol);
              transitionTo(mAudioOn);
              break;
  
          case HeadsetClientHalConstants.AUDIO_STATE_CONNECTING:
              // No state transition is involved, fire broadcast immediately
              broadcastAudioState(device, BluetoothHeadsetClient.STATE_AUDIO_CONNECTING,
                      mAudioState);
              mAudioState = BluetoothHeadsetClient.STATE_AUDIO_CONNECTING;
              break;
  
          case HeadsetClientHalConstants.AUDIO_STATE_DISCONNECTED:
              // No state transition is involved, fire broadcast immediately
              broadcastAudioState(device, BluetoothHeadsetClient.STATE_AUDIO_DISCONNECTED,
                      mAudioState);
              mAudioState = BluetoothHeadsetClient.STATE_AUDIO_DISCONNECTED;
              break;
  
          default:
              Log.e(TAG, "Audio State Device: " + device + " bad state: " + state);
              break;
      }
  }
  ```
