#pragma once
#include "BaseShape.h"
#include <vector>
#include <utility>
#include <string>

class EllipseShape : public BaseShape {
private:
    sf::VertexArray m_fillGeometry;
    sf::VertexArray m_strokeGeometry;
    size_t m_pointCount;
    bool m_showOBB = false;
    float m_radiusX;
    float m_radiusY;
    struct { float radiusX; float radiusY; } m_ellipseSnap;

public:
    EllipseShape(sf::Vector2f position, float radiusX, float radiusY);

    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;
    void updateGeometry() override;
    void showImGuiProperties() override;

    sf::FloatRect getBounds() const override;

    void drawSelection(sf::RenderTarget& target) const override;

    void setStrokeColor(sf::Color color);
    void setStrokeThickness(float thickness);
    std::string getType() const override { return "Ellipse"; }
    void save(std::ostream& out) const override;
    void load(std::istream& in) override;

    std::vector<sf::Vector2f> getBasePoints() const override;
    void setBasePoints(const std::vector<sf::Vector2f>& pts) override;
    void applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) override;

    std::pair<sf::Vector2f, sf::Vector2f> getWorldFoci() const;
    void setFoci(sf::Vector2f f1_world, sf::Vector2f f2_world);
    void moveVertex(int index, sf::Vector2f delta) override;
    int getHitFocus(sf::Vector2f mousePos) const;
    void moveFocus(int index, sf::Vector2f newWorldPos);
    int getHitHandle(sf::Vector2f mousePos) const override;
    sf::Vector2f getOBBCorner(int handle) const;
    void resizeFromOBB(int handle, sf::Vector2f mouseWorldPos);
    void captureState() override;
    void restoreState() override;
};