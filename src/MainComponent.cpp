#include "MainComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <random>

namespace
{
constexpr float kHighVoltageValue = 1.0f;   // Represents +1.0 (depends on interface scaling)
constexpr float kGateHighVoltage = 0.8f;    // Gate/clock high level (digital)
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
    addAndMakeVisible(sidebarViewport);
    sidebarViewport.setViewedComponent(&sidebarContent, false);
    sidebarViewport.setScrollBarsShown(true, false);

    sidebarContent.addAndMakeVisible(deviceSelector);

    sendCvButton.setClickingTogglesState(true);
    sendCvButton.onClick = [this]()
    {
        const bool active = sendCvButton.getToggleState();
        outputValue.store(active ? kHighVoltageValue : 0.0f);
        updateButtonState();
    };
    sidebarContent.addAndMakeVisible(sendCvButton);

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
    sidebarContent.addAndMakeVisible(startSequencerButton);

    randomizeButton.onClick = [this]()
    {
        gridModel.randomize(rng);
        gridComponent.refresh();
        currentStepIndex = 0;
        gridComponent.setPlayheadCell({ -1, -1 });
        updateSelectedCellInfo(gridComponent.getSelectedCell());
    };
    sidebarContent.addAndMakeVisible(randomizeButton);

    scaleLabel.setText("Scale", juce::dontSendNotification);
    scaleLabel.setJustificationType(juce::Justification::centredLeft);
    sidebarContent.addAndMakeVisible(scaleLabel);

    initialiseScaleSelector();
    sidebarContent.addAndMakeVisible(scaleSelector);

    octaveLabel.setText("Base Octave", juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centredLeft);
    sidebarContent.addAndMakeVisible(octaveLabel);

    initialiseOctaveSelector();
    sidebarContent.addAndMakeVisible(octaveSelector);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setText("Audio device idle", juce::dontSendNotification);
    sidebarContent.addAndMakeVisible(statusLabel);

    selectedCellLabel.setJustificationType(juce::Justification::centredLeft);
    selectedCellLabel.setText("Selected cell: none", juce::dontSendNotification);
    sidebarContent.addAndMakeVisible(selectedCellLabel);

    for (int i = 0; i < kVoiceCalibrationCount; ++i)
    {
        voiceCalibrationDigital[i].store(0.0f);
        voicePitchDigital[i].store(0.0f);
        voiceGateDigital[i].store(0.0f);
        voiceGateSamplesRemaining[i].store(0);

        auto label = std::make_unique<juce::Label>();
        label->setJustificationType(juce::Justification::centredLeft);
        label->setText("Voice " + juce::String(i + 1) + " Offset", juce::dontSendNotification);
        voiceOffsetLabels[i] = std::move(label);
        sidebarContent.addAndMakeVisible(*voiceOffsetLabels[i]);

        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        slider->setRange(-1.0, 1.0, 0.0001);
        slider->setNumDecimalPlacesToDisplay(3);
        slider->setTextValueSuffix(" st");
        slider->setValue(0.0);
        slider->setTooltip("Fine-tune calibration in semitones for output " + juce::String(i + 1));
        slider->onValueChange = [this, i]()
        {
            if (voiceOffsetSliders[i])
                updateVoiceCalibration(i, voiceOffsetSliders[i]->getValue());
        };
        voiceOffsetSliders[i] = std::move(slider);
        sidebarContent.addAndMakeVisible(*voiceOffsetSliders[i]);

        updateVoiceCalibration(i, 0.0);
    }

    initialiseChannelSelectors();

