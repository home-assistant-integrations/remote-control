idf_component_register(SRCS "setup_mode.c" "setup_wifi.c" "setup_dns.c" "setup_http.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES config json esp_wifi esp_netif esp_http_server
                                     esp_hw_support bootloader_support lwip log
                       EMBED_TXTFILES "page.html")
