#include <SPI.h>          // f.k. for Arduino-1.5.2
#include "Adafruit_GFX.h"// Hardware-specific library
#include <MCUFRIEND_kbv.h>
#include <math.h>
#include <TimeLib.h>
#include <SDS011.h>

#define AQI_GREEN 0x04CC
#define AQI_YELLOW 0xFEE6
#define AQI_ORANGE 0xFCC6
#define AQI_RED 0xC806
#define AQI_PURPLE 0x6013
#define AQI_DARKRED 0x7804

MCUFRIEND_kbv tft;
SDS011 sds011;

//int tftWidth = tft.width();
//int tft.height() = tft.height();
int dataArray[321]; // My display is 240 pixels wide

int sds011Rx = 12;
int sds011Tx = 11;
int sds011Error;

int fanPin = 10;    // LED connected to digital pin 9
int fanMinValue = 125;
int fanMaxValue = 225;
int fanValue = 0;

bool haveData = false;
int sleepTime = 5000; // Sleep 15 seconds between measurements

float pm25CurrentValue, pm25Average1MinValue, pm25Average1MinSum;
float pm10CurrentValue, pm10Average1MinValue, pm10Average1MinSum;

int average1MinDivider, average15MinDivider;

time_t average1MinStart, average15MinStart;

float averagePm10DustDensity = 0;
float averagePm25DustDensity = 0;
float finalPm10DustDensity = 0;
float finalPm25DustDensity = 0;
int divider = 0;

int averageOverAQIValues = 15;
int averagePm10AQI = 0;
int averagePm10AQISum = 0;
int averagePm25AQI = 0;
int averagePm25AQISum = 0;
int averageAQIDivider = 0;

void setup() {
    Serial.begin(9600);
    //uint16_t identifier = tft.readID();
    //Serial.println(identifier, HEX);
    tft.begin(tft.readID());
    tft.setRotation(3);

    // Whiteout whole screen
    tft.fillScreen(TFT_BLACK);
  
    // Blackout graph part of screen
    //tft.fillRect(0, (tft.height() / 3), tft.width(), tft.height(), TFT_BLACK);
    
    //tft.setTextSize(4);
  
    // Zerofill the array
    for (int i = 0; i <= (tft.width() - 1); i++) {
        dataArray[i] = 0;
    }
    
    sds011.begin(sds011Rx, sds011Tx);
    
    
    Serial.println("Starting SDS011...");
    
    average1MinStart = now();
}

float AQI(float I_high, float I_low, float C_high, float C_low, float C) {
    return (I_high - I_low) * (C - C_low) / (C_high - C_low) + I_low;
}

uint16_t aqiColour(int aqi) {
    if (aqi <= 50) {
        return AQI_GREEN;
    }
    if (aqi <= 100) {
        return AQI_YELLOW;
    }
    if (aqi <= 150) {
        return AQI_ORANGE;
    }
    if (aqi <= 200) {
        return AQI_RED;
    }
    if (aqi <= 300) {
        return AQI_PURPLE;
    }
    
    return AQI_DARKRED;
}

