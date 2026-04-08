#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace spectralz
{

// Tool types for spectral editing
enum class ToolType
{
    Select,          // Rectangular time/frequency selection
    FrequencySelect, // Full-time-range frequency band selection
    TimeSelect,      // Full-frequency-range time selection
    BrushBoost,      // Paint to boost gain
    BrushAttenuate,  // Paint to reduce gain
    Eraser           // Paint to erase (set to floor)
};

// Settings for editing tools
struct ToolSettings
{
    // Brush settings
    float brushRadius = 10.0f;       // In pixels, converted to bins/frames
    float brushStrength = 1.0f;      // 0-1
    float brushFalloff = 0.5f;       // Edge falloff (0=hard, 1=soft)

    // Gain settings
    float gainAmount = 6.0f;         // dB for boost/attenuate

    // Eraser settings
    float eraseFloor = -100.0f;      // dB floor for eraser
};

class ToolPanel : public juce::Component
{
public:
    ToolPanel();
    ~ToolPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Current tool
    void setCurrentTool(ToolType tool);
    [[nodiscard]] ToolType getCurrentTool() const { return currentTool; }

    // Tool settings access
    [[nodiscard]] const ToolSettings& getSettings() const { return settings; }
    void setSettings(const ToolSettings& newSettings);

    // Callbacks
    std::function<void(ToolType)> onToolChanged;
    std::function<void(const ToolSettings&)> onSettingsChanged;

    // Keyboard shortcut handling
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void setupToolButtons();
    void setupSettingsControls();
    void updateSettingsVisibility();
    void updateToolButtonStates();

    // Custom tool button
    class ToolButton : public juce::Button
    {
    public:
        ToolButton(const juce::String& buttonName, ToolType type, const juce::String& shortcut);
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
        [[nodiscard]] ToolType getToolType() const { return toolType; }

    private:
        ToolType toolType;
        juce::String shortcutKey;
    };

    ToolType currentTool = ToolType::Select;
    ToolSettings settings;

    // Tool buttons
    std::vector<std::unique_ptr<ToolButton>> toolButtons;

    // Settings section
    juce::Label settingsLabel{"settingsLabel", "Tool Settings"};

    // Brush size
    juce::Label brushSizeLabel{"brushSizeLabel", "Size:"};
    juce::Slider brushSizeSlider;

    // Brush strength
    juce::Label brushStrengthLabel{"brushStrengthLabel", "Strength:"};
    juce::Slider brushStrengthSlider;

    // Brush falloff
    juce::Label brushFalloffLabel{"brushFalloffLabel", "Softness:"};
    juce::Slider brushFalloffSlider;

    // Gain amount
    juce::Label gainLabel{"gainLabel", "Gain (dB):"};
    juce::Slider gainSlider;

    // Colors
    juce::Colour backgroundColor{0xff1a1a1a};
    juce::Colour primaryColor{0xff4a4a4a};
    juce::Colour accentColor{0xff00cc44};
    juce::Colour textColor{0xffffffff};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolPanel)
};

} // namespace spectralz
