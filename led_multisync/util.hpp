#ifndef UTIL_H
#define UTIL_H


#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <vector>
#include <bitset>
#include <functional>
#include <tuple>
#include "uqueue.hpp"

#include "pattern_animator.hpp"

#include <DMD32.h> //

extern esp_now_peer_info_t peerInfo;

#define DISPLAYS_ACROSS 2
#define DISPLAYS_DOWN 1
extern DMD dmd;
extern PatternAnimator p10;

void serial2_get_data(String raw_serial2, String& data);

void register_peers(UniqueQueue& slave_queue);

void connect_cloud();

int32_t getWiFiChannel(const char* ssid);

void prepare_next_matrix(std::vector<std::vector<int>>& matrix);

void prepare_and_shift_next_matrix(std::vector<std::vector<int>>& matrix, std::function<void(std::vector<std::vector<int>>&)> shifter, bool should_print = false);

void shift_matrix_down(std::vector<std::vector<int>>& matrix);

void shift_matrix_up(std::vector<std::vector<int>>& matrix);

// Function to convert 2D vector to a bit string
std::string convertToBitString(const std::vector<std::vector<int>>& grid);

// Function to compress the bit string into bit fields
std::string compressBitString(const std::string& bitString);

// Function to convert bit string back into 2D vector
std::vector<std::vector<int>> convertFromBitString(const std::string& bitString, int numRows, int numCols);

// Function to decompress the compressed string back into bit string
std::string decompressBitString(const std::string& compressedString);

// Function to print binary representation of a character
std::string binaryString(unsigned char c);

void shift_matrix_diagonal_once(std::vector<std::vector<int>>& grid);

void shift_matrix_diagonal_decaying(std::vector<std::vector<int>>& grid);

void shift_matrix_diagonal_decaying_upwards(std::vector<std::vector<int>>& grid);

void shift_matrix_right(std::vector<std::vector<int>>& grid);

#endif