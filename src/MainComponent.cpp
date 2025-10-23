#include "MainComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <random>

namespace
{
constexpr float kHighVoltageValue = 1.0f;  // Represents +1.0 (depends on interface scaling)
constexpr int kStepsPerBeat = 4;            // 16th-note grid

struct ScaleDescriptor
{
    int id;
    const char* label;
    const char* key;
};

constexpr std::array<ScaleDescriptor, 8> kScaleOptions = {{
    { 1, "Major Pentatonic", "majorPentatonic" },
    { 2, "Minor Pentatonic", "minorPentatonic" },
    { 3, "Blues Pentatonic", "bluesPentatonic" },
    { 4, "Major Scale", "majorScale" },
    { 5, "Natural Minor", "minorScale" },
    { 6, "Dorian", "dorian" },
    { 7, "Mixolydian", "mixolydian" },
    { 8, "Phrygian", "phrygian" },
}};
} // namespace

MainComponent::MainComponent()
    : deviceSelector(deviceManager,
                     0, 0,   // no inputs
                     1, 2,   // allow mono or stereo outs
                     false, false, true, false),
      gridComponent(gridModel),
      rng(std::random_device{}())
{
    addAndMakeVisible(deviceSelector);

    sendCvButton.setClickingTogglesState(true);
    sendCvButton.onClick = [this]()
    {
        const bool active = sendCvButton.getToggleState();
        outputValue.store(active ? kHighVoltageValue : 0.0f);
        updateButtonState();
    };
    addAndMakeVisible(sendCvButton);

    startSequencerButton.setClickingTogglesState(true);
    startSequencerButton.onClick = [this]()
    {
        const bool shouldRun = startSequencerButton.getToggleState();
        if (shouldRun)
        {
            gridModel.start();
            startSequencerPlayback();
        }
        else
        {
            gridModel.stop();
            stopSequencerPlayback();
        }

        updateSequencerState(shouldRun);
    };
    addAndMakeVisible(startSequencerButton);

    randomizeButton.onClick = [this]()
    {
        gridModel.randomize(rng);
        gridComponent.refresh();
        currentStepIndex = 0;
        updateSelectedCellInfo(gridComponent.getSelectedCell());
    };
    addAndMakeVisible(randomizeButton);

    scaleLabel.setText("Scale", juce::dontSendNotification);
    scaleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(scaleLabel);

    initialiseScaleSelector();
    addAndMakeVisible(scaleSelector);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setText("Audio device idle", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    selectedCellLabel.setJustificationType(juce::Justification::centredLeft);
    selectedCellLabel.setText("Selected cell: none", juce::dontSendNotification);
    addAndMakeVisible(selectedCellLabel);

    addAndMakeVisible(gridComponent);
    gridComponent.onCellSelected = [this](int x, int y)
    {
        updateSelectedCellInfo({ x, y });
    };

    updateSequencerState(false);

    auto initialiseError = deviceManager.initialise(0, 1, nullptr, true);
    if (initialiseError.isNotEmpty())
        statusLabel.setText("Audio init error: " + initialiseError, juce::dontSendNotification);

    deviceManager.addAudioCallback(this);

    setSize(980, 640);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeAudioCallback(this);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    auto sidebar = bounds.removeFromLeft(280);

    const int controlHeight = 36;

    auto cvRow = sidebar.removeFromTop(controlHeight);
    sendCvButton.setBounds(cvRow.removeFromLeft(120));
    cvRow.removeFromLeft(8);
    statusLabel.setBounds(cvRow);

    sidebar.removeFromTop(10);
    startSequencerButton.setBounds(sidebar.removeFromTop(controlHeight));

    sidebar.removeFromTop(8);
    randomizeButton.setBounds(sidebar.removeFromTop(controlHeight));

    sidebar.removeFromTop(8);
    auto scaleRow = sidebar.removeFromTop(controlHeight);
    scaleLabel.setBounds(scaleRow.removeFromLeft(90));
    scaleRow.removeFromLeft(6);
    scaleSelector.setBounds(scaleRow);

    sidebar.removeFromTop(8);
    selectedCellLabel.setBounds(sidebar.removeFromTop(controlHeight));

    sidebar.removeFromTop(10);
    deviceSelector.setBounds(sidebar);

    bounds.removeFromLeft(12);
    gridComponent.setBounds(bounds);
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                     int /*numInputChannels*/,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& /*context*/)
{
    const float value = useSequencerOutput.load() ? sequencerOutputValue.load()
                                                  : outputValue.load();

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (auto* channelData = outputChannelData[channel])
            juce::FloatVectorOperations::fill(channelData, value, numSamples);
    }
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/)
{
    juce::MessageManager::callAsync([this]()
    {
        statusLabel.setText("Streaming CV. Toggle the button to start/stop.", juce::dontSendNotification);
    });
}

