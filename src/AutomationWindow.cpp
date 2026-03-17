#include "AutomationWindow.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

static std::string formatFloat(float val) {
    std::ostringstream oss;
    float absVal = std::abs(val);
    if (absVal < 0.001f)      oss << std::fixed << std::setprecision(5) << val;
    else if (absVal < 0.01f)  oss << std::fixed << std::setprecision(4) << val;
    else if (absVal < 0.1f)   oss << std::fixed << std::setprecision(3) << val;
    else if (absVal < 10.0f)  oss << std::fixed << std::setprecision(2) << val;
    else if (absVal < 100.0f) oss << std::fixed << std::setprecision(1) << val;
    else                      oss << std::fixed << std::setprecision(0) << val;
    return oss.str();
}

AutomationWindow::AutomationWindow(int frames) 
    : window(sf::VideoMode({800, 400}), "Parameter Automation")
    , gui(window)
    , totalFrames(frames)
    , effectChainRef(nullptr)
    , selectedKeyframe(-1)
    , draggingKeyframe(false) {
    
    // Load font for canvas labels
    if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        std::cerr << "Warning: Could not load font for automation window" << std::endl;
    }
    
    window.close(); // Start closed
    setupUI();
}

AutomationWindow::~AutomationWindow() {
    if (window.isOpen()) {
        window.close();
    }
}

