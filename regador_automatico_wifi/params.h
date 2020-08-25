// Network
#define FIREBASE_HOST "firebase database url"
#define FIREBASE_AUTH "firebase secret"
#define WIFI_SSID "wifi name"
#define WIFI_PASSWORD "wifi password"
#define TIME_ZONE -3

// Timing
#define CHECK_TIME_SECONDS 1800  // 30'
#define WAIT_DELAY_SECONDS 10800  // 3hs
#define WATER_SECONDS 10

// Other values
#define HUMIDITY_THRESHOLD 290  // Value from 1 (wet) to 1024 (dry)
#define STREAM_DATA_REQUEST "request fresh data"
#define STREAM_WATER_NOW_REQUEST "plants/plantita1/water now"

// Connections (GPIO pins are different to the printed ones on the board)
#define WATER_PUMP_PIN 2  // D4
#define COMMON_ANALOG_INPUT A0
#define S0 5   // D1
#define S1 4   // D2
#define S2 0   // D3
#define S3 2   // D4
// Sensors connected to the CD74HC4067 IC
#define HUMIDITY_SENSOR 0
#define TEMP_SENSOR 1
#define LIGHT_SENSOR 2
#define WATER_LEVEL_SENSOR 3
