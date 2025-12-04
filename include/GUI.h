#pragma once

#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <SFML/Graphics.hpp>
#include "VideoProcessor.h"
#include "EffectChain.h"
#include "FFTEffect.h"
#include "ShadowEffect.h"
#include "LightEffect.h"
#include "DiffuseEffect.h"
#include "AudioColorEffect.h"
#include <memory>

class GUI {
private:
    sf::RenderWindow& window;
    tgui::Gui gui;
    
    VideoProcessor videoProcessor;
    EffectChain effectChain;
    
    tgui::ListBox::Ptr effectList;
    tgui::ListBox::Ptr chainList;
    tgui::Panel::Ptr paramPanel;
    tgui::Button::Ptr previewButton;
    tgui::Button::Ptr processButton;
    tgui::Label::Ptr statusLabel;
    tgui::Slider::Ptr audioPositionSlider;
    tgui::Label::Ptr audioPositionLabel;
    float currentAudioPosition;
    
    sf::Texture previewTexture;
    sf::Sprite previewSprite;
    bool showingPreview;
    cv::Mat currentPreviewFrame;
    std::string currentImagePath;
    cv::Mat loadedImage;
    
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
    void generatePreview();
    void processVideo();
    void processImageLoop();
    void updatePreview(const cv::Mat& frame);

public:
    GUI(sf::RenderWindow& win);
    
    void handleEvent(const sf::Event& event);
    void draw();
};
