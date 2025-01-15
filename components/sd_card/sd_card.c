#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_test_io.h"
#include "sd_card.h"
#include "esp_mac.h"
#include "play.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

static const char *TAG = "example";

#define MOUNT_POINT "/project"

#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
const char *names[] = {"CLK ", "MOSI", "MISO", "CS  "};
const int pins[] = {CONFIG_EXAMPLE_PIN_CLK,
                    CONFIG_EXAMPLE_PIN_MOSI,
                    CONFIG_EXAMPLE_PIN_MISO,
                    CONFIG_EXAMPLE_PIN_CS};

const int pin_count = sizeof(pins) / sizeof(pins[0]);
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
const int adc_channels[] = {CONFIG_EXAMPLE_ADC_PIN_CLK,
                            CONFIG_EXAMPLE_ADC_PIN_MOSI,
                            CONFIG_EXAMPLE_ADC_PIN_MISO,
                            CONFIG_EXAMPLE_ADC_PIN_CS};
#endif // CONFIG_EXAMPLE_ENABLE_ADC_FEATURE

pin_configuration_t config = {
    .names = names,
    .pins = pins,
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
    .adc_channels = adc_channels,
#endif
};
#endif // CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO GPIO_NUM_6
#define PIN_NUM_MOSI GPIO_NUM_4
#define PIN_NUM_CLK GPIO_NUM_5
#define PIN_NUM_CS GPIO_NUM_7

int file_count = 0;
void getSongByIndex(int index, char *prevName, file_info_t *curSong, char *nextName)
{
    if (index == 0)
    {
        strcpy(prevName, files[file_count - 1].name);
        *curSong = files[0];
        strcpy(nextName, files[1].name);
    }
    else if (index == file_count - 1)
    {
        strcpy(prevName, files[file_count - 2].name);
        *curSong = files[file_count - 1];
        strcpy(nextName, files[0].name);
    }
    else
    {
        strcpy(prevName, files[index - 1].name);
        *curSong = files[index];
        strcpy(nextName, files[index + 1].name);
    }
}
size_t parse_wav_header(FILE *f)
{
    uint8_t header[44]; // 假设 WAV 文件头为44字节
    if (fread(header, 1, 44, f) != 44)
    {
        ESP_LOGE(TAG, "Failed to read WAV header");
        return 0;
    }

    // 检查文件标识 "RIFF" 和 "WAVE"
    if (memcmp(header, "RIFF", 4) != 0 || memcmp(&header[8], "WAVE", 4) != 0)
    {
        ESP_LOGE(TAG, "Invalid WAV file");
        return 0;
    }

    // 提取采样率和通道数（确保与 I2S 配置匹配）
    uint32_t sample_rate = *(uint32_t *)&header[24];
    uint16_t channels = *(uint16_t *)&header[22];

    ESP_LOGI(TAG, "Sample rate: %lu, Channels: %u", sample_rate, channels);

    // 仅支持 44100 Hz, 2 通道的 WAV 文件
    if (sample_rate != 44100 || channels != 2)
    {
        ESP_LOGE(TAG, "Unsupported WAV format");
        return 0;
    }

    return 44; // 返回文件数据起始位置
}
size_t convert_wav_to_pcm(uint8_t *buffer, size_t bytes_read, int32_t *pcm_data)
{
    size_t pcm_samples = bytes_read / (32 / 8);

    // 转换 PCM 数据
    for (size_t i = 0; i < pcm_samples; i++)
    {
        pcm_data[i] = *((int32_t *)(buffer + (i * 4)));
    }

    return bytes_read;
}
void openFile(int index)
{
    ESP_LOGI("APP", "Starting I2S example");
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
    // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply first.
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
            check_sd_card_pins(&config, pin_count);
#endif
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    // sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.

    FILE *f = fopen(files[index].name, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", files[index].name);
        return;
    }

    // 读取 WAV 文件头并解析
    size_t header_size = parse_wav_header(f);
    if (header_size == 0)
    {
        ESP_LOGE(TAG, "Invalid WAV file format");
        fclose(f);
        return;
    }

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, header_size, SEEK_SET); // 跳过文件头

    ESP_LOGI(TAG, "File size: %zu, Data starts at: %zu", file_size, header_size);

    // 分配缓冲区
    uint8_t *buffer = (uint8_t *)malloc(I2S_BUFFER_SIZE);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer memory");
        fclose(f);
        return;
    }

    int32_t *pcm_data = (int32_t *)malloc(I2S_BUFFER_SIZE);
    if (pcm_data == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate PCM memory");
        free(buffer);
        fclose(f);
        return;
    }

    size_t total_bytes_read = 0;
    size_t bytes_read;

    // 循环读取文件数据并播放
    while (total_bytes_read < (file_size - header_size))
    {
        // 确保读取的数据对齐到样本边界
        size_t bytes_to_read = ((file_size - header_size - total_bytes_read) > I2S_BUFFER_SIZE) ? I2S_BUFFER_SIZE : (file_size - header_size - total_bytes_read);
        bytes_to_read -= (bytes_to_read % sizeof(int32_t)); // 对齐到32位样本边界

        bytes_read = fread(buffer, 1, bytes_to_read, f);
        if (bytes_read == 0)
        {
            if (feof(f))
            {
                ESP_LOGI(TAG, "End of file reached");
            }
            else
            {
                ESP_LOGE(TAG, "Error reading file");
            }
            break;
        }

        size_t pcm_data_size = convert_wav_to_pcm(buffer, bytes_read, pcm_data);
        if (pcm_data_size > 0)
        {
            play_audio(pcm_data, pcm_data_size);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to convert WAV to PCM");
        }

        total_bytes_read += bytes_read;
    }

    // 释放资源
    free(buffer);
    free(pcm_data);
    fclose(f);

    ESP_LOGI(TAG, "File playback complete");

    // Unmount SD card and clean up
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    // Deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);

// Deinitialize the power control driver if it was used
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete the on-chip LDO power control driver");
        return;
    }
#endif
}
void list_files(const char *path, file_info_t files[MAX_FILES], int *file_count)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return;
    }

    struct dirent *entry;
    *file_count = 0;
    int flag = 0;
    while ((entry = readdir(dir)) != NULL && *file_count < MAX_FILES)
    {
        if (flag == 0)
        {
            flag = 1;
            continue;
        }
        snprintf(files[*file_count].name, sizeof(files[*file_count].name), "%s/%s", path, entry->d_name);
        FILE *f = fopen(files[*file_count].name, "r");
        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        files[*file_count].content_size = file_size;
        fclose(f);
        ESP_LOGI(TAG, "Full path: %s", files[*file_count].name); // 打印完整路径
        (*file_count)++;
    }
    closedir(dir);
}
void init_sd(void)
{
    ESP_LOGI("APP", "Starting I2S example");
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
    // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply first.
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
            check_sd_card_pins(&config, pin_count);
#endif
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    // sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.

    list_files(MOUNT_POINT, files, &file_count);

    // Unmount SD card and clean up
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    // Deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);

    // Deinitialize the power control driver if it was used
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete the on-chip LDO power control driver");
        return;
    }
#endif
}