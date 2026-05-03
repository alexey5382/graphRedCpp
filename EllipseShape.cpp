#include "EllipseShape.h"
#include <cmath>
#include "imgui.h"
#include <algorithm>

EllipseShape::EllipseShape(sf::Vector2f position, sf::Vector2f radius)
    : BaseShape(position, radius, true), m_pointCount(64)
{
    m_sideColors.push_back(sf::Color::Black);
    m_sideThicknesses.push_back(2.0f);
    m_fillColor = sf::Color(200, 200, 200, 100);

    m_fillGeometry.setPrimitiveType(sf::PrimitiveType::TriangleFan);
    m_strokeGeometry.setPrimitiveType(sf::PrimitiveType::Triangles);

    updateGeometry();
}

void EllipseShape::updateGeometry() {
    m_fillGeometry.resize(m_pointCount + 2);
    m_strokeGeometry.resize(m_pointCount * 6);

    float thickness = m_sideThicknesses.empty() ? 2.0f : m_sideThicknesses[0];
    sf::Color strokeColor = m_sideColors.empty() ? sf::Color::Black : m_sideColors[0];

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // ВАЖНО: Вычисляем реальный центр эллипса в мировых координатах для заливки
    // Это избавит от "тянучки", так как заливка будет идти от центра фигуры, а не от якоря
    sf::Vector2f centerWorld = BaseShape::rotatePoint(m_position, anchorWorld, m_rotation);

    std::vector<sf::Vector2f> pts(m_pointCount);
    const float PI = 3.14159265f;

    for (size_t i = 0; i < m_pointCount; ++i) {
        float angle = i * 2 * PI / m_pointCount;
        sf::Vector2f localP(std::cos(angle) * m_size.x, std::sin(angle) * m_size.y);
        sf::Vector2f unrot = m_position + localP;
        pts[i] = BaseShape::rotatePoint(unrot, anchorWorld, m_rotation);
    }

    float signedArea = 0.0f;
    for (size_t i = 0; i < m_pointCount; ++i) {
        size_t next = (i + 1) % m_pointCount;
        signedArea += (pts[i].x * pts[next].y - pts[next].x * pts[i].y);
    }
    float normalSign = (signedArea >= 0.0f) ? 1.0f : -1.0f;

    std::vector<sf::Vector2f> outer(m_pointCount);
    std::vector<sf::Vector2f> inner(m_pointCount);

    for (size_t i = 0; i < m_pointCount; ++i) {
        outer[i] = pts[i];
        size_t prev = (i + m_pointCount - 1) % m_pointCount;
        size_t next = (i + 1) % m_pointCount;

        sf::Vector2f tangent = pts[next] - pts[prev];
        float len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        if (len > 0.0001f) tangent /= len;

        sf::Vector2f normal(-tangent.y * normalSign, tangent.x * normalSign);
        inner[i] = pts[i] + normal * thickness;
    }

    // ЗАЛИВКА: Теперь от центра фигуры
    m_fillGeometry[0].position = centerWorld;
    m_fillGeometry[0].color = m_fillColor;
    for (size_t i = 0; i < m_pointCount; ++i) {
        m_fillGeometry[i + 1].position = inner[i];
        m_fillGeometry[i + 1].color = m_fillColor;
    }
    m_fillGeometry[m_pointCount + 1].position = inner[0];
    m_fillGeometry[m_pointCount + 1].color = m_fillColor;

    // ОБВОДКА
    for (size_t i = 0; i < m_pointCount; ++i) {
        size_t next = (i + 1) % m_pointCount;
        size_t idx = i * 6;
        m_strokeGeometry[idx].position = inner[i];        m_strokeGeometry[idx].color = strokeColor;
        m_strokeGeometry[idx + 1].position = outer[i];    m_strokeGeometry[idx + 1].color = strokeColor;
        m_strokeGeometry[idx + 2].position = outer[next]; m_strokeGeometry[idx + 2].color = strokeColor;

        m_strokeGeometry[idx + 3].position = inner[i];    m_strokeGeometry[idx + 3].color = strokeColor;
        m_strokeGeometry[idx + 4].position = outer[next]; m_strokeGeometry[idx + 4].color = strokeColor;
        m_strokeGeometry[idx + 5].position = inner[next]; m_strokeGeometry[idx + 5].color = strokeColor;
    }
}

