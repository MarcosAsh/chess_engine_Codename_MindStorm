#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>

const int TILE_SIZE = 100;
const int BOARD_SIZE = 8;

bool pieceSelected = false;
sf::Vector2i selectedSquare(-1, -1); // Selected square

// Dummy function to check valid moves
bool isValidMove(sf::Vector2i from, sf::Vector2i to) {
    // Example: Allow any move within the board for testing
    return to.x >= 0 && to.x < BOARD_SIZE && to.y >= 0 && to.y < BOARD_SIZE;
}

void drawHighlights(sf::RenderWindow& window, sf::Vector2i selectedSquare) {
    sf::CircleShape highlight(TILE_SIZE / 4);
    highlight.setFillColor(sf::Color(255, 255, 0, 128)); // Semi-transparent yellow
    highlight.setOrigin(TILE_SIZE / 4, TILE_SIZE / 4);

    sf::Vector2f pos(selectedSquare.y * TILE_SIZE + TILE_SIZE / 2,
                     selectedSquare.x * TILE_SIZE + TILE_SIZE / 2);
    highlight.setPosition(pos);
    window.draw(highlight);
}

int main() {
    sf::RenderWindow window(sf::VideoMode(TILE_SIZE * BOARD_SIZE, TILE_SIZE * BOARD_SIZE), "Chess Input Example");

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                // Get clicked square
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                int row = mousePos.y / TILE_SIZE;
                int col = mousePos.x / TILE_SIZE;

                if (!pieceSelected) {
                    // First click: select piece
                    selectedSquare = sf::Vector2i(row, col);
                    pieceSelected = true;
                } else {
                    // Second click: attempt move
                    sf::Vector2i targetSquare(row, col);
                    if (isValidMove(selectedSquare, targetSquare)) {
                        std::cout << "Moved from (" << selectedSquare.x << ", " << selectedSquare.y << ") to ("
                                  << targetSquare.x << ", " << targetSquare.y << ")\n";
                        // Update board state here
                    } else {
                        std::cout << "Invalid move!\n";
                    }
                    pieceSelected = false; // Deselect after attempting a move
                }
            }
        }

        window.clear();

        // Draw chessboard
        for (int row = 0; row < BOARD_SIZE; ++row) {
            for (int col = 0; col < BOARD_SIZE; ++col) {
                sf::RectangleShape square(sf::Vector2f(TILE_SIZE, TILE_SIZE));
                square.setPosition(col * TILE_SIZE, row * TILE_SIZE);
                square.setFillColor((row + col) % 2 == 0 ? sf::Color::White : sf::Color::Black);
                window.draw(square);
            }
        }

        // Highlight selected piece
        if (pieceSelected) {
            drawHighlights(window, selectedSquare);
        }

        window.display();
    }

    return 0;
}
