#pragma once
#include <SFML/Graphics.hpp>
#include <string> // <-- Убедись, что строка подключена

class IShape {
public:
    virtual ~IShape() = default;

    virtual void draw(sf::RenderTarget& target) const = 0;
    virtual bool contains(sf::Vector2f point) const = 0;
    virtual void updateGeometry() = 0;
    virtual void showImGuiProperties() = 0;

    virtual void setPosition(sf::Vector2f position) = 0;
    virtual sf::Vector2f getPosition() const = 0;

    virtual void setSize(sf::Vector2f size) = 0;
    virtual sf::Vector2f getSize() const = 0;

    virtual void setSelected(bool selected) = 0;
    virtual bool isSelected() const = 0;

    virtual void drawSelection(sf::RenderTarget& target) const = 0;
    virtual int getHitHandle(sf::Vector2f mousePos) const = 0;

    // --- НОВОЕ: Добавляем методы для групп в интерфейс ---
    virtual void setGroupId(const std::string& id) = 0;
    virtual std::string getGroupId() const = 0;
    // Добавь это в конец IShape:
    virtual sf::FloatRect getBounds() const = 0; // Для глобальной рамки

    // Точка привязки и вращение (на будущее)
    virtual void setAnchorOffset(sf::Vector2f offset) = 0;
    virtual sf::Vector2f getAnchorOffset() const = 0;
    virtual void setRotation(float angle) = 0;
    virtual float getRotation() const = 0;
    // Добавь эти методы в конец IShape
    virtual void setGlobalColor(sf::Color color) = 0;
    virtual void setGlobalThickness(float thickness) = 0;
    virtual void setGlobalFillColor(sf::Color color) = 0; // <-- НОВОЕ
    // Выделение отдельной стороны
    virtual int getHitSideIndex(sf::Vector2f mousePos) const = 0;
    virtual void setSelectedSide(int index) = 0;
    virtual int getSelectedSide() const = 0;

    // Движение отдельной вершины
    virtual void moveVertex(int index, sf::Vector2f delta) = 0;
    // --- СОХРАНЕНИЕ И ЗАГРУЗКА ---
    virtual std::string getType() const = 0; // Возвращает "Polygon" или "Circle"
    virtual void save(std::ostream& out) const = 0;
    virtual void load(std::istream& in) = 0;
};