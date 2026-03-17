#include <SFML/Graphics.hpp>
#include "GUI.h"
#include <iostream>

int main() {
    std::cout << "=== VidMod ===" << std::endl;
    std::cout << "Welcome to VidMod!" << std::endl;
    std::cout << "Use the GUI to:" << std::endl;
    std::cout << "  1. Load a video and audio file" << std::endl;
    std::cout << "  2. Add effects to the chain" << std::endl;
    std::cout << "  3. Configure effect parameters" << std::endl;
    std::cout << "  4. Preview a single frame" << std::endl;
    std::cout << "  5. Process the entire video" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Creating window..." << std::endl;
    
    // Create the main window with default style (titlebar, resize, close)
    sf::RenderWindow window(sf::VideoMode({1600, 900}), "VidMod", sf::Style::Default);
    window.setFramerateLimit(60);
    window.requestFocus(); // Request focus on macOS
    
    std::cout << "Window created. Size: " << window.getSize().x << "x" << window.getSize().y << std::endl;
    std::cout << "Is window open: " << window.isOpen() << std::endl;
    
    // Create GUI
    std::cout << "Creating GUI..." << std::endl;
    GUI gui(window);
    std::cout << "GUI created." << std::endl;
    std::cout << "GUI created." << std::endl;
    
    std::cout << "Entering main loop..." << std::endl;
    
    // Main loop
    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            
            gui.handleEvent(*event);
        }
        
        window.clear(sf::Color::White);
        gui.draw();
        window.display();
    }
    
    return 0;
}
