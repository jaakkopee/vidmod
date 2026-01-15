#include "GUI.h"
#include <iostream>

GUI::GUI(sf::RenderWindow& win) : window(win), gui(window), audioPlaylist(44100), previewSprite(previewTexture), currentAudioPosition(0.0f), showingPreview(false) {
    setupUI();
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
    
    // Audio Playlist Section
    auto playlistLabel = tgui::Label::create("Audio Playlist:");
    playlistLabel->setPosition("5%", "41%");
    playlistLabel->setTextSize(16);
    leftPanel->add(playlistLabel);
    
    playlistBox = tgui::ListBox::create();
    playlistBox->setSize("90%", "25%");
    playlistBox->setPosition("5%", "46%");
    leftPanel->add(playlistBox);
    
    auto addAudioBtn = tgui::Button::create("Add Audio");
    addAudioBtn->setSize("44%", "5%");
    addAudioBtn->setPosition("5%", "73%");
    addAudioBtn->onPress([this]() { addAudioToPlaylist(); });
    leftPanel->add(addAudioBtn);
    
    auto removeAudioBtn = tgui::Button::create("Remove");
    removeAudioBtn->setSize("44%", "5%");
    removeAudioBtn->setPosition("51%", "73%");
    removeAudioBtn->onPress([this]() { removeAudioFromPlaylist(); });
    leftPanel->add(removeAudioBtn);
    
    auto clearPlaylistBtn = tgui::Button::create("Clear Playlist");
    clearPlaylistBtn->setSize("90%", "5%");
    clearPlaylistBtn->setPosition("5%", "79%");
    clearPlaylistBtn->onPress([this]() { clearPlaylist(); });
    leftPanel->add(clearPlaylistBtn);
    
    // Chain management buttons
    auto removeButton = tgui::Button::create("Remove");
    removeButton->setSize("44%", "5%");
    removeButton->setPosition("5%", "86%");
    removeButton->onPress([this]() { removeSelectedEffect(); });
    leftPanel->add(removeButton);
    
    auto upButton = tgui::Button::create("Move Up");
    upButton->setSize("44%", "5%");
    upButton->setPosition("51%", "86%");
    upButton->onPress([this]() { moveEffectUp(); });
    leftPanel->add(upButton);
    
    auto downButton = tgui::Button::create("Move Down");
    downButton->setSize("90%", "5%");
    downButton->setPosition("5%", "92%");
    downButton->onPress([this]() { moveEffectDown(); });
    leftPanel->add(downButton);
    
    // Middle panel for effect chain
    auto middlePanel = tgui::Panel::create();
    middlePanel->setSize("20%", "100%");
    middlePanel->setPosition("20%", "0%");
    middlePanel->getRenderer()->setBackgroundColor(tgui::Color(240, 240, 240));
    mainPanel->add(middlePanel);
    
    auto chainLabel = tgui::Label::create("Effect Chain:");
    chainLabel->setPosition("5%", "2%");
    chainLabel->setTextSize(16);
    middlePanel->add(chainLabel);
    
    chainList = tgui::ListBox::create();
    chainList->setSize("90%", "70%");
    chainList->setPosition("5%", "7%");
    chainList->onItemSelect([this](int) { updateParameterPanel(); });
    middlePanel->add(chainList);
    
    // Audio position slider
    auto audioTimeLabel = tgui::Label::create("Audio Time:");
    audioTimeLabel->setPosition("5%", "79%");
    audioTimeLabel->setTextSize(12);
    middlePanel->add(audioTimeLabel);
    
    audioPositionLabel = tgui::Label::create("0.0s");
    audioPositionLabel->setPosition("75%", "79%");
    audioPositionLabel->setTextSize(12);
    middlePanel->add(audioPositionLabel);
    
    audioPositionSlider = tgui::Slider::create();
    audioPositionSlider->setSize("90%", "3%");
    audioPositionSlider->setPosition("5%", "82%");
    audioPositionSlider->setMinimum(0);
    audioPositionSlider->setMaximum(100);
    audioPositionSlider->setValue(0);
    audioPositionSlider->onValueChange([this](float value) {
        currentAudioPosition = value / 100.0f; // 0.0 to 1.0
        if (videoProcessor.getAudioBuffer()) {
            float audioDuration = static_cast<float>(videoProcessor.getAudioBuffer()->size()) / 
                                 videoProcessor.getAudioBuffer()->getSampleRate();
            float timeSeconds = currentAudioPosition * audioDuration;
            audioPositionLabel->setText(std::to_string(static_cast<int>(timeSeconds)) + "." + 
                                       std::to_string(static_cast<int>(timeSeconds * 10) % 10) + "s");
        }
    });
    middlePanel->add(audioPositionSlider);
    
    previewButton = tgui::Button::create("Preview Frame");
    previewButton->setSize("90%", "5%");
    previewButton->setPosition("5%", "86%");
    previewButton->onPress([this]() { 
        std::cout << "Preview button clicked!" << std::endl;
        generatePreview(); 
    });
    middlePanel->add(previewButton);
    
    processButton = tgui::Button::create("Process Video");
    processButton->setSize("90%", "5%");
    processButton->setPosition("5%", "92%");
    processButton->onPress([this]() { 
        std::cout << "Process button clicked!" << std::endl;
        processVideo(); 
    });
    middlePanel->add(processButton);
    
    std::cout << "Process button created at position 92% with size 5%" << std::endl;
    
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
    
    paramPanel = tgui::Panel::create();
    paramPanel->setSize("90%", "70%");
    paramPanel->setPosition("5%", "7%");
    rightPanel->add(paramPanel);
    
    // File loading buttons at bottom of right panel
    auto loadVideoButton = tgui::Button::create("Load Video");
    loadVideoButton->setSize("90%", "5%");
    loadVideoButton->setPosition("5%", "80%");
    loadVideoButton->onPress([this]() { loadVideoFile(); });
    rightPanel->add(loadVideoButton);
    
    auto loadImageButton = tgui::Button::create("Load Image Loop");
    loadImageButton->setSize("90%", "5%");
    loadImageButton->setPosition("5%", "87%");
    loadImageButton->onPress([this]() { loadImageFile(); });
    rightPanel->add(loadImageButton);
    
    // Status label at bottom of preview area (not covering the buttons!)
    statusLabel = tgui::Label::create("Ready");
    statusLabel->setSize("38%", "8%");
    statusLabel->setPosition("61%", "91%");
    statusLabel->setTextSize(14);
    mainPanel->add(statusLabel);
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

void GUI::moveEffectUp() {
    int selected = chainList->getSelectedItemIndex();
    if (selected > 0) {
        effectChain.moveEffect(selected, selected - 1);
        updateChainList();
        chainList->setSelectedItemByIndex(selected - 1);
    }
}

void GUI::moveEffectDown() {
    int selected = chainList->getSelectedItemIndex();
    if (selected >= 0 && selected < static_cast<int>(effectChain.size()) - 1) {
        effectChain.moveEffect(selected, selected + 1);
        updateChainList();
        chainList->setSelectedItemByIndex(selected + 1);
    }
}

void GUI::updateChainList() {
    chainList->removeAllItems();
    const auto& effects = effectChain.getEffects();
    for (size_t i = 0; i < effects.size(); ++i) {
        chainList->addItem(std::to_string(i + 1) + ". " + effects[i]->getName());
    }
}

void GUI::updateParameterPanel() {
    paramPanel->removeAllWidgets();
    
    int selected = chainList->getSelectedItemIndex();
    if (selected < 0) return;
    
    auto effect = effectChain.getEffect(selected);
    if (!effect) return;
    
    float yPos = 5.0f;
    const auto& paramNames = effect->getParameterNames();
    
    for (const auto& paramName : paramNames) {
        auto label = tgui::Label::create(paramName + ":");
        label->setPosition("5%", tgui::String(yPos) + "%");
        label->setTextSize(12);
        paramPanel->add(label);
        
        auto editBox = tgui::EditBox::create();
        editBox->setSize("60%", "8%");
        editBox->setPosition("5%", tgui::String(yPos + 6) + "%");
        editBox->setText(tgui::String(effect->getParameter(paramName)));
        
        // Save parameter on Return key
        editBox->onReturnKeyPress([this, effect, paramName, editBox]() {
            try {
                float value = std::stof(editBox->getText().toStdString());
                effect->setParameter(paramName, value);
                statusLabel->setText("Updated " + paramName);
                std::cout << "Parameter " << paramName << " updated to " << value << std::endl;
            } catch (...) {
                statusLabel->setText("Invalid value for " + paramName);
            }
        });
        
        // Also save parameter when the edit box loses focus
        editBox->onUnfocus([this, effect, paramName, editBox]() {
            try {
                float value = std::stof(editBox->getText().toStdString());
                effect->setParameter(paramName, value);
                std::cout << "Parameter " << paramName << " updated to " << value << " (unfocus)" << std::endl;
            } catch (...) {
                // Invalid value, ignore
            }
        });
        
        paramPanel->add(editBox);
        
        yPos += 16.0f;
    }
}

void GUI::loadVideoFile() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Load Video");
    fileDialog->setSize("400", "150");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Video file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("40%", "20%");
    loadBtn->setPosition("5%", "70%");
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
    fileDialog->setSize("400", "150");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Audio file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("40%", "20%");
    loadBtn->setPosition("5%", "70%");
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
    cancelBtn->setSize("40%", "20%");
    cancelBtn->setPosition("55%", "70%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::loadImageFile() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Load Image for Looping");
    fileDialog->setSize("400", "150");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Image file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto loadBtn = tgui::Button::create("Load");
    loadBtn->setSize("40%", "20%");
    loadBtn->setPosition("5%", "70%");
    loadBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        
        // Load the image and show preview
        loadedImage = cv::imread(path);
        if (!loadedImage.empty()) {
            currentImagePath = path;
            
            // Show preview of the loaded image with effects applied
            cv::Mat previewFrame = loadedImage.clone();
            // If there are effects, apply them; otherwise show the original image
            cv::Mat processedFrame = effectChain.applyEffects(previewFrame, videoProcessor.getAudioBuffer(), 30.0f);
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
    cancelBtn->setSize("40%", "20%");
    cancelBtn->setPosition("55%", "70%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
}

void GUI::addAudioToPlaylist() {
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Add Audio to Playlist");
    fileDialog->setSize("400", "150");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Audio file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    fileDialog->add(pathEdit);
    
    auto loadBtn = tgui::Button::create("Add");
    loadBtn->setSize("40%", "20%");
    loadBtn->setPosition("5%", "70%");
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
    cancelBtn->setSize("40%", "20%");
    cancelBtn->setPosition("55%", "70%");
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

void GUI::updatePlaylistDisplay() {
    playlistBox->removeAllItems();
    for (size_t i = 0; i < audioPlaylist.getTrackCount(); ++i) {
        const auto& track = audioPlaylist.getTrack(i);
        std::string filename = track.filePath.substr(track.filePath.find_last_of("/\\") + 1);
        float duration = static_cast<float>(track.audioData.size()) / track.sampleRate;
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
            
            statusLabel->setText("Processing image loop...");
            
            // Use playlist audio if available, otherwise use VideoProcessor's audio
            AudioBuffer* audioToUse = audioPlaylist.getAudioBuffer() ? 
                                     audioPlaylist.getAudioBuffer() : 
                                     videoProcessor.getAudioBuffer();
            
            if (videoProcessor.processImageLoop(currentImagePath, outputPath, effectChain, audioToUse, duration, fps)) {
                statusLabel->setText("Image loop saved: " + outputPath.substr(outputPath.find_last_of("/\\") + 1));
            } else {
                statusLabel->setText("Failed to process image loop");
            }
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
            
            // Set audio buffer position based on slider
            if (activeAudioBuffer) {
                size_t audioPos = static_cast<size_t>(currentAudioPosition * activeAudioBuffer->size());
                activeAudioBuffer->setIndex(audioPos);
                std::cout << "Preview using audio position: " << currentAudioPosition 
                         << " (sample " << audioPos << ")" << std::endl;
            }
            
            cv::Mat processedFrame = effectChain.applyEffects(previewFrame, activeAudioBuffer, 30.0f);
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
    float audioDuration = static_cast<float>(activeAudioBuffer->size()) / 
                         activeAudioBuffer->getSampleRate();
    float targetTime = currentAudioPosition * audioDuration;
    int targetFrame = static_cast<int>(targetTime * videoProcessor.getFPS());
    
    // Loop video if needed (if video is shorter than audio)
    if (videoProcessor.getTotalFrames() > 0) {
        targetFrame = targetFrame % videoProcessor.getTotalFrames();
    }
    
    std::cout << "Preview at audio position " << currentAudioPosition 
              << " (" << targetTime << "s, frame " << targetFrame << ")" << std::endl;
    
    cv::Mat frame = videoProcessor.getFrameAt(targetFrame);
    
    if (frame.empty()) {
        statusLabel->setText("No video loaded");
        return;
    }
    
    statusLabel->setText("Generating preview...");
    
    // Set audio buffer position
    size_t audioPos = static_cast<size_t>(currentAudioPosition * activeAudioBuffer->size());
    activeAudioBuffer->setIndex(audioPos);
    
    cv::Mat processedFrame = effectChain.applyEffects(frame, activeAudioBuffer, videoProcessor.getFPS());
    updatePreview(processedFrame);
    
    statusLabel->setText("Preview generated");
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
            
            statusLabel->setText("Processing video...");
            
            // Use playlist audio if available, otherwise use VideoProcessor's audio
            AudioBuffer* audioToUse = audioPlaylist.getAudioBuffer() ? 
                                     audioPlaylist.getAudioBuffer() : 
                                     videoProcessor.getAudioBuffer();
            
            if (videoProcessor.saveProcessedVideo(outputPath, effectChain, audioToUse, duration)) {
                statusLabel->setText("Video saved: " + outputPath.substr(outputPath.find_last_of("/\\") + 1));
            } else {
                statusLabel->setText("Failed to save video");
            }
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
    float scaleY = (window.getSize().y * 0.82f) / rgbaFrame.rows;
    float scale = std::min(scaleX, scaleY);
    
    previewSprite.setScale({scale, scale});
    previewSprite.setPosition({window.getSize().x * 0.61f, window.getSize().y * 0.06f});
    
    showingPreview = true;
    currentPreviewFrame = frame.clone();
    
    std::cout << "Preview updated successfully. showingPreview=" << showingPreview << std::endl;
}

void GUI::handleEvent(const sf::Event& event) {
    gui.handleEvent(event);
}

void GUI::draw() {
    gui.draw();
    
    if (showingPreview) {
        // Ensure proper blend mode for drawing the sprite
        sf::RenderStates states = sf::RenderStates::Default;
        window.draw(previewSprite, states);
    }
}
