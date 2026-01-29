#include "NeuralCircleEffect.h"
#include <cmath>
#include <algorithm>

NeuralCircleEffect::NeuralCircleEffect() : Effect("NeuralCircle") {
    parameters["circleSize"] = 20.0f;    // Size of each circle/neuron in pixels
    parameters["threshold"] = 0.5f;      // Activation threshold
    parameters["mode"] = 1.0f;           // 0=vertical, 1=4-way, 2=horizontal
    parameters["iterations"] = 5.0f;     // Number of neural iterations
    parameters["movement"] = 0.3f;       // How much circles move based on activation
    parameters["audioMod"] = 0.5f;       // Audio modulation strength
    gridWidth = 0;
    gridHeight = 0;
}

void NeuralCircleEffect::initializeNeurons(const cv::Mat& frame) {
    int circleSize = static_cast<int>(parameters["circleSize"]);
    if (circleSize < 4) circleSize = 4;
    
    gridWidth = frame.cols / circleSize;
    gridHeight = frame.rows / circleSize;
    
    neurons.clear();
    neurons.resize(gridHeight);
    
    for (int y = 0; y < gridHeight; ++y) {
        neurons[y].resize(gridWidth);
        for (int x = 0; x < gridWidth; ++x) {
            Neuron& n = neurons[y][x];
            n.x = x * circleSize + circleSize / 2;
            n.y = y * circleSize + circleSize / 2;
            
            // Get color from center pixel of circle
            int px = std::min(n.x, frame.cols - 1);
            int py = std::min(n.y, frame.rows - 1);
            n.color = frame.at<cv::Vec3b>(py, px);
            
            // Initial activation from brightness
            float brightness = (n.color[0] + n.color[1] + n.color[2]) / (3.0f * 255.0f);
            n.activation = brightness;
            n.nextActivation = brightness;
        }
    }
}

float NeuralCircleEffect::activationFunction(float input) {
    float threshold = parameters["threshold"];
    
    // Sigmoid-like activation
    if (input < threshold) {
        return input * 0.5f;
    } else {
        return threshold + (input - threshold) * 1.5f;
    }
    
    // Clamp to [0, 1]
    return std::max(0.0f, std::min(1.0f, input));
}

void NeuralCircleEffect::updateNeurons() {
    int mode = static_cast<int>(parameters["mode"]);
    float threshold = parameters["threshold"];
    
    // Calculate next state for all neurons
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            Neuron& n = neurons[y][x];
            float input = n.activation;
            int neighbors = 0;
            
            // Add neighbor contributions based on mode
            if (mode == 0 || mode == 1) { // Vertical or 4-way
                // Top neighbor
                if (y > 0) {
                    input += neurons[y-1][x].activation;
                    neighbors++;
                }
                // Bottom neighbor
                if (y < gridHeight - 1) {
                    input += neurons[y+1][x].activation;
                    neighbors++;
                }
            }
            
            if (mode == 1 || mode == 2) { // 4-way or Horizontal
                // Left neighbor
                if (x > 0) {
                    input += neurons[y][x-1].activation;
                    neighbors++;
                }
                // Right neighbor
                if (x < gridWidth - 1) {
                    input += neurons[y][x+1].activation;
                    neighbors++;
                }
            }
            
            // Average with neighbors
            if (neighbors > 0) {
                input /= (neighbors + 1);
            }
            
            // Apply activation function
            n.nextActivation = activationFunction(input);
        }
    }
    
    // Apply next state
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            neurons[y][x].activation = neurons[y][x].nextActivation;
        }
    }
}

cv::Mat NeuralCircleEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float fps) {
    cv::Mat result = cv::Mat::zeros(frame.size(), frame.type());
    
    int circleSize = static_cast<int>(parameters["circleSize"]);
    int iterations = static_cast<int>(parameters["iterations"]);
    float movement = parameters["movement"];
    float audioMod = parameters["audioMod"];
    
    // Get audio influence
    float audioInfluence = 0.0f;
    if (audioBuffer) {
        int chunkSize = static_cast<int>(audioBuffer->getSampleRate() / fps);
        std::vector<float> audioChunk = audioBuffer->getBuffer(chunkSize);
        audioInfluence = audioBuffer->getRMS(audioChunk) * audioMod;
    }
    
    // Initialize neuron grid
    initializeNeurons(frame);
    
    // Run neural iterations
    for (int i = 0; i < iterations; ++i) {
        updateNeurons();
    }
    
    // Render circles based on neuron activation
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            Neuron& n = neurons[y][x];
            
            // Circle radius based on activation
            float sizeMod = 0.5f + n.activation * 0.5f + audioInfluence * 0.3f;
            int currentRadius = static_cast<int>((circleSize / 2.0f) * sizeMod);
            currentRadius = std::max(2, std::min(circleSize, currentRadius));
            
            // Position offset based on activation and movement
            int offsetX = static_cast<int>((n.activation - 0.5f) * movement * circleSize);
            int offsetY = static_cast<int>((n.activation - 0.5f) * movement * circleSize * 0.5f);
            
            int centerX = x * circleSize + circleSize / 2 + offsetX;
            int centerY = y * circleSize + circleSize / 2 + offsetY;
            
            cv::Point center(centerX, centerY);
            
            // Clamp center to frame bounds
            center.x = std::max(currentRadius, std::min(center.x, frame.cols - 1 - currentRadius));
            center.y = std::max(currentRadius, std::min(center.y, frame.rows - 1 - currentRadius));
            
            // Color intensity based on activation
            cv::Vec3b circleColor = n.color;
            float intensity = 0.3f + n.activation * 0.7f;
            circleColor[0] = static_cast<uchar>(circleColor[0] * intensity);
            circleColor[1] = static_cast<uchar>(circleColor[1] * intensity);
            circleColor[2] = static_cast<uchar>(circleColor[2] * intensity);
            
            // Draw filled circle
            cv::circle(result, center, currentRadius, circleColor, -1);
        }
    }
    
    return result;
}
