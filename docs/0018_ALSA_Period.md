# ALSA Peroid

## 参考文档

* [alsa frame period period_size buffer_size 等解释](https://blog.csdn.net/spark550/article/details/80441499)
* [FramesPeriods](https://www.alsa-project.org/main/index.php/FramesPeriods)
* [android Audio 详解（ 二 ）](https://blog.csdn.net/wince_lover/article/details/50443446)
* [PCM data flow - 7 - Frame & Period](https://blog.csdn.net/azloong/article/details/51044893)

## 简要术语

* Sample：样本长度，音频数据最基本的单位，常见的有 8 位和 16 位；
* Channel：声道数，分为单声道 mono 和立体声 stereo；
* Frame：帧，构成一个完整的声音单元，Frame = Sample * channel；
* Rate：又称 sample rate，采样率，即每秒的采样次数，针对帧而言；
* Period size：周期，每次硬件中断处理音频数据的帧数，对于音频设备的数据读写，以此为单位；
* Buffer size：数据缓冲区大小，这里指 runtime 的 buffer size，而不是结构图 snd_pcm_hardware 中定义的 buffer_bytes_max；一般来说 buffer_size = period_size * period_count， period_count 相当于处理完一个 buffer 数据所需的硬件中断次数。