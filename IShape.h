#pragma once
#include <SFML/Graphics.hpp>
#include <string> // <-- Убедись, что строка подключена

// --- НОВОЕ: Структура снимка теперь доступна всем фигурам ---
// --- НОВОЕ: Структура снимка теперь поддерживает вершины! ---
struct ShapeSnapshot {
    sf::Vector2f position;
    sf::Vector2f size;
    sf::Vector2f anchorOffset;
    float rotation;
    std::vector<sf::Vector2f> basePoints;
};

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

    // --- Добавь эти методы в класс IShape ---
    virtual void setId(int id) = 0;
    virtual int getId() const = 0;
    virtual void setName(const std::string& name) = 0;
    virtual std::string getName() const = 0;

    // --- ИЗМЕНИ тип аргументов с std::string на int ---
    virtual void setGroupId(int id) = 0;
    virtual int getGroupId() const = 0;

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
    //
    virtual void setSelectedVertex(int index) = 0;
    virtual int getSelectedVertex() const = 0;
    // Движение отдельной вершины
    virtual void moveVertex(int index, sf::Vector2f delta) = 0;
    // --- СОХРАНЕНИЕ И ЗАГРУЗКА ---
    virtual std::string getType() const = 0; // Возвращает "Polygon" или "Circle"
    virtual void save(std::ostream& out) const = 0;
    virtual void load(std::istream& in) = 0;
    // Добавь эти методы в конец IShape
    virtual void setAnchorPositionWorld(sf::Vector2f newWorldAnchor) = 0;
    virtual void setRotationFromMouse(sf::Vector2f mousePos) = 0;
    virtual void scaleFromHandle(int handle, sf::Vector2f mousePos) = 0;
    //--
    virtual void resizeFromBoundingBox(float newWidth, float newHeight) = 0;
    //--
    virtual std::vector<sf::Vector2f> getBasePoints() const = 0;
    virtual void setBasePoints(const std::vector<sf::Vector2f>& pts) = 0;
    virtual void applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) = 0;
};