    addAndMakeVisible(gridComponent);
    gridComponent.onCellSelected = [this](int x, int y)
    {
        updateSelectedCellInfo({ x, y });
        previewCell({ x, y });
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
    const int sidebarWidth = juce::jlimit(260, 360, bounds.getWidth() / 2);
    auto sidebarBounds = bounds.removeFromLeft(sidebarWidth);
    sidebarViewport.setBounds(sidebarBounds);

    const int scrollbarAllowance = sidebarViewport.getScrollBarThickness();
    const int contentWidth = juce::jmax(200, sidebarBounds.getWidth() - scrollbarAllowance);
    const int controlHeight = 36;
    const int gapSmall = 6;
    const int gapMedium = 8;
    const int gapLarge = 10;

    int y = 0;

    {
        juce::Rectangle<int> row(0, y, contentWidth, controlHeight);
        sendCvButton.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(gapSmall);
        statusLabel.setBounds(row);
        y += controlHeight + gapLarge;
    }

    startSequencerButton.setBounds(0, y, contentWidth, controlHeight);
    y += controlHeight + gapMedium;

    randomizeButton.setBounds(0, y, contentWidth, controlHeight);
    y += controlHeight + gapMedium;

    {
        juce::Rectangle<int> row(0, y, contentWidth, controlHeight);
        scaleLabel.setBounds(row.removeFromLeft(90));
        row.removeFromLeft(gapSmall);
        scaleSelector.setBounds(row);
        y += controlHeight + gapMedium;
    }

    {
        juce::Rectangle<int> row(0, y, contentWidth, controlHeight);
        octaveLabel.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(gapSmall);
        octaveSelector.setBounds(row);
        y += controlHeight + gapMedium;
    }

    selectedCellLabel.setBounds(0, y, contentWidth, controlHeight);
    y += controlHeight + gapMedium;

    for (int i = 0; i < kVoiceCalibrationCount; ++i)
    {
        if (!voiceOffsetLabels[i] || !voiceOffsetSliders[i])
            continue;

        juce::Rectangle<int> row(0, y, contentWidth, controlHeight);
        voiceOffsetLabels[i]->setBounds(row.removeFromLeft(140));
        row.removeFromLeft(gapSmall);
        voiceOffsetSliders[i]->setBounds(row);
        y += controlHeight + gapSmall;
    }

    y += gapMedium;

    for (int i = 0; i < kChannelSelectorCount; ++i)
    {
        if (!channelSelectorLabels[i] || !channelSelectors[i])
            continue;

        juce::Rectangle<int> row(0, y, contentWidth, controlHeight);
        channelSelectorLabels[i]->setBounds(row.removeFromLeft(140));
        row.removeFromLeft(gapSmall);
        channelSelectors[i]->setBounds(row);
        y += controlHeight + gapSmall;
    }

    y += gapMedium;

    const int deviceSelectorHeight = 220;
    deviceSelector.setBounds(0, y, contentWidth, deviceSelectorHeight);
    y += deviceSelectorHeight + gapMedium;

    sidebarContent.setSize(contentWidth, y);

    bounds.removeFromLeft(12);
    gridComponent.setBounds(bounds);
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                     int numInputChannels,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);

    updateGateAndClockTimers(numSamples);

    const float manualValue = outputValue.load();
    const bool sequencerActive = useSequencerOutput.load();
    const float voicePitch = voicePitchDigital[0].load() + voiceCalibrationDigital[0].load();
    const float voiceGate = voiceGateDigital[0].load();
    const float clockValue = clockDigital.load();

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        float sampleValue = 0.0f;
        const ChannelSource source = channel < kChannelSelectorCount ? channelAssignments[channel]
                                                                    : ChannelSource::none;

        switch (source)
        {
            case ChannelSource::manualCv:
                sampleValue = manualValue;
                break;
            case ChannelSource::sequencerPitch1:
                sampleValue = sequencerActive ? voicePitch : 0.0f;
                break;
            case ChannelSource::sequencerGate1:
                sampleValue = sequencerActive ? voiceGate : 0.0f;
                break;
            case ChannelSource::clockOut:
                sampleValue = sequencerActive ? clockValue : 0.0f;
                break;
            case ChannelSource::none:
            default:
                sampleValue = 0.0f;
                break;
        }

        if (auto* channelData = outputChannelData[channel])
            juce::FloatVectorOperations::fill(channelData, juce::jlimit(-1.0f, 1.0f, sampleValue), numSamples);
    }

    if (!isCvModeActive())
        renderPreviewAudio(outputChannelData, numOutputChannels, numSamples);
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        const double sampleRate = device->getCurrentSampleRate();
        if (sampleRate > 0.0)
            currentSampleRate = sampleRate;
    }

    gateHoldSamples = std::max(1, static_cast<int>(currentSampleRate * 0.01));
    clockHoldSamples = std::max(1, static_cast<int>(currentSampleRate * 0.005));

    for (int i = 0; i < kVoiceCalibrationCount; ++i)
    {
        voiceGateSamplesRemaining[i].store(0);
        voiceGateDigital[i].store(0.0f);
    }
    clockSamplesRemaining.store(0);
    clockDigital.store(0.0f);

    juce::MessageManager::callAsync([this]()
    {
        statusLabel.setText("Streaming CV. Toggle the button to start/stop.", juce::dontSendNotification);
    });
}

