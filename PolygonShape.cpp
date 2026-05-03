#include "PolygonShape.h"
#include <cmath>
#include "imgui.h"
#include <algorithm> // Для std::min/max
#include <vector>


static PolygonShape* g_batchShape = nullptr;
static std::vector<float> g_batchAngles;
static std::vector<float> g_batchLengths; // <-- НОВОЕ
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
    size_t segmentsCount = isClosed ? m_basePoints.size() : m_basePoints.size() - 1;
    m_sideColors.resize(segmentsCount, sf::Color::Black);
    m_sideThicknesses.resize(segmentsCount, 2.0f);
    m_fillColor = sf::Color(200, 200, 200, 100);

    m_fillGeometry.setPrimitiveType(sf::PrimitiveType::Triangles); 
    m_strokeGeometry.setPrimitiveType(sf::PrimitiveType::Triangles);
    updateGeometry();
}
static float getCrossProduct(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}


static bool isPointInTriangle(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c) {
    // Вычисляем cross-product для каждой стороны
    float cp1 = getCrossProduct(a, b, p);
    float cp2 = getCrossProduct(b, c, p);
    float cp3 = getCrossProduct(c, a, p);

    // Проверяем, имеют ли все они один знак
    // (P находится с одной стороны от всех трех векторов)
    bool isAllPos = (cp1 > 0.0001f) && (cp2 > 0.0001f) && (cp3 > 0.0001f);
    bool isAllNeg = (cp1 < -0.0001f) && (cp2 < -0.0001f) && (cp3 < -0.0001f);

    return (isAllPos || isAllNeg);
}
void PolygonShape::updateGeometry() {
    if (m_basePoints.empty()) return;
    size_t n = m_basePoints.size();
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // 1. Масштабируем, сдвигаем И ВРАЩАЕМ базовые точки (pts - мировые координаты)
    std::vector<sf::Vector2f> pts(n);
    for (size_t i = 0; i < n; i++) {
        sf::Vector2f unrotated(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        pts[i] = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);
    }

    // --- НОВОЕ: Определяем направление обхода (Shoelace formula) ---
    float signedArea = 0.0f;
    if (m_isClosed && n >= 3) {
        for (size_t i = 0; i < n; i++) {
            size_t next = (i + 1) % n;
            signedArea += (pts[i].x * pts[next].y - pts[next].x * pts[i].y);
        }
    }
    // В экранных координатах (Y вниз) положительная площадь = по часовой стрелке (CW).
    bool isCW = (signedArea > 0.0f);
    float normalSign = (signedArea >= 0.0f || !m_isClosed) ? 1.0f : -1.0f;


    // 2. Обновляем заливку с использованием алгоритма Ear Clipping
    m_fillGeometry.clear();

    // SFML Triangles - список отдельных треугольников, каждый описывается 3 вершинами
    // В draw() нужно задать: target.draw(m_fillGeometry, sf::Triangles, ...);
    if (m_isClosed && n >= 3) {

        std::vector<sf::Vector2f> trianglesResult; // Список вершин (3 на треугольник)

        // Временный список индексов вершин, которые мы будем «отсекать»
        std::vector<int> vertexIndices(n);
        for (int i = 0; i < n; ++i) vertexIndices[i] = i;

        // Полагаемся на signedArea: если фигура нарисована против часовой,
        // нам нужно инвертировать проверки в алгоритме
        bool shouldInvertOrientation = !isCW;

        int iterations = 0;
        // Работаем, пока не останется 3 вершины (один треугольник)
        while (vertexIndices.size() >= 3 && iterations < 1000) {
            iterations++; // Защита от бесконечного цикла
            bool earFound = false;

            for (int i = 0; i < vertexIndices.size(); ++i) {
                // Берем три последовательных индекса (ABC)
                int prevIdx = vertexIndices[(i == 0) ? vertexIndices.size() - 1 : i - 1];
                int currIdx = vertexIndices[i];
                int nextIdx = vertexIndices[(i == vertexIndices.size() - 1) ? 0 : i + 1];

                sf::Vector2f a = pts[prevIdx];
                sf::Vector2f b = pts[currIdx];
                sf::Vector2f c = pts[nextIdx];

                // --- ШАГ 1: Проверяем, является ли угол внутренним (выпуклым) ---
                float cp = getCrossProduct(a, b, c);
                bool isConvex = (cp > 0.0f);

                // Если мы против часовой стрелки, cp инвертируется
                if (shouldInvertOrientation) isConvex = (cp < 0.0f);

                // Если угол вогнутый, он не может быть «ухом» (он смотрит наружу)
                if (!isConvex) continue;

                // --- ШАГ 2: Проверяем, нет ли других вершин внутри треугольника ABC ---
                bool anyPointInside = false;
                for (int j = 0; j < vertexIndices.size(); ++j) {
                    int checkIdx = vertexIndices[j];
                    // Не проверяем саму вершину или её соседей (ABC)
                    if (checkIdx == prevIdx || checkIdx == currIdx || checkIdx == nextIdx) continue;

                    if (isPointInTriangle(pts[checkIdx], a, b, c)) {
                        anyPointInside = true;
                        break; // Треугольник не пустой, не «ухо»
                    }
                }

                // --- ШАГ 3: «Ухо» найдено, отсекаем его! ---
                if (!anyPointInside) {
                    trianglesResult.push_back(a);
                    trianglesResult.push_back(b);
                    trianglesResult.push_back(c);

                    // Удаляем среднюю вершину из списка активных узлов
                    vertexIndices.erase(vertexIndices.begin() + i);
                    earFound = true;
                    break; // Возвращаемся в начало while
                }
            }

            // Если не смогли найти ни одного «уха», фигура слишком сложная 
            // (или есть самопересечения). В этом случае триангуляция невозможна без 
            // внешних библиотек (libtess2). У Ear Clipping есть ограничения.
            if (!earFound) {
                // Если Ear Clipping зашел в тупик (часто на самопересечениях),
                // применяем фоллбек: просто заливаем веером, как было.
                // (Двойная заливка вернется на самопересечениях, но фигура не исчезнет)
                if (vertexIndices.size() >= 3) {
                    int pivotIdx = vertexIndices[0];
                    for (int j = 1; j < vertexIndices.size() - 1; ++j) {
                        trianglesResult.push_back(pts[pivotIdx]);
                        trianglesResult.push_back(pts[vertexIndices[j]]);
                        trianglesResult.push_back(pts[vertexIndices[j + 1]]);
                    }
                }
                break; // Выходим из while
            }
        }

        // Заполняем sf::VertexArray результатом триангуляции
        if (!trianglesResult.empty()) {
            m_fillGeometry.resize(trianglesResult.size());
            for (size_t i = 0; i < trianglesResult.size(); ++i) {
                m_fillGeometry[i].position = trianglesResult[i];
                m_fillGeometry[i].color = m_fillColor;
            }
        }
    }
    else { m_fillGeometry.clear(); }

    // --- НОВОЕ: Определяем направление обхода для правильной толщины обводки ---
    if (m_isClosed && n >= 3) {
        for (size_t i = 0; i < n; i++) {
            size_t next = (i + 1) % n;
            // Считаем площадь по мировым координатам
            signedArea += (pts[i].x * pts[next].y - pts[next].x * pts[i].y);
        }
    }
    // В экранных координатах (Y вниз) положительная площадь = по часовой стрелке (CW).
    // Если фигура нарисована против часовой, нормали будут смотреть "в другую сторону".
    // Коэффициент normalSign развернет их так, чтобы толщина ВСЕГДА уходила внутрь.
    // Для незамкнутой ломаной оставляем как есть.

    // 3. Рассчитываем точки обводки
    std::vector<sf::Vector2f> outer(n);
    std::vector<sf::Vector2f> inner(n);

    for (size_t i = 0; i < n; i++) {
        outer[i] = pts[i]; // Внешний край всегда лежит точно на математической границе фигуры

        if (m_isClosed) {
            size_t prev = (i == 0) ? n - 1 : i - 1;
            size_t next = (i + 1) % n;

            sf::Vector2f dIn = normalize(pts[i] - pts[prev]);
            sf::Vector2f dOut = normalize(pts[next] - pts[i]);

            // Получаем нормали с учетом направления обхода фигуры
            sf::Vector2f nIn(dIn.y * normalSign, -dIn.x * normalSign);
            sf::Vector2f nOut(dOut.y * normalSign, -dOut.x * normalSign);

            // Отступаем внутрь на заданную толщину
            sf::Vector2f pIn1 = pts[i] - nIn * m_sideThicknesses[prev];
            sf::Vector2f pIn2 = pts[i] - nOut * m_sideThicknesses[i];

            sf::Vector2f fallback((pIn1.x + pIn2.x) / 2.0f, (pIn1.y + pIn2.y) / 2.0f);
            inner[i] = getIntersection(pIn1, dIn, pIn2, dOut, fallback, pts[i]);
        }
        else {
            // Для ломаной линии применяем тот же normalSign, чтобы поведение было предсказуемым
            if (i == 0) {
                sf::Vector2f dOut = normalize(pts[1] - pts[0]);
                sf::Vector2f nOut(dOut.y * normalSign, -dOut.x * normalSign);
                inner[0] = pts[0] - nOut * m_sideThicknesses[0];
            }
            else if (i == n - 1) {
                sf::Vector2f dIn = normalize(pts[n - 1] - pts[n - 2]);
                sf::Vector2f nIn(dIn.y * normalSign, -dIn.x * normalSign);
                inner[n - 1] = pts[n - 1] - nIn * m_sideThicknesses[n - 2];
            }
            else {
                sf::Vector2f dIn = normalize(pts[i] - pts[i - 1]);
                sf::Vector2f dOut = normalize(pts[i + 1] - pts[i]);
                sf::Vector2f nIn(dIn.y * normalSign, -dIn.x * normalSign);
                sf::Vector2f nOut(dOut.y * normalSign, -dOut.x * normalSign);

                sf::Vector2f pIn1 = pts[i] - nIn * m_sideThicknesses[i - 1];
                sf::Vector2f pIn2 = pts[i] - nOut * m_sideThicknesses[i];
                sf::Vector2f fallback((pIn1.x + pIn2.x) / 2.0f, (pIn1.y + pIn2.y) / 2.0f);
                inner[i] = getIntersection(pIn1, dIn, pIn2, dOut, fallback, pts[i]);
            }
        }
    }

    // 4. Заполняем геометрию обводки
    size_t segmentsCount = m_isClosed ? n : n - 1;
    m_strokeGeometry.resize(segmentsCount * 6);

    for (size_t i = 0; i < segmentsCount; i++) {
        size_t next = (i + 1) % n;

        m_strokeGeometry[i * 6 + 0].position = outer[i];
        m_strokeGeometry[i * 6 + 1].position = outer[next];
        m_strokeGeometry[i * 6 + 2].position = inner[next];

        m_strokeGeometry[i * 6 + 3].position = outer[i];
        m_strokeGeometry[i * 6 + 4].position = inner[next];
        m_strokeGeometry[i * 6 + 5].position = inner[i];

        for (int j = 0; j < 6; j++) {
            m_strokeGeometry[i * 6 + j].color = m_sideColors[i];
        }
    }
}

