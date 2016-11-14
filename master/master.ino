//master.ino
//Robbie Culkin
//for the project found at https://robbieculkin.wordpress.com/2016/08/02/rgb-led-audio-visualizer/

#include <Adafruit_NeoPixel.h> //https://github.com/adafruit/Adafruit_NeoPixel
//Dan bAkrhorn
// PINS & INFO
#define STROBE           4
#define RESET            6
#define DC_ONE          A1
#define DC_TWO          A2
#define STRIP_PIN        3
#define N_STRIP_LEDS   180
#define TOWER_PIN        9
#define N_TOWER_LEDS   180
#define LEVELS          16

//SETTINGS         
#define BRIGHTNESS  150       //(0 to 255)
#define RISE_RATE   0.25      //(0 to 1) higher values mean livelier display
#define FALL_RATE   0.10      //(0 to 1) higher values mean livelier display
#define CONTRAST   1.3        //(undefined range)
#define MAX_VOL    600        //(0 to 1023) maximum value reading expected from the shield
#define LIVELINESS  2         //(undefined range)
#define LIVELINESS2 1.7
#define MULTIPLIER 90         //(undefined range) higher values mean fewer LEDs lit @ a given volume

//NeoPixel intitialization
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(N_STRIP_LEDS, STRIP_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel tower  = Adafruit_NeoPixel(N_TOWER_LEDS, TOWER_PIN, NEO_GRB + NEO_KHZ800);

// READ
    void recordFrequencies(int (&frequencies) [7]);
    // records frequencies into the frequencies array
    double getAvg(const int (&frequencies) [7]);
    // returns avg of frequencies
    
//FILTER
    double smoothVol(double newVol);
    // brings vol values closer together, resulting a smoother output (avoid strobing)
    double autoMap(const double vol);
    // scales volume values in order to get consisitent output at different overall volumes

//DISPLAY
    void shiftColors();
    // makes room for a new color value
    uint32_t GetColor(byte pos, double vol);
    // finds the new color value based on color rotation position and volume
    void displayStrip(const double vol, Adafruit_NeoPixel &strip);
    // detemines what LEDs are on/off; sends RGB values to the strip
    void disiplayTower(const double vol, Adafruit_NeoPixel &strip);
    // detemines what LEDs are on/off; sends RGB values to the strip

//HELPER FUNCTIONS
    int findLED(int level, int place);
    //returns the index of any LED on the tower given its level and position on the level


//GLOBALS (globals are normally a bad idea, but the nature of Arduino's loop() 
//         function makes them necessary)
double vols[100];
uint32_t color [100];
uint32_t colorCursor = 0;
int numLoops = 0;


void setup() {
  
  //Spectrum Shield pin configurations
  pinMode(STROBE, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(DC_ONE, INPUT);
  pinMode(DC_TWO, INPUT);  
  digitalWrite(STROBE, HIGH);
  digitalWrite(RESET, HIGH);

//Initialize Spectrum Analyzers
  digitalWrite(STROBE, LOW);
  delay(1);
  digitalWrite(RESET, HIGH);
  delay(1);
  digitalWrite(STROBE, HIGH);
  delay(1);
  digitalWrite(STROBE, LOW);
  delay(1);
  digitalWrite(RESET, LOW);

  strip1.begin();
  strip1.setBrightness(BRIGHTNESS);
  strip1.show();
  tower.begin();
  tower.setBrightness(BRIGHTNESS);
  tower.show();

}

void loop() {
  int frequencies[7];
  double volume;
  static uint16_t pos = 0;
  
  //
  // READ
  //
  recordFrequencies(frequencies);
  volume = getAvg(frequencies);

  //
  // FILTER
  //
  volume = smoothVol(volume);
  volume = autoMap(volume);

  //
  // ASSIGN COLOR/BRIGHTNESS
  //
  shiftColors();
  color[colorCursor] = GetColor(pos & 255, volume);

  //rotate the color assignment wheel
  int rotation = map (volume, 0, strip1.numPixels()/2.0, 0, 25);
  numLoops++;
  if(numLoops%5 == 0) //default 5
    pos+=rotation;

  //
  // DISPLAY
  //  
  displayStrip(volume, strip1);
  disiplayTower(volume, tower);
  
}

void recordFrequencies(int (&frequencies) [7])
{
    int band;
    for (band = 0; band<7; band++)
    {
        frequencies[band] = ((analogRead(DC_ONE) + analogRead(DC_TWO))/2);
        
        if (frequencies[band] < 70) frequencies[band] = 0;
        
        //account for noise
        digitalWrite(STROBE, HIGH);
        digitalWrite(STROBE, LOW);
    }
}

double getAvg(const int (&frequencies)[7])
{
  int total = 0;
  
  for(int k=0; k < 7; k++)
  {
    total+= frequencies[k];
  }
  
  return ((double)total/7);
}

double smoothVol(double newVol)
{
  double oldVol = vols[0];
  
  if (oldVol < newVol)
  {
      newVol = (newVol * RISE_RATE) + (oldVol * (1-RISE_RATE));
      // limit how quickly volume can rise from the last value
      
      return newVol;
  }
      
  else
  {
      newVol = (newVol * FALL_RATE) + (oldVol * (1-FALL_RATE));
      // limit how quickly volume can fall from the last value
      
      return newVol;
  }
      
}

double autoMap(const double vol)
{
  double total = 0;
  static double avg;
  static int prevMillis = 0;

  // this if statement acts like a delay, but without putting the entire sketch on hold
  if(millis() - prevMillis > 100)
  {
    for (int i = 100; i > 0; i--) 
    {
      total += vols[i-1];
      vols[i] = vols[i-1]; 
    }
    vols[0] = vol;
    
    avg = total/100.0;
    
    prevMillis = millis();
  }
  
  if (avg < 5) return 0;
  return map (vol, 0, 3*avg+1, 0, strip1.numPixels()/1.0);
}

void shiftColors()
{
  for (int i = 99; i >=1; i--) 
  {
     color[i] = color[i-1];
  }
}

uint32_t GetColor(byte pos, double vol) //returns color & brightness
{
  pos = 255 - pos;

  // some last-minute transforms I dont want to stick in the main fcn
  vol = pow(vol, CONTRAST);
  vol = map(vol, 0, MAX_VOL, 0, 255);

if(vol >255) vol = 255;
  
  if(pos < 85) {
    int red = 255 - pos * 3;
    int grn = 0;
    int blu = pos * 3;
    return strip1.Color(vol*red/255, vol*grn/255, vol*blu/255);
  }
  if(pos < 170) {
    pos -= 85;
    int red = 0;
    int grn = pos * 3;
    int blu = 255 - pos * 3;
    return strip1.Color(vol*red/255, vol*grn/255, vol*blu/255);
  }
  pos -= 170;
  int red = pos * 3;
  int grn = 255 - pos * 3;
  int blu = 0;
  return strip1.Color(vol*red/255, vol*grn/255, vol*blu/255);
}

void displayStrip(const double vol, Adafruit_NeoPixel &strip)
{
  uint32_t off = strip1.Color(0, 0, 0);

  // some last-minute transforms I dont want to stick in the main fcn
  double newVol = 0.5*vol;
  newVol = pow(newVol, LIVELINESS);
  
  int i;
  for (i = 0; i < strip.numPixels(); i++)
  {
    // some magic numbers here that made the output better. should eventually get rid of them
    double threshold = ((double)(i+1) * MULTIPLIER - (3*newVol)+5);
     
    if (newVol > threshold)
    {
      strip.setPixelColor(strip.numPixels()/2-1-i, color[i]);
    }
    else
    {
      strip.setPixelColor(strip.numPixels()/2-1-i, off);
    }
    
    if (newVol > threshold)
    {
      strip.setPixelColor(strip.numPixels()/2-1+i, color[i]);
    }
    else
    {
      strip.setPixelColor(strip.numPixels()/2-1+i, off);
    }
  }
  strip.show();
}

void disiplayTower(const double vol, Adafruit_NeoPixel &strip)
{
  uint32_t off = strip1.Color(0, 0, 0);

  double myVol = 0.4*vol;
  myVol = pow(myVol, LIVELINESS2);
  
  int i, j;
  for (i=0; i < LEVELS; i+=2)
  {
    double threshold = (((double)(i+1)*MULTIPLIER)-3*myVol+5)*14;
    if(myVol > threshold)
    {
      for (j=0; j < 5; j++)
      {
        strip.setPixelColor(findLED(i, j), color[2*j+i]);
      }
    }
    else
    {
      for (j=0; j < 5; j++)
      {
        strip.setPixelColor(findLED(i, j), off);
      }
    }
  }
  for (i=1; i < LEVELS; i+=2)
  {
    double threshold = (((double)(i+1)*MULTIPLIER)-3*myVol+5)*14;
    if(vol > threshold)
    {
      for (j=0; j < 10; j++)
      {
        strip.setPixelColor(findLED(i, j), color[j+i]);
      }
    }
    else
    {
      for (j=0; j < 10; j++)
      {
        strip.setPixelColor(findLED(i, j), off);
      }
    }
  }
  strip.show();
}

int findLED(int level, int place)
{
  int i, total = 0;
  for(i=0; i < level; i++)
  {
    if(i%2 == 0)
    {
      total += 5;
    }
    else
    {
      total += 10;
    }
  }
  if (level%2 == 0 && place > 4)
  {
    place = 4;
  }
  total += place;

  return total;
}
