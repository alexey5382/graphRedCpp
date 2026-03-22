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

    for (size_t i = 0; i < m_pointCount; ++i) {
        float t = i * 2.0f * PI / m_pointCount;
        float cosT = std::cos(t);
        float sinT = std::sin(t);

        // Внешние точки по границе рамки (SizeX, SizeY)
        float x = m_size.x * cosT;
        float y = m_size.y * sinT;
        outer[i] = sf::Vector2f(m_position.x + x, m_position.y + y);

        // Идеальная математическая нормаль для эллипса
        sf::Vector2f normal(m_size.y * cosT, m_size.x * sinT);
        float len = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (len > 0.001f) {
            normal.x /= len;
            normal.y /= len;
        }

        // Внутренняя точка для толщины (отступает строго перпендикулярно)
        inner[i] = sf::Vector2f(outer[i].x - normal.x * thickness, outer[i].y - normal.y * thickness);

        // Заливка занимает всё внутреннее пространство
        m_fillGeometry[i].position = inner[i];
        m_fillGeometry[i].color = m_fillColor;
    }

    // Собираем треугольники для обводки
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
    float dx = point.x - m_position.x;
    float dy = point.y - m_position.y;
    float rx = m_size.x;
    float ry = m_size.y;

    if (rx <= 0.0001f || ry <= 0.0001f) return false;

    // Проверяем по формуле эллипса
    return (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.0f;
}
void CircleShape::showImGuiProperties() {
    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position:");
    float pos[2] = { worldAnchor.x, worldAnchor.y };
    ImGui::SetNextItemWidth(150);
    if (ImGui::DragFloat2("##cAnchorDrag", pos, 1.0f)) {
        m_position += (sf::Vector2f(pos[0], pos[1]) - worldAnchor);
        updateGeometry();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputFloat2("##cAnchorInp", pos, "%.1f")) {
        m_position += (sf::Vector2f(pos[0], pos[1]) - worldAnchor);
        updateGeometry();
    }

    ImGui::Text("Radiuses (X/Y):");
    float rads[2] = { m_size.x, m_size.y };
    ImGui::SetNextItemWidth(150);
    bool radChanged = ImGui::SliderFloat2("##cRadSl", rads, 5.0f, 500.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    radChanged |= ImGui::InputFloat2("##cRadInp", rads, "%.1f");

    if (radChanged) {
        m_size.x = std::max(1.0f, rads[0]);
        m_size.y = std::max(1.0f, rads[1]);
        updateGeometry();
    }
    // Базовые трансформации
    ImGui::Separator();
    ImGui::Text("Appearance");

    // Заливка (Fill)
    float fill[4] = { m_fillColor.r / 255.f, m_fillColor.g / 255.f, m_fillColor.b / 255.f, m_fillColor.a / 255.f };
    if (ImGui::ColorEdit4("Fill Color", fill)) {
        m_fillColor = sf::Color(fill[0] * 255, fill[1] * 255, fill[2] * 255, fill[3] * 255);
        updateGeometry(); // Сразу перестраиваем фигуру!
    }

    // Обводка круга (Stroke)
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