void drawAqiLine(int x, int aqi) {
    if (aqi > 400) {
        aqi = 400;
    }
    
    int yOffset = 0;
    
    // Draw green
    int drawHeight = map(aqi, 0, 400, 0, ((tft.height() / 3) * 2));
    int y = ((tft.height() - drawHeight) - yOffset);
    tft.drawFastVLine(x, y, drawHeight, AQI_GREEN);

    if (aqi > 50) {
      yOffset = map(50, 0, 400, 0, ((tft.height() / 3) * 2));
      int drawHeight = map((aqi - 50), 0, 400, 0, ((tft.height() / 3) * 2));
      int y = ((tft.height() - drawHeight) - yOffset);
      
      tft.drawFastVLine(x, y, drawHeight, AQI_YELLOW);
    }
    if (aqi > 100) {
      yOffset = map(100, 0, 400, 0, ((tft.height() / 3) * 2));
      int drawHeight = map((aqi - 100), 0, 400, 0, ((tft.height() / 3) * 2));
      int y = ((tft.height() - drawHeight) - yOffset);
      
      tft.drawFastVLine(x, y, drawHeight, AQI_ORANGE);
    }
    if (aqi > 150) {
      yOffset = map(150, 0, 400, 0, ((tft.height() / 3) * 2));
      int drawHeight = map((aqi - 150), 0, 400, 0, ((tft.height() / 3) * 2));
      int y = ((tft.height() - drawHeight) - yOffset);
      
      tft.drawFastVLine(x, y, drawHeight, AQI_RED);
    }
    if (aqi > 200) {
      yOffset = map(200, 0, 400, 0, ((tft.height() / 3) * 2));
      int drawHeight = map((aqi - 200), 0, 400, 0, ((tft.height() / 3) * 2));
      int y = ((tft.height() - drawHeight) - yOffset);
      
      tft.drawFastVLine(x, y, drawHeight, AQI_PURPLE);
    }
    if (aqi > 300) {
      yOffset = map(300, 0, 400, 0, ((tft.height() / 3) * 2));
      int drawHeight = map((aqi - 300), 0, 400, 0, ((tft.height() / 3) * 2));
      int y = ((tft.height() - drawHeight) - yOffset);
      
      tft.drawFastVLine(x, y, drawHeight, AQI_DARKRED);
    }
}

float getAverageDustDensity() {
    sds011.wakeup();
    sds011Error = sds011.read(&pm25CurrentValue,&pm10CurrentValue);
    if (!sds011Error) {
        pm25Average1MinSum = (pm25Average1MinSum + pm25CurrentValue);
       
        pm10Average1MinSum = (pm10Average1MinSum + pm10CurrentValue);

        Serial.println("PM2.5: " + String(pm25DustDensityToAQI(pm25CurrentValue)) + " (" + String(pm25CurrentValue) + ")");
        Serial.println("PM10:  " + String(pm10DustDensityToAQI(pm10CurrentValue)) + " (" + String(pm10CurrentValue) + ")");
        
        average1MinDivider++;

        // If there is no data, print the first values
        if (haveData == false) {
            // Set the average to current value to show initial values on LCD
            pm25Average1MinValue = pm25CurrentValue;
            pm10Average1MinValue = pm10CurrentValue;
            updateDisplay();
            setFanspeed(pm25DustDensityToAQI(pm25Average1MinValue));

            haveData = true;
        }
    }

    // Calculate and show 1 minute averages;
    if (now() >= (average1MinStart + 60)) {
        pm25Average1MinValue = (pm25Average1MinSum / average1MinDivider);
        pm10Average1MinValue = (pm10Average1MinSum / average1MinDivider);

        Serial.println("Average 1 min PM2.5: " + String(pm25DustDensityToAQI(pm25Average1MinValue)) + " (" + String(pm25Average1MinValue) + ")");
        Serial.println("Average 1 min PM10:  " + String(pm10DustDensityToAQI(pm10Average1MinValue)) + " (" + String(pm10Average1MinValue) + ")");

        updateDisplay();
        setFanspeed(pm25DustDensityToAQI(pm25Average1MinValue));
        
        average1MinStart = now();
        pm25Average1MinSum = 0;
        pm10Average1MinSum = 0;
        average1MinDivider = 0;
    }
}

float pm10DustDensityToAQI(float density) {
    int d10 = (int) (density * 10);

    if (d10 <= 0) {return 0;}
    else if (d10 <= 540) {return AQI(50, 0, 540, 0, d10);}
    else if (d10 <= 1540) {return AQI(100, 51, 1540, 550, d10);}
    else if (d10 <= 2540) {return AQI(150, 101, 2540, 1550, d10);}
    else if (d10 <= 3540) {return AQI(200, 151, 3540, 2550, d10);}
    else if (d10 <= 4240) {return AQI(300, 201, 4240, 3550, d10);}
    else if (d10 <= 5040) {return AQI(400, 301, 5040, 4250, d10);}
    else if (d10 <= 6040) {return AQI(500, 401, 6040, 5050, d10);}
    else if (d10 <= 10000) {return AQI(1000, 501, 10000, 6050, d10);}
    else {return 1001;}
}

