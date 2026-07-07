#include "RhythmoHueEffect.h"
#include "RhythmogramDSP.h"
#include <cmath>
#include <algorithm>

RhythmoHueEffect::RhythmoHueEffect()
    : Effect("RhythmoHue"), lastSampleRate_(0)
{
    setParameter("hue_shift_gain", 30.0f);
    setParameter("audio_gain",     1.0f);
}

RhythmoHueEffect::~RhythmoHueEffect() = default;

void RhythmoHueEffect::ensureDSP(int sampleRate) {
    if (!dsp_ || lastSampleRate_ != sampleRate) {
        dsp_ = std::make_unique<RhythmogramDSP>(static_cast<double>(sampleRate));
        lastSampleRate_ = sampleRate;
    }
}

cv::Mat RhythmoHueEffect::apply(const cv::Mat& frame,
                                 AudioBuffer* audioBuffer,
                                 float videoFps) {
    if (!audioBuffer) return frame.clone();

    int n = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> samples = audioBuffer->getBuffer(n);
    if (samples.empty()) return frame.clone();

    float hueShiftGain = getParameter("hue_shift_gain", 30.0f);
    float audioGain    = getParameter("audio_gain",      1.0f);

    ensureDSP(audioBuffer->getSampleRate());

    auto columns = dsp_->processAndCollect(samples.data(), static_cast<int>(samples.size()));
    float energy = dsp_->normalizedEnergy(columns, audioGain);

    // Hue shift in degrees (OpenCV 8-bit HSV: H in [0, 180)).
    float hueShift = hueShiftGain * energy;

    // Convert BGR → HSV
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    // Shift hue in float space and wrap into [0, 180)
    cv::Mat hFloat;
    channels[0].convertTo(hFloat, CV_32F);
    hFloat += hueShift;
    hFloat.forEach<float>([](float& v, const int* /*pos*/) {
        v = std::fmod(v, 180.0f);
        if (v < 0.0f) v += 180.0f;
    });
    hFloat.convertTo(channels[0], CV_8U);

    cv::Mat result;
    cv::merge(channels, result);
    cv::cvtColor(result, result, cv::COLOR_HSV2BGR);

    return result;
}
