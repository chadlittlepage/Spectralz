#include "ToolPanel.h"

namespace spectralz
{

// ToolButton implementation
ToolPanel::ToolButton::ToolButton(const juce::String& buttonName, ToolType type, const juce::String& shortcut)
    : juce::Button(buttonName)
    , toolType(type)
    , shortcutKey(shortcut)
{
    setClickingTogglesState(false);
}

void ToolPanel::ToolButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(2);

    // Background
    juce::Colour bgColor(0xff2d2d2d);
    if (getToggleState())
        bgColor = juce::Colour(0xff4a4a4a);
    else if (highlighted)
        bgColor = juce::Colour(0xff3a3a3a);
    if (down)
        bgColor = bgColor.darker(0.2f);

    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border when selected
    if (getToggleState())
    {
        g.setColour(juce::Colour(0xff00cc44));
        g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    }

    // Icon/Text
    g.setColour(getToggleState() ? juce::Colours::white : juce::Colour(0xffcccccc));
    g.setFont(12.0f);

    // Draw tool name
    auto textBounds = bounds.reduced(4);
    g.drawText(getButtonText(), textBounds.removeFromTop(textBounds.getHeight() * 0.7f),
               juce::Justification::centred, false);

    // Draw shortcut
    g.setColour(juce::Colour(0xff888888));
    g.setFont(10.0f);
    g.drawText(shortcutKey, textBounds, juce::Justification::centred, false);
}

// ToolPanel implementation
ToolPanel::ToolPanel()
{
    setupToolButtons();
    setupSettingsControls();
    updateSettingsVisibility();

    setWantsKeyboardFocus(true);
}

void ToolPanel::setupToolButtons()
{
    // Create tool buttons
    struct ToolInfo
    {
        ToolType type;
        juce::String name;
        juce::String shortcut;
    };

    std::vector<ToolInfo> tools = {
        {ToolType::Select, "Select", "V"},
        {ToolType::FrequencySelect, "Freq", "F"},
        {ToolType::TimeSelect, "Time", "T"},
        {ToolType::BrushBoost, "Boost", "B"},
        {ToolType::BrushAttenuate, "Reduce", "N"},
        {ToolType::Eraser, "Erase", "E"}
    };

    for (const auto& tool : tools)
    {
        auto button = std::make_unique<ToolButton>(tool.name, tool.type, tool.shortcut);
        button->setToggleState(tool.type == currentTool, juce::dontSendNotification);

        button->onClick = [this, type = tool.type]()
        {
            setCurrentTool(type);
        };

        addAndMakeVisible(button.get());
        toolButtons.push_back(std::move(button));
    }
}

void ToolPanel::setupSettingsControls()
{
    // Settings label
    settingsLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    settingsLabel.setColour(juce::Label::textColourId, textColor);
    addAndMakeVisible(settingsLabel);

    // Brush size slider
    brushSizeLabel.setColour(juce::Label::textColourId, textColor);
    addAndMakeVisible(brushSizeLabel);

    brushSizeSlider.setRange(1.0, 50.0, 1.0);
    brushSizeSlider.setValue(settings.brushRadius);
    brushSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brushSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    brushSizeSlider.setColour(juce::Slider::thumbColourId, accentColor);
    brushSizeSlider.onValueChange = [this]()
    {
        settings.brushRadius = static_cast<float>(brushSizeSlider.getValue());
        if (onSettingsChanged)
            onSettingsChanged(settings);
    };
    addAndMakeVisible(brushSizeSlider);

    // Brush strength slider
    brushStrengthLabel.setColour(juce::Label::textColourId, textColor);
    addAndMakeVisible(brushStrengthLabel);

    brushStrengthSlider.setRange(0.1, 1.0, 0.1);
    brushStrengthSlider.setValue(settings.brushStrength);
    brushStrengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brushStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    brushStrengthSlider.setColour(juce::Slider::thumbColourId, accentColor);
    brushStrengthSlider.onValueChange = [this]()
    {
        settings.brushStrength = static_cast<float>(brushStrengthSlider.getValue());
        if (onSettingsChanged)
            onSettingsChanged(settings);
    };
    addAndMakeVisible(brushStrengthSlider);

    // Brush falloff slider
    brushFalloffLabel.setColour(juce::Label::textColourId, textColor);
    addAndMakeVisible(brushFalloffLabel);

    brushFalloffSlider.setRange(0.0, 1.0, 0.1);
    brushFalloffSlider.setValue(settings.brushFalloff);
    brushFalloffSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brushFalloffSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    brushFalloffSlider.setColour(juce::Slider::thumbColourId, accentColor);
    brushFalloffSlider.onValueChange = [this]()
    {
        settings.brushFalloff = static_cast<float>(brushFalloffSlider.getValue());
        if (onSettingsChanged)
            onSettingsChanged(settings);
    };
    addAndMakeVisible(brushFalloffSlider);

    // Gain slider
    gainLabel.setColour(juce::Label::textColourId, textColor);
    addAndMakeVisible(gainLabel);

    gainSlider.setRange(1.0, 24.0, 1.0);
    gainSlider.setValue(settings.gainAmount);
    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    gainSlider.setColour(juce::Slider::thumbColourId, accentColor);
    gainSlider.onValueChange = [this]()
    {
        settings.gainAmount = static_cast<float>(gainSlider.getValue());
        if (onSettingsChanged)
            onSettingsChanged(settings);
    };
    addAndMakeVisible(gainSlider);
}

