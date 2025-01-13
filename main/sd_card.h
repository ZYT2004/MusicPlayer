#ifndef SD_CARD_H
#define SD_CARD_H

// Add your includes here
#include <stdio.h>
#include <time.h>

#define MAX_FILES 20
#define MAX_FILENAME_LENGTH 512
#define MAX_CONTENT_SIZE 4048

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    time_t time;
    char content[MAX_CONTENT_SIZE];
} file_info_t;

void getSongByIndex(int index,file_info_t* prevSong, file_info_t* curSong, file_info_t* nextSong);
// Add your function declarations and definitions here

#endif 