std::pair<sf::Vector2f, sf::Vector2f> EllipseShape::getWorldFoci() const {
    float rx = std::max(0.001f, m_size.x);
    float ry = std::max(0.001f, m_size.y);
    float c = std::sqrt(std::abs(rx * rx - ry * ry));

    sf::Vector2f f1_local, f2_local;
    if (rx >= ry) {
        f1_local = { -c, 0.f };
        f2_local = { c, 0.f };
    }
    else {
        f1_local = { 0.f, -c };
        f2_local = { 0.f, c };
    }

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    sf::Vector2f f1_world = BaseShape::rotatePoint(m_position + f1_local, anchorWorld, m_rotation);
    sf::Vector2f f2_world = BaseShape::rotatePoint(m_position + f2_local, anchorWorld, m_rotation);

    return { f1_world, f2_world };
}

void EllipseShape::setFoci(sf::Vector2f f1_world, sf::Vector2f f2_world) {
    sf::Vector2f center_world = (f1_world + f2_world) / 2.0f;
    float dx = f2_world.x - f1_world.x;
    float dy = f2_world.y - f1_world.y;
    float c = std::sqrt(dx * dx + dy * dy) / 2.0f;
    float angleRad = std::atan2(dy, dx);

    // 1. Сохраняем текущую позицию якоря в мире
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // При перемещении фокусов сохраняем малую полуось, пересчитываем большую
    if (c > 0.5f) { // Обновляем угол только если фокусы не слиплись
        float angleRad = std::atan2(dy, dx);
        if (m_size.x >= m_size.y) {
            m_size.x = std::max(2.0f, std::sqrt(c * c + m_size.y * m_size.y));
            m_rotation = angleRad * 180.f / 3.14159265f;
        }
        else {
            m_size.y = std::max(2.0f, std::sqrt(c * c + m_size.x * m_size.x));
            m_rotation = angleRad * 180.f / 3.14159265f - 90.f;
        }
    }
    else {
        // Если фокусы почти в центре, сохраняем текущий угол и просто меняем радиус
        if (m_size.x >= m_size.y) m_size.x = std::max(2.0f, std::sqrt(c * c + m_size.y * m_size.y));
        else m_size.y = std::max(2.0f, std::sqrt(c * c + m_size.x * m_size.x));
    }

    // 2. ВАЖНО: Вычисляем неповёрнутый центр так, чтобы при текущем вращении 
    // вокруг якоря, визуальный центр оказался в нужной точке center_world.
    m_position = BaseShape::rotatePoint(center_world, anchorWorld, -m_rotation);

    // 3. Корректируем смещение якоря, чтобы он остался на своем месте
    m_anchorOffset = anchorWorld - m_position;

    updateGeometry();
}

std::vector<sf::Vector2f> EllipseShape::getBasePoints() const {
    auto foci = getWorldFoci();
    return { foci.first, foci.second };
}

void EllipseShape::setBasePoints(const std::vector<sf::Vector2f>& pts) {
    // ПУСТО! Это ключевой фикс.
    // BaseShape::restoreState вызывает этот метод. Если здесь вызвать setFoci(), 
    // эллипс попытается пересчитать свой угол через atan2 от фокусов, что 
    // приведет к потере точности float и дикой тряске (jitter) каждый кадр.
    // Фигура идеально восстанавливается просто за счет m_position, m_size и m_rotation!
}