void ToolPanel::updateSettingsVisibility()
{
    bool isBrushTool = (currentTool == ToolType::BrushBoost ||
                        currentTool == ToolType::BrushAttenuate ||
                        currentTool == ToolType::Eraser);

    bool isGainTool = (currentTool == ToolType::BrushBoost ||
                       currentTool == ToolType::BrushAttenuate);

    // Show/hide brush settings
    brushSizeLabel.setVisible(isBrushTool);
    brushSizeSlider.setVisible(isBrushTool);
    brushStrengthLabel.setVisible(isBrushTool);
    brushStrengthSlider.setVisible(isBrushTool);
    brushFalloffLabel.setVisible(isBrushTool);
    brushFalloffSlider.setVisible(isBrushTool);

    // Show/hide gain settings
    gainLabel.setVisible(isGainTool);
    gainSlider.setVisible(isGainTool);

    // Settings label visible if any settings are shown
    settingsLabel.setVisible(isBrushTool || isGainTool);
}

void ToolPanel::updateToolButtonStates()
{
    for (auto& button : toolButtons)
    {
        button->setToggleState(button->getToolType() == currentTool,
                               juce::dontSendNotification);
    }
}

void ToolPanel::setCurrentTool(ToolType tool)
{
    if (currentTool != tool)
    {
        currentTool = tool;
        updateToolButtonStates();
        updateSettingsVisibility();
        resized();

        if (onToolChanged)
            onToolChanged(currentTool);
    }
}

void ToolPanel::setSettings(const ToolSettings& newSettings)
{
    settings = newSettings;
    brushSizeSlider.setValue(settings.brushRadius, juce::dontSendNotification);
    brushStrengthSlider.setValue(settings.brushStrength, juce::dontSendNotification);
    brushFalloffSlider.setValue(settings.brushFalloff, juce::dontSendNotification);
    gainSlider.setValue(settings.gainAmount, juce::dontSendNotification);
}

bool ToolPanel::keyPressed(const juce::KeyPress& key)
{
    // Tool shortcuts
    if (key == juce::KeyPress('v') || key == juce::KeyPress('V'))
    {
        setCurrentTool(ToolType::Select);
        return true;
    }
    if (key == juce::KeyPress('f') || key == juce::KeyPress('F'))
    {
        setCurrentTool(ToolType::FrequencySelect);
        return true;
    }
    if (key == juce::KeyPress('t') || key == juce::KeyPress('T'))
    {
        setCurrentTool(ToolType::TimeSelect);
        return true;
    }
    if (key == juce::KeyPress('b') || key == juce::KeyPress('B'))
    {
        setCurrentTool(ToolType::BrushBoost);
        return true;
    }
    if (key == juce::KeyPress('n') || key == juce::KeyPress('N'))
    {
        setCurrentTool(ToolType::BrushAttenuate);
        return true;
    }
    if (key == juce::KeyPress('e') || key == juce::KeyPress('E'))
    {
        setCurrentTool(ToolType::Eraser);
        return true;
    }

    // Brush size shortcuts
    if (key == juce::KeyPress('['))
    {
        brushSizeSlider.setValue(brushSizeSlider.getValue() - 2.0);
        return true;
    }
    if (key == juce::KeyPress(']'))
    {
        brushSizeSlider.setValue(brushSizeSlider.getValue() + 2.0);
        return true;
    }

    return false;
}

void ToolPanel::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColor);

    // Title
    g.setColour(textColor);
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("Tools", 10, 5, getWidth() - 20, 20, juce::Justification::centredLeft);
}

void ToolPanel::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    bounds.removeFromTop(25); // Title space

    // Tool buttons in a grid (2 columns)
    int buttonWidth = (bounds.getWidth() - 5) / 2;
    int buttonHeight = 45;

    for (size_t i = 0; i < toolButtons.size(); ++i)
    {
        int row = static_cast<int>(i) / 2;
        int col = static_cast<int>(i) % 2;

        toolButtons[i]->setBounds(
            bounds.getX() + col * (buttonWidth + 5),
            bounds.getY() + row * (buttonHeight + 3),
            buttonWidth,
            buttonHeight);
    }

    // Settings section below tools
    int settingsY = bounds.getY() + ((static_cast<int>(toolButtons.size()) + 1) / 2) * (buttonHeight + 3) + 15;

    if (settingsLabel.isVisible())
    {
        settingsLabel.setBounds(bounds.getX(), settingsY, bounds.getWidth(), 20);
        settingsY += 25;
    }

    int sliderHeight = 24;
    int labelWidth = 70;

    if (brushSizeLabel.isVisible())
    {
        brushSizeLabel.setBounds(bounds.getX(), settingsY, labelWidth, sliderHeight);
        brushSizeSlider.setBounds(bounds.getX() + labelWidth, settingsY,
                                  bounds.getWidth() - labelWidth, sliderHeight);
        settingsY += sliderHeight + 5;
    }

    if (brushStrengthLabel.isVisible())
    {
        brushStrengthLabel.setBounds(bounds.getX(), settingsY, labelWidth, sliderHeight);
        brushStrengthSlider.setBounds(bounds.getX() + labelWidth, settingsY,
                                      bounds.getWidth() - labelWidth, sliderHeight);
        settingsY += sliderHeight + 5;
    }

    if (brushFalloffLabel.isVisible())
    {
        brushFalloffLabel.setBounds(bounds.getX(), settingsY, labelWidth, sliderHeight);
        brushFalloffSlider.setBounds(bounds.getX() + labelWidth, settingsY,
                                     bounds.getWidth() - labelWidth, sliderHeight);
        settingsY += sliderHeight + 5;
    }

    if (gainLabel.isVisible())
    {
        gainLabel.setBounds(bounds.getX(), settingsY, labelWidth, sliderHeight);
        gainSlider.setBounds(bounds.getX() + labelWidth, settingsY,
                             bounds.getWidth() - labelWidth, sliderHeight);
    }
}

} // namespace spectralz
