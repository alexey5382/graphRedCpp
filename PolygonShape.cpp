#include "PolygonShape.h"
#include <cmath>
#include "imgui.h"

// Вспомогательная функция для нормализации вектора (если используешь старый стандарт)
// В SFML 3 есть встроенные методы, но так надежнее для любой версии
sf::Vector2f normalize(sf::Vector2f v) {
    float length = std::sqrt(v.x * v.x + v.y * v.y);
    if (length > 0.001f) return sf::Vector2f(v.x / length, v.y / length);
    return v;
}

PolygonShape::PolygonShape(sf::Vector2f position, sf::Vector2f size, bool isClosed,
    const std::vector<sf::Vector2f>& basePoints)
    : BaseShape(position, size, isClosed), m_basePoints(basePoints)
{
    // Инициализируем массивы цветов и толщин по умолчанию
    size_t segmentsCount = isClosed ? m_basePoints.size() : m_basePoints.size() - 1;
    m_sideColors.resize(segmentsCount, sf::Color::Black);
    m_sideThicknesses.resize(segmentsCount, 2.0f);
    m_fillColor = sf::Color(200, 200, 200, 100);

    // Настраиваем типы геометрии
    m_fillGeometry.setPrimitiveType(sf::PrimitiveType::TriangleFan); // Для заливки выпуклых фигур
    m_strokeGeometry.setPrimitiveType(sf::PrimitiveType::Triangles); // Для сторон используем треугольники
    updateGeometry(); // Сразу рассчитываем вершины при создании
}

sf::Vector2f PolygonShape::getIntersection(sf::Vector2f p1, sf::Vector2f dir1,
    sf::Vector2f p2, sf::Vector2f dir2,
    sf::Vector2f fallback, sf::Vector2f corner) const
{
    float cross = dir1.x * dir2.y - dir1.y * dir2.x;
    if (std::abs(cross) < 0.001f) return fallback;

    sf::Vector2f diff = p2 - p1;
    float t = (diff.x * dir2.y - diff.y * dir2.x) / cross;
    sf::Vector2f intersection = p1 + dir1 * t;

    // Срезаем слишком длинные "шипы" (ограничение как в твоем C# коде)
    sf::Vector2f offset = intersection - corner;
    float offsetLen = std::sqrt(offset.x * offset.x + offset.y * offset.y);
    if (offsetLen > 200.0f) {
        intersection = corner + normalize(offset) * 200.0f;
    }
    return intersection;
}

