#include "sd_card.h"
#include "play.h"
file_info_t files[MAX_FILES]; 
void app_main(){
    init_sd();
    init_i2s_g();
    play_audio(files[0].content, files[0].content_size);
}