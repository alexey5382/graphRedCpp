#pragma once
#include "BaseShape.h"
#include <vector>
#include <memory>
#include <algorithm>

class Group : public BaseShape {
private:
    std::vector<std::unique_ptr<IShape>> m_children;

public:
    Group(sf::Vector2f position);

    // Управление детьми
    void addChild(std::unique_ptr<IShape> shape);
    std::vector<std::unique_ptr<IShape>>& getChildren();

    // Реализация интерфейса IShape
    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;
    void updateGeometry() override;
    sf::FloatRect getBounds() const override;

    void setPosition(sf::Vector2f newPos) override;
    void setRotation(float angle) override;
    void applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) override;

    void setGlobalColor(sf::Color color) override;
    void setGlobalThickness(float thickness) override;
    void setGlobalFillColor(sf::Color color) override;

    std::string getType() const override;
    void save(std::ostream& out) const override;
    void load(std::istream& in) override;

    void showImGuiProperties() override;
    //--
    void captureState() override;
    void restoreState() override;
    IShape* getHitShape(sf::Vector2f mousePos) override;
};