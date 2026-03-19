#include "GUI.h"
#include "VideoBuffer.h"
#include "NeuralTileEffect.h"
#include "NeuralCircleEffect.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>  // for strerror
#include <cerrno>   // for errno
#include <thread>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sys/wait.h>  // for WEXITSTATUS
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::vector<std::string> withAudioGainParam(const std::vector<std::string>& paramNames) {
    std::vector<std::string> names = paramNames;
    if (std::find(names.begin(), names.end(), "audio_gain") == names.end()) {
        names.push_back("audio_gain");
    }
    return names;
}

int runFfmpegWithProgressPipe(const std::string& ffmpegCommand,
                               const std::string& progressPath,
                               std::atomic<int>& progressValue,
                               std::atomic<int>& progressTotal) {
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::remove(progressPath, ec);

    progressTotal = 100;
    progressValue = 10;

    // Capture exit code from FFmpeg in a shared variable
    std::atomic<int> ffmpegExitCode(-1);
    
    // Run FFmpeg in background.
    std::thread ffmpegThread([ffmpegCommand, &ffmpegExitCode]() {
        int result = system(ffmpegCommand.c_str());
        // system() returns -1 on error, otherwise (exit_code << 8) | signal
        // Extract actual exit code from the status
        if (result == -1) {
            ffmpegExitCode = -1;
        } else {
            ffmpegExitCode = WEXITSTATUS(result);
        }
    });

    int lastProgress = 10;
    int pollCount = 0;
    
    // Wait for FFmpeg to complete, updating progress every 100ms until done.
    // Allow up to 5 minutes (300 seconds) for long muxing operations.
    const int maxPolls = 3000;
    while (ffmpegThread.joinable() && pollCount < maxPolls) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        pollCount++;
        int progress = std::min(99, 10 + (pollCount / 10));  // Slow progression toward 99%
        if (progress > lastProgress) {
            progressValue = progress;
            lastProgress = progress;
        }
        
        // Check if thread has finished
        if (!ffmpegThread.joinable()) {
            break;
        }
    }

    if (ffmpegThread.joinable()) {
        ffmpegThread.join();
    }
    
    // If FFmpeg didn't complete in time, return error
    if (ffmpegExitCode == -1) {
        std::cerr << "Error: FFmpeg process did not complete or encountered an error" << std::endl;
        progressValue = 0;
        return 1;
    }
    
    if (ffmpegExitCode != 0) {
        std::cerr << "Error: FFmpeg exited with code " << ffmpegExitCode << std::endl;
        progressValue = 0;
        return ffmpegExitCode;
    }
    
    progressValue = 100;
    return 0;
}
}

GUI::GUI(sf::RenderWindow& win) : window(win), gui(window), audioPlaylist(44100), previewSprite(previewTexture), currentAudioPosition(0.0f), renderRangeStart(0.0f), renderRangeEnd(1.0f), showingPreview(false), isProcessing(false), isAudioMuxing(false), shouldStopProcessing(false), currentProcessingFrame(0), totalProcessingFrames(0), isLivePreviewPlaying(false), shouldStopLivePreview(false), livePreviewAudioDurationSeconds(0.0f), currentDisplayFrame(0), isDraggingChainItem(false), isDraggingPlaylistItem(false), dragSourceChainIndex(-1), dragSourcePlaylistIndex(-1), isEditingParameterField(false) {
    automationWindow = std::make_unique<AutomationWindow>(1000);
    setupUI();
}

GUI::~GUI() {
    stopLivePreview();

    if (processingThread && processingThread->joinable()) {
        shouldStopProcessing = true;
        processingThread->join();
    }
}

void GUI::syncAutomationTimeline(float previewFps, AudioBuffer* activeAudioBuffer, float durationOverride) {
    if (!automationWindow) {
        return;
    }

    // Keep automation authoring on a stable normalized timeline.
    // Rendering and preview map their progress onto this range.
    automationWindow->setTotalFrames(1000);
}

int GUI::getAutomationFrameForPosition(float previewFps, AudioBuffer* activeAudioBuffer, float durationOverride) const {
    int totalFrames = automationWindow ? automationWindow->getTotalFrames() : 1000;

    if (totalFrames <= 1) {
        return 0;
    }

    return std::clamp(static_cast<int>(std::lround(currentAudioPosition * (totalFrames - 1))), 0, totalFrames - 1);
}

int GUI::mapRenderFrameToAutomationFrame(int frameIndex, int totalRenderFrames) const {
    int automationFrames = automationWindow ? automationWindow->getTotalFrames() : 1000;

    if (automationFrames <= 1 || totalRenderFrames <= 1) {
        return 0;
    }

    double progress = static_cast<double>(frameIndex) / static_cast<double>(totalRenderFrames - 1);
    return std::clamp(static_cast<int>(std::lround(progress * (automationFrames - 1))), 0, automationFrames - 1);
}

