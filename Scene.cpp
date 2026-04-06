#include "Scene.h"
#include <algorithm>
#include "PolygonShape.h"
#include "CircleShape.h"
#include <fstream>

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
    if (m_isDrawingMode) {
        if (m_cursorCross) window.setMouseCursor(*m_cursorCross);
        return;
    }

    int activeHandle = 0;
    bool hoverSide = false;
    bool hoverShape = false;

    ShapeGroup* formalGroup = getFormalSelectedGroup();
    bool hideIndividualHandles = (m_selectedShapes.size() > 1 || formalGroup != nullptr);

    if (m_isDragging) {
        activeHandle = m_draggedHandle;
    }
    else {
        // --- Ищем наведение на углы группы ---
        if (hideIndividualHandles) {
            float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
            for (auto* shape : m_selectedShapes) {
                sf::FloatRect b = shape->getBounds();
                if (b.position.x < minX) minX = b.position.x;
                if (b.position.y < minY) minY = b.position.y;
                if (b.position.x + b.size.x > maxX) maxX = b.position.x + b.size.x;
                if (b.position.y + b.size.y > maxY) maxY = b.position.y + b.size.y;
            }
            float hw = 6.0f;
            if (mousePos.x >= minX - hw && mousePos.x <= minX + hw && mousePos.y >= minY - hw && mousePos.y <= minY + hw) activeHandle = 21;
            else if (mousePos.x >= maxX - hw && mousePos.x <= maxX + hw && mousePos.y >= minY - hw && mousePos.y <= minY + hw) activeHandle = 22;
            else if (mousePos.x >= maxX - hw && mousePos.x <= maxX + hw && mousePos.y >= maxY - hw && mousePos.y <= maxY + hw) activeHandle = 23;
            else if (mousePos.x >= minX - hw && mousePos.x <= minX + hw && mousePos.y >= maxY - hw && mousePos.y <= maxY + hw) activeHandle = 24;
        }

        // --- Ищем индивидуальные маркеры (ТОЛЬКО если они видимы) ---
        if (activeHandle == 0 && !hideIndividualHandles) {
            for (auto* shape : m_selectedShapes) {
                activeHandle = shape->getHitHandle(mousePos);
                if (activeHandle != 0) break;
                if (shape->getHitSideIndex(mousePos) != -1) { hoverSide = true; break; }
            }
        }

        // Если ни во что не попали, проверяем просто наведение на тело фигуры
        if (activeHandle == 0 && !hoverSide) {
            for (auto* shape : m_selectedShapes) {
                if (shape->contains(mousePos)) { hoverShape = true; break; }
            }
        }
    }

    if (activeHandle == 1 || activeHandle == 3 || activeHandle == 21 || activeHandle == 23) {
        if (m_cursorSizeTLBR) window.setMouseCursor(*m_cursorSizeTLBR);
    }
    else if (activeHandle == 2 || activeHandle == 4 || activeHandle == 22 || activeHandle == 24) {
        if (m_cursorSizeBLTR) window.setMouseCursor(*m_cursorSizeBLTR);
    }
    else if (activeHandle == 5 || activeHandle == 6 || hoverShape) {
        if (m_cursorSizeAll) window.setMouseCursor(*m_cursorSizeAll);
    }
    else if (activeHandle == 7 || activeHandle >= 100 || hoverSide) {
        if (m_cursorHand) window.setMouseCursor(*m_cursorHand);
    }
    else {
        if (m_cursorArrow) window.setMouseCursor(*m_cursorArrow);
    }
}
// Новый метод для сброса курсора при входе в ImGui
void Scene::resetCursor(sf::RenderWindow& window) {
    if (m_cursorArrow) window.setMouseCursor(*m_cursorArrow);
}

void Scene::addShape(std::unique_ptr<IShape> shape) { m_shapes.push_back(std::move(shape)); }

