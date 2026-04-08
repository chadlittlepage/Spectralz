#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace spectralz
{

// Forward declarations
class SpectralEditor;
class SpectralLayer;
class SpectralEdit;

// Abstract command base class
class Command
{
public:
    virtual ~Command() = default;

    virtual void execute() = 0;
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string getDescription() const = 0;

    // For merging consecutive similar commands (e.g., brush strokes)
    [[nodiscard]] virtual bool canMergeWith(const Command& other) const { return false; }
    virtual void mergeWith(Command& other) { (void)other; }

    // Command ID for identifying command type
    [[nodiscard]] virtual int getCommandId() const = 0;
};

// Command for adding an edit to a layer
class AddEditCommand : public Command
{
public:
    AddEditCommand(SpectralLayer* layer,
                   std::unique_ptr<SpectralEdit> edit,
                   const std::string& description);

    void execute() override;
    void undo() override;
    [[nodiscard]] std::string getDescription() const override { return description; }
    [[nodiscard]] int getCommandId() const override { return 1; }

    // Brush strokes can be merged if they're consecutive
    [[nodiscard]] bool canMergeWith(const Command& other) const override;
    void mergeWith(Command& other) override;

private:
    SpectralLayer* targetLayer;
    std::unique_ptr<SpectralEdit> edit;
    std::string description;
    bool executed = false;
};

// Command for removing an edit from a layer
class RemoveEditCommand : public Command
{
public:
    RemoveEditCommand(SpectralLayer* layer,
                      size_t editIndex,
                      const std::string& description);

    void execute() override;
    void undo() override;
    [[nodiscard]] std::string getDescription() const override { return description; }
    [[nodiscard]] int getCommandId() const override { return 2; }

private:
    SpectralLayer* targetLayer;
    size_t editIndex;
    std::unique_ptr<SpectralEdit> removedEdit;
    std::string description;
};

// Command for layer operations (add/remove/rename)
class LayerCommand : public Command
{
public:
    enum class Type { Add, Remove, Rename, Move };

    LayerCommand(Type cmdType,
                 SpectralEditor* editor,
                 int layerIndex,
                 const std::string& param = "");

    void execute() override;
    void undo() override;
    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] int getCommandId() const override { return 3; }

private:
    Type type;
    SpectralEditor* editor;
    int layerIndex;
    int destIndex = -1;  // For Move
    std::string param;   // For Rename: new name, for Add: layer name
    std::string previousName;  // For Rename undo
    std::unique_ptr<class SpectralLayer> removedLayer;  // For Remove undo
};

// Undo manager with configurable history depth
class UndoManager
{
public:
    explicit UndoManager(size_t maxHistory = 100);
    ~UndoManager();

    // Execute a command and add to undo stack
    void executeCommand(std::unique_ptr<Command> command);

    void undo();
    void redo();

    [[nodiscard]] bool canUndo() const { return !undoStack.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoStack.empty(); }

    [[nodiscard]] std::string getUndoDescription() const;
    [[nodiscard]] std::string getRedoDescription() const;

    // Get history info
    [[nodiscard]] size_t getUndoStackSize() const { return undoStack.size(); }
    [[nodiscard]] size_t getRedoStackSize() const { return redoStack.size(); }

    void clear();
    void setMaxHistorySize(size_t size);

    // Mark current state as save point
    void markSavePoint();
    [[nodiscard]] bool isAtSavePoint() const;

    // Callback when undo state changes
    std::function<void()> onStateChanged;

private:
    void trimHistory();

    std::vector<std::unique_ptr<Command>> undoStack;
    std::vector<std::unique_ptr<Command>> redoStack;
    size_t maxHistorySize;

    // Save point tracking
    size_t savePointDepth = 0;
    bool savePointValid = true;
};

} // namespace spectralz
