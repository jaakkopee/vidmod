#pragma once

#include <opencv2/opencv.hpp>
#include "AudioBuffer.h"
#include <memory>
#include <string>
#include <map>

// Base effect class
class Effect {
protected:
    std::string name;
    std::map<std::string, float> parameters;
    bool bypassed;

public:
    Effect(const std::string& effectName) : name(effectName), bypassed(false) {}
    virtual ~Effect() = default;
    
    virtual cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) = 0;
    
    std::string getName() const { return name; }
    
    void setBypass(bool bypass) { bypassed = bypass; }
    bool isBypassed() const { return bypassed; }
    
    void setParameter(const std::string& key, float value) {
        parameters[key] = value;
    }
    
    float getParameter(const std::string& key, float defaultValue = 0.0f) const {
        auto it = parameters.find(key);
        if (it != parameters.end()) {
            return it->second;
        }
        return defaultValue;
    }
    
    const std::map<std::string, float>& getParameters() const {
        return parameters;
    }
    
    virtual std::vector<std::string> getParameterNames() const = 0;
};
