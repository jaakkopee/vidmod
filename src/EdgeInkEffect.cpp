#include "EdgeInkEffect.h"
#include <algorithm>
#include <cmath>

EdgeInkEffect::EdgeInkEffect() : Effect("EdgeInk") {
    setParameter("edge_threshold", 72.0f);
    setParameter("blur_size", 5.0f);
    setParameter("edge_strength", 1.15f);
    setParameter("invert", 0.0f);
    setParameter("ink_r", 30.0f);
    setParameter("ink_g", 200.0f);
    setParameter("ink_b", 255.0f);
    setParameter("blend", 0.55f);
    setParameter("audio_gain", 0.6f);
}

cv::Mat EdgeInkEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (frame.empty()) {
        return frame;
    }

    float threshold = getParameter("edge_threshold", 72.0f);
    int blurSize = static_cast<int>(std::round(getParameter("blur_size", 5.0f)));
    float edgeStrength = getParameter("edge_strength", 1.15f);
    bool invert = getParameter("invert", 0.0f) >= 0.5f;
    float inkR = getParameter("ink_r", 30.0f);
    float inkG = getParameter("ink_g", 200.0f);
    float inkB = getParameter("ink_b", 255.0f);
    float blend = getParameter("blend", 0.55f);
    float audioGain = getParameter("audio_gain", 0.6f);

    threshold = std::max(0.0f, std::min(threshold, 255.0f));
    blurSize = std::max(1, std::min(blurSize, 31));
    if ((blurSize % 2) == 0) {
        blurSize += 1;
    }
    edgeStrength = std::max(0.1f, std::min(edgeStrength, 4.0f));
    blend = std::max(0.0f, std::min(blend, 1.0f));

    if (audioBuffer && videoFps > 0.0f) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        audioFramesPerVideoFrame = std::max(1, audioFramesPerVideoFrame);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        float audioScale = (1.0f - audioGain) + (audioGain * rms);

        threshold = std::max(0.0f, std::min(threshold * (1.1f - 0.45f * audioScale), 255.0f));
        blend = std::max(0.0f, std::min(blend * (0.85f + 0.45f * audioScale), 1.0f));
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    cv::Mat smooth;
    cv::GaussianBlur(gray, smooth, cv::Size(blurSize, blurSize), 0.0);

    cv::Mat gradX, gradY;
    cv::Sobel(smooth, gradX, CV_32F, 1, 0, 3);
    cv::Sobel(smooth, gradY, CV_32F, 0, 1, 3);

    cv::Mat magnitude;
    cv::magnitude(gradX, gradY, magnitude);
    magnitude *= edgeStrength;

    cv::Mat edge8u;
    magnitude.convertTo(edge8u, CV_8U);
    cv::threshold(edge8u, edge8u, threshold, 255, cv::THRESH_BINARY);

    if (invert) {
        cv::bitwise_not(edge8u, edge8u);
    }

    cv::Mat output = frame.clone();
    const cv::Vec3f inkColor(inkB, inkG, inkR);

    for (int y = 0; y < output.rows; ++y) {
        const uchar* maskRow = edge8u.ptr<uchar>(y);
        const cv::Vec3b* srcRow = frame.ptr<cv::Vec3b>(y);
        cv::Vec3b* outRow = output.ptr<cv::Vec3b>(y);

        for (int x = 0; x < output.cols; ++x) {
            float mask = static_cast<float>(maskRow[x]) / 255.0f;
            float alpha = blend * mask;
            if (alpha <= 0.0f) {
                continue;
            }

            cv::Vec3f src(static_cast<float>(srcRow[x][0]),
                          static_cast<float>(srcRow[x][1]),
                          static_cast<float>(srcRow[x][2]));
            cv::Vec3f mixed = src * (1.0f - alpha) + inkColor * alpha;

            outRow[x][0] = static_cast<uchar>(std::clamp(static_cast<int>(std::round(mixed[0])), 0, 255));
            outRow[x][1] = static_cast<uchar>(std::clamp(static_cast<int>(std::round(mixed[1])), 0, 255));
            outRow[x][2] = static_cast<uchar>(std::clamp(static_cast<int>(std::round(mixed[2])), 0, 255));
        }
    }

    return output;
}
