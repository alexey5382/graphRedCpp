#include "Group.h"
#include "imgui.h"

Group::Group(sf::Vector2f position) : BaseShape(position, { 1.f, 1.f }, true) {
    setName("New Group");
}

void Group::addChild(std::unique_ptr<IShape> shape) {
    if (shape) {
        shape->setGroupId(0);
        m_children.push_back(std::move(shape));
    }
}

std::vector<std::unique_ptr<IShape>>& Group::getChildren() {
    return m_children;
}

void Group::draw(sf::RenderTarget& target) const {
    for (const auto& child : m_children) {
        child->draw(target);
    }
}

bool Group::contains(sf::Vector2f point) const {
    for (const auto& child : m_children) {
        if (child->contains(point)) return true;
    }
    return false;
}

void Group::updateGeometry() {
    for (auto& child : m_children) {
        child->updateGeometry();
    }
}

void Group::setGlobalColor(sf::Color color) {
    for (auto& child : m_children) child->setGlobalColor(color);
}

void Group::setGlobalThickness(float thickness) {
    for (auto& child : m_children) child->setGlobalThickness(thickness);
}

void Group::setGlobalFillColor(sf::Color color) {
    for (auto& child : m_children) child->setGlobalFillColor(color);
}

std::string Group::getType() const {
    return "Group";
}

void Group::save(std::ostream& out) const {
    BaseShape::save(out);
    out << "ChildrenCount: " << m_children.size() << "\n";
    for (const auto& child : m_children) {
        out << "[" << child->getType() << "]\n";
        child->save(out);
    }
}

void Group::load(std::istream& in) {
    BaseShape::load(in);
}

sf::FloatRect Group::getBounds() const {
    if (m_children.empty()) return sf::FloatRect({ m_position.x, m_position.y }, { 0.f, 0.f });

    float minX = 999999.f, minY = 999999.f, maxX = -999999.f, maxY = -999999.f;
    for (const auto& child : m_children) {
        sf::FloatRect b = child->getBounds();
        if (b.position.x < minX) minX = b.position.x;
        if (b.position.y < minY) minY = b.position.y;
        if (b.position.x + b.size.x > maxX) maxX = b.position.x + b.size.x;
        if (b.position.y + b.size.y > maxY) maxY = b.position.y + b.size.y;
    }
    return sf::FloatRect({ minX, minY }, { std::max(0.1f, maxX - minX), std::max(0.1f, maxY - minY) });
}

void Group::setPosition(sf::Vector2f newPos) {
    sf::Vector2f delta = newPos - m_position;
    for (auto& child : m_children) {
        child->setPosition(child->getPosition() + delta);
    }
    m_position = newPos;
}

void Group::setRotation(float angle) {
    float delta = angle - m_rotation;
    sf::Vector2f pivot = m_position + m_anchorOffset; // Центр группы (ось вращения)

    for (auto& child : m_children) {
        sf::Vector2f childPos = child->getPosition();
        sf::Vector2f childAnchorWorld = childPos + child->getAnchorOffset();

        // 1. Вращаем истинный якорь ребенка вокруг центра группы
        sf::Vector2f newChildAnchorWorld = BaseShape::rotatePoint(childAnchorWorld, pivot, delta);

        // 2. Вычисляем вектор сдвига и применяем его к физической позиции (top-left)
        sf::Vector2f shift = newChildAnchorWorld - childAnchorWorld;
        child->setPosition(childPos + shift);

        // 3. Поворачиваем саму фигуру вокруг её собственной оси
        child->setRotation(child->getRotation() + delta);
    }
    m_rotation = angle;
    updateGeometry();
}

void Group::applyGlobalScale(sf::Vector2f fixedCorner, float Sx, float Sy) {
    for (auto& child : m_children) {
        child->applyGlobalScale(fixedCorner, Sx, Sy);
    }

    sf::Vector2f oldAnchor = m_position + m_anchorOffset;
    sf::Vector2f newAnchor(
        fixedCorner.x + (oldAnchor.x - fixedCorner.x) * Sx,
        fixedCorner.y + (oldAnchor.y - fixedCorner.y) * Sy
    );

    sf::FloatRect b = getBounds();
    m_position = { b.position.x, b.position.y };
    m_size = { b.size.x, b.size.y };
    m_anchorOffset = newAnchor - m_position;
}

void Group::showImGuiProperties() {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Group Properties");
    ImGui::TextDisabled("Contains %zu shapes", m_children.size());
    ImGui::Separator();

    sf::Vector2f worldAnchor = m_position + m_anchorOffset;
    ImGui::Text("Anchor Position (World):");
    float pos[2] = { worldAnchor.x, worldAnchor.y };

    ImGui::SetNextItemWidth(150);
    bool posDrag = ImGui::DragFloat2("##gAnchorDrag", pos, 1.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    bool posInp = ImGui::InputFloat2("##gAnchorInp", pos, "%.1f");

    if (posDrag || posInp) {
        sf::Vector2f delta(pos[0] - worldAnchor.x, pos[1] - worldAnchor.y);
        setPosition(m_position + delta);
    }

    ImGui::Separator();

    ImGui::Text("Group Rotation:");
    float rot = m_rotation;
    ImGui::SetNextItemWidth(120);
    bool rotChanged = ImGui::DragFloat("##gRotDrag", &rot, 1.0f, -180.0f, 180.0f, "%.1f deg");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    rotChanged |= ImGui::InputFloat("Angle", &rot, 0.0f, 0.0f, "%.1f");

    if (rotChanged) setRotation(rot);

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Apply to All Children:");

    static float sharedFill[4] = { 0.8f, 0.8f, 0.8f, 0.5f };
    if (ImGui::ColorEdit4("Global Fill", sharedFill)) {
        sf::Color c(sharedFill[0] * 255, sharedFill[1] * 255, sharedFill[2] * 255, sharedFill[3] * 255);
        setGlobalFillColor(c);
    }

    static float sharedStroke[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (ImGui::ColorEdit4("Global Stroke", sharedStroke)) {
        sf::Color c(sharedStroke[0] * 255, sharedStroke[1] * 255, sharedStroke[2] * 255, sharedStroke[3] * 255);
        setGlobalColor(c);
    }

    static float sharedThick = 2.0f;
    if (ImGui::SliderFloat("Global Thickness", &sharedThick, 1.0f, 50.0f)) {
        setGlobalThickness(sharedThick);
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Shapes in this Group")) {
        for (const auto& child : m_children) {
            ImGui::BulletText("[ID:%04d] %s (%s)",
                child->getId(),
                child->getName().c_str(),
                child->getType().c_str());
        }
    }
}
void Group::captureState() {
    BaseShape::captureState();
    for (auto& child : m_children) child->captureState(); // Сохраняем детей!
}

void Group::restoreState() {
    BaseShape::restoreState();
    for (auto& child : m_children) child->restoreState(); // Откатываем детей!
}

IShape* Group::getHitShape(sf::Vector2f mousePos) {
    // Ищем с конца, чтобы попадать в фигуры, которые нарисованы поверх других (Z-index)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        IShape* hit = (*it)->getHitShape(mousePos);
        if (hit) return hit; // Если попали в ребенка — возвращаем его
    }
    // Если в детей не попали, но клик внутри рамки группы — возвращаем саму группу
    return contains(mousePos) ? this : nullptr;
}