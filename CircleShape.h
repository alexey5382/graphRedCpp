#pragma once
#include "BaseShape.h"

class CircleShape : public BaseShape {
private:
    size_t m_pointCount; // Качество круга (количество сегментов)

    sf::VertexArray m_fillGeometry;
    sf::VertexArray m_strokeGeometry;

public:
    CircleShape(sf::Vector2f position, sf::Vector2f size);

    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;
    void updateGeometry() override;
    void showImGuiProperties() override;

    void setStrokeColor(sf::Color color);
    void setStrokeThickness(float thickness);
    std::string getType() const override { return "Circle"; }
    void save(std::ostream& out) const override;
    void load(std::istream& in) override;
};