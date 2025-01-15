set(srcs "sd_card.c")
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sdmmc log i2stool 
                       WHOLE_ARCHIVE)
