# ASoC

分析i.MX6 CS42888 ASoC驱动工作原理；

## 参考文档

* [Linux Source Code Online](https://elixir.bootlin.com/linux/latest/source)
* [ALSA SoC Layer](https://www.kernel.org/doc/html/v4.11/sound/soc/index.html)
* [sound-cs42888 dts](https://github.com/torvalds/linux/blob/master/arch/arm/boot/dts/imx6qdl-sabreauto.dtsi#L124)

## 笔记文档

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