#include "SpectralzLookAndFeel.h"

namespace spectralz
{

SpectralzLookAndFeel::SpectralzLookAndFeel()
{
    setupFonts();
    setupColors();
}

void SpectralzLookAndFeel::setupFonts()
{
#if JUCE_MAC
    systemFont = juce::Font("SF Pro Text", getDefaultFontSize(), juce::Font::plain);
    // Fallback if SF Pro not available
    if (systemFont.getTypefaceName() != "SF Pro Text")
        systemFont = juce::Font("Helvetica Neue", getDefaultFontSize(), juce::Font::plain);
    monoFont = juce::Font("SF Mono", getDefaultFontSize(), juce::Font::plain);
    if (monoFont.getTypefaceName() != "SF Mono")
        monoFont = juce::Font("Menlo", getDefaultFontSize(), juce::Font::plain);
#elif JUCE_WINDOWS
    systemFont = juce::Font("Segoe UI", getDefaultFontSize(), juce::Font::plain);
    monoFont = juce::Font("Consolas", getDefaultFontSize(), juce::Font::plain);
#else
    // Linux fallbacks
    systemFont = juce::Font("Ubuntu", getDefaultFontSize(), juce::Font::plain);
    monoFont = juce::Font("Ubuntu Mono", getDefaultFontSize(), juce::Font::plain);
#endif

    setDefaultSansSerifTypefaceName(systemFont.getTypefaceName());
}

void SpectralzLookAndFeel::setupColors()
{
    // Set JUCE color IDs to our color scheme
    setColour(juce::ResizableWindow::backgroundColourId, colors.background);
    setColour(juce::TextButton::buttonColourId, colors.primary);
    setColour(juce::TextButton::buttonOnColourId, colors.primaryLight);
    setColour(juce::TextButton::textColourOffId, colors.textPrimary);
    setColour(juce::TextButton::textColourOnId, colors.textPrimary);

    setColour(juce::ComboBox::backgroundColourId, colors.surface);
    setColour(juce::ComboBox::textColourId, colors.textPrimary);
    setColour(juce::ComboBox::outlineColourId, colors.border);
    setColour(juce::ComboBox::arrowColourId, colors.textSecondary);

    setColour(juce::PopupMenu::backgroundColourId, colors.surface);
    setColour(juce::PopupMenu::textColourId, colors.textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colors.primary);
    setColour(juce::PopupMenu::highlightedTextColourId, colors.textPrimary);

    setColour(juce::Slider::backgroundColourId, colors.surface);
    setColour(juce::Slider::trackColourId, colors.primary);
    setColour(juce::Slider::thumbColourId, colors.accent);
    setColour(juce::Slider::textBoxTextColourId, colors.textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, colors.surface);
    setColour(juce::Slider::textBoxOutlineColourId, colors.border);

    setColour(juce::Label::textColourId, colors.textPrimary);

    setColour(juce::ScrollBar::thumbColourId, colors.primary);
    setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
}

// Platform-specific values
float SpectralzLookAndFeel::getButtonCornerRadius()
{
#if JUCE_MAC
    return 6.0f;  // macOS has more rounded corners
#elif JUCE_WINDOWS
    return 4.0f;  // Windows is more rectangular
#else
    return 5.0f;
#endif
}

float SpectralzLookAndFeel::getControlHeight()
{
#if JUCE_MAC
    return 24.0f;
#elif JUCE_WINDOWS
    return 28.0f;  // Windows controls tend to be slightly taller
#else
    return 26.0f;
#endif
}

float SpectralzLookAndFeel::getSpacing()
{
#if JUCE_MAC
    return 8.0f;   // macOS has tighter spacing
#elif JUCE_WINDOWS
    return 10.0f;  // Windows has more generous spacing
#else
    return 8.0f;
#endif
}

juce::String SpectralzLookAndFeel::getSystemFontName()
{
#if JUCE_MAC
    return "SF Pro Text";
#elif JUCE_WINDOWS
    return "Segoe UI";
#else
    return "Ubuntu";
#endif
}

float SpectralzLookAndFeel::getDefaultFontSize()
{
#if JUCE_MAC
    return 13.0f;
#elif JUCE_WINDOWS
    return 14.0f;
#else
    return 13.0f;
#endif
}

// Font overrides
juce::Font SpectralzLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return systemFont.withHeight(juce::jmin(getDefaultFontSize(), buttonHeight * 0.6f));
}

