#include "BaseShape.h"
#include <iostream>

BaseShape::BaseShape(sf::Vector2f position, sf::Vector2f size, bool isClosed)
    : m_position(position), m_size(size), m_isSelected(false), m_isClosed(isClosed),
    m_groupId(0), m_anchorOffset(0, 0), m_rotation(0.0f), m_id(0), m_name("") {} // <-- ИЗМЕНЕНО

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

// 2. Измени геттер/сеттер группы:
void BaseShape::setGroupId(int id) { m_groupId = id; }
int BaseShape::getGroupId() const { return m_groupId; }

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
    if (checkHit(anchorWorld.x, anchorWorld.y)) return 5;

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
    // Заменяем пробелы в имени на _, чтобы не сломать чтение из файла
    std::string safeName = m_name.empty() ? "Unnamed" : m_name;
    std::replace(safeName.begin(), safeName.end(), ' ', '_');

    out << "ID: " << m_id << "\n"
        << "Name: " << safeName << "\n"
        << "Position: " << m_position.x << " " << m_position.y << "\n"
        << "Size: " << m_size.x << " " << m_size.y << "\n"
        << "IsClosed: " << m_isClosed << "\n"
        << "GroupId: " << m_groupId << "\n"
        << "AnchorOffset: " << m_anchorOffset.x << " " << m_anchorOffset.y << "\n"
        << "Rotation: " << m_rotation << "\n"
        << "FillColor: " << (int)m_fillColor.r << " " << (int)m_fillColor.g << " " << (int)m_fillColor.b << " " << (int)m_fillColor.a << "\n";

    out << "SideColorsCount: " << m_sideColors.size() << "\n";
    for (const auto& c : m_sideColors) {
        out << "  " << (int)c.r << " " << (int)c.g << " " << (int)c.b << " " << (int)c.a << "\n";
    }

    out << "SideThicknessesCount: " << m_sideThicknesses.size() << "\n  ";
    for (float t : m_sideThicknesses) out << t << " ";
    out << "\n";
}
void BaseShape::load(std::istream& in) {
    std::string dummy;
    in >> dummy >> m_id;
    in >> dummy >> m_name;
    std::replace(m_name.begin(), m_name.end(), '_', ' '); // Возвращаем пробелы
    in >> dummy >> m_position.x >> m_position.y;
    in >> dummy >> m_size.x >> m_size.y;
    in >> dummy >> m_isClosed;
    in >> dummy >> m_groupId; // Читаем как int

    in >> dummy >> m_anchorOffset.x >> m_anchorOffset.y;
    in >> dummy >> m_rotation;

    int r, g, b, a;
    in >> dummy >> r >> g >> b >> a;
    m_fillColor = sf::Color(r, g, b, a);

    size_t colCount;
    in >> dummy >> colCount;
    m_sideColors.resize(colCount);
    for (size_t i = 0; i < colCount; i++) {
        in >> r >> g >> b >> a;
        m_sideColors[i] = sf::Color(r, g, b, a);
    }

    size_t thickCount;
    in >> dummy >> thickCount;
    m_sideThicknesses.resize(thickCount);
    for (size_t i = 0; i < thickCount; i++) {
        in >> m_sideThicknesses[i];
    }
}
void BaseShape::setAnchorPositionWorld(sf::Vector2f newWorldAnchor) {
    sf::Vector2f oldWorldAnchor = m_position + m_anchorOffset;

    // Если якорь не изменился, ничего не делаем
    if (std::abs(newWorldAnchor.x - oldWorldAnchor.x) < 0.0001f &&
        std::abs(newWorldAnchor.y - oldWorldAnchor.y) < 0.0001f) return;

    // 1. Узнаем, где сейчас физически на экране находится "базовая точка" m_position
    // (она повернута относительно СТАРОГО якоря)
    sf::Vector2f worldM = rotatePoint(m_position, oldWorldAnchor, m_rotation);

    // 2. Вычисляем новое значение m_position напрямую.
    // Нам нужно такое m_position, которое при повороте вокруг НОВОГО якоря 
    // даст ту же самую мировую точку worldM.
    // Используем обратный поворот (-m_rotation)
    m_position = rotatePoint(worldM, newWorldAnchor, -m_rotation);

    // 3. Сразу же обновляем смещение якоря относительно новой позиции m_position
    m_anchorOffset = newWorldAnchor - m_position;

    // 4. И только теперь, когда ОБА параметра (m_position и m_anchorOffset) обновлены,
    // пересчитываем геометрию вершин.
    updateGeometry();
}

