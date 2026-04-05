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
    if (!selected) {
        m_selectedSide = -1;
        m_selectedVertex = -1;
    }
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

    handle.setPosition({ bounds.position.x - hw, bounds.position.y - hw }); target.draw(handle);
    handle.setPosition({ bounds.position.x + bounds.size.x - hw, bounds.position.y - hw }); target.draw(handle);
    handle.setPosition({ bounds.position.x + bounds.size.x - hw, bounds.position.y + bounds.size.y - hw }); target.draw(handle);
    handle.setPosition({ bounds.position.x - hw, bounds.position.y + bounds.size.y - hw }); target.draw(handle);

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // --- НОВОЕ: Отрисовка маркера вращения (фиксированное расстояние от якоря) ---
    float fixedDist = 50.0f; // Всегда 50 пикселей от якоря
    float rad = m_rotation * 3.14159265f / 180.0f;

    // Расчет позиции маркера относительно якоря
    sf::Vector2f rotHandleWorld(
        anchorWorld.x + fixedDist * std::sin(rad),
        anchorWorld.y - fixedDist * std::cos(rad)
    );

    // Линия от якоря к маркеру вращения
    sf::Vertex line[] = {
        sf::Vertex(anchorWorld, sf::Color(0, 255, 0, 150)), // Теперь от anchorWorld
        sf::Vertex(rotHandleWorld, sf::Color(0, 255, 0, 150))
    };
    target.draw(line, 2, sf::PrimitiveType::Lines);

    // Сам зеленый маркер (ручка)
    sf::CircleShape rotCircle(5.0f);
    rotCircle.setOrigin({ 5.0f, 5.0f });
    rotCircle.setPosition(rotHandleWorld);
    rotCircle.setFillColor(sf::Color::Green);
    rotCircle.setOutlineColor(sf::Color::White);
    rotCircle.setOutlineThickness(1.0f);
    target.draw(rotCircle);

    // Отрисовка якоря
    sf::CircleShape anchor(6.0f);
    anchor.setOrigin({ 6.0f, 6.0f });
    anchor.setPosition(anchorWorld);
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

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // --- Проверяем попадание в ручку вращения (код 7) ---
    float fixedDist = 50.0f; // Фиксированное расстояние 50 пикселей от якоря
    float rad = m_rotation * 3.14159265f / 180.0f;

    // Расчет позиции маркера для проверки клика
    sf::Vector2f rotHandleWorld(
        anchorWorld.x + fixedDist * std::sin(rad),
        anchorWorld.y - fixedDist * std::cos(rad)
    );
    if (checkHit(rotHandleWorld.x, rotHandleWorld.y)) return 7;


    // Углы
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

void BaseShape::setAnchorPositionWorld(sf::Vector2f newWorldAnchor) {
    if (m_rotation == 0.0f) {
        m_anchorOffset = newWorldAnchor - m_position;
    }
    else {
        sf::Vector2f oldWorldAnchor = m_position + m_anchorOffset;

        // 1. Узнаем, где визуально (в мире) сейчас находится m_position
        sf::Vector2f currentVisualPos = rotatePoint(m_position, oldWorldAnchor, m_rotation);

        // 2. Рассчитываем, где должна быть базовая точка, чтобы при вращении
        // вокруг НОВОГО якоря она оказалась на том же визуальном месте
        sf::Vector2f newPos = rotatePoint(currentVisualPos, newWorldAnchor, -m_rotation);

        m_position = newPos;
        m_anchorOffset = newWorldAnchor - m_position;
    }
    updateGeometry();
}