void GUI::setupUI() {
    std::cout << "Setting up GUI..." << std::endl;
    
    // Try to load a font - TGUI 1.x requires explicit font loading
    try {
        tgui::Font::setGlobalFont("/System/Library/Fonts/Helvetica.ttc");
        std::cout << "Font loaded successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not load font: " << e.what() << std::endl;
        std::cerr << "GUI may not display text correctly" << std::endl;
    }
    
    // Main layout with panels
    auto mainPanel = tgui::Panel::create();
    mainPanel->setSize("100%", "100%");
    mainPanel->getRenderer()->setBackgroundColor(tgui::Color::Transparent);
    gui.add(mainPanel);
    
    std::cout << "Main panel created" << std::endl;
    
    // Left panel for effect selection
    auto leftPanel = tgui::Panel::create();
    leftPanel->setSize("20%", "100%");
    leftPanel->setPosition("0%", "0%");
    leftPanel->getRenderer()->setBackgroundColor(tgui::Color(220, 220, 220));
    mainPanel->add(leftPanel);
    
    std::cout << "Left panel created" << std::endl;
    
    auto effectLabel = tgui::Label::create("Available Effects:");
    effectLabel->setPosition("5%", "2%");
    effectLabel->setTextSize(16);
    leftPanel->add(effectLabel);
    
    effectList = tgui::ListBox::create();
    effectList->setSize("90%", "25%");
    effectList->setPosition("5%", "7%");
    effectList->addItem("FFT");
    effectList->addItem("Shadow");
    effectList->addItem("Light");
    effectList->addItem("Diffuse");
    effectList->addItem("AudioColor");
    effectList->addItem("Fractal");
    effectList->addItem("CircleQuilt");
    effectList->addItem("EdgeInk");
    effectList->addItem("CAGlow");
    effectList->addItem("BitplaneReactor");
    effectList->addItem("MoldTrails");
    effectList->addItem("NeuralTile");
    effectList->addItem("NeuralCircle");
    leftPanel->add(effectList);
    
    auto addButton = tgui::Button::create("Add to Chain");
    addButton->setSize("90%", "5%");
    addButton->setPosition("5%", "34%");
    addButton->onPress([this]() { 
        if (effectList->getSelectedItemIndex() >= 0) {
            addEffectToChain(effectList->getSelectedItem().toStdString());
        }
    });
    leftPanel->add(addButton);
    
    // Effect Chain Label
    auto chainLabel = tgui::Label::create("Effect Chain:");
    chainLabel->setPosition("5%", "41%");
    chainLabel->setTextSize(16);
    leftPanel->add(chainLabel);

    chainList = tgui::ListBox::create();
    chainList->setSize("90%", "40%");
    chainList->setPosition("5%", "46%");
    chainList->onItemSelect([this](int) { updateParameterPanel(); });
    leftPanel->add(chainList);
    
    // Chain management buttons (right next to the chain list)
    auto toggleBypassBtn = tgui::Button::create("✓ Toggle Bypass");
    toggleBypassBtn->setSize("90%", "4%");
    toggleBypassBtn->setPosition("5%", "82%");
    toggleBypassBtn->onPress([this]() { 
        int selected = chainList->getSelectedItemIndex();
        if (selected >= 0) {
            auto effect = effectChain.getEffect(selected);
            if (effect) {
                effect->setBypass(!effect->isBypassed());
                updateChainList();  // Update display to show [BYPASSED] label
            }
        }
    });
    leftPanel->add(toggleBypassBtn);
    
    auto removeEffectBtn = tgui::Button::create("Remove Effect");
    removeEffectBtn->setSize("90%", "4%");
    removeEffectBtn->setPosition("5%", "87%");
    removeEffectBtn->onPress([this]() { removeSelectedEffect(); });
    leftPanel->add(removeEffectBtn);

    auto automationButton = tgui::Button::create("Automation");
    automationButton->setSize("90%", "4%");
    automationButton->setPosition("5%", "92%");
    automationButton->onPress([this]() { openAutomationWindow(); });
    leftPanel->add(automationButton);
    
    // Middle panel for audio and file management
    auto middlePanel = tgui::Panel::create();
    middlePanel->setSize("20%", "100%");
    middlePanel->setPosition("20%", "0%");
    middlePanel->getRenderer()->setBackgroundColor(tgui::Color(240, 240, 240));
    mainPanel->add(middlePanel);
    
    // Audio Playlist Section
    auto playlistLabel = tgui::Label::create("Audio Playlist:");
    playlistLabel->setPosition("5%", "2%");
    playlistLabel->setTextSize(16);
    middlePanel->add(playlistLabel);
    
    playlistBox = tgui::ListBox::create();
    playlistBox->setSize("90%", "22%");
    playlistBox->setPosition("5%", "7%");
    middlePanel->add(playlistBox);
    
    auto addAudioBtn = tgui::Button::create("▲ Add Audio");
    addAudioBtn->setSize("44%", "4%");
    addAudioBtn->setPosition("5%", "30.5%");
    addAudioBtn->onPress([this]() { addAudioToPlaylist(); });
    middlePanel->add(addAudioBtn);
    
    auto removeAudioBtn = tgui::Button::create("▼ Remove Audio");
    removeAudioBtn->setSize("44%", "4%");
    removeAudioBtn->setPosition("51%", "30.5%");
    removeAudioBtn->onPress([this]() { removeAudioFromPlaylist(); });
    middlePanel->add(removeAudioBtn);
    
    auto clearPlaylistBtn = tgui::Button::create("Clear Playlist");
    clearPlaylistBtn->setSize("90%", "4%");
    clearPlaylistBtn->setPosition("5%", "35.5%");
    clearPlaylistBtn->onPress([this]() { clearPlaylist(); });
    middlePanel->add(clearPlaylistBtn);

    auto playlistJsonLabel = tgui::Label::create("Playlist JSON:");
    playlistJsonLabel->setPosition("5%", "40.5%");
    playlistJsonLabel->setTextSize(10);
    middlePanel->add(playlistJsonLabel);

    auto savePlaylistBtn = tgui::Button::create("Save Playlist");
    savePlaylistBtn->setSize("44%", "3%");
    savePlaylistBtn->setPosition("5%", "42.5%");
    savePlaylistBtn->onPress([this]() { savePlaylist(); });
    middlePanel->add(savePlaylistBtn);

    auto loadPlaylistBtn = tgui::Button::create("Load Playlist");
    loadPlaylistBtn->setSize("44%", "3%");
    loadPlaylistBtn->setPosition("51%", "42.5%");
    loadPlaylistBtn->onPress([this]() { loadPlaylist(); });
    middlePanel->add(loadPlaylistBtn);
    
    // Chain Save/Load buttons
    auto fxJsonLabel = tgui::Label::create("FX Chain JSON:");
    fxJsonLabel->setPosition("5%", "46.5%");
    fxJsonLabel->setTextSize(10);
    middlePanel->add(fxJsonLabel);

    auto saveChainBtn = tgui::Button::create("Save Chain");
    saveChainBtn->setSize("44%", "3%");
    saveChainBtn->setPosition("5%", "48.5%");
    saveChainBtn->onPress([this]() { saveEffectChain(); });
    middlePanel->add(saveChainBtn);
    
    auto loadChainBtn = tgui::Button::create("Load Chain");
    loadChainBtn->setSize("44%", "3%");
    loadChainBtn->setPosition("51%", "48.5%");
    loadChainBtn->onPress([this]() { loadEffectChain(); });
    middlePanel->add(loadChainBtn);

    fxChainAutomationToggle = tgui::CheckBox::create();
    fxChainAutomationToggle->setPosition("5%", "51.5%");
    fxChainAutomationToggle->setSize("5%", "2.5%");
    fxChainAutomationToggle->setChecked(true);
    middlePanel->add(fxChainAutomationToggle);

    auto fxChainAutomationLabel = tgui::Label::create("Save/Load Automation");
    fxChainAutomationLabel->setPosition("11%", "51.3%");
    fxChainAutomationLabel->setTextSize(10);
    middlePanel->add(fxChainAutomationLabel);
    
    // File Loading Section
    auto fileLabel = tgui::Label::create("Input Files:");
    fileLabel->setPosition("5%", "54%");
    fileLabel->setTextSize(14);
    middlePanel->add(fileLabel);
    
    auto loadVideoButton = tgui::Button::create("Load Video");
    loadVideoButton->setSize("90%", "4%");
    loadVideoButton->setPosition("5%", "57.5%");
    loadVideoButton->onPress([this]() { loadVideoFile(); });
    middlePanel->add(loadVideoButton);
    
    auto loadImageButton = tgui::Button::create("Load Image");
    loadImageButton->setSize("90%", "4%");
    loadImageButton->setPosition("5%", "62%");
    loadImageButton->onPress([this]() { loadImageFile(); });
    middlePanel->add(loadImageButton);
    
    // Audio position slider
    auto audioTimeLabel = tgui::Label::create("Audio Time:");
    audioTimeLabel->setPosition("5%", "70%");
    audioTimeLabel->setTextSize(12);
    middlePanel->add(audioTimeLabel);
    
    audioPositionLabel = tgui::Label::create("0.0s");
    audioPositionLabel->setPosition("75%", "70%");
    audioPositionLabel->setTextSize(12);
    middlePanel->add(audioPositionLabel);
    
    audioPositionSlider = tgui::Slider::create();
    audioPositionSlider->setSize("90%", "4%");
    audioPositionSlider->setPosition("5%", "73%");
    audioPositionSlider->setMinimum(0);
    audioPositionSlider->setMaximum(100);
    audioPositionSlider->setValue(0);
    audioPositionSlider->onValueChange([this](float value) {
        currentAudioPosition = value / 100.0f; // 0.0 to 1.0
        
        // Use playlist audio if available, otherwise use VideoProcessor's audio
        AudioBuffer* activeAudioBuffer = audioPlaylist.getAudioBuffer() ? 
                                         audioPlaylist.getAudioBuffer() : 
                                         videoProcessor.getAudioBuffer();
        
        if (activeAudioBuffer) {
            float audioDuration = static_cast<float>(activeAudioBuffer->size()) / 
                                 activeAudioBuffer->getSampleRate();
            float timeSeconds = currentAudioPosition * audioDuration;
            audioPositionLabel->setText(std::to_string(static_cast<int>(timeSeconds)) + "." + 
                                       std::to_string(static_cast<int>(timeSeconds * 10) % 10) + "s");
        }
    });
    middlePanel->add(audioPositionSlider);

    // Render range slider
    auto renderRangeHeaderLabel = tgui::Label::create("Render Range:");
    renderRangeHeaderLabel->setPosition("5%", "78%");
    renderRangeHeaderLabel->setTextSize(12);
    middlePanel->add(renderRangeHeaderLabel);

    renderRangeLabel = tgui::Label::create("0% - 100%");
    renderRangeLabel->setPosition("48%", "78%");
    renderRangeLabel->setTextSize(12);
    middlePanel->add(renderRangeLabel);

    renderRangeSlider = tgui::RangeSlider::create(0.0f, 100.0f);
    renderRangeSlider->setSize("90%", "4%");
    renderRangeSlider->setPosition("5%", "81%");
    renderRangeSlider->setSelectionStart(0.0f);
    renderRangeSlider->setSelectionEnd(100.0f);
    renderRangeSlider->onRangeChange([this](float start, float end) {
        renderRangeStart = start / 100.0f;
        renderRangeEnd   = end   / 100.0f;

        AudioBuffer* activeAudio = audioPlaylist.getAudioBuffer() ?
                                   audioPlaylist.getAudioBuffer() :
                                   videoProcessor.getAudioBuffer();
        if (activeAudio && activeAudio->getSampleRate() > 0) {
            float totalDur = static_cast<float>(activeAudio->size()) / activeAudio->getSampleRate();
            float startTime = renderRangeStart * totalDur;
            float endTime   = renderRangeEnd   * totalDur;
            auto fmtTime = [](float t) {
                return std::to_string(static_cast<int>(t)) + "." +
                       std::to_string(static_cast<int>(t * 10) % 10) + "s";
            };
            renderRangeLabel->setText(fmtTime(startTime) + " - " + fmtTime(endTime));
        } else {
            renderRangeLabel->setText(std::to_string(static_cast<int>(start)) +
                                      "% - " +
                                      std::to_string(static_cast<int>(end)) + "%");
        }
    });
    middlePanel->add(renderRangeSlider);

    previewButton = tgui::Button::create("Preview Frame");
    previewButton->setSize("90%", "4%");
    previewButton->setPosition("5%", "86%");
    previewButton->onPress([this]() { 
        generatePreview();
    });
    middlePanel->add(previewButton);

    playRangeButton = tgui::Button::create("Play Range");
    playRangeButton->setSize("44%", "4%");
    playRangeButton->setPosition("5%", "91%");
    playRangeButton->onPress([this]() {
        startLivePreview();
    });
    middlePanel->add(playRangeButton);

    stopPreviewButton = tgui::Button::create("Stop");
    stopPreviewButton->setSize("44%", "4%");
    stopPreviewButton->setPosition("51%", "91%");
    stopPreviewButton->onPress([this]() {
        stopLivePreview();
    });
    middlePanel->add(stopPreviewButton);

    livePreviewStateLabel = tgui::Label::create("Live: Stopped");
    livePreviewStateLabel->setPosition("5%", "95%");
    livePreviewStateLabel->setTextSize(11);
    middlePanel->add(livePreviewStateLabel);
    
    verboseCheckbox = tgui::CheckBox::create("Verbose Progress");
    verboseCheckbox->setSize(15, 15);
    verboseCheckbox->setPosition("60%", "95.5%");
    verboseCheckbox->setChecked(true);
    verboseCheckbox->onChange([this](bool checked) {
        videoProcessor.setVerbose(checked);
    });
    middlePanel->add(verboseCheckbox);
    
    // Right panel for parameters
    auto rightPanel = tgui::Panel::create();
    rightPanel->setSize("20%", "100%");
    rightPanel->setPosition("40%", "0%");
    rightPanel->getRenderer()->setBackgroundColor(tgui::Color(240, 240, 240));
    mainPanel->add(rightPanel);
    
    auto paramLabel = tgui::Label::create("Parameters:");
    paramLabel->setPosition("5%", "2%");
    paramLabel->setTextSize(16);
    rightPanel->add(paramLabel);
    
    paramPanel = tgui::ScrollablePanel::create();
    paramPanel->setSize("90%", "70%");
    paramPanel->setPosition("5%", "7%");
    paramPanel->getVerticalScrollbar()->setPolicy(tgui::Scrollbar::Policy::Automatic);
    paramPanel->getHorizontalScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    rightPanel->add(paramPanel);
    
    // Processing buttons at bottom of right panel
    auto processVideoButton = tgui::Button::create("Process Video");
    processVideoButton->setSize("90%", "4%");
    processVideoButton->setPosition("5%", "80%");
    processVideoButton->onPress([this]() { 
        std::cout << "Process video button clicked!" << std::endl;
        processVideo(); 
    });
    rightPanel->add(processVideoButton);
    processButton = processVideoButton;  // Keep pointer for compatibility
    
    auto processImageButton = tgui::Button::create("Process Image Loop");
    processImageButton->setSize("90%", "4%");
    processImageButton->setPosition("5%", "85%");
    processImageButton->onPress([this]() { 
        std::cout << "Process image loop button clicked!" << std::endl;
        processImageLoop(); 
    });
    rightPanel->add(processImageButton);
    
    // Status label at bottom of preview area (not covering the buttons!)
    statusLabel = tgui::Label::create("Ready");
    statusLabel->setSize("38%", "8%");
    statusLabel->setPosition("61%", "87%");
    statusLabel->setTextSize(14);
    mainPanel->add(statusLabel);

    // Processing progress bar for image/video rendering
    processingProgressBar = tgui::ProgressBar::create();
    processingProgressBar->setSize("38%", "2.5%");
    processingProgressBar->setPosition("61%", "83%");
    processingProgressBar->setMinimum(0);
    processingProgressBar->setMaximum(100);
    processingProgressBar->setValue(0);
    processingProgressBar->setVisible(false);
    mainPanel->add(processingProgressBar);
}

