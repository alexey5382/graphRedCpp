#include "Scene.h"
#include <algorithm>
#include "PolygonShape.h"
#include "EllipseShape.h"
#include <fstream>
#include "imgui.h"
#include <map> // Добавь в самое начало Scene.cpp
#include <sstream>
#include "Group.h"

Scene::Scene() {
    // Начальный размер камеры (будет переопределен при старте окна)
    m_view.setCenter({ 600.0f, 400.0f });
    m_view.setSize({ 1200.0f, 800.0f });
}
// ==========================================
// ФАБРИКА И РЕКУРСИВНЫЙ ПАРСИНГ ФИГУР
// ==========================================
std::unique_ptr<IShape> parseShapeRecursive(std::istream& in, int& nextId, bool merge) {
    std::string typeStr;
    if (!(in >> typeStr)) return nullptr;

    std::unique_ptr<IShape> shape;
    if (typeStr == "[Polygon]") shape = std::make_unique<PolygonShape>(sf::Vector2f(0, 0), sf::Vector2f(10, 10), true, std::vector<sf::Vector2f>{});
    else if (typeStr == "[Circle]" || typeStr == "[Ellipse]") shape = std::make_unique<EllipseShape>(sf::Vector2f(0, 0), 10.0f, 10.0f);    else if (typeStr == "[Group]") shape = std::make_unique<Group>(sf::Vector2f(0, 0));

    if (!shape) return nullptr;

    shape->load(in); // Загружаем базовые свойства (имя, позицию, цвета)

    // Обработка ID
    if (merge) shape->setId(nextId++);
    else if (shape->getId() >= nextId) nextId = shape->getId() + 1;

    // Магия рекурсии для группы
    if (typeStr == "[Group]") {
        std::string dummy;
        size_t childCount;
        in >> dummy >> childCount; // Считываем то самое "ChildrenCount: N"

        Group* grp = dynamic_cast<Group*>(shape.get());
        for (size_t i = 0; i < childCount; i++) {
            auto child = parseShapeRecursive(in, nextId, merge);
            if (child) grp->addChild(std::move(child));
        }
    }
    return shape;
}
sf::Vector2f Scene::getScreenToWorld(sf::Vector2i pixelPos, const sf::RenderTarget& target) const {
    return target.mapPixelToCoords(pixelPos, m_view);
}

void Scene::updateViewSize(sf::Vector2f newSize) {
    m_view.setSize(newSize * m_zoom); // Сохраняем зум при ресайзе окна
}

void Scene::handlePanStart(sf::Vector2i pixelPos) {
    m_isPanning = true;
    m_panStartMousePos = pixelPos;
}

void Scene::handlePanMove(sf::Vector2i pixelPos, const sf::RenderTarget& target) {
    if (!m_isPanning) return;
    // Находим разницу в координатах МИРА между прошлым и текущим кадром мыши
    sf::Vector2f prevWorld = target.mapPixelToCoords(m_panStartMousePos, m_view);
    sf::Vector2f currentWorld = target.mapPixelToCoords(pixelPos, m_view);

    // Сдвигаем камеру
    m_view.move(prevWorld - currentWorld);
    m_panStartMousePos = pixelPos;
}

void Scene::handlePanEnd() { m_isPanning = false; }

void Scene::handleZoom(float delta, sf::Vector2i pixelPos, const sf::RenderTarget& target) {
    sf::Vector2f beforeCoords = target.mapPixelToCoords(pixelPos, m_view);

    if (delta > 0) m_zoom *= 0.9f; // Приближение
    else if (delta < 0) m_zoom *= 1.1f; // Отдаление

    m_zoom = std::clamp(m_zoom, 0.05f, 10.0f); // Ограничитель

    sf::Vector2f windowSize(static_cast<float>(target.getSize().x), static_cast<float>(target.getSize().y));
    m_view.setSize(windowSize * m_zoom);

    // Сдвигаем камеру так, чтобы зум происходил строго в точку под курсором
    sf::Vector2f afterCoords = target.mapPixelToCoords(pixelPos, m_view);
    m_view.move(beforeCoords - afterCoords);
}
void Scene::setZoom(float newZoom, sf::Vector2f windowSize) {
    // Ограничиваем масштаб (от 5% до 1000%)
    m_zoom = std::clamp(newZoom, 0.05f, 10.0f);
    m_view.setSize(windowSize * m_zoom);
}

