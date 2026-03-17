#include "AutomationWindow.h"
#include <iostream>
#include <cmath>
#include <algorithm>

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
    auto effectLabel = tgui::Label::create("Effect:");
    effectLabel->setPosition(10, 10);
    effectLabel->setTextSize(14);
    gui.add(effectLabel);
    
    effectComboBox = tgui::ComboBox::create();
    effectComboBox->setPosition(70, 10);
    effectComboBox->setSize(200, 25);
    effectComboBox->onItemSelect([this](int) {
        if (effectComboBox->getSelectedItemIndex() >= 0) {
            selectedEffect = effectComboBox->getSelectedItem().toStdString();
            updateParamList();
        }
    });
    gui.add(effectComboBox);
    
    // Parameter selector
    auto paramLabel = tgui::Label::create("Parameter:");
    paramLabel->setPosition(290, 10);
    paramLabel->setTextSize(14);
    gui.add(paramLabel);
    
    paramComboBox = tgui::ComboBox::create();
    paramComboBox->setPosition(380, 10);
    paramComboBox->setSize(200, 25);
    paramComboBox->onItemSelect([this](int) {
        if (paramComboBox->getSelectedItemIndex() >= 0) {
            selectedParam = paramComboBox->getSelectedItem().toStdString();
            
            // Get current parameter value
            float currentValue = 1.0f;
            if (effectChainRef && !selectedEffect.empty()) {
                const auto& effects = effectChainRef->getEffects();
                for (const auto& effect : effects) {
                    if (effect->getName() == selectedEffect) {
                        currentValue = effect->getParameter(selectedParam);
                        break;
                    }
                }
            }
            
            // Get multiplier from range selector
            int rangeIndex = rangeComboBox->getSelectedItemIndex();
            float multiplier = 1.0f;
            switch (rangeIndex) {
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
            float maxVal = currentValue * multiplier;
            
            // Initialize automation with calculated range if not exists
            if (!selectedEffect.empty() && effectAutomations[selectedEffect].count(selectedParam) == 0) {
                effectAutomations[selectedEffect][selectedParam] = ParameterAutomation(minVal, maxVal);
            }
            
            // Update range label to show current automation range
            if (!selectedEffect.empty() && effectAutomations[selectedEffect].count(selectedParam) > 0) {
                auto& automation = effectAutomations[selectedEffect][selectedParam];
                rangeLabel->setText("Range: " + tgui::String(automation.getMinValue()) + " - " + tgui::String(automation.getMaxValue()));
            }
        }
    });
    gui.add(paramComboBox);
    
    // Range selector
    auto rangeLabel2 = tgui::Label::create("Range:");
    rangeLabel2->setPosition(600, 10);
    rangeLabel2->setTextSize(14);
    gui.add(rangeLabel2);
    
    rangeComboBox = tgui::ComboBox::create();
    rangeComboBox->setPosition(660, 10);
    rangeComboBox->setSize(120, 25);
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
        // Get current parameter value if parameter is selected
        float currentValue = 1.0f;
        if (effectChainRef && !selectedEffect.empty() && !selectedParam.empty()) {
            const auto& effects = effectChainRef->getEffects();
            for (const auto& effect : effects) {
                if (effect->getName() == selectedEffect) {
                    currentValue = effect->getParameter(selectedParam);
                    break;
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
        float maxVal = currentValue * multiplier;
        
        // Always update the range label
        rangeLabel->setText("Range: " + tgui::String(minVal) + " - " + tgui::String(maxVal));
        
        // Update automation range if parameter is selected
        if (!selectedEffect.empty() && !selectedParam.empty()) {
            // If automation exists, update its range (preserving keyframes)
            if (effectAutomations[selectedEffect].count(selectedParam) > 0) {
                effectAutomations[selectedEffect][selectedParam].setRange(minVal, maxVal);
            } else {
                // Create new automation with this range
                effectAutomations[selectedEffect][selectedParam] = ParameterAutomation(minVal, maxVal);
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
}

void AutomationWindow::open(EffectChain& chain) {
    if (!window.isOpen()) {
        window.create(sf::VideoMode({800, 400}), "Parameter Automation");
    }
    
    effectChainRef = &chain;
    
    // Populate effect list
    effectComboBox->removeAllItems();
    const auto& effects = chain.getEffects();
    for (const auto& effect : effects) {
        effectComboBox->addItem(effect->getName());
    }
    
    selectedEffect = "";
    selectedParam = "";
    paramComboBox->removeAllItems();
}

void AutomationWindow::updateParamList() {
    paramComboBox->removeAllItems();
    selectedParam = "";
    
    if (!effectChainRef || selectedEffect.empty()) {
        return;
    }
    
    // Find the selected effect and get its parameters
    const auto& effects = effectChainRef->getEffects();
    for (const auto& effect : effects) {
        if (effect->getName() == selectedEffect) {
            const auto& paramNames = effect->getParameterNames();
            for (const auto& paramName : paramNames) {
                paramComboBox->addItem(paramName);
            }
            break;
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
                updateHoveredKeyframe(localPos);
                
                // Continue dragging
                if (draggingKeyframe) {
                    handleCanvasDrag(localPos);
                }
            } else {
                hoveredKeyframe = -1;
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
                    // Check for double-click (add new node)
                    static sf::Clock doubleClickClock;
                    static bool waitingForDoubleClick = false;
                    
                    int keyframeAtPos = findKeyframeAtPosition(localPos);
                    
                    if (waitingForDoubleClick && doubleClickClock.getElapsedTime().asMilliseconds() < 300) {
                        // Double-click - add new keyframe
                        handleCanvasClick(localPos, true);
                        waitingForDoubleClick = false;
                    } else {
                        // Single click - select/drag existing keyframe
                        if (keyframeAtPos >= 0) {
                            selectedKeyframe = keyframeAtPos;
                            draggingKeyframe = true;
                        } else {
                            // Click on empty canvas
                            handleCanvasClick(localPos, false);
                        }
                        
                        waitingForDoubleClick = true;
                        doubleClickClock.restart();
                    }
                } else if (mousePressed->button == sf::Mouse::Button::Right) {
                    // Right-click to delete node
                    int keyframeAtPos = findKeyframeAtPosition(localPos);
                    if (keyframeAtPos >= 0 && !selectedEffect.empty() && !selectedParam.empty()) {
                        auto& automation = effectAutomations[selectedEffect][selectedParam];
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
    if (selectedEffect.empty() || selectedParam.empty()) {
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
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    automation.addKeyframe(frame, value);
}

void AutomationWindow::handleCanvasDrag(sf::Vector2f pos) {
    if (selectedKeyframe < 0 || selectedEffect.empty() || selectedParam.empty()) {
        return;
    }
    
    // Calculate new position (frame and value)
    int newFrame = static_cast<int>((pos.x / canvasSize.x) * totalFrames);
    newFrame = std::max(0, std::min(newFrame, totalFrames - 1));
    
    float newValue = 1.0f - (pos.y / canvasSize.y);
    newValue = std::max(0.0f, std::min(1.0f, newValue));
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    
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
    if (selectedEffect.empty() || selectedParam.empty()) {
        return -1;
    }
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
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
    if (!selectedEffect.empty() && !selectedParam.empty()) {
        auto& automation = effectAutomations[selectedEffect][selectedParam];
        
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
        rangeLabel->setText("Range: " + std::to_string(static_cast<int>(minVal)) + " - " + std::to_string(static_cast<int>(maxVal)));
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
    
    if (selectedEffect.empty() || selectedParam.empty()) return;
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    float minVal = automation.getMinValue();
    float maxVal = automation.getMaxValue();
    
    // Draw max value label
    sf::Text maxText(font, std::to_string(static_cast<int>(maxVal)), 10);
    maxText.setFillColor(sf::Color(180, 200, 200));
    maxText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, canvasPos.y - 8));
    window.draw(maxText);
    
    // Draw mid value label
    float midVal = (minVal + maxVal) / 2.0f;
    sf::Text midText(font, std::to_string(static_cast<int>(midVal)), 10);
    midText.setFillColor(sf::Color(160, 180, 180));
    midText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, canvasPos.y + canvasSize.y / 2 - 5));
    window.draw(midText);
    
    // Draw min value label
    sf::Text minText(font, std::to_string(static_cast<int>(minVal)), 10);
    minText.setFillColor(sf::Color(140, 160, 160));
    minText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 8, canvasPos.y + canvasSize.y - 12));
    window.draw(minText);
    
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
    if (selectedEffect.empty() || selectedParam.empty()) return;
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    auto keyframes = automation.getAllKeyframes();
    
    if (keyframes.size() < 2) return;
    
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
        
        sf::Vertex v1;
        v1.position = sf::Vector2f(x1, y1);
        v1.color = sf::Color(100, 180, 255);
        lines.append(v1);
        
        sf::Vertex v2;
        v2.position = sf::Vector2f(x2, y2);
        v2.color = sf::Color(100, 180, 255);
        lines.append(v2);
    }
    
    window.draw(lines);
}

void AutomationWindow::drawNodes() {
    if (selectedEffect.empty() || selectedParam.empty()) return;
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    auto keyframes = automation.getAllKeyframes();
    
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
        }
        
        // Draw node circle
        sf::CircleShape node(radius);
        node.setPosition(sf::Vector2f(x - radius, y - radius));
        node.setFillColor(fillColor);
        node.setOutlineColor(outlineColor);
        node.setOutlineThickness(outlineWidth);
        window.draw(node);
        
        // Draw value label above node (only if selected or hovered)
        if (isSelected || isHovered) {
            if (font.getInfo().family != "") {
                auto& automation_ref = effectAutomations[selectedEffect][selectedParam];
                float actualValue = automation_ref.getActualValueAtFrame(kf.frame);
                
                sf::Text valueLabel(font, std::to_string(static_cast<int>(actualValue)), 9);
                valueLabel.setFillColor(sf::Color::White);
                valueLabel.setPosition(sf::Vector2f(x - 15, y - radius - 20));
                window.draw(valueLabel);
            }
        }
    }
}
