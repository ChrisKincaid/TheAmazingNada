#pragma once

#include <JuceHeader.h>

class TheAmazingNottaAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int visualizerSamples = 512;

    TheAmazingNottaAudioProcessor();
    ~TheAmazingNottaAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    int readVisualizerSamples(float* left, float* right, int maximumSamples) noexcept;
    void triggerGlassTink() noexcept;
    void triggerGlassShatter() noexcept;

private:
    static constexpr int fifoCapacity = 16384;
    juce::AbstractFifo visualizerFifo { fifoCapacity };
    std::array<float, fifoCapacity> fifoLeft {};
    std::array<float, fifoCapacity> fifoRight {};
    std::atomic<bool> pendingGlassTink { false };
    std::atomic<bool> pendingGlassShatter { false };
    double effectSampleRate = 44100.0;
    double tinkPhase = 0.0;
    float tinkEnvelope = 0.0f;
    int shatterSamplesRemaining = 0;
    float shatterEnvelope = 0.0f;
    float previousNoise = 0.0f;
    float highPassedNoise = 0.0f;
    double cracklePhase = 0.0;
    std::uint32_t crackleRandomState = 0x6d2b79f5u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheAmazingNottaAudioProcessor)
};