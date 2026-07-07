#include "RhythmoSaturationEffect.h"
#include "RhythmogramDSP.h"
#include <algorithm>

RhythmoSaturationEffect::RhythmoSaturationEffect()
    : Effect("RhythmoSaturation"), lastSampleRate_(0)
{
    setParameter("saturation_gain", 2.0f);
    setParameter("audio_gain",      1.0f);
}

RhythmoSaturationEffect::~RhythmoSaturationEffect() = default;

void RhythmoSaturationEffect::ensureDSP(int sampleRate) {
    if (!dsp_ || lastSampleRate_ != sampleRate) {
        dsp_ = std::make_unique<RhythmogramDSP>(static_cast<double>(sampleRate));
        lastSampleRate_ = sampleRate;
    }
}

cv::Mat RhythmoSaturationEffect::apply(const cv::Mat& frame,
                                        AudioBuffer* audioBuffer,
                                        float videoFps) {
    if (!audioBuffer) return frame.clone();

    int n = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> samples = audioBuffer->getBuffer(n);
    if (samples.empty()) return frame.clone();

    float saturationGain = getParameter("saturation_gain", 2.0f);
    float audioGain      = getParameter("audio_gain",      1.0f);

    ensureDSP(audioBuffer->getSampleRate());

    auto columns = dsp_->processAndCollect(samples.data(), static_cast<int>(samples.size()));
    float energy = dsp_->normalizedEnergy(columns, audioGain);

    // Saturation scale factor: 1 at silence, grows with rhythmic energy.
    float factor = 1.0f + saturationGain * energy;
    factor = std::max(0.0f, factor);

    // Convert BGR → HSV
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    // Scale saturation (channel index 1) and clamp to [0, 255]
    cv::Mat sFloat;
    channels[1].convertTo(sFloat, CV_32F);
    sFloat *= factor;
    cv::min(sFloat, 255.0f, sFloat);
    sFloat.convertTo(channels[1], CV_8U);

    cv::Mat result;
    cv::merge(channels, result);
    cv::cvtColor(result, result, cv::COLOR_HSV2BGR);

    return result;
}
