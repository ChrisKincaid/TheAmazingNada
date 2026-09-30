#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

#include <BinaryData.h>

namespace
{
const juce::Colour slate { 0xff151d27 };
const juce::Colour cyan { 0xff49d8ef };
const juce::Colour amber { 0xffffb454 };
}

class TheAmazingNottaAudioProcessorEditor::SmokeBackgroundComponent final : public juce::Component
{
public:
    static constexpr int particleCount = 120;
    static constexpr int opacityLevels = 16;

    struct SmokeParticle
    {
        juce::Point<float> position, velocity;
        float age = 0.0f, lifetime = 1.0f, radius = 24.0f, seed = 0.0f;
        size_t colorIndex = 0;
        bool active = false;
    };

    SmokeBackgroundComponent()
    {
        setInterceptsMouseClicks(false, false);
        setSkinIndex(2);
    }

    void setSkinIndex(int index)
    {
        if (index == 6)
            palette = { juce::Colour(0xffa5adb2), juce::Colour(0xff626b72), juce::Colour(0xff353b41) };
        else if (index == 0 || index == 5 || index == 9)
            palette = { juce::Colour(0xff61dff4), juce::Colour(0xffa18be8), juce::Colour(0xff7589ed) };
        else
            palette = { juce::Colour(0xffffb15e), juce::Colour(0xffd47b49), juce::Colour(0xff873e36) };
        rebuildSprites();
        repaint();
    }

    void setControls(float turbulenceValue, float densityValue, float exhaleValue)
    {
        turbulence = juce::jlimit(0.0f, 1.0f, turbulenceValue);
        density = juce::jlimit(0.0f, 1.0f, densityValue);
        exhale = juce::jlimit(0.0f, 1.0f, exhaleValue);
    }

    void beginKnobDrag(juce::Point<float> point)
    {
        dragCentre = point;
        dragging = true;
        swirlImpulse = 1.0f;
    }

    void endKnobDrag() { dragging = false; }

    void advance(float level)
    {
        constexpr auto frameSeconds = 1.0f / 60.0f;
        const auto audioLevel = juce::jlimit(0.0f, 1.0f, level);
        const auto transient = juce::jmax(0.0f, audioLevel - previousAudioLevel);
        previousAudioLevel = audioLevel;

        emissionRemainder += frameSeconds * (8.0f + density * 20.0f);
        if (exhale > 0.0f && transient > 0.055f && audioLevel > 0.08f)
        {
            emissionRemainder += exhale * (2.0f + audioLevel * 10.0f);
            audioKick = juce::jmax(audioKick, exhale * (0.35f + audioLevel));
        }
        audioKick *= 0.91f;
        swirlImpulse *= dragging ? 0.96f : 0.88f;

        for (auto& particle : particles)
        {
            if (!particle.active && emissionRemainder >= 1.0f)
            {
                spawnParticle(particle);
                emissionRemainder -= 1.0f;
            }
            if (!particle.active)
                continue;

            constexpr auto areaHeight = 1500.0f;
            constexpr auto areaWidth = 1500.0f;
            constexpr auto dt = frameSeconds;
            const auto waft = std::sin(particle.age * (0.8f + turbulence * 2.0f) + particle.seed);
            particle.position.x += waft * (8.0f + turbulence * 110.0f) * dt;
            particle.velocity.y = -(55.0f + turbulence * 70.0f + density * 40.0f + audioKick * 340.0f);
            particle.velocity.x *= 0.999f;

            if (dragging)
                addSwirl(particle, dt);

            particle.position += particle.velocity * dt;
            particle.radius += (10.0f + density * 22.0f + audioKick * 30.0f) * dt;
            particle.age += dt;
            if (particle.age >= particle.lifetime || particle.position.y + particle.radius < 0.0f
                || particle.position.x + particle.radius < 0.0f
                || particle.position.x - particle.radius > areaWidth
                || particle.position.y - particle.radius > areaHeight)
                particle.active = false;
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(getLocalBounds());
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        for (const auto& particle : particles)
        {
            if (!particle.active)
                continue;
            const auto life = juce::jlimit(0.0f, 1.0f, 1.0f - particle.age / particle.lifetime);
            const auto fadeIn = juce::jmin(1.0f, particle.age * 1.4f);
            const auto opacity = (0.18f + density * 0.27f) * fadeIn * std::pow(life, 0.72f);
            const auto opacityIndex = (size_t) juce::jlimit(0, opacityLevels - 1,
                (int) std::round(opacity / 0.45f * (opacityLevels - 1)));
            const auto& sprite = sprites[particle.colorIndex][opacityIndex];
            const auto diameter = particle.radius * 2.0f;
            g.drawImageWithin(sprite,
                juce::roundToInt(particle.position.x - particle.radius),
                juce::roundToInt(particle.position.y - particle.radius),
                juce::roundToInt(diameter), juce::roundToInt(diameter),
                juce::RectanglePlacement::stretchToFit);
        }
    }

private:
    void spawnParticle(SmokeParticle& particle)
    {
        const auto width = (float) getWidth();
        const auto height = (float) getHeight();
        const auto spawnSpan = juce::jlimit(0.10f, 0.98f,
            0.12f + density * 0.72f + turbulence * 0.14f);
        particle.position = { width * (0.5f + (random.nextFloat() - 0.5f) * spawnSpan),
            height * (0.62f + random.nextFloat() * 0.34f) };
        particle.velocity = { (random.nextFloat() - 0.5f)
                * (12.0f + turbulence * 140.0f + density * 100.0f),
            -(55.0f + turbulence * 70.0f + density * 40.0f + audioKick * 340.0f) };
        particle.age = 0.0f;
        particle.lifetime = 3.8f + random.nextFloat() * 3.6f;
        particle.radius = 42.0f + random.nextFloat() * 44.0f + density * 32.0f + audioKick * 34.0f;
        particle.seed = random.nextFloat() * juce::MathConstants<float>::twoPi;
        particle.colorIndex = (size_t) random.nextInt((int) palette.size());
        particle.active = true;
    }

    void addSwirl(SmokeParticle& particle, float dt)
    {
        const auto offset = particle.position - dragCentre;
        const auto distance = offset.getDistanceFromOrigin();
        constexpr auto swirlRadius = 260.0f;
        if (distance <= 1.0f || distance >= swirlRadius)
            return;
        const auto influence = (1.0f - distance / swirlRadius) * swirlImpulse * (0.08f + turbulence);
        particle.velocity += juce::Point<float>(-offset.y / distance, offset.x / distance)
            * (260.0f * influence * dt);
    }

    void rebuildSprites()
    {
        constexpr int size = 64;
        for (size_t colour = 0; colour < palette.size(); ++colour)
        {
            for (int level = 0; level < opacityLevels; ++level)
            {
                auto& sprite = sprites[colour][(size_t) level];
                sprite = juce::Image(juce::Image::ARGB, size, size, true);
                juce::Image::BitmapData pixels(sprite, juce::Image::BitmapData::writeOnly);
                const auto opacity = level / (float) (opacityLevels - 1) * 0.45f;
                for (int y = 0; y < size; ++y)
                    for (int x = 0; x < size; ++x)
                    {
                        const auto dx = (x + 0.5f - size * 0.5f) / (size * 0.5f);
                        const auto dy = (y + 0.5f - size * 0.5f) / (size * 0.5f);
                        const auto alpha = (juce::uint8) juce::jlimit(0, 255,
                            (int) std::round(opacity * std::exp(-(dx * dx + dy * dy) * 5.2f) * 255.0f));
                        const auto base = palette[colour];
                        pixels.setPixelColour(x, y, juce::Colour::fromRGBA(
                            (juce::uint8) base.getRed(), (juce::uint8) base.getGreen(),
                            (juce::uint8) base.getBlue(), alpha));
                    }
            }
        }
    }

    std::array<juce::Colour, 3> palette {
        juce::Colour(0xffffad63), juce::Colour(0xffd47b49), juce::Colour(0xff873e36)
    };
    std::array<std::array<juce::Image, opacityLevels>, 3> sprites;
    std::array<SmokeParticle, particleCount> particles {};
    juce::Random random { 0x534d4f4b };
    juce::Point<float> dragCentre;
    float turbulence = 0.0f, density = 0.0f, exhale = 0.0f;
    float previousAudioLevel = 0.0f, audioKick = 0.0f, emissionRemainder = 0.0f, swirlImpulse = 0.0f;
    bool dragging = false;
};

class TheAmazingNottaAudioProcessorEditor::Visualizer final : public juce::Component
{
public:
    static constexpr int bandCount = 20;
    static constexpr int waterfallRows = 18;
    static constexpr int particleCount = 88;
    using Samples = std::array<float, TheAmazingNottaAudioProcessor::visualizerSamples>;
    using BandRow = std::array<float, bandCount>;

    enum class DisplayMode { visualizer, playing, gameOver };
    enum class HazardType { record, cassette };

    struct Laser
    {
        float x = 0.0f, y = 0.0f;
        bool active = false;
    };

    struct Hazard
    {
        HazardType type = HazardType::record;
        juce::Point<float> position;
        float speed = 0.0f, angle = 0.0f, spin = 0.0f;
        bool active = false;
    };

    struct Debris
    {
        juce::Point<float> position, velocity;
        float angle = 0.0f, angularVelocity = 0.0f, length = 0.0f;
        float age = 0.0f, lifetime = 0.28f;
        bool active = false;
    };

    struct Particle
    {
        juce::Point<float> position;
        juce::Point<float> velocity;
        float age = 0.0f;
        float lifetime = 1.0f;
        float radius = 1.0f;
        bool amber = false;
    };

