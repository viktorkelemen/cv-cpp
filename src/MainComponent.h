#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <random>

#include "GridComponent.h"
#include "GridModel.h"

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
    void startSequencerPlayback();
    void stopSequencerPlayback();
    void timerCallback() override;
    void advanceSequencerStep();
    float cellSemitoneToVoltage(const cvseq::GridCell& cell) const;

    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::TextButton sendCvButton { "Send CV" };
    juce::TextButton startSequencerButton { "Start Sequencer" };
    juce::TextButton randomizeButton { "Randomize Grid" };
    juce::ComboBox scaleSelector;
    juce::Label scaleLabel;
    juce::Label statusLabel;
    juce::Label selectedCellLabel;

    cvseq::GridModel gridModel;
    cvseq::GridComponent gridComponent;
    std::atomic<float> outputValue { 0.0f };
    std::atomic<float> sequencerOutputValue { 0.0f };
    std::atomic<bool> useSequencerOutput { false };
    int currentStepIndex = 0;
    std::mt19937 rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
