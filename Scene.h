#pragma once
#include <vector>
#include <memory>
#include <string>
#include <SFML/Graphics.hpp>
#include "IShape.h"

// Структура группы, как в твоем C# коде
struct ShapeGroup {
    std::string id;
    std::string name;
    sf::Vector2f anchorPoint; // Глобальный якорь группы
};

class Scene {
private:
    std::vector<std::unique_ptr<IShape>> m_shapes;
    std::vector<IShape*> m_selectedShapes;
    std::vector<ShapeGroup> m_groups;

    bool m_isDragging = false;
    int m_draggedHandle = 0;
    sf::Vector2f m_lastMousePos;

    // --- НОВОЕ: Переменные для рамки выделения ---
    bool m_isBoxSelecting = false;
    sf::Vector2f m_selectionStartPos;
    // Режим интерактивного рисования
    bool m_isDrawingMode = false;
    std::vector<sf::Vector2f> m_drawingPoints;
    sf::Vector2f m_currentMousePos;
public:
    void addShape(std::unique_ptr<IShape> shape);
    void draw(sf::RenderTarget& target) const;
    void clear();

    // Геттеры для UI
    const std::vector<IShape*>& getSelectedShapes() const { return m_selectedShapes; }
    std::vector<ShapeGroup>& getGroups() { return m_groups; }

    // --- ОБНОВЛЕНО: Поддержка клавиши Shift ---
    void handleMousePress(sf::Vector2f mousePos, bool isShiftPressed, bool isCtrlPressed);
    void handleMouseRelease(bool isShiftPressed);
    void handleMouseMove(sf::Vector2f mousePos);

    void groupSelected();
    void ungroupSelected(const std::string& groupId);
    void selectGroup(const std::string& groupId);
    void clearSelection();

    void bringToFront(IShape* shape);
    void sendToBack(IShape* shape);
    void cleanupEmptyGroups();
    ShapeGroup* getFormalSelectedGroup();
    void startDrawingMode();
    void finishDrawing(bool isClosed);
    void cancelDrawing();
    // в файл
    void deleteSelected();
    void saveToFile(const std::string& filename) const;
    void loadFromFile(const std::string& filename);
};