void Scene::resetView(sf::Vector2f windowSize) {
    m_zoom = 1.0f;
    m_view.setSize(windowSize);
    m_view.setCenter(windowSize / 2.0f);
}
void Scene::initCursors() {
    // В SFML 3 используем createFromSystem и строгие перечисления (Type::)
    m_cursorArrow = sf::Cursor::createFromSystem(sf::Cursor::Type::Arrow);
    m_cursorCross = sf::Cursor::createFromSystem(sf::Cursor::Type::Cross);
    m_cursorHand = sf::Cursor::createFromSystem(sf::Cursor::Type::Hand);
    m_cursorSizeTLBR = sf::Cursor::createFromSystem(sf::Cursor::Type::SizeTopLeftBottomRight);
    m_cursorSizeBLTR = sf::Cursor::createFromSystem(sf::Cursor::Type::SizeBottomLeftTopRight);
    m_cursorSizeAll = sf::Cursor::createFromSystem(sf::Cursor::Type::SizeAll);

    m_cursorsLoaded = true;
}

void Scene::updateCursor(sf::RenderWindow& window, sf::Vector2f mousePos) {
    if (!m_cursorsLoaded) return;

    // 1. Курсор для режима рисования
    if (m_isDrawingMode) {
        if (m_cursorCross) window.setMouseCursor(*m_cursorCross);
        return;
    }

    int activeHandle = 0;
    bool hoverSide = false;
    bool hoverShape = false;

    // 2. Определяем, над чем сейчас находится мышь (или что мы уже тащим)
    if (m_isDragging) {
        activeHandle = m_draggedHandle;
    }
    else {
        // Проверяем маркеры выделенных фигур (и Групп тоже!)
        for (auto* shape : m_selectedShapes) {
            activeHandle = shape->getHitHandle(mousePos);
            if (activeHandle != 0) break;

            // Проверка наведения на сторону (работает для одиночных фигур)
            if (m_selectedShapes.size() == 1 && shape->getHitSideIndex(mousePos) != -1) {
                hoverSide = true;
                break;
            }
        }

        // Если не попали ни в маркер, ни в сторону, проверяем наведение на тело фигуры
        if (activeHandle == 0 && !hoverSide) {
            for (auto* shape : m_selectedShapes) {
                if (shape->contains(mousePos)) {
                    hoverShape = true;
                    break;
                }
            }
        }
    }

    // 3. Выбираем нужный курсор на основе активного элемента
    if (activeHandle == 1 || activeHandle == 3 || activeHandle == 11 || activeHandle == 13) {
        if (m_cursorSizeTLBR) window.setMouseCursor(*m_cursorSizeTLBR);
    }
    else if (activeHandle == 2 || activeHandle == 4 || activeHandle == 12 || activeHandle == 14) {
        if (m_cursorSizeBLTR) window.setMouseCursor(*m_cursorSizeBLTR);
    }
    else if (activeHandle == 5 || hoverShape) {
        // Якорь (5) или перетаскивание тела фигуры
        if (m_cursorSizeAll) window.setMouseCursor(*m_cursorSizeAll);
    }
    else if (activeHandle == 7 || activeHandle >= 100 || hoverSide) {
        // Вращение (7), перетаскивание узлов (100+) или сторон
        if (m_cursorHand) window.setMouseCursor(*m_cursorHand);
    }
    else {
        // Обычная мышка в пустом месте
        if (m_cursorArrow) window.setMouseCursor(*m_cursorArrow);
    }
}
// 
void Scene::resetCursor(sf::RenderWindow& window) {
    if (m_cursorArrow) window.setMouseCursor(*m_cursorArrow);
}

// 
void Scene::addShape(std::unique_ptr<IShape> shape) {
    shape->setId(m_nextShapeId++);
    if (shape->getName().empty()) {
        shape->setName(shape->getType() + " " + std::to_string(shape->getId()));
    }
    m_shapes.push_back(std::move(shape));
}