float pm25DustDensityToAQI(float density) {
    int d10 = (int) (density * 10);

    if (d10 <= 0) {return 0;}
    else if (d10 <= 120) {return AQI(50, 0, 120, 0, d10);}
    else if (d10 <= 354) {return AQI(100, 51, 354, 121, d10);}
    else if (d10 <= 554) {return AQI(150, 101, 554, 355, d10);}
    else if (d10 <= 1504) {return AQI(200, 151, 1504, 555, d10);}
    else if (d10 <= 2504) {return AQI(300, 201, 2504, 1505, d10);}
    else if (d10 <= 3504) {return AQI(400, 301, 3504, 2505, d10);}
    else if (d10 <= 5004) {return AQI(500, 401, 5004, 3505, d10);}
    else if (d10 <= 10000) {return AQI(1000, 501, 10000, 5005, d10);}
    else {return 1001;}
}

void printPm10Aqi(int aqi) {
    tft.fillRect((tft.width() / 2), 0, tft.width(), (tft.height() / 3), aqiColour(aqi));
    tft.drawFastVLine(((tft.width() / 2) + 1), 0, (tft.height() / 3), TFT_BLACK);
    tft.setTextColor(TFT_BLACK, aqiColour(aqi));
    
    tft.setTextSize(2);
    tft.setCursor(((tft.width() / 2) + 10), 10);
    tft.println("PM10");
    
    tft.setTextSize(4);
    tft.setCursor(((tft.width() / 2) + 10), 40);
    tft.println(aqi);
}

void printPm25Aqi(int aqi) {
    tft.fillRect(0, 0, (tft.width() / 2), (tft.height() / 3), aqiColour(aqi));
    tft.drawFastVLine((tft.width() / 2), 0, (tft.height() / 3), TFT_BLACK);
    tft.setTextColor(TFT_BLACK, aqiColour(aqi));

    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("PM2.5");
    
    tft.setTextSize(4);
    tft.setCursor(10, 40);
    tft.println(aqi);
}

void setFanspeed(int aqi) {
  if (aqi < 50) {
    fanValue = 0;
  }
  else if (aqi < 100) {
    fanValue = 125;
  }
  else if(aqi < 150) {
    fanValue = 150;
  }
  else if(aqi < 200) {
    fanValue = 175;
  }
  else {
    fanValue = fanMaxValue;
  }

  analogWrite(fanPin, fanValue);
  Serial.println("Set fanspeed to " + String(fanValue));
}

void updateDisplay() {
    printPm10Aqi((int)round(pm10DustDensityToAQI(pm10Average1MinValue)));
    printPm25Aqi((int)round(pm25DustDensityToAQI(pm25Average1MinValue)));
  
    // Store this current value at the 1st element of the array
    dataArray[0] = (int)round(pm25DustDensityToAQI(pm25Average1MinValue));

    // Clear graph part of screen
    tft.fillRect(0, (tft.height() / 3), tft.width(), tft.height(), TFT_BLACK);
    
    // Draw lines
    for (int i = 1; i <= tft.width(); i++ ) {
        if (dataArray[(i - 1)] > 0) {
            drawAqiLine((tft.width() - i), dataArray[(i - 1)]);
        }
    }
 
    // Within the array, shift elements to the right by one step, by copying the adjacent 
    // values found on the left of each element. Start the copying process 
    // from the right(last) element of the array Leave the first element intact
    for (int i = tft.width(); i >= 2; i--) {
        dataArray[(i - 1)] = dataArray[(i - 2)];
    }
}

void loop() {
    getAverageDustDensity();
    //sds011.sleep();
    delay(sleepTime); 
}
