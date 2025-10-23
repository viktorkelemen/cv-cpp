#pragma once

#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace cvseq
{

struct GridCell
{
    bool active = true;
    float probability = 1.0f;
    float velocity = 1.0f;
    int ratchetCount = 1;
    int semitones = 0;
    float cv1 = 0.0f;
    float cv2 = 0.0f;
    int jumpBackSteps = 0;
};

class GridModel
{
public:
    GridModel(int width = 8, int height = 8);

    void resize(int width, int height);

    int getWidth() const noexcept { return width_; }
    int getHeight() const noexcept { return height_; }

    void setStartOctave(int octave) noexcept;
    int getStartOctave() const noexcept { return startOctave_; }

    void setOctaveRange(int range) noexcept;
    int getOctaveRange() const noexcept { return octaveRange_; }

    void setAllowJumps(bool allow) noexcept { allowJumps_ = allow; }
    bool getAllowJumps() const noexcept { return allowJumps_; }

    void setScale(std::string_view scaleName);
    const std::string& getScale() const noexcept { return scaleName_; }

    void setBaseBpm(double bpm) noexcept;
    double getBaseBpm() const noexcept { return baseBpm_; }

    void setCurrentBpm(double bpm) noexcept;
    double getCurrentBpm() const noexcept { return currentBpm_; }

    void start() noexcept;
    void stop() noexcept { isRunning_ = false; }
    bool isRunning() const noexcept { return isRunning_; }

    GridCell& cellAt(int x, int y);
    const GridCell& cellAt(int x, int y) const;

    void randomize(std::mt19937& rng);
    void reset();

private:
    void assignDefaultPitches();
    static int clampInt(int value, int min, int max) noexcept;
    static double clampBpm(double value) noexcept;
    int randomJumpSteps(std::mt19937& rng) const;
    const std::vector<int>& resolveScale() const;

    int width_ = 0;
    int height_ = 0;
    int startOctave_ = 0;
    int octaveRange_ = 3;
    bool allowJumps_ = true;
    bool isRunning_ = false;
    double baseBpm_ = 80.0;
    double currentBpm_ = 80.0;
    std::string scaleName_ { "majorPentatonic" };
    std::vector<GridCell> cells_;
};

} // namespace cvseq