void Scene::draw(sf::RenderTarget& target) const {
    // --- ПРИМЕНЯЕМ КАМЕРУ ---
    target.setView(m_view);

    // 0. Отрисовка бесконечной сетки (сзади всех фигур)
    if (m_showGrid) {
        sf::Vector2f center = m_view.getCenter();
        sf::Vector2f size = m_view.getSize();
        float left = center.x - size.x / 2.0f;
        float right = center.x + size.x / 2.0f;
        float top = center.y - size.y / 2.0f;
        float bottom = center.y + size.y / 2.0f;

        // Отступы для плавного появления
        left -= m_gridSize; right += m_gridSize;
        top -= m_gridSize; bottom += m_gridSize;

        int startX = static_cast<int>(std::floor(left / m_gridSize));
        int endX = static_cast<int>(std::ceil(right / m_gridSize));
        int startY = static_cast<int>(std::floor(top / m_gridSize));
        int endY = static_cast<int>(std::ceil(bottom / m_gridSize));

        std::vector<sf::Vertex> gridVertices;
        // Вертикальные линии
        for (int x = startX; x <= endX; ++x) {
            float px = x * m_gridSize;
            sf::Color color = (x == 0) ? sf::Color(255, 100, 100, 150) : sf::Color(100, 100, 100, 30); // Красная ось Y
            gridVertices.push_back(sf::Vertex({ px, top }, color));
            gridVertices.push_back(sf::Vertex({ px, bottom }, color));
        }
        // Горизонтальные линии
        for (int y = startY; y <= endY; ++y) {
            float py = y * m_gridSize;
            sf::Color color = (y == 0) ? sf::Color(100, 255, 100, 150) : sf::Color(100, 100, 100, 30); // Зеленая ось X
            gridVertices.push_back(sf::Vertex({ left, py }, color));
            gridVertices.push_back(sf::Vertex({ right, py }, color));
        }
        if (!gridVertices.empty()) {
            target.draw(gridVertices.data(), gridVertices.size(), sf::PrimitiveType::Lines);
        }
    }

    // 1. Рисуем фигуры и группы
    for (const auto& shape : m_shapes) {
        shape->draw(target);
    }

    // 2. Отрисовка рамки выделения для всех выделенных объектов (включая группы!)
    for (auto* shape : m_selectedShapes) {
        shape->drawSelection(target);
    }

    // 3. Рамка массового выделения мышью (Drag Selection)
    if (m_isBoxSelecting) {
        float minX = std::min(m_selectionStartPos.x, m_lastMousePos.x);
        float maxX = std::max(m_selectionStartPos.x, m_lastMousePos.x);
        float minY = std::min(m_selectionStartPos.y, m_lastMousePos.y);
        float maxY = std::max(m_selectionStartPos.y, m_lastMousePos.y);

        sf::RectangleShape box({ maxX - minX, maxY - minY });
        box.setPosition({ minX, minY });
        box.setFillColor(sf::Color(30, 144, 255, 50));
        box.setOutlineColor(sf::Color(30, 144, 255));
        box.setOutlineThickness(1.0f);
        target.draw(box);
    }

    // 4. Режим интерактивного рисования линий/полигонов
    if (m_isDrawingMode && !m_drawingPoints.empty()) {
        sf::Vector2f previewPos = m_currentMousePos;

        // Визуальный предпросмотр привязки по Shift
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift)) {
            sf::Vector2f lastPt = m_drawingPoints.back();
            sf::Vector2f diff = previewPos - lastPt;
            float angle = std::atan2(diff.y, diff.x);
            float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            const float snapRad = 15.0f * 3.14159265f / 180.0f; // Шаг 15 градусов
            angle = std::round(angle / snapRad) * snapRad;
            previewPos = lastPt + sf::Vector2f(std::cos(angle) * len, std::sin(angle) * len);
        }

        // Рисуем саму линию
        sf::VertexArray lines(sf::PrimitiveType::LineStrip, m_drawingPoints.size() + 1);
        for (size_t i = 0; i < m_drawingPoints.size(); i++) {
            lines[i].position = m_drawingPoints[i];
            lines[i].color = sf::Color::Black;
        }
        lines[m_drawingPoints.size()].position = previewPos; // Тянем к точке предпросмотра
        lines[m_drawingPoints.size()].color = sf::Color(0, 0, 0, 255);
        target.draw(lines);

        // Оверлей длины и угла возле курсора
        sf::Vector2f lastPt = m_drawingPoints.back();
        sf::Vector2f diff = previewPos - lastPt;

        float length = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        float angleDeg = std::atan2(diff.y, diff.x) * 180.0f / 3.14159265f;

        if (angleDeg < 0.0f) angleDeg += 360.0f;

        sf::Vector2i screenPos = target.mapCoordsToPixel(previewPos, m_view);

        char overlayText[64];
        snprintf(overlayText, sizeof(overlayText), "L: %.1f px\nA: %.1f deg", length, 360 - angleDeg);

        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(static_cast<float>(screenPos.x + 15), static_cast<float>(screenPos.y + 15)),
            IM_COL32(0,0, 0, 255),
            overlayText
        );
    }

    // --- ВАЖНО: ВОЗВРАЩАЕМ ВИД ОКНА ---
    target.setView(target.getDefaultView());
}

void Scene::clear() {
    m_shapes.clear();
    m_selectedShapes.clear();
}

