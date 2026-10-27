#include "PolygonShape.h"
#include "EllipseShape.h"
#include <vector>

extern "C" {
    __declspec(dllexport) IShape* create_rectangle() {
        std::vector<sf::Vector2f> pts = { {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
        return new PolygonShape(sf::Vector2f(600, 400), sf::Vector2f(100, 100), true, pts);
    }

    __declspec(dllexport) IShape* create_triangle() {
        std::vector<sf::Vector2f> pts = { {0, -1}, {1, 1}, {-1, 1} };
        return new PolygonShape(sf::Vector2f(400, 400), sf::Vector2f(100, 100), true, pts);
    }

    __declspec(dllexport) IShape* create_circle() {
        return new EllipseShape(sf::Vector2f(800, 400), 100.0f, 100.0f);
    }

    __declspec(dllexport) IShape* create_trapezoid() {
        std::vector<sf::Vector2f> pts = { {-0.6f, -1.f}, {0.6f, -1.f}, {1.f, 1.f}, {-1.f, 1.f} };
        return new PolygonShape(sf::Vector2f(600, 400), sf::Vector2f(100, 100), true, pts);
    }

    __declspec(dllexport) IShape* create_pentagon() {
        std::vector<sf::Vector2f> pts(5);
        for (int i = 0; i < 5; i++) {
            float angle = (i * 72.0f - 90.0f) * 3.14159265f / 180.0f;
            pts[i] = { std::cos(angle), std::sin(angle) };
        }
        return new PolygonShape(sf::Vector2f(800, 400), sf::Vector2f(100, 100), true, pts);
    }

    __declspec(dllexport) IShape* create_custom_polygon(float cx, float cy, float w, float h, bool closed, const sf::Vector2f* pts, int ptsCount) {
        std::vector<sf::Vector2f> vecPts(pts, pts + ptsCount);
        return new PolygonShape(sf::Vector2f(cx, cy), sf::Vector2f(w, h), closed, vecPts);
    }
}