#include "RhythmoShadowEffect.h"
#include "RhythmogramDSP.h"
#include <algorithm>

RhythmoShadowEffect::RhythmoShadowEffect()
    : Effect("RhythmoShadow"), lastSampleRate_(0)
{
    setParameter("shadow_gain",      1.0f);
    setParameter("kernel_size",      3.0f);
    setParameter("morph_iterations", 1.0f);
    setParameter("audio_gain",       1.0f);
}

RhythmoShadowEffect::~RhythmoShadowEffect() = default;

void RhythmoShadowEffect::ensureDSP(int sampleRate) {
    if (!dsp_ || lastSampleRate_ != sampleRate) {
        dsp_ = std::make_unique<RhythmogramDSP>(static_cast<double>(sampleRate));
        lastSampleRate_ = sampleRate;
    }
}

cv::Mat RhythmoShadowEffect::apply(const cv::Mat& frame,
                                    AudioBuffer* audioBuffer,
                                    float videoFps) {
    if (!audioBuffer) return frame.clone();

    int n = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> samples = audioBuffer->getBuffer(n);
    if (samples.empty()) return frame.clone();

    float shadowGain    = getParameter("shadow_gain",      1.0f);
    int kernelSize      = static_cast<int>(getParameter("kernel_size",      3.0f));
    int morphIterations = static_cast<int>(getParameter("morph_iterations", 1.0f));
    float audioGain     = getParameter("audio_gain",       1.0f);

    kernelSize = std::max(1, std::min(kernelSize, 31));
    if ((kernelSize % 2) == 0) {
        kernelSize += 1;
    }
    morphIterations = std::max(1, std::min(morphIterations, 10));

    ensureDSP(audioBuffer->getSampleRate());

    auto columns = dsp_->processAndCollect(samples.data(), static_cast<int>(samples.size()));
    float energy = dsp_->normalizedEnergy(columns, audioGain);

    // Rhythmic energy drives the blend toward the local-minima image.
    float shadowCoeff = std::max(0.0f, std::min(shadowGain * energy, 1.0f));
    if (shadowCoeff <= 0.0f) return frame.clone();

    // Use morphological erosion to find local minima and their neighborhoods
    cv::Mat minima;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::erode(frame, minima, kernel, cv::Point(-1, -1), morphIterations);

    // Convert to float for blending
    cv::Mat frameFloat, minimaFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    minima.convertTo(minimaFloat, CV_32FC3);

    // Blend using addWeighted (optimized operation)
    cv::Mat result;
    cv::addWeighted(frameFloat, 1.0f - shadowCoeff, minimaFloat, shadowCoeff, 0.0, result);

    // Convert back to uint8
    cv::Mat output;
    result.convertTo(output, CV_8UC3);

    return output;
}
