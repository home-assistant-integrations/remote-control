/*
 * Captive-portal DNS server: answers every A query with 192.168.4.1, so that
 * phones open the upload page by themselves. Other query types get an empty
 * answer.
 */

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "setup_internal.h"

static const char *TAG = "setup_dns";

#define DNS_PORT     53
#define DNS_MAX_LEN  512
#define DNS_HDR_LEN  12
#define TYPE_A       1
#define CLASS_IN     1

static const uint8_t AP_IP[4] = {192, 168, 4, 1};

/* Builds the reply in place. Returns its length, or 0 to drop the packet. */
static int build_reply(uint8_t *buf, int len)
{
    if (len < DNS_HDR_LEN) {
        return 0;
    }
    uint16_t flags = (buf[2] << 8) | buf[3];
    uint16_t qdcount = (buf[4] << 8) | buf[5];
    if ((flags & 0x8000) != 0 || qdcount != 1) {
        return 0; /* a response, or not exactly one question */
    }

    /* Skip the question name. */
    int p = DNS_HDR_LEN;
    while (p < len && buf[p] != 0) {
        if ((buf[p] & 0xC0) != 0 || p + 1 + buf[p] >= len) {
            return 0; /* compression in a query, or truncated */
        }
        p += 1 + buf[p];
    }
    p++; /* the terminating zero */
    if (p + 4 > len) {
        return 0;
    }
    uint16_t qtype = (buf[p] << 8) | buf[p + 1];
    uint16_t qclass = (buf[p + 2] << 8) | buf[p + 3];
    p += 4; /* end of the question; anything after it is dropped */

    /* Header: response, recursion available, keep the opcode and RD bit. */
    buf[2] = 0x80 | (buf[2] & 0x79);
    buf[3] = 0x80;
    buf[6] = 0;
    buf[7] = 0; /* ancount */
    buf[8] = buf[9] = buf[10] = buf[11] = 0;

    if (qtype != TYPE_A || qclass != CLASS_IN) {
        return p; /* no answer for AAAA and others */
    }
    if (p + 16 > DNS_MAX_LEN) {
        return 0;
    }
    buf[7] = 1;
    static const uint8_t answer[] = {
        0xC0, 0x0C,             /* name: pointer to the question */
        0x00, TYPE_A,           /* type */
        0x00, CLASS_IN,         /* class */
        0x00, 0x00, 0x00, 0x3C, /* TTL 60 s */
        0x00, 0x04,             /* data length */
    };
    memcpy(buf + p, answer, sizeof answer);
    memcpy(buf + p + sizeof answer, AP_IP, sizeof AP_IP);
    return p + sizeof answer + sizeof AP_IP;
}

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
        ESP_LOGE(TAG, "bind to port 53 failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "captive DNS on port 53");

    uint8_t buf[DNS_MAX_LEN];
    for (;;) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof from;
        int len = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &from_len);
        if (len <= 0) {
            continue;
        }
        int reply = build_reply(buf, len);
        if (reply > 0) {
            sendto(sock, buf, reply, 0, (struct sockaddr *)&from, from_len);
        }
    }
}

esp_err_t setup_dns_start(void)
{
    BaseType_t ok = xTaskCreate(dns_task, "setup_dns", 3072, NULL, 5, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