    explicit Visualizer(TheAmazingNottaAudioProcessor& source) : processor(source)
    {
        setInterceptsMouseClicks(true, true);
        setWantsKeyboardFocus(true);
        aliasEditor.setInputRestrictions(12);
        aliasEditor.setJustification(juce::Justification::centred);
        aliasEditor.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        aliasEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        aliasEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xff72f2d2));
        aliasEditor.setColour(juce::TextEditor::highlightColourId, juce::Colour(0x6656dcca));
        aliasEditor.setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xff090d12));
        aliasEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        aliasEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        aliasEditor.setColour(juce::CaretComponent::caretColourId, juce::Colour(0xff72f2d2));
        aliasEditor.setEscapeAndReturnKeysConsumed(true);
        aliasEditor.onReturnKey = [this] { submitAlias(); };
        aliasEditor.onEscapeKey = [this]
        {
            if (mode == DisplayMode::gameOver && gameOverLockout <= 0.0f)
                resetToVisualizer();
        };
        aliasEditor.setVisible(false);
        addAndMakeVisible(aliasEditor);
        constexpr auto visualizerSampleCount = TheAmazingNottaAudioProcessor::visualizerSamples;
        for (int sample = 0; sample < visualizerSampleCount; ++sample)
        {
            const auto phase = juce::MathConstants<double>::twoPi * sample / (visualizerSampleCount - 1);
            window[(size_t) sample] = (float) (0.5 - 0.5 * std::cos(phase));
        }

        for (int band = 0; band < bandCount; ++band)
        {
            const auto proportion = (double) band / (bandCount - 1);
            bandBins[(size_t) band] = juce::jlimit(1, visualizerSampleCount / 2 - 1,
                (int) std::round(std::pow((double) (visualizerSampleCount / 2 - 1), proportion)));
        }
    }

    void setLayerIntensities(const std::array<float, 5>& intensities)
    {
        for (size_t layer = 0; layer < layerIntensities.size(); ++layer)
        {
            if (layer == 0 && layerIntensities[layer] > 0.0f && intensities[layer] <= 0.0f)
                for (auto& particle : particles)
                    particle.age = particle.lifetime;
            layerIntensities[layer] = juce::jlimit(0.0f, 1.0f, intensities[layer]);
        }
    }

    void updateFromAudio()
    {
        std::array<float, TheAmazingNottaAudioProcessor::visualizerSamples> incomingLeft {};
        std::array<float, TheAmazingNottaAudioProcessor::visualizerSamples> incomingRight {};
        const auto count = processor.readVisualizerSamples(incomingLeft.data(), incomingRight.data(),
            TheAmazingNottaAudioProcessor::visualizerSamples);

        double sampleEnergy = 0.0;
        float blockPeak = 0.0f;
        if (count > 0)
        {
            const auto remaining = TheAmazingNottaAudioProcessor::visualizerSamples - count;
            std::move(left.begin() + count, left.end(), left.begin());
            std::move(right.begin() + count, right.end(), right.begin());
            std::copy_n(incomingLeft.begin(), count, left.begin() + remaining);
            std::copy_n(incomingRight.begin(), count, right.begin() + remaining);

            for (int index = 0; index < count; ++index)
            {
                const auto leftSample = incomingLeft[(size_t) index];
                const auto rightSample = incomingRight[(size_t) index];
                sampleEnergy += 0.5 * ((double) leftSample * leftSample
                    + (double) rightSample * rightSample);
                blockPeak = juce::jmax(blockPeak, std::abs(leftSample), std::abs(rightSample));
            }
            const auto targetRms = (float) std::sqrt(sampleEnergy / count);
            audioRms += (targetRms - audioRms) * (targetRms > audioRms ? 0.48f : 0.10f);
            const auto peakHit = juce::jlimit(0.0f, 1.0f, blockPeak * 4.0f);
            tubePeakPulse = juce::jmax(peakHit, tubePeakPulse * 0.72f);
        }
        else
        {
            audioRms *= 0.985f;
            tubePeakPulse *= 0.70f;
        }

        if (layerIntensities[0] > 0.0f)
        {
            const auto inputRms = count > 0 ? (float) std::sqrt(sampleEnergy / count) : 0.0f;
            updateParticleCloud(inputRms, blockPeak);
        }

        if (layerIntensities[1] > 0.0f || layerIntensities[3] > 0.0f
            || layerIntensities[4] > 0.0f || mode == DisplayMode::playing)
            updateFrequencyBands();

        if (mode == DisplayMode::playing)
            updateGame(count > 0 ? (float) std::sqrt(sampleEnergy / count) : 0.0f);
        else if (mode == DisplayMode::gameOver)
            gameOverLockout = juce::jmax(0.0f, gameOverLockout - 1.0f / 60.0f);

        updateDebris();

        if (layerIntensities[2] > 0.0f)
            updateGoniometerPath();

        if (layerIntensities[3] > 0.0f)
        {
            waterfall[waterfallWritePosition] = bands;
            waterfallWritePosition = (waterfallWritePosition + 1) % waterfallRows;
        }

        if (layerIntensities[4] > 0.0f)
            updateAuroraPaths();

        repaint();
    }

    float getTubeGlowLevel() const noexcept
    {
        return juce::jlimit(0.0f, 1.0f, juce::jmax(tubePeakPulse, audioRms * 2.4f));
    }

    void resized() override
    {
        const auto centreX = getWidth() / 2;
        const auto aliasRowY = juce::roundToInt(getHeight() * 0.5f - 79.0f);
        aliasEditor.setBounds(centreX - 30, aliasRowY, 165, 19);
    }

    void paint(juce::Graphics& g) override
    {
        const auto visualizerArea = juce::Rectangle<float>(0.0f, 0.0f,
            (float) getWidth(), (float) getHeight());
        g.setColour(juce::Colour(0xff121a23));
        g.fillRect(visualizerArea);
        for (int line = 0; line <= 8; ++line)
        {
            const auto x = juce::jmap((float) line, 0.0f, 8.0f,
                                      visualizerArea.getX(), visualizerArea.getRight());
            g.setColour(line == 4 ? juce::Colour(0xff344957) : juce::Colour(0xff25343e));
            g.drawVerticalLine((int) x, visualizerArea.getY(), visualizerArea.getBottom());
        }
        for (int line = 0; line <= 8; ++line)
        {
            const auto y = juce::jmap((float) line, 0.0f, 8.0f,
                                      visualizerArea.getY(), visualizerArea.getBottom());
            g.setColour(line == 4 ? juce::Colour(0xff344957) : juce::Colour(0xff25343e));
            g.drawHorizontalLine((int) y, visualizerArea.getX(), visualizerArea.getRight());
        }

        if (mode == DisplayMode::playing || mode == DisplayMode::gameOver)
        {
            drawMiniGame(g, visualizerArea);
            return;
        }

        {
            juce::Graphics::ScopedSaveState distortionState(g);
            if (crackStage >= 2)
                g.addTransform(juce::AffineTransform::translation(distortionOffset, 0.0f));

        if (layerIntensities[0] > 0.0f)
            drawParticleCloud(g, visualizerArea, layerIntensities[0]);
        if (layerIntensities[1] > 0.0f)
            drawPlasmaLadder(g, visualizerArea, layerIntensities[1]);
        if (layerIntensities[3] > 0.0f)
            drawWaterfall(g, visualizerArea, layerIntensities[3]);
        if (layerIntensities[4] > 0.0f)
            drawAurora(g, visualizerArea, layerIntensities[4]);
        if (layerIntensities[2] > 0.0f)
            drawGoniometer(g, visualizerArea, layerIntensities[2]);
        }

        drawCracks(g, visualizerArea);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (mode != DisplayMode::visualizer)
            return;

        ++clickCount;
        if (clickCount < 250)
            processor.triggerGlassTink();

        if (clickCount == 50)
        {
            crackOrigin = event.position;
            addFractures(crackOrigin, 16, 0.78f);
            crackStage = 1;
        }
        else if (clickCount == 125)
        {
            addFractures(crackOrigin, 34, 1.15f);
            addSpiderWeb(crackOrigin);
            crackStage = 2;
        }
        else if (clickCount >= 250)
        {
            processor.triggerGlassShatter();
            startMiniGame();
        }
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (mode == DisplayMode::gameOver && key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            if (gameOverLockout <= 0.0f)
                resetToVisualizer();
            return true;
        }

        if (mode != DisplayMode::playing)
            return false;

        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            const auto halfWidth = getPlayerHalfWidth();
            playerX = juce::jmax(halfWidth, playerX - getWidth() * 0.055f);
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            const auto halfWidth = getPlayerHalfWidth();
            playerX = juce::jmin((float) getWidth() - halfWidth,
                                 playerX + getWidth() * 0.055f);
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::spaceKey)
        {
            fireLasers();
            return true;
        }
        return false;
    }

    void visibilityChanged() override
    {
        if (isShowing() && mode == DisplayMode::playing)
            grabKeyboardFocus();
    }