void AutomationWindow::setupUI() {
    // Effect selector
    auto effectLabel = tgui::Label::create("Effect Instance:");
    effectLabel->setPosition(10, 10);
    effectLabel->setTextSize(14);
    gui.add(effectLabel);
    
    effectComboBox = tgui::ComboBox::create();
    effectComboBox->setPosition(130, 10);
    effectComboBox->setSize(200, 25);
    effectComboBox->onItemSelect([this](int index) {
        if (index >= 0) {
            selectedEffectIndex = index;
            updateParamList();
        }
    });
    gui.add(effectComboBox);
    
    // Parameter selector
    auto paramLabel = tgui::Label::create("Parameter:");
    paramLabel->setPosition(350, 10);
    paramLabel->setTextSize(14);
    gui.add(paramLabel);
    
    paramComboBox = tgui::ComboBox::create();
    paramComboBox->setPosition(440, 10);
    paramComboBox->setSize(160, 25);
    paramComboBox->onItemSelect([this](int) {
        if (paramComboBox->getSelectedItemIndex() >= 0 && selectedEffectIndex >= 0) {
            selectedParam = paramComboBox->getSelectedItem().toStdString();
            
            // Capture the base (default) value the first time this parameter is selected.
            // We must do this BEFORE any automation can overwrite the live value.
            if (effectChainRef && !parameterBaseValues[selectedEffectIndex].count(selectedParam)) {
                const auto& effects = effectChainRef->getEffects();
                if (selectedEffectIndex < static_cast<int>(effects.size())) {
                    parameterBaseValues[selectedEffectIndex][selectedParam] =
                        effects[selectedEffectIndex]->getParameter(selectedParam);
                }
            }
            
            // Load saved range index for this parameter, or default to 4 ("1x")
            int savedRangeIndex = 4;  // Default multiplier
            if (parameterRangeIndices[selectedEffectIndex].count(selectedParam) > 0) {
                savedRangeIndex = parameterRangeIndices[selectedEffectIndex][selectedParam];
            }
            
            // Set the range selector to the saved index (will trigger its handler)
            rangeComboBox->setSelectedItemByIndex(savedRangeIndex);
        }
    });
    gui.add(paramComboBox);
    
    // Range selector
    auto rangeLabel2 = tgui::Label::create("Range:");
    rangeLabel2->setPosition(620, 10);
    rangeLabel2->setTextSize(14);
    gui.add(rangeLabel2);
    
    rangeComboBox = tgui::ComboBox::create();
    rangeComboBox->setPosition(680, 10);
    rangeComboBox->setSize(100, 25);
    rangeComboBox->addItem("/10000");
    rangeComboBox->addItem("/1000");
    rangeComboBox->addItem("/100");
    rangeComboBox->addItem("/10");
    rangeComboBox->addItem("1");
    rangeComboBox->addItem("*10");
    rangeComboBox->addItem("*100");
    rangeComboBox->addItem("*1000");
    rangeComboBox->addItem("*10000");
    rangeComboBox->setSelectedItemByIndex(4); // Default to 1
    rangeComboBox->onItemSelect([this](int index) {
        // Save this range index for the current parameter
        if (selectedEffectIndex >= 0 && !selectedParam.empty()) {
            parameterRangeIndices[selectedEffectIndex][selectedParam] = index;
        }
        
        // Use the stored base value — NOT the live parameter value which may have
        // been overwritten by automation. Fall back to 1.0 if somehow not stored yet.
        float baseValue = 1.0f;
        if (selectedEffectIndex >= 0 && !selectedParam.empty()) {
            auto& baseMap = parameterBaseValues[selectedEffectIndex];
            if (baseMap.count(selectedParam)) {
                baseValue = baseMap[selectedParam];
            } else if (effectChainRef) {
                // First time the range fires before param-select captured it
                const auto& effects = effectChainRef->getEffects();
                if (selectedEffectIndex < static_cast<int>(effects.size())) {
                    baseValue = effects[selectedEffectIndex]->getParameter(selectedParam);
                    baseMap[selectedParam] = baseValue;
                }
            }
        }
        
        // Calculate multiplier based on selection
        float multiplier = 1.0f;
        switch (index) {
            case 0: multiplier = 0.0001f; break;  // /10000
            case 1: multiplier = 0.001f; break;   // /1000
            case 2: multiplier = 0.01f; break;    // /100
            case 3: multiplier = 0.1f; break;     // /10
            case 4: multiplier = 1.0f; break;     // 1
            case 5: multiplier = 10.0f; break;    // *10
            case 6: multiplier = 100.0f; break;   // *100
            case 7: multiplier = 1000.0f; break;  // *1000
            case 8: multiplier = 10000.0f; break; // *10000
        }
        
        float minVal = 0.0f;
        float maxVal = baseValue * multiplier;
        
        // Always update the range label
        rangeLabel->setText("Range: " + tgui::String(minVal) + " - " + tgui::String(maxVal));
        
        // Update automation range if parameter is selected
        if (selectedEffectIndex >= 0 && !selectedParam.empty()) {
            // If automation exists, update its range (preserving keyframes)
            if (effectAutomations[selectedEffectIndex].count(selectedParam) > 0) {
                effectAutomations[selectedEffectIndex][selectedParam].setRange(minVal, maxVal);
            } else {
                // Create new automation with this range
                effectAutomations[selectedEffectIndex][selectedParam] = ParameterAutomation(minVal, maxVal);
            }
        }
    });
    gui.add(rangeComboBox);
    
    // Range display label
    rangeLabel = tgui::Label::create("Range: 0 - 1");
    rangeLabel->setPosition(600, 40);
    rangeLabel->setTextSize(12);
    gui.add(rangeLabel);
    
    // Instructions
    auto instructions = tgui::Label::create("Double-click canvas to add node | Drag node to edit | Hover for preview");
    instructions->setPosition(10, 65);
    instructions->setTextSize(11);
    gui.add(instructions);
    
    // Setup canvas area (drawn manually with SFML)
    canvasPos = sf::Vector2f(10, 90);
    canvasSize = sf::Vector2f(780, 290);
    canvasBackground.setSize(canvasSize);
    canvasBackground.setPosition(canvasPos);
    canvasBackground.setFillColor(sf::Color(35, 35, 40));
    canvasBackground.setOutlineColor(sf::Color(120, 120, 140));
    canvasBackground.setOutlineThickness(2.0f);
    
    selectedKeyframe = -1;
    hoveredKeyframe = -1;
    selectedEffectIndex = -1;
    parameterRangeIndices.clear();
    parameterBaseValues.clear();
}