void PolygonShape::updateGeometry() {
    if (m_basePoints.empty()) return;
    size_t n = m_basePoints.size();

    // 1. Масштабируем и сдвигаем базовые точки
    std::vector<sf::Vector2f> pts(n);
    for (size_t i = 0; i < n; i++) {
        pts[i] = sf::Vector2f(m_position.x + m_basePoints[i].x * m_size.x,
            m_position.y + m_basePoints[i].y * m_size.y);
    }

    // 2. Обновляем заливку (HitArea)
    if (m_isClosed) {
        m_fillGeometry.resize(n);
        for (size_t i = 0; i < n; i++) {
            m_fillGeometry[i].position = pts[i];
            m_fillGeometry[i].color = m_fillColor;
        }
    }
    else {
        m_fillGeometry.clear(); // Ломаная не имеет заливки
    }

    // 3. Рассчитываем внешние и внутренние точки для "толстой" обводки
    std::vector<sf::Vector2f> outer(n);
    std::vector<sf::Vector2f> inner(n);

    for (size_t i = 0; i < n; i++) {
        outer[i] = pts[i]; // Внешняя граница строится ровно по точкам

        if (m_isClosed) {
            size_t prev = (i == 0) ? n - 1 : i - 1;
            size_t next = (i + 1) % n;

            sf::Vector2f dIn = normalize(pts[i] - pts[prev]);
            sf::Vector2f dOut = normalize(pts[next] - pts[i]);

            sf::Vector2f nIn(dIn.y, -dIn.x);
            sf::Vector2f nOut(dOut.y, -dOut.x);

            sf::Vector2f pIn1 = pts[i] - nIn * m_sideThicknesses[prev];
            sf::Vector2f pIn2 = pts[i] - nOut * m_sideThicknesses[i];

            sf::Vector2f fallback((pIn1.x + pIn2.x) / 2.0f, (pIn1.y + pIn2.y) / 2.0f);
            inner[i] = getIntersection(pIn1, dIn, pIn2, dOut, fallback, pts[i]);
        }
        else {
            // Логика для незамкнутой ломаной (Polyline)
            if (i == 0) {
                sf::Vector2f dOut = normalize(pts[1] - pts[0]);
                sf::Vector2f nOut(dOut.y, -dOut.x);
                inner[0] = pts[0] - nOut * m_sideThicknesses[0];
            }
            else if (i == n - 1) {
                sf::Vector2f dIn = normalize(pts[n - 1] - pts[n - 2]);
                sf::Vector2f nIn(dIn.y, -dIn.x);
                inner[n - 1] = pts[n - 1] - nIn * m_sideThicknesses[n - 2];
            }
            else {
                sf::Vector2f dIn = normalize(pts[i] - pts[i - 1]);
                sf::Vector2f dOut = normalize(pts[i + 1] - pts[i]);
                sf::Vector2f nIn(dIn.y, -dIn.x);
                sf::Vector2f nOut(dOut.y, -dOut.x);

                sf::Vector2f pIn1 = pts[i] - nIn * m_sideThicknesses[i - 1];
                sf::Vector2f pIn2 = pts[i] - nOut * m_sideThicknesses[i];
                sf::Vector2f fallback((pIn1.x + pIn2.x) / 2.0f, (pIn1.y + pIn2.y) / 2.0f);
                inner[i] = getIntersection(pIn1, dIn, pIn2, dOut, fallback, pts[i]);
            }
        }
    }

    // 4. Заполняем геометрию обводки (Каждая сторона - это 2 треугольника = 6 точек)
    size_t segmentsCount = m_isClosed ? n : n - 1;
    m_strokeGeometry.resize(segmentsCount * 6); // Увеличили множитель до 6

    for (size_t i = 0; i < segmentsCount; i++) {
        size_t next = (i + 1) % n;

        // Вершины нашего "четырехугольника" (стороны)
        // 0: outer[i], 1: outer[next], 2: inner[next], 3: inner[i]

        // Первый треугольник (0, 1, 2)
        m_strokeGeometry[i * 6 + 0].position = outer[i];
        m_strokeGeometry[i * 6 + 1].position = outer[next];
        m_strokeGeometry[i * 6 + 2].position = inner[next];

        // Второй треугольник (0, 2, 3)
        m_strokeGeometry[i * 6 + 3].position = outer[i];
        m_strokeGeometry[i * 6 + 4].position = inner[next];
        m_strokeGeometry[i * 6 + 5].position = inner[i];

        // Применяем цвет конкретной стороны ко всем 6 вершинам
        for (int j = 0; j < 6; j++) {
            m_strokeGeometry[i * 6 + j].color = m_sideColors[i];
        }
    }
}

// Отрисовка фигуры на холсте
void PolygonShape::draw(sf::RenderTarget& target) const {
    if (m_isClosed) target.draw(m_fillGeometry);
    target.draw(m_strokeGeometry);

    // --- НОВОЕ: Рисуем рамку выделения ---
   
}

