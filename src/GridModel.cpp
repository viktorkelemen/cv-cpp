#include "GridModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace cvseq
{

namespace
{
constexpr int kMinWidth = 1;
constexpr int kMinHeight = 1;
constexpr int kMaxWidth = 64;
constexpr int kMaxHeight = 64;
constexpr int kMinOctaveRange = 1;
constexpr int kMaxOctaveRange = 6;
constexpr int kMinStartOctave = -4;
constexpr int kMaxStartOctave = 6;
constexpr double kMinBpm = 20.0;
constexpr double kMaxBpm = 300.0;

const std::array<int, 14> kCMinorScale = { 0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19, 20, 22 };

const std::unordered_map<std::string_view, std::vector<int>>& scaleTable()
{
    static const std::unordered_map<std::string_view, std::vector<int>> kScales = {
        { "majorPentatonic", { 0, 2, 4, 7, 9 } },
        { "minorPentatonic", { 0, 3, 5, 7, 10 } },
        { "bluesPentatonic", { 0, 3, 5, 6, 7, 10 } },
        { "majorScale", { 0, 2, 4, 5, 7, 9, 11 } },
        { "minorScale", { 0, 2, 3, 5, 7, 8, 10 } },
        { "dorian", { 0, 2, 3, 5, 7, 9, 10 } },
        { "mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
        { "phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
    };
    return kScales;
}

const std::vector<int>& fallbackScale()
{
    static const std::vector<int> kFallback = { 0, 2, 4, 7, 9 };
    return kFallback;
}

int indexFor(int x, int y, int width)
{
    return y * width + x;
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

GridModel::GridModel(int width, int height)
{
    width_ = clampInt(width, kMinWidth, kMaxWidth);
    height_ = clampInt(height, kMinHeight, kMaxHeight);
    cells_.resize(static_cast<std::size_t>(width_ * height_));
    assignDefaultPitches();
}

void GridModel::resize(int width, int height)
{
    const int newWidth = clampInt(width, kMinWidth, kMaxWidth);
    const int newHeight = clampInt(height, kMinHeight, kMaxHeight);

    std::vector<GridCell> newCells(static_cast<std::size_t>(newWidth * newHeight));

    const int copyWidth = std::min(width_, newWidth);
    const int copyHeight = std::min(height_, newHeight);

    for (int y = 0; y < copyHeight; ++y)
    {
        for (int x = 0; x < copyWidth; ++x)
        {
            newCells[indexFor(x, y, newWidth)] = cells_[indexFor(x, y, width_)];
        }
    }

    width_ = newWidth;
    height_ = newHeight;
    cells_ = std::move(newCells);
    assignDefaultPitches();
}

void GridModel::setStartOctave(int octave) noexcept
{
    startOctave_ = clampInt(octave, kMinStartOctave, kMaxStartOctave);
    assignDefaultPitches();
}

void GridModel::setOctaveRange(int range) noexcept
{
    octaveRange_ = clampInt(range, kMinOctaveRange, kMaxOctaveRange);
    assignDefaultPitches();
}

void GridModel::setScale(std::string_view scaleName)
{
    if (scaleTable().count(scaleName) == 0)
        scaleName_ = "majorPentatonic";
    else
        scaleName_ = std::string(scaleName);
}

void GridModel::setBaseBpm(double bpm) noexcept
{
    baseBpm_ = clampBpm(bpm);
    if (!isRunning_)
        currentBpm_ = baseBpm_;
}

void GridModel::setCurrentBpm(double bpm) noexcept
{
    currentBpm_ = clampBpm(bpm);
}

void GridModel::start() noexcept
{
    isRunning_ = true;
    currentBpm_ = baseBpm_;
}

GridCell& GridModel::cellAt(int x, int y)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_)
        throw std::out_of_range("GridModel::cellAt index out of range");

    return cells_[indexFor(x, y, width_)];
}

const GridCell& GridModel::cellAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_)
        throw std::out_of_range("GridModel::cellAt index out of range");

    return cells_[indexFor(x, y, width_)];
}

void GridModel::randomize(std::mt19937& rng)
{
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const auto& scale = resolveScale();
    const int octaveRange = std::max(octaveRange_, 1);
    const int scaleSize = static_cast<int>(scale.size());

    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < width_; ++x)
        {
            GridCell& cell = cells_[indexFor(x, y, width_)];

            cell.active = true;
            const float roll = unit(rng);
            cell.probability = clamp01(1.0f - std::pow(roll, 2.0f));
            cell.velocity = clamp01(0.5f + (unit(rng) * 0.5f));

            const int noteIndex = static_cast<int>(unit(rng) * scaleSize) % scaleSize;
            const int octave = static_cast<int>(unit(rng) * static_cast<float>(octaveRange)) + startOctave_;
            cell.semitones = scale[noteIndex] + (octave * 12);

            cell.cv1 = clamp01(unit(rng) * 0.7f);
            cell.cv2 = clamp01(unit(rng) * 0.7f);

            cell.ratchetCount = unit(rng) < 0.85f ? 1 : (static_cast<int>(unit(rng) * 3.0f) + 2);

            if (allowJumps_)
                cell.jumpBackSteps = randomJumpSteps(rng);
            else
                cell.jumpBackSteps = 0;
        }
    }
}

void GridModel::reset()
{
    std::fill(cells_.begin(), cells_.end(), GridCell{});
    assignDefaultPitches();
}

void GridModel::assignDefaultPitches()
{
    if (width_ <= 0 || height_ <= 0)
        return;

    const int totalSteps = std::max(1, height_);
    const int stepsPerOctave = static_cast<int>(kCMinorScale.size());
    const int totalOctaveSteps = std::max(1, octaveRange_) * stepsPerOctave;

    for (int y = 0; y < height_; ++y)
    {
        const int invertedY = height_ - 1 - y;
        const double ratio = static_cast<double>(invertedY) / static_cast<double>(totalSteps);
        int scalePosition = static_cast<int>(std::floor(ratio * totalOctaveSteps));
        scalePosition = std::clamp(scalePosition, 0, std::max(totalOctaveSteps - 1, 0));

        const int noteIndex = scalePosition % stepsPerOctave;
        const int octave = (totalOctaveSteps > 0 ? scalePosition / stepsPerOctave : 0) + startOctave_;

        for (int x = 0; x < width_; ++x)
        {
            cells_[indexFor(x, y, width_)].semitones = kCMinorScale[noteIndex] + (octave * 12);
        }
    }
}

int GridModel::clampInt(int value, int min, int max) noexcept
{
    return std::max(min, std::min(value, max));
}

double GridModel::clampBpm(double value) noexcept
{
    if (std::isnan(value) || std::isinf(value))
        return 80.0;
    return std::clamp(value, kMinBpm, kMaxBpm);
}

int GridModel::randomJumpSteps(std::mt19937& rng) const
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float roll = dist(rng);
    if (roll < 0.6f) return 0;
    if (roll < 0.8f) return 1;
    if (roll < 0.9f) return 2;
    return 3;
}

const std::vector<int>& GridModel::resolveScale() const
{
    const auto& table = scaleTable();
    const auto it = table.find(scaleName_);
    if (it != table.end())
        return it->second;
    return fallbackScale();
}

} // namespace cvseq