void AutomationWindow::open(EffectChain& chain) {
    if (!window.isOpen()) {
        window.create(sf::VideoMode({800, 400}), "Parameter Automation");
    }
    
    effectChainRef = &chain;
    
    // Populate effect list with indices
    effectComboBox->removeAllItems();
    const auto& effects = chain.getEffects();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        std::string label = "[" + std::to_string(i) + "] " + effects[i]->getName();
        effectComboBox->addItem(label);
    }
    
    selectedEffectIndex = -1;
    selectedParam = "";
    paramComboBox->removeAllItems();
}

void AutomationWindow::updateParamList() {
    paramComboBox->removeAllItems();
    selectedParam = "";
    
    if (!effectChainRef || selectedEffectIndex < 0) {
        return;
    }
    
    // Find the selected effect and get its parameters
    const auto& effects = effectChainRef->getEffects();
    if (selectedEffectIndex < static_cast<int>(effects.size())) {
        const auto& paramNames = effects[selectedEffectIndex]->getParameterNames();
        for (const auto& paramName : paramNames) {
            paramComboBox->addItem(paramName);
        }
    }
}

void AutomationWindow::handleEvents() {
    if (!window.isOpen()) return;
    
    while (const auto event = window.pollEvent()) {
        gui.handleEvent(event.value());
        
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }
        
        // Track mouse position for hover effects
        if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
            float mx = static_cast<float>(mouseMoved->position.x);
            float my = static_cast<float>(mouseMoved->position.y);
            
            if (mx >= canvasPos.x && mx <= canvasPos.x + canvasSize.x &&
                my >= canvasPos.y && my <= canvasPos.y + canvasSize.y) {
                
                sf::Vector2f localPos(mx - canvasPos.x, my - canvasPos.y);
                mouseLocalPos = localPos;
                mouseInCanvas = true;
                updateHoveredKeyframe(localPos);
                
                if (draggingKeyframe) {
                    handleCanvasDrag(localPos);
                }
            } else {
                hoveredKeyframe = -1;
                mouseInCanvas = false;
                
                // Continue dragging even outside canvas, clamped to canvas bounds
                if (draggingKeyframe) {
                    float clampedX = std::clamp(mx - canvasPos.x, 0.0f, canvasSize.x);
                    float clampedY = std::clamp(my - canvasPos.y, 0.0f, canvasSize.y);
                    handleCanvasDrag(sf::Vector2f(clampedX, clampedY));
                }
            }
        }
        
        // Handle canvas interactions
        if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            float mx = static_cast<float>(mousePressed->position.x);
            float my = static_cast<float>(mousePressed->position.y);
            
            if (mx >= canvasPos.x && mx <= canvasPos.x + canvasSize.x &&
                my >= canvasPos.y && my <= canvasPos.y + canvasSize.y) {
                
                sf::Vector2f localPos(mx - canvasPos.x, my - canvasPos.y);
                
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    static sf::Clock doubleClickClock;
                    static bool waitingForDoubleClick = false;
                    
                    int keyframeAtPos = findKeyframeAtPosition(localPos);
                    
                    if (keyframeAtPos >= 0) {
                        // There is a node here — always select/drag it, never add
                        selectedKeyframe = keyframeAtPos;
                        draggingKeyframe = true;
                        waitingForDoubleClick = false;  // reset so next empty-space click is clean
                    } else if (waitingForDoubleClick && doubleClickClock.getElapsedTime().asMilliseconds() < 300) {
                        // Double-click on empty space — add new keyframe
                        handleCanvasClick(localPos, true);
                        waitingForDoubleClick = false;
                    } else {
                        // Single click on empty space
                        handleCanvasClick(localPos, false);
                        waitingForDoubleClick = true;
                        doubleClickClock.restart();
                    }
                } else if (mousePressed->button == sf::Mouse::Button::Right) {
                    // Right-click to delete node
                    int keyframeAtPos = findKeyframeAtPosition(localPos);
                    if (keyframeAtPos >= 0 && selectedEffectIndex >= 0 && !selectedParam.empty()) {
                        auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
                        automation.removeKeyframe(keyframeAtPos);
                    }
                }
            }
        } else if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouseReleased->button == sf::Mouse::Button::Left) {
                draggingKeyframe = false;
                selectedKeyframe = -1;
            }
        }
    }
}

