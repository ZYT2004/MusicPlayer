#include "sd_card.h"
#include "play.h"
file_info_t files[MAX_FILES]; 
void app_main(){
    init_i2s_g();
    init_sd();
    openFile(0);
    openFile(1);
}