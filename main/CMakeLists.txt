set(srcs "main.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                                     "../components/sd_card"
                                     "../components/i2stool"
                       REQUIRES sd_card i2stool
                       WHOLE_ARCHIVE)