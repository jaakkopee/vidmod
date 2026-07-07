#include "RhythmoLightEffect.h"
#include "RhythmogramDSP.h"
#include <algorithm>

RhythmoLightEffect::RhythmoLightEffect()
    : Effect("RhythmoLight"), lastSampleRate_(0)
{
    setParameter("light_gain",       1.0f);
    setParameter("kernel_size",      3.0f);
    setParameter("morph_iterations", 1.0f);
    setParameter("audio_gain",       1.0f);
}

RhythmoLightEffect::~RhythmoLightEffect() = default;

void RhythmoLightEffect::ensureDSP(int sampleRate) {
    if (!dsp_ || lastSampleRate_ != sampleRate) {
        dsp_ = std::make_unique<RhythmogramDSP>(static_cast<double>(sampleRate));
        lastSampleRate_ = sampleRate;
    }
}

cv::Mat RhythmoLightEffect::apply(const cv::Mat& frame,
                                   AudioBuffer* audioBuffer,
                                   float videoFps) {
    if (!audioBuffer) return frame.clone();

    int n = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> samples = audioBuffer->getBuffer(n);
    if (samples.empty()) return frame.clone();

    float lightGain     = getParameter("light_gain",       1.0f);
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

    // Rhythmic energy drives the blend toward the local-maxima image.
    float lightCoeff = std::max(0.0f, std::min(lightGain * energy, 1.0f));
    if (lightCoeff <= 0.0f) return frame.clone();

    // Use morphological dilation to find local maxima and their neighborhoods
    cv::Mat maxima;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::dilate(frame, maxima, kernel, cv::Point(-1, -1), morphIterations);

    // Convert to float for blending
    cv::Mat frameFloat, maximaFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    maxima.convertTo(maximaFloat, CV_32FC3);

    // Blend using addWeighted (optimized operation)
    cv::Mat result;
    cv::addWeighted(frameFloat, 1.0f - lightCoeff, maximaFloat, lightCoeff, 0.0, result);

    // Convert back to uint8
    cv::Mat output;
    result.convertTo(output, CV_8UC3);

    return output;
}
