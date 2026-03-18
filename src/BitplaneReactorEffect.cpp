#include "BitplaneReactorEffect.h"
#include <algorithm>
#include <cmath>

BitplaneReactorEffect::BitplaneReactorEffect() : Effect("BitplaneReactor") {
    setParameter("bit_lo",       2.0f);   // start at bit 2 (skip fine noise)
    setParameter("bit_count",    2.0f);   // process 2 bitplanes
    setParameter("wolfram_rule", 110.0f); // rule 110: complex aperiodic patterns
    setParameter("sim_scale",    0.5f);   // run CA at half resolution
    setParameter("blend",        0.60f);
    setParameter("tint_r",       30.0f);
    setParameter("tint_g",       230.0f);
    setParameter("tint_b",       140.0f);
    setParameter("audio_gain",   0.5f);
}

cv::Mat BitplaneReactorEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    // Audio features
    float audioRms = 0.0f;
    float bassEnergy = 0.0f;
    float audioGain = getParameter("audio_gain");
    if (audioBuffer && videoFps > 0.0f) {
        int samplesPerFrame = std::max(1, (int)(audioBuffer->getSampleRate() / videoFps));
        auto samples = audioBuffer->getBuffer(samplesPerFrame);
        float rms = audioBuffer->getRMS(samples);
        audioRms = rms * audioGain;

        std::vector<float> bass, mid, treble;
        audioBuffer->getFFT(samples, bass, mid, treble);
        if (!bass.empty()) {
            float sum = 0.0f;
            for (float v : bass) sum += std::fabs(v);
            bassEnergy = (sum / (float)bass.size()) * audioGain;
        }
    }

    // Envelope + beat transient (positive delta over envelope)
    m_audioEnvelope = m_audioEnvelope * 0.90f + audioRms * 0.10f;
    float beat = std::max(0.0f, audioRms - m_audioEnvelope);
    beat = std::min(beat * 6.0f, 1.0f);
    float energy = std::min(audioRms * 4.0f + bassEnergy * 2.5f + beat * 0.8f, 1.0f);

    int   bitLo   = std::clamp((int)getParameter("bit_lo"),       0,   5);
    int   bitCnt  = std::clamp((int)getParameter("bit_count"),    1,   4);
    int   ruleVal = std::clamp((int)getParameter("wolfram_rule"), 0, 255);
    float scale   = std::clamp(getParameter("sim_scale"),         0.1f, 1.0f);
    float blend   = std::clamp(getParameter("blend") + energy * 0.55f, 0.0f, 1.0f);
    float tintR   = getParameter("tint_r") / 255.0f;
    float tintG   = getParameter("tint_g") / 255.0f;
    float tintB   = getParameter("tint_b") / 255.0f;

    // Audio-reactive structure changes (very visible on beats).
    int dynamicRule = (ruleVal + (int)std::lround(beat * 96.0f) + (int)std::lround(energy * 48.0f)) & 255;
    int dynamicBitLo = std::clamp(bitLo + (int)std::lround(bassEnergy * 3.0f + beat * 2.0f), 0, 6);

    // Grayscale + downsample for fast CA
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    int simW = std::max(32, (int)std::lround(frame.cols * scale));
    int simH = std::max(32, (int)std::lround(frame.rows * scale));

    cv::Mat simGray;
    cv::resize(gray, simGray, cv::Size(simW, simH), 0.0, 0.0, cv::INTER_AREA);

    // Accumulate bitplane CA results
    cv::Mat reactor = cv::Mat::zeros(simH, simW, CV_32F);
    float weight = 1.0f / bitCnt;

    for (int b = dynamicBitLo; b < dynamicBitLo + bitCnt && b <= 7; ++b) {
        // Extract binary bitplane (0 or 1)
        cv::Mat plane(simH, simW, CV_8U);
        for (int y = 0; y < simH; ++y) {
            const uint8_t* src = simGray.ptr<uint8_t>(y);
            uint8_t*       dst = plane.ptr<uint8_t>(y);
            for (int x = 0; x < simW; ++x)
                dst[x] = (src[x] >> b) & 1;
        }

        // Wolfram elementary CA: each row computed from the row above
        // XOR with the seed row so the pattern is anchored to image content
        cv::Mat ca(simH, simW, CV_8U, cv::Scalar(0));
        plane.row(0).copyTo(ca.row(0));

        for (int y = 1; y < simH; ++y) {
            const uint8_t* prev = ca.ptr<uint8_t>(y - 1);
            const uint8_t* seed = plane.ptr<uint8_t>(y);
            uint8_t*       curr = ca.ptr<uint8_t>(y);

            for (int x = 0; x < simW; ++x) {
                // Wrap at edges
                int l = prev[(x > 0)       ? x - 1 : simW - 1];
                int c = prev[x];
                int r = prev[(x < simW - 1) ? x + 1 : 0];
                int pattern = (l << 2) | (c << 1) | r;
                // XOR CA result with image seed to anchor to content
                curr[x] = (uint8_t)(((dynamicRule >> pattern) & 1) ^ seed[x]);
            }
        }

        // Accumulate weighted contribution
        for (int y = 0; y < simH; ++y) {
            const uint8_t* src = ca.ptr<uint8_t>(y);
            float*         dst = reactor.ptr<float>(y);
            for (int x = 0; x < simW; ++x)
                dst[x] += src[x] * weight;
        }
    }

    // Upsample reactor map to full frame size
    cv::Mat reactorFull;
    cv::resize(reactor, reactorFull, frame.size(), 0.0, 0.0, cv::INTER_LINEAR);

    // Composite: overlay tint colour where the reactor is active
    cv::Mat result = frame.clone();
    for (int y = 0; y < frame.rows; ++y) {
        const float*  rmap = reactorFull.ptr<float>(y);
        const cv::Vec3b* in  = frame.ptr<cv::Vec3b>(y);
        cv::Vec3b*       out = result.ptr<cv::Vec3b>(y);
        for (int x = 0; x < frame.cols; ++x) {
            float a = std::min(rmap[x] * blend, 1.0f);
            out[x][0] = (uint8_t)std::clamp((int)(in[x][0] * (1.0f - a) + tintB * 255.0f * a), 0, 255);
            out[x][1] = (uint8_t)std::clamp((int)(in[x][1] * (1.0f - a) + tintG * 255.0f * a), 0, 255);
            out[x][2] = (uint8_t)std::clamp((int)(in[x][2] * (1.0f - a) + tintR * 255.0f * a), 0, 255);
        }
    }

    return result;
}
