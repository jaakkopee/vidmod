#include "EffectChain.h"
#include <iostream>
#include <algorithm>

void EffectChain::addEffect(std::shared_ptr<Effect> effect) {
    effects.push_back(effect);
}

void EffectChain::removeEffect(size_t index) {
    if (index < effects.size()) {
        effects.erase(effects.begin() + index);
    }
}

void EffectChain::moveEffect(size_t fromIndex, size_t toIndex) {
    if (fromIndex < effects.size() && toIndex < effects.size()) {
        auto effect = effects[fromIndex];
        effects.erase(effects.begin() + fromIndex);
        effects.insert(effects.begin() + toIndex, effect);
    }
}

void EffectChain::clear() {
    effects.clear();
}

cv::Mat EffectChain::applyEffects(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    cv::Mat result = frame.clone();
    
    // Save the audio buffer position so all effects read from the same audio samples
    size_t savedAudioPosition = 0;
    if (audioBuffer) {
        savedAudioPosition = audioBuffer->getIndex();
    }
    
    for (auto& effect : effects) {
        std::cout << "Applying effect: " << effect->getName() << std::endl;
        
        // Restore audio position for each effect so they all read the same audio data
        if (audioBuffer) {
            audioBuffer->setIndex(savedAudioPosition);
        }
        
        result = effect->apply(result, audioBuffer, videoFps);
    }
    
    return result;
}

std::shared_ptr<Effect> EffectChain::getEffect(size_t index) const {
    if (index < effects.size()) {
        return effects[index];
    }
    return nullptr;
}
