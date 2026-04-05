#include "CircleShape.h"
#include <cmath>
#include "imgui.h"

CircleShape::CircleShape(sf::Vector2f position, sf::Vector2f size)
    : BaseShape(position, size, true), m_pointCount(64) // 64 точки дают плавный круг
{
    // У круга только 1 цвет и 1 толщина обводки
    m_sideColors.push_back(sf::Color::Black);
    m_sideThicknesses.push_back(2.0f);
    m_fillColor = sf::Color(200, 200, 200, 100);

    m_fillGeometry.setPrimitiveType(sf::PrimitiveType::TriangleFan);
    m_strokeGeometry.setPrimitiveType(sf::PrimitiveType::Triangles);

    updateGeometry();
}



void CircleShape::updateGeometry() {
    m_fillGeometry.resize(m_pointCount);
    m_strokeGeometry.resize(m_pointCount * 6);

    float thickness = m_sideThicknesses.empty() ? 2.0f : m_sideThicknesses[0];
    sf::Color strokeColor = m_sideColors.empty() ? sf::Color::Black : m_sideColors[0];

    std::vector<sf::Vector2f> outer(m_pointCount);
    std::vector<sf::Vector2f> inner(m_pointCount);

    const float PI = 3.14159265f;
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    for (size_t i = 0; i < m_pointCount; ++i) {
        float t = i * 2.0f * PI / m_pointCount;
        float cosT = std::cos(t);
        float sinT = std::sin(t);

        // 1. Исходные математические координаты внешней точки
        sf::Vector2f unrotatedOuter(m_position.x + m_size.x * cosT, m_position.y + m_size.y * sinT);

        // Вращаем внешнюю точку
        outer[i] = BaseShape::rotatePoint(unrotatedOuter, anchorWorld, m_rotation);

        // 2. Вычисляем и вращаем нормаль (направление отступа толщины)
        sf::Vector2f normal(m_size.y * cosT, m_size.x * sinT);
        float len = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (len > 0.001f) { normal.x /= len; normal.y /= len; }

        float rad = m_rotation * PI / 180.0f;
        float nX = normal.x * std::cos(rad) - normal.y * std::sin(rad);
        float nY = normal.x * std::sin(rad) + normal.y * std::cos(rad);
        sf::Vector2f rotatedNormal(nX, nY);

        // 3. Внутренняя точка
        inner[i] = sf::Vector2f(outer[i].x - rotatedNormal.x * thickness, outer[i].y - rotatedNormal.y * thickness);

        m_fillGeometry[i].position = inner[i];
        m_fillGeometry[i].color = m_fillColor;
    }

    for (size_t i = 0; i < m_pointCount; ++i) {
        size_t next = (i + 1) % m_pointCount;

        m_strokeGeometry[i * 6 + 0].position = outer[i];
        m_strokeGeometry[i * 6 + 1].position = outer[next];
        m_strokeGeometry[i * 6 + 2].position = inner[next];

        m_strokeGeometry[i * 6 + 3].position = outer[i];
        m_strokeGeometry[i * 6 + 4].position = inner[next];
        m_strokeGeometry[i * 6 + 5].position = inner[i];

        for (int j = 0; j < 6; ++j) {
            m_strokeGeometry[i * 6 + j].color = strokeColor;
        }
    }
}

void CircleShape::draw(sf::RenderTarget& target) const {
    target.draw(m_fillGeometry);
    target.draw(m_strokeGeometry);
}