void Scene::draw(sf::RenderTarget& target) const {
    // 1. Сначала рисуем сами фигуры
    for (const auto& shape : m_shapes) shape->draw(target);

    // 2. Проверяем, выделена ли целая формальная группа
    bool isFormalGroup = false;
    std::string firstId = "";
    if (!m_selectedShapes.empty()) {
        firstId = m_selectedShapes[0]->getGroupId();
        isFormalGroup = !firstId.empty();
        if (isFormalGroup) {
            for (auto* s : m_selectedShapes) {
                if (s->getGroupId() != firstId) { isFormalGroup = false; break; }
            }
            if (isFormalGroup) {
                size_t count = 0;
                for (const auto& s : m_shapes) { if (s->getGroupId() == firstId) count++; }
                if (count != m_selectedShapes.size()) isFormalGroup = false;
            }
        }
    }

    // Если выделено несколько фигур ИЛИ выделена целая группа — прячем индивидуальные элементы
    bool showIndividualHandles = !(m_selectedShapes.size() > 1 || isFormalGroup);

    // 3. Рисуем индивидуальные элементы (если разрешено)
    if (showIndividualHandles) {
        for (auto* shape : m_selectedShapes) shape->drawSelection(target);
    }

    // 4. --- ГЛОБАЛЬНАЯ РАМКА И ОРАНЖЕВЫЙ ЯКОРЬ ---
    if (m_selectedShapes.size() > 1 || isFormalGroup) {
        float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
        for (auto* shape : m_selectedShapes) {
            sf::FloatRect bounds = shape->getBounds();
            if (bounds.position.x < minX) minX = bounds.position.x;
            if (bounds.position.y < minY) minY = bounds.position.y;
            if (bounds.position.x + bounds.size.x > maxX) maxX = bounds.position.x + bounds.size.x;
            if (bounds.position.y + bounds.size.y > maxY) maxY = bounds.position.y + bounds.size.y;
        }

        // Рисуем рамку группы
        sf::RectangleShape globalBox({ maxX - minX, maxY - minY });
        globalBox.setPosition({ minX, minY });
        globalBox.setFillColor(sf::Color::Transparent);
        globalBox.setOutlineColor(sf::Color(30, 144, 255)); // DodgerBlue
        globalBox.setOutlineThickness(2.0f);
        target.draw(globalBox);

        float hw = 5.0f;
        sf::RectangleShape handle(sf::Vector2f(hw * 2, hw * 2));
        handle.setFillColor(sf::Color::White);
        handle.setOutlineColor(sf::Color(30, 144, 255));
        handle.setOutlineThickness(1.5f);

        handle.setPosition({ minX - hw, minY - hw }); target.draw(handle); // TL
        handle.setPosition({ maxX - hw, minY - hw }); target.draw(handle); // TR
        handle.setPosition({ maxX - hw, maxY - hw }); target.draw(handle); // BR
        handle.setPosition({ minX - hw, maxY - hw }); target.draw(handle); // BL

        // Рисуем оранжевый якорь ТОЛЬКО если выделена вся группа
        if (isFormalGroup) {
            sf::Vector2f anchorPt;
            for (const auto& g : m_groups) {
                if (g.id == firstId) { anchorPt = g.anchorPoint; break; }
            }

            sf::CircleShape gAnchor(7.0f);
            gAnchor.setOrigin({ 7.0f, 7.0f });
            gAnchor.setPosition(anchorPt);
            gAnchor.setFillColor(sf::Color(255, 165, 0)); // Оранжевый
            gAnchor.setOutlineColor(sf::Color::White);
            gAnchor.setOutlineThickness(2.0f);
            target.draw(gAnchor);
        }
    }

    // 5. Рамка выделения (Drag Selection)
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

    // 6. Режим рисования
    if (m_isDrawingMode && !m_drawingPoints.empty()) {
        sf::VertexArray lines(sf::PrimitiveType::LineStrip, m_drawingPoints.size() + 1);
        for (size_t i = 0; i < m_drawingPoints.size(); i++) {
            lines[i].position = m_drawingPoints[i];
            lines[i].color = sf::Color::White;
        }
        lines[m_drawingPoints.size()].position = m_currentMousePos;
        lines[m_drawingPoints.size()].color = sf::Color(255, 255, 255, 150);
        target.draw(lines);
    }
}
void Scene::clear() {
    m_shapes.clear();
    m_selectedShapes.clear();
    m_groups.clear();
}

void Scene::clearSelection() {
    for (auto* shape : m_selectedShapes) shape->setSelected(false);
    m_selectedShapes.clear();
}

