#include "BaseShape.h"
#include <iostream>

BaseShape::BaseShape(sf::Vector2f position, sf::Vector2f size, bool isClosed)
    : m_position(position), m_size(size), m_isSelected(false), m_isClosed(isClosed),
    m_groupId(""), m_anchorOffset(0, 0), m_rotation(0.0f) {}

void BaseShape::setPosition(sf::Vector2f position) {
    m_position = position;
    updateGeometry(); // Если фигура сдвинулась, просим её пересчитать вершины
}

sf::Vector2f BaseShape::getPosition() const {
    return m_position;
}

void BaseShape::setSelected(bool selected) {
    m_isSelected = selected;
}

bool BaseShape::isSelected() const {
    return m_isSelected;
}

void BaseShape::setGroupId(const std::string& id) {
    m_groupId = id;
}

std::string BaseShape::getGroupId() const {
    return m_groupId;
}
void BaseShape::setSize(sf::Vector2f size) {
    // Не даем фигуре "вывернуться наизнанку" (сжаться меньше 5 пикселей)
    m_size.x = std::max(5.0f, size.x);
    m_size.y = std::max(5.0f, size.y);
    updateGeometry(); // Перестраиваем вершины
}

sf::Vector2f BaseShape::getSize() const {
    return m_size;
}

void BaseShape::drawSelection(sf::RenderTarget& target) const {
    if (!m_isSelected) return;

    // --- ИСПРАВЛЕНИЕ: Используем реальные динамические границы ---
    sf::FloatRect bounds = getBounds();

    sf::RectangleShape bbox(bounds.size);
    bbox.setPosition(bounds.position);
    bbox.setFillColor(sf::Color::Transparent);
    bbox.setOutlineColor(sf::Color::Cyan);
    bbox.setOutlineThickness(1.0f);
    target.draw(bbox);

    float hw = 4.0f;
    sf::RectangleShape handle(sf::Vector2f(hw * 2, hw * 2));
    handle.setFillColor(sf::Color::White);
    handle.setOutlineColor(sf::Color::Black);
    handle.setOutlineThickness(1.0f);

    // Маркеры строго по краям реальной рамки
    handle.setPosition({ bounds.position.x - hw, bounds.position.y - hw }); target.draw(handle);
    handle.setPosition({ bounds.position.x + bounds.size.x - hw, bounds.position.y - hw }); target.draw(handle);
    handle.setPosition({ bounds.position.x + bounds.size.x - hw, bounds.position.y + bounds.size.y - hw }); target.draw(handle);
    handle.setPosition({ bounds.position.x - hw, bounds.position.y + bounds.size.y - hw }); target.draw(handle);

    // Отрисовка якоря
    sf::CircleShape anchor(6.0f);
    anchor.setOrigin({ 6.0f, 6.0f });
    anchor.setPosition(m_position + m_anchorOffset);
    anchor.setFillColor(sf::Color::Cyan);
    anchor.setOutlineColor(sf::Color::White);
    anchor.setOutlineThickness(2.0f);
    target.draw(anchor);
}

int BaseShape::getHitHandle(sf::Vector2f mousePos) const {
    if (!m_isSelected) return 0;

    float hw = 6.0f;
    auto checkHit = [&](float x, float y) {
        return mousePos.x >= x - hw && mousePos.x <= x + hw && mousePos.y >= y - hw && mousePos.y <= y + hw;
        };

    // Сначала проверяем якорь (Мировая позиция = m_position + m_anchorOffset)
    if (checkHit(m_position.x + m_anchorOffset.x, m_position.y + m_anchorOffset.y)) return 5;

    // Проверяем углы РЕАЛЬНОЙ рамки
    sf::FloatRect bounds = getBounds();
    if (checkHit(bounds.position.x, bounds.position.y)) return 1; // TL
    if (checkHit(bounds.position.x + bounds.size.x, bounds.position.y)) return 2; // TR
    if (checkHit(bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y)) return 3; // BR
    if (checkHit(bounds.position.x, bounds.position.y + bounds.size.y)) return 4; // BL

    return 0;
}
// Реализация новых методов
sf::FloatRect BaseShape::getBounds() const {
    // В SFML 3 передаем два вектора: позицию и размер
    return sf::FloatRect({ m_position.x - m_size.x, m_position.y - m_size.y },
        { m_size.x * 2.0f, m_size.y * 2.0f });
}
void BaseShape::setAnchorOffset(sf::Vector2f offset) { m_anchorOffset = offset; }
sf::Vector2f BaseShape::getAnchorOffset() const { return m_anchorOffset; }
void BaseShape::setRotation(float angle) { m_rotation = angle; updateGeometry(); }
float BaseShape::getRotation() const { return m_rotation; }
void BaseShape::setGlobalColor(sf::Color color) {
    for (auto& c : m_sideColors) c = color;
    updateGeometry();
}

void BaseShape::setGlobalThickness(float thickness) {
    for (auto& t : m_sideThicknesses) t = thickness;
    updateGeometry();
}
void BaseShape::setGlobalFillColor(sf::Color color) {
    m_fillColor = color;
    updateGeometry();
}
void BaseShape::save(std::ostream& out) const {
    // Чтобы строка с пробелами не сломала чтение, заменяем пустую группу на "NONE"
    std::string safeGroupId = m_groupId.empty() ? "NONE" : m_groupId;

    out << m_position.x << " " << m_position.y << " "
        << m_size.x << " " << m_size.y << " "
        << m_isClosed << " "
        << safeGroupId << " "
        << m_anchorOffset.x << " " << m_anchorOffset.y << " "
        << m_rotation << " "
        << (int)m_fillColor.r << " " << (int)m_fillColor.g << " "
        << (int)m_fillColor.b << " " << (int)m_fillColor.a << "\n";

    out << m_sideColors.size() << " ";
    for (const auto& c : m_sideColors) {
        out << (int)c.r << " " << (int)c.g << " " << (int)c.b << " " << (int)c.a << " ";
    }
    out << "\n" << m_sideThicknesses.size() << " ";
    for (float t : m_sideThicknesses) out << t << " ";
    out << "\n";
}

void BaseShape::load(std::istream& in) {
    std::string safeGroupId;
    in >> m_position.x >> m_position.y >> m_size.x >> m_size.y >> m_isClosed >> safeGroupId;
    m_groupId = (safeGroupId == "NONE") ? "" : safeGroupId;

    in >> m_anchorOffset.x >> m_anchorOffset.y >> m_rotation;

    int r, g, b, a;
    in >> r >> g >> b >> a; m_fillColor = sf::Color(r, g, b, a);

    size_t colCount; in >> colCount; m_sideColors.resize(colCount);
    for (size_t i = 0; i < colCount; i++) {
        in >> r >> g >> b >> a; m_sideColors[i] = sf::Color(r, g, b, a);
    }

    size_t thickCount; in >> thickCount; m_sideThicknesses.resize(thickCount);
    for (size_t i = 0; i < thickCount; i++) {
        in >> m_sideThicknesses[i];
    }
}