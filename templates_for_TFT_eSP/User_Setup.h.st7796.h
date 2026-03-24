//                            USER DEFINED SETTINGS
//   Set driver type, fonts to be loaded, pins used and SPI control method etc.
//
//   Select ONE display driver below by uncommenting the appropriate line.
//   Only one may be active at a time.

// --- Choose your display (uncomment ONE) ---
#define USE_ST7796_480x320

// ##################################################################################
//
// Section 1. Call up the right driver file and any options for it
//
// ##################################################################################

#if defined(USE_ST7789_240x320)

  #define USER_SETUP_INFO "ESP32S3_ST7789_240x320"
  #define ST7789_DRIVER
  #define TFT_RGB_ORDER TFT_BGR
  #define TFT_WIDTH  240
  #define TFT_HEIGHT 320
  #define TFT_INVERSION_ON

#elif defined(USE_ST7796_480x320)

  #define USER_SETUP_INFO "ESP32S3_ST7796_480x320"
  #define ST7796_DRIVER
  #define TFT_RGB_ORDER TFT_BGR
  #define TFT_WIDTH  320
  #define TFT_HEIGHT 480
  // ST7796 typically does NOT need inversion
  // Uncomment if colors are inverted on your panel:
  // #define TFT_INVERSION_ON

#endif

// ##################################################################################
//
// Section 2. Define the pins that are used to interface with the display here
//
// ##################################################################################

// ESP32-S3 pin assignments
#define TFT_MOSI 11   // SDA
#define TFT_SCLK 12   // SCK
#define TFT_CS   10   // CS
#define TFT_DC    2   // DC
#define TFT_RST   4   // RES / RST

// Backlight connected directly to 3.3V, no GPIO control needed
#define TFT_BL   5
#define TFT_BACKLIGHT_ON HIGH

// ##################################################################################
//
// Section 3. Define the fonts that are to be used here
//
// ##################################################################################

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel font (digits + some chars only)
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font (digits only)
#define LOAD_FONT8  // Font 8. Large 75 pixel font (digits only)
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts

#define SMOOTH_FONT

// ##################################################################################
//
// Section 4. Other options
//
// ##################################################################################

// SPI frequency
#if defined(USE_ST7796_480x320)
  #define SPI_FREQUENCY  80000000   // ST7796 handles 80MHz well
#else
  #define SPI_FREQUENCY  40000000   // ST7789 safe at 40MHz
#endif
#define SPI_READ_FREQUENCY 20000000

// Use HSPI (SPI3) on ESP32-S3
#define USE_HSPI_PORT

// Enable DMA for non-blocking SPI transfers on ESP32-S3
#define USE_DMA_TO_TFT