void EllipseShape::applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) {
    // 1. Узнаем, какой угол зафиксирован (0.0 - лево/верх, 1.0 - право/низ)
    sf::FloatRect oldBounds = getBounds();
    float relX = (fixedCorner.x > oldBounds.position.x + oldBounds.size.x / 2.0f) ? 1.0f : 0.0f;
    float relY = (fixedCorner.y > oldBounds.position.y + oldBounds.size.y / 2.0f) ? 1.0f : 0.0f;

    // Целевые размеры новой рамки (куда тянет курсор)
    float targetW = oldBounds.size.x * std::abs(Sx);
    float targetH = oldBounds.size.y * std::abs(Sy);

    // 2. АНАЛИТИЧЕСКИЙ РАСЧЕТ РАДИУСОВ
    // Решаем систему уравнений, чтобы новая рамка идеально совпала с targetW и targetH
    float rad = m_rotation * 3.14159265f / 180.f;
    float c = std::cos(rad);
    float s = std::sin(rad);
    float c2 = c * c;
    float s2 = s * s;
    float det = c2 - s2; // Определитель

    float A = (targetW / 2.0f) * (targetW / 2.0f);
    float B = (targetH / 2.0f) * (targetH / 2.0f);

    bool mathematicallySolved = false;

    // Если угол не близок к 45 градусам (где рамка обязана быть квадратной)
    if (std::abs(det) > 0.001f) {
        float X = (A * c2 - B * s2) / det; // Квадрат Радиуса X
        float Y = (B * c2 - A * s2) / det; // Квадрат Радиуса Y

        // Если пользователь не вытянул рамку в физически невозможную для данного угла пропорцию
        if (X > 1.0f && Y > 1.0f) {
            m_size.x = std::sqrt(X);
            m_size.y = std::sqrt(Y);
            mathematicallySolved = true;
        }
    }

    // Если точное решение невозможно (экстремальное растяжение), используем пропорциональный фоллбэк
    if (!mathematicallySolved) {
        m_size.x = std::max(2.0f, m_snapshot.size.x * std::abs(Sx));
        m_size.y = std::max(2.0f, m_snapshot.size.y * std::abs(Sy));
    }

    // Обработка отзеркаливания
    if (Sx * Sy < 0) {
        if (Sx < 0) m_rotation = 180.f - m_rotation;
        else m_rotation = -m_rotation;
    }

    // ВАЖНО: Обновляем геометрию ПЕРЕД замером новой рамки
    updateGeometry();

    // 3. ЖЕСТКАЯ ФИКСАЦИЯ ПРОТИВОПОЛОЖНОГО УГЛА
    sf::FloatRect newBounds = getBounds();
    sf::Vector2f currentFixedCorner(
        newBounds.position.x + newBounds.size.x * relX,
        newBounds.position.y + newBounds.size.y * relY
    );

    // Сдвигаем эллипс ровно на ту погрешность, на которую уехал фиксированный угол
    sf::Vector2f shift = fixedCorner - currentFixedCorner;
    m_position += shift;

    // 4. Пропорционально смещаем якорь
    sf::Vector2f oldAnchorWorld = m_snapshot.position + m_snapshot.anchorOffset;
    sf::Vector2f targetAnchor(
        fixedCorner.x + (oldAnchorWorld.x - fixedCorner.x) * Sx,
        fixedCorner.y + (oldAnchorWorld.y - fixedCorner.y) * Sy
    );
    m_anchorOffset = targetAnchor - m_position;

    updateGeometry();
}
sf::FloatRect EllipseShape::getBounds() const {
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // Переводим угол в радианы
    float rad = m_rotation * 3.14159265f / 180.f;
    float cosR = std::cos(rad);
    float sinR = std::sin(rad);

    // Точная аналитическая формула габаритов повернутого эллипса
    float halfW = std::sqrt(m_size.x * m_size.x * cosR * cosR + m_size.y * m_size.y * sinR * sinR);
    float halfH = std::sqrt(m_size.x * m_size.x * sinR * sinR + m_size.y * m_size.y * cosR * cosR);

    // Узнаем реальный центр в мире
    sf::Vector2f centerWorld = BaseShape::rotatePoint(m_position, anchorWorld, m_rotation);

    return sf::FloatRect(
        { centerWorld.x - halfW, centerWorld.y - halfH },
        { halfW * 2.0f, halfH * 2.0f }
    );
}
void EllipseShape::draw(sf::RenderTarget& target) const {
    if (m_isClosed) target.draw(m_fillGeometry);
    target.draw(m_strokeGeometry);

    // Отрисовка фокусов только если фигура выделена
    if (m_isSelected) {
        auto foci = getWorldFoci();
        sf::CircleShape handle(5.0f);
        handle.setOutlineColor(sf::Color::Black);
        handle.setOutlineThickness(1.0f);
        handle.setOrigin({ 5.0f, 5.0f });

        // Узнаем, выделен ли сейчас какой-то из фокусов
        int selectedV = getSelectedVertex();

        // Рисуем Фокус 1 (Красный, если выделен, иначе Желтый)
        handle.setPosition(foci.first);
        handle.setFillColor((selectedV == 0) ? sf::Color::Red : sf::Color::Yellow);
        target.draw(handle);

        // Рисуем Фокус 2
        handle.setPosition(foci.second);
        handle.setFillColor((selectedV == 1) ? sf::Color::Red : sf::Color::Yellow);
        target.draw(handle);
    }
}