void Scene::clearSelection() {
    for (auto* shape : m_selectedShapes) shape->setSelected(false);
    m_selectedShapes.clear();
}
void Scene::handleMouseRelease(bool isShiftPressed) {
    // Логика рамки массового выделения мышью (Box Selection)
    if (m_isBoxSelecting) {
        float minX = std::min(m_selectionStartPos.x, m_lastMousePos.x);
        float maxX = std::max(m_selectionStartPos.x, m_lastMousePos.x);
        float minY = std::min(m_selectionStartPos.y, m_lastMousePos.y);
        float maxY = std::max(m_selectionStartPos.y, m_lastMousePos.y);

        sf::FloatRect selBox({ minX, minY }, { maxX - minX, maxY - minY });

        if (selBox.size.x > 5.f && selBox.size.y > 5.f) {
            if (!isShiftPressed) clearSelection();

            for (auto& shapePtr : m_shapes) {
                sf::FloatRect shapeBox = shapePtr->getBounds();
                // Фигура (или ГРУППА) должна ПОЛНОСТЬЮ попасть в рамку
                if (selBox.position.x <= shapeBox.position.x &&
                    selBox.position.y <= shapeBox.position.y &&
                    selBox.position.x + selBox.size.x >= shapeBox.position.x + shapeBox.size.x &&
                    selBox.position.y + selBox.size.y >= shapeBox.position.y + shapeBox.size.y)
                {
                    if (std::find(m_selectedShapes.begin(), m_selectedShapes.end(), shapePtr.get()) == m_selectedShapes.end()) {
                        shapePtr->setSelected(true);
                        m_selectedShapes.push_back(shapePtr.get());
                    }
                }
            }
        }
        m_isBoxSelecting = false;
    }

    m_isDragging = false;
    m_draggedHandle = 0;
}
void Scene::handleMousePress(sf::Vector2f mousePos, sf::Vector2i pixelPos, bool isShiftPressed, bool isCtrlPressed) {
        // =========================================================================
        // 0. РЕЖИМ РИСОВАНИЯ ЛОМАНОЙ/МНОГОУГОЛЬНИКА
        // =========================================================================
    if (m_isDrawingMode) {
        // Привязка угла к 15 градусам при зажатом Shift
        if (isShiftPressed && !m_drawingPoints.empty()) {
            sf::Vector2f lastPt = m_drawingPoints.back();
            sf::Vector2f diff = mousePos - lastPt;
            float angle = std::atan2(diff.y, diff.x);
            float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            const float snapRad = 15.0f * 3.14159265f / 180.0f; // Шаг 15 градусов
            angle = std::round(angle / snapRad) * snapRad;
            mousePos = lastPt + sf::Vector2f(std::cos(angle) * len, std::sin(angle) * len);
        }

        // Ставим новую точку на холст
        m_drawingPoints.push_back(mousePos);
        return; // Выходим, чтобы клик не пытался выделить другие фигуры
    }

    // 1. Проверяем маркеры выделенных фигур
    for (auto* shape : m_selectedShapes) {
        int handle = shape->getHitHandle(mousePos);
        if (handle != 0) {
            m_draggedHandle = handle;
            if (handle >= 100) { shape->setSelectedVertex(handle - 100); shape->setSelectedSide(-1); }
            else { shape->setSelectedVertex(-1); }

            m_isDragging = true;
            m_lastMousePos = mousePos;

            // --- СЕКРЕТ ИДЕАЛЬНОГО МАСШТАБА ЗДЕСЬ ---
            shape->captureState(); // Фигура (или Группа) сама делает снимок себя и всех своих детей!

            if (handle >= 1 && handle <= 4) {
                m_dragStartGroupBounds = shape->getBounds();
                sf::Vector2f perfectCorner;
                if (handle == 1) perfectCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y };
                else if (handle == 2) perfectCorner = { m_dragStartGroupBounds.position.x + m_dragStartGroupBounds.size.x, m_dragStartGroupBounds.position.y };
                else if (handle == 3) perfectCorner = { m_dragStartGroupBounds.position.x + m_dragStartGroupBounds.size.x, m_dragStartGroupBounds.position.y + m_dragStartGroupBounds.size.y };
                else if (handle == 4) perfectCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y + m_dragStartGroupBounds.size.y };
                m_scaleMouseOffset = mousePos - perfectCorner;
            }
            else if (handle >= 11 && handle <= 14) {
                EllipseShape* ellipse = dynamic_cast<EllipseShape*>(shape);
                if (ellipse) {
                    m_scaleMouseOffset = mousePos - ellipse->getOBBCorner(handle);
                }
            }
            else {
                m_scaleMouseOffset = { 0.0f, 0.0f };
            }
            return;
        }
    }

    // 2. КЛИК ПО ТЕЛУ ФИГУРЫ (ИЛИ ГРУППЫ)
    IShape* clickedShape = nullptr;
    for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
        if ((*it)->contains(mousePos)) {
            if (isCtrlPressed) {
                // НОВАЯ ФИЧА: Проваливаемся внутрь группы!
                clickedShape = (*it)->getHitShape(mousePos);
            }
            else {
                clickedShape = it->get();
            }
            break;
        }
    }

    if (clickedShape) {
        bool sideClickHandled = false;
        auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clickedShape);

        if (it != m_selectedShapes.end() && !isCtrlPressed && !isShiftPressed && m_selectedShapes.size() == 1) {
            int hitSide = clickedShape->getHitSideIndex(mousePos);
            if (hitSide != -1) { clickedShape->setSelectedSide(hitSide); sideClickHandled = true; }
        }
        if (!sideClickHandled) for (auto* s : m_selectedShapes) s->setSelectedSide(-1);

        if (isShiftPressed) {
            if (it != m_selectedShapes.end()) { clickedShape->setSelected(false); m_selectedShapes.erase(it); }
            else { clickedShape->setSelected(true); m_selectedShapes.push_back(clickedShape); }
        }
        else {
            if (it == m_selectedShapes.end()) {
                clearSelection();
                clickedShape->setSelected(true);
                m_selectedShapes.push_back(clickedShape);
            }
        }

        m_isDragging = true;
        m_draggedHandle = 0;
        m_lastMousePos = mousePos;

        // ОБЯЗАТЕЛЬНО сохраняем состояние для перетаскивания (для будущей системы Ctrl+Z)
        for (auto* s : m_selectedShapes) s->captureState();
    }
    else {
        // 3. КЛИК В ПУСТОЕ МЕСТО ХОЛСТА
        if (isShiftPressed) handlePanStart(pixelPos);
        else {
            if (!isCtrlPressed) clearSelection();
            m_isBoxSelecting = true;
            m_selectionStartPos = mousePos;
            m_lastMousePos = mousePos;
        }
    }
}
void Scene::handleMouseMove(sf::Vector2f mousePos) {
    if (m_isDrawingMode) {
        m_currentMousePos = mousePos;
        return;
    }

    if (m_isBoxSelecting) {
        m_lastMousePos = mousePos; // Обновляем конец рамки
        return;
    }

    if (!m_isDragging) return;

    // =========================================================================
    // 1. ИДЕАЛЬНОЕ ПРОПОРЦИОНАЛЬНОЕ МАСШТАБИРОВАНИЕ (Теперь работает и для ГРУПП)
    // =========================================================================
    if (m_draggedHandle >= 1 && m_draggedHandle <= 4 && m_selectedShapes.size() == 1) {
        int cornerType = m_draggedHandle;

        float oldW = m_dragStartGroupBounds.size.x;
        float oldH = m_dragStartGroupBounds.size.y;
        if (oldW < 0.1f || oldH < 0.1f) return;

        // Противоположный угол ИЗ СНИМКА (Никогда не сдвинется!)
        sf::Vector2f fixedCorner;
        if (cornerType == 1) fixedCorner = { m_dragStartGroupBounds.position.x + oldW, m_dragStartGroupBounds.position.y + oldH };
        else if (cornerType == 2) fixedCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y + oldH };
        else if (cornerType == 3) fixedCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y };
        else if (cornerType == 4) fixedCorner = { m_dragStartGroupBounds.position.x + oldW, m_dragStartGroupBounds.position.y };

        // Убираем скачок мыши при старте
        sf::Vector2f effMouse = mousePos - m_scaleMouseOffset;

        float newMinX = m_dragStartGroupBounds.position.x, newMaxX = m_dragStartGroupBounds.position.x + oldW;
        float newMinY = m_dragStartGroupBounds.position.y, newMaxY = m_dragStartGroupBounds.position.y + oldH;

        if (cornerType == 1) { newMinX = effMouse.x; newMinY = effMouse.y; }
        else if (cornerType == 2) { newMaxX = effMouse.x; newMinY = effMouse.y; }
        else if (cornerType == 3) { newMaxX = effMouse.x; newMaxY = effMouse.y; }
        else if (cornerType == 4) { newMinX = effMouse.x; newMaxY = effMouse.y; }

        // Защита от выворачивания
        if (newMinX > newMaxX - 10.f) { if (cornerType == 1 || cornerType == 4) newMinX = newMaxX - 10.f; else newMaxX = newMinX + 10.f; }
        if (newMinY > newMaxY - 10.f) { if (cornerType == 1 || cornerType == 2) newMinY = newMaxY - 10.f; else newMaxY = newMinY + 10.f; }

        float newW = std::clamp(newMaxX - newMinX, 10.0f, 10000.0f);
        float newH = std::clamp(newMaxY - newMinY, 10.0f, 10000.0f);

        // Чистый коэффициент масштаба
        float Sx = newW / oldW;
        float Sy = newH / oldH;

        auto* shape = m_selectedShapes[0];

        // 1. Откатываемся к ИДЕАЛЬНОМУ снимку (Теперь работает рекурсивно для всех детей Группы!)
        shape->restoreState();

        // 2. ВЫЗЫВАЕМ МАГИЮ ПОЛИМОРФИЗМА
        shape->applyGlobalScale(fixedCorner, Sx, Sy);
    }
    // =========================================================================
    // 1.5. ЛОКАЛЬНОЕ МАСШТАБИРОВАНИЕ (OBB) ДЛЯ ЭЛЛИПСА
    // =========================================================================
    else if (m_draggedHandle >= 11 && m_draggedHandle <= 14 && m_selectedShapes.size() == 1) {
        EllipseShape* ellipse = dynamic_cast<EllipseShape*>(m_selectedShapes[0]);
        if (ellipse) {
            ellipse->restoreState(); // Откат к идеальному снимку начала клика
            sf::Vector2f effMouse = mousePos - m_scaleMouseOffset; // Снимаем микро-"прыжок" мыши
            ellipse->resizeFromOBB(m_draggedHandle, effMouse);
        }
    }
    // =========================================================================
    // 2. ПЕРЕТАСКИВАНИЕ ЯКОРЯ
    // =========================================================================
    else if (m_draggedHandle == 5 && m_selectedShapes.size() == 1) {
        m_selectedShapes[0]->setAnchorPositionWorld(mousePos);
    }
    // =========================================================================
    // 3. ИДЕАЛЬНОЕ ВРАЩЕНИЕ
    // =========================================================================
    else if (m_draggedHandle == 7 && m_selectedShapes.size() == 1) {
        m_selectedShapes[0]->restoreState(); // <-- ДОБАВИТЬ ЭТО! Возвращаемся к снимку
        m_selectedShapes[0]->setRotationFromMouse(mousePos); // Применяем новый идеальный угол
    }
    // =========================================================================
    // 4. ПЕРЕМЕЩЕНИЕ ВЕРШИН (УЗЛОВ) ФИГУРЫ
    // =========================================================================
    else if (m_draggedHandle >= 100 && m_selectedShapes.size() == 1) {
        int vertexIndex = m_draggedHandle - 100;
        sf::Vector2f delta = mousePos - m_lastMousePos;
        m_selectedShapes[0]->moveVertex(vertexIndex, delta);
        m_lastMousePos = mousePos;
    }
    // =========================================================================
    // 5. ПЕРЕМЕЩЕНИЕ САМОЙ ФИГУРЫ ИЛИ ГРУППЫ
    // =========================================================================
    else if (m_draggedHandle == 0 && !m_isBoxSelecting) {
        sf::Vector2f delta = mousePos - m_lastMousePos;

        for (auto* shape : m_selectedShapes) {
            // Если shape - это Group, она сама сдвинет всех детей
            shape->setPosition(shape->getPosition() + delta);
        }

        m_lastMousePos = mousePos;
    }
}