private:
    static constexpr int sampleCount = TheAmazingNottaAudioProcessor::visualizerSamples;
    static constexpr int maxCrackPaths = 96;
    static constexpr int maxHazards = 12;
    static constexpr int maxLasers = 24;
    static constexpr int maxDebris = 64;

    float getPlayerHalfWidth() const noexcept
    {
        return juce::jmax(24.0f, getWidth() * 0.045f);
    }

    float getRecordRadius() const noexcept
    {
        return getPlayerHalfWidth() * 0.34f;
    }

    float getCassetteHalfWidth() const noexcept
    {
        return getRecordRadius() / 1.9f;
    }

    void addFractures(juce::Point<float> origin, int rays, float lengthScale)
    {
        const auto bounds = getLocalBounds().toFloat();
        for (int ray = 0; ray < rays && crackPathCount < maxCrackPaths; ++ray)
        {
            const auto angle = particleRandom.nextFloat() * juce::MathConstants<float>::twoPi;
            const auto length = juce::jmax(bounds.getWidth(), bounds.getHeight())
                * lengthScale * (0.45f + particleRandom.nextFloat() * 0.55f);
            const auto end = origin + juce::Point<float>(std::cos(angle), std::sin(angle)) * length;
            auto& path = crackPaths[(size_t) crackPathCount++];
            path.clear();
            path.startNewSubPath(origin);
            path.lineTo(end);

            if (crackPathCount < maxCrackPaths && particleRandom.nextFloat() < 0.72f)
            {
                const auto branchStart = origin + (end - origin) * (0.35f + particleRandom.nextFloat() * 0.4f);
                const auto branchAngle = angle + (particleRandom.nextBool() ? 1.0f : -1.0f)
                    * (0.35f + particleRandom.nextFloat() * 0.7f);
                const auto branchEnd = branchStart + juce::Point<float>(std::cos(branchAngle),
                    std::sin(branchAngle)) * length * (0.18f + particleRandom.nextFloat() * 0.25f);
                auto& branch = crackPaths[(size_t) crackPathCount++];
                branch.clear();
                branch.startNewSubPath(branchStart);
                branch.lineTo(branchEnd);
            }
        }
    }

    void addSpiderWeb(juce::Point<float> origin)
    {
        for (int ring = 1; ring <= 4 && crackPathCount < maxCrackPaths; ++ring)
        {
            auto& path = crackPaths[(size_t) crackPathCount++];
            path.clear();
            const auto radius = (float) ring * juce::jmin((float) getWidth(), (float) getHeight()) * 0.075f;
            for (int segment = 0; segment <= 32; ++segment)
            {
                const auto angle = segment / 32.0f * juce::MathConstants<float>::twoPi;
                const auto point = origin + juce::Point<float>(std::cos(angle), std::sin(angle)) * radius;
                if (segment == 0)
                    path.startNewSubPath(point);
                else
                    path.lineTo(point);
            }
        }
    }

    void drawCracks(juce::Graphics& g, juce::Rectangle<float>)
    {
        if (crackStage == 0)
            return;
        for (int index = 0; index < crackPathCount; ++index)
        {
            g.setColour(juce::Colour(0xff74f5ff).withAlpha(0.20f));
            g.strokePath(crackPaths[(size_t) index], juce::PathStrokeType(crackStage == 1 ? 1.4f : 1.8f));
            g.setColour(juce::Colour(0xffefffff).withAlpha(crackStage == 1 ? 0.72f : 0.88f));
            g.strokePath(crackPaths[(size_t) index], juce::PathStrokeType(crackStage == 1 ? 0.65f : 0.85f));
        }
    }

    void startMiniGame()
    {
        mode = DisplayMode::playing;
        aliasEditor.setVisible(false);
        aliasEditor.setReadOnly(false);
        aliasEditor.setCaretVisible(true);
        aliasEditor.clear();
        submittedAlias.clear();
        aliasSubmitted = false;
        score = 0;
        lives = 3;
        gameOverLockout = 0.0f;
        playerX = getWidth() * 0.5f;
        previousLowBand = previousMidBand = 0.0f;
        spawnCooldown = 0.35f;
        for (auto& hazard : hazards) hazard.active = false;
        for (auto& laser : lasers) laser.active = false;
        for (auto& piece : debris) piece.active = false;
        grabKeyboardFocus();
        repaint();
    }

    void resetToVisualizer()
    {
        mode = DisplayMode::visualizer;
        aliasEditor.setVisible(false);
        aliasEditor.setReadOnly(false);
        aliasEditor.setCaretVisible(true);
        aliasEditor.clear();
        submittedAlias.clear();
        aliasSubmitted = false;
        clickCount = 0;
        crackStage = 0;
        crackPathCount = 0;
        for (auto& path : crackPaths) path.clear();
        for (auto& hazard : hazards) hazard.active = false;
        for (auto& laser : lasers) laser.active = false;
        for (auto& piece : debris) piece.active = false;
        grabKeyboardFocus();
        repaint();
    }

    void beginGameOverInput()
    {
        submittedAlias.clear();
        aliasSubmitted = false;
        aliasEditor.clear();
        aliasEditor.setReadOnly(false);
        aliasEditor.setCaretVisible(true);
        aliasEditor.setVisible(true);
        aliasEditor.grabKeyboardFocus();
    }

    void submitAlias()
    {
        if (mode != DisplayMode::gameOver || aliasSubmitted)
            return;

        submittedAlias = aliasEditor.getText().trim().substring(0, 12);
        aliasSubmitted = true;
        aliasEditor.setReadOnly(true);
        aliasEditor.setCaretVisible(false);
        aliasEditor.setVisible(false);
        grabKeyboardFocus();
        repaint();
    }

    void fireLasers()
    {
        const auto x = playerX - getPlayerHalfWidth() * 0.42f;
        const auto y = getHeight() * 0.84f;
        for (auto& laser : lasers)
            if (!laser.active)
            {
                laser = { x, y, true };
                break;
            }
    }

    void spawnHazard(HazardType type)
    {
        for (auto& hazard : hazards)
            if (!hazard.active)
            {
                hazard.type = type;
                const auto margin = type == HazardType::record ? getRecordRadius() : getCassetteHalfWidth();
                hazard.position = { margin + particleRandom.nextFloat() * juce::jmax(1.0f, getWidth() - margin * 2.0f), -margin };
                hazard.speed = type == HazardType::record
                    ? 105.0f + particleRandom.nextFloat() * 75.0f
                    : 190.0f + particleRandom.nextFloat() * 105.0f;
                hazard.angle = particleRandom.nextFloat() * juce::MathConstants<float>::twoPi;
                hazard.spin = (particleRandom.nextFloat() - 0.5f) * 2.4f;
                hazard.active = true;
                return;
            }
    }

    void spawnDebris(juce::Point<float> position)
    {
        for (int index = 0; index < 8; ++index)
            for (auto& piece : debris)
                if (!piece.active)
                {
                    const auto angle = particleRandom.nextFloat() * juce::MathConstants<float>::twoPi;
                    piece.position = position;
                    piece.velocity = juce::Point<float>(std::cos(angle), std::sin(angle))
                        * (75.0f + particleRandom.nextFloat() * 145.0f);
                    piece.angle = angle;
                    piece.angularVelocity = (particleRandom.nextFloat() - 0.5f) * 18.0f;
                    piece.length = 3.0f + particleRandom.nextFloat() * 5.0f;
                    piece.age = 0.0f;
                    piece.lifetime = 0.25f + particleRandom.nextFloat() * 0.05f;
                    piece.active = true;
                    break;
                }
    }

    void updateDebris()
    {
        constexpr float dt = 1.0f / 60.0f;
        for (auto& piece : debris)
        {
            if (!piece.active)
                continue;
            piece.position += piece.velocity * dt;
            piece.velocity *= 0.94f;
            piece.angle += piece.angularVelocity * dt;
            piece.age += dt;
            if (piece.age >= piece.lifetime)
                piece.active = false;
        }
    }

    void updateGame(float inputRms)
    {
        constexpr float dt = 1.0f / 60.0f;
        const auto low = bands[2] + bands[3] + bands[4];
        const auto mid = bands[9] + bands[10] + bands[11];
        const auto lowTransient = low - previousLowBand;
        const auto midTransient = mid - previousMidBand;
        previousLowBand += (low - previousLowBand) * 0.24f;
        previousMidBand += (mid - previousMidBand) * 0.24f;
        spawnCooldown = juce::jmax(0.0f, spawnCooldown - dt);
        if (spawnCooldown <= 0.0f && lowTransient > 0.035f && inputRms > 0.008f)
        {
            spawnHazard(HazardType::record);
            spawnCooldown = juce::jlimit(0.12f, 0.65f, 0.48f - low * 0.035f);
        }
        if (midTransient > 0.045f && inputRms > 0.012f)
            spawnHazard(HazardType::cassette);

        for (auto& laser : lasers)
        {
            if (!laser.active)
                continue;
            laser.y -= 520.0f * dt;
            if (laser.y < -8.0f)
                laser.active = false;
        }

        const auto playerY = getHeight() * 0.84f;
        for (auto& hazard : hazards)
        {
            if (!hazard.active)
                continue;
            hazard.position.y += hazard.speed * dt;
            hazard.angle += hazard.spin * dt;
            const auto hazardRadius = hazard.type == HazardType::record
                ? getRecordRadius() : getCassetteHalfWidth() * 0.42f;
            if (hazard.position.y > getHeight() + hazardRadius)
            {
                hazard.active = false;
                continue;
            }
            const auto playerHalfWidth = getPlayerHalfWidth();
            if (std::abs(hazard.position.y - playerY) < playerHalfWidth * 0.20f + hazardRadius
                && std::abs(hazard.position.x - playerX) < playerHalfWidth * 0.48f + hazardRadius)
            {
                hazard.active = false;
                spawnDebris(hazard.position);
                if (--lives <= 0)
                {
                    mode = DisplayMode::gameOver;
                    gameOverLockout = 2.0f;
                    highScore = juce::jmax(highScore, score);
                    beginGameOverInput();
                    break;
                }
                continue;
            }
            const auto hitRadius = hazard.type == HazardType::record
                ? getRecordRadius() : getCassetteHalfWidth() * 0.42f;
            for (auto& laser : lasers)
            {
                if (laser.active && std::abs(laser.x - hazard.position.x) < hitRadius + 2.0f
                    && std::abs(laser.y - hazard.position.y) < hitRadius + 2.0f)
                {
                    laser.active = false;
                    hazard.active = false;
                    score += hazard.type == HazardType::record ? 10 : 20;
                    spawnDebris(hazard.position);
                    break;
                }
                    }
        }
        repaint();
    }

    void drawMiniGame(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour(juce::Colour(0xff090d12).withAlpha(0.96f));
        g.fillRect(bounds);
        g.setColour(juce::Colour(0xff28444a));
        for (int row = 0; row < 12; ++row)
            g.drawHorizontalLine((int) (row * bounds.getHeight() / 12.0f), bounds.getX(), bounds.getRight());

        if (mode == DisplayMode::playing)
        {
            g.setColour(juce::Colour(0xff72f2d2));
            g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
            g.drawText("BOX BAP: VINYL INVADERS", bounds.reduced(18.0f).removeFromTop(32.0f),
                juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::plain)));
            g.drawText("SCORE " + juce::String(score) + "     LIVES " + juce::String(lives),
                bounds.reduced(18.0f).removeFromTop(60.0f), juce::Justification::centredRight);

            for (const auto& hazard : hazards)
            {
                if (!hazard.active)
                    continue;
                juce::Graphics::ScopedSaveState state(g);
                g.addTransform(juce::AffineTransform::rotation(hazard.angle,
                    hazard.position.x, hazard.position.y));
                if (hazard.type == HazardType::record)
                {
                    const auto radius = getRecordRadius();
                    g.setColour(juce::Colour(0xffee7292));
                    g.drawEllipse(hazard.position.x - radius, hazard.position.y - radius,
                        radius * 2.0f, radius * 2.0f, 1.8f);
                    g.drawEllipse(hazard.position.x - radius * 0.72f, hazard.position.y - radius * 0.72f,
                        radius * 1.44f, radius * 1.44f, 0.9f);
                    g.drawEllipse(hazard.position.x - radius * 0.46f, hazard.position.y - radius * 0.46f,
                        radius * 0.92f, radius * 0.92f, 0.8f);
                    g.setColour(juce::Colour(0xffffd477));
                    g.fillEllipse(hazard.position.x - radius * 0.10f,
                        hazard.position.y - radius * 0.10f, radius * 0.20f, radius * 0.20f);
                }
                else
                {
                    const auto halfWidth = getCassetteHalfWidth();
                    const auto halfHeight = halfWidth * 0.62f;
                    g.setColour(juce::Colour(0xffffbb68));
                    g.drawRoundedRectangle(hazard.position.x - halfWidth, hazard.position.y - halfHeight,
                        halfWidth * 2.0f, halfHeight * 2.0f, 2.0f, 1.3f);
                    g.drawLine(hazard.position.x - halfWidth * 0.78f, hazard.position.y - halfHeight * 0.36f,
                        hazard.position.x + halfWidth * 0.78f, hazard.position.y - halfHeight * 0.36f, 1.0f);
                    const auto spoolRadius = halfHeight * 0.38f;
                    g.drawEllipse(hazard.position.x - halfWidth * 0.48f - spoolRadius,
                        hazard.position.y - spoolRadius * 0.1f, spoolRadius * 2.0f, spoolRadius * 2.0f, 0.9f);
                    g.drawEllipse(hazard.position.x + halfWidth * 0.48f - spoolRadius,
                        hazard.position.y - spoolRadius * 0.1f, spoolRadius * 2.0f, spoolRadius * 2.0f, 0.9f);
                }
            }
            for (const auto& laser : lasers)
                if (laser.active)
                {
                    g.setColour(juce::Colour(0xff78f7ff).withAlpha(0.35f));
                    g.drawLine(laser.x, laser.y, laser.x, laser.y - 22.0f, 7.0f);
                    g.setColour(juce::Colours::white);
                    g.drawLine(laser.x, laser.y, laser.x, laser.y - 22.0f, 2.0f);
                }

            const auto shipY = bounds.getHeight() * 0.84f;
            const auto halfWidth = getPlayerHalfWidth();
            const auto platterX = playerX - halfWidth * 0.42f;
            const auto platterRadius = halfWidth * 0.35f;
            const auto chassis = juce::Rectangle<float>(playerX - halfWidth, shipY - halfWidth * 0.30f,
                halfWidth * 2.0f, halfWidth * 0.60f);
            g.setColour(juce::Colour(0xff6fffe0).withAlpha(0.24f));
            g.drawRoundedRectangle(chassis.expanded(3.0f), 3.0f, 6.0f);
            g.setColour(juce::Colour(0xffa7fff0));
            g.drawRoundedRectangle(chassis, 2.0f, 1.8f);
            g.drawEllipse(platterX - platterRadius, shipY - platterRadius,
                platterRadius * 2.0f, platterRadius * 2.0f, 1.5f);
            g.setColour(juce::Colour(0xff72f2d2).withAlpha(0.58f));
            g.drawEllipse(platterX - platterRadius * 0.72f, shipY - platterRadius * 0.72f,
                platterRadius * 1.44f, platterRadius * 1.44f, 0.8f);
            g.setColour(juce::Colour(0xffffd477));
            g.fillEllipse(platterX - 2.0f, shipY - 2.0f, 4.0f, 4.0f);

            const juce::Point<float> tonearmPivot { playerX + halfWidth * 0.66f, shipY - halfWidth * 0.19f };
            const juce::Point<float> tonearmJoint { playerX + halfWidth * 0.38f, shipY + halfWidth * 0.04f };
            const juce::Point<float> cartridgeEnd { playerX + halfWidth * 0.18f, shipY + halfWidth * 0.18f };
            g.setColour(juce::Colour(0xffffbb68));
            g.fillEllipse(tonearmPivot.x - 2.0f, tonearmPivot.y - 2.0f, 4.0f, 4.0f);
            g.drawLine(tonearmPivot.x, tonearmPivot.y, tonearmJoint.x, tonearmJoint.y, 1.8f);
            g.drawLine(tonearmJoint.x, tonearmJoint.y, cartridgeEnd.x, cartridgeEnd.y, 1.8f);
            g.drawLine(cartridgeEnd.x - 2.0f, cartridgeEnd.y - 2.0f,
                cartridgeEnd.x + 2.0f, cartridgeEnd.y + 2.0f, 2.2f);
        }
        for (const auto& piece : debris)
            if (piece.active)
            {
                const auto alpha = juce::jlimit(0.0f, 1.0f, 1.0f - piece.age / piece.lifetime);
                const auto halfLength = piece.length * 0.5f;
                const auto offsetX = std::cos(piece.angle) * halfLength;
                const auto offsetY = std::sin(piece.angle) * halfLength;
                g.setColour(juce::Colour(0xffffe28a).withAlpha(alpha));
                g.drawLine(piece.position.x - offsetX, piece.position.y - offsetY,
                    piece.position.x + offsetX, piece.position.y + offsetY, 1.3f);
            }

        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::plain)));
        if (mode == DisplayMode::playing)
            g.drawText("ARROWS MOVE   SPACE FIRE", bounds.reduced(18.0f).removeFromBottom(24.0f),
                juce::Justification::centred);
        else
        {
            g.setColour(juce::Colour(0xffff7188));
            const auto firstLineY = bounds.getCentreY() - 100.0f;
            constexpr auto lineStep = 20.0f;
            g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
            g.drawText("GAME OVER", bounds.withY(firstLineY).withHeight(18.0f),
                juce::Justification::centred);

            if (aliasSubmitted)
            {
                g.setColour(juce::Colour(0xff72f2d2));
                g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
                g.drawText("PRODUCER: " + submittedAlias,
                    bounds.withY(firstLineY + lineStep).withHeight(18.0f), juce::Justification::centred);
            }
            else
            {
                g.setColour(juce::Colour(0xff72f2d2));
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
                g.drawText("ENTER ALIAS:",
                    bounds.withX(bounds.getCentreX() - 140.0f).withY(firstLineY + lineStep)
                        .withWidth(105.0f).withHeight(18.0f), juce::Justification::centredRight);
            }

            const std::array<juce::String, 8> gameOverLines {
                "FINAL SCORE: " + juce::String(score),
                "HIGH SCORE: " + juce::String(highScore),
                "",
                "SCREENSHOT THIS & EMAIL TO",
                "INFO@BOXBAP.COM",
                "FOR A DISCOUNT CODE!",
                "",
                "PRESS ESC TO REBOOT"
            };
            for (size_t index = 0; index < gameOverLines.size(); ++index)
            {
                g.setColour(juce::Colour(0xffffd477));
                g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::plain)));
                g.drawText(gameOverLines[index],
                    bounds.withY(firstLineY + (float) (index + 2) * lineStep).withHeight(18.0f),
                    juce::Justification::centred);
            }
        }
    }

    void updateParticleCloud(float inputRms, float inputPeak)
    {
        const auto visualizerArea = juce::Rectangle<float>(0.0f, 0.0f,
            (float) getWidth(), (float) getHeight());
        constexpr auto frameSeconds = 1.0f / 60.0f;
        const auto intensity = layerIntensities[0];
        const auto width = visualizerArea.getWidth();
        const auto rmsActivity = juce::jlimit(0.0f, 1.0f, inputRms * 7.0f);
        const auto peakActivity = juce::jlimit(0.0f, 1.0f, inputPeak * 2.8f);
        const auto normalizedAudio = juce::jmax(rmsActivity, peakActivity);
        audioEnvelope += (normalizedAudio - audioEnvelope)
            * (normalizedAudio > audioEnvelope ? 0.82f : 0.075f);

        emissionRemainder += frameSeconds * intensity * (0.35f + audioEnvelope * 90.0f);
        for (auto& particle : particles)
        {
            if (particle.age >= particle.lifetime && emissionRemainder >= 1.0f)
            {
                particle.position = { width * (0.5f + (particleRandom.nextFloat() - 0.5f)
                    * (0.025f + intensity * 0.04f + audioEnvelope * 0.10f)),
                    visualizerArea.getHeight() * 0.91f };
                particle.velocity = {
                    (particleRandom.nextFloat() - 0.5f)
                        * (7.0f + intensity * 18.0f + audioEnvelope * 180.0f),
                    -(8.0f + intensity * 14.0f + audioEnvelope * 280.0f)
                };
                particle.age = 0.0f;
                particle.lifetime = 0.7f + particleRandom.nextFloat() * (1.0f + intensity * 1.8f);
                particle.radius = 0.8f + particleRandom.nextFloat()
                    * (0.8f + intensity * 1.5f + audioEnvelope * 6.0f);
                particle.amber = particleRandom.nextFloat() < 0.28f;
                emissionRemainder -= 1.0f;
                continue;
            }

            if (particle.age >= particle.lifetime)
                continue;

            particle.age += frameSeconds;
            const auto turbulence = 5.0f + intensity * 10.0f + audioEnvelope * 500.0f;
            particle.velocity.x += (particleRandom.nextFloat() - 0.5f) * turbulence * frameSeconds;
            const auto outward = (particle.position.x - visualizerArea.getCentreX())
                / juce::jmax(1.0f, width * 0.5f);
            particle.velocity.x += outward * (7.0f + intensity * 18.0f) * frameSeconds;
            particle.position += particle.velocity * frameSeconds;

            if (particle.position.x < -12.0f || particle.position.x > visualizerArea.getRight() + 12.0f
                || particle.position.y < -12.0f)
                particle.age = particle.lifetime;
        }
    }

    void updateFrequencyBands()
    {
        double windowScale = 0.0;
        for (const auto value : window)
            windowScale += value;

        for (int band = 0; band < bandCount; ++band)
        {
            const auto bin = bandBins[(size_t) band];
            const auto coefficient = 2.0 * std::cos(juce::MathConstants<double>::twoPi
                * bin / sampleCount);
            double previous = 0.0, previous2 = 0.0;
            for (int sample = 0; sample < sampleCount; ++sample)
            {
                const auto mono = 0.5 * (left[(size_t) sample] + right[(size_t) sample]);
                const auto current = mono * window[(size_t) sample] + coefficient * previous - previous2;
                previous2 = previous;
                previous = current;
            }

            const auto power = juce::jmax(0.0, previous * previous + previous2 * previous2
                - coefficient * previous * previous2);
            const auto magnitude = (float) (2.0 * std::sqrt(power) / juce::jmax(1.0, windowScale));
            bands[(size_t) band] = juce::jlimit(0.0f, 1.0f, magnitude * 3.5f);
            bandPeaks[(size_t) band] = juce::jmax(bands[(size_t) band],
                bandPeaks[(size_t) band] * 0.965f);
        }
    }

    void updateGoniometerPath()
    {
        goniometerPath.clear();
        const auto visualizerArea = juce::Rectangle<float>(0.0f, 0.0f,
            (float) getWidth(), (float) getHeight());
        const auto centreY = visualizerArea.getCentreY();
        const auto amplitude = visualizerArea.getHeight() * 0.48f * layerIntensities[2];
        constexpr auto inverseRootTwo = 0.70710678f;

        for (int index = 0; index < sampleCount; ++index)
        {
            const auto leftSample = left[(size_t) index];
            const auto rightSample = right[(size_t) index];
            const auto mid = (leftSample + rightSample) * inverseRootTwo;
            const auto side = (leftSample - rightSample) * inverseRootTwo;
            const auto phase = (float) index / (float) sampleCount
                * juce::MathConstants<float>::twoPi * 2.0f;
            const auto resonance = juce::jlimit(-1.0f, 1.0f,
                (mid + side * std::sin(phase) * 0.65f) * 6.0f);
            const auto x = juce::jmap((float) index, 0.0f, (float) sampleCount - 1.0f,
                                      visualizerArea.getX(), visualizerArea.getWidth());
            const auto y = centreY - resonance * amplitude;
            if (index == 0)
                goniometerPath.startNewSubPath(x, y);
            else
                goniometerPath.lineTo(x, y);
        }
    }

    void updateAuroraPaths()
    {
        for (int layer = 0; layer < (int) auroraPaths.size(); ++layer)
        {
            auto& path = auroraPaths[(size_t) layer];
            path.clear();
            constexpr auto baseline = 0.01f;
            const auto engineIntensity = layerIntensities[4];
            const auto yAt = [this, layer, baseline, engineIntensity](float position)
            {
                const auto centerDistance = std::abs(position - 0.5f) * 2.0f;
                const auto bandIndex = juce::jlimit(0, bandCount - 1,
                    (int) (centerDistance * (bandCount - 1)));
                const auto audioLevel = juce::jlimit(0.0f, 1.0f, bands[(size_t) bandIndex]);
                const auto calculatedWaveHeight = engineIntensity
                    * (0.03f + audioLevel * (0.36f + layer * 0.0045f));
                const auto arch = std::sin(position * juce::MathConstants<float>::pi);
                const auto ripple = (0.5f + 0.5f * std::sin(centerDistance
                    * juce::MathConstants<float>::twoPi + animationPhase + layer * 0.4f))
                    * 0.06f * engineIntensity;
                return baseline + arch * (calculatedWaveHeight + ripple);
            };

            path.startNewSubPath(0.0f, yAt(1.0f));
            for (int point = 1; point <= 40; ++point)
            {
                const auto position = 1.0f - point / 40.0f;
                path.lineTo(0.5f * (1.0f - position), yAt(position));
            }
            for (int point = 1; point <= 40; ++point)
            {
                const auto position = point / 40.0f;
                path.lineTo(0.5f + position * 0.5f, yAt(position));
            }
            path.lineTo(1.0f, baseline);
            path.lineTo(0.0f, baseline);
            path.closeSubPath();
        }
        animationPhase += 0.045f;
    }

    void drawParticleCloud(juce::Graphics& g, juce::Rectangle<float> bounds, float intensity)
    {
        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(bounds.toNearestInt());
        for (const auto& particle : particles)
        {
            if (particle.age >= particle.lifetime)
                continue;
            const auto life = 1.0f - particle.age / particle.lifetime;
            const auto fadeIn = juce::jmin(1.0f, particle.age * 5.0f);
            const auto alpha = intensity * fadeIn * life * life * (0.28f + audioEnvelope * 0.72f);
            const auto colour = particle.amber ? juce::Colour(0xffffb454) : juce::Colour(0xff49d8ef);
            const auto radius = particle.radius * (0.75f + (1.0f - life) * 0.75f);

            g.setColour(colour.withAlpha(alpha * 0.10f));
            g.fillEllipse(particle.position.x - radius * 3.0f, particle.position.y - radius * 3.0f,
                          radius * 6.0f, radius * 6.0f);
            g.setColour(colour.withAlpha(alpha * 0.30f));
            g.fillEllipse(particle.position.x - radius * 1.7f, particle.position.y - radius * 1.7f,
                          radius * 3.4f, radius * 3.4f);
            g.setColour(colour.withAlpha(alpha));
            g.fillEllipse(particle.position.x - radius, particle.position.y - radius,
                          radius * 2.0f, radius * 2.0f);
        }
    }

    void drawPlasmaLadder(juce::Graphics& g, juce::Rectangle<float> scope, float intensity)
    {
        constexpr int segments = 20;
        constexpr int mirroredCellCount = bandCount * 2;
        const auto cellWidth = scope.getWidth() / mirroredCellCount;
        const auto cellGap = juce::jmax(1.0f, cellWidth * 0.10f);
        const auto stackHeight = scope.getHeight() * 0.94f;
        const auto segmentGap = juce::jmax(1.0f, scope.getHeight() * 0.006f);
        const auto segmentHeight = (stackHeight - segmentGap * (segments - 1)) / segments;
        const auto bottomY = scope.getBottom() - 3.0f;

        for (int band = 0; band < bandCount; ++band)
        {
            const auto litSegments = juce::jlimit(0, segments,
                (int) std::ceil(bands[(size_t) band] * segments));
            const auto holdSegment = (int) std::round(bandPeaks[(size_t) band] * (segments - 1));
            for (int side = 0; side < 2; ++side)
            {
                const auto slot = side == 0 ? band : mirroredCellCount - 1 - band;
                const auto x = scope.getX() + slot * cellWidth + cellGap * 0.5f;
                const auto barWidth = cellWidth - cellGap;
                for (int segment = 0; segment < segments; ++segment)
                {
                    const auto y = bottomY - (segment + 1) * segmentHeight - segment * segmentGap;
                    const auto proportion = (float) segment / (segments - 1);
                    const auto colour = proportion < 0.58f ? juce::Colour(0xff62f778)
                        : (proportion < 0.84f ? juce::Colour(0xffffc24b) : juce::Colour(0xffff4e56));
                    if (segment < litSegments)
                    {
                        g.setColour(colour.withAlpha(intensity * (segment == holdSegment ? 1.0f : 0.82f)));
                        g.fillRoundedRectangle(x, y, barWidth, segmentHeight, 2.0f);
                    }
                    else if (segment == holdSegment)
                    {
                        g.setColour(colour.withAlpha(intensity * 0.9f));
                        g.fillRect(x, y, barWidth, juce::jmax(2.0f, segmentHeight * 0.16f));
                    }
                    else
                    {
                        g.setColour(colour.withAlpha(intensity * 0.10f));
                        g.fillRoundedRectangle(x, y, barWidth, segmentHeight, 2.0f);
                    }
                }
            }
        }
    }

    void drawGoniometer(juce::Graphics& g, juce::Rectangle<float> scope, float intensity)
    {
        const auto centre = scope.getCentre();
        const auto radius = juce::jmin(scope.getWidth(), scope.getHeight()) * 0.46f;
        g.setColour(cyan.withAlpha(intensity * 0.28f));
        for (int ring = 1; ring <= 3; ++ring)
        {
            const auto diameter = radius * 2.0f * ring / 3.0f;
            g.drawEllipse(centre.x - diameter * 0.5f, centre.y - diameter * 0.5f,
                          diameter, diameter, 1.0f);
        }
        g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);
        g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f);
        g.setColour(cyan.withAlpha(intensity * 0.20f));
        g.strokePath(goniometerPath, juce::PathStrokeType(6.0f));
        g.setColour(juce::Colour(0xffa5f9ff).withAlpha(intensity * 0.82f));
        g.strokePath(goniometerPath, juce::PathStrokeType(1.6f));
    }

    void drawWaterfall(juce::Graphics& g, juce::Rectangle<float> scope, float intensity)
    {
        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(scope.toNearestInt());

        const auto centreX = scope.getCentreX();
        constexpr int halfBandCount = bandCount / 2;
        const auto startY = scope.getY() + scope.getHeight() * 0.025f;
        const auto rowDepth = scope.getHeight() * 0.057f;
        const auto maxPeakHeight = scope.getHeight() * 0.24f;

        for (int row = 0; row < waterfallRows; ++row)
        {
            const auto historyIndex = (waterfallWritePosition + waterfallRows - 1 - row) % waterfallRows;
            const auto depth = (float) row / (waterfallRows - 1);
            const auto halfWidth = scope.getWidth() * (0.045f + depth * 0.455f);
            const auto baseline = startY + row * rowDepth;
            const auto verticalScale = maxPeakHeight * (1.0f - depth * 0.10f);
            auto previousLeft = juce::Point<float>(centreX,
                juce::jmax(scope.getY(), baseline - waterfall[historyIndex][0] * verticalScale));
            auto previousRight = previousLeft;
            for (int band = 1; band < halfBandCount; ++band)
            {
                const auto position = (float) band / (halfBandCount - 1);
                const auto leftX = centreX - halfWidth * position;
                const auto rightX = centreX + halfWidth * position;
                const auto y = juce::jmax(scope.getY(),
                    baseline - waterfall[historyIndex][(size_t) (halfBandCount - 1 - band)] * verticalScale);
                const auto magnitude = waterfall[historyIndex][(size_t) (halfBandCount - 1 - band)];
                const auto hue = juce::jmap(magnitude, 0.0f, 1.0f,
                                             0.53f, 0.02f);
                const auto colour = juce::Colour::fromHSV(hue, 0.95f, 0.95f,
                    intensity * (0.22f + 0.58f * (1.0f - depth)));
                g.setColour(colour);
                g.drawLine(previousLeft.x, previousLeft.y, leftX, y, 1.0f);
                g.drawLine(previousRight.x, previousRight.y, rightX, y, 1.0f);
                previousLeft = { leftX, y };
                previousRight = { rightX, y };
            }
        }

        for (int band = 0; band < halfBandCount; ++band)
        {
            auto previousLeft = juce::Point<float>();
            auto previousRight = juce::Point<float>();
            for (int row = 0; row < waterfallRows; ++row)
            {
                const auto historyIndex = (waterfallWritePosition + waterfallRows - 1 - row) % waterfallRows;
                const auto depth = (float) row / (waterfallRows - 1);
                const auto halfWidth = scope.getWidth() * (0.045f + depth * 0.455f);
                const auto position = (float) band / (halfBandCount - 1);
                const auto value = waterfall[historyIndex][(size_t) (halfBandCount - 1 - band)];
                const auto y = juce::jmax(scope.getY(), startY + row * rowDepth
                    - value * maxPeakHeight * (1.0f - depth * 0.10f));
                const juce::Point<float> currentLeft { centreX - halfWidth * position, y };
                const juce::Point<float> currentRight { centreX + halfWidth * position, y };
                if (row > 0)
                {
                    g.setColour(cyan.withAlpha(intensity * 0.22f * (1.0f - depth)));
                    g.drawLine(previousLeft.x, previousLeft.y, currentLeft.x, currentLeft.y, 0.7f);
                    g.drawLine(previousRight.x, previousRight.y, currentRight.x, currentRight.y, 0.7f);
                }
                previousLeft = currentLeft;
                previousRight = currentRight;
            }
        }
    }

    void drawAurora(juce::Graphics& g, juce::Rectangle<float> scope, float intensity)
    {
        static const std::array<juce::Colour, 4> auroraColours {
            juce::Colour(0xff42f5a7), juce::Colour(0xff36d9ff),
            juce::Colour(0xff8c74ff), juce::Colour(0xffff68c4)
        };

        for (size_t index = 0; index < auroraPaths.size(); ++index)
        {
            const auto colour = auroraColours[index];
            const auto top = scope.getY() + scope.getHeight() * 0.015f;
            juce::ColourGradient gradient(colour.withAlpha(intensity * 0.28f), scope.getX(), top,
                colour.withAlpha(0.0f), scope.getX(), scope.getBottom(), false);
            g.setGradientFill(gradient);
            g.fillPath(auroraPaths[index], juce::AffineTransform::scale(scope.getWidth(), scope.getHeight())
                .translated(scope.getX(), scope.getY()));
            g.setColour(colour.withAlpha(intensity * 0.68f));
            g.strokePath(auroraPaths[index], juce::PathStrokeType(1.3f),
                juce::AffineTransform::scale(scope.getWidth(), scope.getHeight())
                    .translated(scope.getX(), scope.getY()));
        }
    }

    TheAmazingNottaAudioProcessor& processor;
    juce::TextEditor aliasEditor;
    juce::String submittedAlias;
    Samples left {}, right {};
    Samples window {};
    std::array<int, bandCount> bandBins {};
    BandRow bands {}, bandPeaks {};
    std::array<BandRow, waterfallRows> waterfall {};
    std::array<float, 5> layerIntensities {};
    std::array<Particle, particleCount> particles {};
    std::array<juce::Path, 4> auroraPaths;
    juce::Path goniometerPath;
    juce::Random particleRandom { 0x4d4f4a4f };
    std::array<juce::Path, maxCrackPaths> crackPaths;
    std::array<Hazard, maxHazards> hazards {};
    std::array<Laser, maxLasers> lasers {};
    std::array<Debris, maxDebris> debris {};
    DisplayMode mode = DisplayMode::visualizer;
    juce::Point<float> crackOrigin;
    float audioEnvelope = 0.0f;
    float emissionRemainder = 0.0f;
    float playerX = 0.0f;
    float previousLowBand = 0.0f;
    float previousMidBand = 0.0f;
    float spawnCooldown = 0.0f;
    float distortionOffset = 0.0f;
    float gameOverLockout = 0.0f;
    int clickCount = 0;
    int crackPathCount = 0;
    int crackStage = 0;
    int score = 0;
    int highScore = 0;
    int lives = 3;
    bool aliasSubmitted = false;
    int waterfallWritePosition = 0;
    float animationPhase = 0.0f;
    float audioRms = 0.0f;
    float tubePeakPulse = 0.0f;
};

