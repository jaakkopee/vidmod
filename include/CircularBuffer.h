#pragma once

#include <vector>
#include <cstddef>

template <typename T>
class CircularBuffer {
protected:
    std::vector<T> data;
    size_t index;

public:
    CircularBuffer(const std::vector<T>& buffer) : data(buffer), index(0) {}
    
    const T& getNext() {
        const T& element = data[index];
        index = (index + 1) % data.size();
        return element;
    }
    
    void setIndex(size_t newIndex) {
        index = newIndex % data.size();
    }
    
    size_t getIndex() const {
        return index;
    }
    
    std::vector<T> getBuffer(size_t length) {
        std::vector<T> buffer;
        buffer.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            buffer.push_back(getNext());
        }
        return buffer;
    }
    
    size_t size() const {
        return data.size();
    }
    
    const std::vector<T>& getData() const {
        return data;
    }
};
