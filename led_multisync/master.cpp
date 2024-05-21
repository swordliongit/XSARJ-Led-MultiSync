#include "master.hpp"

const bool DEBUG = false;

struct_message_to_receive_master message_to_rcv_master;
struct_message_to_send_master message_to_send_master;

void on_data_sent_master(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (DEBUG) {
        Serial.print("\r\nLast Packet Send Status:\t");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
        char macStr[18];
        Serial.print("Packet to: ");
        // Copies the sender mac address to a string
        snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        Serial.print(macStr);
        Serial.print(" send status:\t");
    }
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    // Serial.println(sizeof(message_to_send.anim));
}

void on_data_recv_master(const uint8_t* mac, const uint8_t* incomingData, int len) {
    // Serial.println((uintptr_t)mac, HEX);
    memcpy(&message_to_rcv_master, incomingData, sizeof(message_to_rcv_master));

    Serial.print("Bytes received: ");
    Serial.println(len);
}
