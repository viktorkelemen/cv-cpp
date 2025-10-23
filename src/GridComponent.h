#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "GridModel.h"

namespace cvseq
{

class GridComponent final : public juce::Component
{
public:
    explicit GridComponent(GridModel& modelRef);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    void setSelectedCell(juce::Point<int> cell);
    juce::Point<int> getSelectedCell() const noexcept { return selectedCell_; }

    void refresh();

    std::function<void(int, int)> onCellSelected;

private:
    void handleSelectionEvent(const juce::MouseEvent& event);
    void drawGrid(juce::Graphics& g);

    GridModel& model;
    juce::Point<int> selectedCell_ { -1, -1 };
};

} // namespace cvseq

