#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

TheAmazingNottaAudioProcessor::TheAmazingNottaAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void TheAmazingNottaAudioProcessor::prepareToPlay(double sampleRate, int)
{
    setLatencySamples(0);
    visualizerFifo.reset();
    effectSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    tinkPhase = 0.0;
    tinkEnvelope = 0.0f;
    shatterSamplesRemaining = 0;
    shatterEnvelope = 0.0f;
    previousNoise = 0.0f;
    highPassedNoise = 0.0f;
    cracklePhase = 0.0;
    pendingGlassTink.store(false, std::memory_order_relaxed);
    pendingGlassShatter.store(false, std::memory_order_relaxed);
}

void TheAmazingNottaAudioProcessor::releaseResources()
{
}

bool TheAmazingNottaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return (input == juce::AudioChannelSet::mono() && output == juce::AudioChannelSet::mono())
        || (input == juce::AudioChannelSet::stereo() && output == juce::AudioChannelSet::stereo());
}

void TheAmazingNottaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    // Capture the dry signal before adding any triggered glass feedback.
    const auto sampleCount = buffer.getNumSamples();
    if (sampleCount <= 0 || buffer.getNumChannels() <= 0)
        return;

    const auto* inputLeft = buffer.getReadPointer(0);
    const auto* inputRight = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : inputLeft;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    visualizerFifo.prepareToWrite(sampleCount, start1, size1, start2, size2);

    std::copy_n(inputLeft, size1, fifoLeft.data() + start1);
    std::copy_n(inputRight, size1, fifoRight.data() + start1);
    std::copy_n(inputLeft + size1, size2, fifoLeft.data() + start2);
    std::copy_n(inputRight + size1, size2, fifoRight.data() + start2);
    visualizerFifo.finishedWrite(size1 + size2);

    if (pendingGlassTink.exchange(false, std::memory_order_relaxed))
    {
        tinkPhase = 0.0;
        tinkEnvelope = 0.20f;
    }
    if (pendingGlassShatter.exchange(false, std::memory_order_relaxed))
    {
        shatterSamplesRemaining = juce::jmax(1, (int) (effectSampleRate * 0.18));
        shatterEnvelope = 0.28f;
        previousNoise = 0.0f;
        highPassedNoise = 0.0f;
        cracklePhase = 0.0;
    }

    if (tinkEnvelope <= 0.0001f && shatterSamplesRemaining <= 0)
        return;

    const auto tinkPhaseStep = juce::MathConstants<double>::twoPi * 3200.0 / effectSampleRate;
    const auto tinkDecay = (float) std::exp(-1.0 / (effectSampleRate * 0.0045));
    const auto shatterDecay = (float) std::exp(-1.0 / (effectSampleRate * 0.058));
    const auto cracklePhaseStep = juce::MathConstants<double>::twoPi * 470.0 / effectSampleRate;
    const auto outputChannels = buffer.getNumChannels();

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        float feedback = 0.0f;
        if (tinkEnvelope > 0.0001f)
        {
            feedback += (float) std::sin(tinkPhase) * tinkEnvelope;
            tinkPhase += tinkPhaseStep;
            tinkEnvelope *= tinkDecay;
        }

        if (shatterSamplesRemaining > 0)
        {
            crackleRandomState = crackleRandomState * 1664525u + 1013904223u;
            const auto whiteNoise = ((float) ((crackleRandomState >> 8) & 0x00ffffffu)
                / 8388608.0f) - 1.0f;
            highPassedNoise = whiteNoise - previousNoise + highPassedNoise * 0.94f;
            previousNoise = whiteNoise;
            const auto crackle = std::sin(cracklePhase) >= 0.0 ? 1.0f : -1.0f;
            cracklePhase += cracklePhaseStep;
            feedback += shatterEnvelope * (highPassedNoise * 0.055f + crackle * 0.018f);
            shatterEnvelope *= shatterDecay;
            --shatterSamplesRemaining;
        }

        if (feedback != 0.0f)
        {
            const auto channelSample = feedback * (outputChannels > 1 ? 0.7071f : 1.0f);
            for (int channel = 0; channel < outputChannels; ++channel)
                buffer.addSample(channel, sample, channelSample);
        }
    }
}

void TheAmazingNottaAudioProcessor::triggerGlassTink() noexcept
{
    pendingGlassTink.store(true, std::memory_order_relaxed);
}

void TheAmazingNottaAudioProcessor::triggerGlassShatter() noexcept
{
    pendingGlassShatter.store(true, std::memory_order_relaxed);
}

int TheAmazingNottaAudioProcessor::readVisualizerSamples(float* left, float* right,
                                                          int maximumSamples) noexcept
{
    if (left == nullptr || right == nullptr || maximumSamples <= 0)
        return 0;

    const auto ready = visualizerFifo.getNumReady();
    const auto discardCount = juce::jmax(0, ready - maximumSamples);
    if (discardCount > 0)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        visualizerFifo.prepareToRead(discardCount, start1, size1, start2, size2);
        visualizerFifo.finishedRead(size1 + size2);
    }

    const auto requested = juce::jmin(maximumSamples, visualizerFifo.getNumReady());
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    visualizerFifo.prepareToRead(requested, start1, size1, start2, size2);
    std::copy_n(fifoLeft.data() + start1, size1, left);
    std::copy_n(fifoRight.data() + start1, size1, right);
    std::copy_n(fifoLeft.data() + start2, size2, left + size1);
    std::copy_n(fifoRight.data() + start2, size2, right + size1);
    visualizerFifo.finishedRead(size1 + size2);
    return size1 + size2;
}

juce::AudioProcessorEditor* TheAmazingNottaAudioProcessor::createEditor()
{
    return new TheAmazingNottaAudioProcessorEditor(*this);
}

bool TheAmazingNottaAudioProcessor::hasEditor() const { return true; }
const juce::String TheAmazingNottaAudioProcessor::getName() const { return JucePlugin_Name; }
bool TheAmazingNottaAudioProcessor::acceptsMidi() const { return false; }
bool TheAmazingNottaAudioProcessor::producesMidi() const { return false; }
bool TheAmazingNottaAudioProcessor::isMidiEffect() const { return false; }
double TheAmazingNottaAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TheAmazingNottaAudioProcessor::getNumPrograms() { return 1; }
int TheAmazingNottaAudioProcessor::getCurrentProgram() { return 0; }
void TheAmazingNottaAudioProcessor::setCurrentProgram(int) {}
const juce::String TheAmazingNottaAudioProcessor::getProgramName(int) { return {}; }
void TheAmazingNottaAudioProcessor::changeProgramName(int, const juce::String&) {}
void TheAmazingNottaAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void TheAmazingNottaAudioProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheAmazingNottaAudioProcessor();
}