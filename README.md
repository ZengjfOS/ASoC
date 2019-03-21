# ASoC

分析i.MX6 CS42888 ASoC驱动工作原理；

## 参考文档

* [Linux Source Code Online](https://elixir.bootlin.com/linux/latest/source)
* [ALSA SoC Layer](https://www.kernel.org/doc/html/v4.11/sound/soc/index.html)
* [sound-cs42888 dts](https://github.com/torvalds/linux/blob/master/arch/arm/boot/dts/imx6qdl-sabreauto.dtsi#L124)
* [Tiny library to interface with ALSA in the Linux kernel](https://github.com/tinyalsa/tinyalsa)

## 笔记文档

* [0018_ALSA_Period.md](docs/0018_ALSA_Period.md)：理解ALSA中Period的概念，理解它才能理解应用层软件如何操作声卡；
* [0017_I2S_Frame.md](docs/0017_I2S_Frame.md)：I2S进行声音数据传输的时候，如何表示一帧声音数据，在Linux应用层软件进行PCM数据存储的时候，也是一帧一帧的对数据进行储存；
* [0016_tinycap.md](docs/0016_tinycap.md)：看一下录音在用户层阶段是如何进行录制wav格式声音的；
* [0015_dapm-event.md](docs/0015_dapm-event.md)：dapm并没有外部设备节点控制，如何进行操作的呢？
* [0014_widget_dapm-control.md](docs/0014_widget_dapm-cmntrol.md)：widget和widget连接的route中的control在哪里进行操作？
* [0013_tinymix.md](docs/0013_tinymix.md)：tinymix的调用control流程；
* [0012_Control_Device.md](docs/0012_Control_Device.md)：声卡的control设备节点是怎么生成的，回调函数在哪里？
* [0011_tinyplay.md](docs/0011_tinyplay.md)：aplay代码量有点大，分析一下tinyplay，看看wav文件数据是如何写道PCM设备节点中去的；
* [0010_snd_pcm_new.md](docs/0010_snd_pcm_new.md)：看一下pcm的创建过程；
* [0009_RPI_ALSA_Tinyalsa.md](docs/0009_RPI_ALSA_Tinyalsa.md)：看一下树莓派的声卡情况；
* [0008_widget-control-route_Add_To_Card.md](docs/0008_widget-control-route_Add_To_Card.md)：component中widget/contorl/route添加到Card中的流程；
* [0007_component_probe.md](docs/0007_component_probe.md)：了解一下component中的那些widgets/controls/routes/dai/probe等如何被调用起作用的；
* [0006_bind_dai_link.md](docs/0006_bind_dai_link.md)：pcm runtime(rtd)在这里绑定需要dai_link涉及到的CPU/Platform/Codec component；
* [0005_dai_link_component.md](docs/0005_dai_link_component.md)：dai_link的真实面目；
* [0004_ASoC_Machine_Driver.md](docs/0004_ASoC_Machine_Driver.md)：主板声卡功能注册；
* [0003_ASoC_Platform_Driver.md](docs/0003_ASoC_Platform_Driver.md)：芯片平台声卡接口设备注册；
* [0002_ASoC_Codec_Class_Driver.md](docs/0002_ASoC_Codec_Class_Driver.md)：Codec compnent信息注册；
* [0001_DTS.md](docs/0001_DTS.md)：CS42888声卡设备树示例；