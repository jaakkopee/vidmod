#include "CAGlowEffect.h"
#include <algorithm>
#include <cmath>

CAGlowEffect::CAGlowEffect() : Effect("CAGlow") {
    setParameter("iterations", 5.0f);
    setParameter("state_count", 6.0f);
    setParameter("neighbor_mix", 0.7f);
    setParameter("sim_scale", 0.35f);
    setParameter("blur_size", 13.0f);
    setParameter("contrast", 1.25f);
    setParameter("glow_strength", 1.2f);
    setParameter("blend", 0.55f);
    setParameter("tint_r", 90.0f);
    setParameter("tint_g", 210.0f);
    setParameter("tint_b", 255.0f);
    setParameter("audio_gain", 0.55f);
}

cv::Mat CAGlowEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (frame.empty()) {
        return frame;
    }

    int iterations = static_cast<int>(std::lround(getParameter("iterations", 5.0f)));
    int stateCount = static_cast<int>(std::lround(getParameter("state_count", 6.0f)));
    float neighborMix = getParameter("neighbor_mix", 0.7f);
    float simScale = getParameter("sim_scale", 0.35f);
    int blurSize = static_cast<int>(std::lround(getParameter("blur_size", 13.0f)));
    float contrast = getParameter("contrast", 1.25f);
    float glowStrength = getParameter("glow_strength", 1.2f);
    float blend = getParameter("blend", 0.55f);
    float tintR = getParameter("tint_r", 90.0f);
    float tintG = getParameter("tint_g", 210.0f);
    float tintB = getParameter("tint_b", 255.0f);
    float audioGain = getParameter("audio_gain", 0.55f);

    iterations = std::max(1, std::min(iterations, 20));
    stateCount = std::max(2, std::min(stateCount, 16));
    neighborMix = std::max(0.0f, std::min(neighborMix, 1.0f));
    simScale = std::max(0.15f, std::min(simScale, 1.0f));
    blurSize = std::max(1, std::min(blurSize, 41));
    if ((blurSize % 2) == 0) {
        blurSize += 1;
    }
    contrast = std::max(0.5f, std::min(contrast, 3.0f));
    glowStrength = std::max(0.0f, std::min(glowStrength, 4.0f));
    blend = std::max(0.0f, std::min(blend, 1.0f));

    if (audioBuffer && videoFps > 0.0f) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        audioFramesPerVideoFrame = std::max(1, audioFramesPerVideoFrame);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        float audioScale = (1.0f - audioGain) + (audioGain * rms);

        glowStrength = std::max(0.0f, std::min(glowStrength * (0.9f + 0.6f * audioScale), 4.0f));
        blend = std::max(0.0f, std::min(blend * (0.9f + 0.35f * audioScale), 1.0f));
        iterations = std::max(1, std::min(static_cast<int>(std::round(iterations * (0.9f + 0.4f * audioScale))), 20));
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    int simW = std::max(24, static_cast<int>(std::lround(gray.cols * simScale)));
    int simH = std::max(24, static_cast<int>(std::lround(gray.rows * simScale)));
    cv::Mat simGray;
    cv::resize(gray, simGray, cv::Size(simW, simH), 0.0, 0.0, cv::INTER_AREA);

    cv::Mat states(simGray.size(), CV_8U);
    for (int y = 0; y < simGray.rows; ++y) {
        const uchar* gRow = simGray.ptr<uchar>(y);
        uchar* sRow = states.ptr<uchar>(y);
        for (int x = 0; x < simGray.cols; ++x) {
            int q = (static_cast<int>(gRow[x]) * stateCount) / 256;
            sRow[x] = static_cast<uchar>(std::clamp(q, 0, stateCount - 1));
        }
    }

    cv::Mat nextStates = states.clone();
    for (int it = 0; it < iterations; ++it) {
        for (int y = 0; y < states.rows; ++y) {
            const int ym1 = (y - 1 + states.rows) % states.rows;
            const int yp1 = (y + 1) % states.rows;
            const uchar* prev = states.ptr<uchar>(ym1);
            const uchar* cur = states.ptr<uchar>(y);
            const uchar* next = states.ptr<uchar>(yp1);
            uchar* out = nextStates.ptr<uchar>(y);

            for (int x = 0; x < states.cols; ++x) {
                const int xm1 = (x - 1 + states.cols) % states.cols;
                const int xp1 = (x + 1) % states.cols;

                int selfState = static_cast<int>(cur[x]);
                int sum =
                    static_cast<int>(prev[xm1]) + static_cast<int>(prev[x]) + static_cast<int>(prev[xp1]) +
                    static_cast<int>(cur[xm1]) + static_cast<int>(cur[xp1]) +
                    static_cast<int>(next[xm1]) + static_cast<int>(next[x]) + static_cast<int>(next[xp1]);

                int candidate = (sum + selfState * 3 + it) % stateCount;
                float mixed = (1.0f - neighborMix) * static_cast<float>(selfState)
                    + neighborMix * static_cast<float>(candidate);
                out[x] = static_cast<uchar>(std::clamp(static_cast<int>(std::lround(mixed)), 0, stateCount - 1));
            }
        }
        std::swap(states, nextStates);
    }

    cv::Mat activation;
    states.convertTo(activation, CV_32F, 1.0 / static_cast<double>(stateCount - 1));

    // Emphasize CA structures so it is more than color grading.
    cv::Mat activationEdges;
    cv::Laplacian(activation, activationEdges, CV_32F, 3);
    cv::Mat absEdges = cv::abs(activationEdges);
    activation = activation * 0.65f + absEdges * 1.4f;

    if (contrast != 1.0f) {
        cv::pow(activation, contrast, activation);
    }

    if (blurSize > 1) {
        cv::GaussianBlur(activation, activation, cv::Size(blurSize, blurSize), 0.0);
    }

    cv::Mat activationFull;
    cv::resize(activation, activationFull, frame.size(), 0.0, 0.0, cv::INTER_CUBIC);

    cv::Mat output = frame.clone();
    const cv::Vec3f glowColor(tintB, tintG, tintR);

    for (int y = 0; y < output.rows; ++y) {
        const cv::Vec3b* srcRow = frame.ptr<cv::Vec3b>(y);
        cv::Vec3b* outRow = output.ptr<cv::Vec3b>(y);
        const float* aRow = activationFull.ptr<float>(y);

        for (int x = 0; x < output.cols; ++x) {
            float a = std::clamp(aRow[x], 0.0f, 1.0f);
            float alpha = std::clamp(blend * a * glowStrength, 0.0f, 1.0f);
            if (alpha <= 0.0f) {
                continue;
            }

            cv::Vec3f src(
                static_cast<float>(srcRow[x][0]),
                static_cast<float>(srcRow[x][1]),
                static_cast<float>(srcRow[x][2])
            );
            cv::Vec3f mixed = src * (1.0f - alpha) + glowColor * alpha;

            outRow[x][0] = static_cast<uchar>(std::clamp(static_cast<int>(std::round(mixed[0])), 0, 255));
            outRow[x][1] = static_cast<uchar>(std::clamp(static_cast<int>(std::round(mixed[1])), 0, 255));
            outRow[x][2] = static_cast<uchar>(std::clamp(static_cast<int>(std::round(mixed[2])), 0, 255));
        }
    }

    return output;
}