class TheAmazingNottaAudioProcessorEditor::KnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit KnobLookAndFeel(juce::Image image) : knobImage(std::move(image)) {}

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto size = (float) juce::jmin(width, height);
        const auto left = (float) x + ((float) width - size) * 0.5f;
        const auto top = (float) y + ((float) height - size) * 0.5f;
        const auto centreX = left + size * 0.5f;
        const auto centreY = top + size * 0.5f;

        if (!knobImage.isValid())
        {
            g.drawLine(centreX, centreY, centreX, top + size * 0.12f, 2.0f);
            return;
        }

        const auto angle = juce::jmap(sliderPosition, rotaryStartAngle, rotaryEndAngle);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        juce::Graphics::ScopedSaveState saveState(g);
        g.addTransform(juce::AffineTransform::rotation(angle, centreX, centreY));
        g.drawImage(knobImage, (int) left, (int) top, (int) size, (int) size,
                    0, 0, knobImage.getWidth(), knobImage.getHeight());
    }

private:
    juce::Image knobImage;
};

class TheAmazingNottaAudioProcessorEditor::SkinArrowButton final : public juce::Button
{
public:
    explicit SkinArrowButton(bool pointsRight)
        : juce::Button(pointsRight ? "Next skin" : "Previous skin"), right(pointsRight)
    {
    }

