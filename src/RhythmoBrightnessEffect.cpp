#include "RhythmoBrightnessEffect.h"
#include "RhythmogramDSP.h"
#include <algorithm>

RhythmoBrightnessEffect::RhythmoBrightnessEffect()
    : Effect("RhythmoBrightness"), lastSampleRate_(0)
{
    setParameter("brightness_gain", 1.0f);
    setParameter("audio_gain",      1.0f);
}

RhythmoBrightnessEffect::~RhythmoBrightnessEffect() = default;

void RhythmoBrightnessEffect::ensureDSP(int sampleRate) {
    if (!dsp_ || lastSampleRate_ != sampleRate) {
        dsp_ = std::make_unique<RhythmogramDSP>(static_cast<double>(sampleRate));
        lastSampleRate_ = sampleRate;
    }
}

cv::Mat RhythmoBrightnessEffect::apply(const cv::Mat& frame,
                                        AudioBuffer* audioBuffer,
                                        float videoFps) {
    if (!audioBuffer) return frame.clone();

    int n = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> samples = audioBuffer->getBuffer(n);
    if (samples.empty()) return frame.clone();

    float brightnessGain = getParameter("brightness_gain", 1.0f);
    float audioGain      = getParameter("audio_gain",      1.0f);

    ensureDSP(audioBuffer->getSampleRate());

    auto columns = dsp_->processAndCollect(samples.data(), static_cast<int>(samples.size()));
    float energy = dsp_->normalizedEnergy(columns, audioGain);

    // Scale every pixel uniformly; brightness doubles when energy == 1/gain.
    float factor = 1.0f + brightnessGain * energy;
    factor = std::max(0.0f, factor);

    cv::Mat output;
    frame.convertTo(output, CV_32FC3);
    output *= factor;
    cv::min(output, 255.0f, output);
    output.convertTo(output, CV_8UC3);

    return output;
}
