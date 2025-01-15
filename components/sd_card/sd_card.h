#ifndef SD_CARD_H
#define SD_CARD_H

// Add your includes here
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#define MAX_FILES 20
#define MAX_FILENAME_LENGTH 512

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    time_t time;
    uint8_t* content;
    size_t content_size;
} file_info_t;

extern file_info_t files[MAX_FILES];

void getSongByIndex(int index,char* prevName, file_info_t* curSong, char* nextName);
void list_files(const char *path, file_info_t files[MAX_FILES], int *file_count);
void init_sd(void);
// Add your function declarations and definitions here

#endif 