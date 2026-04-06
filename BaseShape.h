#pragma once
#include "IShape.h"
#include <vector>
#include <string>

class BaseShape : public IShape {
protected:
    // Базовые параметры, общие для всех фигур
    sf::Vector2f m_position; // Позиция (твой LocalAnchor)
    sf::Vector2f m_size;     // SizeX и SizeY

    // Параметры сторон (векторы, так как сторон может быть много)
    std::vector<sf::Color> m_sideColors;
    std::vector<float> m_sideThicknesses;
    sf::Color m_fillColor;
    //
    sf::Vector2f m_anchorOffset; // Смещение якоря относительно m_position
    float m_rotation;            // Угол поворота (на будущее)
    // Состояния
    bool m_isSelected;
    bool m_isClosed;
    std::string m_groupId; // ID группы, если фигура сгруппирована
    int m_selectedSide = -1; // -1 означает, что выделена вся фигура
    int m_selectedVertex = -1; // <-- НОВОЕ: -1 означает, что ничего не выделено

public:
    BaseShape(sf::Vector2f position, sf::Vector2f size, bool isClosed);
    virtual ~BaseShape() = default;

    static sf::Vector2f rotatePoint(sf::Vector2f point, sf::Vector2f anchor, float angleDeg);

    // Геттеры и сеттеры для общих свойств
    void setPosition(sf::Vector2f position);
    sf::Vector2f getPosition() const;

    void setSelected(bool selected);
    bool isSelected() const;

    void setGroupId(const std::string& id);
    std::string getGroupId() const;
    //
    void setSize(sf::Vector2f size);
    sf::Vector2f getSize() const;
    void drawSelection(sf::RenderTarget& target) const;
    int getHitHandle(sf::Vector2f mousePos) const;
    // Методы IShape оставляем виртуальными (override), 
    // чтобы наследники (Circle, Polygon) реализовали их по-своему
    sf::FloatRect getBounds() const override;
    void setAnchorOffset(sf::Vector2f offset) override;
    sf::Vector2f getAnchorOffset() const override;
    void setRotation(float angle) override;
    float getRotation() const override;
    void setAnchorPositionWorld(sf::Vector2f newWorldAnchor) override;
    void setRotationFromMouse(sf::Vector2f mousePos) override;
    void scaleFromHandle(int handle, sf::Vector2f mousePos) override;
    //--
    void resizeFromBoundingBox(float newWidth, float newHeight) override;
    //
    void setGlobalColor(sf::Color color) override;
    void setGlobalThickness(float thickness) override;
    void setGlobalFillColor(sf::Color color) override; // <-- НОВОЕ
    int getHitSideIndex(sf::Vector2f mousePos) const override { return -1; }
    void setSelectedSide(int index) override { m_selectedSide = index; }
    int getSelectedSide() const override { return m_selectedSide; }
    void moveVertex(int index, sf::Vector2f delta) override {} // По умолчанию ничего не делает
    //
    void setSelectedVertex(int index) override { m_selectedVertex = index; }
    int getSelectedVertex() const override { return m_selectedVertex; }
    //в файл
    void save(std::ostream& out) const override;
    void load(std::istream& in) override;
    //--
    std::vector<sf::Vector2f> getBasePoints() const override { return {}; }
    void setBasePoints(const std::vector<sf::Vector2f>& pts) override {}
    void applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) override;
};