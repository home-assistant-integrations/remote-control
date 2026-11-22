idf_component_register(SRCS "config_validate.c" "config_storage.c"
                       INCLUDE_DIRS "include"
                       REQUIRES json esp_common
                       PRIV_REQUIRES joltwallet__littlefs log vfs)
