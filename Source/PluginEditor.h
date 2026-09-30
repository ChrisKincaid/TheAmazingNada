#pragma once

#include "PluginProcessor.h"
#include <array>

class TheAmazingNottaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer
{
public:
    explicit TheAmazingNottaAudioProcessorEditor(TheAmazingNottaAudioProcessor& processor);
    ~TheAmazingNottaAudioProcessorEditor() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    class Visualizer;
    class KnobLookAndFeel;
    class SkinArrowButton;
    class TubeToggleButton;
    class TubeLayer;
    class ReelToReelComponent;
    class SmokeBackgroundComponent;
    class BoxBapLogoBanner;
    class ScrewSpinButton;

    struct TubeAnimation
    {
        enum class Phase { hidden, opening, seated, unplugJerk, drifting };
        Phase phase = Phase::hidden;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        float alpha = 0.0f;
        float elapsed = 0.0f;
        float startX = 0.0f;
        float startY = 0.0f;
        float startAlpha = 0.0f;
        bool targetOn = false;
    };

    void timerCallback() override;
    void changeSkin(int direction);
    void setTubeOn(size_t tubeIndex, bool shouldTurnOn);
    void updateTubeAnimations(float audioRms);
    void toggleLogoBanner();
    void advanceLogoBanner();

    TheAmazingNottaAudioProcessor& processor;
    juce::Image knobImage;
    std::array<juce::Image, 11> skinImages;
    int activeSkinIndex = 2;
    juce::Image toggleOffImage;
    juce::Image toggleOnImage;
    juce::Image tubeLeftImage;
    juce::Image tubeRightImage;
    juce::Image boxBapLogoImage;
    juce::Image screwButtonImage;
    std::unique_ptr<KnobLookAndFeel> knobLookAndFeel;
    juce::ImageComponent backgroundImage;
    std::unique_ptr<SmokeBackgroundComponent> smokeBackground;
    std::array<std::unique_ptr<TubeLayer>, 2> tubeLayers;
    std::array<std::unique_ptr<TubeToggleButton>, 2> tubeToggleButtons;
    std::array<TubeAnimation, 2> tubeAnimations;
    std::unique_ptr<ReelToReelComponent> reelToReel;
    std::unique_ptr<BoxBapLogoBanner> logoBanner;
    std::unique_ptr<ScrewSpinButton> screwButton;
    float logoBannerOffsetY = 420.0f;
    bool logoBannerVisible = false;
    std::array<juce::Slider, 5> knobs;
    std::unique_ptr<SkinArrowButton> previousSkinButton;
    std::unique_ptr<SkinArrowButton> nextSkinButton;
    std::unique_ptr<Visualizer> visualizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheAmazingNottaAudioProcessorEditor)
};