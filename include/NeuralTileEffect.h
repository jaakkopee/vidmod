#pragma once

#include "Effect.h"
#include <vector>

class NeuralTileEffect : public Effect {
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
    NeuralTileEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float fps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {"tileSize", "threshold", "mode", "iterations", "movement", "audioMod", "feedback", "audio_gain"};
    }
};