bool CircleShape::contains(sf::Vector2f point) const {
    if (m_size.x <= 0.0001f || m_size.y <= 0.0001f) return false;

    // Крутим мышку "обратно" вокруг якоря
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    sf::Vector2f unrotatedPt = BaseShape::rotatePoint(point, anchorWorld, -m_rotation);

    float dx = unrotatedPt.x - m_position.x;
    float dy = unrotatedPt.y - m_position.y;

    return (dx * dx) / (m_size.x * m_size.x) + (dy * dy) / (m_size.y * m_size.y) <= 1.0f;
}
void CircleShape::showImGuiProperties() {
    // Вращение
    ImGui::Text("Rotation:");
    float rot = m_rotation;
    ImGui::SetNextItemWidth(120);
    bool rotChanged = ImGui::DragFloat("##cRotDrag", &rot, 1.0f, -180.0f, 180.0f, "%.1f deg");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    rotChanged |= ImGui::InputFloat("Angle", &rot, 0.0f, 0.0f, "%.1f");
    if (rotChanged) {
        setRotation(rot);
    }

    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position:");
    float pos[2] = { worldAnchor.x, worldAnchor.y };
    ImGui::SetNextItemWidth(150);
    // ИСПОЛЬЗУЕМ НОВЫЙ МЕТОД ДЛЯ IMGUI (чтобы при ручном вводе фигура тоже не дергалась)
    if (ImGui::DragFloat2("##pAnchorDrag", pos, 1.0f)) {
        setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputFloat2("##pAnchorInp", pos, "%.1f")) {
        setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    }

    // --- МАСШТАБ КРУГА СО СНИМКАМИ ---
    ImGui::Text("Bounding Box Scale (W/H):");
    sf::FloatRect bounds = getBounds();

    // Статические переменные для хранения снимка
    static bool isCircleUIScaling = false;
    static ShapeSnapshot uiCircleSnap;
    static sf::FloatRect uiCircleStartBounds;
    static float baseCircleW = 1.0f, baseCircleH = 1.0f;

    float scale[2] = { bounds.size.x, bounds.size.y };

    // Абсолютный масштаб
    ImGui::SetNextItemWidth(150);
    bool changed1 = ImGui::SliderFloat2("##cScaleSl", scale, 10.0f, 1000.0f);
    bool act1 = ImGui::IsItemActivated(); bool deact1 = ImGui::IsItemDeactivated();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    bool changed2 = ImGui::InputFloat2("##cScaleInp", scale, "%.1f");
    bool act2 = ImGui::IsItemActivated(); bool deact2 = ImGui::IsItemDeactivated();

    // Относительный масштаб (%)
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

    // 1. ФОТОГРАФИРУЕМ при клике по любому из ползунков
    if (act1 || act2 || act3 || act4) {
        isCircleUIScaling = true;
        uiCircleSnap = { m_position, m_size, m_anchorOffset, m_rotation };
        uiCircleStartBounds = bounds;
        baseCircleW = bounds.size.x;
        baseCircleH = bounds.size.y;
    }
    // Отключаем режим, когда отпустили мышь
    if (deact1 || deact2 || deact3 || deact4) {
        isCircleUIScaling = false;
    }

    // 2. ПРИМЕНЯЕМ масштаб строго к снимку
    if (isCircleUIScaling && (changed1 || changed2 || changed3 || changed4)) {
        float targetW = scale[0];
        float targetH = scale[1];

        // Если тянули проценты — переводим их в размер рамки
        if (changed3 || changed4) {
            float targetDiag = relScale * 1.41421356f;
            float baseDiag = std::sqrt(baseCircleW * baseCircleW + baseCircleH * baseCircleH);
            float factor = targetDiag / baseDiag;
            targetW = baseCircleW * factor;
            targetH = baseCircleH * factor;
        }

        // Откат к идеалу
        m_position = uiCircleSnap.position;
        m_size = uiCircleSnap.size;
        m_anchorOffset = uiCircleSnap.anchorOffset;
        m_rotation = uiCircleSnap.rotation;

        // Применяем вычисленный масштаб
        float Sx = targetW / baseCircleW;
        float Sy = targetH / baseCircleH;
        resizeFromBoundingBox(uiCircleStartBounds.size.x * Sx, uiCircleStartBounds.size.y * Sy);
    }
    // ---------------------------------
    ImGui::Separator();

    ImGui::Text("Appearance");

    float fill[4] = { m_fillColor.r / 255.f, m_fillColor.g / 255.f, m_fillColor.b / 255.f, m_fillColor.a / 255.f };
    if (ImGui::ColorEdit4("Fill Color", fill)) {
        m_fillColor = sf::Color(fill[0] * 255, fill[1] * 255, fill[2] * 255, fill[3] * 255);
        updateGeometry();
    }

    if (!m_sideColors.empty() && !m_sideThicknesses.empty()) {
        float stroke[4] = { m_sideColors[0].r / 255.f, m_sideColors[0].g / 255.f, m_sideColors[0].b / 255.f, m_sideColors[0].a / 255.f };
        if (ImGui::ColorEdit4("Stroke Color", stroke)) {
            m_sideColors[0] = sf::Color(stroke[0] * 255, stroke[1] * 255, stroke[2] * 255, stroke[3] * 255);
            updateGeometry();
        }
        if (ImGui::SliderFloat("Thickness", &m_sideThicknesses[0], 1.0f, 50.0f)) {
            updateGeometry();
        }
    }
}
void CircleShape::setStrokeColor(sf::Color color) {
    if (!m_sideColors.empty()) {
        m_sideColors[0] = color;
        updateGeometry();
    }
}

void CircleShape::setStrokeThickness(float thickness) {
    if (!m_sideThicknesses.empty()) {
        m_sideThicknesses[0] = thickness;
        updateGeometry();
    }
}
void CircleShape::save(std::ostream& out) const { BaseShape::save(out); }
void CircleShape::load(std::istream& in) { BaseShape::load(in); updateGeometry(); }




sf::FloatRect CircleShape::getBounds() const {
    if (m_pointCount == 0) return BaseShape::getBounds();

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
    const float PI = 3.14159265f;

    for (size_t i = 0; i < m_pointCount; ++i) {
        float t = i * 2.0f * PI / m_pointCount;
        sf::Vector2f unrotated(m_position.x + m_size.x * std::cos(t), m_position.y + m_size.y * std::sin(t));
        sf::Vector2f rotated = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);

        if (rotated.x < minX) minX = rotated.x; if (rotated.x > maxX) maxX = rotated.x;
        if (rotated.y < minY) minY = rotated.y; if (rotated.y > maxY) maxY = rotated.y;
    }

    return sf::FloatRect({ minX, minY }, { maxX - minX, maxY - minY });
}