    void paintButton(juce::Graphics& g, bool mouseOver, bool buttonDown) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto alpha = (mouseOver || buttonDown) ? 1.0f : 0.3f;
        g.setColour(juce::Colour(0xfffff2d8).withAlpha(alpha));
        const auto centreX = bounds.getCentreX();
        const auto centreY = bounds.getCentreY();
        const auto direction = right ? 1.0f : -1.0f;
        const auto tipX = centreX + direction * bounds.getWidth() * 0.28f;
        const auto baseX = centreX - direction * bounds.getWidth() * 0.20f;
        const auto halfHeight = bounds.getHeight() * 0.20f;
        g.drawLine(baseX, centreY - halfHeight, tipX, centreY, 2.2f);
        g.drawLine(tipX, centreY, baseX, centreY + halfHeight, 2.2f);
    }

private:
    bool right;
};

class TheAmazingNottaAudioProcessorEditor::TubeToggleButton final : public juce::Button
{
public:
    TubeToggleButton(juce::Image offImage, juce::Image onImage, const juce::String& name)
        : juce::Button(name), offStateImage(std::move(offImage)), onStateImage(std::move(onImage))
    {
        setClickingTogglesState(true);
        setToggleState(false, juce::dontSendNotification);
    }

    void paintButton(juce::Graphics& g, bool, bool) override
    {
        const auto& image = getToggleState() ? onStateImage : offStateImage;
        if (image.isValid())
        {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImageWithin(image, 0, 0, getWidth(), getHeight(),
                              juce::RectanglePlacement::centred);
        }
    }

private:
    juce::Image offStateImage;
    juce::Image onStateImage;
};