void AutomationWindow::handleCanvasClick(sf::Vector2f pos, bool doubleClick) {
    if (selectedEffectIndex < 0 || selectedParam.empty()) {
        return;
    }
    
    if (!doubleClick) {
        // Single click on empty space - just deselect
        selectedKeyframe = -1;
        return;
    }
    
    // Double-click: Add new keyframe at canvas position
    int frame = static_cast<int>((pos.x / canvasSize.x) * totalFrames);
    frame = std::max(0, std::min(frame, totalFrames - 1));
    
    float value = 1.0f - (pos.y / canvasSize.y);
    value = std::max(0.0f, std::min(1.0f, value));
    
    auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
    automation.addKeyframe(frame, value);
}

void AutomationWindow::handleCanvasDrag(sf::Vector2f pos) {
    if (selectedKeyframe < 0 || selectedEffectIndex < 0 || selectedParam.empty()) {
        return;
    }
    
    // Calculate new position (frame and value)
    int newFrame = static_cast<int>((pos.x / canvasSize.x) * totalFrames);
    newFrame = std::max(0, std::min(newFrame, totalFrames - 1));
    
    float newValue = 1.0f - (pos.y / canvasSize.y);
    newValue = std::max(0.0f, std::min(1.0f, newValue));
    
    auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
    
    // If frame changed, remove old keyframe and add with new frame
    if (newFrame != selectedKeyframe) {
        automation.removeKeyframe(selectedKeyframe);
        automation.addKeyframe(newFrame, newValue);
        selectedKeyframe = newFrame;
    } else {
        // Only value changed, update in place
        automation.addKeyframe(selectedKeyframe, newValue);
    }
}

void AutomationWindow::update() {
    // Update is now just for drawing, actual rendering happens in draw()
}

int AutomationWindow::findKeyframeAtPosition(sf::Vector2f localPos) {
    if (selectedEffectIndex < 0 || selectedParam.empty()) {
        return -1;
    }
    
    auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
    auto keyframes = automation.getAllKeyframes();
    
    for (const auto& kf : keyframes) {
        float kfX = (kf.frame / static_cast<float>(totalFrames)) * canvasSize.x;
        float kfY = canvasSize.y - (kf.value * canvasSize.y);
        
        float dist = std::sqrt((localPos.x - kfX) * (localPos.x - kfX) + 
                              (localPos.y - kfY) * (localPos.y - kfY));
        if (dist < NODE_SNAP_DISTANCE) {
            return kf.frame;
        }
    }
    
    return -1;
}

void AutomationWindow::updateHoveredKeyframe(sf::Vector2f localPos) {
    hoveredKeyframe = findKeyframeAtPosition(localPos);
}

void AutomationWindow::draw() {
    if (!window.isOpen()) return;
    
    window.clear(sf::Color(50, 50, 55));
    
    // Draw canvas background
    window.draw(canvasBackground);
    
    // Draw automation visualization if effect and param are selected
    if (selectedEffectIndex >= 0 && !selectedParam.empty()) {
        auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
        
        // Draw grid and scale
        drawGridBackground();
        drawValueScale();
        
        // Draw connecting lines between nodes
        drawConnectingLines();
        
        // Draw the nodes
        drawNodes();
        
        // Update range label
        float minVal = automation.getMinValue();
        float maxVal = automation.getMaxValue();
        rangeLabel->setText("Range: " + formatFloat(minVal) + " - " + formatFloat(maxVal));
    } else {
        // No automation selected - show hint
        if (font.getInfo().family != "") {
            sf::Text hint(font, "Select an effect and parameter", 14);
            hint.setFillColor(sf::Color(150, 150, 150));
            hint.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x / 2 - 80, canvasPos.y + canvasSize.y / 2 - 10));
            window.draw(hint);
        }
    }
    
    gui.draw();
    window.display();
}