// ==========================================
// ЛОГИКА ГРУППИРОВКИ
// ==========================================
void Scene::groupSelected() {
    if (m_selectedShapes.size() < 2) return;

    // Рассчитываем центр для якоря новой группы
    float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
    for (auto* shape : m_selectedShapes) {
        sf::FloatRect bounds = shape->getBounds();
        if (bounds.position.x < minX) minX = bounds.position.x;
        if (bounds.position.y < minY) minY = bounds.position.y;
        if (bounds.position.x + bounds.size.x > maxX) maxX = bounds.position.x + bounds.size.x;
        if (bounds.position.y + bounds.size.y > maxY) maxY = bounds.position.y + bounds.size.y;
    }
    sf::Vector2f center(minX + (maxX - minX) / 2.0f, minY + (maxY - minY) / 2.0f);

    // Создаем группу
    auto newGroup = std::make_unique<Group>(center);
    newGroup->setId(m_nextShapeId++);
    newGroup->setName("Group " + std::to_string(newGroup->getId()));

    // Перемещаем выделенные фигуры внутрь группы
    for (auto* selectedShape : m_selectedShapes) {
        auto it = std::find_if(m_shapes.begin(), m_shapes.end(),
            [&](const std::unique_ptr<IShape>& p) { return p.get() == selectedShape; });

        if (it != m_shapes.end()) {
            newGroup->addChild(std::move(*it));
            m_shapes.erase(it); // Удаляем со сцены
        }
    }

    // Добавляем группу на сцену
    auto* groupPtr = newGroup.get();
    m_shapes.push_back(std::move(newGroup));

    // Оставляем выделенной только новую группу
    clearSelection();
    groupPtr->setSelected(true);
    m_selectedShapes.push_back(groupPtr);
}