class TheAmazingNottaAudioProcessorEditor::TubeLayer final : public juce::Component
{
public:
    TubeLayer(juce::Image tubeImage, bool isLeftTube)
        : image(std::move(tubeImage)), leftTube(isLeftTube)
    {
        setInterceptsMouseClicks(false, false);
        if (image.isValid())
        {
            glowImage = juce::Image(juce::Image::ARGB, image.getWidth(), image.getHeight(), true);
            const juce::Image::BitmapData source(image, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData destination(glowImage, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < image.getHeight(); ++y)
            {
                for (int x = 0; x < image.getWidth(); ++x)
                {
                    const auto pixel = source.getPixelColour(x, y);
                    const auto alpha = pixel.getAlpha();
                    const auto red = juce::jmin(255, (int) std::round(pixel.getRed() * 1.24f + 32.0f));
                    const auto green = juce::jmin(255, (int) std::round(pixel.getGreen() * 1.10f + 17.0f));
                    const auto blue = juce::jmin(255, (int) std::round(pixel.getBlue() * 0.84f));
                    destination.setPixelColour(x, y,
                        juce::Colour::fromRGBA((juce::uint8) red, (juce::uint8) green,
                                               (juce::uint8) blue, (juce::uint8) alpha));
                }
            }
        }
    }

    void setVisualState(float opacity, float filamentGlow)
    {
        const auto clampedOpacity = juce::jlimit(0.0f, 1.0f, opacity);
        const auto clampedGlow = juce::jlimit(0.0f, 1.0f, filamentGlow);
        if (std::abs(getAlpha() - clampedOpacity) > 0.002f
            || std::abs(glow - clampedGlow) > 0.002f)
        {
            glow = clampedGlow;
            setAlpha(clampedOpacity);
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (image.isValid())
        {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(image, 0, 0, getWidth(), getHeight(),
                        0, 0, image.getWidth(), image.getHeight());
        }

        if (glow <= 0.001f || !glowImage.isValid())
            return;

        g.beginTransparencyLayer(glow * 0.88f);
        g.drawImage(glowImage, 0, 0, getWidth(), getHeight(),
                    0, 0, glowImage.getWidth(), glowImage.getHeight());
        g.endTransparencyLayer();
    }

private:
    juce::Image image;
    juce::Image glowImage;
    bool leftTube;
    float glow = 0.0f;
};

namespace
{
class RotatingReel final : public juce::Component
{
public:
    explicit RotatingReel(juce::Image sourceImage) : image(std::move(sourceImage)) {}

    void setRotation(float radians)
    {
        angle = std::remainder(radians, juce::MathConstants<float>::twoPi);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (!image.isValid())
            return;

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(angle,
            getWidth() * 0.5f, getHeight() * 0.5f));
        g.drawImage(image, 0, 0, getWidth(), getHeight(),
                    0, 0, image.getWidth(), image.getHeight());
    }

private:
    juce::Image image;
    float angle = 0.0f;
};
}

class TheAmazingNottaAudioProcessorEditor::ReelToReelComponent final : public juce::Component
{
public:
    ReelToReelComponent(juce::Image backgroundImage, juce::Image leftReelImage,
                        juce::Image rightReelImage)
    {
        backgroundImageData = std::move(backgroundImage);
        background.setImage(backgroundImageData, juce::RectanglePlacement::stretchToFit);
        background.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(background);

        reels[0] = std::make_unique<RotatingReel>(std::move(leftReelImage));
        reels[1] = std::make_unique<RotatingReel>(std::move(rightReelImage));
        for (auto& reel : reels)
        {
            reel->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(*reel);
        }
    }

    void setActive(bool shouldPlay)
    {
        if (shouldPlay == active)
            return;

        slideAnimator.cancelAnimation(this, false);
        hasBeenActivated = true;
        active = shouldPlay;

        if (active)
        {
            const auto scale = getParentWidth() / 1500.0f;
            setBounds(0, juce::roundToInt(tuckedDesignYOffset * scale), getParentWidth(), getParentHeight());
            setVisible(true);
            exitPending = false;
            slideAnimator.animateComponent(this, getWorkingBounds(), 1.0f, revealDurationMilliseconds,
                false, 0.0, 0.0);
        }
        else
        {
            exitPending = true;
            slideAnimator.animateComponent(this, getExitBounds(), 1.0f, exitDurationMilliseconds,
                false, 0.0, 0.0);
        }
    }

    void chooseRandomSpeed()
    {
        targetSpeedMultiplier = 0.5f + speedRandom.nextFloat();
    }

    void setEditorSize(int width, int height, float scale)
    {
        if (!hasBeenActivated)
            setBounds(0, juce::roundToInt(tuckedDesignYOffset * scale), width, height);
        else
            setSize(width, height);
    }

    void advanceRotation(float audioRms)
    {
        if (exitPending && !slideAnimator.isAnimating(this))
        {
            exitPending = false;
            setVisible(false);
        }

        constexpr auto frameSeconds = 1.0f / 60.0f;
        const auto hasAudio = audioRms > 0.0035f;
        const auto targetAngularSpeed = active && hasAudio
            ? baseAngularSpeed * targetSpeedMultiplier : 0.0f;
        const auto speedBlend = targetAngularSpeed > angularSpeed ? 0.24f : 0.045f;
        angularSpeed += (targetAngularSpeed - angularSpeed) * speedBlend;
        if (std::abs(angularSpeed) < 0.015f && targetAngularSpeed == 0.0f)
            angularSpeed = 0.0f;

        rotations[0] = std::remainder(rotations[0] - angularSpeed * frameSeconds,
                                      juce::MathConstants<float>::twoPi);
        rotations[1] = std::remainder(rotations[1] - angularSpeed * frameSeconds * 0.92f,
                                      juce::MathConstants<float>::twoPi);
        reels[0]->setRotation(rotations[0]);
        reels[1]->setRotation(rotations[1]);
    }

    void resized() override
    {
        background.setBounds(getLocalBounds());
        const auto scale = (float) getWidth() / 1500.0f;
        constexpr float reelSize = 277.0f;
        constexpr std::array<float, 2> centreX { 568.0f, 932.0f };
        constexpr auto centreY = 1339.0f;
        const auto scaledSize = juce::roundToInt(reelSize * scale);
        for (size_t index = 0; index < reels.size(); ++index)
        {
            const auto x = centreX[index] * scale;
            const auto y = centreY * scale;
            reels[index]->setBounds(juce::roundToInt(x - scaledSize * 0.5f),
                juce::roundToInt(y - scaledSize * 0.5f), scaledSize, scaledSize);
        }
    }

private:
    juce::Rectangle<int> getWorkingBounds() const
    {
        return { 0, 0, getParentWidth(), getParentHeight() };
    }

    juce::Rectangle<int> getExitBounds() const
    {
        return { 0, getParentHeight() + 1, getParentWidth(), getParentHeight() };
    }

    juce::Image backgroundImageData;
    juce::ImageComponent background;
    std::array<std::unique_ptr<RotatingReel>, 2> reels;
    juce::ComponentAnimator slideAnimator;
    juce::Random speedRandom { 0x5245454c };
    std::array<float, 2> rotations {};
    float angularSpeed = 0.0f;
    float targetSpeedMultiplier = 1.0f;
    bool active = false;
    bool hasBeenActivated = false;
    bool exitPending = false;
    static constexpr float tuckedDesignYOffset = -304.0f;
    static constexpr int revealDurationMilliseconds = 520;
    static constexpr int exitDurationMilliseconds = 1300;
    static constexpr float baseAngularSpeed = 2.2f;
};

class TheAmazingNottaAudioProcessorEditor::BoxBapLogoBanner final : public juce::Component
{
public:
    explicit BoxBapLogoBanner(juce::Image sourceImage)
        : image(std::move(sourceImage)), highlightedImage(makeHighlight(image))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setInterceptsMouseClicks(true, true);
    }

