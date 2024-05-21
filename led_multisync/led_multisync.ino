/*--------------------------------------------------------------------------------------

  dmd_test.cpp
   Demo and example project for the Freetronics DMD, a 512 LED matrix display
   panel arranged in a 32 x 16 layout.

  This LIibrary (DMD32) and example are  fork of original DMD library  was modified to work on ESP32
  Modified by: Khudhur Alfarhan  // Qudoren@gmail.com
  1/Oct./2020

  See http://www.freetronics.com/dmd for resources and a getting started guide.

  Note that the DMD32 library uses the VSPI port for the fastest, low overhead writing to the
  display. Keep an eye on conflicts if there are any other devices running from the same
  SPI port, and that the chip select on those devices is correctly set to be inactive
  when the DMD is being written to.

  USAGE NOTES
  -----------
  - Place the DMD library folder into the "arduino/libraries/" folder of your Arduino installation.
  - Restart the IDE.
  - In the Arduino IDE, go to Tools > Board > and choose any ESP32 board
  - In the Arduino IDE, you can open File > Examples > DMD > dmd_demo, or dmd_clock_readout, and get it
   running straight away!

   See the documentation on Github or attached images to find the pins that should be connected to the DMD LED display


  This example code is in the public domain.
  The DMD32 library is open source (GPL), for more see DMD32.cpp and DMD32.h

  --------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------
  Includes
  --------------------------------------------------------------------------------------*/
#include "esp_system.h"
#include <DMD32.h> //
#include "fonts/SystemFont5x7.h"
#include "fonts/Arial_black_16.h"
#include "fonts/Arial14.h"


#include <iostream>
#include <vector> // for vector
#include <limits> // for numeric_limits
#include <thread>
#include <tuple>

#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <Update.h>
#include <HTTPClient.h>

#include <esp_now.h>
#include <queue>
#include <set>
#include <tuple>
#include <string>
#include <esp_wifi.h>
#include <bitset>
#include <cstring>

#include "pattern_animator.hpp"
#include "charge_animations.hpp"
#include "master.hpp"
#include "slave.hpp"
#include "uqueue.hpp"


#define DISPLAYS_ACROSS 2
#define DISPLAYS_DOWN 1
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);
PatternAnimator p10(&dmd);


#define HOST_OF_OTA "http://panel.xsarj.com:8081/web/content/2568?download=true"
HTTPClient client;
// Global variables
int totalLength;       // total size of firmware
int currentLength = 0; // current size of written firmware
int version = 0;
int camera_version = 0;
int http_get_version = 9999;

const char* host = "esp32";
constexpr char* ssid = "ARTINSYSTEMS";
constexpr char* password = "Artin2023Artin";
// constexpr char *ssid = "SL-MOBILE";
// constexpr char *password = "sword1998";

String wifiname = "";
String wifipassword = "";

String wifi_name = "arduphone";
String wifi_password = "arduphone";

int ble_status = 0;
int wifi_status = 0;
int connection_status = 0;
int last_connection_status = 0;
int wifi_strength = 0;
float test_signal_time = 0;

int wifi_name_found = 0;
String timestamp = "0";

int wifi_connected_counter = 0;

unsigned long previousMillis_wifi_timer = 0;
const long interval_wifi_timer = 30000;

String raw_serial2 = "";
String data_from_serial2 = "6";
char screen_test_char[] = "test";
String screen_test_string = "XSarj";

bool charge_started = false;
bool charge_stopped = false;

// TaskHandle_t Task1; -> for esp32 threads

const int wdtTimeout = 60000; // time in ms to trigger the watchdog
hw_timer_t* timer = NULL;

void IRAM_ATTR resetModule() {
    ets_printf("reboot\n");
    esp_restart();
}

char uniquename[15]; // Create a Unique AP from MAC address
char uniqueid[15];
String id;

bool MASTER = true;
bool SLAVE = false;
bool INIT = true;

UniqueQueue slave_queue(false);
UniqueQueue proxy_queue(true);
uint8_t copied_mac[6];


enum class Animation
{
    ANIM1,
    ANIM2,
    ANIM3
};

enum class Flags : unsigned int
{
    None = 0,
    Init = 1 << 0,  // 0001
    Shift = 1 << 1, // 0010
    Flag3 = 1 << 2, // 0100
    Flag4 = 1 << 3  // 1000
};

std::vector<std::vector<int>> self_anim_part;

// callback function that will be executed when data is received


