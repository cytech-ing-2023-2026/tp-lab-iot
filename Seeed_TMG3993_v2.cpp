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

#include "Seeed_TMG3993_v2.hpp"

TMG3993::TMG3993(TwoWire *i2c, uint8_t i2c_addr) {
    m_i2c = i2c;
    m_addr = i2c_addr;
}

bool TMG3993::initialize(void) {
    //wait 5.7ms
    delay(6);
    //check chip id
    if (getDeviceID() != 0x2a) {
        return false;
    }

    return true;
}

/**
    getDeviceID
*/
uint8_t TMG3993::getDeviceID(void) {
    uint8_t data;
    readRegs(REG_ID, &data, 1);
    return (data >> 2);
}

void TMG3993::enableEngines(uint8_t enable_bits) {
    bool pben = false;
    uint8_t data[2];
    data[0] = REG_ENABLE;
    if (enable_bits & ENABLE_PBEN) {
        enable_bits = ENABLE_PBEN;  //clear all other engine enable bits if set
        pben = true;
    }
    data[1] = ENABLE_PON | enable_bits;
    writeRegs(data, 2);

    if (!pben) {
        delay(7);    //for the prximity&guesture&color ring, we should wait 7ms for the chip to exit sleep mode
    }
}

void TMG3993::setADCIntegrationTime(uint8_t atime) {
    uint8_t data[2];
    data[0] = REG_ATIME;
    data[1] = atime;
    writeRegs(data, 2);
}

void TMG3993::setWaitTime(uint8_t wtime) {
    uint8_t data[2];
    data[0] = REG_WTIME;
    data[1] = wtime;
    writeRegs(data, 2);
}

void TMG3993::enableWaitTime12xFactor(bool enable) {
    uint8_t data[2];
    data[0] = REG_WTIME;
    if (enable) {
        data[1] = 0x62;
    } else {
        data[1] = 0x60;
    }
    writeRegs(data, 2);
}

uint8_t TMG3993::getInterruptPersistenceReg() {
    uint8_t data;
    readRegs(REG_IT_PERS, &data, 1);
    return data;
}

void TMG3993::setInterruptPersistenceReg(uint8_t pers) {
    uint8_t data[2];
    data[0] = REG_IT_PERS;
    data[1] = pers;
    writeRegs(data, 2);
}

uint8_t TMG3993::getControlReg() {
    uint8_t data;
    readRegs(REG_CONTROL, &data, 1);
    return data;
}

void TMG3993::setControlReg(uint8_t control) {
    uint8_t data[2];
    data[0] = REG_CONTROL;
    data[1] = control;
    writeRegs(data, 2);
}

uint8_t TMG3993::getCONFIG2() {
    uint8_t data;
    readRegs(REG_CONFIG2, &data, 1);
    return data;
}

void TMG3993::setCONFIG2(uint8_t config) {
    uint8_t data[2];
    data[0] = REG_CONFIG2;
    data[1] = config;
    writeRegs(data, 2);
}

uint8_t TMG3993::getCONFIG3() {
    uint8_t data;
    readRegs(REG_CONFIG3, &data, 1);
    return data;
}

void TMG3993::setCONFIG3(uint8_t config) {
    uint8_t data[2];
    data[0] = REG_CONFIG3;
    data[1] = config;
    writeRegs(data, 2);
}

uint8_t TMG3993::getSTATUS() {
    uint8_t data;
    readRegs(REG_STATUS, &data, 1);
    return data;
}

void TMG3993::clearPatternBurstInterrupts() {
    uint8_t data;
    readRegs(REG_PBCLEAR, &data, 1);
}
void TMG3993::forceAssertINTPin() {
    uint8_t data;
    readRegs(REG_IFORCE, &data, 1);
}
void TMG3993::clearProximityInterrupts() {
    uint8_t data;
    readRegs(REG_PICLEAR, &data, 1);
}
void TMG3993::clearALSInterrupts() {
    uint8_t data;
    readRegs(REG_CICLEAR, &data, 1);
}
void TMG3993::clearAllInterrupts() {
    uint8_t data;
    readRegs(REG_AICLEAR, &data, 1);
}


//proximity
void TMG3993::setupRecommendedConfigForProximity() {
    //setProximityInterruptThreshold();
    setProximityPulseCntLen(63, 1);
    uint8_t config2 = getCONFIG2();
    //config2 &= (3 << 4);
    config2 |= (3 << 4);  //300% power
    setCONFIG2(config2);

    // this will change the response speed of the detection
    // smaller value will response quickly, but may raise error detection rate
    // bigger value will response slowly, but will ensure the detection is real.
    setInterruptPersistenceReg(0xa << 4);  //10 consecutive proximity values out of range
}

