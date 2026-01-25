#include "NeuralTileEffect.h"
#include <cmath>
#include <algorithm>

NeuralTileEffect::NeuralTileEffect() : Effect("NeuralTile") {
    parameters["tileSize"] = 20.0f;      // Size of each tile/neuron in pixels
    parameters["threshold"] = 0.5f;      // Activation threshold
    parameters["mode"] = 1.0f;           // 0=vertical, 1=4-way, 2=horizontal
    parameters["iterations"] = 5.0f;     // Number of neural iterations
    parameters["movement"] = 0.3f;       // How much tiles move based on activation
    parameters["audioMod"] = 0.5f;       // Audio modulation strength
    gridWidth = 0;
    gridHeight = 0;
}

void NeuralTileEffect::initializeNeurons(const cv::Mat& frame) {
    int tileSize = static_cast<int>(parameters["tileSize"]);
    if (tileSize < 4) tileSize = 4;
    
    gridWidth = frame.cols / tileSize;
    gridHeight = frame.rows / tileSize;
    
    neurons.clear();
    neurons.resize(gridHeight);
    
    for (int y = 0; y < gridHeight; ++y) {
        neurons[y].resize(gridWidth);
        for (int x = 0; x < gridWidth; ++x) {
            Neuron& n = neurons[y][x];
            n.x = x * tileSize + tileSize / 2;
            n.y = y * tileSize + tileSize / 2;
            
            // Get color from center pixel of tile
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

float NeuralTileEffect::activationFunction(float input) {
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

void NeuralTileEffect::updateNeurons() {
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

cv::Mat NeuralTileEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float fps) {
    cv::Mat result = cv::Mat::zeros(frame.size(), frame.type());
    
    int tileSize = static_cast<int>(parameters["tileSize"]);
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
    
    // Render tiles based on neuron activation
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            Neuron& n = neurons[y][x];
            
            // Tile size based on activation
            float sizeMod = 0.5f + n.activation * 0.5f + audioInfluence * 0.3f;
            int currentTileSize = static_cast<int>(tileSize * sizeMod);
            currentTileSize = std::max(2, std::min(tileSize * 2, currentTileSize));
            
            // Position offset based on activation and movement
            int offsetX = static_cast<int>((n.activation - 0.5f) * movement * tileSize);
            int offsetY = static_cast<int>((n.activation - 0.5f) * movement * tileSize * 0.5f);
            
            int centerX = x * tileSize + tileSize / 2 + offsetX;
            int centerY = y * tileSize + tileSize / 2 + offsetY;
            
            // Draw tile as rectangle
            int halfSize = currentTileSize / 2;
            cv::Point topLeft(centerX - halfSize, centerY - halfSize);
            cv::Point bottomRight(centerX + halfSize, centerY + halfSize);
            
            // Clamp to frame bounds
            topLeft.x = std::max(0, std::min(topLeft.x, frame.cols - 1));
            topLeft.y = std::max(0, std::min(topLeft.y, frame.rows - 1));
            bottomRight.x = std::max(0, std::min(bottomRight.x, frame.cols - 1));
            bottomRight.y = std::max(0, std::min(bottomRight.y, frame.rows - 1));
            
            // Color intensity based on activation
            cv::Vec3b tileColor = n.color;
            float intensity = 0.3f + n.activation * 0.7f;
            tileColor[0] = static_cast<uchar>(tileColor[0] * intensity);
            tileColor[1] = static_cast<uchar>(tileColor[1] * intensity);
            tileColor[2] = static_cast<uchar>(tileColor[2] * intensity);
            
            cv::rectangle(result, topLeft, bottomRight, tileColor, -1);
            
            // Optional: draw border for active neurons
            if (n.activation > parameters["threshold"]) {
                cv::rectangle(result, topLeft, bottomRight, cv::Scalar(255, 255, 255), 1);
            }
        }
    }
    
    return result;
}
