/*--------------------------------------------------------------------------------------

  dmd_test.cpp
   Demo and example project for the Freetronics DMD, a 512 LED matrix display
   panel arranged in a 32 x 16 layout.

  This LIibrary (DMD32) and example are  fork of original DMD library  was
  modified to work on ESP32 Modified by: Khudhur Alfarhan  // Qudoren@gmail.com
  1/Oct./2020

  See http://www.freetronics.com/dmd for resources and a getting started guide.

  Note that the DMD32 library uses the VSPI port for the fastest, low overhead
  writing to the display. Keep an eye on conflicts if there are any other
  devices running from the same SPI port, and that the chip select on those
  devices is correctly set to be inactive when the DMD is being written to.

  USAGE NOTES
  -----------
  - Place the DMD library folder into the "arduino/libraries/" folder of your
  Arduino installation.
  - Restart the IDE.
  - In the Arduino IDE, go to Tools > Board > and choose any ESP32 board
  - In the Arduino IDE, you can open File > Examples > DMD > dmd_demo, or
  dmd_clock_readout, and get it running straight away!

   See the documentation on Github or attached images to find the pins that
  should be connected to the DMD LED display


  This example code is in the public domain.
  The DMD32 library is open source (GPL), for more see DMD32.cpp and DMD32.h

  --------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------
  Includes
  --------------------------------------------------------------------------------------*/
#include <Arduino.h>
#include <DMD32.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <bitset>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits> // for numeric_limits
#include <queue>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector> // for vector

#include "charge_animations.hpp"
#include "esp_system.h"
#include "espnow_role_manager.hpp"
#include "fonts/Arial14.h"
#include "fonts/Arial_black_16.h"
#include "fonts/SystemFont5x7.h"
#include "master.hpp"
#include "pattern_animator.hpp"
#include "slave.hpp"
#include "strtovector_converter.hpp"
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
// constexpr char* ssid = "TEMEL";
// constexpr char* password = "Nbl.1043";
// constexpr char* ssid = "arduphone";
// constexpr char* password = "arduphone";
constexpr char* ssid = "ART-IN SYSTEMS LOCAL";
constexpr char* password = "Artin2015";

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

void IRAM_ATTR resetModule()
{
    ets_printf("reboot\n");
    esp_restart();
}

char uniquename[15]; // Create a Unique AP from MAC address
char uniqueid[15];
String id;

bool MASTER = true;
bool SLAVE = false;

// volatile bool roleChangeRequested = false; // Flag to indicate role change
// request volatile bool newMasterRole = false;       // Flag to indicate the
// new role

UniqueQueue slave_queue(false);
UniqueQueue proxy_queue(true);
uint8_t copied_mac[6];

// EspNowRoleManager role_manager(on_role_change, false, false);

enum class Animation { ANIM1, ANIM2, ANIM3 };

enum class Flags : unsigned int {
    None = 0,
    Init = 1 << 0,  // 0001
    Shift = 1 << 1, // 0010
    Flag3 = 1 << 2, // 0100
    Flag4 = 1 << 3  // 1000
};

std::vector<std::vector<int>> self_anim_part;

// callback function that will be executed when data is received

/*--------------------------------------------------------------------------------------
  Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this
  gets called at the period set in Timer1.initialize();
  --------------------------------------------------------------------------------------*/
// volatile bool scanFlag = false;
QueueHandle_t scanQueue;
// void IRAM_ATTR triggerScan() {
//     dmd.scanDisplayBySPI();
// }
void IRAM_ATTR triggerScan()
{
    uint8_t dummy = 0;
    xQueueSendFromISR(scanQueue, &dummy, NULL);
}

void scanTask(void* parameter)
{
    uint8_t dummy;
    for (;;) {
        if (xQueueReceive(scanQueue, &dummy, portMAX_DELAY)) {
            dmd.scanDisplayBySPI();
        }
    }
}

