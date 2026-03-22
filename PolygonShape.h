#pragma once
#include "BaseShape.h"
#include <vector>

class PolygonShape : public BaseShape {
private:
    std::vector<sf::Vector2f> m_basePoints; // Нормализованные точки (от -1 до 1)

    // Готовая геометрия для отрисовки видеокартой
    sf::VertexArray m_fillGeometry;   // Заливка (центр)
    sf::VertexArray m_strokeGeometry; // Обводка (стороны)

    // Вспомогательный математический метод для расчета пересечения линий (острые углы)
    sf::Vector2f getIntersection(sf::Vector2f p1, sf::Vector2f dir1,
        sf::Vector2f p2, sf::Vector2f dir2,
        sf::Vector2f fallback, sf::Vector2f corner) const;

public:
    // Конструктор
    PolygonShape(sf::Vector2f position, sf::Vector2f size, bool isClosed,
        const std::vector<sf::Vector2f>& basePoints);

    // Реализация чисто виртуальных методов из IShape
    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;
    void updateGeometry() override;
    void showImGuiProperties() override;

    // Сеттеры для изменения свойств сторон (после их вызова нужно дергать updateGeometry)
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
};