void Scene::handleMousePress(sf::Vector2f mousePos, bool isShiftPressed, bool isCtrlPressed) {
    ShapeGroup* formalGroup = getFormalSelectedGroup();
    if (m_isDrawingMode) {
        if (isShiftPressed && !m_drawingPoints.empty()) {
            sf::Vector2f lastPt = m_drawingPoints.back();
            sf::Vector2f diff = mousePos - lastPt;
            float angle = std::atan2(diff.y, diff.x);
            float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            const float snapRad = 15.0f * 3.14159265f / 180.0f;
            angle = std::round(angle / snapRad) * snapRad;
            mousePos = lastPt + sf::Vector2f(std::cos(angle) * len, std::sin(angle) * len);
        }
        m_drawingPoints.push_back(mousePos);
        return;
    }

    bool hideIndividualHandles = (m_selectedShapes.size() > 1 || formalGroup != nullptr);

    // --- 1. Проверяем маркеры масштабирования ГРУППЫ ---
    if (hideIndividualHandles) {
        float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
        for (auto* shape : m_selectedShapes) {
            sf::FloatRect bounds = shape->getBounds();
            if (bounds.position.x < minX) minX = bounds.position.x;
            if (bounds.position.y < minY) minY = bounds.position.y;
            if (bounds.position.x + bounds.size.x > maxX) maxX = bounds.position.x + bounds.size.x;
            if (bounds.position.y + bounds.size.y > maxY) maxY = bounds.position.y + bounds.size.y;
        }

        float hw = 6.0f;
        auto checkHit = [&](float x, float y) { return mousePos.x >= x - hw && mousePos.x <= x + hw && mousePos.y >= y - hw && mousePos.y <= y + hw; };

        int hitHandle = 0;
        if (checkHit(minX, minY)) hitHandle = 21;
        else if (checkHit(maxX, minY)) hitHandle = 22;
        else if (checkHit(maxX, maxY)) hitHandle = 23;
        else if (checkHit(minX, maxY)) hitHandle = 24;

        if (hitHandle != 0) {
            m_draggedHandle = hitHandle;
            m_isDragging = true;
            m_lastMousePos = mousePos;

            m_dragStartGroupBounds = sf::FloatRect({ minX, minY }, { maxX - minX, maxY - minY });
            m_dragStartSnapshots.clear(); m_dragStartBounds.clear(); m_dragStartAnchors.clear();

            for (auto* shape : m_selectedShapes) {
                m_dragStartSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation(), shape->getBasePoints() });
                m_dragStartBounds.push_back(shape->getBounds());
                m_dragStartAnchors.push_back(shape->getPosition() + shape->getAnchorOffset());
            }
            if (formalGroup) m_dragStartFormalGroupAnchor = formalGroup->anchorPoint;

            sf::Vector2f perfectCorner;
            if (hitHandle == 21) perfectCorner = { minX, minY };
            else if (hitHandle == 22) perfectCorner = { maxX, minY };
            else if (hitHandle == 23) perfectCorner = { maxX, maxY };
            else if (hitHandle == 24) perfectCorner = { minX, maxY };
            m_scaleMouseOffset = mousePos - perfectCorner;
            return;
        }
    }

    // --- 2. Проверяем ОРАНЖЕВЫЙ ЯКОРЬ группы ---
    if (formalGroup) {
        float hw = 7.0f;
        if (mousePos.x >= formalGroup->anchorPoint.x - hw && mousePos.x <= formalGroup->anchorPoint.x + hw &&
            mousePos.y >= formalGroup->anchorPoint.y - hw && mousePos.y <= formalGroup->anchorPoint.y + hw) {
            m_isDragging = true;
            m_draggedHandle = 6;
            m_lastMousePos = mousePos;
            return;
        }
    }

    // --- 3. Проверяем маркеры выделенных фигур (ТОЛЬКО ЕСЛИ ГРУППА НЕ ВЫДЕЛЕНА) ---
    if (!hideIndividualHandles) {
        for (auto* shape : m_selectedShapes) {
            int handle = shape->getHitHandle(mousePos);
            if (handle != 0) {
                m_draggedHandle = handle;
                if (handle >= 100) { shape->setSelectedVertex(handle - 100); shape->setSelectedSide(-1); }
                else { shape->setSelectedVertex(-1); }

                m_isDragging = true;
                m_lastMousePos = mousePos;

                if (handle >= 1 && handle <= 4) {
                    sf::FloatRect bounds = shape->getBounds();

                    m_dragStartSnapshots.clear();
                    m_dragStartSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation(), shape->getBasePoints() });
                    m_dragStartBounds.clear();
                    m_dragStartBounds.push_back(bounds);
                    m_dragStartAnchors.clear();
                    m_dragStartAnchors.push_back(shape->getPosition() + shape->getAnchorOffset());
                    m_dragStartGroupBounds = bounds;

                    sf::Vector2f perfectCorner;
                    if (handle == 1) perfectCorner = { bounds.position.x, bounds.position.y };
                    else if (handle == 2) perfectCorner = { bounds.position.x + bounds.size.x, bounds.position.y };
                    else if (handle == 3) perfectCorner = { bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y };
                    else if (handle == 4) perfectCorner = { bounds.position.x, bounds.position.y + bounds.size.y };

                    m_scaleMouseOffset = mousePos - perfectCorner;
                }
                else {
                    m_scaleMouseOffset = { 0.0f, 0.0f };
                    m_dragStartSnapshots.clear();
                    m_dragStartSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation(), shape->getBasePoints() });
                }
                return;
            }
        }
    }

    // --- 4. КЛИК ПО ТЕЛУ ФИГУРЫ ---
    IShape* clickedShape = nullptr;
    for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
        if ((*it)->contains(mousePos)) { clickedShape = it->get(); break; }
    }

    if (clickedShape) {
        std::string gId = clickedShape->getGroupId();
        bool sideClickHandled = false;
        auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clickedShape);

        // Проверка клика по стороне (только если выделена ровно одна фигура без модификаторов)
        if (it != m_selectedShapes.end() && !isCtrlPressed && !isShiftPressed && m_selectedShapes.size() == 1 && gId.empty()) {
            int hitSide = clickedShape->getHitSideIndex(mousePos);
            clickedShape->setSelectedSide(hitSide);
            sideClickHandled = true;
        }

        if (!sideClickHandled) {
            for (auto* s : m_selectedShapes) s->setSelectedSide(-1);
        }

        // ========================================================
        // НОВАЯ ПРОФЕССИОНАЛЬНАЯ ЛОГИКА ВЫДЕЛЕНИЯ
        // ========================================================
        if (!gId.empty() && !isCtrlPressed) {
            // КЛИК ПО ФИГУРЕ В ГРУППЕ (БЕЗ CTRL) -> Выделяем всю группу целиком
            bool isGroupFullySelected = true;
            std::vector<IShape*> groupShapes;
            for (auto& s : m_shapes) {
                if (s->getGroupId() == gId) {
                    groupShapes.push_back(s.get());
                    if (std::find(m_selectedShapes.begin(), m_selectedShapes.end(), s.get()) == m_selectedShapes.end()) {
                        isGroupFullySelected = false;
                    }
                }
            }

            if (isShiftPressed) {
                // Shift: добавляем или убираем группу из текущего выделения
                if (isGroupFullySelected) {
                    for (auto* s : groupShapes) {
                        s->setSelected(false);
                        m_selectedShapes.erase(std::remove(m_selectedShapes.begin(), m_selectedShapes.end(), s), m_selectedShapes.end());
                    }
                }
                else {
                    for (auto* s : groupShapes) {
                        if (std::find(m_selectedShapes.begin(), m_selectedShapes.end(), s) == m_selectedShapes.end()) {
                            s->setSelected(true); m_selectedShapes.push_back(s);
                        }
                    }
                }
            }
            else {
                // Обычный клик: выделяем только эту группу
                if (!isGroupFullySelected || m_selectedShapes.size() > groupShapes.size()) {
                    clearSelection();
                    selectGroup(gId);
                }
            }
        }
        else {
            // КЛИК ПО НЕЗАВИСИМОЙ ФИГУРЕ (или фигуре в группе + CTRL) -> Выделяем индивидуально!
            if (isShiftPressed) {
                if (it != m_selectedShapes.end()) {
                    clickedShape->setSelected(false); m_selectedShapes.erase(it);
                }
                else {
                    clickedShape->setSelected(true); m_selectedShapes.push_back(clickedShape);
                }
            }
            else {
                if (it == m_selectedShapes.end() || m_selectedShapes.size() > 1) {
                    clearSelection();
                    clickedShape->setSelected(true);
                    m_selectedShapes.push_back(clickedShape);
                }
            }
        }

        m_isDragging = true;
        m_draggedHandle = 0;
        m_lastMousePos = mousePos;
    }
    else {
        if (!isShiftPressed) clearSelection();
        m_isBoxSelecting = true;
        m_selectionStartPos = mousePos;
        m_lastMousePos = mousePos;
    }
}