// --- НОВОЕ: Вращение мышью за маркер ---
void BaseShape::setRotationFromMouse(sf::Vector2f mousePos) {
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // Поскольку зеленый маркер теперь всегда ровно сверху от якоря (при 0 градусов),
    // наш базовый вектор направлен строго вверх (по оси -Y)
    sf::Vector2f baseVec(0.0f, -1.0f);

    sf::Vector2f mouseVec = mousePos - anchorWorld;

    // Защита от деления на ноль, если мышка прямо на якоре
    if (std::abs(mouseVec.x) < 0.001f && std::abs(mouseVec.y) < 0.001f) return;

    // Считаем углы через арктангенс
    float baseAngle = std::atan2(baseVec.y, baseVec.x);
    float mouseAngle = std::atan2(mouseVec.y, mouseVec.x);

    // Получаем разницу и переводим в градусы
    float rotRad = mouseAngle - baseAngle;
    float rotDeg = rotRad * 180.0f / 3.14159265f;

    // Нормализация в диапазон [-180, 180], чтобы углы не накручивались до бесконечности
    while (rotDeg <= -180.0f) rotDeg += 360.0f;
    while (rotDeg > 180.0f) rotDeg -= 360.0f;

    setRotation(rotDeg);
}
// Добавь этот метод в любое место внутри BaseShape.cpp

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

    // ==========================================================
    // --- ТВОЯ ИДЕЯ: МАСШТАБИРОВАНИЕ БЕЗ УЧЕТА УГЛА ПОВОРОТА ---
    // Мы "делаем вид, что угол равен 0" и просто применяем 
    // коэффициенты масштабирования глобальной рамки напрямую 
    // к локальному размеру фигуры. Это убирает любые искажения!
    // ==========================================================
    m_size.x *= scaleX;
    m_size.y *= scaleY;

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

    // ========================================================
    // --- ТВОЯ ИДЕЯ: МАСШТАБИРУЕМ БЕЗ УЧЕТА УГЛА ПОВОРОТА ---
    // ========================================================
    m_size.x *= scaleX;
    m_size.y *= scaleY;

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
void BaseShape::applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) {
    sf::FloatRect oldBounds = getBounds();
    sf::Vector2f oldAnchorWorld = m_position + m_anchorOffset;

    // 1. Узнаем, какой именно угол рамки мы зафиксировали 
    // (relX и relY будут равны 0.0 для левого/верхнего края и 1.0 для правого/нижнего)
    float relX = (fixedCorner.x > oldBounds.position.x + oldBounds.size.x / 2.0f) ? 1.0f : 0.0f;
    float relY = (fixedCorner.y > oldBounds.position.y + oldBounds.size.y / 2.0f) ? 1.0f : 0.0f;

    // 2. Применяем масштабирование к размерам фигуры 
    // (Это изменит m_size и обновит геометрию)
    resizeFromBoundingBox(oldBounds.size.x * Sx, oldBounds.size.y * Sy);

    // 3. Вычисляем новые границы фигуры после изменения размера
    sf::FloatRect newBounds = getBounds();

    // 4. Находим, куда "уехал" наш зафиксированный угол в новых координатах
    sf::Vector2f newCorner(
        newBounds.position.x + newBounds.size.x * relX,
        newBounds.position.y + newBounds.size.y * relY
    );

    // 5. ИДЕАЛЬНАЯ КОРРЕКЦИЯ: Жестко сдвигаем всю фигуру так, 
    // чтобы "уехавший" угол вернулся ровно в fixedCorner
    sf::Vector2f shift = fixedCorner - newCorner;
    m_position += shift;

    // 6. Пропорционально смещаем оранжевый якорь фигуры
    sf::Vector2f targetAnchor(
        fixedCorner.x + (oldAnchorWorld.x - fixedCorner.x) * Sx,
        fixedCorner.y + (oldAnchorWorld.y - fixedCorner.y) * Sy
    );
    m_anchorOffset = targetAnchor - m_position;

    updateGeometry();
}

// Правильная реализация статического метода в .cpp файле
sf::Vector2f BaseShape::rotatePoint(sf::Vector2f point, sf::Vector2f anchor, float angleDeg) {
    float rad = angleDeg * 3.14159265f / 180.0f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    sf::Vector2f d = point - anchor;
    return {
        anchor.x + d.x * cosA - d.y * sinA,
        anchor.y + d.x * sinA + d.y * cosA
    };
}