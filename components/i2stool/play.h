#ifndef _PLAY_H_
#define _PLAY_H_

#include <stdint.h>
#include <stddef.h> 

//配置I2S驱动
#define I2S_NUM I2S_NUM_0
#define SAMPLE_RATE 44100 //音频采样率
#define I2S_BUFFER_SIZE 2048 //I2S缓冲区大小

void init_i2s_g();
void play_audio(int32_t* pcm_data, size_t data_size);

#endif