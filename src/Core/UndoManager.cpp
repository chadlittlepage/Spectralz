#include "UndoManager.h"
#include "../DSP/SpectralLayer.h"
#include "../DSP/SpectralEdit.h"
#include "../DSP/SpectralEditor.h"

namespace spectralz
{

// AddEditCommand implementation
AddEditCommand::AddEditCommand(SpectralLayer* layer,
                               std::unique_ptr<SpectralEdit> editToAdd,
                               const std::string& desc)
    : targetLayer(layer)
    , edit(std::move(editToAdd))
    , description(desc)
{
}

void AddEditCommand::execute()
{
    if (targetLayer && edit)
    {
        targetLayer->addEdit(std::move(edit));
        executed = true;
    }
}

void AddEditCommand::undo()
{
    if (targetLayer && executed)
    {
        // Get the edit back before removing
        size_t lastIndex = targetLayer->getNumEdits() - 1;
        if (auto* lastEdit = targetLayer->getEdit(lastIndex))
        {
            edit = lastEdit->clone();
        }
        targetLayer->removeLastEdit();
        executed = false;
    }
}

bool AddEditCommand::canMergeWith(const Command& other) const
{
    // Only merge brush-based edits of the same type
    if (other.getCommandId() != getCommandId())
        return false;

    const auto* otherAdd = dynamic_cast<const AddEditCommand*>(&other);
    if (!otherAdd || !edit || !otherAdd->edit)
        return false;

    // Must be same layer and same edit type
    if (targetLayer != otherAdd->targetLayer)
        return false;

    if (edit->getType() != otherAdd->edit->getType())
        return false;

    // Only merge brush-based edits
    return edit->isBrushBased() && otherAdd->edit->isBrushBased();
}

void AddEditCommand::mergeWith(Command& other)
{
    // For brush strokes, we don't actually merge the data
    // We just keep our edit and ignore the other
    // The caller should accumulate brush points before creating the command
    (void)other;
}

// RemoveEditCommand implementation
RemoveEditCommand::RemoveEditCommand(SpectralLayer* layer,
                                     size_t index,
                                     const std::string& desc)
    : targetLayer(layer)
    , editIndex(index)
    , description(desc)
{
}

void RemoveEditCommand::execute()
{
    if (targetLayer && editIndex < targetLayer->getNumEdits())
    {
        // Clone the edit before removing
        if (auto* editPtr = targetLayer->getEdit(editIndex))
        {
            removedEdit = editPtr->clone();
        }
        targetLayer->removeEdit(editIndex);
    }
}

void RemoveEditCommand::undo()
{
    if (targetLayer && removedEdit)
    {
        // Re-add the edit at the same position
        // For simplicity, we add to end (order might differ)
        targetLayer->addEdit(std::move(removedEdit));
    }
}

// LayerCommand implementation
LayerCommand::LayerCommand(Type cmdType,
                           SpectralEditor* ed,
                           int index,
                           const std::string& p)
    : type(cmdType)
    , editor(ed)
    , layerIndex(index)
    , param(p)
{
}

void LayerCommand::execute()
{
    if (!editor)
        return;

    switch (type)
    {
        case Type::Add:
            editor->addLayer(param.empty() ? "Layer" : param);
            break;

        case Type::Remove:
            if (auto* layer = editor->getLayer(layerIndex))
            {
                removedLayer = layer->clone();
            }
            editor->removeLayer(layerIndex);
            break;

        case Type::Rename:
            if (auto* layer = editor->getLayer(layerIndex))
            {
                previousName = layer->getName();
                layer->setName(param);
            }
            break;

        case Type::Move:
            destIndex = std::stoi(param);
            editor->moveLayer(layerIndex, destIndex);
            break;
    }
}

void LayerCommand::undo()
{
    if (!editor)
        return;

    switch (type)
    {
        case Type::Add:
            // Remove the last added layer
            editor->removeLayer(editor->getNumLayers() - 1);
            break;

        case Type::Remove:
            // Re-insert the removed layer
            if (removedLayer)
            {
                editor->insertLayer(layerIndex, std::move(removedLayer));
            }
            break;

        case Type::Rename:
            if (auto* layer = editor->getLayer(layerIndex))
            {
                layer->setName(previousName);
            }
            break;

        case Type::Move:
            // Move back
            editor->moveLayer(destIndex, layerIndex);
            break;
    }
}

std::string LayerCommand::getDescription() const
{
    switch (type)
    {
        case Type::Add:
            return "Add Layer";
        case Type::Remove:
            return "Remove Layer";
        case Type::Rename:
            return "Rename Layer";
        case Type::Move:
            return "Move Layer";
    }
    return "Layer Operation";
}

// UndoManager implementation
UndoManager::UndoManager(size_t maxHistory)
    : maxHistorySize(maxHistory)
{
}

UndoManager::~UndoManager() = default;

void UndoManager::executeCommand(std::unique_ptr<Command> command)
{
    if (!command)
        return;

    // Check if we can merge with the last command
    if (!undoStack.empty() && undoStack.back()->canMergeWith(*command))
    {
        undoStack.back()->mergeWith(*command);
    }
    else
    {
        // Execute and add to stack
        command->execute();
        undoStack.push_back(std::move(command));

        // Clear redo stack on new command
        redoStack.clear();
        savePointValid = false;

        // Trim history if needed
        trimHistory();
    }

    if (onStateChanged)
        onStateChanged();
}

void UndoManager::undo()
{
    if (undoStack.empty())
        return;

    auto command = std::move(undoStack.back());
    undoStack.pop_back();

    command->undo();
    redoStack.push_back(std::move(command));

    if (onStateChanged)
        onStateChanged();
}

void UndoManager::redo()
{
    if (redoStack.empty())
        return;

    auto command = std::move(redoStack.back());
    redoStack.pop_back();

    command->execute();
    undoStack.push_back(std::move(command));

    if (onStateChanged)
        onStateChanged();
}

std::string UndoManager::getUndoDescription() const
{
    if (undoStack.empty())
        return "";
    return undoStack.back()->getDescription();
}

std::string UndoManager::getRedoDescription() const
{
    if (redoStack.empty())
        return "";
    return redoStack.back()->getDescription();
}

void UndoManager::clear()
{
    undoStack.clear();
    redoStack.clear();
    savePointDepth = 0;
    savePointValid = true;

    if (onStateChanged)
        onStateChanged();
}

void UndoManager::setMaxHistorySize(size_t size)
{
    maxHistorySize = size;
    trimHistory();
}

void UndoManager::trimHistory()
{
    while (undoStack.size() > maxHistorySize)
    {
        undoStack.erase(undoStack.begin());
        savePointValid = false;
    }
}

void UndoManager::markSavePoint()
{
    savePointDepth = undoStack.size();
    savePointValid = true;
}

bool UndoManager::isAtSavePoint() const
{
    return savePointValid && undoStack.size() == savePointDepth;
}

} // namespace spectralz