void MainComponent::audioDeviceStopped()
{
    juce::MessageManager::callAsync([this]()
    {
        statusLabel.setText("Audio device idle", juce::dontSendNotification);
    });
}

void MainComponent::updateButtonState()
{
    const bool active = sendCvButton.getToggleState();
    sendCvButton.setButtonText(active ? "Stop CV" : "Send CV");

    const juce::String message = active ? "CV active (+1.0f DC)" : "CV idle";
    statusLabel.setText(message, juce::dontSendNotification);
}

void MainComponent::updateSequencerState(bool isRunning)
{
    startSequencerButton.setButtonText(isRunning ? "Stop Sequencer" : "Start Sequencer");
    selectedCellLabel.setColour(juce::Label::textColourId,
                                isRunning ? juce::Colours::yellow : juce::Colours::lightgrey);
}

void MainComponent::updateSelectedCellInfo(juce::Point<int> cell)
{
    if (cell.x < 0 || cell.y < 0)
    {
        selectedCellLabel.setText("Selected cell: none", juce::dontSendNotification);
        return;
    }

    try
    {
        const auto& data = gridModel.cellAt(cell.x, cell.y);
        juce::String text;
        text << "Cell (" << cell.x << ", " << cell.y << ") "
             << "Prob " << juce::String(data.probability, 2)
             << " Vel " << juce::String(data.velocity, 2)
             << " Semitones " << data.semitones;
        selectedCellLabel.setText(text, juce::dontSendNotification);
    }
    catch (const std::exception&)
    {
        selectedCellLabel.setText("Selected cell: none", juce::dontSendNotification);
    }
}

void MainComponent::initialiseScaleSelector()
{
    scaleSelector.clear(juce::dontSendNotification);
    for (const auto& option : kScaleOptions)
        scaleSelector.addItem(option.label, option.id);

    auto applyScale = [this](int id)
    {
        const auto it = std::find_if(kScaleOptions.begin(), kScaleOptions.end(),
                                     [id](const ScaleDescriptor& descriptor)
                                     {
                                         return descriptor.id == id;
                                     });

        if (it != kScaleOptions.end())
            gridModel.setScale(it->key);
        else
            gridModel.setScale("majorPentatonic");

        gridComponent.refresh();
        updateSelectedCellInfo(gridComponent.getSelectedCell());
    };

    scaleSelector.onChange = [this, applyScale]()
    {
        applyScale(scaleSelector.getSelectedId());
    };

    if (!kScaleOptions.empty())
    {
        scaleSelector.setSelectedId(kScaleOptions.front().id, juce::dontSendNotification);
        applyScale(kScaleOptions.front().id);
    }
}

void MainComponent::startSequencerPlayback()
{
    const double bpm = std::max(20.0, gridModel.getCurrentBpm());
    const double intervalMs = std::max(1.0, 60000.0 / (bpm * static_cast<double>(kStepsPerBeat)));

    currentStepIndex = 0;
    useSequencerOutput.store(true);
    advanceSequencerStep();
    startTimer(static_cast<int>(std::round(intervalMs)));
}

void MainComponent::stopSequencerPlayback()
{
    stopTimer();
    useSequencerOutput.store(false);
    sequencerOutputValue.store(0.0f);
    currentStepIndex = 0;
}

void MainComponent::timerCallback()
{
    advanceSequencerStep();
}

void MainComponent::advanceSequencerStep()
{
    const int width = std::max(1, gridModel.getWidth());
    const int height = std::max(1, gridModel.getHeight());
    const int totalCells = width * height;

    if (totalCells <= 0)
        return;

    currentStepIndex = currentStepIndex % totalCells;
    const int step = currentStepIndex;
    const int x = step % width;
    const int y = step / width;

    const auto& cell = gridModel.cellAt(x, y);

    float output = 0.0f;
    if (cell.active)
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(rng) <= cell.probability)
            output = cellSemitoneToVoltage(cell);
    }

    sequencerOutputValue.store(output);
    updateSelectedCellInfo({ x, y });

    currentStepIndex = (currentStepIndex + 1) % totalCells;
}

float MainComponent::cellSemitoneToVoltage(const cvseq::GridCell& cell) const
{
    return static_cast<float>(cell.semitones) / 12.0f;
}