bool EllipseShape::contains(sf::Vector2f point) const {
    sf::FloatRect b = getBounds();
    return b.contains(point);
}

void EllipseShape::setStrokeColor(sf::Color color) {
    if (!m_sideColors.empty()) { m_sideColors[0] = color; updateGeometry(); }
}

void EllipseShape::setStrokeThickness(float thickness) {
    if (!m_sideThicknesses.empty()) { m_sideThicknesses[0] = thickness; updateGeometry(); }
}

void EllipseShape::save(std::ostream& out) const {
    BaseShape::save(out);
}

void EllipseShape::load(std::istream& in) {
    BaseShape::load(in);
    updateGeometry();
}

void EllipseShape::showImGuiProperties() {

    ImGui::Text("Global Rotation:");
    float rot = m_rotation;
    ImGui::SetNextItemWidth(120);
    // ИСПРАВЛЕНИЕ: Используем setRotation, чтобы базовая логика правильно обрабатывала поворот
    if (ImGui::DragFloat("##cRotDrag", &rot, 1.0f, -180.0f, 180.0f, "%.1f deg")) {
        setRotation(rot);
    }
    ImGui::SameLine();
    // НОВАЯ КНОПКА ЗАПЕКАНИЯ
    if (ImGui::Button("Bake Rotation") && std::abs(m_rotation) > 0.01f) {
        // Сохраняем мировую позицию якоря перед трансформацией
        sf::Vector2f anchorWorld = m_position + m_anchorOffset;

        // Получаем текущие габариты повернутого эллипса
        sf::FloatRect bounds = getBounds();

        // Вписываем эллипс прямо в эти габариты и сбрасываем угол
        m_position = { bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f };
        m_size = { bounds.size.x / 2.0f, bounds.size.y / 2.0f };
        m_rotation = 0.0f;

        // Пересчитываем смещение якоря, чтобы он остался на месте
        m_anchorOffset = anchorWorld - m_position;
        updateGeometry();
    }
    ImGui::Separator();

    ImGui::Text("Ellipse Parameters:");

    float rx = m_size.x;
    float ry = m_size.y;
    bool sizeChanged = false;

    ImGui::SetNextItemWidth(150);
    if (ImGui::DragFloat("Radius X", &rx, 1.0f, 0.1f, 10000.f)) sizeChanged = true;
    ImGui::SetNextItemWidth(150);
    if (ImGui::DragFloat("Radius Y", &ry, 1.0f, 0.1f, 10000.f)) sizeChanged = true;
    ImGui::Separator();
    ImGui::Checkbox("Show Rotated Bounds (Local Scale)", &m_showOBB);
    if (sizeChanged) {
        m_size.x = std::max(0.1f, rx);
        m_size.y = std::max(0.1f, ry);
        updateGeometry();
    }

    auto foci = getWorldFoci();
    float f1[2] = { foci.first.x, foci.first.y };
    float f2[2] = { foci.second.x, foci.second.y };
    bool fociChanged = false;

    ImGui::SetNextItemWidth(200);
    if (ImGui::DragFloat2("Focus 1", f1, 1.0f)) fociChanged = true;
    ImGui::SetNextItemWidth(200);
    if (ImGui::DragFloat2("Focus 2", f2, 1.0f)) fociChanged = true;

    if (fociChanged) {
        setFoci({ f1[0], f1[1] }, { f2[0], f2[1] });
    }

    ImGui::Separator();
    ImGui::Text("Appearance");

    if (ImGui::Button("Randomize Colors & Thickness", ImVec2(-1, 0))) {
        m_fillColor = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 100 + std::rand() % 155);
        if (!m_sideColors.empty()) {
            m_sideColors[0] = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 255);
        }
        if (!m_sideThicknesses.empty()) {
            m_sideThicknesses[0] = 2.0f + static_cast<float>(std::rand() % 28);
        }
        updateGeometry();
    }

    if (m_isClosed) {
        float fill[4] = { m_fillColor.r / 255.f, m_fillColor.g / 255.f, m_fillColor.b / 255.f, m_fillColor.a / 255.f };
        if (ImGui::ColorEdit4("Fill Color", fill)) {
            m_fillColor = sf::Color(fill[0] * 255, fill[1] * 255, fill[2] * 255, fill[3] * 255);
            updateGeometry();
        }
    }

    if (!m_sideColors.empty() && !m_sideThicknesses.empty()) {
        float stroke[4] = { m_sideColors[0].r / 255.f, m_sideColors[0].g / 255.f, m_sideColors[0].b / 255.f, m_sideColors[0].a / 255.f };
        if (ImGui::ColorEdit4("Stroke Color", stroke)) {
            m_sideColors[0] = sf::Color(stroke[0] * 255, stroke[1] * 255, stroke[2] * 255, stroke[3] * 255);
            updateGeometry();
        }

        float thick = m_sideThicknesses[0];
        if (ImGui::SliderFloat("Stroke Thickness", &thick, 1.0f, 50.0f)) {
            m_sideThicknesses[0] = thick;
            updateGeometry();
        }
    }
    ImGui::Separator();

    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position:");
    float pos[2] = { worldAnchor.x, worldAnchor.y };
    if (ImGui::DragFloat2("##cAnchorDrag", pos, 1.0f)) {
        setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    }
}
void EllipseShape::moveVertex(int index, sf::Vector2f delta) {
    auto foci = getWorldFoci();

    // Твоя сцена передает index = 0 (если handle был 100) или index = 1 (если handle был 101)
    if (index == 0) {
        setFoci(foci.first + delta, foci.second);
    }
    else if (index == 1) {
        setFoci(foci.first, foci.second + delta);
    }
}
int EllipseShape::getHitFocus(sf::Vector2f mousePos) const {
    if (!m_isSelected) return -1; // Проверяем только если эллипс выделен

    auto foci = getWorldFoci();
    float hitRadius = 8.0f; // Радиус захвата мышкой (чуть больше самого кружка для удобства)

    // Функция расстояния
    auto dist = [](sf::Vector2f a, sf::Vector2f b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        };

    if (dist(mousePos, foci.first) <= hitRadius) return 0; // Первый фокус
    if (dist(mousePos, foci.second) <= hitRadius) return 1; // Второй фокус

    return -1; // Никуда не попали
}

