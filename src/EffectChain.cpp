#include "EffectChain.h"
#include "FFTEffect.h"
#include "ShadowEffect.h"
#include "LightEffect.h"
#include "DiffuseEffect.h"
#include "AudioColorEffect.h"
#include "FractalEffect.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
        // Restore audio position for each effect so they all read the same audio data
        if (audioBuffer) {
            audioBuffer->setIndex(savedAudioPosition);
        }
        
        result = effect->apply(result, audioBuffer, videoFps);
    }
    
    // After all effects are applied, advance audio position by the samples used for this frame
    // This ensures the next frame gets the next chunk of audio
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        // The audio position has already advanced during effect processing, no need to advance again
        // Just ensure we're at the correct position after all effects
        audioBuffer->setIndex(savedAudioPosition + audioFramesPerVideoFrame);
    }
    
    return result;
}

std::shared_ptr<Effect> EffectChain::getEffect(size_t index) const {
    if (index < effects.size()) {
        return effects[index];
    }
    return nullptr;
}

bool EffectChain::saveToJson(const std::string& filePath) const {
    try {
        json j;
        j["version"] = "1.0";
        j["effects"] = json::array();
        
        for (const auto& effect : effects) {
            json effectJson;
            effectJson["name"] = effect->getName();
            effectJson["parameters"] = json::object();
            
            for (const auto& [key, value] : effect->getParameters()) {
                effectJson["parameters"][key] = value;
            }
            
            j["effects"].push_back(effectJson);
        }
        
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            return false;
        }
        
        file << j.dump(2); // Pretty print with 2 spaces
        file.close();
        
        std::cout << "Effect chain saved to: " << filePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving effect chain: " << e.what() << std::endl;
        return false;
    }
}

bool EffectChain::loadFromJson(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for reading: " << filePath << std::endl;
            return false;
        }
        
        json j;
        file >> j;
        file.close();
        
        return fromJsonString(j.dump());
    } catch (const std::exception& e) {
        std::cerr << "Error loading effect chain: " << e.what() << std::endl;
        return false;
    }
}

std::string EffectChain::toJsonString() const {
    try {
        json j;
        j["version"] = "1.0";
        j["effects"] = json::array();
        
        for (const auto& effect : effects) {
            json effectJson;
            effectJson["name"] = effect->getName();
            effectJson["parameters"] = json::object();
            
            for (const auto& [key, value] : effect->getParameters()) {
                effectJson["parameters"][key] = value;
            }
            
            j["effects"].push_back(effectJson);
        }
        
        return j.dump(2);
    } catch (const std::exception& e) {
        std::cerr << "Error converting to JSON: " << e.what() << std::endl;
        return "";
    }
}

bool EffectChain::fromJsonString(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        // Clear existing effects
        clear();
        
        if (!j.contains("effects") || !j["effects"].is_array()) {
            std::cerr << "Invalid JSON: missing 'effects' array" << std::endl;
            return false;
        }
        
        for (const auto& effectJson : j["effects"]) {
            if (!effectJson.contains("name")) {
                std::cerr << "Effect missing 'name' field" << std::endl;
                continue;
            }
            
            std::string name = effectJson["name"];
            std::shared_ptr<Effect> effect;
            
            // Create effect based on name
            if (name == "FFT") {
                effect = std::make_shared<FFTEffect>();
            } else if (name == "Shadow") {
                effect = std::make_shared<ShadowEffect>();
            } else if (name == "Light") {
                effect = std::make_shared<LightEffect>();
            } else if (name == "Diffuse") {
                effect = std::make_shared<DiffuseEffect>();
            } else if (name == "AudioColor") {
                effect = std::make_shared<AudioColorEffect>();
            } else if (name == "Fractal") {
                effect = std::make_shared<FractalEffect>();
            } else {
                std::cerr << "Unknown effect type: " << name << std::endl;
                continue;
            }
            
            // Set parameters
            if (effectJson.contains("parameters") && effectJson["parameters"].is_object()) {
                for (auto& [key, value] : effectJson["parameters"].items()) {
                    if (value.is_number()) {
                        effect->setParameter(key, value.get<float>());
                    }
                }
            }
            
            addEffect(effect);
        }
        
        std::cout << "Loaded " << effects.size() << " effects from JSON" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}