// --- НОВОЕ: Вращение мышью за маркер ---
void BaseShape::setRotationFromMouse(sf::Vector2f mousePos) {
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // Вектор маркера в базовом (неповернутом) состоянии - торчит строго вверх
    sf::Vector2f unrotRotHandle(m_position.x, m_position.y - m_size.y - 30.0f);
    sf::Vector2f baseVec = unrotRotHandle - anchorWorld;

    // Если якорь стоит прямо на ручке (редко, но защита от atan2(0,0))
    if (std::abs(baseVec.x) < 0.001f && std::abs(baseVec.y) < 0.001f) return;

    float baseAngle = std::atan2(baseVec.y, baseVec.x);
    float mouseAngle = std::atan2(mousePos.y - anchorWorld.y, mousePos.x - anchorWorld.x);

    float rotRad = mouseAngle - baseAngle;
    float rotDeg = rotRad * 180.0f / 3.14159265f;

    // Нормализация в диапазон [-180, 180]
    while (rotDeg <= -180.0f) rotDeg += 360.0f;
    while (rotDeg > 180.0f) rotDeg -= 360.0f;

    setRotation(rotDeg); // setRotation внутри вызывает updateGeometry
}

// Добавь этот метод в любое место внутри BaseShape.cpp

void BaseShape::scaleFromHandle(int handle, sf::Vector2f mousePos) {
    if (handle < 1 || handle > 4) return;

    sf::FloatRect oldBounds = getBounds();
    sf::Vector2f oldAnchorWorld = m_position + m_anchorOffset;

    // Защита от деления на ноль
    const float EPSILON = 0.01f;
    if (oldBounds.size.x < EPSILON || oldBounds.size.y < EPSILON) return;

    // 1. Запоминаем относительную позицию якоря в старой рамке
    float relAnchorX = (oldAnchorWorld.x - oldBounds.position.x) / oldBounds.size.x;
    float relAnchorY = (oldAnchorWorld.y - oldBounds.position.y) / oldBounds.size.y;

    float oldMinX = oldBounds.position.x;
    float oldMinY = oldBounds.position.y;
    float oldMaxX = oldBounds.position.x + oldBounds.size.x;
    float oldMaxY = oldBounds.position.y + oldBounds.size.y;

    // 2. Находим фиксированный (противоположный) угол рамки
    sf::Vector2f fixedCorner;
    if (handle == 1) fixedCorner = { oldMaxX, oldMaxY };
    else if (handle == 2) fixedCorner = { oldMinX, oldMaxY };
    else if (handle == 3) fixedCorner = { oldMinX, oldMinY };
    else if (handle == 4) fixedCorner = { oldMaxX, oldMinY };

    // 3. Считаем новые границы рамки
    float newMinX, newMaxX, newMinY, newMaxY;
    if (handle == 1) { newMinX = mousePos.x; newMaxX = fixedCorner.x; newMinY = mousePos.y; newMaxY = fixedCorner.y; }
    else if (handle == 2) { newMinX = fixedCorner.x; newMaxX = mousePos.x; newMinY = mousePos.y; newMaxY = fixedCorner.y; }
    else if (handle == 3) { newMinX = fixedCorner.x; newMaxX = mousePos.x; newMinY = fixedCorner.y; newMaxY = mousePos.y; }
    else { newMinX = mousePos.x; newMaxX = fixedCorner.x; newMinY = fixedCorner.y; newMaxY = mousePos.y; }

    // Защита от "выворачивания" (рамка не может быть меньше 5x5)
    if (newMinX > newMaxX - 5.f) { if (handle == 1 || handle == 4) newMinX = newMaxX - 5.f; else newMaxX = newMinX + 5.f; }
    if (newMinY > newMaxY - 5.f) { if (handle == 1 || handle == 2) newMinY = newMaxY - 5.f; else newMaxY = newMinY + 5.f; }

    float newWidth = std::max(5.0f, std::abs(mousePos.x - fixedCorner.x));
    float newHeight = std::max(5.0f, std::abs(mousePos.y - fixedCorner.y));

    // Ограничение максимального размера (защита от "бесконечности")
    newWidth = std::min(newWidth, 10000.0f);
    newHeight = std::min(newHeight, 10000.0f);

    // Безопасное вычисление коэффициентов масштаба
    float scaleX = newWidth / oldBounds.size.x;
    float scaleY = newHeight / oldBounds.size.y;

    // Проекция на локальные оси
    float rad = m_rotation * 3.14159265f / 180.0f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float cos2 = cosA * cosA;
    float sin2 = sinA * sinA;

    float localScaleX = scaleX * cos2 + scaleY * sin2;
    float localScaleY = scaleX * sin2 + scaleY * cos2;

    // Применяем масштаб с ограничением
    m_size.x *= localScaleX;
    m_size.y *= localScaleY;
    m_size.x = std::clamp(m_size.x, 5.0f, 5000.0f);
    m_size.y = std::clamp(m_size.y, 5.0f, 5000.0f);

    updateGeometry();

    // 6. Узнаем, куда уехал противоположный угол после изменения размера
    sf::FloatRect tempBounds = getBounds();
    sf::Vector2f tempFixedCorner;
    if (handle == 1) tempFixedCorner = { tempBounds.position.x + tempBounds.size.x, tempBounds.position.y + tempBounds.size.y };
    else if (handle == 2) tempFixedCorner = { tempBounds.position.x, tempBounds.position.y + tempBounds.size.y };
    else if (handle == 3) tempFixedCorner = { tempBounds.position.x, tempBounds.position.y };
    else if (handle == 4) tempFixedCorner = { tempBounds.position.x + tempBounds.size.x, tempBounds.position.y };

    // 7. Сдвигаем фигуру ровно настолько, чтобы фиксированный угол вернулся на место
    sf::Vector2f shift = fixedCorner - tempFixedCorner;
    m_position += shift;
    updateGeometry();

    // 8. Обновляем якорь, чтобы он остался пропорционально в том же месте
    sf::FloatRect finalBounds = getBounds();
    sf::Vector2f newAnchorWorld(
        finalBounds.position.x + finalBounds.size.x * relAnchorX,
        finalBounds.position.y + finalBounds.size.y * relAnchorY
    );

    setAnchorPositionWorld(newAnchorWorld);
}
void BaseShape::resizeFromBoundingBox(float newWidth, float newHeight) {
    sf::FloatRect oldBounds = getBounds();
    const float EPSILON = 0.01f;

    // Если текущая рамка схлопнулась, масштабирование невозможно
    if (oldBounds.size.x < EPSILON || oldBounds.size.y < EPSILON) return;

    // Ограничиваем входные параметры
    newWidth = std::clamp(newWidth, 5.0f, 10000.0f);
    newHeight = std::clamp(newHeight, 5.0f, 10000.0f);

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    float relAnchorX = (anchorWorld.x - oldBounds.position.x) / oldBounds.size.x;
    float relAnchorY = (anchorWorld.y - oldBounds.position.y) / oldBounds.size.y;

    float scaleX = newWidth / oldBounds.size.x;
    float scaleY = newHeight / oldBounds.size.y;

    float rad = m_rotation * 3.14159265f / 180.0f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    float localScaleX = scaleX * cosA * cosA + scaleY * sinA * sinA;
    float localScaleY = scaleX * sinA * sinA + scaleY * cosA * cosA;

    m_size.x *= localScaleX;
    m_size.y *= localScaleY;

    // Финальная проверка на разумные пределы
    m_size.x = std::clamp(m_size.x, 2.0f, 5000.0f);
    m_size.y = std::clamp(m_size.y, 2.0f, 5000.0f);

    updateGeometry();

    // 4. Узнаем новые границы (рамка могла "уехать" от якоря)
    sf::FloatRect tempBounds = getBounds();

    // Находим, где СЕЙЧАС оказалась та самая якорная точка внутри новой рамки
    sf::Vector2f tempAnchor(
        tempBounds.position.x + tempBounds.size.x * relAnchorX,
        tempBounds.position.y + tempBounds.size.y * relAnchorY
    );

    // 5. Сдвигаем всю фигуру, чтобы якорь визуально остался на своем старом месте
    sf::Vector2f shift = anchorWorld - tempAnchor;
    m_position += shift;
    updateGeometry();

    // 6. Восстанавливаем математическую точность якоря
    m_anchorOffset = anchorWorld - m_position;
}