void EllipseShape::moveFocus(int index, sf::Vector2f newWorldPos) {
    auto foci = getWorldFoci();

    // Если тянем первый фокус (0), второй остается на месте, и наоборот
    if (index == 0) {
        setFoci(newWorldPos, foci.second);
    }
    else if (index == 1) {
        setFoci(foci.first, newWorldPos);
    }
}
void EllipseShape::drawSelection(sf::RenderTarget& target) const {
    if (!m_isSelected) return;

    // ==========================================================
    // 1. ПРЯМАЯ РАМКА (AABB) И БАЗОВЫЕ МАРКЕРЫ (БЕЗ КВАДРАТОВ УГЛОВ)
    // ==========================================================
    sf::FloatRect bounds = getBounds();
    sf::RectangleShape bbox(bounds.size);
    bbox.setPosition(bounds.position);
    bbox.setFillColor(sf::Color::Transparent);
    bbox.setOutlineColor(sf::Color::Cyan);
    bbox.setOutlineThickness(1.0f);
    target.draw(bbox);

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // Отрисовка маркера вращения (фиксированное расстояние от якоря)
    float fixedDist = 50.0f;
    float rad = m_rotation * 3.14159265f / 180.0f;

    sf::Vector2f rotHandleWorld(
        anchorWorld.x + fixedDist * std::sin(rad),
        anchorWorld.y - fixedDist * std::cos(rad)
    );

    // Линия от якоря к маркеру вращения
    sf::Vertex line[] = {
        sf::Vertex(anchorWorld, sf::Color(0, 255, 0, 150)),
        sf::Vertex(rotHandleWorld, sf::Color(0, 255, 0, 150))
    };
    target.draw(line, 2, sf::PrimitiveType::Lines);

    // Зеленый маркер вращения
    sf::CircleShape rotCircle(5.0f);
    rotCircle.setOrigin({ 5.0f, 5.0f });
    rotCircle.setPosition(rotHandleWorld);
    rotCircle.setFillColor(sf::Color::Green);
    rotCircle.setOutlineColor(sf::Color::White);
    rotCircle.setOutlineThickness(1.0f);
    target.draw(rotCircle);

    // Голубой маркер якоря
    sf::CircleShape anchor(6.0f);
    anchor.setOrigin({ 6.0f, 6.0f });
    anchor.setPosition(anchorWorld);
    anchor.setFillColor(sf::Color::Cyan);
    anchor.setOutlineColor(sf::Color::White);
    anchor.setOutlineThickness(2.0f);
    target.draw(anchor);

    // ==========================================================
    // 2. ПОВЕРНУТАЯ РАМКА ДЛЯ МАСШТАБИРОВАНИЯ (OBB)
    // ==========================================================
    if (m_showOBB) {
        sf::Vector2f centerWorld = BaseShape::rotatePoint(m_position, anchorWorld, m_rotation);

        sf::RectangleShape obb(sf::Vector2f(m_size.x * 2.0f, m_size.y * 2.0f));
        obb.setOrigin({ m_size.x, m_size.y });
        obb.setPosition(centerWorld);
        obb.setRotation(sf::degrees(m_rotation));
        obb.setFillColor(sf::Color::Transparent);
        obb.setOutlineColor(sf::Color::Magenta);
        obb.setOutlineThickness(1.5f);
        target.draw(obb);

        // Отрисовка 4-х маркеров масштабирования
        float hw = 5.0f;
        sf::RectangleShape handle(sf::Vector2f(hw * 2, hw * 2));
        handle.setFillColor(sf::Color::White);
        handle.setOutlineColor(sf::Color::Magenta);
        handle.setOutlineThickness(1.0f);
        handle.setOrigin({ hw, hw });

        handle.setPosition(getOBBCorner(11)); target.draw(handle); // TL
        handle.setPosition(getOBBCorner(12)); target.draw(handle); // TR
        handle.setPosition(getOBBCorner(13)); target.draw(handle); // BR
        handle.setPosition(getOBBCorner(14)); target.draw(handle); // BL
    }

    // ==========================================================
    // 3. ФОКУСЫ
    // ==========================================================
    auto foci = getWorldFoci();
    sf::CircleShape fHandle(5.0f);
    fHandle.setOutlineColor(sf::Color::Black);
    fHandle.setOutlineThickness(1.0f);
    fHandle.setOrigin({ 5.0f, 5.0f });

    int selectedV = getSelectedVertex();
    fHandle.setPosition(foci.first);
    fHandle.setFillColor((selectedV == 0) ? sf::Color::Red : sf::Color::Yellow);
    target.draw(fHandle);

    fHandle.setPosition(foci.second);
    fHandle.setFillColor((selectedV == 1) ? sf::Color::Red : sf::Color::Yellow);
    target.draw(fHandle);
}

