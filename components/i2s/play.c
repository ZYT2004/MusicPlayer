#include "play.h"
#include "i2s.h"
#include "esp_log.h"
#include <stdio.h>


void init_i2s_g(){
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX, //设为主模式和传输模式
        .sample_rate = SAMPLE_RATE,  //设置音频采样率
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, //每个采样16位
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // 双声道，左右声道
        .communication_format = I2S_COMM_FORMAT_I2S, //使用I2S标准格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, //中断标志设置
        .dma_buf_count = 8, //DMA缓冲区的数量
        .dma_buf_len = I2S_BUFFER_SIZE, //每个DMA缓冲区大小
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 2,//bck
        .ws_io_num = 10,//lck
        .data_out_num = 9 //din
    };

    //初始化2S
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0,&i2s_config,0,NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0,&pin_config));
    ESP_LOGI("I2S","I2S driver initialized");
}

//播放音频文件
void play_audio(const uint8_t* audio_data, size_t data_size) {
    if (!audio_data || data_size == 0) {
        ESP_LOGE("AUDIO", "Invalid audio data or size");
        return;
    }

    size_t bytes_written;
    size_t offset = 0;

    // 持续读取音频数据并将数据发送给I2S播放
    while (offset < data_size) {
        size_t bytes_to_write = (data_size - offset) > I2S_BUFFER_SIZE ? I2S_BUFFER_SIZE : (data_size - offset);
        i2s_write(I2S_NUM_0, audio_data + offset, bytes_to_write, &bytes_written, portMAX_DELAY);
        offset += bytes_written;
    }
}
