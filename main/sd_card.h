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

void getSongByIndex(int index,char* prevName, file_info_t* curSong, char* nextName);
void list_files(const char *path, file_info_t files[MAX_FILES], int *file_count);
// Add your function declarations and definitions here

#endif 