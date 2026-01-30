#pragma once

#include <map>
#include <vector>

struct Keyframe {
    int frame;
    float value;
};

class ParameterAutomation {
private:
    std::map<int, float> keyframes; // frame -> normalized value (0-1)
    float minValue;
    float maxValue;

public:
    ParameterAutomation(float min = 0.0f, float max = 1.0f) 
        : minValue(min), maxValue(max) {}
    
    void addKeyframe(int frame, float normalizedValue);
    void removeKeyframe(int frame);
    float getValueAtFrame(int frame) const; // Returns normalized value
    float getActualValueAtFrame(int frame) const; // Returns scaled to actual range
    bool hasKeyframes() const { return !keyframes.empty(); }
    std::vector<Keyframe> getAllKeyframes() const;
    void clear() { keyframes.clear(); }
    
    void setRange(float min, float max) { minValue = min; maxValue = max; }
    float getMinValue() const { return minValue; }
    float getMaxValue() const { return maxValue; }
};
