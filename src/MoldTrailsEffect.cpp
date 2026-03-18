#include "MoldTrailsEffect.h"
#include <algorithm>
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;

MoldTrailsEffect::MoldTrailsEffect() : Effect("MoldTrails"), m_rng(std::random_device{}()) {
    setParameter("agent_count",    4000.0f);
    setParameter("sensor_angle",   30.0f);   // degrees
    setParameter("sensor_dist",    8.0f);    // sim-space pixels
    setParameter("turn_speed",     0.35f);   // radians per step
    setParameter("move_speed",     1.2f);    // sim-space pixels per step
    setParameter("decay_rate",     0.06f);   // trail fade per frame
    setParameter("blur_size",      5.0f);    // diffusion kernel
    setParameter("deposit_amount", 0.55f);
    setParameter("blend",          0.70f);
    setParameter("tint_r",         220.0f);
    setParameter("tint_g",         255.0f);
    setParameter("tint_b",         60.0f);
    setParameter("audio_gain",     0.55f);
    setParameter("sim_scale",      0.25f);
}

// ── helpers ───────────────────────────────────────────────────────────────────

void MoldTrailsEffect::initAgents(int count, int w, int h, const cv::Mat& grayLuma) {
    // Build a brightness-weighted spawn distribution from the luminance map
    // so agents preferentially appear in bright regions of the video.
    std::vector<float> weights(w * h);
    float total = 0.0f;
    for (int y = 0; y < h; ++y) {
        const float* row = grayLuma.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            float v = row[x] + 0.05f;   // small bias so dark areas still spawn
            weights[y * w + x] = v;
            total += v;
        }
    }

    // Normalize → CDF for inverse-transform sampling
    std::vector<float> cdf(weights.size());
    float acc = 0.0f;
    for (size_t i = 0; i < weights.size(); ++i) {
        acc += weights[i] / total;
        cdf[i] = acc;
    }

    std::uniform_real_distribution<float> uniF(0.0f, 1.0f);
    std::uniform_real_distribution<float> uniAngle(0.0f, 2.0f * PI);

    m_agents.resize(count);
    for (auto& a : m_agents) {
        float r = uniF(m_rng);
        auto it = std::lower_bound(cdf.begin(), cdf.end(), r);
        int idx = (int)std::clamp((long)(it - cdf.begin()), 0L, (long)(w * h - 1));
        a.x     = (float)(idx % w) + uniF(m_rng) - 0.5f;
        a.y     = (float)(idx / w) + uniF(m_rng) - 0.5f;
        a.angle = uniAngle(m_rng);
    }

    m_agentCount = count;
}

float MoldTrailsEffect::sampleTrail(float x, float y) const {
    // Bilinear sample with edge clamping
    float cx = std::clamp(x, 0.0f, (float)(m_simW - 1));
    float cy = std::clamp(y, 0.0f, (float)(m_simH - 1));
    int   ix = (int)cx, iy = (int)cy;
    float fx = cx - ix,  fy = cy - iy;
    int   ix1 = std::min(ix + 1, m_simW - 1);
    int   iy1 = std::min(iy + 1, m_simH - 1);

    float v00 = m_trailMap.at<float>(iy,  ix);
    float v10 = m_trailMap.at<float>(iy,  ix1);
    float v01 = m_trailMap.at<float>(iy1, ix);
    float v11 = m_trailMap.at<float>(iy1, ix1);
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) +
           (v01 * (1 - fx) + v11 * fx) * fy;
}

void MoldTrailsEffect::depositTrail(float x, float y, float amount) {
    int ix = (int)std::round(x);
    int iy = (int)std::round(y);
    if (ix >= 0 && ix < m_simW && iy >= 0 && iy < m_simH)
        m_trailMap.at<float>(iy, ix) = std::min(m_trailMap.at<float>(iy, ix) + amount, 1.0f);
}

// ── apply ─────────────────────────────────────────────────────────────────────

