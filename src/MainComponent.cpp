#include "MainComponent.h"

namespace
{
constexpr float kHighVoltageValue = 1.0f;  // Represents +1.0 (depends on interface scaling)
}

MainComponent::MainComponent()
    : deviceSelector(deviceManager,
                     0, 0,   // no inputs
                     1, 2,   // allow mono or stereo outs
                     false, false, true, false)
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

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setText("Audio device idle", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    auto initialiseError = deviceManager.initialise(0, 1, nullptr, true);
    if (initialiseError.isNotEmpty())
        statusLabel.setText("Audio init error: " + initialiseError, juce::dontSendNotification);

    deviceManager.addAudioCallback(this);

    setSize(520, 420);
}

MainComponent::~MainComponent()
{
    deviceManager.removeAudioCallback(this);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(12);

    auto controlRow = bounds.removeFromTop(36);
    sendCvButton.setBounds(controlRow.removeFromLeft(120));
    controlRow.removeFromLeft(12);
    statusLabel.setBounds(controlRow);

    bounds.removeFromTop(12);
    deviceSelector.setBounds(bounds);
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                     int /*numInputChannels*/,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& /*context*/)
{
    const auto value = outputValue.load();

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

    auto message = active ? "CV active (+1.0f DC)" : "CV idle";
    statusLabel.setText(message, juce::dontSendNotification);
}
