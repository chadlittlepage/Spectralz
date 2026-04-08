#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralz
{

/**
 * Platform-aware LookAndFeel for Spectralz
 * Provides native appearance on macOS and Windows
 */
class SpectralzLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SpectralzLookAndFeel();
    ~SpectralzLookAndFeel() override = default;

    // Fonts
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getSliderPopupFont(juce::Slider&) override;
    juce::Font getPopupMenuFont() override;

    // Buttons
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // ComboBox
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    // Sliders
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    // Scrollbars
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override;

    // PopupMenu
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    // Platform-specific getters
    [[nodiscard]] static float getButtonCornerRadius();
    [[nodiscard]] static float getControlHeight();
    [[nodiscard]] static float getSpacing();
    [[nodiscard]] static juce::String getSystemFontName();
    [[nodiscard]] static float getDefaultFontSize();

    // Color scheme (grey tones)
    struct Colors
    {
        juce::Colour background{0xff1a1a1a};    // Darkest grey
        juce::Colour surface{0xff2d2d2d};       // Dark grey
        juce::Colour primary{0xff4a4a4a};       // Medium grey
        juce::Colour primaryLight{0xff5a5a5a};  // Lighter grey
        juce::Colour accent{0xffff6b35};        // Orange accent
        juce::Colour textPrimary{0xffffffff};
        juce::Colour textSecondary{0xaaffffff};
        juce::Colour textDisabled{0x55ffffff};
        juce::Colour border{0x40ffffff};
        juce::Colour highlight{0x30ffffff};
    };

    Colors colors;

private:
    juce::Font systemFont;
    juce::Font monoFont;

    void setupColors();
    void setupFonts();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralzLookAndFeel)
};

} // namespace spectralz