void GUI::addEffectToChain(const std::string& effectName) {
    std::shared_ptr<Effect> effect;
    
    if (effectName == "FFT") {
        effect = std::make_shared<FFTEffect>();
    } else if (effectName == "Shadow") {
        effect = std::make_shared<ShadowEffect>();
    } else if (effectName == "Light") {
        effect = std::make_shared<LightEffect>();
    } else if (effectName == "Diffuse") {
        effect = std::make_shared<DiffuseEffect>();
    } else if (effectName == "AudioColor") {
        effect = std::make_shared<AudioColorEffect>();
    } else if (effectName == "Fractal") {
        effect = std::make_shared<FractalEffect>();
    } else if (effectName == "CircleQuilt") {
        effect = std::make_shared<CircleQuiltEffect>();
    } else if (effectName == "EdgeInk") {
        effect = std::make_shared<EdgeInkEffect>();
    } else if (effectName == "CAGlow") {
        effect = std::make_shared<CAGlowEffect>();
    } else if (effectName == "BitplaneReactor") {
        effect = std::make_shared<BitplaneReactorEffect>();
    } else if (effectName == "MoldTrails") {
        effect = std::make_shared<MoldTrailsEffect>();
    } else if (effectName == "NeuralTile") {
        effect = std::make_shared<NeuralTileEffect>();
    } else if (effectName == "NeuralCircle") {
        effect = std::make_shared<NeuralCircleEffect>();
    }
    
    if (effect) {
        effectChain.addEffect(effect);
        updateChainList();
        statusLabel->setText("Added " + effectName + " to chain");
    }
}

void GUI::removeSelectedEffect() {
    int selected = chainList->getSelectedItemIndex();
    if (selected >= 0) {
        effectChain.removeEffect(selected);
        updateChainList();
        paramPanel->removeAllWidgets();
        statusLabel->setText("Effect removed");
    }
}

void GUI::updateChainList() {
    chainList->removeAllItems();
    const auto& effects = effectChain.getEffects();
    for (size_t i = 0; i < effects.size(); ++i) {
        // Show bypass status in the label
        std::string label = std::to_string(i + 1) + ". " + effects[i]->getName();
        if (effects[i]->isBypassed()) {
            label += " [BYPASSED]";
        }
        chainList->addItem(label);
    }
}

void GUI::updateParameterPanel() {
    paramPanel->removeAllWidgets();
    isEditingParameterField = false;
    
    int selected = chainList->getSelectedItemIndex();
    if (selected < 0) return;
    
    auto effect = effectChain.getEffect(selected);
    if (!effect) return;
    
    float yPos = 10.0f;
    const auto paramNames = withAudioGainParam(effect->getParameterNames());
    
    for (const auto& paramName : paramNames) {
        auto label = tgui::Label::create(paramName + ":");
        label->setPosition(10, yPos);
        label->setTextSize(12);
        paramPanel->add(label);
        
        auto editBox = tgui::EditBox::create();
        editBox->setSize(150, 25);
        editBox->setPosition(10, yPos + 20);
        
        // Get current value (may be automated)
        float currentValue = effect->getParameter(paramName);
        editBox->setText(tgui::String(currentValue));

        editBox->onFocus([this]() {
            isEditingParameterField = true;
        });
        
        // Save parameter on Return key
        editBox->onReturnKeyPress([this, effect, paramName, editBox]() {
            try {
                float value = std::stof(editBox->getText().toStdString());
                effect->setParameter(paramName, value);
                isEditingParameterField = false;
                statusLabel->setText("Updated " + paramName);
                std::cout << "Parameter " << paramName << " updated to " << value << std::endl;
            } catch (...) {
                statusLabel->setText("Invalid value for " + paramName);
            }
        });
        
        // Also save parameter when the edit box loses focus
        editBox->onUnfocus([this, effect, paramName, editBox]() {
            isEditingParameterField = false;
            try {
                float value = std::stof(editBox->getText().toStdString());
                effect->setParameter(paramName, value);
                std::cout << "Parameter " << paramName << " updated to " << value << " (unfocus)" << std::endl;
            } catch (...) {
                // Invalid value, ignore
            }
        });
        
        paramPanel->add(editBox);
        
        yPos += 55.0f;
    }
    
    // Set content size for scrollable panel - width should match panel width, height based on content
    float panelWidth = paramPanel->getSize().x;
    paramPanel->setContentSize({panelWidth, yPos + 10.0f});
}

