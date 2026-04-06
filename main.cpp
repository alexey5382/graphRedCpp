#include <SFML/Graphics.hpp>
#include "imgui.h"
#include "imgui-SFML.h"

// Подключаем нашу архитектуру
#include "Scene.h"
#include "PolygonShape.h"
#include "CircleShape.h"

int main() {
    // Создаем окно (синтаксис SFML 3.0)
    // Получаем разрешение рабочего стола
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    // Создаем окно на весь экран
    sf::RenderWindow window(desktop, "Graphic Editor (C++ / SFML)", sf::Style::Default, sf::State::Fullscreen);
    window.setFramerateLimit(60);

    // Инициализация интерфейса
    if (!ImGui::SFML::Init(window)) return -1;
    ImGuiIO& io = ImGui::GetIO();
    
    // -----------------------------------------
    // Создаем нашу сцену (замена MainCanvas)
    Scene scene;
    scene.initCursors(); // <-- ДОБАВИТЬ ЭТО

    sf::Clock deltaClock;
    scene.resetView(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            // Получаем мировые координаты мыши
            // 1. ОБНОВЛЕНИЕ КУРСОРА 
            sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
            if (!io.WantCaptureMouse) {
                sf::Vector2f worldPos = scene.getScreenToWorld(pixelPos, window);
                scene.updateCursor(window, worldPos);
            }
            else {
                scene.resetCursor(window);
            }

            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            // 2. ОБРАБОТКА ИЗМЕНЕНИЯ РАЗМЕРА ОКНА
            else if (const auto* resize = event->getIf<sf::Event::Resized>()) {
                scene.updateViewSize(sf::Vector2f(static_cast<float>(resize->size.x), static_cast<float>(resize->size.y)));
            }
            // 3. ЗУМ (Колесико мыши)
            else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (!io.WantCaptureMouse) {
                    scene.handleZoom(scroll->delta, scroll->position, window);
                }
            }
            // 4. НАЖАТИЯ МЫШИ
            else if (const auto* mousePress = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (!io.WantCaptureMouse) {
                    if (mousePress->button == sf::Mouse::Button::Middle) {
                        scene.handlePanStart(mousePress->position); // Начало панорамирования
                    }
                    else if (mousePress->button == sf::Mouse::Button::Left) {
                        bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                        bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

                        sf::Vector2f worldPos = scene.getScreenToWorld(mousePress->position, window);
                        // ВАЖНО: Теперь передаем mousePress->position (пиксели) вторым аргументом
                        scene.handleMousePress(worldPos, mousePress->position, isShift, isCtrl);
                    }
                }
            }
            // 5. ОТПУСКАНИЕ МЫШИ
            else if (const auto* mouseRelease = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseRelease->button == sf::Mouse::Button::Middle) {
                    scene.handlePanEnd();
                }
                else if (mouseRelease->button == sf::Mouse::Button::Left) {
                    bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                    scene.handlePanEnd(); // <-- ДОБАВЛЯЕМ ЭТО: останавливаем перемещение холста
                    scene.handleMouseRelease(isShift);
                }
            }
            // 6. ДВИЖЕНИЕ МЫШИ
            else if (const auto* mouseMove = event->getIf<sf::Event::MouseMoved>()) {
                // Передаем пиксели для перетаскивания холста
                scene.handlePanMove(mouseMove->position, window);

                // Если не в ImGui, двигаем фигуры с учетом камеры
                if (!io.WantCaptureMouse) {
                    sf::Vector2f worldPos = scene.getScreenToWorld(mouseMove->position, window);
                    scene.handleMouseMove(worldPos);
                }
            }
            // 7. КЛАВИАТУРА
            else if (const auto* keyPress = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPress->code == sf::Keyboard::Key::Enter) scene.finishDrawing(false);
                if (keyPress->code == sf::Keyboard::Key::Tab) scene.finishDrawing(true);
                if (keyPress->code == sf::Keyboard::Key::Escape) scene.cancelDrawing();
                if (keyPress->code == sf::Keyboard::Key::Delete && !io.WantTextInput) scene.deleteSelected();
                if (keyPress->code == sf::Keyboard::Key::G && keyPress->control && !io.WantTextInput) {
                    scene.groupSelected();
                }
            }
            // ----------------------------------------------------
        }
        ImGui::SFML::Update(window, deltaClock.restart());

        // ==========================================
         // UI: ПАНЕЛЬ ИНСТРУМЕНТОВ
         // ==========================================

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));

        // Начальный размер (ты сможешь тянуть её в ширину и высоту за правый нижний угол)
        ImGui::SetNextWindowSize(ImVec2(300.0f, 400.0f), ImGuiCond_FirstUseEver);

        // Блокируем перемещение и сворачивание, НО оставляем возможность менять размер
        ImGuiWindowFlags leftFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Tools", nullptr, leftFlags); // <-- Убедись, что передаешь флаги!

        //ImGui::Begin("Tools");
        // --- НОВЫЙ БЛОК: СОХРАНЕНИЕ И УДАЛЕНИЕ ---
        ImGui::Text("File & Edit:");
        if (ImGui::Button("Save Scene", ImVec2(-1, 0))) {
            scene.saveToFile("scene_save.txt");
        }
        if (ImGui::Button("Load Scene", ImVec2(-1, 0))) {
            scene.loadFromFile("scene_save.txt");
        }

        //ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Красная кнопка
        if (ImGui::Button("Delete Selected (Del)", ImVec2(-1, 0))) {
            scene.deleteSelected();
        }
        //ImGui::PopStyleColor();

        if (ImGui::Button("Clear Canvas", ImVec2(-1, 0))) {
            scene.clear();
        }

        // ==========================================
        // ГЛОБАЛЬНЫЕ НАСТРОЙКИ ХОЛСТА (КАМЕРА И СЕТКА)
        // ==========================================
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Canvas Properties");

        // Масштаб (отображаем в процентах для удобства пользователя: 1.0 = 100%)
        float currentZoom = scene.getZoom() * 100.0f;
        ImGui::SetNextItemWidth(150);
        if (ImGui::DragFloat("Zoom (%%)", &currentZoom, 1.0f, 5.0f, 1000.0f, "%.0f%%")) {
            scene.setZoom(currentZoom / 100.0f, sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
        }

        // Настройки сетки
        ImGui::Checkbox("Show Grid", &scene.getShowGridRef());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Grid Size", &scene.getGridSizeRef(), 1.0f, 10.0f, 500.0f, "%.0f px");

        if (ImGui::Button("Reset Camera (0,0)", ImVec2(-1, 0))) {
            scene.resetView(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
        }
        ImGui::Separator();
        // ------------------------------------------
        ImGui::Text("Create Shapes:");

        if (ImGui::Button("Add Rectangle", ImVec2(-1, 0))) {
            std::vector<sf::Vector2f> pts = { {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
            scene.addShape(std::make_unique<PolygonShape>(sf::Vector2f(600, 400), sf::Vector2f(100, 100), true, pts));
        }

        if (ImGui::Button("Add Triangle", ImVec2(-1, 0))) {
            std::vector<sf::Vector2f> pts = { {0, -1}, {1, 1}, {-1, 1} };
            scene.addShape(std::make_unique<PolygonShape>(sf::Vector2f(400, 400), sf::Vector2f(100, 100), true, pts));
        }

        if (ImGui::Button("Add Circle", ImVec2(-1, 0))) {
            scene.addShape(std::make_unique<CircleShape>(sf::Vector2f(800, 400), sf::Vector2f(100, 100)));
        }
        if (ImGui::Button("Add Trapezoid", ImVec2(-1, 0))) {
            // Точные координаты из твоего C# (MyTrapezoid)
            std::vector<sf::Vector2f> pts = { {-0.6f, -1.f}, {0.6f, -1.f}, {1.f, 1.f}, {-1.f, 1.f} };
            scene.addShape(std::make_unique<PolygonShape>(sf::Vector2f(600, 400), sf::Vector2f(100, 100), true, pts));
        }

        if (ImGui::Button("Add Pentagon", ImVec2(-1, 0))) {
            // Математика из твоего C# (MyPentagon)
            std::vector<sf::Vector2f> pts(5);
            for (int i = 0; i < 5; i++) {
                float angle = (i * 72.0f - 90.0f) * 3.14159265f / 180.0f;
                pts[i] = { std::cos(angle), std::sin(angle) };
            }
            scene.addShape(std::make_unique<PolygonShape>(sf::Vector2f(800, 400), sf::Vector2f(100, 100), true, pts));
        }
        if (ImGui::Button("Draw Polyline/Polygon", ImVec2(-1, 0))) {
            scene.startDrawingMode();
        }
        

        // --- НОВАЯ КНОПКА ВЫХОДА ---
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Красный цвет
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // Светло-красный при наведении
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f)); // Темно-красный при нажатии
        if (ImGui::Button("Exit", ImVec2(-1, 0))) {
            window.close(); // Закрываем окно SFML, что прервет главный цикл
        }
        ImGui::PopStyleColor(3); // Обязательно возвращаем цвета обратно!

        ImGui::Separator();
        ImGui::End();

        // ==========================================
        // UI: ИНСПЕКТОР СВОЙСТВ И ГРУПП
        // ==========================================

        sf::Vector2u winSize = window.getSize();

        // Примагничиваем правый верхний угол панели (точка опоры 1.0, 0.0) к правому краю экрана
        ImGui::SetNextWindowPos(ImVec2((float)winSize.x, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

        // СТРОГО фиксируем высоту (всегда равна экрану), а ширину разрешаем менять от 250 до 800
        ImGui::SetNextWindowSizeConstraints(ImVec2(250.0f, (float)winSize.y), ImVec2(800.0f, (float)winSize.y));

        // Начальная ширина панели при самом первом запуске
        ImGui::SetNextWindowSize(ImVec2(350.0f, (float)winSize.y), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags rightFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        

        ImGui::Begin("Properties", nullptr, rightFlags);


        //ImGui::Begin("Properties Inspector");

        // 1. ПАНЕЛЬ ГРУПП (Как в твоем C# коде)
        if (!scene.getGroups().empty()) {
            ImGui::Text("GROUPS:");
            for (auto& group : scene.getGroups()) {
                ImGui::PushID(group.id.c_str());

                // Поле для переименования группы
                char nameBuf[256];
                snprintf(nameBuf, sizeof(nameBuf), "%s", group.name.c_str());

                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) {
                    group.name = nameBuf;
                }

                ImGui::SameLine();
                if (ImGui::Button("Select")) {
                    scene.clearSelection();
                    scene.selectGroup(group.id);
                }

                ImGui::PopID();
            }
            ImGui::Separator();
        }

        // 2. ДИНАМИЧЕСКИЕ СВОЙСТВА ФИГУР
        const auto& selected = scene.getSelectedShapes();

        if (selected.size() == 1) {
            std::string gId = selected[0]->getGroupId();
            if (!gId.empty()) {
                std::string gName = "Unknown";
                for (auto& g : scene.getGroups()) if (g.id == gId) gName = g.name;

                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "In Group: %s", gName.c_str());
                if (ImGui::Button("Remove from Group", ImVec2(-1, 0))) {
                    selected[0]->setGroupId("");
                    scene.cleanupEmptyGroups();
                }
                ImGui::Separator();
            }

            // ВАЖНО: Показываем полное меню свойств фигуры в любом случае!
            selected[0]->showImGuiProperties();

            ImGui::Separator();
            ImGui::Text("Z-Index & Anchor:");
            if (ImGui::Button("Bring to Front")) scene.bringToFront(selected[0]);
            ImGui::SameLine();
            if (ImGui::Button("Send to Back")) scene.sendToBack(selected[0]);

            // --- ПРАВИЛЬНОЕ ЦЕНТРИРОВАНИЕ ЯКОРЯ ---
            if (ImGui::Button("Center Anchor", ImVec2(-1, 0))) {

                /*sf::FloatRect bounds = selected[0]->getBounds();
                // Находим центр виртуальной рамки
                sf::Vector2f center(bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f);
                // Якорь - это смещение относительно m_position
                selected[0]->setAnchorOffset(center - selected[0]->getPosition());*/

                // Центрирование якоря по визуальной рамке (AABB)
                sf::FloatRect bounds = selected[0]->getBounds();
                sf::Vector2f center(bounds.position.x + bounds.size.x / 2.0f,
                    bounds.position.y + bounds.size.y / 2.0f);
                selected[0]->setAnchorPositionWorld(center);
            }
        }
        else if (selected.size() > 1) {
            // МУЛЬТИ-ВЫДЕЛЕНИЕ ИЛИ ГРУППА
            ImGui::Text("Selected %zu shapes", selected.size());
            ImGui::Spacing();

            if (ImGui::Button("Group Selected (Ctrl+G)", ImVec2(-1, 0))) scene.groupSelected();

            ShapeGroup* formalGroup = scene.getFormalSelectedGroup();

            if (formalGroup) {
                if (ImGui::Button("Ungroup", ImVec2(-1, 0))) {
                    scene.ungroupSelected(formalGroup->id);
                    scene.cleanupEmptyGroups();
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "Group Anchor (World):");

                // Изменение глобального якоря группы
                float gAnchor[2] = { formalGroup->anchorPoint.x, formalGroup->anchorPoint.y };
                if (ImGui::DragFloat2("##gAnchor", gAnchor, 1.0f)) {
                    formalGroup->anchorPoint = { gAnchor[0], gAnchor[1] };
                }

                if (ImGui::Button("Center Anchor")) {
                    float minX = 99999.f, minY = 99999.f, maxX = -99999.f, maxY = -99999.f;
                    for (auto* shape : selected) {
                        sf::FloatRect bounds = shape->getBounds();
                        if (bounds.position.x < minX) minX = bounds.position.x;
                        if (bounds.position.y < minY) minY = bounds.position.y;
                        if (bounds.position.x + bounds.size.x > maxX) maxX = bounds.position.x + bounds.size.x;
                        if (bounds.position.y + bounds.size.y > maxY) maxY = bounds.position.y + bounds.size.y;
                    }
                    formalGroup->anchorPoint = { minX + (maxX - minX) / 2.0f, minY + (maxY - minY) / 2.0f };
                }

                // ==========================================
                // --- ИДЕАЛЬНОЕ МАСШТАБИРОВАНИЕ ГРУППЫ ---
                // ==========================================
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "Group Scale:");

                float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
                for (auto* shape : selected) {
                    sf::FloatRect b = shape->getBounds();
                    if (b.position.x < minX) minX = b.position.x;
                    if (b.position.y < minY) minY = b.position.y;
                    if (b.position.x + b.size.x > maxX) maxX = b.position.x + b.size.x;
                    if (b.position.y + b.size.y > maxY) maxY = b.position.y + b.size.y;
                }
                float currentGWidth = std::max(0.1f, maxX - minX);
                float currentGHeight = std::max(0.1f, maxY - minY);

                // Статическая память для снимков UI
                static bool isGroupUIScaling = false;
                static float baseGWidth = 1.0f, baseGHeight = 1.0f;
                static sf::Vector2f baseGroupAnchor;
                static sf::Vector2f baseGroupFixedCorner;
                static std::vector<ShapeSnapshot> uiSnapshots;

                float displayScale[2] = { currentGWidth, currentGHeight };

                ImGui::Text("Bounding Box Scale (W/H):");
                ImGui::SetNextItemWidth(150);
                bool changed1 = ImGui::SliderFloat2("##gScaleSl", displayScale, 10.0f, 2000.0f);
                bool act1 = ImGui::IsItemActivated(); bool deact1 = ImGui::IsItemDeactivated();
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                bool changed2 = ImGui::InputFloat2("##gScaleInp", displayScale, "%.1f");
                bool act2 = ImGui::IsItemActivated(); bool deact2 = ImGui::IsItemDeactivated();

                ImGui::Text("Relative Scale (%%):");
                float canvasDiag = std::sqrt(1200.0f * 1200.0f + 800.0f * 800.0f);
                float currentGDiag = std::sqrt(currentGWidth * currentGWidth + currentGHeight * currentGHeight);
                float displayRelScale = (currentGDiag / canvasDiag) * 500.0f;

                ImGui::SetNextItemWidth(150);
                bool changed3 = ImGui::SliderFloat("##gRelSl", &displayRelScale, 1.0f, 500.0f, "%.1f %%");
                bool act3 = ImGui::IsItemActivated(); bool deact3 = ImGui::IsItemDeactivated();
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                bool changed4 = ImGui::InputFloat("##gRelInp", &displayRelScale, 0.0f, 0.0f, "%.1f");
                bool act4 = ImGui::IsItemActivated(); bool deact4 = ImGui::IsItemDeactivated();

                bool anyAct = act1 || act2 || act3 || act4;
                bool anyDeact = deact1 || deact2 || deact3 || deact4;
                bool anyChanged = changed1 || changed2 || changed3 || changed4;

                // ФОТОГРАФИРУЕМ ГРУППУ
                if (anyAct) {
                    isGroupUIScaling = true;
                    baseGWidth = currentGWidth;
                    baseGHeight = currentGHeight;
                    baseGroupAnchor = formalGroup->anchorPoint;
                    baseGroupFixedCorner = { minX, minY };
                    uiSnapshots.clear();
                    for (auto* shape : selected) {
                        uiSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation(), shape->getBasePoints() });
                    }
                }

                // ПРИМЕНЯЕМ МАСШТАБ СТРОГО К СНИМКУ
                if (anyChanged) {
                    if (!isGroupUIScaling) {
                        isGroupUIScaling = true;
                        baseGWidth = currentGWidth;
                        baseGHeight = currentGHeight;
                        baseGroupAnchor = formalGroup->anchorPoint;
                        baseGroupFixedCorner = { minX, minY };
                        uiSnapshots.clear();
                        for (auto* shape : selected) {
                            uiSnapshots.push_back({ shape->getPosition(), shape->getSize(), shape->getAnchorOffset(), shape->getRotation(), shape->getBasePoints() });
                        }
                    }

                    float targetW = displayScale[0];
                    float targetH = displayScale[1];

                    if (changed3 || changed4) {
                        float targetDiag = (displayRelScale / 500.0f) * canvasDiag;
                        float baseDiag = std::sqrt(baseGWidth * baseGWidth + baseGHeight * baseGHeight);
                        float factor = targetDiag / baseDiag;
                        targetW = baseGWidth * factor;
                        targetH = baseGHeight * factor;
                    }

                    float Sx = targetW / baseGWidth;
                    float Sy = targetH / baseGHeight;

                    for (size_t i = 0; i < selected.size(); i++) {
                        if (i >= uiSnapshots.size()) continue;
                        auto* shape = selected[i];
                        const auto& snap = uiSnapshots[i];

                        shape->setPosition(snap.position);
                        shape->setSize(snap.size);
                        shape->setAnchorOffset(snap.anchorOffset);
                        shape->setRotation(snap.rotation);
                        if (!snap.basePoints.empty()) shape->setBasePoints(snap.basePoints);

                        // Используем НАШ НОВЫЙ МЕТОД глобального искажения для всей группы!
                        shape->applyGlobalScale(baseGroupFixedCorner, Sx, Sy);
                    }

                    // Масштабируем якорь самой группы
                    sf::Vector2f dist = baseGroupAnchor - baseGroupFixedCorner;
                    formalGroup->anchorPoint = baseGroupFixedCorner + sf::Vector2f(dist.x * Sx, dist.y * Sy);
                }

                if (anyDeact) {
                    isGroupUIScaling = false;
                }
                // ==========================================

                // Координаты фигур 
                if (ImGui::CollapsingHeader("Shapes Coordinates (World & Relative)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (size_t i = 0; i < selected.size(); i++) {
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Shape %zu", i + 1);

                        // 1. Мировые координаты
                        sf::Vector2f wPos = selected[i]->getPosition();
                        float w[2] = { wPos.x, wPos.y };
                        ImGui::SetNextItemWidth(150);
                        if (ImGui::InputFloat2("World Pos", w, "%.1f")) {
                            selected[i]->setPosition({ w[0], w[1] });
                        }

                        ImGui::SameLine();

                        // 2. Относительные координаты (от Якоря Группы до Якоря Фигуры)
                        sf::Vector2f shapeAnchorWorld = wPos + selected[i]->getAnchorOffset(); // Мировой якорь фигуры
                        sf::Vector2f rPos = shapeAnchorWorld - formalGroup->anchorPoint;       // Относительный вектор

                        float r[2] = { rPos.x, rPos.y };
                        ImGui::SetNextItemWidth(150);
                        if (ImGui::InputFloat2("Rel. Pos", r, "%.1f")) {
                            // Восстанавливаем мировой якорь фигуры из новых относительных координат
                            sf::Vector2f newShapeAnchorWorld = formalGroup->anchorPoint + sf::Vector2f(r[0], r[1]);
                            // Сдвигаем левый верхний угол (m_position) так, чтобы якорь оказался в нужном месте
                            selected[i]->setPosition(newShapeAnchorWorld - selected[i]->getAnchorOffset());
                        }

                        ImGui::Separator();
                        ImGui::PopID();
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Global Group Properties");

            static float sharedFill[4] = { 0.8f, 0.8f, 0.8f, 0.5f };
            if (ImGui::ColorEdit4("Fill Color", sharedFill)) {
                sf::Color c(sharedFill[0] * 255, sharedFill[1] * 255, sharedFill[2] * 255, sharedFill[3] * 255);
                for (auto* s : selected) s->setGlobalFillColor(c);
            }

            static float sharedColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            if (ImGui::ColorEdit4("Stroke Color", sharedColor)) {
                sf::Color c(sharedColor[0] * 255, sharedColor[1] * 255, sharedColor[2] * 255, sharedColor[3] * 255);
                for (auto* s : selected) s->setGlobalColor(c);
            }

            static float sharedThick = 2.0f;
            if (ImGui::SliderFloat("Thickness", &sharedThick, 1.0f, 50.0f)) {
                for (auto* s : selected) s->setGlobalThickness(sharedThick);
            }
        }
        else {
            ImGui::TextDisabled("Select a shape to edit properties.");
        }

        ImGui::End();
        // ==========================================

        // Отрисовка
        window.clear(sf::Color(37, 37, 38)); // Цвет фона окна, как в твоем WPF словаре (BgMain)

        scene.draw(window);          // 1. Сначала рисуем холст с фигурами
        ImGui::SFML::Render(window); // 2. Затем поверх рисуем интерфейс

        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}