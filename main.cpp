#include <SFML/Graphics.hpp>
#include "imgui.h"
#include "imgui-SFML.h"

// Подключаем нашу архитектуру
#include "Scene.h"
#include "PolygonShape.h"
#include "CircleShape.h"

int main() {
    // Создаем окно (синтаксис SFML 3.0)
    sf::RenderWindow window(sf::VideoMode({ 1200, 800 }), "Graphic Editor (C++ / SFML)");
    window.setFramerateLimit(60);

    // Инициализация интерфейса
    if (!ImGui::SFML::Init(window)) return -1;
    // --- ДОБАВЛЯЕМ ПОДДЕРЖКУ РУССКОГО ЯЗЫКА ---
    ImGuiIO& io = ImGui::GetIO();
    // Загружаем шрифт Arial из папки Windows. Размер 18 пикселей. Подключаем кириллицу.
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 12.f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    // Обязательно обновляем текстуру шрифта, чтобы SFML его увидел!
    ImGui::SFML::UpdateFontTexture();
    // -----------------------------------------
    // Создаем нашу сцену (замена MainCanvas)
    Scene scene;

    sf::Clock deltaClock;
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            // --- НОВОЕ: Фикс искажения пропорций (чтобы круг не был овалом) ---
            if (const auto* resize = event->getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea({ 0.f, 0.f }, { static_cast<float>(resize->size.x), static_cast<float>(resize->size.y) });
                window.setView(sf::View(visibleArea));
            }
            // --- ПРАВИЛЬНАЯ ОБРАБОТКА МЫШИ (Без блокировок от ImGui) ---
            if (const auto* mousePress = event->getIf<sf::Event::MouseButtonPressed>()) {
                // Игнорируем нажатие, только если курсор находится ПРЯМО НАД окном ImGui
                if (mousePress->button == sf::Mouse::Button::Left && !ImGui::GetIO().WantCaptureMouse) {
                    bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

                    scene.handleMousePress(window.mapPixelToCoords(mousePress->position), isShift, isCtrl);
                }
            }
            else if (const auto* mouseRelease = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseRelease->button == sf::Mouse::Button::Left) {
                    bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                    // Отпускаем мышь ВСЕГДА, чтобы рамка выделения не "залипала"
                    scene.handleMouseRelease(isShift);
                }
            }
            else if (const auto* mouseMove = event->getIf<sf::Event::MouseMoved>()) {
                // Передаем координаты движения ВСЕГДА
                scene.handleMouseMove(window.mapPixelToCoords(mouseMove->position));
            }

            // --- НОВОЕ: ОБРАБОТКА КЛАВИАТУРЫ (Вернули Ctrl+G) ---
            else if (const auto* keyPress = event->getIf<sf::Event::KeyPressed>()) {
                // Завершить как открытую ломаную (Enter)
                if (keyPress->code == sf::Keyboard::Key::Enter) scene.finishDrawing(false);
                // Завершить как замкнутую фигуру (Tab)
                if (keyPress->code == sf::Keyboard::Key::Tab) scene.finishDrawing(true);
                // Отмена (Escape)
                if (keyPress->code == sf::Keyboard::Key::Escape) scene.cancelDrawing();
                //удаление
                if (keyPress->code == sf::Keyboard::Key::Delete && !ImGui::GetIO().WantTextInput) {
                    scene.deleteSelected();
                }
                // Проверяем, что нажата G и зажат любой Ctrl (левый или правый)
                if (keyPress->code == sf::Keyboard::Key::G && keyPress->control) {
                    // Группируем ТОЛЬКО если курсор не в поле ввода текста ImGui
                    if (!ImGui::GetIO().WantTextInput) {
                        scene.groupSelected();
                    }
                }
            }
            // ----------------------------------------------------
        }
        ImGui::SFML::Update(window, deltaClock.restart());

        // ==========================================
         // UI: ПАНЕЛЬ ИНСТРУМЕНТОВ
         // ==========================================
        ImGui::Begin("Tools");
        // --- НОВЫЙ БЛОК: СОХРАНЕНИЕ И УДАЛЕНИЕ ---
        ImGui::Text("File & Edit:");
        if (ImGui::Button("Save Scene", ImVec2(-1, 0))) {
            scene.saveToFile("scene_save.txt");
        }
        if (ImGui::Button("Load Scene", ImVec2(-1, 0))) {
            scene.loadFromFile("scene_save.txt");
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Красная кнопка
        if (ImGui::Button("Delete Selected (Del)", ImVec2(-1, 0))) {
            scene.deleteSelected();
        }
        ImGui::PopStyleColor();
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
        ImGui::Separator();

        if (ImGui::Button("Clear Canvas", ImVec2(-1, 0))) {
            scene.clear();
        }
        ImGui::End();

        // ==========================================
        // UI: ИНСПЕКТОР СВОЙСТВ И ГРУПП
        // ==========================================
        ImGui::Begin("Properties Inspector");

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
                sf::FloatRect bounds = selected[0]->getBounds();
                // Находим центр виртуальной рамки
                sf::Vector2f center(bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f);
                // Якорь - это смещение относительно m_position
                selected[0]->setAnchorOffset(center - selected[0]->getPosition());
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