/*--------------------------------------------------------------------------------------
  Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this gets
  called at the period set in Timer1.initialize();
  --------------------------------------------------------------------------------------*/
void IRAM_ATTR triggerScan() {
    dmd.scanDisplayBySPI();
}

void anim_drawMarquee(const char* bChars, byte length) {
    dmd.drawMarquee(bChars, length, (32 * DISPLAYS_ACROSS) - 1, 4);
    long start = millis();
    long timer = start;
    boolean ret = false;
    while (!ret) {
        if ((timer + 21) < millis()) {
            ret = dmd.stepMarquee(-1, 0);
            timer = millis();
        }
    }
}

/*--------------------------------------------------------------------------------------
  setup
  Called by the Arduino architecture before the main loop begins
  --------------------------------------------------------------------------------------*/
void setup(void) {
    Serial.begin(115200);
    Serial2.begin(9600);

    if (INIT) {
        // send mac to first ESP
        // master or slave?
        ;
    }
    if (MASTER) {
        // ESP-NOW and WiFi working simultaneously
        // Set device as a Wi-Fi Station
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(ssid, password);
        // //check wi-fi is connected to wi-fi network
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.print(".");
        }
        Serial.print("Station IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Wi-Fi Channel: ");
        Serial.println(WiFi.channel());
        Serial.println(WiFi.macAddress());
        // Init ESP-NOW
        if (esp_now_init() != ESP_OK) {
            Serial.println("Error initializing ESP-NOW");
            return;
        }

        // Once ESPNow is successfully Init, we will register for recv CB to
        // get recv packer info
        esp_now_register_recv_cb(on_data_recv_master);
        // Once ESPNow is successfully Init, we will register for Send CB to
        // get the status of Trasnmitted packet
        esp_now_register_send_cb(on_data_sent_master);

        pinMode(LED_BUILTIN, OUTPUT);   // blue led on chip to notify boot is pressed
        digitalWrite(LED_BUILTIN, LOW); // start off


        register_peers(slave_queue);
        connect_cloud();
    } else if (SLAVE) {
        // Set device as a Wi-Fi Station
        WiFi.mode(WIFI_STA);

        constexpr char WIFI_SSID[] = "ARTINSYSTEMS";
        int32_t channel = getWiFiChannel(WIFI_SSID);

        WiFi.printDiag(Serial); // Uncomment to verify channel number before
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        WiFi.printDiag(Serial); // Uncomment to verify channel change after

        // Init ESP-NOW
        if (esp_now_init() != ESP_OK) {
            Serial.println("Error initializing ESP-NOW");
            return;
        }

        // Once ESPNow is successfully Init, we will register for recv CB to
        // get recv packer info
        esp_now_register_recv_cb(on_data_recv_slave);
        // Once ESPNow is successfully Init, we will register for Send CB to
        // get the status of Trasnmitted packet
        esp_now_register_send_cb(on_data_sent_slave);

        pinMode(LED_BUILTIN, OUTPUT);   // blue led on chip to notify boot is pressed
        digitalWrite(LED_BUILTIN, LOW); // start off

        // Register peer
        peerInfo.channel = 0;
        peerInfo.encrypt = false;

        uint8_t broadcastAddress[6] = {0xB0, 0xA7, 0x32, 0xDB, 0xC5, 0x3C};
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("Failed to add peer");
            return;
        }
    }

    // return the clock speed of the CPU
    uint8_t cpuClock = ESP.getCpuFreqMHz();
    // Use 1st timer of 4
    // devide cpu clock speed on its speed value by MHz to get 1us for each signal  of the timer
    timer = timerBegin(0, cpuClock, true);
    // Attach triggerScan function to our timer
    timerAttachInterrupt(timer, &triggerScan, true);
    // Set alarm to call triggerScan function
    // Repeat the alarm (third parameter)
    timerAlarmWrite(timer, 300, true);
    // Start an alarm
    timerAlarmEnable(timer);
    // clear/init the DMD pixels held in RAM
    dmd.clearScreen(true); // true is normal (all pixels off), false is negative (all pixels on)

    Serial.println("Screen Started");
    Serial.println("Waiting For Serial Datas");
}


