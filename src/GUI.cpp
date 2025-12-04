#include "GUI.h"
#include <iostream>

GUI::GUI(sf::RenderWindow& win) : window(win), gui(window), previewSprite(previewTexture), currentAudioPosition(0.0f), showingPreview(false) {
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
    effectList->setSize("90%", "40%");
    effectList->setPosition("5%", "7%");
    effectList->addItem("FFT");
    effectList->addItem("Shadow");
    effectList->addItem("Light");
    effectList->addItem("Diffuse");
    effectList->addItem("AudioColor");
    leftPanel->add(effectList);
    
    auto addButton = tgui::Button::create("Add to Chain");
    addButton->setSize("90%", "6%");
    addButton->setPosition("5%", "50%");
    addButton->onPress([this]() { 
        if (effectList->getSelectedItemIndex() >= 0) {
            addEffectToChain(effectList->getSelectedItem().toStdString());
        }
    });
    leftPanel->add(addButton);
    
    // Chain management buttons
    auto removeButton = tgui::Button::create("Remove");
    removeButton->setSize("44%", "6%");
    removeButton->setPosition("5%", "58%");
    removeButton->onPress([this]() { removeSelectedEffect(); });
    leftPanel->add(removeButton);
    
    auto upButton = tgui::Button::create("Move Up");
    upButton->setSize("44%", "6%");
    upButton->setPosition("51%", "58%");
    upButton->onPress([this]() { moveEffectUp(); });
    leftPanel->add(upButton);
    
    auto downButton = tgui::Button::create("Move Down");
    downButton->setSize("90%", "6%");
    downButton->setPosition("5%", "66%");
    downButton->onPress([this]() { moveEffectDown(); });
    leftPanel->add(downButton);
    
    // File loading buttons
    auto loadVideoButton = tgui::Button::create("Load Video");
    loadVideoButton->setSize("90%", "5%");
    loadVideoButton->setPosition("5%", "76%");
    loadVideoButton->onPress([this]() { loadVideoFile(); });
    leftPanel->add(loadVideoButton);
    
    auto loadAudioButton = tgui::Button::create("Load Audio");
    loadAudioButton->setSize("90%", "5%");
    loadAudioButton->setPosition("5%", "82%");
    loadAudioButton->onPress([this]() { loadAudioFile(); });
    leftPanel->add(loadAudioButton);
    
    auto loadImageButton = tgui::Button::create("Load Image Loop");
    loadImageButton->setSize("90%", "5%");
    loadImageButton->setPosition("5%", "88%");
    loadImageButton->onPress([this]() { loadImageFile(); });
    leftPanel->add(loadImageButton);
    
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
    paramPanel->setSize("90%", "85%");
    paramPanel->setPosition("5%", "7%");
    rightPanel->add(paramPanel);
    
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
    
    // Calculate default duration from audio if available
    std::string defaultDuration = "10.0";
    if (videoProcessor.getAudioBuffer()) {
        float audioDuration = static_cast<float>(videoProcessor.getAudioBuffer()->size()) / 
                             videoProcessor.getAudioBuffer()->getSampleRate();
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
            
            if (videoProcessor.processImageLoop(currentImagePath, outputPath, effectChain, duration, fps)) {
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
    // Check if we have a loaded image first
    try {
        if (!loadedImage.empty()) {
            statusLabel->setText("Generating preview...");
            cv::Mat previewFrame = loadedImage.clone();
            
            // Set audio buffer position based on slider
            if (videoProcessor.getAudioBuffer()) {
                size_t audioPos = static_cast<size_t>(currentAudioPosition * videoProcessor.getAudioBuffer()->size());
                videoProcessor.getAudioBuffer()->setIndex(audioPos);
                std::cout << "Preview using audio position: " << currentAudioPosition 
                         << " (sample " << audioPos << ")" << std::endl;
            }
            
            cv::Mat processedFrame = effectChain.applyEffects(previewFrame, videoProcessor.getAudioBuffer(), 30.0f);
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
    videoProcessor.reset();
    cv::Mat frame = videoProcessor.getNextFrame();
    
    if (frame.empty()) {
        statusLabel->setText("No video or image loaded");
        return;
    }
    
    statusLabel->setText("Generating preview...");
    
    cv::Mat processedFrame = effectChain.applyEffects(frame, videoProcessor.getAudioBuffer(), videoProcessor.getFPS());
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
    
    std::cout << "Opening file dialog for video processing" << std::endl;
    
    // Create a file dialog window
    auto fileDialog = tgui::ChildWindow::create("Save Processed Video");
    fileDialog->setSize("400", "150");
    fileDialog->setPosition("(&.size - size) / 2");
    
    auto pathLabel = tgui::Label::create("Output file path:");
    pathLabel->setPosition("5%", "10%");
    pathLabel->setTextSize(14);
    fileDialog->add(pathLabel);
    
    auto pathEdit = tgui::EditBox::create();
    pathEdit->setSize("90%", "20%");
    pathEdit->setPosition("5%", "30%");
    pathEdit->setDefaultText("output.mp4");
    fileDialog->add(pathEdit);
    
    auto saveBtn = tgui::Button::create("Save");
    saveBtn->setSize("40%", "20%");
    saveBtn->setPosition("5%", "70%");
    saveBtn->onPress([this, fileDialog, pathEdit]() {
        std::string path = pathEdit->getText().toStdString();
        gui.remove(fileDialog);
        
        statusLabel->setText("Processing video...");
        
        if (videoProcessor.saveProcessedVideo(path, effectChain)) {
            statusLabel->setText("Video saved: " + path.substr(path.find_last_of("/\\") + 1));
        } else {
            statusLabel->setText("Failed to save video");
        }
    });
    fileDialog->add(saveBtn);
    
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize("40%", "20%");
    cancelBtn->setPosition("55%", "70%");
    cancelBtn->onPress([this, fileDialog]() {
        gui.remove(fileDialog);
    });
    fileDialog->add(cancelBtn);
    
    gui.add(fileDialog);
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