void AutomationWindow::drawGridBackground() {
    // Draw horizontal grid lines (value divisions)
    const int gridDivisions = 4;
    for (int i = 0; i <= gridDivisions; ++i) {
        float y = canvasPos.y + (canvasSize.y / gridDivisions) * i;
        
        sf::VertexArray gridLine(sf::PrimitiveType::Lines, 2);
        gridLine[0].position = sf::Vector2f(canvasPos.x, y);
        gridLine[0].color = sf::Color(80, 80, 90);
        gridLine[1].position = sf::Vector2f(canvasPos.x + canvasSize.x, y);
        gridLine[1].color = sf::Color(80, 80, 90);
        window.draw(gridLine);
    }
    
    // Draw vertical grid lines (time divisions)
    const int timeGridDivisions = 10;
    for (int i = 0; i <= timeGridDivisions; ++i) {
        float x = canvasPos.x + (canvasSize.x / timeGridDivisions) * i;
        
        sf::VertexArray gridLine(sf::PrimitiveType::Lines, 2);
        gridLine[0].position = sf::Vector2f(x, canvasPos.y);
        gridLine[0].color = sf::Color(70, 70, 80);
        gridLine[1].position = sf::Vector2f(x, canvasPos.y + canvasSize.y);
        gridLine[1].color = sf::Color(70, 70, 80);
        window.draw(gridLine);
    }
}

