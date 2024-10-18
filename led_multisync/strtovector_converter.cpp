
#include "strtovector_converter.hpp"
#include <DMD32.h>
#include "fonts/SystemFont5x7.h"

void VirtualDMD::writePixel(unsigned int bX, unsigned int bY, byte bGraphicsMode, byte bPixel)
{
    if (bX >= width || bY >= height) {
        return;
    }

    // Determine which panel the pixel belongs to
    int panel = bX / DMD_PIXELS_ACROSS;
    int xWithinPanel = bX % DMD_PIXELS_ACROSS;

    // Calculate the actual x-coordinate within the grid
    int actualX = panel * DMD_PIXELS_ACROSS + xWithinPanel;

    bool pixelState;
    switch (bGraphicsMode) {
    case GRAPHICS_NORMAL:
        pixelState = (bPixel == true); // true means pixel on
        break;
    case GRAPHICS_INVERSE: pixelState = (bPixel == false); break;
    case GRAPHICS_TOGGLE: pixelState = !grid[bY][actualX]; break;
    case GRAPHICS_OR: pixelState = grid[bY][actualX] | (bPixel == true); break;
    case GRAPHICS_NOR: pixelState = !(grid[bY][actualX] | (bPixel == true)); break;
    default: pixelState = (bPixel == true);
    }
    grid[bY][actualX] = pixelState;
}

void VirtualDMD::drawString(int bX, int bY, const char* bChars, byte length, byte bGraphicsMode)
{
    if (bX >= width || bY >= height)
        return;
    uint8_t height = pgm_read_byte(Font + FONT_HEIGHT);
    if (bY + height < 0)
        return;

    int strWidth = 0;
    for (int i = 0; i < length; i++) {
        int charWide = drawChar(bX + strWidth, bY, bChars[i], bGraphicsMode);
        if (charWide > 0) {
            strWidth += charWide + 1;
        }
        else if (charWide < 0) {
            return;
        }
        if ((bX + strWidth) >= width || bY >= height)
            return;
    }
}

void VirtualDMD::selectFont(const uint8_t* font)
{
    Font = font;
}

int VirtualDMD::drawChar(int bX, int bY, const unsigned char letter, byte bGraphicsMode)
{
    if (bX >= width || bY >= height)
        return -1;
    unsigned char c = letter;
    uint8_t height = pgm_read_byte(Font + FONT_HEIGHT);
    if (c == ' ') {
        int charWide = charWidth(' ');
        return charWide;
    }
    uint8_t width = 0;
    uint8_t bytes = (height + 7) / 8;

    uint8_t firstChar = pgm_read_byte(Font + FONT_FIRST_CHAR);
    uint8_t charCount = pgm_read_byte(Font + FONT_CHAR_COUNT);

    uint16_t index = 0;

    if (c < firstChar || c >= (firstChar + charCount))
        return 0;
    c -= firstChar;

    if (pgm_read_byte(Font + FONT_LENGTH) == 0 && pgm_read_byte(Font + FONT_LENGTH + 1) == 0) {
        width = pgm_read_byte(Font + FONT_FIXED_WIDTH);
        index = c * bytes * width + FONT_WIDTH_TABLE;
    }
    else {
        for (uint8_t i = 0; i < c; i++) {
            index += pgm_read_byte(Font + FONT_WIDTH_TABLE + i);
        }
        index = index * bytes + charCount + FONT_WIDTH_TABLE;
        width = pgm_read_byte(Font + FONT_WIDTH_TABLE + c);
    }
    if (bX < -width || bY < -height)
        return width;

    // Draw the character
    for (uint8_t j = 0; j < width; j++) {
        for (uint8_t i = bytes - 1; i < 254; i--) {
            uint8_t data = pgm_read_byte(Font + index + j + (i * width));
            int offset = (i * 8);
            if ((i == bytes - 1) && bytes > 1) {
                offset = height - 8;
            }
            for (uint8_t k = 0; k < 8; k++) {
                if ((offset + k >= i * 8) && (offset + k <= height)) {
                    if (data & (1 << k)) {
                        writePixel(bX + j, bY + offset + k, bGraphicsMode, true);
                    }
                    else {
                        writePixel(bX + j, bY + offset + k, bGraphicsMode, false);
                    }
                }
            }
        }
    }
    return width;
}

int VirtualDMD::charWidth(const unsigned char letter)
{
    unsigned char c = letter;
    if (c == ' ')
        c = 'n';
    uint8_t width = 0;

    uint8_t firstChar = pgm_read_byte(Font + FONT_FIRST_CHAR);
    uint8_t charCount = pgm_read_byte(Font + FONT_CHAR_COUNT);

    if (c < firstChar || c >= (firstChar + charCount)) {
        return 0;
    }
    c -= firstChar;

    if (pgm_read_byte(Font + FONT_LENGTH) == 0 && pgm_read_byte(Font + FONT_LENGTH + 1) == 0) {
        width = pgm_read_byte(Font + FONT_FIXED_WIDTH);
    }
    else {
        width = pgm_read_byte(Font + FONT_WIDTH_TABLE + c);
    }
    return width;
}

std::vector<std::vector<int>> VirtualDMD::getGrid() const
{
    std::vector<std::vector<int>> result(height, std::vector<int>(width, 0));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            result[y][x] = grid[y][x] ? 1 : 0;
        }
    }
    return result;
}

std::vector<std::vector<int>> stringToLEDGrid(const char* bChars, byte length, int left, int top, int totalRows)
{
    return generateFlexibleGrid(bChars, length, left, top, totalRows);
}

std::vector<std::vector<int>> generateFlexibleGrid(const char* text, int length, int left, int top, int totalRows)
{
    // First, generate the 8x64 grid
    VirtualDMD vdmd(2, 1); // 2 displays across, 1 display down (8x64)
    vdmd.selectFont(System5x7);
    vdmd.drawString(left, top, text, length, GRAPHICS_NORMAL);
    auto baseGrid = vdmd.getGrid();

    // Ensure we have at least 8 rows and no more than 64
    totalRows = std::max(8, std::min(totalRows, 64));

    // Create the final grid with the desired number of rows
    std::vector<std::vector<int>> finalGrid(totalRows, std::vector<int>(64, 0));

    // Copy the base grid (which is 8x64) into the final grid
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 64; ++j) {
            finalGrid[i][j] = baseGrid[i][j];
        }
    }

    // The remaining rows (if any) are already filled with zeros

    return finalGrid;
}