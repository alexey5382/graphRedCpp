#pragma once
#include "BaseShape.h"
#include <vector>

class CircleShape : public BaseShape {
private:
    size_t m_pointCount;

    sf::VertexArray m_fillGeometry;
    sf::VertexArray m_strokeGeometry;

    // --- НОВОЕ: Круг теперь тоже имеет вершины для идеального масштаба! ---
    std::vector<sf::Vector2f> m_basePoints;

public:
    CircleShape(sf::Vector2f position, sf::Vector2f size);

    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;
    void updateGeometry() override;
    void showImGuiProperties() override;

    sf::FloatRect getBounds() const override;

    void setStrokeColor(sf::Color color);
    void setStrokeThickness(float thickness);
    std::string getType() const override { return "Circle"; }
    void save(std::ostream& out) const override;
    void load(std::istream& in) override;

    // --- НОВОЕ: Переопределяем методы вершин ---
    std::vector<sf::Vector2f> getBasePoints() const override { return m_basePoints; }
    void setBasePoints(const std::vector<sf::Vector2f>& pts) override { m_basePoints = pts; updateGeometry(); }
    void applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) override;
};