#pragma once
#include "BaseShape.h"
#include <vector>

class SHAPE_API PolygonShape : public BaseShape {
private:
    std::vector<sf::Vector2f> m_basePoints;

    sf::VertexArray m_fillGeometry;   
    sf::VertexArray m_strokeGeometry; 

    
    sf::Vector2f getIntersection(sf::Vector2f p1, sf::Vector2f dir1,
        sf::Vector2f p2, sf::Vector2f dir2,
        sf::Vector2f fallback, sf::Vector2f corner) const;

public:
    // Конструктор
    virtual ~PolygonShape() = default; 
    PolygonShape(sf::Vector2f position, sf::Vector2f size, bool isClosed,
        const std::vector<sf::Vector2f>& basePoints);

    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;
    void updateGeometry() override;
    void showImGuiProperties() override;

    void setSideColor(size_t index, sf::Color color);
    void setSideThickness(size_t index, float thickness);
    sf::FloatRect getBounds() const override;
    //
    void drawSelection(sf::RenderTarget& target) const override;
    int getHitHandle(sf::Vector2f mousePos) const override;
    int getHitSideIndex(sf::Vector2f mousePos) const override;
    void moveVertex(int index, sf::Vector2f delta) override;
    //в файл
    std::string getType() const override { return "Polygon"; }
    void save(std::ostream& out) const override;
    void load(std::istream& in) override;
    //--
    std::vector<sf::Vector2f> getBasePoints() const override { return m_basePoints; }
    void setBasePoints(const std::vector<sf::Vector2f>& pts) override { m_basePoints = pts; updateGeometry(); }
    void applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) override;

    //new json
    nlohmann::json toJson() const override;
    void fromJson(const nlohmann::json& j) override;

};