void PolygonShape::draw(sf::RenderTarget& target) const {
    if (m_isClosed) target.draw(m_fillGeometry);
    target.draw(m_strokeGeometry);
}

bool PolygonShape::contains(sf::Vector2f point) const {
    if (m_basePoints.empty()) return false;
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    std::vector<sf::Vector2f> pts(m_basePoints.size());
    for (size_t i = 0; i < m_basePoints.size(); i++) {
        sf::Vector2f unrotated(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        pts[i] = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);
    }

    if (!m_isClosed) {
        for (size_t i = 0; i < pts.size() - 1; i++) {
            sf::Vector2f a = pts[i];
            sf::Vector2f b = pts[i + 1];
            float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
            if (l2 == 0.0f) continue;
            float t = std::max(0.0f, std::min(1.0f, ((point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y)) / l2));
            sf::Vector2f proj = a + t * (b - a);
            float dist2 = (point.x - proj.x) * (point.x - proj.x) + (point.y - proj.y) * (point.y - proj.y);
            float thick = (m_sideThicknesses[i] / 2.0f) + 5.0f;
            if (dist2 <= thick * thick) return true;
        }
        return false;
    }

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
    ImGui::Text("Global Rotation:");
    float rot = m_rotation;
    ImGui::SetNextItemWidth(120);
    bool rotChanged = ImGui::DragFloat("##pRotDrag", &rot, 1.0f, -180.0f, 180.0f, "%.1f deg");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    rotChanged |= ImGui::InputFloat("Angle", &rot, 0.0f, 0.0f, "%.1f");
    if (rotChanged) setRotation(rot);

    // --- НОВОЕ: Кнопка "Сбросить (Запечь) поворот" ---
    ImGui::SameLine();
    if (ImGui::Button("Bake Rotation") && std::abs(m_rotation) > 0.01f) {
        size_t n = m_basePoints.size();
        sf::Vector2f anchorWorld = m_position + m_anchorOffset;
        std::vector<sf::Vector2f> worldPts(n);

        // 1. Вычисляем физические (мировые) координаты всех точек с учетом текущего поворота
        float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
        for (size_t i = 0; i < n; i++) {
            sf::Vector2f unrotated(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
            worldPts[i] = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);

            if (worldPts[i].x < minX) minX = worldPts[i].x;
            if (worldPts[i].x > maxX) maxX = worldPts[i].x;
            if (worldPts[i].y < minY) minY = worldPts[i].y;
            if (worldPts[i].y > maxY) maxY = worldPts[i].y;
        }

        // 2. Создаем новую виртуальную рамку (Bounding Box), которая охватывает повернутую фигуру
        m_position = { minX, minY };
        m_size = { std::max(5.0f, maxX - minX), std::max(5.0f, maxY - minY) };

        // 3. Переводим мировые координаты обратно во внутренние (от 0 до 1) для новой рамки
        for (size_t i = 0; i < n; i++) {
            m_basePoints[i].x = (worldPts[i].x - m_position.x) / m_size.x;
            m_basePoints[i].y = (worldPts[i].y - m_position.y) / m_size.y;
        }

        // 4. Обнуляем угол и корректируем якорь (чтобы он физически остался в той же точке экрана)
        m_rotation = 0.0f;
        m_anchorOffset = anchorWorld - m_position;

        updateGeometry();
    }
    ImGui::Separator();

    // --- ИДЕАЛЬНЫЙ МАСШТАБ ФИГУРЫ ---
    ImGui::Text("Bounding Box Scale (W/H):");
    sf::FloatRect bounds = getBounds();

    static bool isShapeUIScaling = false;
    static ShapeSnapshot uiShapeSnap;
    static sf::FloatRect uiShapeStartBounds;
    static float baseW = 1.0f, baseH = 1.0f;

    float scale[2] = { bounds.size.x, bounds.size.y };

    ImGui::SetNextItemWidth(150);
    bool changed1 = ImGui::SliderFloat2("##pScaleSl", scale, 10.0f, 1000.0f);
    bool act1 = ImGui::IsItemActivated(); bool deact1 = ImGui::IsItemDeactivated();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    bool changed2 = ImGui::InputFloat2("##pScaleInp", scale, "%.1f");
    bool act2 = ImGui::IsItemActivated(); bool deact2 = ImGui::IsItemDeactivated();

    ImGui::Text("Relative Scale (%%):");
    float diag = std::sqrt(bounds.size.x * bounds.size.x + bounds.size.y * bounds.size.y);
    float relScale = diag / 1.41421356f;

    ImGui::SetNextItemWidth(150);
    bool changed3 = ImGui::SliderFloat("##pRelSl", &relScale, 5.0f, 500.0f, "%.1f %%");
    bool act3 = ImGui::IsItemActivated(); bool deact3 = ImGui::IsItemDeactivated();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    bool changed4 = ImGui::InputFloat("##pRelInp", &relScale, 0.0f, 0.0f, "%.1f");
    bool act4 = ImGui::IsItemActivated(); bool deact4 = ImGui::IsItemDeactivated();

    bool anyAct = act1 || act2 || act3 || act4;
    bool anyDeact = deact1 || deact2 || deact3 || deact4;
    bool anyChanged = changed1 || changed2 || changed3 || changed4;

    // 1. Старт захвата ползунка
    if (anyAct) {
        isShapeUIScaling = true;
        uiShapeSnap = { m_position, m_size, m_anchorOffset, m_rotation, m_basePoints };
        uiShapeStartBounds = bounds;
        baseW = bounds.size.x;
        baseH = bounds.size.y;
    }

    // 2. Обработка изменений
    if (anyChanged) {
        // Фолбек: если клик не зарегистрировался (ввод с клавиатуры), делаем снимок прямо сейчас
        if (!isShapeUIScaling) {
            isShapeUIScaling = true;
            uiShapeSnap = { m_position, m_size, m_anchorOffset, m_rotation, m_basePoints };
            uiShapeStartBounds = bounds;
            baseW = bounds.size.x;
            baseH = bounds.size.y;
        }

        float targetW = scale[0];
        float targetH = scale[1];

        if (changed3 || changed4) {
            float targetDiag = relScale * 1.41421356f;
            float baseDiag = std::sqrt(baseW * baseW + baseH * baseH);
            float factor = targetDiag / baseDiag;
            targetW = baseW * factor;
            targetH = baseH * factor;
        }

        // Возвращаем фигуру к снимку
        m_position = uiShapeSnap.position;
        m_size = uiShapeSnap.size;
        m_anchorOffset = uiShapeSnap.anchorOffset;
        m_rotation = uiShapeSnap.rotation;
        m_basePoints = uiShapeSnap.basePoints;

        float Sx = targetW / baseW;
        float Sy = targetH / baseH;

        // Магия глобального масштаба теперь работает и из UI!
        sf::Vector2f fixedCorner = { uiShapeStartBounds.position.x, uiShapeStartBounds.position.y };
        applyGlobalScale(fixedCorner, Sx, Sy);
    }

    // 3. Отключаем режим СТРОГО после обработки изменений
    if (anyDeact) {
        isShapeUIScaling = false;
    }

    ImGui::Separator();

    ImGui::Text("Appearance");

    // --- НОВАЯ КНОПКА СЛУЧАЙНЫХ ПАРАМЕТРОВ (ДЛЯ ВСЕХ СТОРОН!) ---
    if (ImGui::Button("Randomize Colors & Thickness", ImVec2(-1, 0))) {
        m_fillColor = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 100 + std::rand() % 155);

        // Проходим циклом и назначаем КАЖДОЙ стороне свой случайный цвет
        for (auto& c : m_sideColors) {
            c = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 255);
        }
        // Проходим циклом и назначаем КАЖДОЙ стороне случайную толщину (от 2 до 30)
        for (auto& t : m_sideThicknesses) {
            t = 2.0f + static_cast<float>(std::rand() % 28);
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
    ImGui::Separator();

    // ЯКОРЬ
    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position:");
    float pos[2] = { worldAnchor.x, worldAnchor.y };
    ImGui::SetNextItemWidth(150);
    if (ImGui::DragFloat2("##pAnchorDrag", pos, 1.0f)) setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputFloat2("##pAnchorInp", pos, "%.1f")) setAnchorPositionWorld(sf::Vector2f(pos[0], pos[1]));
    ImGui::Separator();

    const size_t numPoints = m_basePoints.size();
    if (numPoints < 2) return;

    size_t segmentsCount = m_isClosed ? numPoints : numPoints - 1;

    // =========================================================
    // --- ПАНЕЛЬ УПРАВЛЕНИЯ ПАКЕТНЫМ ВВОДОМ (ГЕОМЕТРИЯ) ---
    // =========================================================
    if (g_batchShape != this) {
        if (ImGui::Button("Batch Edit Geometry (Lengths & Angles)", ImVec2(-1, 0))) {
            g_batchShape = this;
            g_batchAngles.clear(); g_batchAngles.resize(numPoints, 0.0f);
            g_batchLengths.clear(); g_batchLengths.resize(segmentsCount, 0.0f);

            // 1. Считываем текущие углы
            float signedArea = 0.0f;
            if (m_isClosed && numPoints >= 3) {
                for (size_t k = 0; k < numPoints; ++k) {
                    size_t nextK = (k + 1) % numPoints;
                    signedArea += (m_basePoints[k].x * m_basePoints[nextK].y - m_basePoints[nextK].x * m_basePoints[k].y);
                }
            }
            bool isCW = (signedArea > 0.0f);

            for (size_t i = 0; i < numPoints; i++) {
                bool canCalc = m_isClosed ? true : (i > 0 && i < numPoints - 1);
                if (canCalc) {
                    size_t prev = (i + numPoints - 1) % numPoints;
                    size_t next = (i + 1) % numPoints;
                    sf::Vector2f p0 = m_position + sf::Vector2f(m_basePoints[prev].x * m_size.x, m_basePoints[prev].y * m_size.y);
                    sf::Vector2f p1 = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
                    sf::Vector2f p2 = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);

                    float dot = (p0.x - p1.x) * (p2.x - p1.x) + (p0.y - p1.y) * (p2.y - p1.y);
                    float det = (p0.x - p1.x) * (p2.y - p1.y) - (p0.y - p1.y) * (p2.x - p1.x);
                    float angleDeg = std::atan2(det, dot) * 180.0f / 3.14159265f;

                    if (isCW) angleDeg = -angleDeg;
                    if (angleDeg < 0.0f) angleDeg += 360.0f;
                    g_batchAngles[i] = angleDeg;
                }
            }

            // 2. Считываем текущие длины сторон
            for (size_t i = 0; i < segmentsCount; i++) {
                size_t next = (i + 1) % numPoints;
                sf::Vector2f p1 = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
                sf::Vector2f p2 = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);
                float dx = p2.x - p1.x; float dy = p2.y - p1.y;
                g_batchLengths[i] = std::sqrt(dx * dx + dy * dy);
            }
        }
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        if (ImGui::Button("APPLY GEOMETRY", ImVec2(ImGui::GetContentRegionAvail().x / 2, 0))) {
            sf::Vector2f anchorWorld = m_position + m_anchorOffset;

            // Определяем направление обхода для правильного расчета
            float signedArea = 0.0f;
            if (m_isClosed && numPoints >= 3) {
                for (size_t k = 0; k < numPoints; ++k) {
                    size_t nextK = (k + 1) % numPoints;
                    signedArea += (m_basePoints[k].x * m_basePoints[nextK].y - m_basePoints[nextK].x * m_basePoints[k].y);
                }
            }
            bool isCW = (signedArea > 0.0f);

            // 1. Корректируем введенные углы (чтобы фигура математически смогла замкнуться)
            std::vector<float> correctedAngles = g_batchAngles;
            if (m_isClosed && numPoints > 2) {
                float sum = 0;
                for (float a : correctedAngles) sum += a;
                float target = (numPoints - 2) * 180.0f;
                float diff = target - sum;
                for (float& a : correctedAngles) a += diff / numPoints;
            }

            // 2. Достаем текущие точки и берем ДЛИНЫ из ввода пользователя
            std::vector<sf::Vector2f> pts(numPoints);
            for (size_t i = 0; i < numPoints; ++i) {
                pts[i] = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
            }

            std::vector<float> L = g_batchLengths;
            std::vector<sf::Vector2f> u(numPoints);
            for (size_t i = 0; i < segmentsCount; ++i) {
                size_t next = (i + 1) % numPoints;
                sf::Vector2f v = pts[next] - pts[i];
                float curL = std::sqrt(v.x * v.x + v.y * v.y);
                if (curL < 0.001f) curL = 1.0f;
                u[i] = v / curL;
            }

            // 3. Строим векторы (u) строго по введенным углам
            for (size_t i = 1; i < segmentsCount; ++i) {
                float targetAngDeg = correctedAngles[i];
                if (isCW) targetAngDeg = -targetAngDeg;
                float targetAngRad = targetAngDeg * 3.14159265f / 180.0f;

                sf::Vector2f v1 = -u[i - 1];
                float cosA = std::cos(targetAngRad);
                float sinA = std::sin(targetAngRad);
                u[i] = sf::Vector2f(v1.x * cosA - v1.y * sinA, v1.x * sinA + v1.y * cosA);
            }

            // 4. Подгоняем длины для замыкания (Только если есть разрыв!)
            if (m_isClosed && numPoints > 2) {
                for (int iter = 0; iter < 1000; ++iter) {
                    sf::Vector2f err(0, 0);
                    for (size_t i = 0; i < numPoints; ++i) err += u[i] * L[i];
                    if (std::abs(err.x) < 0.001f && std::abs(err.y) < 0.001f) break;
                    for (size_t i = 0; i < numPoints; ++i) {
                        L[i] -= (err.x * u[i].x + err.y * u[i].y) / numPoints;
                        if (L[i] < 5.0f) L[i] = 5.0f;
                    }
                }
            }

            // 5. Перестраиваем абсолютные точки, используя новые векторы и новые длины
            for (size_t i = 0; i < numPoints - 1; ++i) {
                pts[i + 1] = pts[i] + u[i] * L[i];
            }

            // 6. Пересчитываем локальный Bounding Box фигуры (размер мог кардинально измениться)
            float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
            for (const auto& p : pts) {
                if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
                if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
            }
            float newW = std::max(5.0f, maxX - minX);
            float newH = std::max(5.0f, maxY - minY);

            m_position.x = minX;
            m_position.y = minY;
            m_size.x = newW;
            m_size.y = newH;

            // Нормализуем вершины
            for (size_t i = 0; i < numPoints; ++i) {
                m_basePoints[i].x = (pts[i].x - m_position.x) / m_size.x;
                m_basePoints[i].y = (pts[i].y - m_position.y) / m_size.y;
            }
            updateGeometry();

            // 7. Сдвигаем центр новой виртуальной рамки обратно на Якорь
            sf::FloatRect newBounds = getBounds();
            sf::Vector2f boundsCenter(newBounds.position.x + newBounds.size.x / 2.0f, newBounds.position.y + newBounds.size.y / 2.0f);
            sf::Vector2f shift = anchorWorld - boundsCenter;
            m_position += shift;
            m_anchorOffset = anchorWorld - m_position;

            updateGeometry();
            g_batchShape = nullptr; // Выходим из режима
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("CANCEL", ImVec2(-1, 0))) g_batchShape = nullptr;
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    // =========================================================

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

            float stroke[4] = { m_sideColors[i].r / 255.f, m_sideColors[i].g / 255.f, m_sideColors[i].b / 255.f, m_sideColors[i].a / 255.f };
            ImGui::SetNextItemWidth(150);
            if (ImGui::ColorEdit4("##Col", stroke, ImGuiColorEditFlags_NoInputs)) {
                m_sideColors[i] = sf::Color(stroke[0] * 255, stroke[1] * 255, stroke[2] * 255, stroke[3] * 255);
                updateGeometry();
            }
            ImGui::SameLine();

            float thick = m_sideThicknesses[i];
            ImGui::SetNextItemWidth(80);
            if (ImGui::SliderFloat("##ThSl", &thick, 1.0f, 50.0f, "")) { m_sideThicknesses[i] = thick; updateGeometry(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(40);
            if (ImGui::InputFloat("Thick", &thick, 0.0f, 0.0f, "%.1f")) {
                m_sideThicknesses[i] = std::max(1.0f, std::min(100.0f, thick)); updateGeometry();
            }

            // --- БЛОК ВВОДА ДЛИНЫ СТОРОНЫ ---
            if (g_batchShape == this) {
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("##BatchLnDrag", &g_batchLengths[i], 1.0f, 5.0f, 5000.0f, "%.1f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                ImGui::InputFloat("Length", &g_batchLengths[i], 0.0f, 0.0f, "%.1f");
            }
            else {
                float editLen = length;
                ImGui::SetNextItemWidth(100);
                bool lenChanged = ImGui::SliderFloat("##LnSl", &editLen, 10.0f, 1000.0f, "");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                lenChanged |= ImGui::InputFloat("Length", &editLen, 0.0f, 0.0f, "%.1f");

                if (lenChanged) {
                    float newLen = std::max(1.0f, editLen);
                    if (length > 0.01f) {
                        sf::Vector2f dir = diff / length;
                        sf::Vector2f deltaMove = dir * (newLen - length);
                        if (!m_isClosed) {
                            for (size_t j = i + 1; j < numPoints; j++) {
                                sf::Vector2f pj = m_position + sf::Vector2f(m_basePoints[j].x * m_size.x, m_basePoints[j].y * m_size.y);
                                pj += deltaMove;
                                m_basePoints[j].x = (pj.x - m_position.x) / m_size.x;
                                m_basePoints[j].y = (pj.y - m_position.y) / m_size.y;
                            }
                        }
                        else {
                            size_t prevNode = (i + numPoints - 1) % numPoints;
                            for (size_t j = 0; j < numPoints; j++) {
                                if (j == i || j == prevNode) continue;
                                sf::Vector2f pj = m_position + sf::Vector2f(m_basePoints[j].x * m_size.x, m_basePoints[j].y * m_size.y);
                                pj += deltaMove;
                                m_basePoints[j].x = (pj.x - m_position.x) / m_size.x;
                                m_basePoints[j].y = (pj.y - m_position.y) / m_size.y;
                            }
                        }
                        updateGeometry();
                    }
                }
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Vertices Coordinates & Angles", ImGuiTreeNodeFlags_DefaultOpen)) {
        float signedArea = 0.0f;
        if (m_isClosed && numPoints >= 3) {
            for (size_t k = 0; k < numPoints; ++k) {
                size_t nextK = (k + 1) % numPoints;
                signedArea += (m_basePoints[k].x * m_basePoints[nextK].y - m_basePoints[nextK].x * m_basePoints[k].y);
            }
        }
        bool isCW = (signedArea > 0.0f);

        if (m_selectedVertex != -1) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Editing Selected Node %d", m_selectedVertex + 1);
            if (ImGui::Button("Deselect Node", ImVec2(-1, 0))) m_selectedVertex = -1;
            ImGui::Separator();
        }
        size_t startV = (m_selectedVertex != -1) ? m_selectedVertex : 0;
        size_t endV = (m_selectedVertex != -1) ? m_selectedVertex + 1 : numPoints;

        for (size_t i = startV; i < endV; i++) {
            ImGui::PushID(static_cast<int>(i + 1000));
            if (m_selectedVertex == -1) ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Node %zu", i + 1);

            sf::Vector2f unrotated = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
            sf::Vector2f rotated = BaseShape::rotatePoint(unrotated, worldAnchor, m_rotation);
            sf::Vector2f relNode = rotated - worldAnchor;

            float relData[2] = { relNode.x, relNode.y };
            ImGui::SetNextItemWidth(150);
            if (ImGui::DragFloat2("Coord (Rel)", relData, 1.0f)) {
                sf::Vector2f newWorldNode = worldAnchor + sf::Vector2f(relData[0], relData[1]);
                sf::Vector2f newUnrotated = BaseShape::rotatePoint(newWorldNode, worldAnchor, -m_rotation);
                m_basePoints[i].x = (newUnrotated.x - m_position.x) / m_size.x;
                m_basePoints[i].y = (newUnrotated.y - m_position.y) / m_size.y;
                updateGeometry();
            }

            bool canCalcAngle = m_isClosed ? true : (i > 0 && i < numPoints - 1);
            if (canCalcAngle) {
                // --- БЛОК ВВОДА УГЛОВ ---
                if (g_batchShape == this) {
                    ImGui::SetNextItemWidth(120);
                    ImGui::DragFloat("##BatchAngDrag", &g_batchAngles[i], 1.0f, 0.0f, 360.0f, "%.1f deg");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(80);
                    ImGui::InputFloat("Angle", &g_batchAngles[i], 0.0f, 0.0f, "%.1f");
                }
                else {
                    size_t prev = (i + numPoints - 1) % numPoints;
                    size_t next = (i + 1) % numPoints;

                    sf::Vector2f p0 = m_position + sf::Vector2f(m_basePoints[prev].x * m_size.x, m_basePoints[prev].y * m_size.y);
                    sf::Vector2f p1 = m_position + sf::Vector2f(m_basePoints[i].x * m_size.x, m_basePoints[i].y * m_size.y);
                    sf::Vector2f p2 = m_position + sf::Vector2f(m_basePoints[next].x * m_size.x, m_basePoints[next].y * m_size.y);

                    float dot = (p0.x - p1.x) * (p2.x - p1.x) + (p0.y - p1.y) * (p2.y - p1.y);
                    float det = (p0.x - p1.x) * (p2.y - p1.y) - (p0.y - p1.y) * (p2.x - p1.x);

                    float angleRad = std::atan2(det, dot);
                    float angleDeg = angleRad * 180.0f / 3.14159265f;

                    if (isCW) angleDeg = -angleDeg;
                    if (angleDeg < 0.0f) angleDeg += 360.0f;

                    float editAng = angleDeg;
                    ImGui::SetNextItemWidth(120);
                    bool angleChanged = ImGui::DragFloat("##IntAngDrag", &editAng, 1.0f, 0.0f, 360.0f, "%.1f deg");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(80);
                    angleChanged |= ImGui::InputFloat("Angle", &editAng, 0.0f, 0.0f, "%.1f");

                    if (angleChanged) {
                        float targetAngDeg = editAng;
                        if (isCW) targetAngDeg = -targetAngDeg;
                        float targetAngRad = targetAngDeg * 3.14159265f / 180.0f;
                        float currentAngRad = std::atan2(det, dot);

                        float deltaRad = targetAngRad - currentAngRad;
                        float cosD = std::cos(deltaRad);
                        float sinD = std::sin(deltaRad);

                        if (!m_isClosed) {
                            for (size_t j = i + 1; j < numPoints; j++) {
                                sf::Vector2f pj = m_position + sf::Vector2f(m_basePoints[j].x * m_size.x, m_basePoints[j].y * m_size.y);
                                float dx = pj.x - p1.x; float dy = pj.y - p1.y;
                                sf::Vector2f newPj(p1.x + dx * cosD - dy * sinD, p1.y + dx * sinD + dy * cosD);
                                m_basePoints[j].x = (newPj.x - m_position.x) / m_size.x;
                                m_basePoints[j].y = (newPj.y - m_position.y) / m_size.y;
                            }
                        }
                        else {
                            for (size_t j = 0; j < numPoints; j++) {
                                if (j == i || j == prev) continue;
                                sf::Vector2f pj = m_position + sf::Vector2f(m_basePoints[j].x * m_size.x, m_basePoints[j].y * m_size.y);
                                float dx = pj.x - p1.x; float dy = pj.y - p1.y;
                                sf::Vector2f newPj(p1.x + dx * cosD - dy * sinD, p1.y + dx * sinD + dy * cosD);
                                m_basePoints[j].x = (newPj.x - m_position.x) / m_size.x;
                                m_basePoints[j].y = (newPj.y - m_position.y) / m_size.y;
                            }
                        }
                        updateGeometry();
                    }
                }
            }
            else {
                ImGui::TextDisabled("Angle: N/A (Edge Node)");
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
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
    for (size_t i = 0; i < m_basePoints.size(); i++) {
        sf::Vector2f unrotated(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f rotated = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);
        if (rotated.x < minX) minX = rotated.x; if (rotated.x > maxX) maxX = rotated.x;
        if (rotated.y < minY) minY = rotated.y; if (rotated.y > maxY) maxY = rotated.y;
    }
    return sf::FloatRect({ minX, minY }, { maxX - minX, maxY - minY });
}

void PolygonShape::drawSelection(sf::RenderTarget& target) const {
    if (!m_isSelected) return;
    BaseShape::drawSelection(target); // рисует синюю рамку и якорь

    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    if (m_selectedSide != -1 && m_selectedSide < (m_isClosed ? m_basePoints.size() : m_basePoints.size() - 1)) {
        size_t next = (m_selectedSide + 1) % m_basePoints.size();

        sf::Vector2f un1(m_position.x + m_basePoints[m_selectedSide].x * m_size.x, m_position.y + m_basePoints[m_selectedSide].y * m_size.y);
        sf::Vector2f un2(m_position.x + m_basePoints[next].x * m_size.x, m_position.y + m_basePoints[next].y * m_size.y);
        sf::Vector2f p1 = BaseShape::rotatePoint(un1, anchorWorld, m_rotation);
        sf::Vector2f p2 = BaseShape::rotatePoint(un2, anchorWorld, m_rotation);

        sf::Vector2f diff = p2 - p1;
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        float angle = std::atan2(diff.y, diff.x);

        float fixedThick = 6.0f;
        sf::RectangleShape hlRect({ len, fixedThick });
        hlRect.setOrigin({ 0, fixedThick / 2.0f });
        hlRect.setPosition(p1);
        hlRect.setRotation(sf::radians(angle));
        hlRect.setFillColor(sf::Color(255, 255, 0, 180));
        target.draw(hlRect);
    }

    for (size_t i = 0; i < m_basePoints.size(); i++) {
        sf::Vector2f unp(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f p = BaseShape::rotatePoint(unp, anchorWorld, m_rotation);
        sf::CircleShape vHandle(5.0f);
        vHandle.setOrigin({ 5.0f, 5.0f });
        vHandle.setPosition(p);
        if (static_cast<int>(i) == m_selectedVertex) {
            vHandle.setFillColor(sf::Color::Green);
            vHandle.setOutlineColor(sf::Color::White);
            vHandle.setRadius(6.0f); // Можно сделать чуть больше для акцента
            vHandle.setOrigin({ 6.0f, 6.0f });
        }
        else {
            vHandle.setFillColor(sf::Color::Yellow);
            vHandle.setOutlineColor(sf::Color::Black);
            vHandle.setRadius(5.0f);
            vHandle.setOrigin({ 5.0f, 5.0f });
        }
        vHandle.setOutlineThickness(1.0f);
        target.draw(vHandle);
    }
}
int PolygonShape::getHitHandle(sf::Vector2f mousePos) const {
    if (!m_isSelected) return 0;
    float hw = 6.0f;
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    for (size_t i = 0; i < m_basePoints.size(); i++) {
        sf::Vector2f unp(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f p = BaseShape::rotatePoint(unp, anchorWorld, m_rotation);
        if (mousePos.x >= p.x - hw && mousePos.x <= p.x + hw && mousePos.y >= p.y - hw && mousePos.y <= p.y + hw) {
            return 100 + static_cast<int>(i);
        }
    }
    return BaseShape::getHitHandle(mousePos);
}

void PolygonShape::moveVertex(int index, sf::Vector2f delta) {
    if (index >= 0 && index < m_basePoints.size()) {
        sf::Vector2f anchorWorld = m_position + m_anchorOffset;
        sf::Vector2f unrotated = m_position + sf::Vector2f(m_basePoints[index].x * m_size.x, m_basePoints[index].y * m_size.y);

        // 1. Узнаем, где сейчас узел в мире
        sf::Vector2f currentWorldPos = BaseShape::rotatePoint(unrotated, anchorWorld, m_rotation);

        // 2. Двигаем
        currentWorldPos += delta;

        // 3. Крутим обратно, чтобы сохранить в "прямую" математику
        sf::Vector2f newUnrotated = BaseShape::rotatePoint(currentWorldPos, anchorWorld, -m_rotation);

        m_basePoints[index].x = (newUnrotated.x - m_position.x) / m_size.x;
        m_basePoints[index].y = (newUnrotated.y - m_position.y) / m_size.y;
        updateGeometry();
    }
}
int PolygonShape::getHitSideIndex(sf::Vector2f mousePos) const {
    if (!m_isSelected) return -1;
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;
    size_t segmentsCount = m_isClosed ? m_basePoints.size() : m_basePoints.size() - 1;

    for (size_t i = 0; i < segmentsCount; i++) {
        size_t next = (i + 1) % m_basePoints.size();
        sf::Vector2f unA(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f unB(m_position.x + m_basePoints[next].x * m_size.x, m_position.y + m_basePoints[next].y * m_size.y);

        sf::Vector2f a = BaseShape::rotatePoint(unA, anchorWorld, m_rotation);
        sf::Vector2f b = BaseShape::rotatePoint(unB, anchorWorld, m_rotation);

        float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
        if (l2 == 0.0f) continue;

        float t = std::max(0.0f, std::min(1.0f, ((mousePos.x - a.x) * (b.x - a.x) + (mousePos.y - a.y) * (b.y - a.y)) / l2));
        sf::Vector2f proj = a + t * (b - a);
        float dist2 = (mousePos.x - proj.x) * (mousePos.x - proj.x) + (mousePos.y - proj.y) * (mousePos.y - proj.y);
        float thick = (m_sideThicknesses[i] / 2.0f) + 5.0f;

        if (dist2 <= thick * thick) return i;
    }
    return -1;
}

void PolygonShape::save(std::ostream& out) const {
    BaseShape::save(out);
    out << "PointsCount: " << m_basePoints.size() << "\n";
    for (const auto& p : m_basePoints) {
        out << "  " << p.x << " " << p.y << "\n";
    }
}
void PolygonShape::load(std::istream& in) {
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

sf::Vector2f PolygonShape::getIntersection(sf::Vector2f p1, sf::Vector2f dir1, sf::Vector2f p2, sf::Vector2f dir2, sf::Vector2f fallback, sf::Vector2f corner) const {
    float cross = dir1.x * dir2.y - dir1.y * dir2.x;
    if (std::abs(cross) < 0.001f) return fallback;
    sf::Vector2f diff = p2 - p1;
    float t = (diff.x * dir2.y - diff.y * dir2.x) / cross;
    sf::Vector2f intersection = p1 + dir1 * t;
    sf::Vector2f offset = intersection - corner;
    float offsetLen = std::sqrt(offset.x * offset.x + offset.y * offset.y);
    if (offsetLen > 200.0f) intersection = corner + normalize(offset) * 200.0f;
    return intersection;
}

void PolygonShape::applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) {
    size_t n = m_basePoints.size();
    std::vector<sf::Vector2f> worldPts(n);
    sf::Vector2f anchorWorld = m_position + m_anchorOffset;

    // Вычисляем новую позицию якоря (он тоже масштабируется глобально)
    sf::Vector2f newAnchorWorld(
        fixedCorner.x + (anchorWorld.x - fixedCorner.x) * Sx,
        fixedCorner.y + (anchorWorld.y - fixedCorner.y) * Sy
    );

    float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;

    for (size_t i = 0; i < n; i++) {
        // 1. Получаем мировые координаты точки
        sf::Vector2f unrot(m_position.x + m_basePoints[i].x * m_size.x, m_position.y + m_basePoints[i].y * m_size.y);
        sf::Vector2f wp = BaseShape::rotatePoint(unrot, anchorWorld, m_rotation);

        // 2. Применяем глобальный масштаб (точки идеально следуют за рамкой)
        wp.x = fixedCorner.x + (wp.x - fixedCorner.x) * Sx;
        wp.y = fixedCorner.y + (wp.y - fixedCorner.y) * Sy;

        // 3. "Запекаем" точку обратно, вращая её в обратную сторону вокруг НОВОГО якоря
        wp = BaseShape::rotatePoint(wp, newAnchorWorld, -m_rotation);
        worldPts[i] = wp;

        if (wp.x < minX) minX = wp.x;
        if (wp.x > maxX) maxX = wp.x;
        if (wp.y < minY) minY = wp.y;
        if (wp.y > maxY) maxY = wp.y;
    }

    // Пересчитываем локальные параметры фигуры
    m_position = { minX, minY };
    m_size = { std::max(5.0f, maxX - minX), std::max(5.0f, maxY - minY) };

    for (size_t i = 0; i < n; i++) {
        m_basePoints[i].x = (worldPts[i].x - m_position.x) / m_size.x;
        m_basePoints[i].y = (worldPts[i].y - m_position.y) / m_size.y;
    }

    m_anchorOffset = newAnchorWorld - m_position;
    updateGeometry();
}