void Scene::bringToFront(IShape* shape) {
    auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [shape](const auto& ptr) { return ptr.get() == shape; });
    if (it != m_shapes.end()) {
        auto ptr = std::move(*it);
        m_shapes.erase(it);
        m_shapes.push_back(std::move(ptr)); // Переносим в конец (будет рисоваться последним = сверху)
    }
}
void Scene::sendToBack(IShape* shape) {
    auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [shape](const auto& ptr) { return ptr.get() == shape; });
    if (it != m_shapes.end()) {
        auto ptr = std::move(*it);
        m_shapes.erase(it);
        m_shapes.insert(m_shapes.begin(), std::move(ptr)); // Переносим в начало (= сзади всех)
    }
}
//
void Scene::startDrawingMode() {
    clearSelection();
    m_isDrawingMode = true;
    m_drawingPoints.clear();
}

void Scene::cancelDrawing() {
    m_isDrawingMode = false;
    m_drawingPoints.clear();
}

void Scene::finishDrawing(bool isClosed) {
    if (m_drawingPoints.size() < 2) { cancelDrawing(); return; }

    // Находим центр и габариты нарисованного
    float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
    for (auto& p : m_drawingPoints) {
        if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
    }

    float width = maxX - minX; float height = maxY - minY;
    float size = std::max(width, height) / 2.0f;
    if (size < 1.0f) size = 50.0f; // Защита от микро-фигур

    sf::Vector2f center(minX + width / 2.0f, minY + height / 2.0f);

    // Нормализуем точки (-1 до 1) относительно центра
    std::vector<sf::Vector2f> basePoints(m_drawingPoints.size());
    for (size_t i = 0; i < m_drawingPoints.size(); i++) {
        basePoints[i] = { (m_drawingPoints[i].x - center.x) / size, (m_drawingPoints[i].y - center.y) / size };
    }

    // Создаем фигуру (ломаную или замкнутую)
    auto shape = std::make_unique<PolygonShape>(center, sf::Vector2f(size, size), isClosed, basePoints);
    if (!isClosed) shape->setGlobalFillColor(sf::Color::Transparent); // У ломаной нет заливки

    addShape(std::move(shape));

    cancelDrawing();
}
void Scene::deleteSelected() {
    if (m_selectedShapes.empty()) return;

    // Рекурсивное удаление: проходит по сцене и всем группам
    auto removeSelected = [&](auto& self, std::vector<std::unique_ptr<IShape>>& list) -> void {
        // Удаляем выделенные элементы из текущего списка
        list.erase(std::remove_if(list.begin(), list.end(),
            [this](const std::unique_ptr<IShape>& shape) {
                return std::find(m_selectedShapes.begin(), m_selectedShapes.end(), shape.get()) != m_selectedShapes.end();
            }), list.end());

        // Заходим в оставшиеся группы и чистим детей там
        for (auto& shape : list) {
            if (shape->getType() == "Group") {
                Group* g = dynamic_cast<Group*>(shape.get());
                if (g) self(self, g->getChildren());
            }
        }
        };

    removeSelected(removeSelected, m_shapes);
    m_selectedShapes.clear();
}