void Scene::handleMouseRelease(bool isShiftPressed) {
    // --- ТВОЯ ИДЕЯ: Возвращаем оригинальный угол после отпускания мыши ---
    if (m_isDragging && m_draggedHandle >= 1 && m_draggedHandle <= 4 && m_selectedShapes.size() == 1) {
        if (!m_dragStartSnapshots.empty()) {
            m_selectedShapes[0]->setRotation(m_dragStartSnapshots[0].rotation);
        }
    }
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
                // Фигура должна ПОЛНОСТЬЮ попасть в рамку
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
void Scene::handleMouseMove(sf::Vector2f mousePos) {
    if (m_isDrawingMode) {
        m_currentMousePos = mousePos;
        return;
    }

    if (m_isDragging) {

        // =========================================================================
        // 1. ИДЕАЛЬНОЕ ПРОПОРЦИОНАЛЬНОЕ МАСШТАБИРОВАНИЕ (Группа и Одиночная фигура)
        // =========================================================================
        if ((m_draggedHandle >= 1 && m_draggedHandle <= 4 && m_selectedShapes.size() == 1) ||
            (m_draggedHandle >= 21 && m_draggedHandle <= 24 && m_selectedShapes.size() > 1)) {

            int cornerType = m_draggedHandle;
            if (cornerType > 20) cornerType -= 20; // Превращаем 21-24 в 1-4

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

            // Чистый коэффициент масштаба рамки
            float Sx = newW / oldW;
            float Sy = newH / oldH;

            for (size_t i = 0; i < m_selectedShapes.size(); i++) {
                auto* shape = m_selectedShapes[i];
                if (i >= m_dragStartSnapshots.size()) continue;
                const auto& snap = m_dragStartSnapshots[i];

                // 1. Откатываемся к ИДЕАЛЬНОМУ снимку (восстанавливаем даже вершины!)
                shape->setPosition(snap.position);
                shape->setSize(snap.size);
                shape->setAnchorOffset(snap.anchorOffset);
                shape->setRotation(snap.rotation);
                if (!snap.basePoints.empty()) shape->setBasePoints(snap.basePoints);

                // 2. ВЫЗЫВАЕМ МАГИЮ: Точки сдвигаются ровно за рамкой, угол сохраняется сам!
                shape->applyGlobalScale(fixedCorner, Sx, Sy);
            }

            // Масштабируем якорь формальной группы
            if (m_selectedShapes.size() > 1) {
                ShapeGroup* formalGroup = getFormalSelectedGroup();
                if (formalGroup) {
                    sf::Vector2f dist = m_dragStartFormalGroupAnchor - fixedCorner;
                    formalGroup->anchorPoint = fixedCorner + sf::Vector2f(dist.x * Sx, dist.y * Sy);
                }
            }
        }

        // =========================================================================
        // 2. ИДЕАЛЬНОЕ МАСШТАБИРОВАНИЕ ГРУППЫ ФИГУР
        // =========================================================================
        else if (m_draggedHandle >= 21 && m_draggedHandle <= 24 && m_selectedShapes.size() > 1) {
            int cornerType = m_draggedHandle - 20;

            float oldW = m_dragStartGroupBounds.size.x;
            float oldH = m_dragStartGroupBounds.size.y;

            if (oldW > 0.1f && oldH > 0.1f) {
                sf::Vector2f fixedCorner;
                if (cornerType == 1) fixedCorner = { m_dragStartGroupBounds.position.x + oldW, m_dragStartGroupBounds.position.y + oldH };
                else if (cornerType == 2) fixedCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y + oldH };
                else if (cornerType == 3) fixedCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y };
                else if (cornerType == 4) fixedCorner = { m_dragStartGroupBounds.position.x + oldW, m_dragStartGroupBounds.position.y };

                float newMinX = m_dragStartGroupBounds.position.x, newMaxX = m_dragStartGroupBounds.position.x + oldW;
                float newMinY = m_dragStartGroupBounds.position.y, newMaxY = m_dragStartGroupBounds.position.y + oldH;

                if (cornerType == 1) { newMinX = mousePos.x; newMinY = mousePos.y; }
                else if (cornerType == 2) { newMaxX = mousePos.x; newMinY = mousePos.y; }
                else if (cornerType == 3) { newMaxX = mousePos.x; newMaxY = mousePos.y; }
                else if (cornerType == 4) { newMinX = mousePos.x; newMaxY = mousePos.y; }

                if (newMinX > newMaxX - 10.f) { if (cornerType == 1 || cornerType == 4) newMinX = newMaxX - 10.f; else newMaxX = newMinX + 10.f; }
                if (newMinY > newMaxY - 10.f) { if (cornerType == 1 || cornerType == 2) newMinY = newMaxY - 10.f; else newMaxY = newMinY + 10.f; }

                float newW = std::clamp(newMaxX - newMinX, 10.0f, 10000.0f);
                float newH = std::clamp(newMaxY - newMinY, 10.0f, 10000.0f);

                float Sx = newW / oldW;
                float Sy = newH / oldH;

                for (size_t i = 0; i < m_selectedShapes.size(); i++) {
                    auto* shape = m_selectedShapes[i];
                    if (i >= m_dragStartSnapshots.size()) continue;

                    const auto& snap = m_dragStartSnapshots[i];
                    sf::FloatRect initialB = m_dragStartBounds[i];
                    sf::Vector2f initialAnchor = m_dragStartAnchors[i];

                    shape->setPosition(snap.position);
                    shape->setSize(snap.size);
                    shape->setAnchorOffset(snap.anchorOffset);
                    shape->setRotation(snap.rotation);

                    shape->resizeFromBoundingBox(initialB.size.x * Sx, initialB.size.y * Sy);

                    sf::Vector2f dist = initialAnchor - fixedCorner;
                    sf::Vector2f targetAnchor = fixedCorner + sf::Vector2f(dist.x * Sx, dist.y * Sy);

                    sf::Vector2f currentAnchor = shape->getPosition() + shape->getAnchorOffset();
                    sf::Vector2f shift = targetAnchor - currentAnchor;
                    shape->setPosition(shape->getPosition() + shift);
                }

                float actualMinX = 99999.f, actualMinY = 99999.f, actualMaxX = -99999.f, actualMaxY = -99999.f;
                for (auto* shape : m_selectedShapes) {
                    sf::FloatRect b = shape->getBounds();
                    if (b.position.x < actualMinX) actualMinX = b.position.x;
                    if (b.position.y < actualMinY) actualMinY = b.position.y;
                    if (b.position.x + b.size.x > actualMaxX) actualMaxX = b.position.x + b.size.x;
                    if (b.position.y + b.size.y > actualMaxY) actualMaxY = b.position.y + b.size.y;
                }

                sf::Vector2f actualFixedCorner;
                if (cornerType == 1) actualFixedCorner = { actualMaxX, actualMaxY };
                else if (cornerType == 2) actualFixedCorner = { actualMinX, actualMaxY };
                else if (cornerType == 3) actualFixedCorner = { actualMinX, actualMinY };
                else if (cornerType == 4) actualFixedCorner = { actualMaxX, actualMinY };

                sf::Vector2f correction = fixedCorner - actualFixedCorner;
                for (auto* shape : m_selectedShapes) {
                    shape->setPosition(shape->getPosition() + correction);
                }

                ShapeGroup* formalGroup = getFormalSelectedGroup();
                if (formalGroup) {
                    sf::Vector2f dist = m_dragStartFormalGroupAnchor - fixedCorner;
                    formalGroup->anchorPoint = fixedCorner + sf::Vector2f(dist.x * Sx, dist.y * Sy) + correction;
                }
            }
        }

        // =========================================================================
        // 3. ПЕРЕТАСКИВАНИЕ ЯКОРЯ ОДИНОЧНОЙ ФИГУРЫ
        // =========================================================================
        else if (m_draggedHandle == 5 && m_selectedShapes.size() == 1) {
            m_selectedShapes[0]->setAnchorPositionWorld(mousePos);
        }

        // =========================================================================
        // 4. ПЕРЕТАСКИВАНИЕ ЯКОРЯ ГРУППЫ
        // =========================================================================
        else if (m_draggedHandle == 6) {
            ShapeGroup* formalGroup = getFormalSelectedGroup();
            if (formalGroup) {
                formalGroup->anchorPoint = mousePos;
            }
        }

        // =========================================================================
        // 5. ИДЕАЛЬНОЕ ВРАЩЕНИЕ
        // =========================================================================
        else if (m_draggedHandle == 7 && m_selectedShapes.size() == 1) {
            m_selectedShapes[0]->setRotationFromMouse(mousePos);
        }

        // =========================================================================
        // 6. ПЕРЕМЕЩЕНИЕ ВЕРШИН (УЗЛОВ) ФИГУРЫ
        // =========================================================================
        else if (m_draggedHandle >= 100 && m_selectedShapes.size() == 1) {
            int vertexIndex = m_draggedHandle - 100;
            sf::Vector2f delta = mousePos - m_lastMousePos;
            m_selectedShapes[0]->moveVertex(vertexIndex, delta);
            m_lastMousePos = mousePos;
        }

        // =========================================================================
        // 7. ПЕРЕМЕЩЕНИЕ САМОЙ ФИГУРЫ ИЛИ ГРУППЫ
        // =========================================================================
        else if (m_draggedHandle == 0 && !m_isBoxSelecting) {
            sf::Vector2f delta = mousePos - m_lastMousePos;
            for (auto* shape : m_selectedShapes) {
                shape->setPosition(shape->getPosition() + delta);
            }

            // Если перемещаем группу целиком - смещаем и её якорь, чтобы он не отстал
            if (m_selectedShapes.size() > 1) {
                ShapeGroup* formalGroup = getFormalSelectedGroup();
                if (formalGroup) {
                    formalGroup->anchorPoint += delta;
                }
            }

            m_lastMousePos = mousePos;
        }

    }
    else if (m_isBoxSelecting) {
        // Обновляем текущую позицию для отрисовки рамки выделения
        m_currentMousePos = mousePos;
    }
}

