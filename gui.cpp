#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>

// Constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int TILE_SIZE = 100;

void drawChessboard(sf::RenderWindow& window) {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            sf::RectangleShape square(sf::Vector2f(TILE_SIZE, TILE_SIZE));
            square.setPosition(col * TILE_SIZE, row * TILE_SIZE);
            square.setFillColor((row + col) % 2 == 0 ? sf::Color::White : sf::Color::Black);
            window.draw(square);
        }
    }
}

void drawPiece(sf::RenderWindow& window, sf::Texture& texture, int row, int col) {
    sf::Sprite piece(texture);
    piece.setPosition(col * TILE_SIZE, row * TILE_SIZE);
    piece.setScale(TILE_SIZE / 100.0f, TILE_SIZE / 100.0f); // Scale to fit tile size
    window.draw(piece);
}

int main() {
    // Initialize window
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Chess Game");

    // Load textures
    sf::Texture whitePawnTexture;
    if (!whitePawnTexture.loadFromFile("white_pawn.png")) {
        std::cerr << "Failed to load piece texture!\n";
        return 1;
    }

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        // Clear window
        window.clear();

        // Draw chessboard
        drawChessboard(window);

        // Draw a sample piece (e.g., white pawn at position (6, 4))
        drawPiece(window, whitePawnTexture, 6, 4);

        // Display window
        window.display();
    }

    return 0;
}