    bool hitTest(int x, int y) override
    {
        if (!image.isValid() || getWidth() <= 0 || getHeight() <= 0
            || x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
            return false;

        const auto imageX = juce::jlimit(0, image.getWidth() - 1,
            x * image.getWidth() / getWidth());
        const auto imageY = juce::jlimit(0, image.getHeight() - 1,
            y * image.getHeight() / getHeight());
        return image.getPixelAt(imageX, imageY).getAlpha() > 8;
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        hovered = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hovered = false;
        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        juce::URL("https://www.boxbap.com").launchInDefaultBrowser();
    }

    void paint(juce::Graphics& g) override
    {
        if (!image.isValid())
            return;

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(image, 0, 0, getWidth(), getHeight(),
                    0, 0, image.getWidth(), image.getHeight());
        if (hovered && highlightedImage.isValid())
        {
            g.beginTransparencyLayer(0.42f);
            g.drawImage(highlightedImage, 0, 0, getWidth(), getHeight(),
                        0, 0, highlightedImage.getWidth(), highlightedImage.getHeight());
            g.endTransparencyLayer();
        }
    }

private:
    static juce::Image makeHighlight(const juce::Image& source)
    {
        if (!source.isValid())
            return {};

        juce::Image result(juce::Image::ARGB, source.getWidth(), source.getHeight(), true);
        const juce::Image::BitmapData sourcePixels(source, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData resultPixels(result, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < source.getHeight(); ++y)
        {
            for (int x = 0; x < source.getWidth(); ++x)
            {
                const auto pixel = sourcePixels.getPixelColour(x, y);
                resultPixels.setPixelColour(x, y, juce::Colour::fromRGBA(
                    (juce::uint8) juce::jmin(255, (int) std::round(pixel.getRed() * 1.12f + 22.0f)),
                    (juce::uint8) juce::jmin(255, (int) std::round(pixel.getGreen() * 1.12f + 22.0f)),
                    (juce::uint8) juce::jmin(255, (int) std::round(pixel.getBlue() * 1.12f + 22.0f)),
                    (juce::uint8) pixel.getAlpha()));
            }
        }
        return result;
    }

    juce::Image image;
    juce::Image highlightedImage;
    bool hovered = false;
};

class TheAmazingNottaAudioProcessorEditor::ScrewSpinButton final : public juce::Button
{
public:
    explicit ScrewSpinButton(juce::Image sourceImage)
        : juce::Button("Box Bap banner screw"), image(std::move(sourceImage))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void startSkinSpin()
    {
        spinElapsed = 0.0f;
        spinning = true;
        repaint();
    }

    void advanceAnimation()
    {
        if (!spinning)
            return;
        spinElapsed += 1.0f / 60.0f;
        if (spinElapsed >= spinDuration)
        {
            spinElapsed = spinDuration;
            spinning = false;
        }
        repaint();
    }

    void paintButton(juce::Graphics& g, bool mouseOver, bool buttonDown) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        if (mouseOver || buttonDown)
        {
            g.setColour(juce::Colour(0x38fff1d2));
            g.fillEllipse(bounds.reduced(bounds.getWidth() * 0.08f));
        }

        if (!image.isValid())
            return;

        const auto progress = spinDuration > 0.0f ? spinElapsed / spinDuration : 1.0f;
        const auto easedProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
        const auto angle = spinning || spinElapsed >= spinDuration
            ? easedProgress * juce::MathConstants<float>::twoPi : 0.0f;
        juce::Graphics::ScopedSaveState state(g);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
        g.drawImage(image, 0, 0, getWidth(), getHeight(),
                    0, 0, image.getWidth(), image.getHeight());
    }

private:
    juce::Image image;
    float spinElapsed = 0.0f;
    bool spinning = false;
    static constexpr float spinDuration = 0.28f;
};

TheAmazingNottaAudioProcessorEditor::TheAmazingNottaAudioProcessorEditor(
    TheAmazingNottaAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor), processor(audioProcessor)
{
    setResizable(true, false);
    getConstrainer()->setFixedAspectRatio(1.0);

    knobImage = juce::ImageCache::getFromMemory(BinaryData::Knob001_png,
                                                BinaryData::Knob001_pngSize);
    toggleOffImage = juce::ImageCache::getFromMemory(BinaryData::OffOnToggleALLAOFF_png,
        BinaryData::OffOnToggleALLAOFF_pngSize);
    toggleOnImage = juce::ImageCache::getFromMemory(BinaryData::OffOnToggleALLAON_png,
        BinaryData::OffOnToggleALLAON_pngSize);
    boxBapLogoImage = juce::ImageCache::getFromMemory(BinaryData::BOXBAPLogo_png,
        BinaryData::BOXBAPLogo_pngSize);
    screwButtonImage = juce::ImageCache::getFromMemory(BinaryData::SCREWYOUBUTTON_png,
        BinaryData::SCREWYOUBUTTON_pngSize);
    tubeLeftImage = juce::ImageCache::getFromMemory(BinaryData::TUBELEFT_png,
        BinaryData::TUBELEFT_pngSize);
    tubeRightImage = juce::ImageCache::getFromMemory(BinaryData::TUBERIGHT_png,
        BinaryData::TUBERIGHT_pngSize);
    skinImages = {
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDCOLOR_png, BinaryData::BACKGROUNDCOLOR_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDDRIP_png, BinaryData::BACKGROUNDDRIP_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDHEAT_png, BinaryData::BACKGROUNDHEAT_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDIDK_png, BinaryData::BACKGROUNDIDK_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDJUICE_png, BinaryData::BACKGROUNDJUICE_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDMAGIC_png, BinaryData::BACKGROUNDMAGIC_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDMOJO_png, BinaryData::BACKGROUNDMOJO_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDSAUCE_png, BinaryData::BACKGROUNDSAUCE_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDSOMETHIN_png, BinaryData::BACKGROUNDSOMETHIN_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDVIBE_png, BinaryData::BACKGROUNDVIBE_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::BACKGROUNDWARMTH_png, BinaryData::BACKGROUNDWARMTH_pngSize)
    };
    knobLookAndFeel = std::make_unique<KnobLookAndFeel>(knobImage);

    smokeBackground = std::make_unique<SmokeBackgroundComponent>();
    smokeBackground->setSkinIndex(activeSkinIndex);
    addAndMakeVisible(*smokeBackground);

    reelToReel = std::make_unique<ReelToReelComponent>(
        juce::ImageCache::getFromMemory(BinaryData::Reel2ReelAll_png,
                                        BinaryData::Reel2ReelAll_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::LEFTREEL_png, BinaryData::LEFTREEL_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::RIGHTREEL_png, BinaryData::RIGHTREEL_pngSize));
    addChildComponent(*reelToReel);

    logoBanner = std::make_unique<BoxBapLogoBanner>(boxBapLogoImage);
    addAndMakeVisible(*logoBanner);

    tubeLayers[0] = std::make_unique<TubeLayer>(tubeLeftImage, true);
    tubeLayers[1] = std::make_unique<TubeLayer>(tubeRightImage, false);
    tubeAnimations[0].xOffset = 210.0f;
    tubeAnimations[1].xOffset = -210.0f;
    for (size_t index = 0; index < tubeLayers.size(); ++index)
    {
        tubeLayers[index]->setVisualState(0.0f, 0.0f);
        addAndMakeVisible(*tubeLayers[index]);
    }

    visualizer = std::make_unique<Visualizer>(processor);
    addAndMakeVisible(*visualizer);

    backgroundImage.setImage(skinImages[(size_t) activeSkinIndex],
                              juce::RectanglePlacement::stretchToFit);
    backgroundImage.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(backgroundImage);
    backgroundImage.toFront(false);

    for (size_t index = 0; index < tubeToggleButtons.size(); ++index)
    {
        tubeToggleButtons[index] = std::make_unique<TubeToggleButton>(toggleOffImage, toggleOnImage,
            index == 0 ? "Left vacuum tube" : "Right vacuum tube");
        if (index == 0)
            tubeToggleButtons[index]->onClick = [this]
            {
                const auto shouldTurnOn = tubeToggleButtons[0]->getToggleState();
                setTubeOn(0, shouldTurnOn);
                setTubeOn(1, shouldTurnOn);
            };
        else
            tubeToggleButtons[index]->onClick = [this]
            {
                reelToReel->setActive(tubeToggleButtons[1]->getToggleState());
            };
        addAndMakeVisible(*tubeToggleButtons[index]);
    }

    static const std::array<const char*, 5> names {
        "Spectral Flux Density",
        "Harmonic Dispersal",
        "Resonant Coherence",
        "Transient Topology",
        "Phase Elasticity"
    };

    for (size_t index = 0; index < knobs.size(); ++index)
    {
        auto& knob = knobs[index];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange(0.0, 1.0, 0.0);
        knob.setValue(0.0, juce::dontSendNotification);
        knob.setDoubleClickReturnValue(true, 0.0);
        knob.setRotaryParameters(juce::degreesToRadians(-135.0f),
                                 juce::degreesToRadians(135.0f), true);
        knob.setLookAndFeel(knobLookAndFeel.get());
        knob.setTooltip(names[index]);
        knob.onDragStart = [this, index]
        {
            reelToReel->chooseRandomSpeed();
            constexpr std::array<float, 5> knobCentresX { 512.0f, 631.0f, 750.0f, 869.0f, 988.0f };
            const auto scale = (float) getWidth() / 1500.0f;
            smokeBackground->beginKnobDrag({ knobCentresX[index] * scale, 1080.0f * scale });
        };
        knob.onDragEnd = [this] { smokeBackground->endKnobDrag(); };
        addAndMakeVisible(knob);
    }

    previousSkinButton = std::make_unique<SkinArrowButton>(false);
    nextSkinButton = std::make_unique<SkinArrowButton>(true);
    previousSkinButton->onClick = [this] { changeSkin(-1); };
    nextSkinButton->onClick = [this] { changeSkin(1); };
    previousSkinButton->setTooltip("Previous chassis finish");
    nextSkinButton->setTooltip("Next chassis finish");
    addAndMakeVisible(*previousSkinButton);
    addAndMakeVisible(*nextSkinButton);

    screwButton = std::make_unique<ScrewSpinButton>(screwButtonImage);
    screwButton->onClick = [this] { toggleLogoBanner(); };
    screwButton->setTooltip("Toggle Box Bap logo banner");
    addAndMakeVisible(*screwButton);

    setSize(1000, 1000);
    startTimerHz(60);
}

TheAmazingNottaAudioProcessorEditor::~TheAmazingNottaAudioProcessorEditor()
{
    stopTimer();
    for (auto& knob : knobs)
        knob.setLookAndFeel(nullptr);
}
void TheAmazingNottaAudioProcessorEditor::toggleLogoBanner()
{
    logoBannerVisible = !logoBannerVisible;
}
void TheAmazingNottaAudioProcessorEditor::advanceLogoBanner()
{
    constexpr auto frameSeconds = 1.0f / 60.0f;
    const auto scale = (float) getWidth() / 1500.0f;
    const auto targetOffset = logoBannerVisible ? 0.0f : 420.0f;
    const auto blend = 1.0f - std::exp(-frameSeconds / 0.20f);
    logoBannerOffsetY += (targetOffset - logoBannerOffsetY) * blend;
    if (std::abs(targetOffset - logoBannerOffsetY) < 0.5f)
        logoBannerOffsetY = targetOffset;

    logoBanner->setBounds(0, juce::roundToInt(logoBannerOffsetY * scale), getWidth(), getHeight());
}