void multianim_horizontal_shift() {
    constexpr int ANIM_DELAY = 20; // 20
    constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
    bool flip = false;

    message_to_send_master.flags.set(0);

    // Animate Self
    std::vector<std::vector<int>> temp = p10.grid;
    p10.draw_pattern_static(temp, 4, 0);
    delay(ANIM_DELAY);
    for (int i = 0; i < 7; i++) {
        shift_matrix_down(temp);
        p10.draw_pattern_static(temp, 4, 0);
        delay(ANIM_DELAY);
    }
    p10.draw_pattern_static(p10.grid_turned_off, 4, 0);
    delay(SWITCH_DELAY);

    // Animate Slaves
    while (!slave_queue.empty()) {
        std::vector<std::vector<int>> temp = p10.grid;

        prepare_next_matrix(temp);
        esp_err_t result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));
        delay(ANIM_DELAY);

        for (int i = 0; i < 7; i++) {
            prepare_and_shift_next_matrix(temp, shift_matrix_down);
            result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));
            // delete[] message_to_send.bitString;
            delay(ANIM_DELAY);
        }
        // Clear the slave's screen
        prepare_next_matrix(p10.grid_turned_off);
        result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

        proxy_queue.push(slave_queue.top());
        slave_queue.pop();

        delay(SWITCH_DELAY);
    }
    delay(SWITCH_DELAY * 2);
    temp = p10.grid;
    prepare_and_shift_next_matrix(temp, shift_matrix_up); // 000....1 , last column on
    while (!proxy_queue.empty()) {
        esp_err_t result = esp_now_send(std::get<0>(proxy_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));
        delay(ANIM_DELAY);

        for (int i = 0; i < 7; i++) {
            prepare_and_shift_next_matrix(temp, shift_matrix_up);
            result = esp_now_send(std::get<0>(proxy_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));
            delay(ANIM_DELAY);
        }
        // Clear the slave's screen
        prepare_next_matrix(p10.grid_turned_off);
        result = esp_now_send(std::get<0>(proxy_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

        slave_queue.push(proxy_queue.top());
        proxy_queue.pop();

        prepare_and_shift_next_matrix(temp, shift_matrix_up); // 000....1 , last column on
        delay(SWITCH_DELAY);
    }
    // Animate Self
    p10.draw_pattern_static(temp, 4, 0);
    delay(ANIM_DELAY);
    for (int i = 0; i < 7; i++) {
        shift_matrix_up(temp);
        p10.draw_pattern_static(temp, 4, 0);
        delay(ANIM_DELAY);
    }
    p10.draw_pattern_static(p10.grid_turned_off, 4, 0);
    delay(SWITCH_DELAY * 2);
}


void multianim_diagonal_shift() {
    constexpr int ANIM_DELAY = 50; // 20
    constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
    constexpr size_t MAX_ROW = 32;
    bool flip = false;
    // p10.draw_pattern_static(p10.grid, 4, 0);
    // delay(1);
    message_to_send_master.flags.set(0);

    std::vector<std::vector<int>> grid = p10.square_32;
    for (size_t i = 0; i < MAX_ROW + 1; i++) {
        std::vector<std::vector<int>> self_anim_part;
        auto end_iterator = grid.begin() + 8;
        for (auto it = grid.begin(); it != end_iterator; ++it) {
            self_anim_part.push_back(*it);
        }
        p10.draw_pattern_static(self_anim_part, 4, 0);
        // delay(ANIM_DELAY);
        int count = 2;
        // Serial.println(slave_queue.size());
        while (!slave_queue.empty()) {
            std::vector<std::vector<int>> slave_anim_part;
            auto end_iterator = grid.begin() + (8 * count);
            for (auto it = grid.begin() + (8 * (count - 1)); it != end_iterator; ++it) {
                slave_anim_part.push_back(*it);
            }
            prepare_next_matrix(slave_anim_part);

            ++count;

            esp_err_t result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

            proxy_queue.push(slave_queue.top());
            slave_queue.pop();
        }
        while (!proxy_queue.empty()) {
            slave_queue.push(proxy_queue.top());
            proxy_queue.pop();
        }
        shift_matrix_diagonal_decaying(grid);
        delay(ANIM_DELAY);
    }
    delay(ANIM_DELAY * 4);
}


void multianim_diagonal_shift_fold() {
    constexpr int ANIM_DELAY = 50;
    constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
    constexpr size_t MAX_ROW = 32;
    bool flip = false;
    bool fold = true;
    int shape_length = 4;

    std::vector<std::vector<int>> grid = p10.square_32;

    message_to_send_master.flags.set(0);

    for (size_t i = 0; i < MAX_ROW - shape_length; ++i) {
        // Animate Self
        std::vector<std::vector<int>> self_anim_part;
        auto end_iterator = grid.begin() + 8;
        for (auto it = grid.begin(); it != end_iterator; ++it) {
            self_anim_part.push_back(*it);
        }
        p10.draw_pattern_static(self_anim_part, 4, 0);
        int count = 2;
        // std::cout << "Queue Size: " << slave_queue.size() << "\n";

        // Animate Slaves
        while (!slave_queue.empty()) {
            std::vector<std::vector<int>> slave_anim_part;
            auto end_iterator = grid.begin() + (8 * count);
            for (auto it = grid.begin() + (8 * (count - 1)); it != end_iterator; ++it) {
                slave_anim_part.push_back(*it);
            }
            prepare_next_matrix(slave_anim_part);

            ++count;

            esp_err_t result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

            proxy_queue.push(slave_queue.top());
            slave_queue.pop();
        }
        while (!proxy_queue.empty()) {
            slave_queue.push(proxy_queue.top());
            proxy_queue.pop();
        }
        shift_matrix_diagonal_decaying(grid);
        delay(ANIM_DELAY);
    }

    while (!slave_queue.empty()) {
        proxy_queue.push(slave_queue.top());
        slave_queue.pop();
    }

    delay(ANIM_DELAY);

    // Fold to reverse direction
    for (size_t i = 0; i < MAX_ROW + 1; ++i) {
        int count = 4;
        while (!proxy_queue.empty()) {
            std::vector<std::vector<int>> slave_anim_part;
            auto end_iterator = grid.begin() + (8 * count);
            for (auto it = grid.begin() + (8 * (count - 1)); it != end_iterator; ++it) {
                slave_anim_part.push_back(*it);
            }
            prepare_next_matrix(slave_anim_part);

            --count;

            esp_err_t result = esp_now_send(std::get<0>(proxy_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

            slave_queue.push(proxy_queue.top());
            proxy_queue.pop();
        }
        while (!slave_queue.empty()) {
            proxy_queue.push(slave_queue.top());
            slave_queue.pop();
        }
        // Animate Self
        std::vector<std::vector<int>> self_anim_part;
        auto end_iterator = grid.begin() + 8;
        for (auto it = grid.begin(); it != end_iterator; ++it) {
            self_anim_part.push_back(*it);
        }
        p10.draw_pattern_static(self_anim_part, 4, 0);

        shift_matrix_diagonal_decaying_upwards(grid);
        delay(ANIM_DELAY);
    }
}

void multianim_scrolling_marquee() {
    message_to_send_master.flags.set(1); // marquee on

    std::vector<const char*> bChars{"Adana", "Istanbul", "Bursa"};

    // Animate Self
    anim_drawMarquee("Antalya", static_cast<byte>(std::strlen("Antalya")));

    while (!slave_queue.empty()) {
        const char* current_bChar = bChars.front();
        strcpy(message_to_send_master.bChar, current_bChar);
        bChars.erase(bChars.begin());

        if (DEBUG) {
            Serial.println(message_to_send_master.bChar);
        }

        esp_err_t result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

        proxy_queue.push(slave_queue.top());
        slave_queue.pop();
    }
    while (!proxy_queue.empty()) {
        slave_queue.push(proxy_queue.top());
        proxy_queue.pop();
    }
}

/*--------------------------------------------------------------------------------------
  loop
  Arduino architecture main loop
  --------------------------------------------------------------------------------------*/


void loop(void) {
    timerWrite(timer, 0); // reset timer (feed watchdog)

    byte b;
    // 10 x 14 font clock, including demo of OR and NOR modes for pixels so that the flashing colon can be overlayed
    dmd.clearScreen(true);
    dmd.selectFont(System5x7);

    if (MASTER) {
        multianim_diagonal_shift_fold();
        // multianim_scrolling_marquee();
        // multianim_diagonal_shift();
        delay(1);
    } else if (SLAVE) {
        if (should_animate) {
            if (message_to_rcv_slave.flags.test(0)) {
                p10.draw_pattern_static(reconstructedGrid, 4, 0);
            } else if (message_to_rcv_slave.flags.test(1)) {
                // Serial.println(message_to_rcv.bChar);
                anim_drawMarquee(message_to_rcv_slave.bChar, static_cast<byte>(std::strlen(message_to_rcv_slave.bChar)));
            }
        }

        delay(1);
    }
}
