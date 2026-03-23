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
    std::vector<int> trackBoundaryFrames;
    std::vector<int> rhythmSubsectionFrames;
    
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

    // Zoom / Pan viewport
    float zoomX    = 1.0f;   // horizontal zoom (1=full, 16=max)
    float zoomY    = 1.0f;   // vertical / value-axis zoom
    float panX     = 0.0f;   // normalized start of visible X window [0, 1-1/zoomX]
    float panYOff  = 0.0f;   // normalized start of visible Y window [0, 1-1/zoomY]
    bool  isPanning      = false;
    float panStartX      = 0.0f;
    float panStartMouseX = 0.0f;

    // TGUI zoom/pan controls
    tgui::Slider::Ptr zoomXSlider;
    tgui::Slider::Ptr zoomYSlider;
    tgui::Slider::Ptr panXSlider;
    tgui::Label::Ptr  viewInfoLabel;

    // Node visualization constants
    static constexpr float NODE_RADIUS = 8.0f;
    static constexpr float NODE_HOVER_RADIUS = 10.0f;
    static constexpr float NODE_SNAP_DISTANCE = 12.0f;
    
    // Helper methods
    void setupUI();
    void recalculateCanvasLayout();
    void updateParamList();
    void drawAutomationCanvas();
    void drawGridBackground();
    void drawValueScale();
    void drawNodes();
    void drawConnectingLines();
    void handleCanvasClick(sf::Vector2f pos, bool doubleClick);
    void handleCanvasDrag(sf::Vector2f pos);

    // Viewport coordinate helpers
    float frameToCanvasX(int frame) const;
    float canvasXToFrame(float localX) const;
    float normValToCanvasY(float normVal) const;
    float canvasYToNormVal(float localY) const;
    void  clampView();
    void  updateViewInfo();

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
    void setAudioGuideMarkers(const std::vector<int>& trackBoundaries,
                              const std::vector<int>& rhythmSubsections);
};