// ==========================================
// ЛОГИКА ГРУППИРОВКИ
// ==========================================
void Scene::groupSelected() {
    if (m_selectedShapes.size() < 2) return;

    // Ищем уже существующие группы в выделении (Как в твоем C# коде)
    std::vector<std::string> existingGroups;
    for (auto* s : m_selectedShapes) {
        std::string gid = s->getGroupId();
        if (!gid.empty() && std::find(existingGroups.begin(), existingGroups.end(), gid) == existingGroups.end()) {
            existingGroups.push_back(gid);
        }
    }

    // Защита: нельзя сгруппировать одну и ту же группу дважды, если больше ничего не выделено
    if (existingGroups.size() == 1) {
        int shapesInGroup = 0;
        for (auto& s : m_shapes) if (s->getGroupId() == existingGroups[0]) shapesInGroup++;
        if (shapesInGroup == m_selectedShapes.size()) return;
    }

    std::string newId = "Group_" + std::to_string(std::rand());
    std::string newName = "Group " + std::to_string(m_groups.size() + 1);

    // Умное именование
    if (existingGroups.size() == 1) {
        for (auto& g : m_groups) if (g.id == existingGroups[0]) { newName = g.name; break; }
    }
    else if (existingGroups.size() > 1) {
        newName = "";
        for (size_t i = 0; i < existingGroups.size(); i++) {
            for (auto& g : m_groups) if (g.id == existingGroups[i]) { newName += g.name; break; }
            if (i < existingGroups.size() - 1) newName += " + ";
        }
    }

    // Рассчитываем центр для якоря
    float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
    for (auto* shape : m_selectedShapes) {
        sf::FloatRect bounds = shape->getBounds();
        if (bounds.position.x < minX) minX = bounds.position.x;
        if (bounds.position.y < minY) minY = bounds.position.y;
        if (bounds.position.x + bounds.size.x > maxX) maxX = bounds.position.x + bounds.size.x;
        if (bounds.position.y + bounds.size.y > maxY) maxY = bounds.position.y + bounds.size.y;
    }

    ShapeGroup newGroup;
    newGroup.id = newId;
    newGroup.name = newName;
    newGroup.anchorPoint = sf::Vector2f(minX + (maxX - minX) / 2.0f, minY + (maxY - minY) / 2.0f);

    m_groups.push_back(newGroup);
    for (auto* shape : m_selectedShapes) shape->setGroupId(newId);

    cleanupEmptyGroups(); // Удаляем старые слившиеся группы
}
void Scene::selectGroup(const std::string& groupId) {
    for (auto& shapePtr : m_shapes) {
        if (shapePtr->getGroupId() == groupId) {
            if (std::find(m_selectedShapes.begin(), m_selectedShapes.end(), shapePtr.get()) == m_selectedShapes.end()) {
                shapePtr->setSelected(true);
                m_selectedShapes.push_back(shapePtr.get());
            }
        }
    }
}

