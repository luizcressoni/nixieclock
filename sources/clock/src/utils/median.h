/*! \file median.h */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class cMedianFilter {
    private:
        std::vector<uint8_t> buffer;
        size_t bufferSize;
    public:
        explicit cMedianFilter(size_t size = 3);
        uint8_t Update(uint8_t newValue);
    
    private:
        uint8_t CalculateMedian() const;
    };

//eof median.h