void Scene::selectShape(int id) {
    IShape* target = nullptr;

    // Рекурсивная лямбда для поиска по всем уровням вложенности
    auto findShape = [&](auto& self, const std::vector<std::unique_ptr<IShape>>& list) -> void {
        for (const auto& s : list) {
            if (s->getId() == id) {
                target = s.get();
                return;
            }
            if (s->getType() == "Group") {
                Group* g = dynamic_cast<Group*>(s.get());
                if (g) self(self, g->getChildren()); // Ныряем внутрь группы
                if (target) return; // Если нашли внутри, выходим
            }
        }
        };

    findShape(findShape, m_shapes);

    if (target) {
        if (std::find(m_selectedShapes.begin(), m_selectedShapes.end(), target) == m_selectedShapes.end()) {
            target->setSelected(true);
            m_selectedShapes.push_back(target);
        }
    }
}// --- НОВЫЙ МЕТОД СОХРАНЕНИЯ ТОЛЬКО ВЫДЕЛЕННОГО ---
void Scene::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return;

    out << "ShapesCount: " << m_shapes.size() << "\n";
    for (const auto& shape : m_shapes) {
        out << "[" << shape->getType() << "]\n";
        shape->save(out);
    }
}

void Scene::saveSelectedToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return;

    out << "ShapesCount: " << m_selectedShapes.size() << "\n";
    for (const auto* shape : m_selectedShapes) {
        out << "[" << shape->getType() << "]\n";
        shape->save(out);
    }
}

void Scene::copySelected() {
    if (m_selectedShapes.empty()) return;

    std::ostringstream out;

    out << "ShapesCount: " << m_selectedShapes.size() << "\n";
    for (const auto* shape : m_selectedShapes) {
        out << "[" << shape->getType() << "]\n";
        shape->save(out);
    }

    m_clipboard = out.str();
    m_pasteCount = 1;
}