juce::Font SpectralzLookAndFeel::getLabelFont(juce::Label&)
{
    return systemFont;
}

juce::Font SpectralzLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return systemFont;
}

juce::Font SpectralzLookAndFeel::getSliderPopupFont(juce::Slider&)
{
    return systemFont.withHeight(getDefaultFontSize() - 1.0f);
}

juce::Font SpectralzLookAndFeel::getPopupMenuFont()
{
    return systemFont;
}

// Button drawing
void SpectralzLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool shouldDrawButtonAsHighlighted,
                                                  bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
    auto cornerRadius = getButtonCornerRadius();

    auto baseColour = backgroundColour;

    if (shouldDrawButtonAsDown)
        baseColour = baseColour.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.1f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerRadius);

#if JUCE_MAC
    // macOS: subtle gradient for depth
    if (!shouldDrawButtonAsDown)
    {
        auto gradient = juce::ColourGradient(
            baseColour.brighter(0.05f), bounds.getX(), bounds.getY(),
            baseColour.darker(0.05f), bounds.getX(), bounds.getBottom(),
            false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds, cornerRadius);
    }
#endif

    // Border
    g.setColour(colors.border);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
}

// ComboBox drawing
void SpectralzLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int buttonX, int buttonY, int buttonW, int buttonH,
                                         juce::ComboBox& box)
{
    auto cornerRadius = getButtonCornerRadius();
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f, 0.5f);

    g.setColour(colors.surface);
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(colors.border);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

    // Arrow
    auto arrowZone = juce::Rectangle<int>(buttonX, buttonY, buttonW, buttonH).toFloat();
    auto arrowPath = juce::Path();
    auto arrowSize = juce::jmin(8.0f, arrowZone.getWidth() * 0.3f);

    arrowPath.addTriangle(
        arrowZone.getCentreX() - arrowSize * 0.5f, arrowZone.getCentreY() - arrowSize * 0.25f,
        arrowZone.getCentreX() + arrowSize * 0.5f, arrowZone.getCentreY() - arrowSize * 0.25f,
        arrowZone.getCentreX(), arrowZone.getCentreY() + arrowSize * 0.35f);

    g.setColour(box.isEnabled() ? colors.textSecondary : colors.textDisabled);
    g.fillPath(arrowPath);
}

// Slider drawing
void SpectralzLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float minSliderPos, float maxSliderPos,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
{
    auto trackWidth = 4.0f;

#if JUCE_MAC
    trackWidth = 3.0f;  // Thinner tracks on macOS
#endif

    auto isHorizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;

    juce::Rectangle<float> track;
    if (isHorizontal)
    {
        auto trackY = static_cast<float>(y) + static_cast<float>(height) * 0.5f - trackWidth * 0.5f;
        track = juce::Rectangle<float>(static_cast<float>(x), trackY, static_cast<float>(width), trackWidth);
    }
    else
    {
        auto trackX = static_cast<float>(x) + static_cast<float>(width) * 0.5f - trackWidth * 0.5f;
        track = juce::Rectangle<float>(trackX, static_cast<float>(y), trackWidth, static_cast<float>(height));
    }

    // Track background
    g.setColour(colors.surface);
    g.fillRoundedRectangle(track, trackWidth * 0.5f);

    // Filled portion
    juce::Rectangle<float> filledTrack;
    if (isHorizontal)
    {
        filledTrack = track.withWidth(sliderPos - static_cast<float>(x));
    }
    else
    {
        filledTrack = track.withTop(sliderPos);
    }

    g.setColour(colors.primary);
    g.fillRoundedRectangle(filledTrack, trackWidth * 0.5f);

    // Thumb
    auto thumbRadius = 7.0f;
#if JUCE_MAC
    thumbRadius = 6.0f;
#endif

    juce::Point<float> thumbPos;
    if (isHorizontal)
        thumbPos = {sliderPos, track.getCentreY()};
    else
        thumbPos = {track.getCentreX(), sliderPos};

    g.setColour(colors.accent);
    g.fillEllipse(juce::Rectangle<float>(thumbRadius * 2.0f, thumbRadius * 2.0f).withCentre(thumbPos));

    // Thumb border
    g.setColour(colors.textPrimary.withAlpha(0.3f));
    g.drawEllipse(juce::Rectangle<float>(thumbRadius * 2.0f, thumbRadius * 2.0f).withCentre(thumbPos), 1.0f);
}

