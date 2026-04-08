#include "TransportControls.h"

namespace spectralz
{

// TransportButton implementation
TransportControls::TransportButton::TransportButton(const juce::String& name, ButtonType type)
    : juce::Button(name), buttonType(type)
{
}

void TransportControls::TransportButton::paintButton(juce::Graphics& g,
                                                      bool shouldDrawButtonAsHighlighted,
                                                      bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background - only show when engaged (toggle on) or pressed
    // Skip background for Loop button - it uses icon color instead
    if (buttonType != ButtonType::Loop)
    {
        if (getToggleState())
        {
            g.setColour(juce::Colour(0xff4a9eff));  // Blue when engaged
            g.fillRoundedRectangle(bounds, 4.0f);
        }
        else if (shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colour(0xff4a9eff).withAlpha(0.5f));  // Semi-transparent blue when pressed
            g.fillRoundedRectangle(bounds, 4.0f);
        }
    }
    // No background for normal/highlighted state - transparent

    // Icon
    g.setColour(juce::Colours::white);
    auto iconBounds = bounds.reduced(bounds.getWidth() * 0.25f);

    switch (buttonType)
    {
        case ButtonType::Play:
        {
            juce::Path triangle;
            triangle.addTriangle(iconBounds.getX(), iconBounds.getY(),
                                iconBounds.getX(), iconBounds.getBottom(),
                                iconBounds.getRight(), iconBounds.getCentreY());
            g.fillPath(triangle);
            break;
        }
        case ButtonType::Pause:
        {
            float barWidth = iconBounds.getWidth() * 0.3f;
            float gap = iconBounds.getWidth() * 0.2f;
            g.fillRect(iconBounds.getX(), iconBounds.getY(), barWidth, iconBounds.getHeight());
            g.fillRect(iconBounds.getX() + barWidth + gap, iconBounds.getY(), barWidth, iconBounds.getHeight());
            break;
        }
        case ButtonType::Stop:
        {
            g.fillRect(iconBounds);
            break;
        }
        case ButtonType::SkipBack:
        {
            juce::Path icon;
            float midX = iconBounds.getCentreX();
            // Left bar
            g.fillRect(iconBounds.getX(), iconBounds.getY(), 3.0f, iconBounds.getHeight());
            // Triangle
            icon.addTriangle(midX, iconBounds.getY(),
                            midX, iconBounds.getBottom(),
                            iconBounds.getX() + 3.0f, iconBounds.getCentreY());
            g.fillPath(icon);
            break;
        }
        case ButtonType::SkipForward:
        {
            juce::Path icon;
            float midX = iconBounds.getCentreX();
            // Triangle
            icon.addTriangle(iconBounds.getX(), iconBounds.getY(),
                            iconBounds.getX(), iconBounds.getBottom(),
                            midX, iconBounds.getCentreY());
            g.fillPath(icon);
            // Right bar
            g.fillRect(iconBounds.getRight() - 3.0f, iconBounds.getY(), 3.0f, iconBounds.getHeight());
            break;
        }
        case ButtonType::Loop:
        {
            g.setColour(getToggleState() ? juce::Colour(0xff4a9eff) : juce::Colours::white);

            // Draw two opposing curved arrows forming a circle (like refresh icon)
            // Use same bounds as stop icon (no extra reduction)
            float cx = iconBounds.getCentreX();
            float cy = iconBounds.getCentreY();
            float radius = std::min(iconBounds.getWidth(), iconBounds.getHeight()) * 0.5f;
            float strokeWidth = 2.0f;
            float arrowSize = 5.0f;

            // Top arc (right half going clockwise)
            juce::Path arc1;
            arc1.addCentredArc(cx, cy, radius, radius, 0.0f,
                              -juce::MathConstants<float>::pi * 0.15f,   // Start (right side, slightly up)
                              -juce::MathConstants<float>::pi * 0.85f,  // End (left side, slightly up)
                              true);
            g.strokePath(arc1, juce::PathStrokeType(strokeWidth));

            // Bottom arc (left half going clockwise)
            juce::Path arc2;
            arc2.addCentredArc(cx, cy, radius, radius, 0.0f,
                              juce::MathConstants<float>::pi * 0.85f,   // Start (left side, slightly down)
                              juce::MathConstants<float>::pi * 0.15f,   // End (right side, slightly down)
                              true);
            g.strokePath(arc2, juce::PathStrokeType(strokeWidth));

            // Arrow 1 - at the end of top arc (pointing left/down)
            float angle1 = -juce::MathConstants<float>::pi * 0.85f;
            float ax1 = cx + radius * std::cos(angle1);
            float ay1 = cy + radius * std::sin(angle1);
            juce::Path arrow1;
            arrow1.addTriangle(
                ax1 - arrowSize * 0.3f, ay1 + arrowSize,
                ax1 + arrowSize, ay1,
                ax1 - arrowSize * 0.3f, ay1 - arrowSize);
            g.fillPath(arrow1);

            // Arrow 2 - at the end of bottom arc (pointing right/up)
            float angle2 = juce::MathConstants<float>::pi * 0.15f;
            float ax2 = cx + radius * std::cos(angle2);
            float ay2 = cy + radius * std::sin(angle2);
            juce::Path arrow2;
            arrow2.addTriangle(
                ax2 + arrowSize * 0.3f, ay2 - arrowSize,
                ax2 - arrowSize, ay2,
                ax2 + arrowSize * 0.3f, ay2 + arrowSize);
            g.fillPath(arrow2);
            break;
        }
        case ButtonType::Open:
        {
            // Folder icon
            g.drawRect(iconBounds.reduced(2.0f), 1.5f);
            g.fillRect(iconBounds.getX() + 2.0f, iconBounds.getY(),
                      iconBounds.getWidth() * 0.4f, 3.0f);
            break;
        }
        case ButtonType::Save:
        {
            // Floppy disk icon
            g.drawRect(iconBounds.reduced(2.0f), 1.5f);
            g.fillRect(iconBounds.getX() + 4.0f, iconBounds.getY() + 2.0f,
                      iconBounds.getWidth() - 8.0f, 4.0f);
            break;
        }
    }
}

