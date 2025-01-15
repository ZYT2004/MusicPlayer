#include "play.h"
#include "i2s.h"
#include "esp_log.h"
#include <stdio.h>

void init_i2s_g()
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100, // 确保与 WAV 文件采样率匹配
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // 根据实际声道数选择
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_len = 1024,
        .dma_buf_count = 4,
        .use_apll = false};

    i2s_pin_config_t pin_config = {
        .bck_io_num = 2,  // bck
        .ws_io_num = 10,  // lck
        .data_out_num = 9 // din
    };

    // 初始化2S
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pin_config));
    ESP_LOGI("I2S", "I2S driver initialized");
}
// 简单的去噪处理函数
void denoise_pcm_data(int32_t *pcm_data, size_t data_size)
{
    // 噪声阈值
    const int32_t noise_threshold = 100; // 根据需要调整阈值

    // 计算样本数
    size_t num_samples = data_size / sizeof(int32_t);

    // 遍历所有样本并进行去噪处理
    for (size_t i = 0; i < num_samples; i++)
    {
        if (pcm_data[i] < noise_threshold && pcm_data[i] > -noise_threshold)
        {
            pcm_data[i] = 0;
        }
    }
}
void play_audio(int32_t *pcm_data, size_t data_size)
{
    if (!pcm_data || data_size == 0)
    {
        ESP_LOGE("I2S", "Invalid PCM data or size");
        return;
    }

    // 计算字节数和样本数
    size_t bytes_written = 0; // 声明并初始化 bytes_written

    if ((data_size % 4) != 0)
    {
        ESP_LOGE("I2S", "Data size is not a multiple of 4, invalid 32-bit PCM data");
        return;
    }
    // 对 PCM 数据进行去噪处理
    denoise_pcm_data(pcm_data, data_size);

    // 将 PCM 数据写入 I2S
    esp_err_t res = i2s_write(I2S_NUM, pcm_data, data_size, &bytes_written, portMAX_DELAY);
    if (res != ESP_OK)
    {
        ESP_LOGE("I2S", "Error writing data to I2S: %s", esp_err_to_name(res));
    }
}