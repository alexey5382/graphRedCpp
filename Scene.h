#pragma once
#include <vector>
#include <memory>
#include <string>
#include <SFML/Graphics.hpp>
#include "IShape.h"
#include <SFML/Window/Cursor.hpp> // <-- ДОБАВЛЕНО
#include <optional>             // <-- ДОБАВИТЬ ЭТО ДЛЯ SFML 3

// 1. Обновляем структуру группы
struct ShapeGroup {
	int id;              // <-- Изменено на int
	std::string name;
	sf::Vector2f anchorPoint;
};

class Scene {
private:
	std::vector<std::unique_ptr<IShape>> m_shapes;
	std::vector<IShape*> m_selectedShapes;
	std::vector<ShapeGroup> m_groups;

	bool m_isDragging = false;
	int m_draggedHandle = 0;
	sf::Vector2f m_lastMousePos;

	// --- НОВОЕ: Переменные для рамки выделения ---
	bool m_isBoxSelecting = false;
	sf::Vector2f m_selectionStartPos;
	// Режим интерактивного рисования
	bool m_isDrawingMode = false;
	std::vector<sf::Vector2f> m_drawingPoints;
	sf::Vector2f m_currentMousePos;
	// --- ИСПРАВЛЕНИЕ: Оборачиваем курсоры в std::optional ---
	std::optional<sf::Cursor> m_cursorArrow;
	std::optional<sf::Cursor> m_cursorCross;
	std::optional<sf::Cursor> m_cursorHand;
	std::optional<sf::Cursor> m_cursorSizeTLBR;
	std::optional<sf::Cursor> m_cursorSizeBLTR;
	std::optional<sf::Cursor> m_cursorSizeAll;
	bool m_cursorsLoaded = false;
	// --- НОВОЕ: Переменные для снимков при масштабировании ---
	std::vector<ShapeSnapshot> m_dragStartSnapshots;
	std::vector<sf::FloatRect> m_dragStartBounds;
	std::vector<sf::Vector2f> m_dragStartAnchors;
	sf::FloatRect m_dragStartGroupBounds;
	sf::Vector2f m_dragStartFormalGroupAnchor;
	sf::Vector2f m_scaleMouseOffset; // <-- ДОБАВИТЬ ЭТО (Смещение мыши для прямой рамки)
	// --- НОВОЕ: Камера и сетка ---
	sf::View m_view;
	float m_zoom = 1.0f;
	bool m_isPanning = false;
	sf::Vector2i m_panStartMousePos;

	bool m_showGrid = true;
	float m_gridSize = 50.0f;
	//--
	int m_nextShapeId = 1; // <-- НОВЫЕ СЧЕТЧИКИ
	int m_nextGroupId = 1;

public:
	Scene();
	void addShape(std::unique_ptr<IShape> shape);
	void draw(sf::RenderTarget& target) const;
	void clear();

	// Геттеры для UI
	const std::vector<IShape*>& getSelectedShapes() const { return m_selectedShapes; }
	std::vector<ShapeGroup>& getGroups() { return m_groups; }
	const std::vector<std::unique_ptr<IShape>>& getShapes() const { return m_shapes; }

	// --- ОБНОВЛЕНО: Поддержка клавиши Shift ---
	void handleMousePress(sf::Vector2f mousePos, sf::Vector2i pixelPos, bool isShiftPressed, bool isCtrlPressed);
	void handleMouseRelease(bool isShiftPressed);
	void handleMouseMove(sf::Vector2f mousePos);

	void groupSelected();
	void ungroupSelected(int groupId);
	void selectGroup(int groupId);
	void selectShape(int id); // <-- Новый метод для клика из меню!
	void clearSelection();

	void bringToFront(IShape* shape);
	void sendToBack(IShape* shape);
	void cleanupEmptyGroups();
	ShapeGroup* getFormalSelectedGroup();
	void startDrawingMode();
	void finishDrawing(bool isClosed);
	void cancelDrawing();
	// в файл
	void deleteSelected();
	void saveToFile(const std::string& filename) const;
	void loadFromFile(const std::string& filename);
	// --- НОВОЕ: Методы курсора ---
	void initCursors();
	void updateCursor(sf::RenderWindow& window, sf::Vector2f mousePos);
	void resetCursor(sf::RenderWindow& window); // <-- ДОБАВИТЬ ЭТО
	// --- НОВОЕ: Методы камеры и сетки ---
	sf::Vector2f getScreenToWorld(sf::Vector2i pixelPos, const sf::RenderTarget& target) const;
	void updateViewSize(sf::Vector2f newSize);
	void handlePanStart(sf::Vector2i pixelPos);
	void handlePanMove(sf::Vector2i pixelPos, const sf::RenderTarget& target);
	void handlePanEnd();
	void handleZoom(float delta, sf::Vector2i pixelPos, const sf::RenderTarget& target);
	void resetView(sf::Vector2f windowSize);

	bool& getShowGridRef() { return m_showGrid; }
	float& getGridSizeRef() { return m_gridSize; }
	// Добавь эти две строки рядом с getShowGridRef()
	float getZoom() const { return m_zoom; }
	void setZoom(float newZoom, sf::Vector2f windowSize);
};