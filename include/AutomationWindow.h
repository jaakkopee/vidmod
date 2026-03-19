#pragma once

#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include "ParameterAutomation.h"
#include "EffectChain.h"
#include <map>
#include <string>
#include <vector>

class AutomationWindow {
private:
    sf::RenderWindow window;
    tgui::Gui gui;
    
    // Automation data - indexed by effect instance index in the chain
    std::map<int, std::map<std::string, ParameterAutomation>> effectAutomations;
    // Range multiplier index for each parameter (0-8)
    std::map<int, std::map<std::string, int>> parameterRangeIndices;
    // Original default value captured the first time a parameter is selected
    std::map<int, std::map<std::string, float>> parameterBaseValues;
    int selectedEffectIndex;  // Index of effect instance in the chain (-1 if none)
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
    int hoveredKeyframe;
    sf::Vector2i lastMousePos;
    sf::Vector2f mouseLocalPos;
    bool mouseInCanvas = false;
    
    // Node visualization constants
    static constexpr float NODE_RADIUS = 8.0f;
    static constexpr float NODE_HOVER_RADIUS = 10.0f;
    static constexpr float NODE_SNAP_DISTANCE = 12.0f;
    
    // Helper methods
    void setupUI();
    void updateParamList();
    void drawAutomationCanvas();
    void drawGridBackground();
    void drawValueScale();
    void drawNodes();
    void drawConnectingLines();
    void handleCanvasClick(sf::Vector2f pos, bool doubleClick);
    void handleCanvasDrag(sf::Vector2f pos);
    
    // Utility
    int findKeyframeAtPosition(sf::Vector2f localPos);
    void updateHoveredKeyframe(sf::Vector2f localPos);

public:
    AutomationWindow(int frames = 1000);
    ~AutomationWindow();
    
    void open(EffectChain& chain);
    bool isOpen() const { return window.isOpen(); }
    void handleEvents();
    void update();
    void draw();
    
    // Get automation data
    const std::map<int, std::map<std::string, ParameterAutomation>>& getAutomations() const {
        return effectAutomations;
    }

    // JSON serialization for optional save/load together with effect chain files
    std::string exportAutomationJson() const;
    bool importAutomationJson(const std::string& jsonText);
    void moveEffectAutomation(int fromIndex, int toIndex);
    void insertEffectAutomation(int atIndex);
    void removeEffectAutomation(int atIndex);
    void clearAutomationData();
    
    void setTotalFrames(int frames) { totalFrames = frames; }
    int getTotalFrames() const { return totalFrames; }
};
