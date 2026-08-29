#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class MidiPatternLauncherAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit MidiPatternLauncherAudioProcessorEditor(MidiPatternLauncherAudioProcessor&);
    ~MidiPatternLauncherAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;

    juce::String patternNameFromValue(int value) const;
    juce::String pendingNameFromValue(int value) const;
    juce::String transposeTextFromValue(int value) const;
    juce::String rotationTextFromValue(int value) const;
    juce::String inversionTextFromValue(bool value) const;
    juce::String retrogradeTextFromValue(bool value) const;
    juce::String m7TextFromValue(bool value) const;

    int getDisplayedPattern() const;
    juce::Rectangle<int> getPatternMatrixArea() const;
    juce::Rectangle<int> getPatternStepBox(int patternIndex, int stepIndex) const;

    bool getPatternStepAtPosition(juce::Point<int> position, int& patternIndexOut, int& stepIndexOut) const;
    bool getMelodyPaintCellAtPosition(juce::Point<int> position, int& stepIndexOut, int& midiNoteOut) const;

    void updateSelectedStepFromMousePosition(juce::Point<int> position);
    void toggleStepAtMousePosition(juce::Point<int> position);
    void setStepAtMousePosition(juce::Point<int> position, bool shouldHaveNote);
    void setPatternStepValue(int patternIndex, int stepIndex, bool shouldHaveNote);
    int getMatrixPitchForMousePosition(int patternIndex, int stepIndex, juce::Point<int> position) const;
    void setMatrixStepPitchAtMousePosition(juce::Point<int> position);
    int getMatrixVelocityForMousePosition(int patternIndex, int stepIndex, juce::Point<int> position) const;
    void setMatrixStepVelocityAtMousePosition(juce::Point<int> position);
    void moveMatrixStepAtMousePosition(juce::Point<int> position);
    void setMelodyPaintCellValue(int stepIndex, int midiNote, bool shouldHaveNote);
    void setMelodyPaintCellAtMousePosition(juce::Point<int> position, bool shouldHaveNote);
    void startMelodyDurationDrag(int stepIndex, int midiNote);
    void updateMelodyDragDurationAtMousePosition(juce::Point<int> position);
    void updateMelodyDragPitchAtMousePosition(juce::Point<int> position);
    void updateMelodyDragMoveAtMousePosition(juce::Point<int> position);

    bool melodyStepRangeOverlapsExistingNote(int patternIndex,
                                             int startStep,
                                             int durationSteps,
                                             int ignoredStepIndex) const;

    int getMaxNonOverlappingMelodyDuration(int patternIndex,
                                           int startStep,
                                           int requestedDuration,
                                           int ignoredStepIndex) const;

    int findMelodyNoteStartAtCell(int patternIndex,
                                  int stepIndex,
                                  int midiNote) const;

    void applyMelodyNoteEditSafely(int patternIndex,
                                   int stepIndex,
                                   int midiNote,
                                   int velocity,
                                   int durationSteps,
                                   int ignoredStepIndex);

    void setupButtonCallbacks();
    void updateEditPatternButtonHighlights();

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    MidiPatternLauncherAudioProcessor& audioProcessor;

    int selectedStep = 0;
    int selectedEditPattern = 0;
    
    enum class MatrixEditDragMode
    {
        None,
        PaintOrErase,
        PitchExistingOnly,
        VelocityExistingOnly,
        MoveExistingStep
    };

    MatrixEditDragMode matrixEditDragMode = MatrixEditDragMode::None;

    bool isDraggingPatternStepEdit = false;
    bool dragPaintValue = true;
    int lastEditedPattern = -1;
    int lastEditedStep = -1;

    int matrixMoveSourcePattern = -1;
    int matrixMoveSourceStep = -1;
    int matrixMoveNote = 60;
    int matrixMoveVelocity = 100;
    int matrixMoveDuration = 1;

    enum class MelodyEditDragMode
    {
        None,
        CreateOrResize,
        ResizeExistingDuration,
        MoveExistingTime,
        MoveExistingPitch,
        MoveExistingPitchAndDuration
    };

    MelodyEditDragMode melodyEditDragMode = MelodyEditDragMode::None;

    bool isDraggingMelodyDuration = false;
    int melodyDragPattern = -1;
    int melodyDragStartStep = -1;
    int melodyDragStartNote = -1;

    int melodyDragOriginalStep = -1;
    int melodyDragOriginalNote = -1;
    int melodyDragOriginalDuration = 1;
    int melodyDragOriginalVelocity = 100;
    int melodyDragClickOffsetSteps = 0;

    juce::TextButton editPattern1Button{ "Edit P1" };
    juce::TextButton editPattern2Button{ "Edit P2" };
    juce::TextButton editPattern3Button{ "Edit P3" };

    juce::TextButton copyToP1Button{ "Copy > P1" };
    juce::TextButton copyToP2Button{ "Copy > P2" };
    juce::TextButton copyToP3Button{ "Copy > P3" };
    juce::TextButton clearPatternButton{ "Clear Pattern" };

    juce::TextButton noteDownButton{ "Note -" };
    juce::TextButton noteUpButton{ "Note +" };
    juce::TextButton velocityDownButton{ "Vel -" };
    juce::TextButton velocityUpButton{ "Vel +" };
    juce::TextButton durationDownButton{ "Dur -" };
    juce::TextButton durationUpButton{ "Dur +" };
    juce::TextButton clearButton{ "Clear Step" };
    juce::TextButton makeNoteButton{ "Make Note" };

    juce::TextButton transposeDownOctaveButton{ "Tr -12" };
    juce::TextButton transposeDownButton{ "Tr -" };
    juce::TextButton transposeResetButton{ "Tr 0" };
    juce::TextButton transposeUpButton{ "Tr +" };
    juce::TextButton transposeUpOctaveButton{ "Tr +12" };

    juce::TextButton rotationDownButton{ "Rot -" };
    juce::TextButton rotationResetButton{ "Rot 0" };
    juce::TextButton rotationUpButton{ "Rot +" };
    juce::TextButton inversionToggleButton{ "Inv Off" };
    juce::TextButton retrogradeToggleButton{ "Retro Off" };
    juce::TextButton m7ToggleButton{ "M7 Off" };

    juce::Label targetTitleLabel;
    juce::Label targetPatternLabel;
    juce::Label gridModeLabel;
    juce::Label editorViewModeLabel;
    juce::Label targetStepLabel;
    juce::Label targetNoteLabel;
    juce::Label targetVelocityLabel;
    juce::Label targetDurationLabel;
    juce::Label globalSwingLabel;
    juce::Label globalRateLabel;
    juce::Label targetEnabledLabel;
    juce::Label externalControlLabel;
    juce::Label externalControlChannelLabel;
    juce::Label midiDebugLabel;
    juce::ToggleButton externalControlToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> externalControlToggleAttachment;


    juce::ComboBox targetPatternBox;
    juce::ComboBox gridModeBox;
    juce::ComboBox editorViewModeBox;
    juce::ComboBox globalSwingBox;
    juce::ComboBox globalRateBox;
    juce::Slider targetStepSlider;

    juce::Slider targetNoteSlider;
    juce::Slider targetVelocitySlider;
    juce::Slider targetDurationSlider;
    juce::ToggleButton targetEnabledButton{ "Enabled" };
    juce::ToggleButton externalControlEnabledButton{ "Ext Ctrl" };
    juce::ComboBox externalControlChannelBox;

    std::unique_ptr<ComboBoxAttachment> targetPatternAttachment;
    std::unique_ptr<ComboBoxAttachment> gridModeAttachment;
    std::unique_ptr<ComboBoxAttachment> editorViewModeAttachment;
    std::unique_ptr<ComboBoxAttachment> globalSwingAttachment;
    std::unique_ptr<ComboBoxAttachment> globalRateAttachment;
    std::unique_ptr<SliderAttachment> targetStepAttachment;
    std::unique_ptr<SliderAttachment> targetNoteAttachment;
    std::unique_ptr<SliderAttachment> targetVelocityAttachment;
    std::unique_ptr<SliderAttachment> targetDurationAttachment;
    std::unique_ptr<ButtonAttachment> targetEnabledAttachment;
    std::unique_ptr<ButtonAttachment> externalControlEnabledAttachment;
    std::unique_ptr<ComboBoxAttachment> externalControlChannelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiPatternLauncherAudioProcessorEditor)
};