int EllipseShape::getHitHandle(sf::Vector2f mousePos) const {
    if (!m_isSelected) return 0;

    // 1. Проверяем 4 угла повернутой рамки (Индексы 11-14)
    if (m_showOBB) {
        float hw = 7.0f;
        auto checkHit = [&](sf::Vector2f p) {
            return mousePos.x >= p.x - hw && mousePos.x <= p.x + hw && mousePos.y >= p.y - hw && mousePos.y <= p.y + hw;
            };
        if (checkHit(getOBBCorner(11))) return 11;
        if (checkHit(getOBBCorner(12))) return 12;
        if (checkHit(getOBBCorner(13))) return 13;
        if (checkHit(getOBBCorner(14))) return 14;
    }

    // 2. Проверяем фокусы (Индексы 100-101)
    auto foci = getWorldFoci();
    float hitRadius = 8.0f;
    auto dist = [](sf::Vector2f a, sf::Vector2f b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        };
    if (dist(mousePos, foci.first) <= hitRadius) return 100;
    if (dist(mousePos, foci.second) <= hitRadius) return 101;

    // 3. Запрашиваем клики у базового класса
    int baseHit = BaseShape::getHitHandle(mousePos);

    // ПОЛНОСТЬЮ ОТКЛЮЧАЕМ масштабирование эллипса за синюю рамку!
    // Индексы 1, 2, 3 и 4 соответствуют углам глобальной AABB рамки.
    if (baseHit >= 1 && baseHit <= 4) {
        return 0;
    }

    // Возвращаем попадание только если это якорь (5) или вращение (7)
    return baseHit;
}
sf::Vector2f EllipseShape::getOBBCorner(int handle) const {
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    sf::Vector2f centerWorld = BaseShape::rotatePoint(m_position, anchorWorld, m_rotation);

    if (handle == 11) return BaseShape::rotatePoint(centerWorld + sf::Vector2f(-m_size.x, -m_size.y), centerWorld, m_rotation); // TL
    if (handle == 12) return BaseShape::rotatePoint(centerWorld + sf::Vector2f(m_size.x, -m_size.y), centerWorld, m_rotation);  // TR
    if (handle == 13) return BaseShape::rotatePoint(centerWorld + sf::Vector2f(m_size.x, m_size.y), centerWorld, m_rotation);   // BR
    if (handle == 14) return BaseShape::rotatePoint(centerWorld + sf::Vector2f(-m_size.x, m_size.y), centerWorld, m_rotation);  // BL
    return centerWorld;
}