void AutomationWindow::drawValueScale() {
    if (font.getInfo().family == "") return;
    
    if (selectedEffectIndex < 0 || selectedParam.empty()) return;
    
    auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
    float minVal = automation.getMinValue();
    float maxVal = automation.getMaxValue();
    
    // Draw max value label
    sf::Text maxText(font, formatFloat(maxVal), 10);
    maxText.setFillColor(sf::Color(180, 200, 200));
    maxText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, canvasPos.y - 8));
    window.draw(maxText);
    
    // Draw mid value label
    float midVal = (minVal + maxVal) / 2.0f;
    sf::Text midText(font, formatFloat(midVal), 10);
    midText.setFillColor(sf::Color(160, 180, 180));
    midText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, canvasPos.y + canvasSize.y / 2 - 5));
    window.draw(midText);
    
    // Draw min value label
    sf::Text minText(font, formatFloat(minVal), 10);
    minText.setFillColor(sf::Color(140, 160, 160));
    minText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, canvasPos.y + canvasSize.y - 12));
    window.draw(minText);
    
    // Draw hover guideline + value when mouse is in canvas
    if (mouseInCanvas && selectedEffectIndex >= 0 && !selectedParam.empty()) {
        auto& a = effectAutomations[selectedEffectIndex][selectedParam];
        float normY = 1.0f - std::clamp(mouseLocalPos.y / canvasSize.y, 0.0f, 1.0f);
        float hoverVal = a.getMinValue() + normY * (a.getMaxValue() - a.getMinValue());
        float lineY = canvasPos.y + mouseLocalPos.y;
        
        sf::VertexArray guideLine(sf::PrimitiveType::Lines, 2);
        guideLine[0].position = sf::Vector2f(canvasPos.x, lineY);
        guideLine[0].color = sf::Color(220, 220, 100, 100);
        guideLine[1].position = sf::Vector2f(canvasPos.x + canvasSize.x, lineY);
        guideLine[1].color = sf::Color(220, 220, 100, 100);
        window.draw(guideLine);
        
        // Value label on the right (outside canvas)
        sf::Text hoverLabel(font, formatFloat(hoverVal), 10);
        hoverLabel.setFillColor(sf::Color(255, 255, 150));
        hoverLabel.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, lineY - 6));
        window.draw(hoverLabel);
        
        // Value label inside canvas left edge with dark background for readability
        if (font.getInfo().family != "") {
            sf::Text inlineLabel(font, formatFloat(hoverVal), 10);
            inlineLabel.setFillColor(sf::Color(255, 255, 150));
            sf::FloatRect bounds = inlineLabel.getLocalBounds();
            float lx = canvasPos.x + 4.0f;
            float ly = lineY - bounds.size.y - 4.0f;
            // Clamp so it stays inside the canvas
            if (ly < canvasPos.y) ly = lineY + 3.0f;
            
            sf::RectangleShape bg(sf::Vector2f(bounds.size.x + 6.0f, bounds.size.y + 4.0f));
            bg.setFillColor(sf::Color(20, 20, 25, 200));
            bg.setPosition(sf::Vector2f(lx - 2.0f, ly - 1.0f));
            window.draw(bg);
            inlineLabel.setPosition(sf::Vector2f(lx, ly));
            window.draw(inlineLabel);
        }
    }
    
    // Draw frame scale labels at canvas bottom
    const int frameMarkers = 5;
    for (int i = 0; i <= frameMarkers; ++i) {
        float x = canvasPos.x + (canvasSize.x / frameMarkers) * i;
        int frame = static_cast<int>((i / static_cast<float>(frameMarkers)) * totalFrames);
        
        sf::Text frameText(font, std::to_string(frame), 9);
        frameText.setFillColor(sf::Color(140, 160, 160));
        frameText.setPosition(sf::Vector2f(x - 10, canvasPos.y + canvasSize.y + 3));
        window.draw(frameText);
    }
}

void AutomationWindow::drawConnectingLines() {
    if (selectedEffectIndex < 0 || selectedParam.empty()) return;
    
    auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
    auto keyframes = automation.getAllKeyframes();
    
    if (keyframes.size() < 2) return;
    
    float minVal = automation.getMinValue();
    float maxVal = automation.getMaxValue();
    float range = maxVal - minVal;
    if (range <= 0) range = 1.0f;
    
    // Sort keyframes by frame
    std::sort(keyframes.begin(), keyframes.end(), 
        [](const Keyframe& a, const Keyframe& b) { return a.frame < b.frame; });
    
    // Draw lines between consecutive keyframes
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        const auto& kf1 = keyframes[i];
        const auto& kf2 = keyframes[i + 1];
        
        float x1 = canvasPos.x + (kf1.frame / static_cast<float>(totalFrames)) * canvasSize.x;
        float y1 = canvasPos.y + canvasSize.y - (kf1.value * canvasSize.y);
        
        float x2 = canvasPos.x + (kf2.frame / static_cast<float>(totalFrames)) * canvasSize.x;
        float y2 = canvasPos.y + canvasSize.y - (kf2.value * canvasSize.y);
        
        // Get actual values and their percentage within the range
        float actual1 = automation.getActualValueAtFrame(kf1.frame);
        float actual2 = automation.getActualValueAtFrame(kf2.frame);
        float pct1 = (actual1 - minVal) / range;
        float pct2 = (actual2 - minVal) / range;
        pct1 = std::clamp(pct1, 0.0f, 1.0f);
        pct2 = std::clamp(pct2, 0.0f, 1.0f);
        
        // Color brightness scales with actual value percentage
        uint8_t a1 = static_cast<uint8_t>(80 + static_cast<int>(pct1 * 175));
        uint8_t a2 = static_cast<uint8_t>(80 + static_cast<int>(pct2 * 175));
        
        sf::Vertex v1;
        v1.position = sf::Vector2f(x1, y1);
        v1.color = sf::Color(static_cast<uint8_t>(60 + pct1 * 100), 
                             static_cast<uint8_t>(120 + pct1 * 135), 
                             255, a1);
        lines.append(v1);
        
        sf::Vertex v2;
        v2.position = sf::Vector2f(x2, y2);
        v2.color = sf::Color(static_cast<uint8_t>(60 + pct2 * 100),
                             static_cast<uint8_t>(120 + pct2 * 135),
                             255, a2);
        lines.append(v2);
    }
    
    window.draw(lines);
}

