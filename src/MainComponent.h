#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <array>
#include <memory>
#include <random>

#include "GridComponent.h"
#include "GridModel.h"

enum class ChannelSource
{
    none = 0,
    manualCv,
    sequencerPitch1,
    sequencerGate1,
    clockOut,
};

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void updateButtonState();
    void updateSequencerState(bool isRunning);
    void updateSelectedCellInfo(juce::Point<int> cell);
    void initialiseScaleSelector();
    void initialiseOctaveSelector();
    void startSequencerPlayback();
    void stopSequencerPlayback();
    void timerCallback() override;
    void advanceSequencerStep();
    float cellSemitoneToVoltage(const cvseq::GridCell& cell) const;
    void updateVoiceCalibration(int index, double semitoneOffset);
    void initialiseChannelSelectors();
    void updateChannelAssignment(int channelIndex, int selectionId);
    static int channelSourceToMenuId(ChannelSource source);
    static ChannelSource menuIdToChannelSource(int id);
    void updateGateAndClockTimers(int samplesPerBlock);

    juce::AudioDeviceManager deviceManager;
    juce::Viewport sidebarViewport;
    juce::Component sidebarContent;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::TextButton sendCvButton { "Send CV" };
    juce::TextButton startSequencerButton { "Start Sequencer" };
    juce::TextButton randomizeButton { "Randomize Grid" };
    juce::ComboBox scaleSelector;
    juce::Label scaleLabel;
    juce::ComboBox octaveSelector;
    juce::Label octaveLabel;
    juce::Label statusLabel;
    juce::Label selectedCellLabel;

    cvseq::GridModel gridModel;
    cvseq::GridComponent gridComponent;
    std::atomic<float> outputValue { 0.0f };
    std::atomic<float> sequencerOutputValue { 0.0f };
    std::atomic<bool> useSequencerOutput { false };
    int currentStepIndex = 0;
    static constexpr int kVoiceCalibrationCount = 3;
    std::array<std::unique_ptr<juce::Slider>, kVoiceCalibrationCount> voiceOffsetSliders;
    std::array<std::unique_ptr<juce::Label>, kVoiceCalibrationCount> voiceOffsetLabels;
    std::array<std::atomic<float>, kVoiceCalibrationCount> voiceCalibrationDigital {};
    std::array<std::atomic<float>, kVoiceCalibrationCount> voicePitchDigital {};
    std::array<std::atomic<float>, kVoiceCalibrationCount> voiceGateDigital {};
    std::array<std::atomic<int>, kVoiceCalibrationCount> voiceGateSamplesRemaining {};

    static constexpr int kChannelSelectorCount = 8;
    std::array<std::unique_ptr<juce::Label>, kChannelSelectorCount> channelSelectorLabels;
    std::array<std::unique_ptr<juce::ComboBox>, kChannelSelectorCount> channelSelectors;
    std::array<ChannelSource, kChannelSelectorCount> channelAssignments {};

    std::atomic<int> clockSamplesRemaining { 0 };
    std::atomic<float> clockDigital { 0.0f };
    double currentSampleRate = 48000.0;
    int gateHoldSamples = 480;
    int clockHoldSamples = 240;
    std::mt19937 rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
