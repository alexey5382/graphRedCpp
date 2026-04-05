#include "CircleShape.h"
#include <cmath>
#include "imgui.h"
#include <algorithm>

CircleShape::CircleShape(sf::Vector2f position, sf::Vector2f size)
    : BaseShape(position, size, true), m_pointCount(64)
{
    m_sideColors.push_back(sf::Color::Black);
    m_sideThicknesses.push_back(2.0f);
    m_fillColor = sf::Color(200, 200, 200, 100);

    m_fillGeometry.setPrimitiveType(sf::PrimitiveType::TriangleFan);
    m_strokeGeometry.setPrimitiveType(sf::PrimitiveType::Triangles);

    // Инициализируем 64 точки идеального круга (от -1 до 1)
    const float PI = 3.14159265f;
    for (size_t i = 0; i < m_pointCount; ++i) {
        float angle = i * 2 * PI / m_pointCount;
        m_basePoints.push_back(sf::Vector2f(std::cos(angle), std::sin(angle)));
    }

    updateGeometry();
}

void CircleShape::updateGeometry() {
    if (m_basePoints.empty()) return;

    m_fillGeometry.resize(m_pointCount + 2);
    m_strokeGeometry.resize(m_pointCount * 6);

    float thickness = m_sideThicknesses.empty() ? 2.0f : m_sideThicknesses[0];
    sf::Color strokeColor = m_sideColors.empty() ? sf::Color::Black : m_sideColors[0];

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // Получаем мировые повернутые координаты (идеальная математическая граница)
    std::vector<sf::Vector2f> pts(m_pointCount);
    for (size_t i = 0; i < m_pointCount; ++i) {
        sf::Vector2f unrot(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        pts[i] = BaseShape::rotatePoint(unrot, anchorWorld, m_rotation);
    }

    // 1. Узнаем направление обхода точек (по или против часовой стрелки)
    // Это нужно, если фигура была вывернута наизнанку (отрицательный масштаб)
    float signedArea = 0.0f;
    for (size_t i = 0; i < m_pointCount; ++i) {
        size_t next = (i + 1) % m_pointCount;
        signedArea += (pts[i].x * pts[next].y - pts[next].x * pts[i].y);
    }
    float normalSign = (signedArea >= 0.0f) ? 1.0f : -1.0f;

    // 2. Вычисляем ИДЕАЛЬНУЮ обводку (используем перпендикуляры к кривой)
    std::vector<sf::Vector2f> outer(m_pointCount);
    std::vector<sf::Vector2f> inner(m_pointCount);

    for (size_t i = 0; i < m_pointCount; ++i) {
        outer[i] = pts[i]; // Внешний край прибит к границе рамки

        size_t prev = (i + m_pointCount - 1) % m_pointCount;
        size_t next = (i + 1) % m_pointCount;

        // Вычисляем КАСАТЕЛЬНУЮ к линии в этой точке (от предыдущей к следующей)
        sf::Vector2f tangent = pts[next] - pts[prev];
        float len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        if (len > 0.0001f) tangent /= len;

        // Поворачиваем касательную на 90 градусов, чтобы получить НОРМАЛЬ (перпендикуляр),
        // и учитываем направление обхода, чтобы нормаль всегда смотрела строго ВНУТРЬ
        sf::Vector2f normal(-tangent.y * normalSign, tangent.x * normalSign);

        // Сдвигаем точку внутрь ровно на заданную толщину
        inner[i] = pts[i] + normal * thickness;
    }

    // 3. ЗАЛИВКА (TriangleFan)
    // Заливаем по ВНУТРЕННЕМУ радиусу, чтобы полупрозрачные цвета заливки и обводки не наслаивались друг на друга
    m_fillGeometry[0].position = anchorWorld;
    m_fillGeometry[0].color = m_fillColor;
    for (size_t i = 0; i < m_pointCount; ++i) {
        m_fillGeometry[i + 1].position = inner[i];
        m_fillGeometry[i + 1].color = m_fillColor;
    }
    m_fillGeometry[m_pointCount + 1].position = inner[0]; // Замыкаем
    m_fillGeometry[m_pointCount + 1].color = m_fillColor;

    // 4. ОБВОДКА (Triangles)
    for (size_t i = 0; i < m_pointCount; ++i) {
        size_t next = (i + 1) % m_pointCount;

        size_t idx = i * 6;
        m_strokeGeometry[idx].position = inner[i];      m_strokeGeometry[idx].color = strokeColor;
        m_strokeGeometry[idx + 1].position = outer[i];  m_strokeGeometry[idx + 1].color = strokeColor;
        m_strokeGeometry[idx + 2].position = outer[next]; m_strokeGeometry[idx + 2].color = strokeColor;

        m_strokeGeometry[idx + 3].position = inner[i];  m_strokeGeometry[idx + 3].color = strokeColor;
        m_strokeGeometry[idx + 4].position = outer[next]; m_strokeGeometry[idx + 4].color = strokeColor;
        m_strokeGeometry[idx + 5].position = inner[next]; m_strokeGeometry[idx + 5].color = strokeColor;
    }
}void CircleShape::applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) {
    size_t n = m_basePoints.size();
    std::vector<sf::Vector2f> worldPts(n);
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    sf::Vector2f newAnchorWorld(
        fixedCorner.x + (anchorWorld.x - fixedCorner.x) * Sx,
        fixedCorner.y + (anchorWorld.y - fixedCorner.y) * Sy
    );

    float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;

    for (size_t i = 0; i < n; i++) {
        sf::Vector2f unrot(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f wp = BaseShape::rotatePoint(unrot, anchorWorld, m_rotation);

        wp.x = fixedCorner.x + (wp.x - fixedCorner.x) * Sx;
        wp.y = fixedCorner.y + (wp.y - fixedCorner.y) * Sy;

        wp = BaseShape::rotatePoint(wp, newAnchorWorld, -m_rotation);
        worldPts[i] = wp;

        if (wp.x < minX) minX = wp.x;
        if (wp.x > maxX) maxX = wp.x;
        if (wp.y < minY) minY = wp.y;
        if (wp.y > maxY) maxY = wp.y;
    }

    m_position = { minX, minY };
    m_size = { std::max(5.0f, maxX - minX), std::max(5.0f, maxY - minY) };

    for (size_t i = 0; i < n; i++) {
        m_basePoints[i].x = (worldPts[i].x - m_position.x) / m_size.x;
        m_basePoints[i].y = (worldPts[i].y - m_position.y) / m_size.y;
    }

    m_anchorOffset = newAnchorWorld - m_position;
    updateGeometry();
}

sf::FloatRect CircleShape::getBounds() const {
    if (m_pointCount == 0 || m_basePoints.empty()) return BaseShape::getBounds();

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;

    for (size_t i = 0; i < m_pointCount; ++i) {
        sf::Vector2f unrot(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f wp = BaseShape::rotatePoint(unrot, anchorWorld, m_rotation);

        if (wp.x < minX) minX = wp.x;
        if (wp.x > maxX) maxX = wp.x;
        if (wp.y < minY) minY = wp.y;
        if (wp.y > maxY) maxY = wp.y;
    }

    // --- ИСПРАВЛЕНИЕ: Больше не прибавляем толщину к рамке ---
    // Рамка теперь ИДЕАЛЬНО совпадает с физическими точками фигуры
    return sf::FloatRect({ minX, minY }, { std::max(0.1f, maxX - minX), std::max(0.1f, maxY - minY) });
}

void CircleShape::draw(sf::RenderTarget& target) const {
    if (m_isClosed) target.draw(m_fillGeometry);
    target.draw(m_strokeGeometry);
}

bool CircleShape::contains(sf::Vector2f point) const {
    sf::FloatRect b = getBounds();
    return b.contains(point); // Упрощенный клик по рамке (стабильно работает с искажениями)
}

void CircleShape::setStrokeColor(sf::Color color) {
    if (!m_sideColors.empty()) { m_sideColors[0] = color; updateGeometry(); }
}

void CircleShape::setStrokeThickness(float thickness) {
    if (!m_sideThicknesses.empty()) { m_sideThicknesses[0] = thickness; updateGeometry(); }
}

void CircleShape::save(std::ostream& out) const {
    BaseShape::save(out);
    out << "PointsCount: " << m_basePoints.size() << "\n";
    for (const auto& p : m_basePoints) {
        out << "  " << p.x << " " << p.y << "\n";
    }
}

void CircleShape::load(std::istream& in) {
    BaseShape::load(in);
    std::string dummy;
    size_t ptsCount;
    in >> dummy >> ptsCount;
    m_basePoints.resize(ptsCount);
    for (size_t i = 0; i < ptsCount; i++) {
        in >> m_basePoints[i].x >> m_basePoints[i].y;
    }
    updateGeometry();
}

void CircleShape::showImGuiProperties() {
    ImGui::Text("Global Rotation:");
    float rot = m_rotation;
    ImGui::SetNextItemWidth(120);
    bool rotChanged = ImGui::DragFloat("##cRotDrag", &rot, 1.0f, -180.0f, 180.0f, "%.1f deg");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    rotChanged |= ImGui::InputFloat("Angle", &rot, 0.0f, 0.0f, "%.1f");
    if (rotChanged) setRotation(rot);

    ImGui::SameLine();
    if (ImGui::Button("Bake Rotation") && std::abs(m_rotation) > 0.01f) {
        size_t n = m_basePoints.size();
        sf::Vector2f anchorWorld = m_position + m_anchorOffset;
        std::vector<sf::Vector2f> worldPts(n);

        float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
        for (size_t i = 0; i < n; i++) {
            sf::Vector2f unrotated(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
            worldPts[i] = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);

            if (worldPts[i].x < minX) minX = worldPts[i].x;
            if (worldPts[i].x > maxX) maxX = worldPts[i].x;
            if (worldPts[i].y < minY) minY = worldPts[i].y;
            if (worldPts[i].y > maxY) maxY = worldPts[i].y;
        }

        m_position = { minX, minY };
        m_size = { std::max(5.0f, maxX - minX), std::max(5.0f, maxY - minY) };

        for (size_t i = 0; i < n; i++) {
            m_basePoints[i].x = (worldPts[i].x - m_position.x) / m_size.x;
            m_basePoints[i].y = (worldPts[i].y - m_position.y) / m_size.y;
        }

        m_rotation = 0.0f;
        m_anchorOffset = anchorWorld - m_position;

        updateGeometry();
    }
    ImGui::Separator();

    // --- МАСШТАБ КРУГА СО СНИМКАМИ ---
    ImGui::Text("Bounding Box Scale (W/H):");
    sf::FloatRect bounds = getBounds();

    static bool isCircleUIScaling = false;
    static ShapeSnapshot uiCircleSnap;
    static sf::FloatRect uiCircleStartBounds;
    static float baseCircleW = 1.0f, baseCircleH = 1.0f;

    float scale[2] = { bounds.size.x, bounds.size.y };

    ImGui::SetNextItemWidth(150);
    bool changed1 = ImGui::SliderFloat2("##cScaleSl", scale, 10.0f, 1000.0f);
    bool act1 = ImGui::IsItemActivated(); bool deact1 = ImGui::IsItemDeactivated();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    bool changed2 = ImGui::InputFloat2("##cScaleInp", scale, "%.1f");
    bool act2 = ImGui::IsItemActivated(); bool deact2 = ImGui::IsItemDeactivated();

    ImGui::Text("Relative Scale (%%):");
    float diag = std::sqrt(bounds.size.x * bounds.size.x + bounds.size.y * bounds.size.y);
    float relScale = diag / 1.41421356f;

    ImGui::SetNextItemWidth(150);
    bool changed3 = ImGui::SliderFloat("##cRelSl", &relScale, 5.0f, 500.0f, "%.1f %%");
    bool act3 = ImGui::IsItemActivated(); bool deact3 = ImGui::IsItemDeactivated();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    bool changed4 = ImGui::InputFloat("##cRelInp", &relScale, 0.0f, 0.0f, "%.1f");
    bool act4 = ImGui::IsItemActivated(); bool deact4 = ImGui::IsItemDeactivated();

    bool anyAct = act1 || act2 || act3 || act4;
    bool anyDeact = deact1 || deact2 || deact3 || deact4;
    bool anyChanged = changed1 || changed2 || changed3 || changed4;

    if (anyAct) {
        isCircleUIScaling = true;
        uiCircleSnap = { m_position, m_size, m_anchorOffset, m_rotation, m_basePoints };
        uiCircleStartBounds = bounds;
        baseCircleW = bounds.size.x;
        baseCircleH = bounds.size.y;
    }

    if (anyChanged) {
        if (!isCircleUIScaling) {
            isCircleUIScaling = true;
            uiCircleSnap = { m_position, m_size, m_anchorOffset, m_rotation, m_basePoints };
            uiCircleStartBounds = bounds;
            baseCircleW = bounds.size.x;
            baseCircleH = bounds.size.y;
        }

        float targetW = scale[0];
        float targetH = scale[1];

        if (changed3 || changed4) {
            float targetDiag = relScale * 1.41421356f;
            float baseDiag = std::sqrt(baseCircleW * baseCircleW + baseCircleH * baseCircleH);
            float factor = targetDiag / baseDiag;
            targetW = baseCircleW * factor;
            targetH = baseCircleH * factor;
        }

        m_position = uiCircleSnap.position;
        m_size = uiCircleSnap.size;
        m_anchorOffset = uiCircleSnap.anchorOffset;
        m_rotation = uiCircleSnap.rotation;
        m_basePoints = uiCircleSnap.basePoints;

        float Sx = targetW / baseCircleW;
        float Sy = targetH / baseCircleH;

        sf::Vector2f fixedCorner = { uiCircleStartBounds.position.x, uiCircleStartBounds.position.y };
        applyGlobalScale(fixedCorner, Sx, Sy);
    }

    if (anyDeact) {
        isCircleUIScaling = false;
    }
    ImGui::Separator();

    ImGui::Separator();
    ImGui::Text("Appearance");

    // --- НОВАЯ КНОПКА СЛУЧАЙНЫХ ПАРАМЕТРОВ ---
    if (ImGui::Button("Randomize Colors & Thickness", ImVec2(-1, 0))) {
        // Рандомим цвет заливки (делаем полупрозрачным от 100 до 255)
        m_fillColor = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 100 + std::rand() % 155);

        // Рандомим обводку круга
        if (!m_sideColors.empty()) {
            m_sideColors[0] = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 255);
        }
        if (!m_sideThicknesses.empty()) {
            m_sideThicknesses[0] = 2.0f + static_cast<float>(std::rand() % 28); // Толщина от 2 до 30
        }
        updateGeometry();
    }

    // Заливка
    if (m_isClosed) {
        float fill[4] = { m_fillColor.r / 255.f, m_fillColor.g / 255.f, m_fillColor.b / 255.f, m_fillColor.a / 255.f };
        if (ImGui::ColorEdit4("Fill Color", fill)) {
            m_fillColor = sf::Color(fill[0] * 255, fill[1] * 255, fill[2] * 255, fill[3] * 255);
            updateGeometry();
        }
    }

    // --- ВОЗВРАЩАЕМ ЦВЕТ ОБВОДКИ КРУГА ---
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

    // ЯКОРЬ
    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position:");
    float pos[2] = { worldAnchor.x, worldAnchor.y };
    ImGui::SetNextItemWidth(150);
    if (ImGui::DragFloat2("##cAnchorDrag", pos, 1.0f)) setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputFloat2("##cAnchorInp", pos, "%.1f")) setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    ImGui::Separator();

}