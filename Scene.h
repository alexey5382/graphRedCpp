#pragma once
#include <vector>
#include <memory>
#include <string>
#include <SFML/Graphics.hpp>
#include "IShape.h"
#include <SFML/Window/Cursor.hpp>
#include <optional>

class Scene {
private:
	std::vector<std::unique_ptr<IShape>> m_shapes;
	std::vector<IShape*> m_selectedShapes;

	bool m_isDragging = false;
	int m_draggedHandle = 0;
	sf::Vector2f m_lastMousePos;

	bool m_isBoxSelecting = false;
	sf::Vector2f m_selectionStartPos;

	bool m_isDrawingMode = false;
	std::vector<sf::Vector2f> m_drawingPoints;
	sf::Vector2f m_currentMousePos;

	std::optional<sf::Cursor> m_cursorArrow;
	std::optional<sf::Cursor> m_cursorCross;
	std::optional<sf::Cursor> m_cursorHand;
	std::optional<sf::Cursor> m_cursorSizeTLBR;
	std::optional<sf::Cursor> m_cursorSizeBLTR;
	std::optional<sf::Cursor> m_cursorSizeAll;
	bool m_cursorsLoaded = false;

	sf::FloatRect m_dragStartGroupBounds;
	sf::Vector2f m_scaleMouseOffset;

	sf::View m_view;
	float m_zoom = 1.0f;
	bool m_isPanning = false;
	sf::Vector2i m_panStartMousePos;

	bool m_showGrid = true;
	float m_gridSize = 50.0f;

	int m_nextShapeId = 1;
	std::string m_clipboard;
	int m_pasteCount = 1;

public:
	Scene();
	void addShape(std::unique_ptr<IShape> shape);
	void draw(sf::RenderTarget& target) const;
	void clear();

	const std::vector<IShape*>& getSelectedShapes() const { return m_selectedShapes; }
	const std::vector<std::unique_ptr<IShape>>& getShapes() const { return m_shapes; }

	void handleMousePress(sf::Vector2f mousePos, sf::Vector2i pixelPos, bool isShiftPressed, bool isCtrlPressed);
	void handleMouseRelease(bool isShiftPressed);
	void handleMouseMove(sf::Vector2f mousePos);

	void groupSelected();
	void ungroupSelected();
	void selectShape(int id);
	void clearSelection();

	bool isShapeInGroup(IShape* shape) const;
	void extractFromGroup(IShape* shape);

	void bringToFront(IShape* shape);
	void sendToBack(IShape* shape);
	void cleanupEmptyGroups();

	void startDrawingMode();
	void finishDrawing(bool isClosed);
	void cancelDrawing();

	void deleteSelected();
	void saveToFile(const std::string& filename) const;
	void saveSelectedToFile(const std::string& filename) const;
	void loadFromFile(const std::string& filename, bool merge = false);

	void initCursors();
	void updateCursor(sf::RenderWindow& window, sf::Vector2f mousePos);
	void resetCursor(sf::RenderWindow& window);

	sf::Vector2f getScreenToWorld(sf::Vector2i pixelPos, const sf::RenderTarget& target) const;
	void updateViewSize(sf::Vector2f newSize);
	void handlePanStart(sf::Vector2i pixelPos);
	void handlePanMove(sf::Vector2i pixelPos, const sf::RenderTarget& target);
	void handlePanEnd();
	void handleZoom(float delta, sf::Vector2i pixelPos, const sf::RenderTarget& target);
	void resetView(sf::Vector2f windowSize);

	bool& getShowGridRef() { return m_showGrid; }
	float& getGridSizeRef() { return m_gridSize; }
	float getZoom() const { return m_zoom; }
	void setZoom(float newZoom, sf::Vector2f windowSize);

	void copySelected();
	void paste();

};