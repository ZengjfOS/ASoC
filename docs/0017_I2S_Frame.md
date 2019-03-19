# I2S Frame

WAV声音文件里面，一开始有个头部，头部里面就含有采样频率还有位宽 还有左右声道，声音数据；


## 参考文档

* [音频相关参数的记录（MCLK、BCLK、256fs等等）](https://blog.csdn.net/lugandong/article/details/72468831)

## I2S Frame

* 采样的位深是32 bit
* 左右声道各占了32 BCLK
* 那一个完整的LRCLK一共`32 * 2 = 64 BCLK = 64 fs`。 
* 到目前为止听说的声音的采样深度都是8的倍数，所以在存储的时候正好是一个一个字节的整数倍；

## 示波器看I2S信号

* 由于手边的示波器只有2路信号，测量信号的时候只能多拍几张图来对比；
* LRCLK中包含32个CLK时钟，正好对应32bit的控制器移位寄存器，对应I2S的左对齐，右对齐等对齐方式；  
  ![images/I2S_LRCLK_BCLK.jpg](images/I2S_LRCLK_BCLK.jpg)
* 主要是查看LRCLK、BCLK、DATA信号的关系，LRCLK翻转的第一个Clock下降沿之后才开始传输数据；  
  ![images/I2S_LRCLK_BCLK_2Times.jpg](images/I2S_LRCLK_BCLK_2Times.jpg)  
  ![images/I2S_LRCLK_DATA_2Times.jpg](images/I2S_LRCLK_DATA_2Times.jpg)  
* 从上图可知，BCLK跟LRCLK有关，和采样深度其实是关系不大的，控制器位宽是32bit；