#include "ParameterAutomation.h"
#include <algorithm>

void ParameterAutomation::addKeyframe(int frame, float normalizedValue) {
    keyframes[frame] = normalizedValue;
}

void ParameterAutomation::removeKeyframe(int frame) {
    keyframes.erase(frame);
}

float ParameterAutomation::getValueAtFrame(int frame) const {
    if (keyframes.empty()) {
        return 0.5f; // Middle of normalized range
    }
    
    // If exact keyframe exists, return it
    auto it = keyframes.find(frame);
    if (it != keyframes.end()) {
        return it->second;
    }
    
    // Find surrounding keyframes for interpolation
    auto upper = keyframes.upper_bound(frame);
    
    // If no keyframe after this frame, use the last keyframe value
    if (upper == keyframes.end()) {
        return keyframes.rbegin()->second;
    }
    
    // If no keyframe before this frame, use the first keyframe value
    if (upper == keyframes.begin()) {
        return upper->second;
    }
    
    // Interpolate between two keyframes
    auto lower = std::prev(upper);
    
    int frame1 = lower->first;
    float value1 = lower->second;
    int frame2 = upper->first;
    float value2 = upper->second;
    
    // Linear interpolation
    float t = static_cast<float>(frame - frame1) / static_cast<float>(frame2 - frame1);
    return value1 + t * (value2 - value1);
}

float ParameterAutomation::getActualValueAtFrame(int frame) const {
    float normalized = getValueAtFrame(frame);
    // Scale from [0,1] to [minValue, maxValue]
    return minValue + normalized * (maxValue - minValue);
}

std::vector<Keyframe> ParameterAutomation::getAllKeyframes() const {
    std::vector<Keyframe> result;
    result.reserve(keyframes.size());
    
    for (const auto& kf : keyframes) {
        result.push_back({kf.first, kf.second});
    }
    
    return result;
}