bool PolygonShape::contains(sf::Vector2f point) const {
    if (m_basePoints.empty()) return false;

    std::vector<sf::Vector2f> pts(m_basePoints.size());
    for (size_t i = 0; i < m_basePoints.size(); i++) {
        pts[i] = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
    }

    if (!m_isClosed) {
        // --- ВЫДЕЛЕНИЕ ЛОМАНОЙ: Математика расстояния от точки до отрезка ---
        for (size_t i = 0; i < pts.size() - 1; i++) {
            sf::Vector2f a = pts[i];
            sf::Vector2f b = pts[i + 1];

            float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
            if (l2 == 0.0f) continue;

            // Находим проекцию клика на прямую отрезка
            float t = std::max(0.0f, std::min(1.0f, ((point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y)) / l2));
            sf::Vector2f proj = a + t * (b - a);

            // Расстояние от клика до проекции
            float dist2 = (point.x - proj.x) * (point.x - proj.x) + (point.y - proj.y) * (point.y - proj.y);
            float thick = (m_sideThicknesses[i] / 2.0f) + 5.0f; // Учитываем толщину + допуск 5 пикселей для удобства

            if (dist2 <= thick * thick) return true;
        }
        return false;
    }

    // --- ВЫДЕЛЕНИЕ ЗАМКНУТЫХ (Ray-casting) ---
    bool inside = false;
    for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
        if (((pts[i].y > point.y) != (pts[j].y > point.y)) &&
            (point.x < (pts[j].x - pts[i].x) * (point.y - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

void PolygonShape::showImGuiProperties() {
    // 1. МАСШТАБ
    ImGui::Text("Global Scale X/Y:");
    float scale[2] = { m_size.x, m_size.y };
    ImGui::SetNextItemWidth(150);
    bool scaleChanged = ImGui::SliderFloat2("##pScaleSl", scale, 5.0f, 500.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    scaleChanged |= ImGui::InputFloat2("##pScaleInp", scale, "%.1f");
    if (scaleChanged) {
        m_size.x = std::max(1.0f, scale[0]);
        m_size.y = std::max(1.0f, scale[1]);
        updateGeometry();
    }
    ImGui::Separator();

    // 2. ЗАЛИВКА
    if (m_isClosed) {
        float fill[4] = { m_fillColor.r / 255.f, m_fillColor.g / 255.f, m_fillColor.b / 255.f, m_fillColor.a / 255.f };
        if (ImGui::ColorEdit4("Fill Color", fill)) {
            m_fillColor = sf::Color(fill[0] * 255, fill[1] * 255, fill[2] * 255, fill[3] * 255);
            updateGeometry();
        }
        ImGui::Separator();
    }

    // 3. ЯКОРЬ (Перенесен под заливку)
    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position:");
    float pos[2] = { worldAnchor.x, worldAnchor.y };
    ImGui::SetNextItemWidth(150);
    if (ImGui::DragFloat2("##pAnchorDrag", pos, 1.0f)) {
        m_position += (sf::Vector2f(pos[0], pos[1]) - worldAnchor);
        updateGeometry();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputFloat2("##pAnchorInp", pos, "%.1f")) {
        m_position += (sf::Vector2f(pos[0], pos[1]) - worldAnchor);
        updateGeometry();
    }
    ImGui::Separator();

    const size_t numPoints = m_basePoints.size();
    if (numPoints < 2) return;

    // 4. ПАРАМЕТРЫ СТОРОН (Цвет, Толщина, Длина)
    size_t segmentsCount = m_isClosed ? numPoints : numPoints - 1;
    size_t startIdx = (m_selectedSide != -1) ? m_selectedSide : 0;
    size_t endIdx = (m_selectedSide != -1) ? m_selectedSide + 1 : segmentsCount;

    if (ImGui::CollapsingHeader("Sides Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_selectedSide != -1) {
            ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "Editing Selected Side %d", m_selectedSide + 1);
            if (ImGui::Button("Deselect Side", ImVec2(-1, 0))) m_selectedSide = -1;
            ImGui::Separator();
        }

        for (size_t i = startIdx; i < endIdx; i++) {
            ImGui::PushID(static_cast<int>(i));
            size_t next = (i + 1) % numPoints;

            sf::Vector2f p1 = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
            sf::Vector2f p2 = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);
            sf::Vector2f diff = p2 - p1;
            float length = std::sqrt(diff.x * diff.x + diff.y * diff.y);

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Side %zu (Node %zu -> %zu)", i + 1, i + 1, next + 1);

            // Цвет
            float stroke[4] = { m_sideColors[i].r / 255.f, m_sideColors[i].g / 255.f, m_sideColors[i].b / 255.f, m_sideColors[i].a / 255.f };
            ImGui::SetNextItemWidth(150);
            if (ImGui::ColorEdit4("##Col", stroke, ImGuiColorEditFlags_NoInputs)) {
                m_sideColors[i] = sf::Color(stroke[0] * 255, stroke[1] * 255, stroke[2] * 255, stroke[3] * 255);
                updateGeometry();
            }
            ImGui::SameLine();

            // Толщина
            float thick = m_sideThicknesses[i];
            ImGui::SetNextItemWidth(80);
            if (ImGui::SliderFloat("##ThSl", &thick, 1.0f, 50.0f, "")) { m_sideThicknesses[i] = thick; updateGeometry(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(40);
            if (ImGui::InputFloat("Thick", &thick, 0.0f, 0.0f, "%.1f")) {
                m_sideThicknesses[i] = std::max(1.0f, std::min(100.0f, thick)); updateGeometry();
            }

            // Длина (перенесена прямо под толщину!)
            float editLen = length;
            ImGui::SetNextItemWidth(100);
            bool lenChanged = ImGui::SliderFloat("##LnSl", &editLen, 10.0f, 1000.0f, "");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            lenChanged |= ImGui::InputFloat("Length", &editLen, 0.0f, 0.0f, "%.1f");

            if (lenChanged) {
                float newLen = std::max(1.0f, editLen);
                if (length > 0.01f) {
                    sf::Vector2f newP2 = p1 + (diff / length) * newLen;
                    m_basePoints[next].x = (newP2.x - m_position.x) / m_size.x;
                    m_basePoints[next].y = (newP2.y - m_position.y) / m_size.y;
                    updateGeometry();
                }
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    // 5. НОВЫЙ БЛОК: ВЕРШИНЫ И УГЛЫ
    if (ImGui::CollapsingHeader("Vertices Coordinates & Angles", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (size_t i = 0; i < numPoints; i++) {
            ImGui::PushID(static_cast<int>(i + 1000));
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Node %zu", i + 1);

            // Координаты вершины (Относительно якоря)
            sf::Vector2f p = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
            sf::Vector2f relNode = p - worldAnchor;
            float relData[2] = { relNode.x, relNode.y };
            ImGui::SetNextItemWidth(150);
            if (ImGui::DragFloat2("Coord (Rel)", relData, 1.0f)) {
                sf::Vector2f newWorldNode = worldAnchor + sf::Vector2f(relData[0], relData[1]);
                m_basePoints[i].x = (newWorldNode.x - m_position.x) / m_size.x;
                m_basePoints[i].y = (newWorldNode.y - m_position.y) / m_size.y;
                updateGeometry();
            }

            // Угол (Turn Angle)
            bool canCalcAngle = m_isClosed || i > 0;
            if (canCalcAngle) {
                size_t prev = (i + numPoints - 1) % numPoints;
                size_t next = (i + 1) % numPoints;

                sf::Vector2f p0 = m_position + sf::Vector2f(m_basePoints[prev].x * m_size.x, m_basePoints[prev].y * m_size.y);
                sf::Vector2f p1 = p; // Текущая вершина
                sf::Vector2f p2 = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);

                sf::Vector2f v_in = p1 - p0;
                sf::Vector2f v_out = p2 - p1;

                float len_in = std::sqrt(v_in.x * v_in.x + v_in.y * v_in.y);
                float len_out = std::sqrt(v_out.x * v_out.x + v_out.y * v_out.y);

                if (len_in > 0.1f && len_out > 0.1f) {
                    float dot = v_in.x * v_out.x + v_in.y * v_out.y;
                    float det = v_in.x * v_out.y - v_in.y * v_out.x;
                    float angleDeg = std::atan2(det, dot) * 180.0f / 3.14159265f;

                    float editAng = angleDeg;
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::SliderFloat("Turn Angle", &editAng, -180.0f, 180.0f, "%.1f deg")) {
                        float targetAngRad = editAng * 3.14159265f / 180.0f;
                        float baseAngRad = std::atan2(v_in.y, v_in.x);
                        float finalAngRad = baseAngRad + targetAngRad;

                        sf::Vector2f newP2 = p1 + sf::Vector2f(std::cos(finalAngRad) * len_out, std::sin(finalAngRad) * len_out);
                        m_basePoints[next].x = (newP2.x - m_position.x) / m_size.x;
                        m_basePoints[next].y = (newP2.y - m_position.y) / m_size.y;
                        updateGeometry();
                    }
                }
            }
            else {
                ImGui::TextDisabled("Angle: N/A (Start Node)");
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }
}
void PolygonShape::setSideColor(size_t index, sf::Color color) {
    if (index < m_sideColors.size()) {
        m_sideColors[index] = color;
        updateGeometry();
    }
}

void PolygonShape::setSideThickness(size_t index, float thickness) {
    if (index < m_sideThicknesses.size()) {
        m_sideThicknesses[index] = thickness;
        updateGeometry();
    }
}
sf::FloatRect PolygonShape::getBounds() const {
    if (m_basePoints.empty()) return BaseShape::getBounds();

    // Инициализируем cực времён экстремальными значениями или первой точкой
    sf::Vector2f firstP = m_position + sf::Vector2f(m_basePoints[0].x * m_size.x, m_basePoints[0].y * m_size.y);
    float minX = firstP.x, minY = firstP.y, maxX = firstP.x, maxY = firstP.y;

    // Пробегаем по всем вершинам
    for (size_t i = 1; i < m_basePoints.size(); i++) {
        // Вычисляем мировую координату узла i
        float px = m_position.x + m_basePoints[i].x * m_size.x;
        float py = m_position.y + m_basePoints[i].y * m_size.y;

        if (px < minX) minX = px; if (px > maxX) maxX = px;
        if (py < minY) minY = py; if (py > maxY) maxY = py;
    }

    // --- ИСПРАВЛЕНИЕ: Рамка строго по крайним точкам, без отступов ---
    return sf::FloatRect({ minX, minY }, { maxX - minX, maxY - minY });
}
void PolygonShape::drawSelection(sf::RenderTarget& target) const {
    // --- ИСПРАВЛЕНИЕ: Прячем рамку и узлы, если фигура не выделена ---
    if (!m_isSelected) return;

    BaseShape::drawSelection(target); // Рисуем синюю рамку и голубой якорь

    // Отрисовка подсвеченной стороны (с фиксированной толщиной)
    if (m_selectedSide != -1 && m_selectedSide < (m_isClosed ? m_basePoints.size() : m_basePoints.size() - 1)) {
        size_t next = (m_selectedSide + 1) % m_basePoints.size();
        sf::Vector2f p1 = m_position + sf::Vector2f(m_basePoints[m_selectedSide].x * m_size.x, m_basePoints[m_selectedSide].y * m_size.y);
        sf::Vector2f p2 = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);

        sf::Vector2f diff = p2 - p1;
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        float angle = std::atan2(diff.y, diff.x);

        float fixedThick = 6.0f; // Фиксированная толщина линии подсветки
        sf::RectangleShape hlRect({ len, fixedThick });
        hlRect.setOrigin({ 0, fixedThick / 2.0f });
        hlRect.setPosition(p1);
        hlRect.setRotation(sf::degrees(angle));
        hlRect.setFillColor(sf::Color(255, 255, 0, 180)); // Ярко-желтая прозрачная линия
        target.draw(hlRect);
    }

    // Рисуем кружочки на вершинах
    for (size_t i = 0; i < m_basePoints.size(); i++) {
        sf::Vector2f p = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
        sf::CircleShape vHandle(5.0f);
        vHandle.setOrigin({ 5.0f, 5.0f });
        vHandle.setPosition(p);
        vHandle.setFillColor(sf::Color::Yellow);
        vHandle.setOutlineColor(sf::Color::Black);
        vHandle.setOutlineThickness(1.0f);
        target.draw(vHandle);
    }
}
int PolygonShape::getHitHandle(sf::Vector2f mousePos) const {
    if (!m_isSelected) return 0;

    // 1. Сначала проверяем кружочки вершин (коды 100, 101, 102...)
    float hw = 6.0f;
    for (size_t i = 0; i < m_basePoints.size(); i++) {
        sf::Vector2f p = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
        if (mousePos.x >= p.x - hw && mousePos.x <= p.x + hw && mousePos.y >= p.y - hw && mousePos.y <= p.y + hw) {
            return 100 + static_cast<int>(i);
        }
    }

    // 2. Если не попали в вершины, проверяем рамку и якорь
    return BaseShape::getHitHandle(mousePos);
}

void PolygonShape::moveVertex(int index, sf::Vector2f delta) {
    if (index >= 0 && index < m_basePoints.size()) {
        sf::Vector2f worldPos = m_position + sf::Vector2f(m_basePoints[index].x * m_size.x, m_basePoints[index].y * m_size.y);
        worldPos += delta; // Сдвигаем узел
        m_basePoints[index].x = (worldPos.x - m_position.x) / m_size.x;
        m_basePoints[index].y = (worldPos.y - m_position.y) / m_size.y;
        updateGeometry();
    }
}

int PolygonShape::getHitSideIndex(sf::Vector2f mousePos) const {
    if (!m_isSelected) return -1;
    size_t segmentsCount = m_isClosed ? m_basePoints.size() : m_basePoints.size() - 1;
    for (size_t i = 0; i < segmentsCount; i++) {
        size_t next = (i + 1) % m_basePoints.size();
        sf::Vector2f a = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
        sf::Vector2f b = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);

        float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
        if (l2 == 0.0f) continue;

        float t = std::max(0.0f, std::min(1.0f, ((mousePos.x - a.x) * (b.x - a.x) + (mousePos.y - a.y) * (b.y - a.y)) / l2));
        sf::Vector2f proj = a + t * (b - a);

        float dist2 = (mousePos.x - proj.x) * (mousePos.x - proj.x) + (mousePos.y - proj.y) * (mousePos.y - proj.y);
        float thick = (m_sideThicknesses[i] / 2.0f) + 5.0f; // Допуск 5 пикселей

        if (dist2 <= thick * thick) return i;
    }
    return -1;
}
void PolygonShape::save(std::ostream& out) const {
    BaseShape::save(out);
    out << m_basePoints.size() << "\n";
    for (const auto& p : m_basePoints) out << p.x << " " << p.y << " ";
    out << "\n";
}

void PolygonShape::load(std::istream& in) {
    BaseShape::load(in);
    size_t ptsCount;
    in >> ptsCount;
    m_basePoints.resize(ptsCount);
    for (size_t i = 0; i < ptsCount; i++) {
        in >> m_basePoints[i].x >> m_basePoints[i].y;
    }
    updateGeometry();
}