// Идеальное масштабирование по ЛОКАЛЬНЫМ осям (рамка никогда не уплывет!)
void EllipseShape::resizeFromOBB(int handle, sf::Vector2f mouseWorldPos) {
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    sf::Vector2f centerWorld = BaseShape::rotatePoint(m_position, anchorWorld, m_rotation);

    // Локальные координаты мыши (относительно центра, без поворота)
    sf::Vector2f mouseLocal = BaseShape::rotatePoint(mouseWorldPos, centerWorld, -m_rotation);

    // Ищем локальные координаты фиксированного (противоположного) угла
    sf::Vector2f fixedLocal;
    if (handle == 11) fixedLocal = centerWorld + sf::Vector2f(m_size.x, m_size.y);  // Тянем TL, фиксируем BR
    else if (handle == 12) fixedLocal = centerWorld + sf::Vector2f(-m_size.x, m_size.y); // Тянем TR, фиксируем BL
    else if (handle == 13) fixedLocal = centerWorld + sf::Vector2f(-m_size.x, -m_size.y); // Тянем BR, фиксируем TL
    else if (handle == 14) fixedLocal = centerWorld + sf::Vector2f(m_size.x, -m_size.y); // Тянем BL, фиксируем TR

    // Центр новой рамки в локальных координатах
    sf::Vector2f newCenterLocal = (fixedLocal + mouseLocal) / 2.0f;

    // Новые радиусы (половина ширины и высоты локальной рамки)
    float newRx = std::max(2.0f, std::abs(mouseLocal.x - fixedLocal.x) / 2.0f);
    float newRy = std::max(2.0f, std::abs(mouseLocal.y - fixedLocal.y) / 2.0f);

    // Переводим новый центр обратно в мировые координаты
    sf::Vector2f newCenterWorld = BaseShape::rotatePoint(newCenterLocal, centerWorld, m_rotation);

    // Обновляем m_position и размеры
    m_position = BaseShape::rotatePoint(newCenterWorld, anchorWorld, -m_rotation);
    m_size.x = newRx;
    m_size.y = newRy;

    // Корректируем смещение якоря
    m_anchorOffset = anchorWorld - m_position;
    updateGeometry();
}