void MainComponent::audioDeviceStopped()
{
    for (int i = 0; i < kVoiceCalibrationCount; ++i)
    {
        voiceGateSamplesRemaining[i].store(0);
        voiceGateDigital[i].store(0.0f);
    }
    clockSamplesRemaining.store(0);
    clockDigital.store(0.0f);

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

void MainComponent::initialiseChannelSelectors()
{
    static const char* labels[kChannelSelectorCount] = {
        "Channel 1",
        "Channel 2",
        "Channel 3",
        "Channel 4",
        "Channel 5",
        "Channel 6",
        "Channel 7",
        "Channel 8",
    };

    for (int i = 0; i < kChannelSelectorCount; ++i)
    {
        auto label = std::make_unique<juce::Label>();
        label->setJustificationType(juce::Justification::centredLeft);
        label->setText(labels[i], juce::dontSendNotification);
        channelSelectorLabels[i] = std::move(label);
        sidebarContent.addAndMakeVisible(*channelSelectorLabels[i]);

        auto combo = std::make_unique<juce::ComboBox>();
        combo->addItem("None", channelSourceToMenuId(ChannelSource::none));
        combo->addItem("Manual CV", channelSourceToMenuId(ChannelSource::manualCv));
        combo->addItem("Voice 1 Pitch", channelSourceToMenuId(ChannelSource::sequencerPitch1));
        combo->addItem("Voice 1 Gate", channelSourceToMenuId(ChannelSource::sequencerGate1));
        combo->addItem("Clock", channelSourceToMenuId(ChannelSource::clockOut));
        combo->onChange = [this, i]()
        {
            if (channelSelectors[i])
                updateChannelAssignment(i, channelSelectors[i]->getSelectedId());
        };
        channelSelectors[i] = std::move(combo);
        sidebarContent.addAndMakeVisible(*channelSelectors[i]);
    }

    channelAssignments = {
        ChannelSource::sequencerPitch1,
        ChannelSource::sequencerGate1,
        ChannelSource::manualCv,
        ChannelSource::manualCv,
        ChannelSource::manualCv,
        ChannelSource::manualCv,
        ChannelSource::manualCv,
        ChannelSource::clockOut,
    };

    for (int i = 0; i < kChannelSelectorCount; ++i)
        if (channelSelectors[i])
            channelSelectors[i]->setSelectedId(channelSourceToMenuId(channelAssignments[i]), juce::dontSendNotification);
}

int MainComponent::channelSourceToMenuId(ChannelSource source)
{
    switch (source)
    {
        case ChannelSource::none: return 1;
        case ChannelSource::manualCv: return 2;
        case ChannelSource::sequencerPitch1: return 3;
        case ChannelSource::sequencerGate1: return 4;
        case ChannelSource::clockOut: return 5;
    }
    return 1;
}

ChannelSource MainComponent::menuIdToChannelSource(int id)
{
    switch (id)
    {
        case 2: return ChannelSource::manualCv;
        case 3: return ChannelSource::sequencerPitch1;
        case 4: return ChannelSource::sequencerGate1;
        case 5: return ChannelSource::clockOut;
        case 1:
        default:
            return ChannelSource::none;
    }
}

void MainComponent::updateChannelAssignment(int channelIndex, int selectionId)
{
    if (channelIndex < 0 || channelIndex >= kChannelSelectorCount)
        return;

    channelAssignments[channelIndex] = menuIdToChannelSource(selectionId);
}

void MainComponent::updateGateAndClockTimers(int samplesPerBlock)
{
    samplesPerBlock = std::max(1, samplesPerBlock);

    for (int i = 0; i < kVoiceCalibrationCount; ++i)
    {
        const int remaining = voiceGateSamplesRemaining[i].load();
        if (remaining > 0)
        {
            voiceGateDigital[i].store(kGateHighVoltage);
            const int newRemaining = std::max(0, remaining - samplesPerBlock);
            voiceGateSamplesRemaining[i].store(newRemaining);
        }
        else
        {
            voiceGateDigital[i].store(0.0f);
        }
    }

    const int clockRemaining = clockSamplesRemaining.load();
    if (clockRemaining > 0)
    {
        clockDigital.store(kGateHighVoltage);
        const int newRemaining = std::max(0, clockRemaining - samplesPerBlock);
        clockSamplesRemaining.store(newRemaining);
    }
    else
    {
        clockDigital.store(0.0f);
    }
}

bool MainComponent::isCvModeActive() const noexcept
{
    const auto type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr)
        return false;

    const juce::String name = type->getTypeName();
    return name.containsIgnoreCase("ES-8") || name.containsIgnoreCase("ES8") || name.containsIgnoreCase("ESX");
}

void MainComponent::renderPreviewAudio(float* const* outputChannelData, int numOutputChannels, int numSamples)
{
    if (!previewActive.load())
        return;

    const float frequency = previewFrequency.load();
    if (frequency <= 0.0f)
    {
        previewActive.store(false);
        return;
    }

    const double sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 48000.0;
    const float phaseIncrement = static_cast<float>((frequency / sampleRate) * juce::MathConstants<double>::twoPi);
    float phase = previewPhase.load();

    const int channelsToFill = std::min(numOutputChannels, 2);
    for (int i = 0; i < numSamples; ++i)
    {
        const float sample = std::sin(phase) * 0.2f;
        phase += phaseIncrement;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        for (int ch = 0; ch < channelsToFill; ++ch)
        {
            if (auto* buffer = outputChannelData[ch])
                buffer[i] += sample;
        }
    }

    previewPhase.store(phase);
    previewActive.store(false);
}