void TheAmazingNottaAudioProcessorEditor::setTubeOn(size_t tubeIndex, bool shouldTurnOn)
{
    if (tubeIndex >= tubeAnimations.size())
        return;

    auto& animation = tubeAnimations[tubeIndex];
    if (animation.targetOn == shouldTurnOn)
        return;

    animation.targetOn = shouldTurnOn;
    animation.elapsed = 0.0f;
    if (shouldTurnOn)
    {
        animation.phase = TubeAnimation::Phase::opening;
        animation.startX = (tubeIndex == 0 ? 210.0f : -210.0f);
        animation.startY = 0.0f;
        animation.startAlpha = 0.0f;
        animation.xOffset = animation.startX;
        animation.yOffset = 0.0f;
        animation.alpha = 0.0f;
        tubeLayers[tubeIndex]->setVisualState(0.0f, 0.0f);
    }
    else if (animation.alpha > 0.0f)
    {
        animation.phase = TubeAnimation::Phase::unplugJerk;
        animation.startX = animation.xOffset;
        animation.startY = animation.yOffset;
        animation.startAlpha = animation.alpha;
        tubeLayers[tubeIndex]->setVisualState(animation.alpha, 0.0f);
    }
    else
    {
        animation.phase = TubeAnimation::Phase::hidden;
        animation.alpha = 0.0f;
        tubeLayers[tubeIndex]->setVisualState(0.0f, 0.0f);
    }
}

void TheAmazingNottaAudioProcessorEditor::updateTubeAnimations(float audioRms)
{
    constexpr auto frameSeconds = 1.0f / 60.0f;
    const auto scale = (float) getWidth() / 1500.0f;
    const auto outwardOffset = 520.0f;

    for (size_t index = 0; index < tubeAnimations.size(); ++index)
    {
        auto& animation = tubeAnimations[index];
        const auto outwardDirection = index == 0 ? -1.0f : 1.0f;

        if (animation.phase == TubeAnimation::Phase::opening)
        {
            constexpr auto duration = 0.52f;
            animation.elapsed += frameSeconds;
            const auto progress = juce::jlimit(0.0f, 1.0f, animation.elapsed / duration);
            const auto eased = 1.0f - std::pow(1.0f - progress, 3.0f);
            const auto spring = std::exp(-6.0f * progress) * std::sin(progress * 12.0f);
            animation.xOffset = animation.startX * (1.0f - eased)
                + outwardDirection * 18.0f * spring;
            animation.yOffset = animation.startY
                + 9.0f * std::exp(-6.0f * progress) * std::sin(progress * 12.0f);
            animation.alpha = progress * progress * (3.0f - 2.0f * progress);
            if (progress >= 1.0f)
            {
                animation.phase = TubeAnimation::Phase::seated;
                animation.xOffset = 0.0f;
                animation.yOffset = 0.0f;
                animation.alpha = 1.0f;
            }
        }
        else if (animation.phase == TubeAnimation::Phase::unplugJerk)
        {
            constexpr auto jerkDuration = 0.085f;
            animation.elapsed += frameSeconds;
            const auto progress = juce::jlimit(0.0f, 1.0f, animation.elapsed / jerkDuration);
            const auto eased = progress * progress * (3.0f - 2.0f * progress);
            animation.xOffset = animation.startX + outwardDirection * 28.0f * eased;
            animation.yOffset = animation.startY + 14.0f * eased;
            animation.alpha = animation.startAlpha;
            if (progress >= 1.0f)
            {
                animation.phase = TubeAnimation::Phase::drifting;
                animation.elapsed = 0.0f;
                animation.startX = animation.xOffset;
                animation.startY = animation.yOffset;
                animation.startAlpha = animation.alpha;
            }
        }
        else if (animation.phase == TubeAnimation::Phase::drifting)
        {
            constexpr auto driftDuration = 0.42f;
            animation.elapsed += frameSeconds;
            const auto progress = juce::jlimit(0.0f, 1.0f, animation.elapsed / driftDuration);
            const auto eased = progress * progress * (3.0f - 2.0f * progress);
            animation.xOffset = animation.startX
                + (outwardDirection * outwardOffset - animation.startX) * eased;
            animation.yOffset = animation.startY + 18.0f * eased;
            animation.alpha = animation.startAlpha * (1.0f - eased);
            if (progress >= 1.0f)
            {
                animation.phase = TubeAnimation::Phase::hidden;
                animation.alpha = 0.0f;
            }
        }

        tubeLayers[index]->setBounds(juce::roundToInt(animation.xOffset * scale),
            juce::roundToInt(animation.yOffset * scale), getWidth(), getHeight());
        const auto glow = animation.phase == TubeAnimation::Phase::seated && animation.targetOn
            ? juce::jlimit(0.0f, 1.0f, audioRms * 8.0f) : 0.0f;
        tubeLayers[index]->setVisualState(animation.alpha, glow);
    }
}

void TheAmazingNottaAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(slate);
}

void TheAmazingNottaAudioProcessorEditor::resized()
{
    constexpr float designSize = 1500.0f;
    constexpr float screwDiameter = 21.0f;
    constexpr float knobDiameter = 88.0f;
    constexpr float arrowWidth = 32.0f;
    constexpr float arrowHeight = 48.0f;
    constexpr float tubeToggleWidth = 86.0f;
    constexpr float tubeToggleHeight = 145.0f;
    constexpr std::array<float, 5> knobCentresX { 512.0f, 631.0f, 750.0f, 869.0f, 988.0f };
    constexpr std::array<float, 2> tubeToggleCentresX { 415.0f, 1085.0f };
    const auto scale = (float) getWidth() / designSize;

    smokeBackground->setBounds(getLocalBounds());
    reelToReel->setEditorSize(getWidth(), getHeight(), scale);
    logoBanner->setBounds(0, juce::roundToInt(logoBannerOffsetY * scale), getWidth(), getHeight());
    backgroundImage.setBounds(getLocalBounds());
    visualizer->setBounds(juce::roundToInt(530.0f * scale),
        juce::roundToInt(660.0f * scale), juce::roundToInt(440.0f * scale),
        juce::roundToInt(350.0f * scale));

    const auto knobSize = juce::roundToInt(knobDiameter * scale);
    const auto knobCentreY = 1080.0f * scale;
    for (size_t index = 0; index < knobs.size(); ++index)
    {
        const auto centreX = knobCentresX[index] * scale;
        knobs[index].setBounds(juce::roundToInt(centreX - knobSize * 0.5f),
            juce::roundToInt(knobCentreY - knobSize * 0.5f), knobSize, knobSize);
    }

    for (size_t index = 0; index < tubeLayers.size(); ++index)
    {
        tubeLayers[index]->setBounds(juce::roundToInt(tubeAnimations[index].xOffset * scale),
            juce::roundToInt(tubeAnimations[index].yOffset * scale), getWidth(), getHeight());
        const auto toggleWidth = juce::roundToInt(tubeToggleWidth * scale);
        const auto toggleHeight = juce::roundToInt(tubeToggleHeight * scale);
        const auto centreX = tubeToggleCentresX[index] * scale;
        const auto centreY = 735.0f * scale;
        tubeToggleButtons[index]->setBounds(juce::roundToInt(centreX - toggleWidth * 0.5f),
            juce::roundToInt(centreY - toggleHeight * 0.5f), toggleWidth, toggleHeight);
    }

    const auto arrowButtonWidth = juce::roundToInt(arrowWidth * scale);
    const auto arrowButtonHeight = juce::roundToInt(arrowHeight * scale);
    const auto arrowCentreY = 750.0f * scale;
    const auto leftCentreX = 325.0f * scale;
    const auto rightCentreX = 1175.0f * scale;
    previousSkinButton->setBounds(juce::roundToInt(leftCentreX - arrowButtonWidth * 0.5f),
        juce::roundToInt(arrowCentreY - arrowButtonHeight * 0.5f),
        arrowButtonWidth, arrowButtonHeight);
    nextSkinButton->setBounds(juce::roundToInt(rightCentreX - arrowButtonWidth * 0.5f),
        juce::roundToInt(arrowCentreY - arrowButtonHeight * 0.5f),
        arrowButtonWidth, arrowButtonHeight);

    const auto screwSize = juce::roundToInt(screwDiameter * scale);
    const auto screwCentreX = 750.0f * scale;
    const auto screwCentreY = 582.0f * scale;
    screwButton->setBounds(juce::roundToInt(screwCentreX - screwSize * 0.5f),
        juce::roundToInt(screwCentreY - screwSize * 0.5f), screwSize, screwSize);
}

void TheAmazingNottaAudioProcessorEditor::changeSkin(int direction)
{
    constexpr auto skinCount = 11;
    activeSkinIndex = (activeSkinIndex + direction + skinCount) % skinCount;
    smokeBackground->setSkinIndex(activeSkinIndex);
    screwButton->startSkinSpin();
    backgroundImage.setImage(skinImages[(size_t) activeSkinIndex],
                             juce::RectanglePlacement::stretchToFit);
    backgroundImage.repaint();
    repaint();
}

void TheAmazingNottaAudioProcessorEditor::parentHierarchyChanged()
{
    if (!juce::JUCEApplicationBase::isStandaloneApp() || getParentComponent() == nullptr)
        return;

    juce::Component::SafePointer<TheAmazingNottaAudioProcessorEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis]
    {
        if (safeThis == nullptr)
            return;

        auto* topLevel = safeThis->getTopLevelComponent();
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (topLevel == nullptr || display == nullptr)
            return;

        const auto area = display->userArea;
        const auto maxX = juce::jmax(area.getX(), area.getRight() - topLevel->getWidth());
        const auto maxY = juce::jmax(area.getY(), area.getBottom() - topLevel->getHeight());
        const auto x = juce::jlimit(area.getX(), maxX,
            area.getCentreX() - topLevel->getWidth() / 2);
        const auto y = juce::jlimit(area.getY(), maxY,
            area.getCentreY() - topLevel->getHeight() / 2);
        topLevel->setTopLeftPosition(x, y);
        topLevel->toFront(true);
    });
}

void TheAmazingNottaAudioProcessorEditor::timerCallback()
{
    std::array<float, 5> effectiveIntensities {};
    for (size_t index = 0; index < knobs.size(); ++index)
    {
        effectiveIntensities[index] = (float) knobs[index].getValue();
    }

    visualizer->setLayerIntensities(effectiveIntensities);
    visualizer->updateFromAudio();
    smokeBackground->setControls((float) knobs[0].getValue(),
        (float) knobs[1].getValue(), (float) knobs[2].getValue());
    smokeBackground->advance(visualizer->getTubeGlowLevel());
    reelToReel->advanceRotation(visualizer->getTubeGlowLevel());
    updateTubeAnimations(visualizer->getTubeGlowLevel());
    advanceLogoBanner();
    screwButton->advanceAnimation();
}