void Scene::ungroupSelected(const std::string& groupId) {
    for (auto* shape : m_selectedShapes) {
        if (shape->getGroupId() == groupId) shape->setGroupId("");
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(),
        [&](const ShapeGroup& g) { return g.id == groupId; }), m_groups.end());
}
// РЕАЛИЗАЦИЯ Z-INDEX
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
void Scene::cleanupEmptyGroups() {
    for (auto it = m_groups.begin(); it != m_groups.end(); ) {
        int count = 0;
        for (auto& s : m_shapes) { if (s->getGroupId() == it->id) count++; }
        if (count <= 1) {
            for (auto& s : m_shapes) { if (s->getGroupId() == it->id) s->setGroupId(""); }
            it = m_groups.erase(it);
        }
        else {
            ++it;
        }
    }
}
ShapeGroup* Scene::getFormalSelectedGroup() {
    if (m_selectedShapes.empty()) return nullptr;
    std::string firstId = m_selectedShapes[0]->getGroupId();
    if (firstId.empty()) return nullptr;

    for (auto* s : m_selectedShapes) if (s->getGroupId() != firstId) return nullptr;

    int count = 0;
    for (auto& s : m_shapes) if (s->getGroupId() == firstId) count++;
    if (count != m_selectedShapes.size()) return nullptr; // Выделена не вся группа

    for (auto& g : m_groups) if (g.id == firstId) return &g;
    return nullptr;
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

    // Удаляем из общего массива m_shapes те, которые есть в m_selectedShapes
    m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
        [this](const std::unique_ptr<IShape>& shape) {
            return std::find(m_selectedShapes.begin(), m_selectedShapes.end(), shape.get()) != m_selectedShapes.end();
        }), m_shapes.end());

    m_selectedShapes.clear();
    cleanupEmptyGroups(); // Удаляем группы, если они опустели
}

