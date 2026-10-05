/*! \file median.cpp */
#include "median.h"

#include <algorithm>


cMedianFilter::cMedianFilter(size_t size) : bufferSize(size) {
    buffer.reserve(bufferSize);
}
    

uint8_t cMedianFilter::Update(uint8_t newValue) {
    buffer.push_back(newValue);
    if (buffer.size() > bufferSize) {
        buffer.erase(buffer.begin());
    }

    if (buffer.size() == bufferSize) {
        return CalculateMedian();
    }
    return newValue;
}


uint8_t cMedianFilter::CalculateMedian() const {
    std::vector<uint8_t> tempBuffer = buffer;
    size_t n = tempBuffer.size() / 2;
    std::nth_element(tempBuffer.begin(), tempBuffer.begin() + n, tempBuffer.end());
    return tempBuffer[n];
}


//eof median.cpp