void MainComponent::initialiseOctaveSelector()
{
    struct OctaveOption { int id; const char* label; int startOctave; int referenceSemitones; };
    static constexpr std::array<OctaveOption, 6> options = {{
        { 1, "C0", -2, 0 },
        { 2, "C1", -1, 12 },
        { 3, "C2", 0, 24 },
        { 4, "C3", 1, 36 },
        { 5, "C4", 2, 48 },
        { 6, "C5", 3, 60 },
    }};

    octaveSelector.clear(juce::dontSendNotification);
    for (const auto& option : options)
        octaveSelector.addItem(option.label, option.id);

    auto applyOctave = [this](int id)
    {
        const auto* found = std::find_if(options.begin(), options.end(),
                                         [id](const OctaveOption& opt) { return opt.id == id; });
        const int startOct = found != options.end() ? found->startOctave : 0;
        const int reference = found != options.end() ? found->referenceSemitones : 24;
        gridModel.setStartOctave(startOct);
        pitchReferenceSemitones.store(reference);
        gridComponent.refresh();
        updateSelectedCellInfo(gridComponent.getSelectedCell());
    };

    octaveSelector.onChange = [this, applyOctave]()
    {
        applyOctave(octaveSelector.getSelectedId());
    };

    const int defaultId = 3; // C2
    octaveSelector.setSelectedId(defaultId, juce::dontSendNotification);
    applyOctave(defaultId);
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
    gridComponent.setPlayheadCell({ -1, -1 });

    for (int i = 0; i < kVoiceCalibrationCount; ++i)
    {
        voiceGateSamplesRemaining[i].store(0);
        voiceGateDigital[i].store(0.0f);
    }
    clockSamplesRemaining.store(0);
    clockDigital.store(0.0f);
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

    const float pitchValue = cellSemitoneToVoltage(cell);
    voicePitchDigital[0].store(pitchValue);

    bool triggered = false;
    if (cell.active)
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        triggered = dist(rng) <= cell.probability;
    }

    if (triggered)
    {
        voiceGateSamplesRemaining[0].store(std::max(1, gateHoldSamples));
        voiceGateDigital[0].store(kGateHighVoltage);
    }

    clockSamplesRemaining.store(std::max(1, clockHoldSamples));
    clockDigital.store(kGateHighVoltage);

    const float calibratedPitch = pitchValue + voiceCalibrationDigital[0].load();
    sequencerOutputValue.store(calibratedPitch);
    updateSelectedCellInfo({ x, y });
    gridComponent.setPlayheadCell({ x, y });

    currentStepIndex = (currentStepIndex + 1) % totalCells;
}

float MainComponent::cellSemitoneToVoltage(const cvseq::GridCell& cell) const
{
    constexpr float voltsPerOctaveDigital = 0.1f; // digital units per volt in 1V/oct scaling
    constexpr int semitonesPerOctave = 12;
    const int reference = pitchReferenceSemitones.load();
    return static_cast<float>(cell.semitones - reference) * (voltsPerOctaveDigital / static_cast<float>(semitonesPerOctave));
}

void MainComponent::updateVoiceCalibration(int index, double semitoneOffset)
{
    if (index < 0 || index >= kVoiceCalibrationCount)
        return;

    constexpr float voltsPerOctaveDigital = 0.1f;
    constexpr float semitoneToDigital = voltsPerOctaveDigital / 12.0f;
    voiceCalibrationDigital[index].store(static_cast<float>(semitoneOffset) * semitoneToDigital);
}
void MainComponent::previewCell(juce::Point<int> cell)
{
    if (cell.x < 0 || cell.y < 0)
        return;

    const int width = gridModel.getWidth();
    const int height = gridModel.getHeight();
    if (cell.x >= width || cell.y >= height)
        return;

    const auto& data = gridModel.cellAt(cell.x, cell.y);
    const float pitch = cellSemitoneToVoltage(data);
    voicePitchDigital[0].store(pitch);
    voiceGateDigital[0].store(kGateHighVoltage);
    voiceGateSamplesRemaining[0].store(std::max(1, gateHoldSamples));
    clockDigital.store(0.0f);
    clockSamplesRemaining.store(0);

    if (channelAssignments[0] == ChannelSource::manualCv)
        useSequencerOutput.store(false);

    if (!isCvModeActive())
    {
        const double baseFrequency = 261.63; // C4 reference
        const double frequency = baseFrequency * std::pow(2.0, static_cast<double>(data.semitones - pitchReferenceSemitones.load()) / 12.0);
        previewFrequency.store(static_cast<float>(frequency));
        previewActive.store(true);
    }
}
