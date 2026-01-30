#include "AutomationWindow.h"
#include <iostream>
#include <cmath>

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
    auto instructions = tgui::Label::create("Double-click to add keyframe. Drag to edit. Right-click to delete.");
    instructions->setPosition(10, 65);
    instructions->setTextSize(12);
    gui.add(instructions);
    
    // Setup canvas area (drawn manually with SFML)
    canvasPos = sf::Vector2f(10, 90);
    canvasSize = sf::Vector2f(780, 300);
    canvasBackground.setSize(canvasSize);
    canvasBackground.setPosition(canvasPos);
    canvasBackground.setFillColor(sf::Color(40, 40, 40));
    canvasBackground.setOutlineColor(sf::Color(100, 100, 100));
    canvasBackground.setOutlineThickness(2.0f);
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
        
        // Handle canvas interactions
        if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            float mx = static_cast<float>(mousePressed->position.x);
            float my = static_cast<float>(mousePressed->position.y);
            
            if (mx >= canvasPos.x && mx <= canvasPos.x + canvasSize.x &&
                my >= canvasPos.y && my <= canvasPos.y + canvasSize.y) {
                
                sf::Vector2f localPos(mx - canvasPos.x, my - canvasPos.y);
                
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    // Check for double-click
                    static sf::Clock doubleClickClock;
                    static bool waitingForDoubleClick = false;
                    
                    if (waitingForDoubleClick && doubleClickClock.getElapsedTime().asMilliseconds() < 300) {
                        // Check if double-clicking near a keyframe to delete it
                        bool deletedKeyframe = false;
                        if (!selectedEffect.empty() && !selectedParam.empty()) {
                            auto& automation = effectAutomations[selectedEffect][selectedParam];
                            auto keyframes = automation.getAllKeyframes();
                            
                            for (const auto& kf : keyframes) {
                                float kfX = (kf.frame / static_cast<float>(totalFrames)) * canvasSize.x;
                                float kfY = canvasSize.y - (kf.value * canvasSize.y);
                                
                                float dist = std::sqrt((localPos.x - kfX) * (localPos.x - kfX) + 
                                                      (localPos.y - kfY) * (localPos.y - kfY));
                                if (dist < 10.0f) {
                                    automation.removeKeyframe(kf.frame);
                                    deletedKeyframe = true;
                                    break;
                                }
                            }
                        }
                        
                        // Only add keyframe if we didn't delete one
                        if (!deletedKeyframe) {
                            handleCanvasClick(localPos, true);
                        }
                        waitingForDoubleClick = false;
                    } else {
                        handleCanvasClick(localPos, false);
                        waitingForDoubleClick = true;
                        doubleClickClock.restart();
                    }
                }
            }
        } else if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouseReleased->button == sf::Mouse::Button::Left) {
                draggingKeyframe = false;
                selectedKeyframe = -1;
            }
        } else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
            if (draggingKeyframe) {
                float mx = static_cast<float>(mouseMoved->position.x);
                float my = static_cast<float>(mouseMoved->position.y);
                sf::Vector2f localPos(mx - canvasPos.x, my - canvasPos.y);
                handleCanvasDrag(localPos);
            }
        }
    }
}

void AutomationWindow::handleCanvasClick(sf::Vector2f pos, bool doubleClick) {
    if (selectedEffect.empty() || selectedParam.empty()) {
        std::cout << "No effect/param selected for automation" << std::endl;
        return;
    }
    
    // Convert position to frame and value
    int frame = static_cast<int>((pos.x / canvasSize.x) * totalFrames);
    float value = 1.0f - (pos.y / canvasSize.y);
    value = std::max(0.0f, std::min(1.0f, value));
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    
    if (doubleClick) {
        // Add new keyframe
        automation.addKeyframe(frame, value);
        std::cout << "Added keyframe at frame " << frame << " with value " << value << std::endl;
    } else {
        // Check if clicking near existing keyframe to drag it
        auto keyframes = automation.getAllKeyframes();
        for (const auto& kf : keyframes) {
            float kfX = (kf.frame / static_cast<float>(totalFrames)) * canvasSize.x;
            float kfY = canvasSize.y - (kf.value * canvasSize.y);
            
            float dist = std::sqrt((pos.x - kfX) * (pos.x - kfX) + (pos.y - kfY) * (pos.y - kfY));
            if (dist < 10.0f) {
                selectedKeyframe = kf.frame;
                draggingKeyframe = true;
                break;
            }
        }
    }
}

