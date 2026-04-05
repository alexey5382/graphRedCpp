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

    if (m_isDragging) {
        activeHandle = m_draggedHandle;
    }
    else {
        // --- НОВОЕ: Ищем наведение на углы группы ---
        if (m_selectedShapes.size() > 1) {
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

        // Если не попали в группу, ищем маркеры фигур (старый код)
        if (activeHandle == 0) {
            for (auto* shape : m_selectedShapes) {
                activeHandle = shape->getHitHandle(mousePos);
                if (activeHandle != 0) break;
                if (shape->getHitSideIndex(mousePos) != -1) { hoverSide = true; break; }
            }
            if (activeHandle == 0 && !hoverSide) {
                for (auto* shape : m_selectedShapes) {
                    if (shape->contains(mousePos)) { hoverShape = true; break; }
                }
            }
        }
    }

    // Разыменовываем optional через * перед передачей в окно
    // Разыменовываем optional
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
    for (const auto& shape : m_shapes) shape->draw(target);
    for (const auto& shape : m_shapes) shape->drawSelection(target);

    // --- ГЛОБАЛЬНАЯ РАМКА И ОРАНЖЕВЫЙ ЯКОРЬ ---
    if (m_selectedShapes.size() > 1) {
        float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
        for (auto* shape : m_selectedShapes) {
            sf::FloatRect bounds = shape->getBounds();
            if (bounds.position.x < minX) minX = bounds.position.x;
            if (bounds.position.y < minY) minY = bounds.position.y;
            if (bounds.position.x + bounds.size.x > maxX) maxX = bounds.position.x + bounds.size.x;
            if (bounds.position.y + bounds.size.y > maxY) maxY = bounds.position.y + bounds.size.y;
        }

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
        

        
        // Ищем, является ли это полноценной группой
        std::string firstId = m_selectedShapes[0]->getGroupId();
        bool isFormalGroup = !firstId.empty();
        for (auto* s : m_selectedShapes) {
            if (s->getGroupId() != firstId) { isFormalGroup = false; break; }
        }

        // --- ИСПРАВЛЕНИЕ: Дополнительно проверяем, что выделена ВСЯ группа ---
        if (isFormalGroup) {
            size_t count = 0;
            for (const auto& s : m_shapes) { if (s->getGroupId() == firstId) count++; }
            if (count != m_selectedShapes.size()) isFormalGroup = false;
        }

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

    // Рамка выделения (Drag Selection)
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
    if (m_isDrawingMode && !m_drawingPoints.empty()) {
        // Рисуем уже поставленные отрезки
        sf::VertexArray lines(sf::PrimitiveType::LineStrip, m_drawingPoints.size() + 1);
        for (size_t i = 0; i < m_drawingPoints.size(); i++) {
            lines[i].position = m_drawingPoints[i];
            lines[i].color = sf::Color::White;
        }
        // Линия тянется к курсору
        lines[m_drawingPoints.size()].position = m_currentMousePos;
        lines[m_drawingPoints.size()].color = sf::Color(255, 255, 255, 150); // Полупрозрачная

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
    // --- 1. Проверяем маркеры масштабирования ГРУППЫ ---
    if (m_selectedShapes.size() > 1) {
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

            // --- ДЕЛАЕМ СНИМОК ГРУППЫ ---
            m_dragStartGroupBounds = sf::FloatRect({ minX, minY }, { maxX - minX, maxY - minY });
            m_dragStartSnapshots.clear(); m_dragStartBounds.clear(); m_dragStartAnchors.clear();

            for (auto* shape : m_selectedShapes) {
                m_dragStartSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation() });
                m_dragStartBounds.push_back(shape->getBounds());
                m_dragStartAnchors.push_back(shape->getPosition() + shape->getAnchorOffset());
            }
            ShapeGroup* fg = getFormalSelectedGroup();
            if (fg) m_dragStartFormalGroupAnchor = fg->anchorPoint;
            return;
        }
    }
    // 2. Проверяем ОРАНЖЕВЫЙ ЯКОРЬ группы

    if (formalGroup) {
        float hw = 7.0f;
        if (mousePos.x >= formalGroup->anchorPoint.x - hw && mousePos.x <= formalGroup->anchorPoint.x + hw &&
            mousePos.y >= formalGroup->anchorPoint.y - hw && mousePos.y <= formalGroup->anchorPoint.y + hw) {
            m_isDragging = true;
            m_draggedHandle = 6; // 6 - Код глобального якоря группы
            m_lastMousePos = mousePos;
            return;
        }
    }

    // 2. Проверяем маркеры выделенных фигур
    for (auto* shape : m_selectedShapes) {
        int handle = shape->getHitHandle(mousePos);
        if (handle != 0) {
            m_draggedHandle = handle;
            if (handle >= 100) { shape->setSelectedVertex(handle - 100); shape->setSelectedSide(-1); }
            else { shape->setSelectedVertex(-1); }

            m_isDragging = true;
            m_lastMousePos = mousePos;

            // --- ДЕЛАЕМ СНИМОК ОДИНОЧНОЙ ФИГУРЫ (углы 1-4) ---
            if (handle >= 1 && handle <= 4) {
                m_dragStartSnapshots.clear();
                m_dragStartSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation() });
                m_dragStartBounds.clear();
                m_dragStartBounds.push_back(shape->getBounds());
                m_dragStartAnchors.clear();
                m_dragStartAnchors.push_back(shape->getPosition() + shape->getAnchorOffset());
                m_dragStartGroupBounds = shape->getBounds(); // Для одиночной фигуры группа = ее собственная рамка
            }
            return;
        }

        // Если не попали в маркер, проверяем клик по контуру (стороне)
        int sideIndex = shape->getHitSideIndex(mousePos);
        if (sideIndex != -1) {
            shape->setSelectedSide(sideIndex);
            shape->setSelectedVertex(-1); // <-- Добавь сброс вершины при выборе стороны
            return;
        }

        // Кликнули просто в тело фигуры
        if (shape->contains(mousePos)) {
            shape->setSelectedSide(-1);
            shape->setSelectedVertex(-1); // <-- Добавь сброс
        }
    }

    // 3. Ищем фигуру
    IShape* clickedShape = nullptr;
    for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
        if ((*it)->contains(mousePos)) { clickedShape = it->get(); break; }
    }

    if (clickedShape) {
        std::string gId = clickedShape->getGroupId();

        // 1. Проверяем, кликнули ли мы по УЖЕ выделенной фигуре
        bool sideClickHandled = false;
        auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clickedShape);

        if (it != m_selectedShapes.end() && !isCtrlPressed && !isShiftPressed && m_selectedShapes.size() == 1) {
            // Фигура УЖЕ была выделена -> второй клик выделяет конкретную сторону
            int hitSide = clickedShape->getHitSideIndex(mousePos);
            clickedShape->setSelectedSide(hitSide); // Если промахнулся по стороне, сбросится на -1
            sideClickHandled = true;
        }

        // Если это клик по новой фигуре, сбрасываем стороны у старых
        if (!sideClickHandled) {
            for (auto* s : m_selectedShapes) s->setSelectedSide(-1);
        }
        // Если зажат Ctrl И фигура в группе -> выделяем группу
        if (!gId.empty() && isCtrlPressed) {
            bool isGroupFullySelected = true;
            std::vector<IShape*> groupShapes;
            for (auto& s : m_shapes) {
                if (s->getGroupId() == gId) {
                    groupShapes.push_back(s.get());
                    if (std::find(m_selectedShapes.begin(), m_selectedShapes.end(), s.get()) == m_selectedShapes.end()) isGroupFullySelected = false;
                }
            }

            if (isShiftPressed) {
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
                clearSelection();
                selectGroup(gId);
            }
        }
        else {
            // Обычный клик: Выделяем ТОЛЬКО саму фигуру (даже в группе)
            auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clickedShape);
            if (isShiftPressed) {
                if (it != m_selectedShapes.end()) { clickedShape->setSelected(false); m_selectedShapes.erase(it); }
                else { clickedShape->setSelected(true); m_selectedShapes.push_back(clickedShape); }
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
        if (!isShiftPressed) clearSelection(); // ИСПРАВЛЕН SHIFT ДЛЯ РАМКИ!
        m_isBoxSelecting = true;
        m_selectionStartPos = mousePos;
        m_lastMousePos = mousePos;
    }

}
void Scene::handleMouseRelease(bool isShiftPressed) {
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
        // Проверяем глобально, зажат ли Shift
        bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

        if (isShift && !m_drawingPoints.empty()) {
            sf::Vector2f lastPt = m_drawingPoints.back();
            sf::Vector2f diff = mousePos - lastPt;

            float angle = std::atan2(diff.y, diff.x);
            float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);

            // Округляем до ближайшего угла, кратного 15 градусам (в радианах)
            const float snapRad = 15.0f * 3.14159265f / 180.0f;
            angle = std::round(angle / snapRad) * snapRad;

            // Пересчитываем позицию мыши с учетом нового угла и старой длины
            mousePos = lastPt + sf::Vector2f(std::cos(angle) * len, std::sin(angle) * len);
        }

        m_currentMousePos = mousePos; // Запоминаем для линии-превью
        return;
    }
    if (m_isBoxSelecting) {
        m_lastMousePos = mousePos; // Растягиваем рамку
        return;
    }
    
    if (!m_isDragging || m_selectedShapes.empty()) return;
    sf::Vector2f delta = mousePos - m_lastMousePos;
    if (m_draggedHandle == 6) {
        ShapeGroup* formalGroup = getFormalSelectedGroup();
        if (formalGroup) formalGroup->anchorPoint += delta;
    }
    else if (m_draggedHandle == 7) {
        // --- ВРАЩЕНИЕ МЫШЬЮ ЗА ЗЕЛЕНЫЙ КРУГ ---
        m_selectedShapes[0]->setRotationFromMouse(mousePos);
    }
    else if (m_draggedHandle == 5) {
        // --- ТЯНЕМ ЯКОРЬ БЕЗ ИСКАЖЕНИЙ ---
        sf::Vector2f currentWorldAnchor = m_selectedShapes[0]->getPosition() + m_selectedShapes[0]->getAnchorOffset();
        m_selectedShapes[0]->setAnchorPositionWorld(currentWorldAnchor + delta);
    }
    else if (m_draggedHandle == 0) {
        // --- ДВИГАЕМ ФИГУРЫ ---
        for (auto* shape : m_selectedShapes) shape->setPosition(shape->getPosition() + delta);

        // --- ИСПРАВЛЕНИЕ: ЯКОРЬ ДВИГАЕТСЯ ВМЕСТЕ С ГРУППОЙ! ---
        ShapeGroup* formalGroup = getFormalSelectedGroup();
        if (formalGroup) formalGroup->anchorPoint += delta;
    }
    else if (m_draggedHandle >= 100) {
        m_selectedShapes[0]->moveVertex(m_draggedHandle - 100, delta);
    }
    else if ((m_draggedHandle >= 1 && m_draggedHandle <= 4 && m_selectedShapes.size() == 1) ||
        (m_draggedHandle >= 21 && m_draggedHandle <= 24 && m_selectedShapes.size() > 1)) {

        // 1. Определяем, какой угол тянем (TL, TR, BR, BL)
        int cornerType = 0;
        if (m_draggedHandle == 1 || m_draggedHandle == 21) cornerType = 1;      // Top-Left
        else if (m_draggedHandle == 2 || m_draggedHandle == 22) cornerType = 2; // Top-Right
        else if (m_draggedHandle == 3 || m_draggedHandle == 23) cornerType = 3; // Bottom-Right
        else if (m_draggedHandle == 4 || m_draggedHandle == 24) cornerType = 4; // Bottom-Left

        // Берем начальные размеры из идеального снимка
        float oldW = m_dragStartGroupBounds.size.x;
        float oldH = m_dragStartGroupBounds.size.y;
        if (oldW < 0.1f || oldH < 0.1f) return;

        // Строго фиксируем противоположный угол по снимку
        sf::Vector2f fixedCorner;
        if (cornerType == 1) fixedCorner = { m_dragStartGroupBounds.position.x + oldW, m_dragStartGroupBounds.position.y + oldH };
        else if (cornerType == 2) fixedCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y + oldH };
        else if (cornerType == 3) fixedCorner = { m_dragStartGroupBounds.position.x, m_dragStartGroupBounds.position.y };
        else if (cornerType == 4) fixedCorner = { m_dragStartGroupBounds.position.x + oldW, m_dragStartGroupBounds.position.y };

        // Рассчитываем новые границы
        float newMinX = m_dragStartGroupBounds.position.x, newMaxX = m_dragStartGroupBounds.position.x + oldW;
        float newMinY = m_dragStartGroupBounds.position.y, newMaxY = m_dragStartGroupBounds.position.y + oldH;

        if (cornerType == 1) { newMinX = mousePos.x; newMinY = mousePos.y; }
        else if (cornerType == 2) { newMaxX = mousePos.x; newMinY = mousePos.y; }
        else if (cornerType == 3) { newMaxX = mousePos.x; newMaxY = mousePos.y; }
        else if (cornerType == 4) { newMinX = mousePos.x; newMaxY = mousePos.y; }

        // Защита от выворачивания рамки
        if (newMinX > newMaxX - 10.f) { if (cornerType == 1 || cornerType == 4) newMinX = newMaxX - 10.f; else newMaxX = newMinX + 10.f; }
        if (newMinY > newMaxY - 10.f) { if (cornerType == 1 || cornerType == 2) newMinY = newMaxY - 10.f; else newMaxY = newMinY + 10.f; }

        float newW = std::clamp(newMaxX - newMinX, 10.0f, 10000.0f);
        float newH = std::clamp(newMaxY - newMinY, 10.0f, 10000.0f);

        // Абсолютный множитель масштаба
        float Sx = newW / oldW;
        float Sy = newH / oldH;

        for (size_t i = 0; i < m_selectedShapes.size(); i++) {
            auto* shape = m_selectedShapes[i];
            if (i >= m_dragStartSnapshots.size()) continue;

            const auto& snap = m_dragStartSnapshots[i];
            sf::FloatRect initialB = m_dragStartBounds[i];
            sf::Vector2f initialAnchor = m_dragStartAnchors[i];

            // --- МАГИЯ: Каждый кадр откатываем фигуру к идеалу ---
            shape->setPosition(snap.position);
            shape->setSize(snap.size);
            shape->setAnchorOffset(snap.anchorOffset);
            shape->setRotation(snap.rotation);

            // 1. Увеличиваем размер самой фигуры (относительно её начальных габаритов)
            shape->resizeFromBoundingBox(initialB.size.x * Sx, initialB.size.y * Sy);

            // 2. Высчитываем новую позицию якоря (расстояние умножается на масштаб)
            sf::Vector2f dist = initialAnchor - fixedCorner;
            sf::Vector2f targetAnchor = fixedCorner + sf::Vector2f(dist.x * Sx, dist.y * Sy);

            // 3. Физически перемещаем всю фигуру на новое место
            sf::Vector2f currentAnchor = shape->getPosition() + shape->getAnchorOffset();
            sf::Vector2f shift = targetAnchor - currentAnchor;
            shape->setPosition(shape->getPosition() + shift);
        }

        // --- КОРРЕКЦИЯ: Чтобы фиксированный угол не сдвинулся ни на 0.001 пиксель ---
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

        // Идеально сдвигаем якорь формальной группы
        if (m_selectedShapes.size() > 1) {
            ShapeGroup* formalGroup = getFormalSelectedGroup();
            if (formalGroup) {
                sf::Vector2f dist = m_dragStartFormalGroupAnchor - fixedCorner;
                formalGroup->anchorPoint = fixedCorner + sf::Vector2f(dist.x * Sx, dist.y * Sy) + correction;
            }
        }
    }
    m_lastMousePos = mousePos;
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
}void Scene::selectGroup(const std::string& groupId) {
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

    // 1. Сохраняем группы
    out << m_groups.size() << "\n";
    for (const auto& g : m_groups) {
        // Заменяем пробелы в имени на '_', чтобы не сломать чтение
        std::string safeName = g.name;
        std::replace(safeName.begin(), safeName.end(), ' ', '_');
        out << g.id << " " << safeName << " " << g.anchorPoint.x << " " << g.anchorPoint.y << "\n";
    }

    // 2. Сохраняем фигуры (Тот самый полиморфизм!)
    out << m_shapes.size() << "\n";
    for (const auto& shape : m_shapes) {
        out << shape->getType() << "\n";
        shape->save(out);
    }
}

