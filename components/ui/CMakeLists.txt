idf_component_register(SRCS "ui_logic.c" "ui_task.cpp" "ui_render.cpp" "ui_status.cpp" "ui_cover.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES config layout
                       PRIV_REQUIRES ha assets m5stack__m5unified nvs_flash esp_timer esp_netif
                                     esp_http_client log)
