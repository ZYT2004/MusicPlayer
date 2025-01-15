set(srcs "sd_card.c")
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/storage/sd_card/sdmmc/components/sd_card")
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sdmmc log
                       WHOLE_ARCHIVE)
