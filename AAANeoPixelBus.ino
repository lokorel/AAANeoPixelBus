#include <NeoPixelBus.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////          CONFIG SECTION STARTS               /////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool skipFirstLed = true; // if set the first led in the strip will be set to black (for level shifters using sacrifice LED)
int serialSpeed = 921600; // serial port speed

////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////            CONFIG SECTION ENDS               /////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int pixelCount = 0; // This is dynamic, don't change it

#define LED_TYPE NeoGrbFeature

NeoPixelBus<LED_TYPE, NeoEsp8266Uart1800KbpsMethod> *strip = NULL;

void Init(int count)
{
  if (strip != NULL)
    delete strip;

  pixelCount = count;
  strip = new NeoPixelBus<LED_TYPE, NeoEsp8266Uart1800KbpsMethod>(pixelCount);
  strip->Begin();
}

enum class AwaProtocol
{
  HEADER_A,
  HEADER_d,
  HEADER_a,
  HEADER_HI,
  HEADER_LO,
  HEADER_CRC,
  RED,
  GREEN,
  BLUE
};

// static data buffer for the loop
#define MAX_BUFFER 2048
uint8_t buffer[MAX_BUFFER];
AwaProtocol state = AwaProtocol::HEADER_A;
uint8_t incoming_gain = 0;
uint8_t incoming_red = 0;
uint8_t incoming_green = 0;
uint8_t incoming_blue = 0;
uint8_t CRC = 0;
uint16_t count = 0;
uint16_t currentPixel = 0;

RgbColor inputColor;

// stats
unsigned long stat_start = 0;
bool wantShow = false;

inline void ShowMe()
{
  if (wantShow == true && strip != NULL && strip->CanShow())
  {
    wantShow = false;
    strip->Show();
  }
}

void readSerialData()
{
  unsigned long curTime = millis();
  uint16_t bufferPointer = 0;
  uint16_t internalIndex = min(Serial.available(), MAX_BUFFER);

  if (internalIndex > 0)
    internalIndex = Serial.readBytes(buffer, internalIndex);

  // magic word
  if (curTime - stat_start > 5000)
  {
    stat_start = curTime;
    strip->ClearTo(RgbColor(0, 0, 0));
    strip->Show();
    Serial.print("Ada\n");
  }

  if (state == AwaProtocol::HEADER_A)
    ShowMe();

  while (bufferPointer < internalIndex)
  {
    byte input = buffer[bufferPointer++];
    switch (state)
    {
    case AwaProtocol::HEADER_A:
      if (input == 'A')
        state = AwaProtocol::HEADER_d;
      break;

    case AwaProtocol::HEADER_d:
      if (input == 'd')
        state = AwaProtocol::HEADER_a;
      else
        state = AwaProtocol::HEADER_A;
      break;

    case AwaProtocol::HEADER_a:
      if (input == 'a')
        state = AwaProtocol::HEADER_HI;
      else
        state = AwaProtocol::HEADER_A;
      break;

    case AwaProtocol::HEADER_HI:
      currentPixel = 0;
      count = input * 0x100;
      CRC = input;
      state = AwaProtocol::HEADER_LO;
      break;

    case AwaProtocol::HEADER_LO:
      count += input;
      CRC = CRC ^ input ^ 0x55;
      state = AwaProtocol::HEADER_CRC;
      break;

    case AwaProtocol::HEADER_CRC:
      if (CRC == input)
      {
        if (count + 1 != pixelCount)
          Init(count + 1);
        
        state = AwaProtocol::RED;
      }
      else
        state = AwaProtocol::HEADER_A;
      break;

    case AwaProtocol::RED:
      inputColor.R = input;
      state = AwaProtocol::GREEN;
      break;

    case AwaProtocol::GREEN:
      inputColor.G = input;
      state = AwaProtocol::BLUE;
      break;

    case AwaProtocol::BLUE:
      inputColor.B = input;

      if (currentPixel == 0 && skipFirstLed)
      {
        strip->SetPixelColor(currentPixel++, RgbColor(0, 0, 0));
      }
      else
        setStripPixel(currentPixel++, inputColor);

      if (count-- > 0)
        state = AwaProtocol::RED;
      else
      {
        wantShow = true;
        stat_start = curTime;
        ShowMe();
        state = AwaProtocol::HEADER_A;
      }
      break;
      
    }
  }
}

inline void setStripPixel(uint16_t pix, RgbColor &inputColor)
{
  if (pix < pixelCount)
  {
    strip->SetPixelColor(pix, inputColor);
  }
}

void setup()
{
  // Init serial port
  Serial.begin(serialSpeed);
  Serial.setTimeout(50);
  Serial.setRxBufferSize(2048);

  // Display config
  Serial.write("\r\nWelcome!\r\nAda driver\r\n");

  // first LED info
  if (skipFirstLed)
    Serial.write("First LED: disabled\r\n");
  else
    Serial.write("First LED: enabled\r\n");

  // RGBW claibration info
  Serial.write("Color mode: RGB\r\n");
}


void loop()
{
  readSerialData();
}