void TMG3993::setProximityInterruptThreshold(uint8_t low, uint8_t high) {
    uint8_t data[2];
    data[0] = REG_PROX_IT_L;
    data[1] = low;
    writeRegs(data, 2);

    data[0] = REG_PROX_IT_H;
    data[1] = high;
    writeRegs(data, 2);
}

void TMG3993::setProximityPulseCntLen(uint8_t cnt, uint8_t len) {
    uint8_t data[2];
    data[0] = REG_PPULSE;
    if (cnt > 63) {
        cnt = 63;
    }
    if (len > 3) {
        len = 3;
    }
    data[1] = (cnt & 0x3f) | (len & 0xb0);
    writeRegs(data, 2);
}

uint8_t TMG3993::getProximityRaw() {
    uint8_t data;
    readRegs(REG_PROX_DATA, &data, 1);
    return data;
}

//-- ALS(light)
void TMG3993::setALSInterruptThreshold(uint16_t low, uint16_t high) {
    uint8_t data[5];
    data[0] = REG_ALS_IT;

    data[1] = low & 0xff;
    data[2] = low >> 8;
    data[3] = high & 0xff;
    data[4] = high >> 8;
    writeRegs(data, 5);
}

void TMG3993::getRGBCRaw(uint16_t* R, uint16_t* G, uint16_t* B, uint16_t* C) {
    uint8_t data[8];
    readRegs(REG_RGBC_DATA, data, 8);
    *C = (data[1] << 8) | data[0];
    *R = (data[3] << 8) | data[2];
    *G = (data[5] << 8) | data[4];
    *B = (data[7] << 8) | data[6];
}

int32_t TMG3993::getLux(uint16_t R, uint16_t G, uint16_t B, uint16_t C) {
    uint8_t data;
    readRegs(REG_ATIME, &data, 1);
    float ms = (256 - data) * 2.78;

    data = getControlReg();
    int gain;
    switch (data & 0x3) {
        case 0x0:
            gain = 1;
            break;
        case 0x1:
            gain = 4;
            break;
        case 0x2:
            gain = 16;
            break;
        case 0x3:
            gain = 64;
            break;
        default:
            gain = 1;
    }

    // Serial.println(ms);
    // Serial.println(gain);

    int32_t IR;
    float CPL, Y, L;

    IR = R + G + B;
    IR = (IR - C) / 2;
    if (IR < 0) {
        IR = 0;
    }
    Y = 0.362 * (R - IR) + 1 * (G - IR) + 0.136 * (B - IR);
    CPL = ms * gain / 412;
    L = Y / CPL;

    // Serial.println(IR);
    // Serial.println(CPL);
    // Serial.println(Y);

    return (int32_t)L;
}

int32_t TMG3993::getLux() {
    uint16_t R, G, B, C;
    getRGBCRaw(&R, &G, &B, &C);

    return getLux(R, G, B, C);
}

int32_t TMG3993::getCCT(uint16_t R, uint16_t G, uint16_t B, uint16_t C) {
    int32_t IR, lR, lG, lB, minV;
    IR = R + G + B;
    IR = (IR - C) / 2;
    if (IR < 0) {
        IR = 0;
    }

    lR = R; lG = G; lB = B;
    minV = min(lR, lB);
    if (IR < minV) {
        IR = minV - 0.1;
    }

    float rate = (float)(lB - IR) / (float)(lR - IR);

    return 2745 * (int32_t)rate + 2242;
}

int32_t TMG3993::getCCT() {
    uint16_t R, G, B, C;
    getRGBCRaw(&R, &G, &B, &C);

    return getCCT(R, G, B, C);
}







int TMG3993::readRegs(int addr, uint8_t* data, int len) {
    uint8_t result; // Return value

    m_i2c->beginTransmission(m_addr);
    m_i2c->write(addr);

    result = m_i2c->endTransmission();

    if (result != 0) {
        return result;
    }

    m_i2c->requestFrom((int)m_addr, len);   // Ask for `len` byte
    while (m_i2c->available() && len-- > 0) { // slave may send more than requested
        *data++ = m_i2c->read();
    }

    return 0;
}

int TMG3993::writeRegs(uint8_t* data, int len) {
    m_i2c->beginTransmission(m_addr);
    while (len-- > 0) {
        m_i2c->write(*data++);
    }
    m_i2c->endTransmission();
    return 0;
}