void AutomationWindow::handleCanvasDrag(sf::Vector2f pos) {
    if (selectedKeyframe < 0 || selectedEffect.empty() || selectedParam.empty()) {
        return;
    }
    
    // Calculate new value based on y position (don't change frame)
    float value = 1.0f - (pos.y / canvasSize.y);
    value = std::max(0.0f, std::min(1.0f, value));
    
    auto& automation = effectAutomations[selectedEffect][selectedParam];
    automation.addKeyframe(selectedKeyframe, value);
}

void AutomationWindow::update() {
    // Update is now just for drawing, actual rendering happens in draw()
}

void AutomationWindow::draw() {
    if (!window.isOpen()) return;
    
    window.clear(sf::Color(50, 50, 50));
    
    // Draw canvas background
    window.draw(canvasBackground);
    
    // Draw automation curve if effect and param are selected
    if (!selectedEffect.empty() && !selectedParam.empty()) {
        auto& automation = effectAutomations[selectedEffect][selectedParam];
        auto keyframes = automation.getAllKeyframes();
        
        // Update range label
        float minVal = automation.getMinValue();
        float maxVal = automation.getMaxValue();
        rangeLabel->setText("Range: " + std::to_string(minVal) + " - " + std::to_string(maxVal));
        
        // Draw value labels on canvas
        if (font.getInfo().family != "") {
            sf::Text maxText(font, std::to_string(maxVal), 10);
            maxText.setFillColor(sf::Color::White);
            maxText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 5, canvasPos.y - 5));
            window.draw(maxText);
            
            sf::Text minText(font, std::to_string(minVal), 10);
            minText.setFillColor(sf::Color::White);
            minText.setPosition(sf::Vector2f(canvasPos.x + canvasSize.x + 5, canvasPos.y + canvasSize.y - 15));
            window.draw(minText);
        }
        
        if (!keyframes.empty()) {
            // Draw interpolation curve
            sf::VertexArray curve(sf::PrimitiveType::LineStrip);
            for (int frame = 0; frame < totalFrames; ++frame) {
                float value = automation.getValueAtFrame(frame);
                float x = canvasPos.x + (frame / static_cast<float>(totalFrames)) * canvasSize.x;
                float y = canvasPos.y + canvasSize.y - (value * canvasSize.y);
                
                sf::Vertex vert;
                vert.position = sf::Vector2f(x, y);
                vert.color = sf::Color::Green;
                curve.append(vert);
            }
            window.draw(curve);
            
            // Draw keyframe markers
            for (const auto& kf : keyframes) {
                float x = canvasPos.x + (kf.frame / static_cast<float>(totalFrames)) * canvasSize.x;
                float y = canvasPos.y + canvasSize.y - (kf.value * canvasSize.y);
                
                // Vertical line
                sf::VertexArray line(sf::PrimitiveType::Lines, 2);
                line[0].position = sf::Vector2f(x, canvasPos.y);
                line[0].color = sf::Color::Red;
                line[1].position = sf::Vector2f(x, canvasPos.y + canvasSize.y);
                line[1].color = sf::Color::Red;
                window.draw(line);
                
                // Control point
                sf::CircleShape point(5.0f);
                point.setPosition(sf::Vector2f(x - 5.0f, y - 5.0f));
                point.setFillColor(sf::Color::Yellow);
                window.draw(point);
            }
        }
    }
    
    gui.draw();
    window.display();
}
