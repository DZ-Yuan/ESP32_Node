#include "pch.h"
#include "_global_var.h"
#include "_common.h"

#define CLOUD_MODE 0

static const char *TAG = "Node";

#define DEBUG_MSG 0

// CMD
#if DEBUG_MSG
#define CMD_HELLO '0'
#define CMD_LCC_ADVERTISEMENT '1'
#define CMD_DEV_ADVERTISEMENT '2'
#define CMD_DEV_CONN '3'
#define CMD_COMFIRM '4'
#define CMD_REJECT '5'
#else
#define CMD_HELLO 0
#define CMD_LCC_ADVERTISEMENT 1
#define CMD_DEV_ADVERTISEMENT 2
#define CMD_DEV_CONN 3
#define CMD_COMFIRM 4
#define CMD_REJECT 5
#define CMD_CONTROL 6
#define CMD_MAX 255
#endif

// RECV MSG TYPE
#define RECV_HELLO_MSG 0
#define RECV_LCC_ADVERTISEMENT_MSG 1

__uint32_t lcc_ip = 0;
__uint32_t bcast_ip = 0;
__uint32_t cloud_ip = 0;

char lcc_ip_str[MAX_IP_SIZE] = {0};
char bcast_ip_str[MAX_IP_SIZE] = {0};

int bcast_sock = -1;
int lcc_sock = -1;

void gpio_control(int pin, int lv);
void store_gpio_val(uint32_t *data);
uint64_t get_gpio_mask();

void send_announcement()
{
    // datagram format
    // char msg[64] = {0};

    // snprintf(msg, sizeof(msg), "DEV:DA:%s", DEVICE_ID);
    //
}

void send_solicitation(const in_addr_t *dest, int retry)
{
}

void print_char_arr_hex(char arr[], int len)
{
    for (int i = 0; i < len; i++)
    {
        printf("0x%02X ", arr[i]);
    }

    printf("\n");
}

void control(char *data)
{
    int dev_id = (int)*data;
    ++data;
    // PIN
    int pin = (int)*data;
    printf("Set Pin %d\n", pin);
    ++data;
    // level
    int level = (int)*data;

    gpio_control(pin, level);
}

int parse_cmd(char *data, int len, char *out_param)
{
    // CMD 1B
    int cmd = 0;
    memcpy(&cmd, data, 1);

    if (cmd < CMD_HELLO || cmd > CMD_MAX)
    {
        return -1;
    }

    return cmd;
}

void on_conn_lcc()
{
    if (lcc_ip == 0)
    {
        return;
    }

    char rx_buf[MAX_MSG_SIZE] = {0};
    char msg[MAX_MSG_SIZE] = {0};
    char dev_name[4] = "D01";
    int err = 0;

    printf("Send solicitation to %s\n", inet_ntoa(lcc_ip));

    // CMD
    msg[0] = CMD_DEV_CONN;
    // dev id
    msg[1] = DEVICE_ID;
    // dev name
    memcpy(msg + 2, dev_name, sizeof(dev_name));

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // local ip
    struct sockaddr_in local_addr;
    local_addr.sin_addr.s_addr = inet_addr(device_ip_str);
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(COMMUNICATION_UDP_PORT);

    // lcc ip
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = lcc_ip;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(COMMUNICATION_UDP_PORT);

    err = bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d\n", errno);
    }

    bool is_connected = false;

    // try to connect lcc
    for (int i = 0; i < 3; i++)
    {
        printf("Send No.%d\n", i + 1);
        err = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d\n", errno);
            printf("send retry after 3s...\n");
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            continue;
        }

        // block 5s
        err = recv(sock, rx_buf, sizeof(rx_buf), 0);
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during recv: errno %d\n", errno);
            continue;
        }

        // parse recv msg wait for comfirm
        int cmd = parse_cmd(rx_buf, sizeof(rx_buf), NULL);
        if (cmd == CMD_COMFIRM)
        {
            printf("Connected to LCC successfully!\n");
            is_connected = true;
            break;
        }
    }

    if (!is_connected)
    {
        shutdown(sock, 0);
        close(sock);
        return;
    }

    timeout.tv_sec = 60;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    err = 0;
    // bool heart_beat = false;
    // TODO: need retry time and heartbeat timer?

    // communicate with LCC
    while (1)
    {
        printf("Waiting for LCC msg...\n");
        err = recv(sock, rx_buf, sizeof(rx_buf), 0);
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during recv: errno %d\n", errno);
            printf("Recv LCC err...\n");
            break;
        }

        int cmd = parse_cmd(rx_buf, sizeof(rx_buf), NULL);

        switch (cmd)
        {
        case CMD_HELLO:
            printf("Recive Heatbeat pkt!\n");
            break;

        case CMD_CONTROL:
            printf("Recive control pkt!\n");
            control(rx_buf + 1);
            break;

        default:
            printf("Unknow CMD\n");
            break;
        }
    }

    shutdown(sock, 0);
    close(sock);
}

