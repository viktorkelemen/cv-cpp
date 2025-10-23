#include "GridComponent.h"

#include <algorithm>

namespace cvseq
{

namespace
{
juce::Colour colourForCell(const GridCell& cell)
{
    if (!cell.active)
        return juce::Colours::darkgrey.withAlpha(0.4f);

    const float probability = std::clamp(cell.probability, 0.0f, 1.0f);
    const float velocity = std::clamp(cell.velocity, 0.0f, 1.0f);
    const float brightness = 0.25f + 0.75f * probability;
    const float saturation = 0.3f + 0.6f * velocity;
    return juce::Colour::fromHSV(0.58f, saturation, brightness, 1.0f);
}

juce::Point<int> cellFromPosition(const juce::Rectangle<int>& bounds,
                                  int columns,
                                  int rows,
                                  juce::Point<float> position)
{
    if (columns <= 0 || rows <= 0)
        return { -1, -1 };

    const auto local = position - bounds.getPosition().toFloat();
    const float cellWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(columns);
    const float cellHeight = static_cast<float>(bounds.getHeight()) / static_cast<float>(rows);

    if (cellWidth <= 0.0f || cellHeight <= 0.0f)
        return { -1, -1 };

    const int x = std::clamp(static_cast<int>(local.x / cellWidth), 0, columns - 1);
    const int y = std::clamp(static_cast<int>(local.y / cellHeight), 0, rows - 1);
    return { x, y };
}
} // namespace

GridComponent::GridComponent(GridModel& modelRef)
    : model(modelRef)
{
    setInterceptsMouseClicks(true, false);
}

void GridComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.85f));
    drawGrid(g);
}

void GridComponent::mouseDown(const juce::MouseEvent& event)
{
    handleSelectionEvent(event);
}

void GridComponent::mouseDrag(const juce::MouseEvent& event)
{
    handleSelectionEvent(event);
}

void GridComponent::setSelectedCell(juce::Point<int> cell)
{
    selectedCell_ = cell;
    repaint();
}

void GridComponent::refresh()
{
    repaint();
}

void GridComponent::handleSelectionEvent(const juce::MouseEvent& event)
{
    const auto bounds = getLocalBounds();
    auto cell = cellFromPosition(bounds, model.getWidth(), model.getHeight(), event.position);

    if (cell == selectedCell_ || cell.x < 0 || cell.y < 0)
        return;

    selectedCell_ = cell;
    if (onCellSelected)
        onCellSelected(cell.x, cell.y);
    repaint();
}

void GridComponent::drawGrid(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const int columns = std::max(1, model.getWidth());
    const int rows = std::max(1, model.getHeight());
    const float cellWidth = bounds.getWidth() / static_cast<float>(columns);
    const float cellHeight = bounds.getHeight() / static_cast<float>(rows);

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < columns; ++x)
        {
            const auto& cell = model.cellAt(x, y);
            const auto cellBounds = juce::Rectangle<float>(
                bounds.getX() + x * cellWidth,
                bounds.getY() + y * cellHeight,
                cellWidth,
                cellHeight);

            g.setColour(colourForCell(cell));
            g.fillRect(cellBounds.reduced(1.0f));

            if (selectedCell_.x == x && selectedCell_.y == y)
            {
                g.setColour(juce::Colours::yellow);
                g.drawRect(cellBounds, 2.0f);
            }
        }
    }

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    for (int x = 1; x < columns; ++x)
    {
        const float xPos = bounds.getX() + x * cellWidth;
        g.drawLine(xPos, bounds.getY(), xPos, bounds.getBottom());
    }
    for (int y = 1; y < rows; ++y)
    {
        const float yPos = bounds.getY() + y * cellHeight;
        g.drawLine(bounds.getX(), yPos, bounds.getRight(), yPos);
    }
}

} // namespace cvseq
