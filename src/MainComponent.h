#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "GridModel.h"

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback
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

    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::TextButton sendCvButton { "Send CV" };
    juce::Label statusLabel;

    std::atomic<float> outputValue { 0.0f };
    cvseq::GridModel gridModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
