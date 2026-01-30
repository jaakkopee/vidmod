#pragma once

#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include "ParameterAutomation.h"
#include "EffectChain.h"
#include <map>
#include <string>

class AutomationWindow {
private:
    sf::RenderWindow window;
    tgui::Gui gui;
    
    // Automation data
    std::map<std::string, std::map<std::string, ParameterAutomation>> effectAutomations;
    std::string selectedEffect;
    std::string selectedParam;
    int totalFrames;
    
    // Reference to effect chain
    EffectChain* effectChainRef;
    
    // UI elements
    tgui::ComboBox::Ptr effectComboBox;
    tgui::ComboBox::Ptr paramComboBox;
    tgui::ComboBox::Ptr rangeComboBox;
    tgui::Label::Ptr rangeLabel;
    
    // Canvas drawn directly with SFML
    sf::RectangleShape canvasBackground;
    sf::Vector2f canvasPos;
    sf::Vector2f canvasSize;
    sf::Font font;
    
    // Interaction state
    int selectedKeyframe;
    bool draggingKeyframe;
    sf::Vector2i lastMousePos;
    
    void setupUI();
    void updateParamList();
    void drawAutomationCurve();
    void handleCanvasClick(sf::Vector2f pos, bool doubleClick);
    void handleCanvasDrag(sf::Vector2f pos);

public:
    AutomationWindow(int frames = 1000);
    ~AutomationWindow();
    
    void open(EffectChain& chain);
    bool isOpen() const { return window.isOpen(); }
    void handleEvents();
    void update();
    void draw();
    
    // Get automation data
    const std::map<std::string, std::map<std::string, ParameterAutomation>>& getAutomations() const {
        return effectAutomations;
    }
    
    void setTotalFrames(int frames) { totalFrames = frames; }
};
