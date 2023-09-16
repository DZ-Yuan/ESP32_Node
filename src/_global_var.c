#include "pch.h"
#include "_common.h"

esp_ip4_addr_t device_ip = {
    .addr = 0,
};
char device_ip_str[18] = {0};

// is connect to wifi LAN
bool is_connected = false;
bool is_conn_cloud = false;
bool is_conn_gateway = false;
// is connect to local control center(APP/miniProgram)
bool is_conn_lcc = false;
bool node_runing = false;

int device_mode = mNode;

// 0 - Passive discovery; 1 - Active discovery
int discover_mode = 0;

int device_control_mode = mViaCloud;