void Scene::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;

    clear(); // Очищаем текущую сцену

    size_t groupCount = 0;
    if (!(in >> groupCount)) return;

    for (size_t i = 0; i < groupCount; ++i) {
        ShapeGroup g;
        in >> g.id >> g.name >> g.anchorPoint.x >> g.anchorPoint.y;
        std::replace(g.name.begin(), g.name.end(), '_', ' '); // Возвращаем пробелы
        m_groups.push_back(g);
    }

    size_t shapeCount = 0;
    in >> shapeCount;

    // Фабрика фигур: читаем тип и создаем нужный объект
    for (size_t i = 0; i < shapeCount; ++i) {
        std::string type;
        in >> type;

        std::unique_ptr<IShape> shape;
        if (type == "Polygon") {
            // Создаем болванку, load() перепишет все её данные
            shape = std::make_unique<PolygonShape>(sf::Vector2f(0, 0), sf::Vector2f(1, 1), true, std::vector<sf::Vector2f>());
        }
        else if (type == "Circle") {
            shape = std::make_unique<CircleShape>(sf::Vector2f(0, 0), sf::Vector2f(1, 1));
        }

        if (shape) {
            shape->load(in); // Полиморфный вызов загрузки
            m_shapes.push_back(std::move(shape));
        }
    }
}