#include "LayerPanel.h"

namespace spectralz
{

// LayerRowComponent implementation
LayerPanel::LayerRowComponent::LayerRowComponent(LayerPanel& ownerPanel)
    : owner(ownerPanel)
{
    // Setup toggle buttons
    visibleButton.setToggleable(true);
    visibleButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00cc44));
    visibleButton.onClick = [this]()
    {
        if (layer && owner.onLayerVisibilityChanged)
        {
            layer->setVisible(visibleButton.getToggleState());
            owner.onLayerVisibilityChanged(layerIndex, visibleButton.getToggleState());
        }
    };
    addAndMakeVisible(visibleButton);

    muteButton.setToggleable(true);
    muteButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffff4444));
    muteButton.onClick = [this]()
    {
        if (layer && owner.onLayerMuteChanged)
        {
            layer->setMuted(muteButton.getToggleState());
            owner.onLayerMuteChanged(layerIndex, muteButton.getToggleState());
        }
    };
    addAndMakeVisible(muteButton);

    soloButton.setToggleable(true);
    soloButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffcc00));
    soloButton.onClick = [this]()
    {
        if (layer && owner.onLayerSoloChanged)
        {
            layer->setSolo(soloButton.getToggleState());
            owner.onLayerSoloChanged(layerIndex, soloButton.getToggleState());
        }
    };
    addAndMakeVisible(soloButton);

    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    nameLabel.setEditable(true, true, false);
    nameLabel.onTextChange = [this]()
    {
        if (layer)
        {
            layer->setName(nameLabel.getText().toStdString());
        }
    };
    addAndMakeVisible(nameLabel);
}

void LayerPanel::LayerRowComponent::setLayer(SpectralLayer* layerPtr, int index, bool selected)
{
    layer = layerPtr;
    layerIndex = index;
    isSelected = selected;

    if (layer)
    {
        visibleButton.setToggleState(layer->isVisible(), juce::dontSendNotification);
        muteButton.setToggleState(layer->isMuted(), juce::dontSendNotification);
        soloButton.setToggleState(layer->isSolo(), juce::dontSendNotification);
        nameLabel.setText(juce::String(layer->getName()), juce::dontSendNotification);
    }
}

void LayerPanel::LayerRowComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    if (isSelected)
    {
        g.setColour(juce::Colour(0xff3a3a3a));
    }
    else
    {
        g.setColour(juce::Colour(0xff2d2d2d));
    }
    g.fillRect(bounds);

    // Bottom border
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawHorizontalLine(bounds.getBottom() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
}

void LayerPanel::LayerRowComponent::resized()
{
    auto bounds = getLocalBounds().reduced(4, 2);

    int buttonSize = 24;

    // Toggle buttons on the left
    visibleButton.setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(2);
    muteButton.setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(2);
    soloButton.setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(5);

    // Name takes remaining space
    nameLabel.setBounds(bounds);
}

// LayerPanel implementation
LayerPanel::LayerPanel()
{
    layerList.setModel(this);
    layerList.setRowHeight(32);
    layerList.setColour(juce::ListBox::backgroundColourId, backgroundColor);
    layerList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(layerList);

    // Add button
    addButton.setColour(juce::TextButton::buttonColourId, primaryColor);
    addButton.onClick = [this]()
    {
        if (onAddLayer)
            onAddLayer();
    };
    addAndMakeVisible(addButton);

    // Remove button
    removeButton.setColour(juce::TextButton::buttonColourId, primaryColor);
    removeButton.onClick = [this]()
    {
        int selectedRow = layerList.getSelectedRow();
        if (selectedRow >= 0 && onRemoveLayer)
            onRemoveLayer(selectedRow);
    };
    addAndMakeVisible(removeButton);
}

void LayerPanel::setEditor(SpectralEditor* ed)
{
    editor = ed;

    if (editor)
    {
        // Connect to editor callbacks
        editor->onLayersChanged = [this]()
        {
            refresh();
        };
    }

    refresh();
}

void LayerPanel::refresh()
{
    layerList.updateContent();
    layerList.repaint();

    // Select active layer
    if (editor)
    {
        layerList.selectRow(editor->getActiveLayerIndex());
    }
}

int LayerPanel::getNumRows()
{
    return editor ? editor->getNumLayers() : 0;
}

void LayerPanel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                  int width, int height, bool rowIsSelected)
{
    // Handled by custom row component
    (void)rowNumber;
    (void)g;
    (void)width;
    (void)height;
    (void)rowIsSelected;
}

void LayerPanel::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    (void)e;

    if (editor)
    {
        editor->setActiveLayer(row);
        if (onLayerSelected)
            onLayerSelected(row);
    }
}

void LayerPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent& e)
{
    (void)e;
    (void)row;
    // Could open layer properties dialog
}

juce::Component* LayerPanel::refreshComponentForRow(int rowNumber, bool isRowSelected,
                                                    juce::Component* existingComponentToUpdate)
{
    LayerRowComponent* rowComponent;

    if (existingComponentToUpdate == nullptr)
    {
        rowComponent = new LayerRowComponent(*this);
    }
    else
    {
        rowComponent = dynamic_cast<LayerRowComponent*>(existingComponentToUpdate);
        jassert(rowComponent != nullptr);
    }

    if (editor && rowNumber < editor->getNumLayers())
    {
        rowComponent->setLayer(editor->getLayer(rowNumber), rowNumber, isRowSelected);
    }

    return rowComponent;
}

void LayerPanel::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColor);

    // Title
    g.setColour(textColor);
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("Layers", 10, 5, getWidth() - 20, 20, juce::Justification::centredLeft);
}

void LayerPanel::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    bounds.removeFromTop(25); // Title space

    // Buttons at bottom
    auto buttonArea = bounds.removeFromBottom(30);
    buttonArea.removeFromTop(5);

    int buttonWidth = (buttonArea.getWidth() - 5) / 2;
    addButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(5);
    removeButton.setBounds(buttonArea.removeFromLeft(buttonWidth));

    // List takes remaining space
    layerList.setBounds(bounds);
}

} // namespace spectralz
