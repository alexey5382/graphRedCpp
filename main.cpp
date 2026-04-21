#include <SFML/Graphics.hpp>
#include "imgui.h"
#include "imgui-SFML.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

// Подключаем нашу архитектуру
#include "Scene.h"
#include "PolygonShape.h"
#include "CircleShape.h"
#include <filesystem> // <-- ДОБАВЛЕНО ДЛЯ РАБОТЫ С ПАПКАМИ
#include "Group.h"

namespace fs = std::filesystem; // Для удобства
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
    ImGui::StyleColorsLight(); // <-- ВКЛЮЧАЕМ СВЕТЛУЮ ТЕМУ IMGUI
    // Создаем папку для сохранений, если ее нет
    if (!fs::exists("Saves")) {
        fs::create_directory("Saves");
    }
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    // --- Переменные для инструмента "Правильный многоугольник" ---
    bool showRegularPolygonModal = false;
    int rpSides = 5;
    float rpLength = 50.0f;
    float rpFill[4] = { 0.8f, 0.8f, 0.8f, 0.5f };
    float rpStroke[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool rpRandomize = false;
    // --- Переменные для всплывающих окон сохранения/загрузки ---
    bool showSaveModal = false;
    bool saveOnlySelected = false;
    char saveFilename[128] = "my_scene";

    bool showLoadModal = false;
    int loadMode = 0; // 0 = Replace, 1 = Add
    std::string selectedLoadFile = "";
    // -----------------------------------------
    Scene scene;
    scene.initCursors();

    sf::Clock deltaClock;
    scene.resetView(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));

    while (window.isOpen()) {
        // ==========================================
        // ГОРЯЧИЕ КЛАВИШИ (Ctrl+C, Ctrl+V, Del)
        // ==========================================
        static bool cPressed = false;
        static bool vPressed = false;
        static bool delPressed = false;

        bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

        // Копирование (Ctrl + C)
        if (isCtrl && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C)) {
            if (!cPressed) { scene.copySelected(); cPressed = true; }
        }
        else { cPressed = false; }

        // Вставка (Ctrl + V)
        if (isCtrl && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V)) {
            if (!vPressed) { scene.paste(); vPressed = true; }
        }
        else { vPressed = false; }

        // Удаление (Delete) - Заодно добавим хоткей на удаление!
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete)) {
            if (!delPressed) { scene.deleteSelected(); delPressed = true; }
        }
        else { delPressed = false; }
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

        // Кнопка сохранения ВСЕЙ сцены
        if (ImGui::Button("Save Scene", ImVec2(140, 0))) {
            showSaveModal = true;
            saveOnlySelected = false;
        }
        ImGui::SameLine();

        // Кнопка сохранения ВЫДЕЛЕННОГО (активна только если есть выделение)
        ImGui::BeginDisabled(scene.getSelectedShapes().empty());
        if (ImGui::Button("Save Selected", ImVec2(140, 0))) {
            showSaveModal = true;
            saveOnlySelected = true;
        }
        ImGui::EndDisabled();

        // Кнопка загрузки
        if (ImGui::Button("Load Scene", ImVec2(-1, 0))) {
            showLoadModal = true;
            selectedLoadFile = "";
        }

        if (ImGui::Button("Delete Selected (Del)", ImVec2(-1, 0))) scene.deleteSelected();
        if (ImGui::Button("Clear Canvas", ImVec2(-1, 0))) scene.clear();

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
        if (ImGui::Button("New Regular Polygon", ImVec2(-1, 0))) {
            showRegularPolygonModal = true;
            rpRandomize = false; // Сбрасываем галочку при открытии
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

        // ==========================================
        // 1. ИЕРАРХИЯ СЦЕНЫ (Список всех объектов)
        // ==========================================
        if (ImGui::CollapsingHeader("Scene Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {

            // Лямбда для рекурсивной отрисовки дерева (работает как матрешка)
            auto drawNode = [&](auto& self, IShape* shape) -> void {
                ImGui::PushID(shape->getId());

                bool isGroup = (shape->getType() == "Group");
                ImVec4 color = isGroup ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
                const char* prefix = isGroup ? "Gr" : "ID";

                // Настраиваем логику отображения узла
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

                // Проверяем, выделена ли эта фигура прямо сейчас
                const auto& selected = scene.getSelectedShapes();
                if (std::find(selected.begin(), selected.end(), shape) != selected.end()) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                // Если это обычная фигура, убираем стрелочку раскрытия списка
                if (!isGroup) {
                    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                // Рисуем сам узел (текст с ID)
                bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)shape->getId(), flags, "[%s:%04d]", prefix, shape->getId());
                ImGui::PopStyleColor();

                // Обработка клика: выделяем фигуру при нажатии на текст
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                    if (!isShift) scene.clearSelection();
                    scene.selectShape(shape->getId());
                }

                // Поле ввода для переименования сразу справа от ID
                ImGui::SameLine();
                char sName[256];
                snprintf(sName, sizeof(sName), "%s", shape->getName().c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); // Растягиваем до конца панели
                if (ImGui::InputText("##sname", sName, sizeof(sName))) shape->setName(sName);

                // Если это группа И пользователь кликнул по стрелочке, чтобы её раскрыть:
                if (isGroup && nodeOpen) {
                    Group* group = dynamic_cast<Group*>(shape);
                    if (group) {
                        for (auto& childPtr : group->getChildren()) {
                            self(self, childPtr.get()); // Рекурсивно рисуем детей
                        }
                    }
                    ImGui::TreePop(); // Закрываем ветку
                }

                ImGui::PopID();
                };

            for (auto& shapePtr : scene.getShapes()) {
                drawNode(drawNode, shapePtr.get());
            }
        }
        ImGui::Separator();

        // ==========================================
        // 2. ИНФОРМАЦИЯ О ВЫДЕЛЕННОМ ОБЪЕКТЕ
        // ==========================================
        const auto& selected = scene.getSelectedShapes();

        if (selected.size() == 1) {
            auto* shape = selected[0];

            // --- УНИВЕРСАЛЬНАЯ ШАПКА ---
            ImVec4 headerColor = (shape->getType() == "Group") ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            ImGui::TextColored(headerColor, "Selected: %s", shape->getName().c_str());
            ImGui::TextDisabled("ID: %04d | Type: %s", shape->getId(), shape->getType().c_str());
            ImGui::Separator();

            // ВАЖНО: Вызов полиморфного метода.
            // Если это Круг - покажет радиус. Если Группа - покажет её параметры масштаба и список детей!
            shape->showImGuiProperties();

            ImGui::Separator();
            ImGui::Text("Z-Index:");
            if (ImGui::Button("Bring to Front")) scene.bringToFront(shape);
            ImGui::SameLine();
            if (ImGui::Button("Send to Back")) scene.sendToBack(shape);

            // Кнопка разгруппировки (только для объекта Group)
            if (shape->getType() == "Group") {
                ImGui::Separator();
                if (ImGui::Button("Ungroup", ImVec2(-1, 0))) {
                    scene.ungroupSelected(); // <--- ТЕПЕРЬ БЕЗ АРГУМЕНТОВ
                }
            }
        }
        else if (selected.size() > 1) {
            // Мультивыделение (когда зажат Shift)
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "Multiple Shapes Selected: %zu", selected.size());

            if (ImGui::Button("Group Selected", ImVec2(-1, 0))) {
                scene.groupSelected();
            }

            ImGui::Separator();
            ImGui::Text("Global Properties");

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

        // ==========================================
        // МОДАЛЬНЫЕ ОКНА СОХРАНЕНИЯ И ЗАГРУЗКИ
        // ==========================================

        // 1. Окно СОХРАНЕНИЯ
        if (showSaveModal) ImGui::OpenPopup("Save File##modal");

        // Центрируем окно на экране
        ImGui::SetNextWindowPos(ImVec2(winSize.x / 2.0f, winSize.y / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Save File##modal", &showSaveModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(saveOnlySelected ? "Saving SELECTED shapes..." : "Saving ENTIRE scene...");
            ImGui::Separator();

            ImGui::InputText("Filename", saveFilename, IM_ARRAYSIZE(saveFilename));
            ImGui::TextDisabled("File will be saved to 'Saves/' folder as .json");
            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                std::string fullPath = "Saves/" + std::string(saveFilename) + ".json";
                if (saveOnlySelected) scene.saveSelectedToFile(fullPath);
                else scene.saveToFile(fullPath);
                showSaveModal = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) showSaveModal = false;

            ImGui::EndPopup();
        }

        // 2. Окно ЗАГРУЗКИ
        if (showLoadModal) ImGui::OpenPopup("Load File##modal");

        ImGui::SetNextWindowPos(ImVec2(winSize.x / 2.0f, winSize.y / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Load File##modal", &showLoadModal, ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::RadioButton("Replace current scene", &loadMode, 0); ImGui::SameLine();
            ImGui::RadioButton("Add to current scene", &loadMode, 1);
            ImGui::Separator();

            ImGui::Text("Saved Files:");
            ImGui::BeginChild("Files", ImVec2(350, 200), true);

            // Читаем файлы из папки Saves
            if (fs::exists("Saves")) {
                for (const auto& entry : fs::directory_iterator("Saves")) {
                    if (entry.path().extension() == ".json") {
                        std::string fname = entry.path().filename().string();
                        bool isSelected = (selectedLoadFile == fname);
                        if (ImGui::Selectable(fname.c_str(), isSelected)) {
                            selectedLoadFile = fname;
                        }
                    }
                }
            }
            ImGui::EndChild();
            ImGui::Spacing();

            ImGui::BeginDisabled(selectedLoadFile.empty());
            if (ImGui::Button("Load", ImVec2(120, 0))) {
                scene.loadFromFile("Saves/" + selectedLoadFile, loadMode == 1);
                showLoadModal = false;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) showLoadModal = false;

            ImGui::EndPopup();
        }
        // ==========================================
        // 3. Окно ПРАВИЛЬНОГО МНОГОУГОЛЬНИКА
        // ==========================================
        if (showRegularPolygonModal) ImGui::OpenPopup("New Regular Polygon##modal");

        ImGui::SetNextWindowPos(ImVec2(winSize.x / 2.0f, winSize.y / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("New Regular Polygon##modal", &showRegularPolygonModal, ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Checkbox("Random Parameters", &rpRandomize);
            ImGui::Separator();

            // Если стоит галочка "Рандом", блокируем ручной ввод
            ImGui::BeginDisabled(rpRandomize);
            ImGui::SliderInt("Number of Sides", &rpSides, 3, 20);
            ImGui::InputFloat("Side Length (px)", &rpLength);
            ImGui::ColorEdit4("Fill Color", rpFill);
            ImGui::ColorEdit4("Stroke Color", rpStroke);
            ImGui::EndDisabled();

            ImGui::Spacing();

            if (ImGui::Button("Create", ImVec2(120, 0))) {
                int sides = rpSides;
                float length = rpLength;
                sf::Color fill(rpFill[0] * 255, rpFill[1] * 255, rpFill[2] * 255, rpFill[3] * 255);
                sf::Color stroke(rpStroke[0] * 255, rpStroke[1] * 255, rpStroke[2] * 255, rpStroke[3] * 255);

                // Генерируем случайные параметры
                if (rpRandomize) {
                    sides = 3 + std::rand() % 8; // От 3 до 10 сторон
                    length = 20.0f + static_cast<float>(std::rand() % 80); // Длина от 20 до 100
                    fill = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 100 + std::rand() % 100);
                    stroke = sf::Color(std::rand() % 256, std::rand() % 256, std::rand() % 256, 255);
                }

                // Защита от слишком маленькой длины
                if (length < 5.0f) length = 5.0f;

                // Математика: Радиус описанной окружности = L / (2 * sin(PI / N))
                const float PI = 3.14159265f;
                float radius = length / (2.0f * std::sin(PI / sides));

                // Создаем нормализованные точки правильного многоугольника
                std::vector<sf::Vector2f> basePoints;
                float startAngle = -PI / 2.0f; // Начинаем с верхней точки
                for (int i = 0; i < sides; ++i) {
                    float angle = startAngle + i * (2.0f * PI / sides);
                    basePoints.push_back({ std::cos(angle), std::sin(angle) });
                }

                // Размещаем фигуру по центру текущего обзора камеры
                sf::Vector2f center = scene.getScreenToWorld(sf::Vector2i(winSize.x / 2, winSize.y / 2), window);

                // Создаем фигуру (размер рамки = 2 радиуса)
                auto poly = std::make_unique<PolygonShape>(center, sf::Vector2f(radius * 2.0f, radius * 2.0f), true, basePoints);

                // Применяем цвета
                poly->setGlobalFillColor(fill);
                poly->setGlobalColor(stroke);

                scene.addShape(std::move(poly));

                showRegularPolygonModal = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) showRegularPolygonModal = false;

            ImGui::EndPopup();
        }
        ImGui::End();
        // ==========================================


        // Отрисовка
        window.clear(sf::Color(240, 240, 240)); // <-- СВЕТЛО-СЕРЫЙ ЦВЕТ ВМЕСТО ТЕМНОГО
        scene.draw(window);          // 1. Сначала рисуем холст с фигурами
        ImGui::SFML::Render(window); // 2. Затем поверх рисуем интерфейс

        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}