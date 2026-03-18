#pragma once

#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include "VideoProcessor.h"
#include "EffectChain.h"
#include "FFTEffect.h"
#include "ShadowEffect.h"
#include "LightEffect.h"
#include "DiffuseEffect.h"
#include "AudioColorEffect.h"
#include "FractalEffect.h"
#include "CircleQuiltEffect.h"
#include "EdgeInkEffect.h"
#include "CAGlowEffect.h"
#include "BitplaneReactorEffect.h"
#include "MoldTrailsEffect.h"
#include "AudioPlaylist.h"
#include "AutomationWindow.h"
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

class GUI {
private:
    sf::RenderWindow& window;
    tgui::Gui gui;
    
    VideoProcessor videoProcessor;
    EffectChain effectChain;
    AudioPlaylist audioPlaylist;
    
    tgui::ListBox::Ptr effectList;
    tgui::ListBox::Ptr chainList;
    tgui::ListBox::Ptr playlistBox;
    tgui::ScrollablePanel::Ptr paramPanel;
    tgui::Button::Ptr previewButton;
    tgui::Button::Ptr playRangeButton;
    tgui::Button::Ptr processButton;
    tgui::Label::Ptr statusLabel;
    tgui::Label::Ptr livePreviewStateLabel;
    tgui::ProgressBar::Ptr processingProgressBar;
    tgui::Slider::Ptr audioPositionSlider;
    tgui::Label::Ptr audioPositionLabel;
    tgui::RangeSlider::Ptr renderRangeSlider;
    tgui::Label::Ptr renderRangeLabel;
    tgui::Button::Ptr stopPreviewButton;
    tgui::CheckBox::Ptr verboseCheckbox;
    
    // Automation window
    std::unique_ptr<AutomationWindow> automationWindow;
    
    sf::Texture previewTexture;
    sf::Sprite previewSprite;
    float currentAudioPosition;
    float renderRangeStart;
    float renderRangeEnd;
    bool showingPreview;
    cv::Mat currentPreviewFrame;
    std::string currentImagePath;
    cv::Mat loadedImage;
    
    // Threading for video processing
    std::unique_ptr<std::thread> processingThread;
    std::atomic<bool> isProcessing;
    std::atomic<bool> isAudioMuxing;
    std::atomic<bool> shouldStopProcessing;
    std::atomic<int> currentProcessingFrame;
    std::atomic<int> totalProcessingFrames;
    std::unique_ptr<std::thread> livePreviewThread;
    std::atomic<bool> isLivePreviewPlaying;
    std::atomic<bool> shouldStopLivePreview;
    std::unique_ptr<sf::SoundBuffer> livePreviewSoundBuffer;
    std::unique_ptr<sf::Sound> livePreviewSound;
    std::chrono::steady_clock::time_point livePreviewAudioStartTime;
    float livePreviewAudioDurationSeconds;
    std::mutex previewMutex;
    cv::Mat latestProcessedFrame;
    int currentDisplayFrame;
    
    void setupUI();
    void addEffectToChain(const std::string& effectName);
    void removeSelectedEffect();
    void moveEffectUp();
    void moveEffectDown();
    void updateParameterPanel();
    void updateChainList();
    void loadVideoFile();
    void loadAudioFile();
    void loadImageFile();
    void addAudioToPlaylist();
    void removeAudioFromPlaylist();
    void clearPlaylist();
    void updatePlaylistDisplay();
    void syncPlaylistToVideoProcessor();
    void saveEffectChain();
    void loadEffectChain();
    void generatePreview();
    void startLivePreview();
    void stopLivePreview();
    void livePreviewLoop();
    void processVideo();
    void processImageLoop();
    void processImageLoopThreaded(const std::string& outputPath, float duration, float fps, AudioBuffer* audioToUse);
    bool renderImageLoopWithAutomation(const std::string& outputPath, float duration, float fps, AudioBuffer* audioToUse);
    void updatePreview(const cv::Mat& frame);
    void processVideoThreaded(const std::string& outputPath, float duration, AudioBuffer* audioToUse);
    void stopProcessing();
    void updateProcessingProgress();
    void openAutomationWindow();
    void updateAutomationWindow();
    void syncAutomationTimeline(float previewFps, AudioBuffer* activeAudioBuffer, float durationOverride = -1.0f);
    int getAutomationFrameForPosition(float previewFps, AudioBuffer* activeAudioBuffer, float durationOverride = -1.0f) const;
    int mapRenderFrameToAutomationFrame(int frameIndex, int totalRenderFrames) const;
    void applyAutomationAtFrame(int frameNumber);
    void updateParameterDisplayValues();

public:
    GUI(sf::RenderWindow& win);
    ~GUI();
    
    void handleEvent(const sf::Event& event);
    void draw();
};