// TimeDisplay implementation
TransportControls::TimeDisplay::TimeDisplay()
{
}

void TransportControls::TimeDisplay::setTime(double seconds, double total)
{
    currentTime = seconds;
    totalTime = total;
    repaint();
}

void TransportControls::TimeDisplay::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);

    auto formatTime = [](double seconds) -> juce::String
    {
        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        int millis = static_cast<int>((seconds - std::floor(seconds)) * 1000);
        return juce::String::formatted("%02d:%02d.%03d", mins, secs, millis);
    };

    juce::String text = formatTime(currentTime) + " / " + formatTime(totalTime);
    g.drawText(text, getLocalBounds(), juce::Justification::centred);
}

// TransportControls implementation
TransportControls::TransportControls()
{
    // Create buttons
    openButton = std::make_unique<TransportButton>("Open", TransportButton::ButtonType::Open);
    saveButton = std::make_unique<TransportButton>("Save", TransportButton::ButtonType::Save);
    skipBackButton = std::make_unique<TransportButton>("SkipBack", TransportButton::ButtonType::SkipBack);
    playPauseButton = std::make_unique<TransportButton>("PlayPause", TransportButton::ButtonType::Play);
    stopButton = std::make_unique<TransportButton>("Stop", TransportButton::ButtonType::Stop);
    skipForwardButton = std::make_unique<TransportButton>("SkipForward", TransportButton::ButtonType::SkipForward);
    loopButton = std::make_unique<TransportButton>("Loop", TransportButton::ButtonType::Loop);

    // Add and configure buttons
    addAndMakeVisible(*openButton);
    addAndMakeVisible(*saveButton);
    addAndMakeVisible(*skipBackButton);
    addAndMakeVisible(*playPauseButton);
    addAndMakeVisible(*stopButton);
    addAndMakeVisible(*skipForwardButton);
    addAndMakeVisible(*loopButton);
    addAndMakeVisible(timeDisplay);
    addAndMakeVisible(positionSlider);

    loopButton->setClickingTogglesState(true);

    // Button callbacks
    openButton->onClick = [this]()
    {
        if (onOpenFile)
            onOpenFile();
    };

    saveButton->onClick = [this]()
    {
        if (onSaveFile)
            onSaveFile();
    };

    playPauseButton->onClick = [this]()
    {
        if (playing)
        {
            if (onPause) onPause();
        }
        else
        {
            if (onPlay) onPlay();
        }
    };

    stopButton->onClick = [this]()
    {
        if (onStop)
            onStop();
    };

    skipBackButton->onClick = [this]()
    {
        if (onSkipBackward)
            onSkipBackward();
    };

    skipForwardButton->onClick = [this]()
    {
        if (onSkipForward)
            onSkipForward();
    };

    loopButton->onClick = [this]()
    {
        if (onLoopToggled)
            onLoopToggled(loopButton->getToggleState());
    };

    // Position slider setup
    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setRange(0.0, 1.0, 0.001);
    positionSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff4a9eff));
    positionSlider.setColour(juce::Slider::trackColourId, primaryColor);
    positionSlider.setColour(juce::Slider::backgroundColourId, backgroundColor.brighter(0.1f));

    positionSlider.onDragStart = [this]()
    {
        sliderBeingDragged = true;
    };

    positionSlider.onDragEnd = [this]()
    {
        sliderBeingDragged = false;
        if (onPositionChanged && duration > 0.0)
            onPositionChanged(positionSlider.getValue() * duration);
    };

    positionSlider.onValueChange = [this]()
    {
        if (sliderBeingDragged && onPositionChanged && duration > 0.0)
            onPositionChanged(positionSlider.getValue() * duration);
    };

    // Note: Timer no longer started - updates handled by PluginEditor
}