void AutomationWindow::drawNodes() {
    if (selectedEffectIndex < 0 || selectedParam.empty()) return;
    
    auto& automation = effectAutomations[selectedEffectIndex][selectedParam];
    auto keyframes = automation.getAllKeyframes();
    
    float minVal = automation.getMinValue();
    float maxVal = automation.getMaxValue();
    float range = maxVal - minVal;
    if (range <= 0) range = 1.0f;
    
    for (const auto& kf : keyframes) {
        float x = canvasPos.x + (kf.frame / static_cast<float>(totalFrames)) * canvasSize.x;
        float y = canvasPos.y + canvasSize.y - (kf.value * canvasSize.y);
        
        // Determine node appearance based on state
        bool isSelected = (kf.frame == selectedKeyframe && draggingKeyframe);
        bool isHovered = (kf.frame == hoveredKeyframe);
        
        float radius = NODE_RADIUS;
        sf::Color fillColor = sf::Color(100, 200, 100);  // Default: green
        sf::Color outlineColor = sf::Color(150, 255, 150);
        float outlineWidth = 1.5f;
        
        if (isSelected) {
            // Selected node: larger, bright yellow with thick outline
            radius = NODE_HOVER_RADIUS + 2.0f;
            fillColor = sf::Color(255, 255, 100);
            outlineColor = sf::Color(255, 255, 200);
            outlineWidth = 2.5f;
        } else if (isHovered) {
            // Hovered node: slightly larger, bright cyan
            radius = NODE_HOVER_RADIUS;
            fillColor = sf::Color(100, 255, 255);
            outlineColor = sf::Color(150, 255, 255);
            outlineWidth = 2.0f;
        } else {
            // Color/size based on actual value within the range
            float actualValue = automation.getActualValueAtFrame(kf.frame);
            float pct = std::clamp((actualValue - minVal) / range, 0.0f, 1.0f);
            
            radius = NODE_RADIUS * (0.6f + 0.8f * pct);
            // Color shifts from dim green (low values) to bright lime/yellow (high values)
            uint8_t r = static_cast<uint8_t>(60  + static_cast<int>(pct * 200));
            uint8_t g = static_cast<uint8_t>(160 + static_cast<int>(pct * 95));
            uint8_t b = static_cast<uint8_t>(60  - static_cast<int>(pct * 40));
            fillColor = sf::Color(r, g, b);
            outlineColor = sf::Color(r, g, b, 180);
        }
        
        // Draw node circle
        sf::CircleShape node(radius);
        node.setPosition(sf::Vector2f(x - radius, y - radius));
        node.setFillColor(fillColor);
        node.setOutlineColor(outlineColor);
        node.setOutlineThickness(outlineWidth);
        window.draw(node);
        
        // Draw value label above node (when selected or hovered)
        if ((isSelected || isHovered) && font.getInfo().family != "") {
            auto& automation_ref = effectAutomations[selectedEffectIndex][selectedParam];
            float actualValue = automation_ref.getActualValueAtFrame(kf.frame);
            
            sf::Text valueLabel(font, formatFloat(actualValue), 9);
            valueLabel.setFillColor(sf::Color::White);
            valueLabel.setPosition(sf::Vector2f(x - 15, y - radius - 20));
            window.draw(valueLabel);
        }
    }
}