void Scene::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return;

    // Сохраняем группы
    out << "GroupsCount: " << m_groups.size() << "\n";
    for (const auto& g : m_groups) {
        std::string safeName = g.name;
        std::replace(safeName.begin(), safeName.end(), ' ', '_'); // Заменяем пробелы для безопасного чтения
        out << "Group: " << g.id << " " << safeName << " " << g.anchorPoint.x << " " << g.anchorPoint.y << "\n";
    }

    // Сохраняем фигуры
    out << "\nShapesCount: " << m_shapes.size() << "\n";
    for (const auto& shape : m_shapes) {
        // Оборачиваем тип в скобки для красоты, например [Polygon]
        out << "\n[" << shape->getType() << "]\n";
        shape->save(out);
    }
}

void Scene::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;

    clear(); // Очищаем текущую сцену

    std::string dummy; // Переменная-пустышка для "съедания" текстовых маркеров
    size_t groupCount = 0;

    // Читаем "GroupsCount:" и число
    if (!(in >> dummy >> groupCount)) return;

    for (size_t i = 0; i < groupCount; ++i) {
        ShapeGroup g;
        in >> dummy >> g.id >> g.name >> g.anchorPoint.x >> g.anchorPoint.y; // dummy съедает слово "Group:"
        std::replace(g.name.begin(), g.name.end(), '_', ' '); // Возвращаем пробелы
        m_groups.push_back(g);
    }

    size_t shapeCount = 0;
    in >> dummy >> shapeCount; // Съедает "ShapesCount:"

    // Фабрика фигур
    for (size_t i = 0; i < shapeCount; ++i) {
        std::string typeStr;
        in >> typeStr; // Читает маркер типа, например "[Polygon]"

        std::unique_ptr<IShape> shape;
        if (typeStr == "[Polygon]") {
            shape = std::make_unique<PolygonShape>(sf::Vector2f(0, 0), sf::Vector2f(10, 10), true, std::vector<sf::Vector2f>{});
        }
        else if (typeStr == "[Circle]") {
            shape = std::make_unique<CircleShape>(sf::Vector2f(0, 0), sf::Vector2f(10, 10));
        }

        if (shape) {
            shape->load(in); // Фигура сама считает свои параметры
            m_shapes.push_back(std::move(shape));
        }
    }
}