TransportControls::~TransportControls()
{
    stopTimer();
}

void TransportControls::setPlaying(bool isPlaying)
{
    playing = isPlaying;
    updatePlayPauseButton();
}

void TransportControls::setLooping(bool isLooping)
{
    looping = isLooping;
    loopButton->setToggleState(isLooping, juce::dontSendNotification);
}

void TransportControls::setPosition(double positionInSeconds)
{
    position = positionInSeconds;
    if (!sliderBeingDragged && duration > 0.0)
    {
        positionSlider.setValue(position / duration, juce::dontSendNotification);
    }
    timeDisplay.setTime(position, duration);
}

void TransportControls::setTotalDuration(double durationInSeconds)
{
    duration = durationInSeconds;
    timeDisplay.setTime(position, duration);
}

void TransportControls::setFileLoaded(bool loaded)
{
    fileLoaded = loaded;
    playPauseButton->setEnabled(loaded);
    stopButton->setEnabled(loaded);
    skipBackButton->setEnabled(loaded);
    skipForwardButton->setEnabled(loaded);
    loopButton->setEnabled(loaded);
    positionSlider.setEnabled(loaded);
    saveButton->setEnabled(loaded);
}

void TransportControls::updatePlayPauseButton()
{
    // Recreate button with appropriate icon
    auto bounds = playPauseButton->getBounds();
    bool enabled = playPauseButton->isEnabled();

    if (playing)
    {
        playPauseButton = std::make_unique<TransportButton>("PlayPause", TransportButton::ButtonType::Pause);
    }
    else
    {
        playPauseButton = std::make_unique<TransportButton>("PlayPause", TransportButton::ButtonType::Play);
    }

    playPauseButton->setBounds(bounds);
    playPauseButton->setEnabled(enabled);
    playPauseButton->onClick = [this]()
    {
        if (playing)
        {
            if (onPause) onPause();
        }
        else
        {
            if (onPlay) onPlay();
        }
    };

    addAndMakeVisible(*playPauseButton);
}

void TransportControls::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColor);

    // Top border line
    g.setColour(primaryColor);
    g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds().reduced(8);

    const int buttonSize = 32;
    const int buttonSpacing = 4;

    // Left side: Open and Save buttons
    auto leftArea = bounds.removeFromLeft(buttonSize * 2 + buttonSpacing);
    openButton->setBounds(leftArea.removeFromLeft(buttonSize));
    leftArea.removeFromLeft(buttonSpacing);
    saveButton->setBounds(leftArea.removeFromLeft(buttonSize));

    bounds.removeFromLeft(16); // Spacer

    // Transport buttons in center
    int transportWidth = buttonSize * 5 + buttonSpacing * 4;
    auto transportArea = bounds.removeFromLeft(transportWidth);

    skipBackButton->setBounds(transportArea.removeFromLeft(buttonSize));
    transportArea.removeFromLeft(buttonSpacing);
    playPauseButton->setBounds(transportArea.removeFromLeft(buttonSize));
    transportArea.removeFromLeft(buttonSpacing);
    stopButton->setBounds(transportArea.removeFromLeft(buttonSize));
    transportArea.removeFromLeft(buttonSpacing);
    skipForwardButton->setBounds(transportArea.removeFromLeft(buttonSize));
    transportArea.removeFromLeft(buttonSpacing);
    loopButton->setBounds(transportArea.removeFromLeft(buttonSize));

    bounds.removeFromLeft(16); // Spacer

    // Time display
    timeDisplay.setBounds(bounds.removeFromLeft(140));

    bounds.removeFromLeft(16); // Spacer

    // Position slider takes remaining space
    positionSlider.setBounds(bounds);
}

void TransportControls::timerCallback()
{
    // Timer for UI updates (handled externally by AudioEngine callbacks)
}

juce::String TransportControls::formatTime(double seconds) const
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int millis = static_cast<int>((seconds - std::floor(seconds)) * 1000);
    return juce::String::formatted("%02d:%02d.%03d", mins, secs, millis);
}

} // namespace spectralz
