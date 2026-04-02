/*
    A library for Grove - Light&Gesture&Color&Proximity Sensor

    Copyright (c) 2018 seeed technology co., ltd.
    Author      : Jack Shao
    Create Time: June 2018
    Change Log :

    The MIT License (MIT)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#ifndef __SEEED_TMG3993_V2_H__
#define __SEEED_TMG3993_V2_H__

#if (ARDUINO >= 100)
    #include "Arduino.h"
#else
    #include "WProgram.h"
#endif

#include <Wire.h>

#define TMG3993_DEFAULT_ADDRESS 0x39

//Register addresses
#define REG_RAM        0x00  //~0X7F
#define REG_ENABLE     0x80
#define REG_ATIME      0x81
#define REG_WTIME      0x83
#define REG_ALS_IT     0x84  //~0X87
#define REG_PROX_IT_L  0x89
#define REG_PROX_IT_H  0x8B
#define REG_IT_PERS    0x8C
#define REG_CONFIG1    0x8D
#define REG_PPULSE     0x8E
#define REG_CONTROL    0x8F
#define REG_CONFIG2    0x90
#define REG_REVID      0x91
#define REG_ID         0x92
#define REG_STATUS     0x93
#define REG_RGBC_DATA  0x94  //~9B
#define REG_PROX_DATA  0x9C
#define REG_POFFSET_NE 0x9D
#define REG_POFFSET_SW 0x9E
#define REG_CONFIG3    0x9F
//The following CONFIG registers have different definations under different modes
#define REG_CONFIG_A0  0xA0
#define REG_CONFIG_A1  0xA1
#define REG_CONFIG_A2  0xA2
#define REG_CONFIG_A3  0xA3
#define REG_CONFIG_A4  0xA4
#define REG_CONFIG_A5  0xA5
#define REG_CONFIG_A6  0xA6
#define REG_CONFIG_A7  0xA7
#define REG_CONFIG_A8  0xA8
#define REG_CONFIG_A9  0xA9
#define REG_CONFIG_AA  0xAA
#define REG_CONFIG_AB  0xAB
#define REG_CONFIG_AC  0xAC
#define REG_CONFIG_AE  0xAE
#define REG_CONFIG_AF  0xAF

#define REG_PBCLEAR    0xE3
#define REG_IFORCE     0xE4
#define REG_PICLEAR    0xE5
#define REG_CICLEAR    0xE6
#define REG_AICLEAR    0xE7
#define REG_GFIFO_N    0xFC
#define REG_GFIFO_S    0xFD
#define REG_GFIFO_W    0xFE
#define REG_GFIFO_E    0xFF


//Enable bits
#define ENABLE_PON   0x01
#define ENABLE_AEN   0x02
#define ENABLE_PEN   0x04
#define ENABLE_WEN   0x08
#define ENABLE_AIEN  0x10
#define ENABLE_PIEN  0x20
#define ENABLE_GEN   0x40
#define ENABLE_PBEN  0x80

//Status bits
#define STATUS_AVALID    (1)
#define STATUS_PVALID    (1<<1)
#define STATUS_GINT      (1<<2)
#define STATUS_PBINT     (1<<3)
#define STATUS_AINT      (1<<4)
#define STATUS_PINT      (1<<5)
#define STATUS_PGSAT     (1<<6)
#define STATUS_CPSAT     (1<<7)



class TMG3993 {
  public:
    TMG3993(TwoWire *i2c, uint8_t i2c_addr = TMG3993_DEFAULT_ADDRESS);

    bool initialize(void) ;

    //common
    uint8_t getDeviceID(void) ;
    void enableEngines(uint8_t enable_bits);
    void setADCIntegrationTime(uint8_t atime);
    void setWaitTime(uint8_t wtime);
    void enableWaitTime12xFactor(bool enable);  //CONFIG1 register
    uint8_t getInterruptPersistenceReg();
    void setInterruptPersistenceReg(uint8_t pers);
    uint8_t getControlReg();
    void setControlReg(uint8_t control);
    uint8_t getCONFIG2();
    void setCONFIG2(uint8_t config);
    uint8_t getCONFIG3();
    void setCONFIG3(uint8_t config);
    uint8_t getSTATUS();
    void clearPatternBurstInterrupts();
    void forceAssertINTPin();
    void clearProximityInterrupts();
    void clearALSInterrupts();
    void clearAllInterrupts();

    //proximity
    void setupRecommendedConfigForProximity();
    void setProximityInterruptThreshold(uint8_t low, uint8_t high);
    void setProximityPulseCntLen(uint8_t cnt, uint8_t len);
    uint8_t getProximityRaw();
    // if you have glasses on the sensor, then you should adjust the offset
    // void setProximityOffsetNE(uint8_t offset);
    // void setProximityOffsetSW(uint8_t offset);

    //Color and ALS (Light)
    void setALSInterruptThreshold(uint16_t low, uint16_t high);
    void getRGBCRaw(uint16_t* R, uint16_t* G, uint16_t* B, uint16_t* C);
    int32_t getLux(uint16_t R, uint16_t G, uint16_t B, uint16_t C);
    int32_t getLux();
    //Correlated Color Temperature (CCT)
    int32_t getCCT(uint16_t R, uint16_t G, uint16_t B, uint16_t C);
    int32_t getCCT();

    //Gesture
    //Gesture needs algorithm, which is pending on AMS's delievery.
    //Will update this library as soon as we get the alglrithm from AMS.



  private:
    TwoWire* m_i2c;
    uint8_t m_addr;
    int readRegs(int addr, uint8_t* data, int len);
    int writeRegs(uint8_t* data, int len);
};

#endif