void on_listen_broadcast(int sock)
{
    char rx_buf[128] = {0};

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(bcast_ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(BROADCAST_UDP_PORT);

    socklen_t socklen = sizeof(dest_addr);
    int retry = 1000;

    bind(sock, (struct sockaddr *)&dest_addr, socklen);

    struct sockaddr_in from_addr;
    bzero(&from_addr, sizeof(from_addr));
    socklen_t from_len = sizeof(from_addr);

    // FIXME: exit status
    // listening broadcast msg
    while (retry > 0)
    {
        retry--;
        printf("On Listen to broadcast...\n");
        // struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        // socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buf, sizeof(rx_buf) - 1, 0, (struct sockaddr *)&from_addr, &from_len);

        // Error occurred during receiving
        if (len < 0)
        {
            ESP_LOGE(TAG, "recvfrom failed: errno %d\n", errno);
            // break;
            continue;
        }
        else
        {
            // Data received
            rx_buf[len] = 0; // Null-terminate whatever we received and treat like a string
            ESP_LOGI(TAG, "Received %d bytes from %s\n", len, inet_ntoa(from_addr.sin_addr));
            ESP_LOGI(TAG, "%s\n", rx_buf);
            // Parse
            int res = -1;
            if ((res = parse_cmd(rx_buf, len, NULL)) > 0)
            {
                switch (res)
                {
                case CMD_HELLO:
                    printf("Recv Heartbeat!\n");
                    break;

                case CMD_LCC_ADVERTISEMENT:
                {
                    printf("Recv LCC Advertisement!\n");
                    // send_solicitation(&from_addr.sin_addr.s_addr, 3);
                    lcc_ip = from_addr.sin_addr.s_addr;
                    on_conn_lcc();
                    break;
                }

                default:
                    break;
                }
                // is_conn_lcc = true;
                // return;
            }
        }
    }
}

bool on_active_discover(int sock)
{
    char msg[] = "Hello";

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(bcast_ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(BROADCAST_UDP_PORT);

    // socklen_t socklen = sizeof(dest_addr);

    while (1)
    {
        // interval send broadcast msg
        int err = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }
    }

    return false;
}

void conn_lcc()
{
    printf("Connect to Local control center...\n");
    // get broadcast ip
    memset(bcast_ip_str, 0, MAX_IP_SIZE);
    sprintf(bcast_ip_str, IPSTR, esp_ip4_addr1(&device_ip), esp_ip4_addr2(&device_ip), esp_ip4_addr3(&device_ip), esp_ip4_addr4(&device_ip) | 0xffU);

    bcast_ip = device_ip.addr;
    bcast_ip |= 0xffU;

    printf("Broadcast IP Address: %s\n", bcast_ip_str);

    // set broadcast ip address
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    // struct sockaddr_in dest_addr;
    // dest_addr.sin_addr.s_addr = inet_addr(bcast_ip_str);
    // dest_addr.sin_family = AF_INET;
    // dest_addr.sin_port = htons(BROADCAST_UDP_PORT);

    bcast_sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (bcast_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d\n", errno);
        return;
    }

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(bcast_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int opt = 1;
    // setsockopt(bcast_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(bcast_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    on_listen_broadcast(bcast_sock);

    shutdown(bcast_sock, 0);
    close(bcast_sock);
    bcast_sock = -1;
    printf("Disconnect from LCC...\n");
}

void on_conn_cloud(int sock)
{
    char rx_buf[MAX_MSG_SIZE] = {0};
    char msg[MAX_MSG_SIZE] = {0};

    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int retry_times = 5;
    // TODO:
    while (retry_times > 0)
    {
        printf("wait for cloud msg...\n");
        memset(rx_buf, 0, sizeof(rx_buf));
        int len = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
        // Error occurred during receiving
        if (len < 0)
        {
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            retry_times--;
            printf("Retry %d...\n", retry_times);
            continue;
        }
        else if (len == 0)
        {
            ESP_LOGE(TAG, "Peer close socket! errno %d", errno);
            // FIXME: don't forget close sock!!!
            return;
        }

        ESP_LOGI(TAG, "Received %d bytes from %s:", len, CLOUD_ADDR);
        ESP_LOGI(TAG, "%s", rx_buf);
        print_char_arr_hex(rx_buf, len);

        // check whether is Heartbeat pkg or not
        uint16_t pack_len = *(uint16_t *)(rx_buf);
        uint32_t cloud_id = *(uint32_t *)(rx_buf + 3);
        printf("Check recv packet len: %hu, Cloud Id: %lu\n", pack_len, cloud_id);
        if (rx_buf[2] == CMD_HELLO && cloud_id == (uint32_t)CLOUD_ID)
        {
            printf("Reply heartbeat msg\n");
            memset(msg, 0, sizeof(msg));
            // comfirm recv
            msg[0] = (uint16_t)MAX_MSG_SIZE;
            msg[2] = (uint8_t)CLOUD_NODE_SYSTEM_ID;
            msg[3] = (uint8_t)CMD_COMFIRM;
            msg[4] = 0;
            msg[5] = (uint8_t)CMD_HELLO;
            msg[6] = (uint8_t)DEVICE_ID;

            send(sock, msg, sizeof(msg), 0);
            retry_times = 5;
        }
        else if (rx_buf[2] == CMD_CONTROL)
        {
            printf("Recive from cloud control pkt!\n");
            control(rx_buf + 3);
            retry_times = 5;
        }
        else
            retry_times--;
    }

    close(sock);
}

void conn_cloud()
{
    printf("Try to connect Cloud Server...\n");

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d\n", errno);
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // local ip
    // struct sockaddr_in local_addr;
    // local_addr.sin_addr.s_addr = inet_addr(device_ip_str);
    // local_addr.sin_family = AF_INET;
    // local_addr.sin_port = htons(COMMUNICATION_UDP_PORT);

    // cloud server ip
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(CLOUD_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(CLOUD_TCP_PORT);

    // Initiate connection
    // TODO:

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        ESP_LOGI(TAG, "Socket connect retry after 5s...");
        close(sock);
        sleep(5);
        return;
    }

    ESP_LOGI(TAG, "Successfully connected");
    sleep(1);

    // connected，send device info
    char rx_buf[MAX_MSG_SIZE] = {0};
    char msg[MAX_MSG_SIZE] = {0};
    char dev_name[4] = "D01";

    // uint8_t cmd = CMD_DEV_CONN;
    // uint8_t dev_id = DEVICE_ID;

    msg[0] = (uint16_t)MAX_MSG_SIZE;
    // cloud server node system id
    msg[2] = (uint8_t)CLOUD_NODE_SYSTEM_ID;
    // CMD
    msg[3] = (uint8_t)CMD_DEV_CONN;
    // dev id
    msg[4] = (uint8_t)DEVICE_ID;
    // dev name
    memcpy(msg + 5, dev_name, sizeof(dev_name));
    // gpio mask (only need 4 byte for now)
    uint64_t gpio_mask = get_gpio_mask();
    memcpy(msg + 9, &gpio_mask, 4);
    // gpio val (only need 4 byte for now)
    uint32_t gpio_val = 0;
    store_gpio_val(&gpio_val);
    memcpy(msg + 13, &gpio_val, sizeof(gpio_val));

    printf("Check gpio_mask: %llu\n", gpio_mask);
    printf("Check gpio_val: %lu\n", gpio_val);

    while (1)
    {
        memset(rx_buf, 0, sizeof(rx_buf));
        int err = send(sock, msg, sizeof(msg), 0);

        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d, retry 5s after...", errno);
            sleep(5);
            close(sock);
            return;
        }

        int len = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
        // Error occurred during receiving
        if (len < 0)
        {
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            continue;
        }
        else if (len == 0)
        {
            ESP_LOGE(TAG, "Peer close socket! errno %d", errno);
            // FIXME: don't forget close sock!!!
            close(sock);
            sock = -1;
            return;
        }

        // Data received
        rx_buf[len] = 0; // Null-terminate whatever we received and treat like a string
        ESP_LOGI(TAG, "Received %d bytes from %s:", len, CLOUD_ADDR);
        ESP_LOGI(TAG, "%s", rx_buf);

        // Check if the server accepts connections
        if (rx_buf[2] == CMD_COMFIRM)
        {
            printf("Cloud Server accept connect request successfully...\n");
            break;
        }

        printf("Send device info retry after 3s...\n");
        sleep(3);
    }

    on_conn_cloud(sock);
}

void node_entry()
{
    printf("Entry Node Mode...\n");
    if (!is_connected)
    {
        printf("Not connect to a wifi network!\n");
        return;
    }

    // node_runing = true;

    while (1)
    {
        if (device_control_mode == mViaLocalControl)
        {
            conn_lcc();
        }
        else
        {
            conn_cloud();
        }
    }
    // TODO: clean and restart
}