// ==========================================
// ВСТАВКА ИЗ БУФЕРА ОБМЕНА
// ==========================================
void Scene::loadFromFile(const std::string& filename, bool merge) {
    std::ifstream in(filename);
    if (!in) return;

    if (!merge) clear(); // Очищаем сцену, если это не слияние

    std::string dummy;
    size_t shapeCount = 0;

    // Считываем заголовок "ShapesCount: N"
    if (!(in >> dummy >> shapeCount)) return;

    for (size_t i = 0; i < shapeCount; ++i) {
        auto shape = parseShapeRecursive(in, m_nextShapeId, merge);
        if (shape) {
            if (merge) {
                shape->setSelected(true); // При слиянии выделяем загруженное
                m_selectedShapes.push_back(shape.get());
            }
            m_shapes.push_back(std::move(shape));
        }
    }
}

void Scene::paste() {
    if (m_clipboard.empty()) return;

    std::istringstream in(m_clipboard);
    clearSelection(); // Сбрасываем выделение, чтобы выделить только вставленное

    std::string dummy;
    size_t shapeCount = 0;
    if (!(in >> dummy >> shapeCount)) return;

    // Рассчитываем смещение "лесенкой"
    sf::Vector2f offset(20.0f * m_pasteCount, 20.0f * m_pasteCount);

    for (size_t i = 0; i < shapeCount; ++i) {
        // При вставке логика ID всегда как при merge (merge = true)
        auto shape = parseShapeRecursive(in, m_nextShapeId, true);
        if (shape) {
            // Сдвигаем корневую фигуру. 
            // Если это Group, метод setPosition внутри неё сдвинет всех её детей!
            shape->setPosition(shape->getPosition() + offset);

            shape->setSelected(true);
            m_selectedShapes.push_back(shape.get());
            m_shapes.push_back(std::move(shape));
        }
    }

    m_pasteCount++;
}
void Scene::cleanupEmptyGroups() {
    auto cleanup = [&](auto& self, std::vector<std::unique_ptr<IShape>>& list) -> void {
        for (size_t i = 0; i < list.size(); ) {
            if (list[i]->getType() == "Group") {
                Group* g = dynamic_cast<Group*>(list[i].get());
                if (g) {
                    self(self, g->getChildren()); // Рекурсивно чистим вложенные

                    size_t childCount = g->getChildren().size();
                    // Если группа пуста или в ней всего 1 фигура — распускаем её
                    if (childCount <= 1) {
                        if (childCount == 1) {
                            // Переносим последнюю фигуру на уровень выше
                            list.push_back(std::move(g->getChildren()[0]));
                        }
                        list.erase(list.begin() + i);
                        continue; // Проверяем тот же индекс снова
                    }
                }
            }
            i++;
        }
        };
    cleanup(cleanup, m_shapes);
}

void Scene::ungroupSelected() {
    if (m_selectedShapes.size() != 1) return;
    IShape* target = m_selectedShapes[0];

    if (target->getType() != "Group") return;
    Group* groupPtr = dynamic_cast<Group*>(target);

    auto process = [&](auto& self, std::vector<std::unique_ptr<IShape>>& list) -> bool {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].get() == target) {
                // Извлекаем группу и удаляем её, не ломая итератор
                std::unique_ptr<IShape> extracted = std::move(list[i]);
                list.erase(list.begin() + i);

                auto& children = groupPtr->getChildren();
                for (auto& child : children) {
                    child->setSelected(true);
                    m_selectedShapes.push_back(child.get());
                    list.push_back(std::move(child));
                }
                children.clear();
                return true;
            }
            if (list[i]->getType() == "Group") {
                Group* g = dynamic_cast<Group*>(list[i].get());
                if (g && self(self, g->getChildren())) return true;
            }
        }
        return false;
        };

    m_selectedShapes.clear();
    process(process, m_shapes);
    cleanupEmptyGroups();
}

bool Scene::isShapeInGroup(IShape* shape) const {
    // Если фигура в корневом списке m_shapes — она не в группе
    for (const auto& s : m_shapes) {
        if (s.get() == shape) return false;
    }
    return true; // Иначе она находится внутри какой-то группы
}
void Scene::extractFromGroup(IShape* shape) {
    if (!isShapeInGroup(shape)) return;

    auto extract = [&](auto& self, std::vector<std::unique_ptr<IShape>>& list) -> bool {
        for (auto it = list.begin(); it != list.end(); ++it) {
            if ((*it)->getType() == "Group") {
                Group* g = dynamic_cast<Group*>(it->get());
                if (g) {
                    auto& children = g->getChildren();
                    auto childIt = std::find_if(children.begin(), children.end(),
                        [&](const std::unique_ptr<IShape>& p) { return p.get() == shape; });

                    if (childIt != children.end()) {
                        m_shapes.push_back(std::move(*childIt));
                        children.erase(childIt);
                        return true;
                    }
                    if (self(self, children)) return true;
                }
            }
        }
        return false;
        };

    extract(extract, m_shapes);
    cleanupEmptyGroups(); // Чистим возможные одиночные/пустые группы
}