void anim_drawMarquee(const char* bChars, byte length)
{
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

void on_role_change(bool master, bool slave)
{
    if (master) {
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

        if (connect_cloud()) {
            Serial.println("Master successfully subscribed to cloud");
            EspNowRoleManager::get_instance().set_cloud_connected();
        }
    }
    else if (slave) {
        // Set device as a Wi-Fi Station
        WiFi.mode(WIFI_STA);

        // constexpr char WIFI_SSID[] = "SWORD";
        int32_t channel = getWiFiChannel(ssid);

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
    // isMaster = master;
    Serial.println(master ? "MASTER" : "SLAVE");
}

/*--------------------------------------------------------------------------------------
  setup
  Called by the Arduino architecture before the main loop begins
  --------------------------------------------------------------------------------------*/
void setup(void)
{
    Serial.begin(115200);
    Serial2.begin(9600);

    scanQueue = xQueueCreate(10, sizeof(uint8_t));
    xTaskCreate(scanTask, "Scan Task", 2048, NULL, 1, NULL);

    EspNowRoleManager::init(on_role_change, false, false);
    EspNowRoleManager::get_instance().set_first_update_required(true); // force master to fetch first time

    // return the clock speed of the CPU
    uint8_t cpuClock = ESP.getCpuFreqMHz();
    // Use 1st timer of 4
    // devide cpu clock speed on its speed value by MHz to get 1us for each
    // signal  of the timer
    timer = timerBegin(0, cpuClock, true);
    // Attach triggerScan function to our timer
    timerAttachInterrupt(timer, &triggerScan, true);
    // Set alarm to call triggerScan function
    // Repeat the alarm (third parameter)
    timerAlarmWrite(timer, 300, true);
    // Start an alarm
    timerAlarmEnable(timer);
    // clear/init the DMD pixels held in RAM
    dmd.clearScreen(true); // true is normal (all pixels off), false is negative
                           // (all pixels on)

    Serial.println("Screen Started");
    Serial.println("Waiting for Role command...");
}

void multianim_horizontal_shift(std::vector<std::vector<int>> grid)
{
    constexpr int ANIM_DELAY = 20; // 20
    constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
    bool flip = false;

    // message_to_send_master.flags.set(0);
    std::vector<std::vector<int>> working_grid;
    auto end_iterator = grid.begin() + 8;
    for (auto it = grid.begin(); it != end_iterator; ++it) {
        working_grid.push_back(*it);
    }

    // Animate Self
    std::vector<std::vector<int>> temp = working_grid;
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
        std::vector<std::vector<int>> temp = working_grid;

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
    temp = working_grid;
    prepare_and_shift_next_matrix(temp,
                                  shift_matrix_up); // 000....1 , last column on
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

        prepare_and_shift_next_matrix(temp,
                                      shift_matrix_up); // 000....1 , last column on
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

class WavePattern {
  private:
    int verticalOffset;
    double horizontalOffset;
    double time;
    std::mt19937 rng; // <random>
    std::uniform_real_distribution<> dist;
    bool horizontalShiftEnabled;
    int horizontalShiftDirection;
    double horizontalShiftExtent;
    const int WIDTH = 64;
    const int HEIGHT = 64;

  public:
    std::vector<std::vector<int>> pattern;
    WavePattern(bool enableHorizontalShift = false) : pattern(HEIGHT, std::vector<int>(WIDTH, 0)), verticalOffset(0), horizontalOffset(0), time(0), rng(std::random_device{}()), dist(-0.05, 0.05), horizontalShiftEnabled(enableHorizontalShift), horizontalShiftDirection(1), horizontalShiftExtent(0)
    {
        updatePattern();
    }

    void updatePattern()
    {
        // Clear the pattern
        for (auto& row : pattern) {
            std::fill(row.begin(), row.end(), 0);
        }

        // Create the wave
        double baseAmplitude = WIDTH / 8.0; // Reduced amplitude for shorter wave tops
        double frequencyMod = 1.0 + 0.1 * std::sin(time * 0.1);

        for (int y = 0; y < HEIGHT; ++y) {
            // Vary amplitude over time and add some randomness
            double amplitude = baseAmplitude * (1 + 0.2 * std::sin(time * 0.05)) + dist(rng);

            // Use a combination of sine waves for more frequent tops
            double wave = std::sin((y + verticalOffset) * 0.4 * frequencyMod) + // Increased frequency
                          0.3 * std::sin((y + verticalOffset) * 0.2 * frequencyMod + time * 0.1);

            double x = (wave * amplitude) + (WIDTH * 0.6); // Moved base position more to the left

            // Apply horizontal offset if enabled
            if (horizontalShiftEnabled) {
                x += horizontalOffset;
            }

            // Only draw the wave if it's within the grid
            if (x >= 0 && x < WIDTH) {
                int xInt = static_cast<int>(x);
                pattern[y][xInt] = 1;
                if (xInt > 0)
                    pattern[y][xInt - 1] = 1;
                if (xInt < WIDTH - 1)
                    pattern[y][xInt + 1] = 1;
            }
        }

        time += 0.1;
    }

    void moveWaveVertical()
    {
        verticalOffset = (verticalOffset + 1) % HEIGHT;
        updatePattern();
    }

    void moveWaveHorizontal()
    {
        if (horizontalShiftEnabled) {
            double minShift = WIDTH * 0.3; // 30% of width as minimum (more to the left)
            double maxShift = WIDTH * 0.8; // 80% of width as maximum
            double shiftRange = maxShift - minShift;

            horizontalShiftExtent += 0.2 * horizontalShiftDirection; // Increased speed

            if (horizontalShiftExtent > shiftRange || horizontalShiftExtent < 0) {
                horizontalShiftDirection *= -1; // Reverse direction
                horizontalShiftExtent = std::max(0.0, std::min(horizontalShiftExtent, shiftRange));
            }

            horizontalOffset = minShift + horizontalShiftExtent - (WIDTH * 0.6); // Adjust for new base position

            updatePattern();
        }
    }

    void setHorizontalShiftEnabled(bool enabled)
    {
        horizontalShiftEnabled = enabled;
    }

    // void print() const
    // {
    //     for (const auto& row : pattern)
    //     {
    //         for (int cell : row)
    //         {
    //             std::cout << (cell ? "1" : ".");
    //         }
    //         std::cout << std::endl;
    //     }
    //     std::cout << std::endl;
    // }
};

void multianim_wave_pattern()
{
    // Create two wave patterns, one with horizontal shift and one without
    WavePattern waveWithShift(true);
    // WavePattern waveWithoutShift(false);

    constexpr int ANIM_DELAY = 50;
    // std::vector<std::vector<int>> grid = waveWithShift.pattern;

    // Animation loop
    for (int i = 0; i < 1000; ++i) {
        // Animate Self
        std::vector<std::vector<int>> self_anim_part;
        auto end_iterator = waveWithShift.pattern.begin() + 8;
        for (auto it = waveWithShift.pattern.begin(); it != end_iterator; ++it) {
            self_anim_part.push_back(*it);
        }

        p10.draw_pattern_static(self_anim_part, 4, 0);

        int count = 2;

        while (!slave_queue.empty()) {
            std::vector<std::vector<int>> slave_anim_part;
            auto end_iterator = waveWithShift.pattern.begin() + (8 * count);
            for (auto it = waveWithShift.pattern.begin() + (8 * (count - 1)); it != end_iterator; ++it) {
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

        waveWithShift.moveWaveVertical();
        // Move the wave horizontally every frame for the shifting version
        waveWithShift.moveWaveHorizontal();

        delay(ANIM_DELAY);
    }
}

/////////////////////////////////
//********** BATTERY ************
/////////////////////////////////

// Function to add a battery fill to the grid
void addBatteryFill(std::vector<std::vector<int>>& grid, int startCol, int width = 3, int height = 8)
{
    if (startCol + width > grid[0].size()) {
        throw std::out_of_range("Battery fill exceeds grid width");
    }

    for (int row = 0; row < height && row < grid.size(); ++row) {
        for (int col = startCol; col < startCol + width; ++col) {
            grid[row][col] = 1;
        }
    }
}

void multianim_battery()
{
    constexpr auto ANIM_DELAY = 50;
    constexpr size_t MAX_BATTERY = 12;
    constexpr size_t BLINK_COUNT = 3;
    int shape_length = 4;
    const int HEIGHT = 8;
    const int WIDTH = 64;

    std::vector<std::vector<int>> blink(HEIGHT, std::vector<int>(WIDTH, 0));
    std::vector<std::vector<int>> battery = p10.Battery;
    // std::vector<std::vector<int>> tempBattery(HEIGHT, std::vector<int>(WIDTH,
    // 0));

    for (size_t i = 0; i <= MAX_BATTERY; ++i) {
        // Animate Self

        // Show - Blink - Show - Blink - Show - Blink,
        // Blink - Show - Blink - Show - Blink - Show
        // Then we increment Battery
        for (int i = 0; i < BLINK_COUNT; ++i) {
            p10.draw_pattern_static(self_anim_part, 4, 0);
            int count = 2;
            // std::cout << "Queue Size: " << slave_queue.size() << "\n";

            // Animate Slaves
            while (!slave_queue.empty()) {
                prepare_next_matrix(blink);

                ++count;

                esp_err_t result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

                proxy_queue.push(slave_queue.top());
                slave_queue.pop();
            }
            while (!proxy_queue.empty()) {
                slave_queue.push(proxy_queue.top());
                proxy_queue.pop();
            }
            // shift_matrix_diagonal_decaying(grid);
            delay(ANIM_DELAY);

            // BLINK

            p10.draw_pattern_static(self_anim_part, 4, 0);
            count = 2;

            // Animate Slaves
            while (!slave_queue.empty()) {
                prepare_next_matrix(battery);

                ++count;

                esp_err_t result = esp_now_send(std::get<0>(slave_queue.top()), (uint8_t*)&message_to_send_master, sizeof(message_to_send_master));

                proxy_queue.push(slave_queue.top());
                slave_queue.pop();
            }
            while (!proxy_queue.empty()) {
                slave_queue.push(proxy_queue.top());
                proxy_queue.pop();
            }
            // shift_matrix_diagonal_decaying(grid);
            delay(ANIM_DELAY);
        }
        // TRIPLE BLINK ENDED
        // TIME TO SHIFT THE BATTERY SIDEWAYS
        if (i < MAX_BATTERY) // IF we are on the last fill, we have to exit, to
                             // avoid going out of grid
        {
            blink = battery; // will be used to act as a blinker for the next
                             // incremented battery
            addBatteryFill(battery, 6 + (i * 5));
        }
    }
}

//****************************
//****************************

void multianim_diagonal_shift(std::vector<std::vector<int>> grid)
{
    constexpr int ANIM_DELAY = 50; // 20
    constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
    size_t MAX_ROW = 8 * (slave_queue.size() + 1);
    bool flip = false;
    // p10.draw_pattern_static(p10.grid, 4, 0);
    // delay(1);
    // message_to_send_master.flags.set(0);

    for (size_t i = 0; i < MAX_ROW; i++) {
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

void multianim_diagonal_shift_fold(std::vector<std::vector<int>> grid)
{
    constexpr int ANIM_DELAY = 50;
    constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
    // constexpr size_t MAX_ROW = 32;
    bool flip = false;
    bool fold = true;
    int shape_length = 4;
    size_t MAX_ROW = 8 * (slave_queue.size() + 1);

    // std::vector<std::vector<int>> grid = p10.square_32;
    // for (auto& row : grid) {
    //     for (auto& el : row) {
    //         Serial.print(el);
    //     }
    //     Serial.println();
    // }
    // Serial.println();
    // Serial.println();

    // message_to_send_master.flags.set(0);

    for (size_t i = 0; i < MAX_ROW - shape_length; ++i) {
        // Animate Self
        std::vector<std::vector<int>> self_anim_part;
        auto end_iterator = grid.begin() + 8;
        for (auto it = grid.begin(); it != end_iterator; ++it) {
            self_anim_part.push_back(*it);
        }
        p10.draw_pattern_static(self_anim_part, 4, 0);
        int count = slave_queue.size() - 1;
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

    // Fill the queue back again
    while (!proxy_queue.empty()) {
        slave_queue.push(proxy_queue.top());
        proxy_queue.pop();
    }
}

// void multianim_diagonal_shift_fold(std::vector<std::vector<int>> grid)
// {
//     constexpr int ANIM_DELAY = 50;
//     constexpr int SWITCH_DELAY = ANIM_DELAY * 4;
//     constexpr size_t MAX_ROW = 32;
//     bool flip = false;
//     bool fold = true;
//     int shape_length = 4;

//     // std::vector<std::vector<int>> grid = p10.square_32;
//     // for (auto& row : grid) {
//     //     for (auto& el : row) {
//     //         Serial.print(el);
//     //     }
//     //     Serial.println();
//     // }
//     // Serial.println();
//     // Serial.println();

//     // message_to_send_master.flags.set(0);

//     for (size_t i = 0; i < MAX_ROW - shape_length; ++i)
//         {
//             // Animate Self
//             std::vector<std::vector<int>> self_anim_part;
//             auto end_iterator = grid.begin() + 8;
//             for (auto it = grid.begin(); it != end_iterator; ++it)
//                 {
//                     self_anim_part.push_back(*it);
//                 }
//             p10.draw_pattern_static(self_anim_part, 4, 0);
//             int count = 2;
//             // std::cout << "Queue Size: " << slave_queue.size() << "\n";

//             // Animate Slaves
//             while (!slave_queue.empty())
//                 {
//                     std::vector<std::vector<int>> slave_anim_part;
//                     auto end_iterator = grid.begin() + (8 * count);
//                     for (auto it = grid.begin() + (8 * (count - 1)); it !=
//                     end_iterator; ++it)
//                         {
//                             slave_anim_part.push_back(*it);
//                         }
//                     prepare_next_matrix(slave_anim_part);

//                     ++count;

//                     esp_err_t result
//                       = esp_now_send(std::get<0>(slave_queue.top()),
//                       (uint8_t*)&message_to_send_master,
//                       sizeof(message_to_send_master));

//                     proxy_queue.push(slave_queue.top());
//                     slave_queue.pop();
//                 }
//             while (!proxy_queue.empty())
//                 {
//                     slave_queue.push(proxy_queue.top());
//                     proxy_queue.pop();
//                 }
//             shift_matrix_diagonal_decaying(grid);
//             delay(ANIM_DELAY);
//         }

//     while (!slave_queue.empty())
//         {
//             proxy_queue.push(slave_queue.top());
//             slave_queue.pop();
//         }

//     delay(ANIM_DELAY);

//     // Fold to reverse direction
//     for (size_t i = 0; i < MAX_ROW + 1; ++i)
//         {
//             int count = 4;
//             while (!proxy_queue.empty())
//                 {
//                     std::vector<std::vector<int>> slave_anim_part;
//                     auto end_iterator = grid.begin() + (8 * count);
//                     for (auto it = grid.begin() + (8 * (count - 1)); it !=
//                     end_iterator; ++it)
//                         {
//                             slave_anim_part.push_back(*it);
//                         }
//                     prepare_next_matrix(slave_anim_part);

//                     --count;

//                     esp_err_t result
//                       = esp_now_send(std::get<0>(proxy_queue.top()),
//                       (uint8_t*)&message_to_send_master,
//                       sizeof(message_to_send_master));

//                     slave_queue.push(proxy_queue.top());
//                     proxy_queue.pop();
//                 }
//             while (!slave_queue.empty())
//                 {
//                     proxy_queue.push(slave_queue.top());
//                     slave_queue.pop();
//                 }
//             // Animate Self
//             std::vector<std::vector<int>> self_anim_part;
//             auto end_iterator = grid.begin() + 8;
//             for (auto it = grid.begin(); it != end_iterator; ++it)
//                 {
//                     self_anim_part.push_back(*it);
//                 }
//             p10.draw_pattern_static(self_anim_part, 4, 0);

//             shift_matrix_diagonal_decaying_upwards(grid);
//             delay(ANIM_DELAY);
//         }

//     // Fill the queue back again
//     while (!proxy_queue.empty())
//         {
//             slave_queue.push(proxy_queue.top());
//             proxy_queue.pop();
//         }
// }

void multianim_scrolling_marquee(std::vector<String>& display_texts)
{
    // message_to_send_master.flags.set(1); // marquee on

    // for (auto& el : display_texts) {
    //     Serial.print(el + ", ");
    // }
    // Serial.println();

    std::vector<const char*> bChars;
    for (const auto& text : display_texts) {
        bChars.push_back(text.c_str());
    }

    // Animate Self
    const char* first = bChars[0];
    anim_drawMarquee(first, static_cast<byte>(std::strlen(first)));
    bChars.erase(bChars.begin());

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

void multianim_text_shift(const char* text)
{
    // For the maximum 64x64 grid
    auto grid64x64 = stringToLEDGrid(text, std ::strlen(text), 0, 0, 64);
}
/*--------------------------------------------------------------------------------------
  loop
  Arduino architecture main loop
  --------------------------------------------------------------------------------------*/
// bool toggle = false;

// void loop()
// {
//     while (!Serial)
//     {
//         ;
//     }

//     if (!toggle)
//     {
//         // Print the grid
//         Serial.println("LED Grid (64x64):");
//         for (const auto& row : grid64x64)
//         {
//             for (int pixel : row)
//             {
//                 Serial.print(pixel ? '#' : '.');
//             }
//             Serial.println(); // New line after each row
//         }

//         Serial.println(); // Extra line for readability

//         toggle = true;
//     }

//     delay(100000);
// }

void loop(void)
{
    timerWrite(timer, 0); // reset timer (feed watchdog)

    byte b;
    // 10 x 14 font clock, including demo of OR and NOR modes for pixels so that
    // the flashing colon can be overlayed dmd.clearScreen(true);
    dmd.selectFont(System5x7);

    // Get Role from the parent Esp32
    auto cmd = serial2_get_data("ms_", "!");
    // 0 - 32
    //
    cmd = "0";
    bool singular = false; // multi-sync vs singular mode
                           // auto cmd = "0";

    if (singular) {
        //+----------------------------------------------------+
        // Singular Mode
        //+----------------------------------------------------+
        while (true) {
            anim_StationWaiting_custom(p10, "TEST", 4);
            delay(100);
        }

        // simulate 8 seconds per amp increase
        // run_every_seconds(1000, charge_bat);
        // anim_charge_battery(p10, amp / 4);
        // convert string amp into int
        // int amp = atoi(cmd.c_str());
    }
    else {
        //+----------------------------------------------------+
        // Multi-Sync Mode
        //+----------------------------------------------------+
        if (cmd == "1") {
            // Serial.println(cmd.c_str());
            EspNowRoleManager::get_instance().set_master();
            // Serial.println("You are the MASTER");
        }
        else if (cmd == "0") {
            EspNowRoleManager::get_instance().set_slave();
            // Serial.println("You are a SLAVE");
        }

        // Master specific block
        if (EspNowRoleManager::get_instance().is_master()) {
            // If Led Endpoint connected, send heartbeat to check for
            // updates
            if (EspNowRoleManager::get_instance().is_cloud_connected()) {
                run_every_seconds(15000, send_heartbeat);
            }

            if (EspNowRoleManager::get_instance().is_update_required() || EspNowRoleManager::get_instance().first_update_required()) {
                Serial.println("[MASTER] -> Update signal received");
                EspNowRoleManager::get_instance().set_first_update_required(false);

                if (get_action_from_cloud()) {
                    Serial.println("[MASTER] -> Action received from cloud");
                    setup_action(slave_queue);
                }
            }
        }

        // Perform action
        if (EspNowRoleManager::get_instance().is_master() && EspNowRoleManager::get_instance().is_action_set()) {
            // Serial.println("Action set, animation starting...");
            if (EspNowRoleManager::get_instance().is_pattern) {
                // Serial.println("[It's pattern]");
                // Animation Selection
                // Serial.print("Anim in check: ");
                // Serial.print(EspNowRoleManager::get_instance().pattern_animation);
                for (const String& animation : EspNowRoleManager::get_instance().animation_list) {
                    if (animation == "h_scroll") {
                        multianim_horizontal_shift(p10.line);
                    }
                    else if (animation == "d_scroll_fold") {
                        multianim_diagonal_shift_fold(p10.ball_64);
                    }
                    else if (animation == "wave") {
                        multianim_wave_pattern();
                    }
                    else if (animation == "battery") {
                        multianim_battery();
                    }
                    // TODO:
                    else if (animation == "h_scroll_text") {
                        multianim_text_shift("XSARJ");
                    }
                }
            }
            else {
                multianim_scrolling_marquee(EspNowRoleManager::get_instance().display_texts);
            }

            delay(1);
        }

        if (EspNowRoleManager::get_instance().is_slave()) {
            if (should_animate) {
                if (message_to_rcv_slave.flags.test(0)) {
                    p10.draw_pattern_static(reconstructedGrid, 4, 0);
                }
                else if (message_to_rcv_slave.flags.test(1)) {
                    // Serial.println(message_to_rcv.bChar);
                    anim_drawMarquee(message_to_rcv_slave.bChar, static_cast<byte>(std::strlen(message_to_rcv_slave.bChar)));
                }
                should_animate = false;
            }

            delay(1);
        }
    }
}
