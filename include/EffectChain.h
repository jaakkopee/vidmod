#pragma once

#include "Effect.h"
#include <vector>
#include <memory>
#include <string>

class EffectChain {
private:
    std::vector<std::shared_ptr<Effect>> effects;

public:
    void addEffect(std::shared_ptr<Effect> effect);
    void removeEffect(size_t index);
    void moveEffect(size_t fromIndex, size_t toIndex);
    void clear();
    
    cv::Mat applyEffects(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps);
    
    size_t size() const { return effects.size(); }
    std::shared_ptr<Effect> getEffect(size_t index) const;
    const std::vector<std::shared_ptr<Effect>>& getEffects() const { return effects; }
    
    // JSON serialization
    bool saveToJson(const std::string& filePath) const;
    bool loadFromJson(const std::string& filePath);
    std::string toJsonString() const;
    bool fromJsonString(const std::string& jsonStr);
};
