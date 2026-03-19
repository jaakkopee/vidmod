#pragma once

#include "Effect.h"
#include <vector>

class NeuralCircleEffect : public Effect {
private:
    struct Neuron {
        float activation;      // Current activation level
        float nextActivation;  // Next state activation
        int x, y;              // Position in grid
        cv::Vec3b color;       // Color from center pixel
    };
    
    std::vector<std::vector<Neuron>> neurons;
    int gridWidth;
    int gridHeight;
    cv::Mat previousFrame;
    
    void initializeNeurons(const cv::Mat& frame);
    void updateNeurons();
    float activationFunction(float input);
    
public:
    NeuralCircleEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float fps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {"circleSize", "threshold", "mode", "iterations", "movement", "audioMod", "feedback", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "circleSize")  return 200.0f;  // pixels
        if (name == "iterations")  return 20.0f;
        if (name == "mode")        return 2.0f;    // enum 0-2
        if (name == "threshold" || name == "movement" || name == "audioMod" ||
            name == "feedback"   || name == "audio_gain") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }
};
