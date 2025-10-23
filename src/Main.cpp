#include <juce_gui_extra/juce_gui_extra.h>

#include "MainComponent.h"

class CvSenderApplication final : public juce::JUCEApplication
{
public:
    CvSenderApplication() = default;

    const juce::String getApplicationName() override
    {
        return "CV Sender";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow("CV Sender",
                             juce::Colours::darkgrey,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(CvSenderApplication)