void GUI::loadVideoFile() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Load Video");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Video file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([this, pathEdit]() {
        // Use macOS file picker
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file with prompt \"Select Video File\" of type {\"public.movie\", \"public.mpeg-4\"})' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("30%", "15%");
    loadBtn->setPosition("5%", "75%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        if (videoProcessor.loadVideo(path)) {
            statusLabel->setText("Video loaded: " + path.substr(path.find_last_of("/\\") + 1));
        } else {
            statusLabel->setText("Failed to load video");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(loadBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("40%", "20%");
    cancelBtn->setPosition("55%", "70%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::loadAudioFile() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Load Audio");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Audio file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([this, pathEdit]() {
        // Use macOS file picker
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file with prompt \"Select Audio File\" of type {\"public.audio\"})' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("30%", "15%");
    loadBtn->setPosition("5%", "75%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        if (videoProcessor.loadAudio(path)) {
            statusLabel->setText("Audio loaded: " + path.substr(path.find_last_of("/\\") + 1));
        } else {
            statusLabel->setText("Failed to load audio");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(loadBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::loadImageFile() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Load Image for Looping");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Image file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([this, pathEdit]() {
        // Use macOS file picker
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file with prompt \"Select Image File\" of type {\"public.image\"})' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("30%", "15%");
    loadBtn->setPosition("5%", "75%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        
        // Load the image and show preview
        loadedImage = cv::imread(path);
        if (!loadedImage.empty()) {
            currentImagePath = path;
            
            // Apply automation at frame 0 for image preview
            applyAutomationAtFrame(0);
            updateParameterDisplayValues();
            
            // Show preview of the loaded image with effects applied
            cv::Mat previewFrame = loadedImage.clone();
            // If there are effects, apply them; otherwise show the original image
            AudioBuffer* previewAudio = audioPlaylist.getAudioBuffer() ?
                                       audioPlaylist.getAudioBuffer() :
                                       videoProcessor.getAudioBuffer();
            cv::Mat processedFrame = effectChain.applyEffects(previewFrame, previewAudio, 30.0f);
            updatePreview(processedFrame);
            
            statusLabel->setText("Image loaded (preview shown). Add effects and click 'Process Video' to render.");
            gui.remove(fileDialog);
        } else {
            statusLabel->setText("Failed to load image");
            gui.remove(fileDialog);
        }
    });
    fileDialog->add(loadBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::addAudioToPlaylist() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Add Audio to Playlist");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Audio file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([this, pathEdit]() {
        // Use macOS file picker
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file with prompt \"Select Audio File\" of type {\"public.audio\"})' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);
    
    auto loadBtn = tgui::Button::create("Add");
    loadBtn->setSize("30%", "15%");
    loadBtn->setPosition("5%", "75%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        if (audioPlaylist.addTrack(path)) {
            updatePlaylistDisplay();
            syncPlaylistToVideoProcessor();
            statusLabel->setText("Added to playlist: " + path.substr(path.find_last_of("/\\") + 1));
        } else {
            statusLabel->setText("Failed to add audio");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(loadBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::removeAudioFromPlaylist() {
    if (playlistBox->getSelectedItemIndex() >= 0) {
        size_t index = static_cast<size_t>(playlistBox->getSelectedItemIndex());
        if (audioPlaylist.removeTrack(index)) {
            updatePlaylistDisplay();
            syncPlaylistToVideoProcessor();
            statusLabel->setText("Track removed from playlist");
        }
    } else {
        statusLabel->setText("Select a track to remove");
    }
}

void GUI::clearPlaylist() {
    audioPlaylist.clear();
    updatePlaylistDisplay();
    syncPlaylistToVideoProcessor();
    statusLabel->setText("Playlist cleared");
}

void GUI::savePlaylist() {
    auto fileDialog = tgui::ChildWindow::create("Save Playlist");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");

    auto pathLabel = tgui::Label::create("Save as:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);

    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    pathEdit->setText("audio_playlist.json");
    fileDialog->add(pathEdit);

    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([pathEdit]() {
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file name with prompt \"Save Playlist\" default name \"audio_playlist.json\")' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);

    auto saveBtn = tgui::Button::create("Save");
    saveBtn->setSize("30%", "15%");
    saveBtn->setPosition("5%", "75%");
    saveBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        if (audioPlaylist.saveToJson(path)) {
            statusLabel->setText("Playlist saved: " + path);
        } else {
            statusLabel->setText("Failed to save playlist");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(saveBtn);

    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);

    gui.add(fileDialog);
}

void GUI::loadPlaylist() {
    auto fileDialog = tgui::ChildWindow::create("Load Playlist");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");

    auto pathLabel = tgui::Label::create("File path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);

    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    pathEdit->setText("audio_playlist.json");
    fileDialog->add(pathEdit);

    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([pathEdit]() {
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file with prompt \"Select Playlist File\" of type {\"public.json\"})' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);

    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("30%", "15%");
    loadBtn->setPosition("5%", "75%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        stopLivePreview();
        std::string path = pathEdit->getText().toStdString();
        if (audioPlaylist.loadFromJson(path)) {
            updatePlaylistDisplay();
            syncPlaylistToVideoProcessor();
            statusLabel->setText("Playlist loaded: " + path);
        } else {
            statusLabel->setText("Failed to load playlist");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(loadBtn);

    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);

    gui.add(fileDialog);
}

void GUI::updatePlaylistDisplay() {
    playlistBox->removeAllItems();
    for (size_t i = 0; i < audioPlaylist.getTrackCount(); ++i) {
        const auto& track = audioPlaylist.getTrack(i);
        std::string filename = track.filePath.substr(track.filePath.find_last_of("/\\") + 1);
        float duration = static_cast<float>(track.endIndex - track.startIndex) / track.sampleRate;
        playlistBox->addItem(std::to_string(i + 1) + ". " + filename + " (" + 
                            std::to_string(static_cast<int>(duration)) + "s)");
    }
    
    // Update audio position slider if playlist has audio
    if (audioPlaylist.getAudioBuffer()) {
        float totalDuration = audioPlaylist.getTotalDuration();
        audioPositionLabel->setText("0.0s / " + std::to_string(static_cast<int>(totalDuration)) + "s");
    }
}

void GUI::syncPlaylistToVideoProcessor() {
    // This is a workaround since VideoProcessor stores its own AudioBuffer
    // For now, we'll need to manually load the combined audio from playlist
    // In a better design, VideoProcessor could accept an external AudioBuffer
    
    if (audioPlaylist.getAudioBuffer()) {
        // We can't directly assign the playlist's buffer to VideoProcessor
        // But the playlist's buffer can be accessed when processing
        std::cout << "Playlist synced: " << audioPlaylist.getTrackCount() << " tracks, "
                  << "total duration: " << audioPlaylist.getTotalDuration() << "s" << std::endl;
    }
}

void GUI::saveEffectChain() {
    auto fileDialog = tgui::ChildWindow::create("Save Effect Chain");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Save as:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    pathEdit->setText("effect_chain.json");
    fileDialog->add(pathEdit);
    
    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([pathEdit]() {
        // Use macOS file picker for save location
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file name with prompt \"Save Effect Chain\" default name \"effect_chain.json\")' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);
    
    auto saveBtn = tgui::Button::create("Save");
    saveBtn->setSize("30%", "15%");
    saveBtn->setPosition("5%", "75%");
    saveBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        bool saveAutomation = fxChainAutomationToggle && fxChainAutomationToggle->isChecked();
        bool ok = false;
        if (saveAutomation && automationWindow) {
            try {
                json root;
                root["format"] = "vidmod-chain-bundle-v1";
                root["effectChain"] = json::parse(effectChain.toJsonString());
                std::string automationText = automationWindow->exportAutomationJson();
                root["automation"] = automationText.empty() ? json::object() : json::parse(automationText);

                std::ofstream out(path);
                if (out.is_open()) {
                    out << root.dump(2);
                    ok = true;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to save chain bundle: " << e.what() << std::endl;
                ok = false;
            }
        } else {
            ok = effectChain.saveToJson(path);
        }

        if (ok) {
            statusLabel->setText(saveAutomation ? "Chain + automation saved: " + path : "Chain saved: " + path);
        } else {
            statusLabel->setText(saveAutomation ? "Failed to save chain + automation" : "Failed to save chain");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(saveBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::loadEffectChain() {
    auto fileDialog = tgui::ChildWindow::create("Load Effect Chain");
    fileDialog->setSize("500", "180");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("File path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    pathEdit->setText("effect_chain.json");
    fileDialog->add(pathEdit);
    
    auto browseBtn = tgui::Button::create("Browse...");
    browseBtn->setSize("30%", "15%");
    browseBtn->setPosition("5%", "55%");
    browseBtn->onPress([pathEdit]() {
        // Use macOS file picker
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file with prompt \"Select Effect Chain File\" of type {\"public.json\"})' 2>/dev/null", "r");
        if (pipe) {
            char buffer[1024];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            pclose(pipe);
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            if (!result.empty()) {
                pathEdit->setText(result);
            }
        }
    });
    fileDialog->add(browseBtn);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("30%", "15%");
    loadBtn->setPosition("5%", "75%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        bool loadAutomation = fxChainAutomationToggle && fxChainAutomationToggle->isChecked();
        bool ok = false;
        bool loadedAutomation = false;

        if (loadAutomation) {
            try {
                std::ifstream in(path);
                if (in.is_open()) {
                    json root;
                    in >> root;

                    if (root.contains("effectChain")) {
                        ok = effectChain.fromJsonString(root["effectChain"].dump());
                        if (ok && root.contains("automation") && automationWindow) {
                            loadedAutomation = automationWindow->importAutomationJson(root["automation"].dump());
                        }
                    } else {
                        // Backward-compatible fallback for plain chain JSON files.
                        ok = effectChain.fromJsonString(root.dump());
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to load chain bundle: " << e.what() << std::endl;
                ok = false;
            }
        } else {
            ok = effectChain.loadFromJson(path);
        }

        if (ok) {
            updateChainList();
            updateParameterPanel();
            if (loadAutomation) {
                statusLabel->setText(loadedAutomation ? "Chain + automation loaded: " + path : "Chain loaded (no automation in file): " + path);
            } else {
                statusLabel->setText("Chain loaded: " + path);
            }
        } else {
            statusLabel->setText(loadAutomation ? "Failed to load chain bundle" : "Failed to load chain");
        }
        gui.remove(fileDialog);
    });
    fileDialog->add(loadBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("30%", "15%");
    cancelBtn->setPosition("65%", "75%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::processImageLoop() {
    std::cout << "processImageLoop() called" << std::endl;
    std::cout << "currentImagePath: " << currentImagePath << std::endl;
    
    // Create a dialog for image loop parameters
    auto paramDialog = tgui::ChildWindow::create("Process Image Loop");
    paramDialog->setSize("450", "250");
    paramDialog->setPosition("(&.size - size) / 2");
    
    // Show loaded image
    auto imageLabel = tgui::Label::create("Image: " + currentImagePath.substr(currentImagePath.find_last_of("/\\") + 1));
    imageLabel->setPosition("5%", "5%");
    imageLabel->setTextSize(14);
    paramDialog->add(imageLabel);
    
    // Duration
    auto durationLabel = tgui::Label::create("Duration (seconds):");
    durationLabel->setPosition("5%", "15%");
    durationLabel->setTextSize(14);
    paramDialog->add(durationLabel);
    
    auto durationEdit = tgui::EditBox::create();
    durationEdit->setSize("90%", "12%");
    durationEdit->setPosition("5%", "23%");
    
    // Calculate default duration from audio if available (prefer playlist)
    std::string defaultDuration = "10.0";
    AudioBuffer* audioForDuration = audioPlaylist.getAudioBuffer() ? 
                                   audioPlaylist.getAudioBuffer() : 
                                   videoProcessor.getAudioBuffer();
    if (audioForDuration) {
        float audioDuration = static_cast<float>(audioForDuration->size()) / 
                             audioForDuration->getSampleRate();
        defaultDuration = std::to_string(audioDuration);
    }
    durationEdit->setText(defaultDuration);
    paramDialog->add(durationEdit);
    
    // FPS
    auto fpsLabel = tgui::Label::create("FPS:");
    fpsLabel->setPosition("5%", "40%");
    fpsLabel->setTextSize(14);
    paramDialog->add(fpsLabel);
    
    auto fpsEdit = tgui::EditBox::create();
    fpsEdit->setSize("90%", "12%");
    fpsEdit->setPosition("5%", "48%");
    fpsEdit->setText("30.0");
    paramDialog->add(fpsEdit);
    
    // Output path
    auto outputLabel = tgui::Label::create("Output path:");
    outputLabel->setPosition("5%", "65%");
    outputLabel->setTextSize(14);
    paramDialog->add(outputLabel);
    
    auto outputEdit = tgui::EditBox::create();
    outputEdit->setSize("90%", "12%");
    outputEdit->setPosition("5%", "73%");
    outputEdit->setText("image_loop_output.mp4");
    paramDialog->add(outputEdit);
    
    // Process button
    auto processBtn = tgui::Button::create("Process");
    processBtn->setSize("40%", "12%");
    processBtn->setPosition("5%", "88%");
    processBtn->onPress([this, paramDialog, durationEdit, fpsEdit, outputEdit]() {
        try {
            std::string durationStr = durationEdit->getText().toStdString();
            std::string fpsStr = fpsEdit->getText().toStdString();
            std::string outputPath = outputEdit->getText().toStdString();
            
            if (durationStr.empty()) durationStr = "10.0";
            if (fpsStr.empty()) fpsStr = "30.0";
            if (outputPath.empty()) outputPath = "image_loop_output.mp4";
            
            float duration = std::stof(durationStr);
            float fps = std::stof(fpsStr);
            
            gui.remove(paramDialog);

            // Use playlist audio if available, otherwise use VideoProcessor's audio
            AudioBuffer* audioToUse = audioPlaylist.getAudioBuffer() ? 
                                     audioPlaylist.getAudioBuffer() : 
                                     videoProcessor.getAudioBuffer();

            processImageLoopThreaded(outputPath, duration, fps, audioToUse);
        } catch (const std::exception& e) {
            statusLabel->setText("Error: Invalid input values");
            gui.remove(paramDialog);
        }
    });
    paramDialog->add(processBtn);
    
    // Cancel button
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("40%", "12%");
    cancelBtn->setPosition("55%", "88%");
    cancelBtn->onPress([this, paramDialog]() {
        gui.remove(paramDialog);
    });
    paramDialog->add(cancelBtn);
    
    gui.add(paramDialog);
}

bool GUI::renderImageLoopWithAutomation(const std::string& outputPath, float duration, float fps, AudioBuffer* audioToUse) {
    if (loadedImage.empty()) {
        std::cerr << "Error: No image loaded for image loop render" << std::endl;
        return false;
    }

    if (duration <= 0.0f || fps <= 0.0f) {
        std::cerr << "Error: Invalid image loop duration or fps" << std::endl;
        return false;
    }

    const int frameCount = std::max(1, static_cast<int>(std::lround(duration * fps)));
    const bool hasAudioMuxStep = (audioToUse != nullptr);
    totalProcessingFrames = frameCount + (hasAudioMuxStep ? 1 : 0);
    currentProcessingFrame = 0;
    syncAutomationTimeline(fps, audioToUse, duration);

    cv::VideoWriter writer(outputPath,
                           cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                           fps,
                           cv::Size(loadedImage.cols, loadedImage.rows));

    if (!writer.isOpened()) {
        std::cerr << "Error: Could not open video writer for: " << outputPath << std::endl;
        return false;
    }

    // Capture render range for this render pass
    const float rsStart = renderRangeStart;
    const float rsRange = std::max(0.001f, renderRangeEnd - renderRangeStart);

    if (audioToUse) {
        size_t audioStart = static_cast<size_t>(rsStart * audioToUse->size());
        audioToUse->setIndex(audioStart);
    }

    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        if (shouldStopProcessing) {
            break;
        }

        if (audioToUse) {
            double progress = static_cast<double>(frameIndex) / frameCount;
            size_t audioPos = static_cast<size_t>((rsStart + progress * rsRange) * audioToUse->size());
            audioToUse->setIndex(audioPos);
        }

        applyAutomationAtFrame(mapRenderFrameToAutomationFrame(frameIndex, frameCount));
        cv::Mat processedFrame = effectChain.applyEffects(loadedImage.clone(), audioToUse, fps);
        writer.write(processedFrame);
        currentProcessingFrame = frameIndex + 1;

        if ((frameIndex + 1) % 5 == 0) {
            std::lock_guard<std::mutex> lock(previewMutex);
            latestProcessedFrame = processedFrame.clone();
        }
    }

    writer.release();

    if (shouldStopProcessing) {
        isAudioMuxing = false;
        return false;
    }

    if (!audioToUse) {
        isAudioMuxing = false;
        return true;
    }

    // Keep progress one step short while muxing runs.
    currentProcessingFrame = frameCount;
    isAudioMuxing = true;

    std::string tempAudioPath = outputPath + ".temp_audio.wav";
    if (!audioToUse->saveToWAV(tempAudioPath)) {
        std::cerr << "Failed to save audio to WAV file" << std::endl;
        isAudioMuxing = false;
        return true;
    }

    std::string tempVideoPath = outputPath + ".temp_video.mp4";
    if (rename(outputPath.c_str(), tempVideoPath.c_str()) != 0) {
        std::cerr << "Failed to rename video file. Error: " << strerror(errno) << std::endl;
        remove(tempAudioPath.c_str());
        isAudioMuxing = false;
        return true;
    }

    std::string ffmpegProgressPath = outputPath + ".ffmpeg_progress.txt";
    std::string ffmpegLogPath = outputPath + ".ffmpeg.log";
    std::string ffmpegCmd = "ffmpeg -y -hide_banner -loglevel error -i \"" + tempVideoPath +
        "\" -i \"" + tempAudioPath + "\" -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 192k -shortest "
        "-disposition:a:0 default \"" + outputPath + "\" > \"" + ffmpegLogPath + "\" 2>&1 < /dev/null";

    // Restart progress bar for muxing stage and update via -progress option.
    totalProcessingFrames = 100;
    currentProcessingFrame = 0;
    int result = runFfmpegWithProgressPipe(ffmpegCmd, ffmpegProgressPath, currentProcessingFrame, totalProcessingFrames);

    remove(tempAudioPath.c_str());
    std::filesystem::remove(ffmpegProgressPath);

    if (result != 0) {
        std::cerr << "FFmpeg muxing failed with code: " << result << std::endl;
        if (rename(tempVideoPath.c_str(), outputPath.c_str()) != 0) {
            std::cerr << "Failed to restore original video after mux failure. Error: " << strerror(errno) << std::endl;
        }
    } else {
        remove(tempVideoPath.c_str());
    }

    isAudioMuxing = false;

    return true;
}

void GUI::processImageLoopThreaded(const std::string& outputPath, float duration, float fps, AudioBuffer* audioToUse) {
    // Stop any existing processing thread
    if (processingThread && processingThread->joinable()) {
        shouldStopProcessing = true;
        processingThread->join();
    }

    shouldStopProcessing = false;
    isProcessing = true;
    isAudioMuxing = false;
    currentProcessingFrame = 0;
    totalProcessingFrames = std::max(1, static_cast<int>(std::lround(duration * fps)));

    statusLabel->setText("Processing image loop...");

    processingThread = std::make_unique<std::thread>([this, outputPath, duration, fps, audioToUse]() {
        try {
            bool ok = renderImageLoopWithAutomation(outputPath, duration, fps, audioToUse);
            if (!ok && !shouldStopProcessing) {
                std::cerr << "Image loop processing failed for output: " << outputPath << std::endl;
            }
            isProcessing = false;
        } catch (const std::exception& e) {
            std::cerr << "Image loop processing error: " << e.what() << std::endl;
            isProcessing = false;
        }
    });
}

void GUI::generatePreview() {
    // Determine which audio buffer to use (playlist takes priority)
    AudioBuffer* activeAudioBuffer = audioPlaylist.getAudioBuffer() ? 
                                     audioPlaylist.getAudioBuffer() : 
                                     videoProcessor.getAudioBuffer();
    
    // Check if we have a loaded image first
    try {
        if (!loadedImage.empty()) {
            statusLabel->setText("Generating preview...");
            cv::Mat previewFrame = loadedImage.clone();
            const float previewFps = 30.0f;
            syncAutomationTimeline(previewFps, activeAudioBuffer);
            int automationFrame = getAutomationFrameForPosition(previewFps, activeAudioBuffer);
            
            // Set audio buffer position based on slider
            if (activeAudioBuffer) {
                size_t audioPos = static_cast<size_t>(currentAudioPosition * activeAudioBuffer->size());
                activeAudioBuffer->setIndex(audioPos);
                std::cout << "Preview using audio position: " << currentAudioPosition 
                         << " (sample " << audioPos << ", automation frame " << automationFrame << ")" << std::endl;
            }
            
            applyAutomationAtFrame(automationFrame);
            updateParameterDisplayValues();
            
            cv::Mat processedFrame = effectChain.applyEffects(previewFrame, activeAudioBuffer, previewFps);
            updatePreview(processedFrame);
            statusLabel->setText("Preview generated");
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in image preview: " << e.what() << std::endl;
        statusLabel->setText("Error generating preview");
        return;
    }
    
    // Otherwise use video processor
    if (!activeAudioBuffer) {
        statusLabel->setText("No audio loaded");
        return;
    }
    
    // Calculate which video frame to show based on audio position
    float previewFps = videoProcessor.getFPS() > 0.0f ? videoProcessor.getFPS() : 30.0f;
    syncAutomationTimeline(previewFps, activeAudioBuffer);
    float audioDuration = static_cast<float>(activeAudioBuffer->size()) / 
                         activeAudioBuffer->getSampleRate();
    float targetTime = currentAudioPosition * audioDuration;
    int automationFrame = getAutomationFrameForPosition(previewFps, activeAudioBuffer);
    int targetFrame = automationFrame;
    
    // Loop video if needed (if video is shorter than audio)
    if (videoProcessor.getTotalFrames() > 0) {
        targetFrame = targetFrame % videoProcessor.getTotalFrames();
    }
    
    std::cout << "Preview at audio position " << currentAudioPosition 
              << " (" << targetTime << "s, automation frame " << automationFrame
              << ", video frame " << targetFrame << ")" << std::endl;
    
    cv::Mat frame = videoProcessor.getFrameAt(targetFrame);
    
    if (frame.empty()) {
        statusLabel->setText("No video loaded");
        return;
    }
    
    statusLabel->setText("Generating preview...");
    
    // Set audio buffer position
    size_t audioPos = static_cast<size_t>(currentAudioPosition * activeAudioBuffer->size());
    activeAudioBuffer->setIndex(audioPos);
    
    // Apply automation for this frame
    applyAutomationAtFrame(automationFrame);
    updateParameterDisplayValues();
    
    cv::Mat processedFrame = effectChain.applyEffects(frame, activeAudioBuffer, previewFps);
    updatePreview(processedFrame);
    
    statusLabel->setText("Preview generated");
}

void GUI::startLivePreview() {
    if (isLivePreviewPlaying) {
        return;
    }

    stopLivePreview();

    shouldStopLivePreview = false;
    isLivePreviewPlaying = true;
    livePreviewAudioDurationSeconds = 0.0f;

    // Prepare audio playback for the selected play range.
    AudioBuffer* activeAudioBuffer = audioPlaylist.getAudioBuffer() ?
                                     audioPlaylist.getAudioBuffer() :
                                     videoProcessor.getAudioBuffer();
    if (activeAudioBuffer && activeAudioBuffer->size() > 0 && activeAudioBuffer->getSampleRate() > 0) {
        const auto& src = activeAudioBuffer->getData();
        const size_t srcSize = src.size();

        size_t startIndex = static_cast<size_t>(renderRangeStart * srcSize);
        size_t endIndex = static_cast<size_t>(renderRangeEnd * srcSize);
        startIndex = std::min(startIndex, srcSize - 1);
        endIndex = std::min(endIndex, srcSize);
        if (endIndex <= startIndex) {
            endIndex = std::min(srcSize, startIndex + 1);
        }

        std::vector<std::int16_t> intSamples;
        intSamples.reserve(endIndex - startIndex);
        for (size_t i = startIndex; i < endIndex; ++i) {
            float s = std::clamp(src[i], -1.0f, 1.0f);
            intSamples.push_back(static_cast<std::int16_t>(s * 32767.0f));
        }

        if (!intSamples.empty()) {
            auto buffer = std::make_unique<sf::SoundBuffer>();
            if (buffer->loadFromSamples(intSamples.data(),
                                        intSamples.size(),
                                        1,
                                        activeAudioBuffer->getSampleRate(),
                                        std::vector<sf::SoundChannel>{sf::SoundChannel::Mono})) {
                livePreviewSoundBuffer = std::move(buffer);
                livePreviewSound = std::make_unique<sf::Sound>(*livePreviewSoundBuffer);
                livePreviewSound->play();
                livePreviewAudioDurationSeconds = static_cast<float>(intSamples.size()) /
                                                  static_cast<float>(activeAudioBuffer->getSampleRate());
                livePreviewAudioStartTime = std::chrono::steady_clock::now();
            }
        }
    }

    statusLabel->setText("Live preview playing...");
    livePreviewThread = std::make_unique<std::thread>([this]() {
        livePreviewLoop();
    });
}

void GUI::stopLivePreview() {
    shouldStopLivePreview = true;
    isLivePreviewPlaying = false;

    if (livePreviewSound) {
        livePreviewSound->stop();
    }
    livePreviewSound.reset();
    livePreviewSoundBuffer.reset();
    livePreviewAudioDurationSeconds = 0.0f;

    if (livePreviewThread && livePreviewThread->joinable()) {
        livePreviewThread->join();
    }
    livePreviewThread.reset();
}

void GUI::livePreviewLoop() {
    try {
        const float previewFps = 15.0f;
        const auto frameDelay = std::chrono::milliseconds(static_cast<int>(1000.0f / previewFps));

        AudioBuffer* activeAudioBuffer = audioPlaylist.getAudioBuffer() ?
                                         audioPlaylist.getAudioBuffer() :
                                         videoProcessor.getAudioBuffer();

        const bool hasImage = !loadedImage.empty();
        const bool hasVideo = !videoProcessor.getVideoPath().empty();

        if (!hasImage && !hasVideo) {
            isLivePreviewPlaying = false;
            return;
        }

        int frameStep = 0;
        while (!shouldStopLivePreview) {
            const float rangeLen = std::max(0.001f, renderRangeEnd - renderRangeStart);
            float progress = 0.0f;
            if (livePreviewAudioDurationSeconds > 0.0f) {
                float elapsed = std::chrono::duration<float>(
                    std::chrono::steady_clock::now() - livePreviewAudioStartTime).count();
                if (elapsed >= livePreviewAudioDurationSeconds) {
                    break;
                }
                progress = std::clamp(elapsed / livePreviewAudioDurationSeconds, 0.0f, 1.0f);
            } else {
                progress = std::fmod(frameStep * 0.02f, 1.0f);
            }

            float playPos = renderRangeStart + (progress * rangeLen);
            currentAudioPosition = playPos;

            if (activeAudioBuffer && activeAudioBuffer->size() > 0) {
                size_t audioPos = static_cast<size_t>(playPos * activeAudioBuffer->size());
                audioPos = std::min(audioPos, activeAudioBuffer->size() - 1);
                activeAudioBuffer->setIndex(audioPos);
            }

            cv::Mat source;
            if (hasImage) {
                source = loadedImage;
            } else {
                int totalFrames = std::max(1, videoProcessor.getTotalFrames());
                int frameIndex = static_cast<int>(playPos * (totalFrames - 1));
                source = videoProcessor.getFrameAt(frameIndex);
            }

            if (source.empty()) {
                std::this_thread::sleep_for(frameDelay);
                frameStep++;
                continue;
            }

            const int maxPreviewDim = 540;
            cv::Mat lowResSource;
            float scale = std::min(static_cast<float>(maxPreviewDim) / source.cols,
                                   static_cast<float>(maxPreviewDim) / source.rows);
            if (scale < 1.0f) {
                cv::resize(source, lowResSource, cv::Size(), scale, scale, cv::INTER_LINEAR);
            } else {
                lowResSource = source;
            }

            syncAutomationTimeline(previewFps, activeAudioBuffer);
            applyAutomationAtFrame(getAutomationFrameForPosition(previewFps, activeAudioBuffer));
            cv::Mat processed = effectChain.applyEffects(lowResSource.clone(), activeAudioBuffer, previewFps);

            {
                std::lock_guard<std::mutex> lock(previewMutex);
                latestProcessedFrame = processed;
            }

            std::this_thread::sleep_for(frameDelay);
            frameStep++;
        }
    } catch (const std::exception& e) {
        std::cerr << "Live preview error: " << e.what() << std::endl;
    }

    isLivePreviewPlaying = false;

    if (livePreviewSound) {
        livePreviewSound->stop();
    }
    livePreviewSound.reset();
    livePreviewSoundBuffer.reset();
    livePreviewAudioDurationSeconds = 0.0f;
}

void GUI::processVideo() {
    std::cout << "processVideo() called" << std::endl;
    std::cout << "loadedImage empty: " << loadedImage.empty() << std::endl;
    
    // Check if we're in image loop mode
    if (!loadedImage.empty()) {
        std::cout << "Calling processImageLoop()" << std::endl;
        processImageLoop();
        return;
    }
    
    std::cout << "Opening video processing dialog" << std::endl;
    
    // Create a dialog for video processing parameters (similar to image loop)
    auto paramDialog = tgui::ChildWindow::create("Process Video");
    paramDialog->setSize("450", "250");
    paramDialog->setPosition("(&.size - size) / 2");
    
    // Show loaded video info
    if (!videoProcessor.getVideoPath().empty()) {
        auto videoLabel = tgui::Label::create("Video: " + videoProcessor.getVideoPath().substr(videoProcessor.getVideoPath().find_last_of("/\\") + 1));
        videoLabel->setPosition("5%", "5%");
        videoLabel->setTextSize(14);
        paramDialog->add(videoLabel);
    }
    
    // Duration setting
    auto durationLabel = tgui::Label::create("Duration (seconds):");
    durationLabel->setPosition("5%", "15%");
    durationLabel->setTextSize(14);
    paramDialog->add(durationLabel);
    
    auto durationEdit = tgui::EditBox::create();
    durationEdit->setSize("90%", "12%");
    durationEdit->setPosition("5%", "23%");
    
    // Calculate default duration from audio if available (prefer playlist)
    std::string defaultDuration = "10.0";
    AudioBuffer* audioForDuration = audioPlaylist.getAudioBuffer() ? 
                                   audioPlaylist.getAudioBuffer() : 
                                   videoProcessor.getAudioBuffer();
    if (audioForDuration) {
        float audioDuration = static_cast<float>(audioForDuration->size()) / 
                             audioForDuration->getSampleRate();
        defaultDuration = std::to_string(audioDuration);
    }
    durationEdit->setText(defaultDuration);
    paramDialog->add(durationEdit);
    
    // Output path
    auto outputLabel = tgui::Label::create("Output path:");
    outputLabel->setPosition("5%", "45%");
    outputLabel->setTextSize(14);
    paramDialog->add(outputLabel);
    
    auto outputEdit = tgui::EditBox::create();
    outputEdit->setSize("90%", "15%");
    outputEdit->setPosition("5%", "55%");
    outputEdit->setText("processed_output.mp4");
    paramDialog->add(outputEdit);
    
    // Process button
    auto processBtn = tgui::Button::create("Process");
    processBtn->setSize("40%", "15%");
    processBtn->setPosition("5%", "80%");
    processBtn->onPress([this, paramDialog, durationEdit, outputEdit]() {
        try {
            std::string durationStr = durationEdit->getText().toStdString();
            std::string outputPath = outputEdit->getText().toStdString();
            
            if (durationStr.empty()) durationStr = "10.0";
            if (outputPath.empty()) outputPath = "processed_output.mp4";
            
            float duration = std::stof(durationStr);
            
            gui.remove(paramDialog);
            
            // Use playlist audio if available, otherwise use VideoProcessor's audio
            AudioBuffer* audioToUse = audioPlaylist.getAudioBuffer() ? 
                                     audioPlaylist.getAudioBuffer() : 
                                     videoProcessor.getAudioBuffer();
            
            // Start processing in separate thread
            processVideoThreaded(outputPath, duration, audioToUse);
            
        } catch (const std::exception& e) {
            statusLabel->setText("Error: Invalid input values");
            gui.remove(paramDialog);
        }
    });
    paramDialog->add(processBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("40%", "15%");
    cancelBtn->setPosition("55%", "80%");
    cancelBtn->onPress([this, paramDialog]() {
        gui.remove(paramDialog);
    });
    paramDialog->add(cancelBtn);
    
    gui.add(paramDialog);
}

void GUI::updatePreview(const cv::Mat& frame) {
    if (frame.empty()) {
        std::cerr << "updatePreview: frame is empty!" << std::endl;
        return;
    }
    
    std::cout << "updatePreview: frame " << frame.cols << "x" << frame.rows << " type=" << frame.type() << std::endl;
    
    // Convert OpenCV Mat to RGBA for SFML (SFML expects 4 channels)
    cv::Mat rgbaFrame;
    cv::cvtColor(frame, rgbaFrame, cv::COLOR_BGR2RGBA);
    
    // Ensure the Mat is continuous in memory
    if (!rgbaFrame.isContinuous()) {
        rgbaFrame = rgbaFrame.clone();
    }
    
    // Check if conversion worked
    if (rgbaFrame.empty()) {
        std::cerr << "updatePreview: rgbaFrame is empty after cvtColor!" << std::endl;
        return;
    }
    
    std::cout << "RGBA frame: " << rgbaFrame.cols << "x" << rgbaFrame.rows 
              << " type=" << rgbaFrame.type() 
              << " channels=" << rgbaFrame.channels()
              << " continuous=" << rgbaFrame.isContinuous() << std::endl;
    
    // Create/resize texture
    if (!previewTexture.resize({static_cast<unsigned int>(rgbaFrame.cols), static_cast<unsigned int>(rgbaFrame.rows)})) {
        std::cerr << "Failed to resize texture!" << std::endl;
        return;
    }
    
    // Update texture with image data
    previewTexture.update(rgbaFrame.data);
    previewSprite.setTexture(previewTexture, true); // true = reset texture rect
    
    // Scale to fit preview area
    float scaleX = (window.getSize().x * 0.38f) / rgbaFrame.cols;
    float scaleY = (window.getSize().y * 0.75f) / rgbaFrame.rows;
    float scale = std::min(scaleX, scaleY);
    
    previewSprite.setScale({scale, scale});
    previewSprite.setPosition({window.getSize().x * 0.61f, window.getSize().y * 0.06f});
    
    showingPreview = true;
    currentPreviewFrame = frame.clone();
    
    std::cout << "Preview updated successfully. showingPreview=" << showingPreview << std::endl;
}

void GUI::handleEvent(const sf::Event& event) {
    if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePressed->button == sf::Mouse::Button::Left) {
            startListDrag(sf::Vector2f(static_cast<float>(mousePressed->position.x),
                                       static_cast<float>(mousePressed->position.y)));
        }
    }

    if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mouseReleased->button == sf::Mouse::Button::Left) {
            finishListDrag(sf::Vector2f(static_cast<float>(mouseReleased->position.x),
                                        static_cast<float>(mouseReleased->position.y)));
        }
    }

    gui.handleEvent(event);
}

int GUI::getListBoxIndexAtPosition(const tgui::ListBox::Ptr& listBox, sf::Vector2f mousePos) const {
    if (!listBox) return -1;

    const int itemCount = static_cast<int>(listBox->getItemCount());
    if (itemCount <= 0) return -1;

    const sf::Vector2f pos = listBox->getAbsolutePosition();
    const sf::Vector2f size = listBox->getSize();
    if (mousePos.x < pos.x || mousePos.x > (pos.x + size.x) ||
        mousePos.y < pos.y || mousePos.y > (pos.y + size.y)) {
        return -1;
    }

    const float itemHeight = static_cast<float>(listBox->getItemHeight());
    if (itemHeight <= 0.0f) {
        return -1;
    }

    const float relativeY = mousePos.y - pos.y;
    const int topItemOffset = static_cast<int>(listBox->getScrollbar()->getValue());
    const int index = topItemOffset + static_cast<int>(relativeY / itemHeight);
    return std::clamp(index, 0, itemCount - 1);
}

void GUI::startListDrag(sf::Vector2f mousePos) {
    dragSourceChainIndex = getListBoxIndexAtPosition(chainList, mousePos);
    if (dragSourceChainIndex >= 0) {
        isDraggingChainItem = true;
        return;
    }

    dragSourcePlaylistIndex = getListBoxIndexAtPosition(playlistBox, mousePos);
    if (dragSourcePlaylistIndex >= 0) {
        isDraggingPlaylistItem = true;
    }
}

void GUI::finishListDrag(sf::Vector2f mousePos) {
    if (isDraggingChainItem) {
        const int targetIndex = getListBoxIndexAtPosition(chainList, mousePos);
        if (dragSourceChainIndex >= 0 && targetIndex >= 0 && dragSourceChainIndex != targetIndex) {
            effectChain.moveEffect(static_cast<size_t>(dragSourceChainIndex), static_cast<size_t>(targetIndex));
            updateChainList();
            chainList->setSelectedItemByIndex(targetIndex);
            updateParameterPanel();
            statusLabel->setText("Effect reordered");
        }
    }

    if (isDraggingPlaylistItem) {
        const int targetIndex = getListBoxIndexAtPosition(playlistBox, mousePos);
        if (dragSourcePlaylistIndex >= 0 && targetIndex >= 0 && dragSourcePlaylistIndex != targetIndex) {
            stopLivePreview();
            if (audioPlaylist.moveTrack(static_cast<size_t>(dragSourcePlaylistIndex), static_cast<size_t>(targetIndex))) {
                updatePlaylistDisplay();
                playlistBox->setSelectedItemByIndex(targetIndex);
                syncPlaylistToVideoProcessor();
                statusLabel->setText("Playlist reordered");
            }
        }
    }

    isDraggingChainItem = false;
    isDraggingPlaylistItem = false;
    dragSourceChainIndex = -1;
    dragSourcePlaylistIndex = -1;
}

void GUI::draw() {
    // Update processing progress if active
    if (isProcessing) {
        updateProcessingProgress();
    }

    // Live preview uses latestProcessedFrame even when not in render-processing mode.
    {
        std::lock_guard<std::mutex> lock(previewMutex);
        if (!latestProcessedFrame.empty() && !isProcessing) {
            updatePreview(latestProcessedFrame);
            latestProcessedFrame.release();
        }
    }

    if (livePreviewStateLabel) {
        livePreviewStateLabel->setText(isLivePreviewPlaying ? "Live: Playing" : "Live: Stopped");
    }
    
    gui.draw();
    
    if (showingPreview) {
        // Ensure proper blend mode for drawing the sprite
        sf::RenderStates states = sf::RenderStates::Default;
        window.draw(previewSprite, states);
    }
    
    // Update parameter display values during playback/rendering to reflect automation
    // This works for live preview, image loop rendering, and video rendering with automation
    if (isLivePreviewPlaying || isProcessing) {
        updateParameterDisplayValues();
    }
    
    // Update automation window if open
    updateAutomationWindow();
}

void GUI::processVideoThreaded(const std::string& outputPath, float duration, AudioBuffer* audioToUse) {
    // Stop any existing processing thread
    if (processingThread && processingThread->joinable()) {
        shouldStopProcessing = true;
        processingThread->join();
    }
    
    shouldStopProcessing = false;
    isProcessing = true;
    isAudioMuxing = false;
    currentProcessingFrame = 0;
    
    statusLabel->setText("Starting processing...");
    
    // Launch processing in separate thread
    processingThread = std::make_unique<std::thread>([this, outputPath, duration, audioToUse]() {
        try {
            if (videoProcessor.getVideoPath().empty()) {
                std::lock_guard<std::mutex> lock(previewMutex);
                isProcessing = false;
                return;
            }
            
            float fps = videoProcessor.getFPS();
            int width = videoProcessor.getWidth();
            int height = videoProcessor.getHeight();
            int totalFrames = videoProcessor.getTotalFrames();
            
            // Create VideoBuffer with disk-based buffering (100 frames at a time)
            std::cout << "Creating VideoBuffer with disk-based buffering..." << std::endl;
            VideoBuffer videoBuffer(videoProcessor.getVideoPath(), 100);
            
            if (videoBuffer.getTotalFrames() == 0) {
                std::cerr << "ERROR: Could not open video file!" << std::endl;
                std::lock_guard<std::mutex> lock(previewMutex);
                isProcessing = false;
                return;
            }
            
            std::cout << "VideoBuffer created for " << videoBuffer.getTotalFrames() << " frames" << std::endl;
            
            // Calculate target frame count
            int targetFrames = totalFrames;
            float videoDuration = totalFrames / fps;
            float audioDuration = 0.0f;
            
            if (duration > 0) {
                targetFrames = static_cast<int>(duration * fps);
                std::cout << "Using specified duration: " << duration << "s = " << targetFrames << " frames" << std::endl;
            } else if (audioToUse) {
                audioDuration = static_cast<float>(audioToUse->size()) / audioToUse->getSampleRate();
                int audioFrames = static_cast<int>(audioDuration * fps);
                targetFrames = std::max(totalFrames, audioFrames);
                
                std::cout << "Video duration: " << videoDuration << "s (" << totalFrames << " frames)" << std::endl;
                std::cout << "Audio duration: " << audioDuration << "s (" << audioFrames << " frames)" << std::endl;
                
                if (totalFrames > audioFrames) {
                    std::cout << "Video is LONGER - audio will loop" << std::endl;
                } else if (audioFrames > totalFrames) {
                    std::cout << "Audio is LONGER - video will loop (VideoBuffer)" << std::endl;
                } else {
                    std::cout << "Video and audio have EQUAL duration" << std::endl;
                }
                std::cout << "Target frames: " << targetFrames << std::endl;
            }
            
            const bool hasAudioMuxStep = (audioToUse != nullptr);
            totalProcessingFrames = targetFrames + (hasAudioMuxStep ? 1 : 0);
            
            std::cout << "Creating VideoWriter for " << targetFrames << " frames at " << fps << " fps" << std::endl;
            std::cout << "Output path: " << outputPath << std::endl;
            
            // Create video writer
            int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
            cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
            
            if (!writer.isOpened()) {
                std::cerr << "ERROR: Could not open video writer!" << std::endl;
                std::lock_guard<std::mutex> lock(previewMutex);
                isProcessing = false;
                return;
            }
            
            std::cout << "VideoWriter opened successfully" << std::endl;
            
            // Reset audio buffer to render range start
            if (audioToUse) {
                size_t audioStart = static_cast<size_t>(renderRangeStart * audioToUse->size());
                audioToUse->setIndex(audioStart);
            }
            
            // Process frames using VideoBuffer (automatic looping)
            int frameCount = 0;
            
            std::cout << "Starting frame processing loop. Target: " << targetFrames << " frames" << std::endl;
            
            while (frameCount < targetFrames && !shouldStopProcessing) {
                // Get next frame from VideoBuffer (automatically loops)
                const cv::Mat& currentFrame = videoBuffer.getFrame();
                
                frameCount++;
                currentProcessingFrame = frameCount;
                
                // Log looping events
                if (frameCount > totalFrames && frameCount % totalFrames == 1) {
                    std::cout << "Video looping - now at frame " << frameCount << " (loop #" << (frameCount / totalFrames + 1) << ")" << std::endl;
                }
                
                // Apply automation across the normalized automation timeline.
                applyAutomationAtFrame(mapRenderFrameToAutomationFrame(frameCount - 1, targetFrames));
                
                // Apply effects
                cv::Mat processedFrame = effectChain.applyEffects(currentFrame, audioToUse, fps);
                writer.write(processedFrame);
                
                // Update preview every 5 frames to avoid overwhelming the GUI
                if (frameCount % 5 == 0) {
                    std::lock_guard<std::mutex> lock(previewMutex);
                    latestProcessedFrame = processedFrame.clone();
                }
            }
            
            std::cout << "Frame processing complete. Total frames written: " << frameCount << std::endl;
            
            writer.release();
            
            std::cout << "Video writer released. Frames written: " << frameCount << std::endl;
            
            // Mux audio if available
            if (audioToUse && !shouldStopProcessing) {
                // Keep one final step reserved for muxing.
                currentProcessingFrame = targetFrames;
                isAudioMuxing = true;
                std::cout << "Starting audio muxing process..." << std::endl;
                std::cout << "Audio buffer size: " << audioToUse->size() << " samples" << std::endl;
                std::string tempAudioPath = outputPath + ".temp_audio.wav";
                std::cout << "Saving audio to: " << tempAudioPath << std::endl;
                
                if (audioToUse->saveToWAV(tempAudioPath)) {
                    std::cout << "Audio saved successfully to: " << tempAudioPath << std::endl;
                    std::string tempVideoPath = outputPath + ".temp_video.mp4";
                    std::cout << "Attempting to rename " << outputPath << " to " << tempVideoPath << std::endl;
                    
                    if (rename(outputPath.c_str(), tempVideoPath.c_str()) == 0) {
                        std::cout << "Video renamed successfully to: " << tempVideoPath << std::endl;
                        // Map streams explicitly and set audio as default.
                        std::string ffmpegProgressPath = outputPath + ".ffmpeg_progress.txt";
                        std::string ffmpegLogPath = outputPath + ".ffmpeg.log";
                        std::string ffmpegCmd = "ffmpeg -y -hide_banner -loglevel error -i \"" + tempVideoPath +
                            "\" -i \"" + tempAudioPath + "\" -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 192k -shortest " +
                            "-disposition:a:0 default \"" + outputPath + "\" > \"" + ffmpegLogPath + "\" 2>&1 < /dev/null";
                        std::cout << "Running FFmpeg with output log: " << ffmpegLogPath << std::endl;

                        // Restart progress bar for muxing stage and update via -progress option.
                        totalProcessingFrames = 100;
                        currentProcessingFrame = 0;
                        int result = runFfmpegWithProgressPipe(ffmpegCmd, ffmpegProgressPath, currentProcessingFrame, totalProcessingFrames);
                        std::cout << "FFmpeg returned code: " << result << std::endl;

                        if (result != 0) {
                            std::cerr << "FFmpeg muxing failed with code: " << result << std::endl;
                            if (rename(tempVideoPath.c_str(), outputPath.c_str()) != 0) {
                                std::cerr << "Failed to restore original video after mux failure. Error: " << strerror(errno) << std::endl;
                            }
                        } else {
                            std::cout << "FFmpeg muxing completed successfully" << std::endl;
                            std::cout << "Removing temp video: " << tempVideoPath << std::endl;
                            remove(tempVideoPath.c_str());
                        }
                        std::filesystem::remove(ffmpegProgressPath);
                    } else {
                        std::cerr << "Failed to rename video file. Error: " << strerror(errno) << std::endl;
                    }
                    std::cout << "Removing temp audio: " << tempAudioPath << std::endl;
                    remove(tempAudioPath.c_str());
                } else {
                    std::cerr << "Failed to save audio to WAV file" << std::endl;
                }
                isAudioMuxing = false;
            } else {
                if (!audioToUse) {
                    std::cout << "No audio buffer available for muxing" << std::endl;
                }
                if (shouldStopProcessing) {
                    std::cout << "Processing was stopped, skipping muxing" << std::endl;
                }
            }
            
            std::cout << "Processing thread finishing..." << std::endl;
            isAudioMuxing = false;
            isProcessing = false;
            
        } catch (const std::exception& e) {
            std::cerr << "Processing error: " << e.what() << std::endl;
            isAudioMuxing = false;
            isProcessing = false;
        }
    });
}

void GUI::updateProcessingProgress() {
    // Update preview with latest processed frame
    {
        std::lock_guard<std::mutex> lock(previewMutex);
        if (!latestProcessedFrame.empty()) {
            updatePreview(latestProcessedFrame);
        }
    }
    
    // Update status label
    if (isProcessing) {
        int current = currentProcessingFrame.load();
        int total = totalProcessingFrames.load();
        if (total > 0) {
            int percentage = (100 * current) / total;
            const std::string phaseLabel = isAudioMuxing ? "Audio muxing" : "Processing video";
            statusLabel->setText(phaseLabel + ": " + std::to_string(current) + "/" +
                               std::to_string(total) + " (" + std::to_string(percentage) + "%)");
            if (processingProgressBar) {
                processingProgressBar->setVisible(true);
                processingProgressBar->setMinimum(0);
                processingProgressBar->setMaximum(total);
                processingProgressBar->setValue(current);
            }
        } else if (processingProgressBar) {
            processingProgressBar->setVisible(true);
            processingProgressBar->setMinimum(0);
            processingProgressBar->setMaximum(100);
            processingProgressBar->setValue(0);
        }
    } else if (processingThread && processingThread->joinable()) {
        // Processing just finished
        processingThread->join();
        processingThread.reset();

        if (processingProgressBar) {
            processingProgressBar->setVisible(false);
        }
        
        if (shouldStopProcessing) {
            statusLabel->setText("Processing stopped");
        } else {
            statusLabel->setText("Processing complete!");
        }
        isAudioMuxing = false;
    }
}

void GUI::stopProcessing() {
    if (isProcessing) {
        shouldStopProcessing = true;
        statusLabel->setText("Stopping...");
    }
}

void GUI::openAutomationWindow() {
    AudioBuffer* activeAudioBuffer = audioPlaylist.getAudioBuffer() ?
                                     audioPlaylist.getAudioBuffer() :
                                     videoProcessor.getAudioBuffer();
    float previewFps = loadedImage.empty() ?
                       (videoProcessor.getFPS() > 0.0f ? videoProcessor.getFPS() : 30.0f) :
                       30.0f;
    syncAutomationTimeline(previewFps, activeAudioBuffer);
    automationWindow->open(effectChain);
}

void GUI::updateAutomationWindow() {
    if (automationWindow && automationWindow->isOpen()) {
        automationWindow->handleEvents();
        automationWindow->update();
        automationWindow->draw();
    }
}

void GUI::applyAutomationAtFrame(int frameNumber) {
    if (!automationWindow) return;
    
    const auto& automations = automationWindow->getAutomations();
    if (automations.empty()) return;
    
    // Apply automation for each effect instance that has automation data
    for (const auto& effectAutomation : automations) {
        int effectIndex = effectAutomation.first;
        const auto& paramAutomations = effectAutomation.second;
        
        // Get the effect at this index in the chain
        auto effect = effectChain.getEffect(effectIndex);
        if (effect) {
            // Apply each parameter automation for this effect instance
            for (const auto& paramAuto : paramAutomations) {
                const std::string& paramName = paramAuto.first;
                const ParameterAutomation& automation = paramAuto.second;
                
                if (automation.hasKeyframes()) {
                    // Use getActualValueAtFrame to scale to parameter's range
                    float automatedValue = automation.getActualValueAtFrame(frameNumber);
                    effect->setParameter(paramName, automatedValue);
                }
            }
        }
    }
    
    currentDisplayFrame = frameNumber;
}

void GUI::updateParameterDisplayValues() {
    if (isEditingParameterField) {
        return;
    }

    int selected = chainList->getSelectedItemIndex();
    if (selected < 0) return;
    
    auto effect = effectChain.getEffect(selected);
    if (!effect) return;
    
    // Update edit boxes with current (possibly automated) values
    const auto paramNames = withAudioGainParam(effect->getParameterNames());
    auto widgets = paramPanel->getWidgets();
    
    int editBoxIndex = 0;
    for (const auto& paramName : paramNames) {
        // Find the corresponding edit box (every other widget after labels)
        if (editBoxIndex * 2 + 1 < widgets.size()) {
            auto editBox = std::dynamic_pointer_cast<tgui::EditBox>(widgets[editBoxIndex * 2 + 1]);
            if (editBox) {
                float currentValue = effect->getParameter(paramName);
                editBox->setText(tgui::String(currentValue));
            }
        }
        editBoxIndex++;
    }
}