cv::Mat MoldTrailsEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (frame.empty()) return frame;
    m_frameCounter++;

    // ── parameters ──────────────────────────────────────────────────────────
    int   agentCount   = std::clamp((int)getParameter("agent_count"),   100, 20000);
    float sensorAngle  = std::clamp(getParameter("sensor_angle"),       5.0f,  90.0f);
    float sensorDist   = std::clamp(getParameter("sensor_dist"),        2.0f,  30.0f);
    float turnSpeed    = std::clamp(getParameter("turn_speed"),         0.05f,  1.5f);
    float moveSpeed    = std::clamp(getParameter("move_speed"),         0.2f,   6.0f);
    float decayRate    = std::clamp(getParameter("decay_rate"),         0.005f, 0.5f);
    int   blurSz       = std::clamp((int)getParameter("blur_size"),     1, 21);
    float depositAmt   = std::clamp(getParameter("deposit_amount"),     0.01f,  2.0f);
    float blend        = std::clamp(getParameter("blend"),              0.0f,   1.0f);
    float tintR        = getParameter("tint_r") / 255.0f;
    float tintG        = getParameter("tint_g") / 255.0f;
    float tintB        = getParameter("tint_b") / 255.0f;
    float audioGain    = getParameter("audio_gain");
    float simScale     = std::clamp(getParameter("sim_scale"),          0.1f,   0.6f);

    if ((blurSz % 2) == 0) ++blurSz;  // must be odd

    // ── audio reactivity ────────────────────────────────────────────────────
    // Typical RMS is 0.05-0.3, scale by 5x so audioEnergy covers [0,1] usefully.
    float audioEnergy = 0.0f;
    float bassEnergy = 0.0f;
    if (audioBuffer && videoFps > 0.0f) {
        int n = std::max(1, (int)(audioBuffer->getSampleRate() / videoFps));
        auto samples = audioBuffer->getBuffer(n);
        float rms = audioBuffer->getRMS(samples);
        audioEnergy = std::min(rms * audioGain * 5.0f, 1.0f);

        std::vector<float> bass, mid, treble;
        audioBuffer->getFFT(samples, bass, mid, treble);
        if (!bass.empty()) {
            float sum = 0.0f;
            for (float v : bass) sum += std::fabs(v);
            bassEnergy = std::min((sum / (float)bass.size()) * audioGain * 2.5f, 1.0f);
        }
    }
    audioEnergy = std::min(audioEnergy + bassEnergy * 0.7f, 1.0f);
    // Beat energy is positive transients over a slow envelope.
    m_smoothedAudioEnergy = m_smoothedAudioEnergy * 0.88f + audioEnergy * 0.12f;
    float beatEnergy = std::max(0.0f, audioEnergy - m_smoothedAudioEnergy);
    beatEnergy = std::min(beatEnergy * 4.0f, 1.0f);

    // On loud beats: agents sprint, deposit heavily, turn chaotically, trails linger longer
    moveSpeed  = std::min(moveSpeed  * (1.0f + audioEnergy * 6.0f), 10.0f);
    depositAmt = std::min(depositAmt * (1.0f + audioEnergy * 5.0f),  3.0f);
    turnSpeed  = std::min(turnSpeed  * (1.0f + audioEnergy * 2.0f + beatEnergy * 1.8f),  3.0f);
    decayRate  = std::max(decayRate  * (1.0f - audioEnergy * 0.5f),  0.001f);
    blend      = std::min(blend + audioEnergy * 0.25f + beatEnergy * 0.2f, 1.0f);

    // ── sim grid size ────────────────────────────────────────────────────────
    int simW = std::max(32, (int)std::lround(frame.cols * simScale));
    int simH = std::max(32, (int)std::lround(frame.rows * simScale));

    // Luminance map at sim resolution (used for spawning & attracting agents)
    cv::Mat gray, graySmall;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    gray.convertTo(graySmall, CV_32F, 1.0f / 255.0f);
    cv::resize(graySmall, graySmall, cv::Size(simW, simH), 0.0, 0.0, cv::INTER_AREA);

    // ── (re)init ─────────────────────────────────────────────────────────────
    bool reinit = !m_initialized
               || frame.size() != m_frameSize
               || simW != m_simW || simH != m_simH
               || agentCount != m_agentCount;

    if (reinit) {
        m_simW      = simW;
        m_simH      = simH;
        m_frameSize = frame.size();
        m_trailMap  = cv::Mat::zeros(simH, simW, CV_32F);
        initAgents(agentCount, simW, simH, graySmall);
        m_initialized = true;
    }

    // ── subtly attract agents toward bright image areas ───────────────────────
    // Keep image guidance weak to avoid freezing into a fixed static pattern.
    float imagePull = 0.0025f + 0.008f * audioEnergy;
    m_trailMap += graySmall * imagePull;

    // ── agent step ────────────────────────────────────────────────────────────
    float sAngRad = sensorAngle * (PI / 180.0f);
    std::uniform_real_distribution<float> jitter(-turnSpeed * 0.5f, turnSpeed * 0.5f);
    std::uniform_real_distribution<float> unit01(0.0f, 1.0f);
    std::uniform_real_distribution<float> fullAngle(0.0f, 2.0f * PI);

    for (auto& a : m_agents) {
        // Beat-driven stochastic reorientation to avoid long-term lock-in.
        if (unit01(m_rng) < (0.002f + 0.02f * beatEnergy)) {
            a.angle = fullAngle(m_rng);
        }

        // Sensor positions
        float lx = a.x + std::cos(a.angle - sAngRad) * sensorDist;
        float ly = a.y + std::sin(a.angle - sAngRad) * sensorDist;
        float cx = a.x + std::cos(a.angle)            * sensorDist;
        float cy = a.y + std::sin(a.angle)            * sensorDist;
        float rx = a.x + std::cos(a.angle + sAngRad) * sensorDist;
        float ry = a.y + std::sin(a.angle + sAngRad) * sensorDist;

        float sL = sampleTrail(lx, ly);
        float sC = sampleTrail(cx, cy);
        float sR = sampleTrail(rx, ry);

        // Steer
        if (sC >= sL && sC >= sR) {
            // Continue straight (optional small jitter)
            a.angle += jitter(m_rng) * 0.3f;
        } else if (sL > sR) {
            a.angle -= turnSpeed;
        } else if (sR > sL) {
            a.angle += turnSpeed;
        } else {
            a.angle += jitter(m_rng);
        }

        // Add low-amplitude evolving flow field; stronger with audio.
        float flow = std::sin(0.015f * (float)m_frameCounter + a.y * 0.045f + a.x * 0.018f);
        a.angle += flow * (0.06f + 0.30f * audioEnergy + 0.35f * beatEnergy);

        // Move
        a.x += std::cos(a.angle) * moveSpeed;
        a.y += std::sin(a.angle) * moveSpeed;

        // Wrap around edges
        if (a.x < 0.0f)           a.x += (float)simW;
        else if (a.x >= (float)simW) a.x -= (float)simW;
        if (a.y < 0.0f)           a.y += (float)simH;
        else if (a.y >= (float)simH) a.y -= (float)simH;

        // Deposit pheromone
        depositTrail(a.x, a.y, depositAmt * (1.0f + beatEnergy * 0.8f));
    }

    // ── trail diffusion + decay ────────────────────────────────────────────────
    cv::GaussianBlur(m_trailMap, m_trailMap, cv::Size(blurSz, blurSz), 0.0);
    m_trailMap *= (1.0f - decayRate);
    cv::threshold(m_trailMap, m_trailMap, 1.0f, 1.0f, cv::THRESH_TRUNC);

    // ── upsample + composite ──────────────────────────────────────────────────
    cv::Mat trailFull;
    cv::resize(m_trailMap, trailFull, frame.size(), 0.0, 0.0, cv::INTER_LINEAR);

    cv::Mat result = frame.clone();
    for (int y = 0; y < frame.rows; ++y) {
        const float*     tmap = trailFull.ptr<float>(y);
        const cv::Vec3b* in   = frame.ptr<cv::Vec3b>(y);
        cv::Vec3b*       out  = result.ptr<cv::Vec3b>(y);
        for (int x = 0; x < frame.cols; ++x) {
            float a = std::min(tmap[x] * blend, 1.0f);
            out[x][0] = (uint8_t)std::clamp((int)(in[x][0] * (1.0f - a) + tintB * 255.0f * a), 0, 255);
            out[x][1] = (uint8_t)std::clamp((int)(in[x][1] * (1.0f - a) + tintG * 255.0f * a), 0, 255);
            out[x][2] = (uint8_t)std::clamp((int)(in[x][2] * (1.0f - a) + tintR * 255.0f * a), 0, 255);
        }
    }

    return result;
}
