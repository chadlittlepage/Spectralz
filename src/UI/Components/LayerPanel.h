#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../DSP/SpectralEditor.h"
#include <functional>

namespace spectralz
{

class LayerPanel : public juce::Component,
                   public juce::ListBoxModel
{
public:
    LayerPanel();
    ~LayerPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Set the editor to display layers from
    void setEditor(SpectralEditor* ed);
    void refresh();

    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                         int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate) override;

    // Callbacks
    std::function<void(int)> onLayerSelected;
    std::function<void(int, bool)> onLayerVisibilityChanged;
    std::function<void(int, bool)> onLayerMuteChanged;
    std::function<void(int, bool)> onLayerSoloChanged;
    std::function<void()> onAddLayer;
    std::function<void(int)> onRemoveLayer;

private:
    // Row component for layer list
    class LayerRowComponent : public juce::Component
    {
    public:
        LayerRowComponent(LayerPanel& ownerPanel);
        void setLayer(SpectralLayer* layerPtr, int index, bool selected);
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        LayerPanel& owner;
        SpectralLayer* layer = nullptr;
        int layerIndex = -1;
        bool isSelected = false;

        juce::ToggleButton visibleButton{"V"};
        juce::ToggleButton muteButton{"M"};
        juce::ToggleButton soloButton{"S"};
        juce::Label nameLabel;
    };

    SpectralEditor* editor = nullptr;
    juce::ListBox layerList;
    juce::TextButton addButton{"+"};
    juce::TextButton removeButton{"-"};

    juce::Colour backgroundColor{0xff1a1a1a};
    juce::Colour primaryColor{0xff4a4a4a};
    juce::Colour textColor{0xffffffff};
    juce::Colour selectedColor{0xff3a3a3a};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerPanel)
};

} // namespace spectralz