// Scrollbar drawing
void SpectralzLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                                          int x, int y, int width, int height,
                                          bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
#if JUCE_MAC
    // macOS overlay-style scrollbar
    auto thumbOpacity = (isMouseOver || isMouseDown) ? 0.7f : 0.4f;
    auto thumbWidth = isScrollbarVertical ? 6.0f : static_cast<float>(height);
    auto thumbHeight = isScrollbarVertical ? static_cast<float>(height) : 6.0f;

    juce::Rectangle<float> thumbBounds;
    if (isScrollbarVertical)
    {
        auto thumbX = static_cast<float>(x + width) - thumbWidth - 2.0f;
        thumbBounds = juce::Rectangle<float>(thumbX, static_cast<float>(thumbStartPosition), thumbWidth, static_cast<float>(thumbSize));
    }
    else
    {
        auto thumbY = static_cast<float>(y + height) - thumbHeight - 2.0f;
        thumbBounds = juce::Rectangle<float>(static_cast<float>(thumbStartPosition), thumbY, static_cast<float>(thumbSize), thumbHeight);
    }

    g.setColour(colors.textPrimary.withAlpha(thumbOpacity));
    g.fillRoundedRectangle(thumbBounds, thumbWidth * 0.5f);
#else
    // Windows traditional scrollbar
    g.setColour(colors.surface);
    g.fillRect(x, y, width, height);

    juce::Rectangle<int> thumbBounds;
    if (isScrollbarVertical)
        thumbBounds = juce::Rectangle<int>(x, thumbStartPosition, width, thumbSize);
    else
        thumbBounds = juce::Rectangle<int>(thumbStartPosition, y, thumbSize, height);

    auto thumbColour = isMouseDown ? colors.primaryLight : (isMouseOver ? colors.primary.brighter(0.1f) : colors.primary);
    g.setColour(thumbColour);
    g.fillRect(thumbBounds.reduced(2));
#endif
}

int SpectralzLookAndFeel::getDefaultScrollbarWidth()
{
#if JUCE_MAC
    return 10;  // Thin overlay scrollbar
#else
    return 14;  // Traditional scrollbar
#endif
}

// PopupMenu item drawing
void SpectralzLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu,
                                              const juce::String& text, const juce::String& shortcutKeyText,
                                              const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        r.removeFromTop(r.getHeight() / 2 - 1);
        g.setColour(colors.border);
        g.fillRect(r.removeFromTop(1));
        return;
    }

    auto r = area.reduced(1);

    if (isHighlighted && isActive)
    {
#if JUCE_MAC
        // macOS: rounded highlight
        g.setColour(colors.primary);
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
#else
        // Windows: rectangular highlight
        g.setColour(colors.primary);
        g.fillRect(r);
#endif
    }

    auto textColourToUse = textColour != nullptr ? *textColour
                           : (isHighlighted ? colors.textPrimary
                              : (isActive ? colors.textPrimary : colors.textDisabled));

    auto maxTextWidth = r.getWidth() - (hasSubMenu ? 25 : 5);

    if (isTicked)
    {
        auto tickArea = r.removeFromLeft(juce::roundToInt(r.getHeight() * 0.8f));
        auto tick = getTickShape(tickArea.getHeight() * 0.5f);
        g.setColour(textColourToUse);
        g.fillPath(tick, tick.getTransformToScaleToFit(tickArea.reduced(4).toFloat(), true));
    }
    else
    {
        r.removeFromLeft(5);
    }

    g.setColour(textColourToUse);
    g.setFont(getPopupMenuFont());
    g.drawFittedText(text, r.reduced(5, 0), juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(colors.textSecondary);
        g.setFont(getPopupMenuFont().withHeight(getDefaultFontSize() - 2.0f));
        g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
    }

    if (hasSubMenu)
    {
        auto arrowH = 8.0f;
        auto arrowX = static_cast<float>(r.getRight()) - arrowH * 0.6f - 5.0f;
        auto arrowY = static_cast<float>(r.getCentreY());

        juce::Path arrow;
        arrow.addTriangle(arrowX, arrowY - arrowH * 0.4f,
                          arrowX, arrowY + arrowH * 0.4f,
                          arrowX + arrowH * 0.4f, arrowY);

        g.setColour(textColourToUse);
        g.fillPath(arrow);
    }
}

} // namespace spectralz
