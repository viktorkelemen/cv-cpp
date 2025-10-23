#include "GridModel.h"

#include <cmath>
#include <iostream>
#include <random>

using cvseq::GridModel;
using cvseq::GridCell;

namespace
{
bool within(float value, float min, float max)
{
    return value >= min - 1e-5f && value <= max + 1e-5f;
}

void expect(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main()
{
    try
    {
        GridModel model(8, 8);
        expect(model.getWidth() == 8, "Width mismatch after construction");
        expect(model.getHeight() == 8, "Height mismatch after construction");

        std::mt19937 rng { 12345u };
        model.randomize(rng);

        for (int y = 0; y < model.getHeight(); ++y)
        {
            for (int x = 0; x < model.getWidth(); ++x)
            {
                const GridCell& cell = model.cellAt(x, y);
                expect(cell.active, "Randomized cell should be active");
                expect(within(cell.probability, 0.0f, 1.0f), "Probability outside [0,1]");
                expect(within(cell.velocity, 0.0f, 1.0f), "Velocity outside [0,1]");
                expect(cell.ratchetCount >= 1, "Ratchet count must be >= 1");
                expect(within(cell.cv1, 0.0f, 0.7f + 1e-5f), "CV1 outside expected range");
                expect(within(cell.cv2, 0.0f, 0.7f + 1e-5f), "CV2 outside expected range");
                expect(std::abs(cell.semitones) < 128, "Semitone out of reasonable bounds");
            }
        }

        model.setAllowJumps(true);
        model.randomize(rng);
        bool anyJump = false;
        for (int y = 0; y < model.getHeight(); ++y)
        {
            for (int x = 0; x < model.getWidth(); ++x)
            {
                const int jump = model.cellAt(x, y).jumpBackSteps;
                expect(jump >= 0 && jump <= 3, "Jump value outside [0,3]");
                anyJump = anyJump || jump > 0;
            }
        }
        expect(anyJump, "Expected some cells to have jump steps when jumps enabled");

        model.resize(4, 12);
        expect(model.getWidth() == 4, "Width mismatch after resize");
        expect(model.getHeight() == 12, "Height mismatch after resize");

        // Ensure resize preserved existing data where possible
        const GridCell& preserved = model.cellAt(0, 0);
        expect(within(preserved.probability, 0.0f, 1.0f), "Preserved cell probability invalid after resize");

        model.setStartOctave(1);
        model.setOctaveRange(2);
        model.reset();
        const GridCell& highCell = model.cellAt(0, 0);
        expect(highCell.semitones >= 12, "Start octave adjustment should raise semitone values");

        model.setBaseBpm(60.0);
        model.start();
        expect(std::abs(model.getCurrentBpm() - 60.0) < 1e-6, "Current BPM should match base after start");
        model.setCurrentBpm(120.0);
        expect(std::abs(model.getCurrentBpm() - 120.0) < 1e-6, "Failed to set current BPM while running");
        model.stop();

        std::cout << "GridModel tests passed\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Test failure: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
