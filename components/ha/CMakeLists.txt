idf_component_register(SRCS "wifi.c" "ha_ws.c" "ha_client.c" "media_state.c"
                       INCLUDE_DIRS "include"
                       REQUIRES config layout
                       PRIV_REQUIRES json esp_wifi esp_netif esp_event esp_timer log
                                     espressif__esp_websocket_client)
