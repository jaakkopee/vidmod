#include "CircleQuiltEffect.h"
#include <algorithm>
#include <cmath>

CircleQuiltEffect::CircleQuiltEffect() : Effect("CircleQuilt") {
    setParameter("cells_x", 14.0f);
    setParameter("cells_y", 14.0f);
    setParameter("radius_scale", 0.92f);
    setParameter("edge_softness", 0.35f);
    setParameter("blend", 0.82f);
    setParameter("audio_gain", 0.65f);
}

cv::Mat CircleQuiltEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (frame.empty()) {
        return frame;
    }

    const int width = frame.cols;
    const int height = frame.rows;

    int cellsX = static_cast<int>(std::round(getParameter("cells_x", 14.0f)));
    int cellsY = static_cast<int>(std::round(getParameter("cells_y", 14.0f)));
    float radiusScale = getParameter("radius_scale", 0.92f);
    float edgeSoftness = getParameter("edge_softness", 0.35f);
    float blend = getParameter("blend", 0.82f);
    float audioGain = getParameter("audio_gain", 0.65f);

    cellsX = std::max(1, std::min(cellsX, 256));
    cellsY = std::max(1, std::min(cellsY, 256));
    radiusScale = std::max(0.1f, std::min(radiusScale, 2.5f));
    edgeSoftness = std::max(0.0f, std::min(edgeSoftness, 1.0f));
    blend = std::max(0.0f, std::min(blend, 1.0f));

    if (audioBuffer && videoFps > 0.0f) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        audioFramesPerVideoFrame = std::max(1, audioFramesPerVideoFrame);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        float audioScale = (1.0f - audioGain) + (audioGain * rms);

        // Keep modulation subtle so visuals remain stable.
        radiusScale = std::max(0.1f, std::min(radiusScale * (0.9f + 0.5f * audioScale), 2.5f));
        blend = std::max(0.0f, std::min(blend * (0.85f + 0.5f * audioScale), 1.0f));
    }

    const float cellW = static_cast<float>(width) / static_cast<float>(cellsX);
    const float cellH = static_cast<float>(height) / static_cast<float>(cellsY);

    cv::Mat output = frame.clone();

    for (int y = 0; y < height; ++y) {
        cv::Vec3b* outRow = output.ptr<cv::Vec3b>(y);
        const cv::Vec3b* inRow = frame.ptr<cv::Vec3b>(y);

        for (int x = 0; x < width; ++x) {
            int cellX = std::min(cellsX - 1, static_cast<int>(x / cellW));
            int cellY = std::min(cellsY - 1, static_cast<int>(y / cellH));

            int cx = std::min(width - 1, std::max(0, static_cast<int>((cellX + 0.5f) * cellW)));
            int cy = std::min(height - 1, std::max(0, static_cast<int>((cellY + 0.5f) * cellH)));
            cv::Vec3b tileColor = frame.at<cv::Vec3b>(cy, cx);

            float dx = static_cast<float>(x) - static_cast<float>(cx);
            float dy = static_cast<float>(y) - static_cast<float>(cy);
            float dist = std::sqrt(dx * dx + dy * dy);

            float baseRadius = 0.5f * std::min(cellW, cellH);
            float radius = std::max(1.0f, baseRadius * radiusScale);
            float inside = 1.0f - (dist / radius);

            float mask = 0.0f;
            if (inside >= 0.0f) {
                if (edgeSoftness <= 0.001f) {
                    mask = 1.0f;
                } else {
                    float softPow = 1.0f + 3.0f * edgeSoftness;
                    mask = std::pow(std::max(0.0f, std::min(1.0f, inside)), softPow);
                }
            }

            // Keep a visible mosaic base everywhere and strengthen inside circles.
            float alpha = blend * (0.25f + 0.75f * mask);
            if (alpha <= 0.0f) {
                continue;
            }

            cv::Vec3b src = inRow[x];
            cv::Vec3b out;
            for (int c = 0; c < 3; ++c) {
                float mixed = (1.0f - alpha) * static_cast<float>(src[c]) + alpha * static_cast<float>(tileColor[c]);
                out[c] = static_cast<unsigned char>(std::clamp(static_cast<int>(std::round(mixed)), 0, 255));
            }
            outRow[x] = out;
        }
    }

    return output;
}
