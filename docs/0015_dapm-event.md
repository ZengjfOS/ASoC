# dapm-event

## 参考文档

* [ALSA声卡驱动中的DAPM详解之七：dapm事件机制（dapm event）](https://blog.csdn.net/DroidPhone/article/details/14548631)

## 简要说明

把dai widget和stream widget连接在一起，就是为了能把ASoc中的pcm处理部分和dapm进行关联，pcm的处理过程中，会通过发出stream event来通知dapm系统，重新扫描并调整音频路径上各个widget的电源状态，