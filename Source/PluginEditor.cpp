#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiPatternLauncherAudioProcessorEditor::MidiPatternLauncherAudioProcessorEditor(MidiPatternLauncherAudioProcessor& p)
    : AudioProcessorEditor(&p),
    audioProcessor(p)
{
    // v1.29.0: +32 over the historical 760 to make room for the new Rate row
    // (targetRow3) split out from what used to be one overflowing targetRow2.
    setSize(900, 792);

    addAndMakeVisible(editPattern1Button);
    addAndMakeVisible(editPattern2Button);
    addAndMakeVisible(editPattern3Button);

    addAndMakeVisible(copyToP1Button);
    addAndMakeVisible(copyToP2Button);
    addAndMakeVisible(copyToP3Button);
    addAndMakeVisible(clearPatternButton);

    addAndMakeVisible(noteDownButton);
    addAndMakeVisible(noteUpButton);
    addAndMakeVisible(velocityDownButton);
    addAndMakeVisible(velocityUpButton);
    addAndMakeVisible(durationDownButton);
    addAndMakeVisible(durationUpButton);
    addAndMakeVisible(clearButton);
    addAndMakeVisible(makeNoteButton);

    addAndMakeVisible(transposeDownOctaveButton);
    addAndMakeVisible(transposeDownButton);
    addAndMakeVisible(transposeResetButton);
    addAndMakeVisible(transposeUpButton);
    addAndMakeVisible(transposeUpOctaveButton);

    addAndMakeVisible(rotationDownButton);
    addAndMakeVisible(rotationResetButton);
    addAndMakeVisible(rotationUpButton);
    addAndMakeVisible(inversionToggleButton);
    addAndMakeVisible(retrogradeToggleButton);
    addAndMakeVisible(m7ToggleButton);

    auto setupLabel = [](juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setColour(juce::Label::textColourId, juce::Colours::white);
            label.setJustificationType(juce::Justification::centredLeft);
        };

    setupLabel(targetTitleLabel, "Host Target Step Parameters");
    setupLabel(targetPatternLabel, "Pattern");
    setupLabel(gridModeLabel, "Grid");
    setupLabel(editorViewModeLabel, "View");
    setupLabel(targetStepLabel, "Step");
    setupLabel(targetNoteLabel, "Note");
    setupLabel(targetVelocityLabel, "Velocity");
    setupLabel(targetDurationLabel, "Duration");
    setupLabel(globalSwingLabel, "Swing");
    setupLabel(globalRateLabel, "Rate");
    setupLabel(targetEnabledLabel, "Enabled");
    setupLabel(externalControlLabel, "External");
    setupLabel(externalControlChannelLabel, "Channel");

    addAndMakeVisible(targetTitleLabel);
    addAndMakeVisible(targetPatternLabel);
    addAndMakeVisible(gridModeLabel);
    addAndMakeVisible(editorViewModeLabel);
    addAndMakeVisible(targetStepLabel);
    addAndMakeVisible(targetNoteLabel);
    addAndMakeVisible(targetVelocityLabel);
    addAndMakeVisible(targetDurationLabel);
    addAndMakeVisible(globalSwingLabel);
    addAndMakeVisible(globalRateLabel);
    addAndMakeVisible(targetEnabledLabel);
    addAndMakeVisible(externalControlLabel);
    addAndMakeVisible(externalControlChannelLabel);

    targetPatternBox.addItem("Pattern 1", 1);
    targetPatternBox.addItem("Pattern 2", 2);
    targetPatternBox.addItem("Pattern 3", 3);

    gridModeBox.addItem("Binary 16", 1);
    gridModeBox.addItem("Ternary 12", 2);

    globalSwingBox.addItem("Off", 1);
    globalSwingBox.addItem("Triplet", 2);
    globalSwingBox.addItem("Shuffle", 3);

    globalRateBox.addItem("Augmented", 1);
    globalRateBox.addItem("Normal", 2);
    globalRateBox.addItem("Diminished", 3);

    editorViewModeBox.addItem("Matrix", 1);
    editorViewModeBox.addItem("Melody", 2);

    externalControlChannelBox.addItem("All", 1);
    for (int channel = 1; channel <= 16; ++channel)
        externalControlChannelBox.addItem(juce::String(channel), channel + 1);

    auto setupSlider = [](juce::Slider& slider)
        {
            slider.setSliderStyle(juce::Slider::IncDecButtons);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1f2d32));
            slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff5aa6b8));
        };

    setupSlider(targetStepSlider);
    setupSlider(targetNoteSlider);
    setupSlider(targetVelocitySlider);
    setupSlider(targetDurationSlider);

    addAndMakeVisible(targetPatternBox);
    addAndMakeVisible(gridModeBox);
    addAndMakeVisible(editorViewModeBox);
    addAndMakeVisible(globalSwingBox);
    addAndMakeVisible(globalRateBox);
    addAndMakeVisible(targetStepSlider);
    addAndMakeVisible(targetNoteSlider);
    addAndMakeVisible(targetVelocitySlider);
    addAndMakeVisible(targetDurationSlider);
    addAndMakeVisible(targetEnabledButton);
    addAndMakeVisible(externalControlEnabledButton);
    addAndMakeVisible(externalControlChannelBox);

    midiDebugLabel.setText("Last CC: none", juce::dontSendNotification);
    midiDebugLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    midiDebugLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(midiDebugLabel);

    externalControlToggle.setButtonText("External Control");
    externalControlToggle.setClickingTogglesState(true);
    addAndMakeVisible(externalControlToggle);

    externalControlToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getApvts(),
        "externalControlEnabledParam",
        externalControlToggle);


    externalControlToggle.setButtonText("External Control");
    addAndMakeVisible(externalControlToggle);

    externalControlToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getApvts(),
        "externalControlEnabledParam",
        externalControlToggle);

    auto& apvts = audioProcessor.getAPVTS();

    targetPatternAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, "targetPatternParam", targetPatternBox);

    gridModeAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, "gridModeParam", gridModeBox);

    editorViewModeAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, "editorViewModeParam", editorViewModeBox);

    targetStepAttachment = std::make_unique<SliderAttachment>(
        apvts, "targetStepParam", targetStepSlider);

    targetNoteAttachment = std::make_unique<SliderAttachment>(
        apvts, "targetNoteParam", targetNoteSlider);

    targetVelocityAttachment = std::make_unique<SliderAttachment>(
        apvts, "targetVelocityParam", targetVelocitySlider);

    targetDurationAttachment = std::make_unique<SliderAttachment>(
        apvts, "targetDurationParam", targetDurationSlider);

    globalSwingAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, "globalSwingParam", globalSwingBox);

    globalRateAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, "globalRateParam", globalRateBox);

    targetEnabledAttachment = std::make_unique<ButtonAttachment>(
        apvts, "targetEnabledParam", targetEnabledButton);

    externalControlEnabledAttachment = std::make_unique<ButtonAttachment>(
        apvts, "externalControlEnabledParam", externalControlEnabledButton);

    externalControlChannelAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, "externalControlChannelParam", externalControlChannelBox);

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);

    setupButtonCallbacks();
    updateEditPatternButtonHighlights();

    startTimerHz(20);
}

MidiPatternLauncherAudioProcessorEditor::~MidiPatternLauncherAudioProcessorEditor()
{
    stopTimer();
}

void MidiPatternLauncherAudioProcessorEditor::setupButtonCallbacks()
{
    editPattern1Button.onClick = [this]()
        {
            selectedEditPattern = 0;
            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            updateEditPatternButtonHighlights();
            repaint();
        };
    editPattern2Button.onClick = [this]()
        {
            selectedEditPattern = 1;
            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            updateEditPatternButtonHighlights();
            repaint();
        };

    editPattern3Button.onClick = [this]()
        {
            selectedEditPattern = 2;
            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            updateEditPatternButtonHighlights();
            repaint();
        };

    copyToP1Button.onClick = [this]()
        {
            const int sourcePattern = getDisplayedPattern();
            audioProcessor.copyPattern(sourcePattern, 0);
            selectedEditPattern = 0;
            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            updateEditPatternButtonHighlights();
            repaint();
        };

    copyToP2Button.onClick = [this]()
        {
            const int sourcePattern = getDisplayedPattern();
            audioProcessor.copyPattern(sourcePattern, 1);
            selectedEditPattern = 1;
            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            updateEditPatternButtonHighlights();
            repaint();
        };

    copyToP3Button.onClick = [this]()
        {
            const int sourcePattern = getDisplayedPattern();
            audioProcessor.copyPattern(sourcePattern, 2);
            selectedEditPattern = 2;
            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            updateEditPatternButtonHighlights();
            repaint();
        };

    clearPatternButton.onClick = [this]()
        {
            audioProcessor.clearPattern(getDisplayedPattern());
            repaint();
        };

    noteDownButton.onClick = [this]()
        {
            audioProcessor.changeStepNote(getDisplayedPattern(), selectedStep, -1);
            repaint();
        };

    noteUpButton.onClick = [this]()
        {
            audioProcessor.changeStepNote(getDisplayedPattern(), selectedStep, 1);
            repaint();
        };

    velocityDownButton.onClick = [this]()
        {
            audioProcessor.changeStepVelocity(getDisplayedPattern(), selectedStep, -5);
            repaint();
        };

    velocityUpButton.onClick = [this]()
        {
            audioProcessor.changeStepVelocity(getDisplayedPattern(), selectedStep, 5);
            repaint();
        };

    durationDownButton.onClick = [this]()
        {
            audioProcessor.changeStepDuration(getDisplayedPattern(), selectedStep, -1);
            repaint();
        };

    durationUpButton.onClick = [this]()
        {
            audioProcessor.changeStepDuration(getDisplayedPattern(), selectedStep, 1);
            repaint();
        };

    clearButton.onClick = [this]()
        {
            audioProcessor.clearStep(getDisplayedPattern(), selectedStep);
            repaint();
        };

    makeNoteButton.onClick = [this]()
        {
            audioProcessor.makeStepNote(getDisplayedPattern(), selectedStep);
            repaint();
        };

    transposeDownOctaveButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.changePatternTranspose(pattern, -12);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    transposeDownButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.changePatternTranspose(pattern, -1);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    transposeResetButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.resetPatternTranspose(pattern);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    transposeUpButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.changePatternTranspose(pattern, 1);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    transposeUpOctaveButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.changePatternTranspose(pattern, 12);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    rotationDownButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.changePatternRotation(pattern, -1);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    rotationResetButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.resetPatternRotation(pattern);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    rotationUpButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.changePatternRotation(pattern, 1);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    inversionToggleButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.togglePatternInverted(pattern);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    retrogradeToggleButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.togglePatternRetrograde(pattern);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };

    m7ToggleButton.onClick = [this]()
        {
            const int pattern = getDisplayedPattern();
            audioProcessor.togglePatternM7(pattern);
            audioProcessor.updateAutomationParametersForPattern(pattern);
            repaint();
        };
}

void MidiPatternLauncherAudioProcessorEditor::updateEditPatternButtonHighlights()
{
    auto setButtonStyle = [](juce::TextButton& button, bool selected)
        {
            if (selected)
            {
                button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5aa6b8));
                button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5aa6b8));
                button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
                button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            }
            else
            {
                button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff26363b));
                button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff26363b));
                button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffedf6f8));
                button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffedf6f8));
            }
        };

    setButtonStyle(editPattern1Button, selectedEditPattern == 0);
    setButtonStyle(editPattern2Button, selectedEditPattern == 1);
    setButtonStyle(editPattern3Button, selectedEditPattern == 2);
}

void MidiPatternLauncherAudioProcessorEditor::timerCallback()
{
    selectedEditPattern = audioProcessor.getTargetPatternIndex();
    selectedStep = audioProcessor.getTargetStepIndex();

    juce::String debugText = "Last CC: ";

    const int cc = audioProcessor.getDebugLastCCNumber();
    const int ch = audioProcessor.getDebugLastCCChannel();
    const int val = audioProcessor.getDebugLastCCValue();
    const bool accepted = audioProcessor.getDebugLastCCAccepted();

    if (cc < 0)
    {
        debugText += "none";
    }
    else
    {
        debugText << "CC " << cc
                  << " ch " << ch
                  << " val " << val
                  << " | " << (accepted ? "accepted" : "ignored");
    }

    midiDebugLabel.setText(debugText, juce::dontSendNotification);

    updateEditPatternButtonHighlights();
    repaint();
}

juce::String MidiPatternLauncherAudioProcessorEditor::patternNameFromValue(int value) const
{
    if (value == 0)
        return "Pattern 1";

    if (value == 1)
        return "Pattern 2";

    if (value == 2)
        return "Pattern 3";

    return "Stopped";
}

juce::String MidiPatternLauncherAudioProcessorEditor::pendingNameFromValue(int value) const
{
    if (value == -2)
        return "None";

    if (value == -1)
        return "Stop";

    if (value == 0)
        return "Pattern 1";

    if (value == 1)
        return "Pattern 2";

    if (value == 2)
        return "Pattern 3";

    return "Unknown";
}

juce::String MidiPatternLauncherAudioProcessorEditor::transposeTextFromValue(int value) const
{
    if (value > 0)
        return "+" + juce::String(value);

    return juce::String(value);
}

juce::String MidiPatternLauncherAudioProcessorEditor::inversionTextFromValue(bool value) const
{
    return value ? "On" : "Off";
}

juce::String MidiPatternLauncherAudioProcessorEditor::retrogradeTextFromValue(bool value) const
{
    return value ? "On" : "Off";
}

juce::String MidiPatternLauncherAudioProcessorEditor::m7TextFromValue(bool value) const
{
    return value ? "On" : "Off";
}

juce::String MidiPatternLauncherAudioProcessorEditor::rotationTextFromValue(int value) const
{
    if (value > 0)
        return "+" + juce::String(value);

    return juce::String(value);
}

int MidiPatternLauncherAudioProcessorEditor::getDisplayedPattern() const
{
    return selectedEditPattern;
}

juce::Rectangle<int> MidiPatternLauncherAudioProcessorEditor::getPatternMatrixArea() const
{
    auto bounds = getLocalBounds().reduced(24);

    bounds.removeFromTop(26);   // title/version row
    bounds.removeFromTop(18);   // compact status row
    bounds.removeFromTop(4);
    bounds.removeFromTop(22);   // matrix title
    bounds.removeFromTop(4);

    const int editGridHeight = 330;

    return bounds.removeFromTop(editGridHeight);
}

juce::Rectangle<int> MidiPatternLauncherAudioProcessorEditor::getPatternStepBox(int patternIndex, int stepIndex) const
{
    auto matrixArea = getPatternMatrixArea();

    const int numberOfSteps = audioProcessor.getGridStepCount();

    const int rowHeight = 64;
    const int rowGap = 8;
    const int labelWidth = 34;
    const int transformWidth = 132;
    const int labelGap = 8;
    const int transformGap = 12;
    const int stepGap = 4;

    const int availableStepWidth = matrixArea.getWidth()
        - labelWidth
        - labelGap
        - transformGap
        - transformWidth;

    const int stepWidth = (availableStepWidth - ((numberOfSteps - 1) * stepGap)) / numberOfSteps;

    const int x = matrixArea.getX()
        + labelWidth
        + labelGap
        + stepIndex * (stepWidth + stepGap);

    const int y = matrixArea.getY()
        + patternIndex * (rowHeight + rowGap);

    return juce::Rectangle<int>(x, y, stepWidth, rowHeight);
}

bool MidiPatternLauncherAudioProcessorEditor::getPatternStepAtPosition(
    juce::Point<int> position,
    int& patternIndexOut,
    int& stepIndexOut) const
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    for (int patternIndex = 0; patternIndex < 3; ++patternIndex)
    {
        for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
        {
            if (getPatternStepBox(patternIndex, stepIndex).contains(position))
            {
                patternIndexOut = patternIndex;
                stepIndexOut = stepIndex;
                return true;
            }
        }
    }

    patternIndexOut = -1;
    stepIndexOut = -1;
    return false;
}

bool MidiPatternLauncherAudioProcessorEditor::getMelodyPaintCellAtPosition(
    juce::Point<int> position,
    int& stepIndexOut,
    int& midiNoteOut) const
{
    static constexpr int melodyLowestMidiNote = 48;
    static constexpr int melodyHighestMidiNote = 71;
    static constexpr int melodyNoteCount = melodyHighestMidiNote - melodyLowestMidiNote + 1;

    const int gridStepCount = audioProcessor.getGridStepCount();

    auto melodyArea = getPatternMatrixArea();

    const int noteLabelWidth = 38;
    const int stepLabelHeight = 16;
    const int stepGap = 3;
    const int noteGap = 1;

    melodyArea.removeFromLeft(noteLabelWidth);
    melodyArea.removeFromTop(stepLabelHeight);

    auto gridArea = melodyArea;

    if (!gridArea.contains(position))
    {
        stepIndexOut = -1;
        midiNoteOut = -1;
        return false;
    }

    const int cellWidth = (gridArea.getWidth() - ((gridStepCount - 1) * stepGap)) / gridStepCount;
    const int cellHeight = (gridArea.getHeight() - ((melodyNoteCount - 1) * noteGap)) / melodyNoteCount;

    for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
    {
        const int x = gridArea.getX() + stepIndex * (cellWidth + stepGap);

        for (int noteOffset = 0; noteOffset < melodyNoteCount; ++noteOffset)
        {
            const int y = gridArea.getY() + noteOffset * (cellHeight + noteGap);

            auto cellBox = juce::Rectangle<int>(
                x,
                y,
                cellWidth,
                cellHeight);

            if (cellBox.contains(position))
            {
                stepIndexOut = stepIndex;
                midiNoteOut = melodyHighestMidiNote - noteOffset;
                return true;
            }
        }
    }

    stepIndexOut = -1;
    midiNoteOut = -1;
    return false;
}


bool MidiPatternLauncherAudioProcessorEditor::melodyStepRangeOverlapsExistingNote(
    int patternIndex,
    int startStep,
    int durationSteps,
    int ignoredStepIndex) const
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    if (patternIndex < 0 || patternIndex >= 3)
        return true;

    if (startStep < 0 || startStep >= gridStepCount)
        return true;

    durationSteps = juce::jlimit(1, gridStepCount, durationSteps);

    const int newStart = startStep;
    const int newEnd = juce::jmin(gridStepCount - 1, startStep + durationSteps - 1);

    for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
    {
        if (stepIndex == ignoredStepIndex)
            continue;

        if (!audioProcessor.stepHasNote(patternIndex, stepIndex))
            continue;

        const int existingDuration = juce::jlimit(
            1,
            gridStepCount,
            audioProcessor.getStepDurationSteps(patternIndex, stepIndex));

        const int existingStart = stepIndex;
        const int existingEnd = juce::jmin(gridStepCount - 1, existingStart + existingDuration - 1);

        const bool rangesOverlap = newStart <= existingEnd && existingStart <= newEnd;

        if (rangesOverlap)
            return true;
    }

    return false;
}

int MidiPatternLauncherAudioProcessorEditor::getMaxNonOverlappingMelodyDuration(
    int patternIndex,
    int startStep,
    int requestedDuration,
    int ignoredStepIndex) const
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    if (startStep < 0 || startStep >= gridStepCount)
        return 1;

    requestedDuration = juce::jlimit(1, gridStepCount - startStep, requestedDuration);

    int maxDuration = requestedDuration;

    for (int duration = 1; duration <= requestedDuration; ++duration)
    {
        if (melodyStepRangeOverlapsExistingNote(patternIndex, startStep, duration, ignoredStepIndex))
            break;

        maxDuration = duration;
    }

    return juce::jlimit(1, gridStepCount - startStep, maxDuration);
}

int MidiPatternLauncherAudioProcessorEditor::findMelodyNoteStartAtCell(
    int patternIndex,
    int stepIndex,
    int midiNote) const
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    if (patternIndex < 0 || patternIndex >= 3)
        return -1;

    if (stepIndex < 0 || stepIndex >= gridStepCount)
        return -1;

    for (int candidateStart = 0; candidateStart < gridStepCount; ++candidateStart)
    {
        if (!audioProcessor.stepHasNote(patternIndex, candidateStart))
            continue;

        const int candidateNote = audioProcessor.getStepNote(patternIndex, candidateStart);

        if (candidateNote != midiNote)
            continue;

        const int candidateDuration = juce::jlimit(
            1,
            gridStepCount,
            audioProcessor.getStepDurationSteps(patternIndex, candidateStart));

        const int candidateEnd = juce::jmin(gridStepCount - 1, candidateStart + candidateDuration - 1);

        if (stepIndex >= candidateStart && stepIndex <= candidateEnd)
            return candidateStart;
    }

    return -1;
}

void MidiPatternLauncherAudioProcessorEditor::applyMelodyNoteEditSafely(
    int patternIndex,
    int stepIndex,
    int midiNote,
    int velocity,
    int durationSteps,
    int ignoredStepIndex)
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    if (patternIndex < 0 || patternIndex >= 3)
        return;

    if (stepIndex < 0 || stepIndex >= gridStepCount)
        return;

    midiNote = juce::jlimit(0, 127, midiNote);
    velocity = juce::jlimit(1, 127, velocity);
    durationSteps = juce::jlimit(1, gridStepCount - stepIndex, durationSteps);

    const int safeDuration = getMaxNonOverlappingMelodyDuration(
        patternIndex,
        stepIndex,
        durationSteps,
        ignoredStepIndex);

    if (melodyStepRangeOverlapsExistingNote(patternIndex, stepIndex, safeDuration, ignoredStepIndex))
        return;

    audioProcessor.setStepValues(
        patternIndex,
        stepIndex,
        midiNote,
        velocity,
        safeDuration);

    selectedEditPattern = patternIndex;
    selectedStep = stepIndex;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    audioProcessor.updateTargetParametersFromStep();

    lastEditedPattern = patternIndex;
    lastEditedStep = stepIndex;

    updateEditPatternButtonHighlights();
    repaint();
}

void MidiPatternLauncherAudioProcessorEditor::setMelodyPaintCellValue(
    int stepIndex,
    int midiNote,
    bool shouldHaveNote)
{
    static constexpr int melodyLowestMidiNote = 48;
    static constexpr int melodyHighestMidiNote = 71;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (stepIndex < 0 || stepIndex >= gridStepCount)
        return;

    if (midiNote < melodyLowestMidiNote || midiNote > melodyHighestMidiNote)
        return;

    const int patternIndex = getDisplayedPattern();

    if (lastEditedPattern == patternIndex && lastEditedStep == stepIndex && shouldHaveNote)
        return;

    const int existingNoteStart = findMelodyNoteStartAtCell(patternIndex, stepIndex, midiNote);

    if (shouldHaveNote)
    {
        // If the user clicks inside an already-sustained note block, select the
        // original note start instead of creating a duplicate hidden note.
        if (existingNoteStart >= 0)
        {
            selectedEditPattern = patternIndex;
            selectedStep = existingNoteStart;

            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            audioProcessor.updateTargetParametersFromStep();

            lastEditedPattern = patternIndex;
            lastEditedStep = existingNoteStart;

            updateEditPatternButtonHighlights();
            repaint();
            return;
        }

        // Full monophonic overlap protection: do not insert a new note if its
        // start cell is already covered by any existing note duration.
        if (melodyStepRangeOverlapsExistingNote(patternIndex, stepIndex, 1, -1))
            return;

        const int velocity = juce::jlimit(1, 127, audioProcessor.getTargetVelocity());
        const int requestedDuration = juce::jlimit(1, gridStepCount - stepIndex, audioProcessor.getTargetDurationSteps());

        applyMelodyNoteEditSafely(
            patternIndex,
            stepIndex,
            midiNote,
            velocity,
            requestedDuration,
            stepIndex);

        return;
    }

    // Erase behavior: if right-clicking anywhere inside a sustained note block,
    // erase the original note start.
    if (existingNoteStart >= 0)
        stepIndex = existingNoteStart;

    selectedEditPattern = patternIndex;
    selectedStep = stepIndex;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    audioProcessor.clearStep(patternIndex, stepIndex);
    audioProcessor.updateTargetParametersFromStep();

    lastEditedPattern = patternIndex;
    lastEditedStep = stepIndex;

    updateEditPatternButtonHighlights();
    repaint();
}

void MidiPatternLauncherAudioProcessorEditor::setMelodyPaintCellAtMousePosition(
    juce::Point<int> position,
    bool shouldHaveNote)
{
    int stepIndex = -1;
    int midiNote = -1;

    if (!getMelodyPaintCellAtPosition(position, stepIndex, midiNote))
        return;

    setMelodyPaintCellValue(stepIndex, midiNote, shouldHaveNote);
}

void MidiPatternLauncherAudioProcessorEditor::startMelodyDurationDrag(
    int stepIndex,
    int midiNote)
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    if (stepIndex < 0 || stepIndex >= gridStepCount)
        return;

    if (midiNote < 0 || midiNote > 127)
        return;

    melodyDragPattern = getDisplayedPattern();

    const int existingNoteStart = findMelodyNoteStartAtCell(melodyDragPattern, stepIndex, midiNote);

    if (existingNoteStart >= 0)
    {
        melodyDragStartStep = existingNoteStart;
        melodyDragStartNote = audioProcessor.getStepNote(melodyDragPattern, existingNoteStart);
    }
    else
    {
        if (melodyStepRangeOverlapsExistingNote(melodyDragPattern, stepIndex, 1, -1))
        {
            melodyEditDragMode = MelodyEditDragMode::None;
            isDraggingMelodyDuration = false;
            return;
        }

        melodyDragStartStep = stepIndex;
        melodyDragStartNote = midiNote;
    }

    melodyDragOriginalStep = melodyDragStartStep;
    melodyDragOriginalNote = melodyDragStartNote;

    melodyDragOriginalDuration = audioProcessor.stepHasNote(melodyDragPattern, melodyDragStartStep)
        ? juce::jlimit(1, gridStepCount, audioProcessor.getStepDurationSteps(melodyDragPattern, melodyDragStartStep))
        : 1;

    melodyDragOriginalVelocity = audioProcessor.stepHasNote(melodyDragPattern, melodyDragStartStep)
        ? juce::jlimit(1, 127, audioProcessor.getStepVelocity(melodyDragPattern, melodyDragStartStep))
        : juce::jlimit(1, 127, audioProcessor.getTargetVelocity());

    isDraggingMelodyDuration = true;
    melodyEditDragMode = MelodyEditDragMode::CreateOrResize;

    selectedEditPattern = melodyDragPattern;
    selectedStep = melodyDragStartStep;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);

    applyMelodyNoteEditSafely(
        melodyDragPattern,
        melodyDragStartStep,
        melodyDragStartNote,
        melodyDragOriginalVelocity,
        juce::jmax(1, melodyDragOriginalDuration),
        melodyDragStartStep);
}

void MidiPatternLauncherAudioProcessorEditor::updateMelodyDragDurationAtMousePosition(
    juce::Point<int> position)
{
    if (!isDraggingMelodyDuration)
        return;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (melodyDragPattern < 0 || melodyDragPattern >= 3)
        return;

    if (melodyDragStartStep < 0 || melodyDragStartStep >= gridStepCount)
        return;

    int hoverStepIndex = -1;
    int hoverMidiNote = -1;

    if (!getMelodyPaintCellAtPosition(position, hoverStepIndex, hoverMidiNote))
        return;

    juce::ignoreUnused(hoverMidiNote);

    const int endStep = juce::jlimit(melodyDragStartStep, gridStepCount - 1, hoverStepIndex);
    const int requestedDuration = juce::jlimit(
        1,
        gridStepCount - melodyDragStartStep,
        endStep - melodyDragStartStep + 1);

    const int velocity = audioProcessor.stepHasNote(melodyDragPattern, melodyDragStartStep)
        ? juce::jlimit(1, 127, audioProcessor.getStepVelocity(melodyDragPattern, melodyDragStartStep))
        : melodyDragOriginalVelocity;

    applyMelodyNoteEditSafely(
        melodyDragPattern,
        melodyDragStartStep,
        melodyDragStartNote,
        velocity,
        requestedDuration,
        melodyDragStartStep);
}

void MidiPatternLauncherAudioProcessorEditor::updateMelodyDragPitchAtMousePosition(
    juce::Point<int> position)
{
    if (!isDraggingMelodyDuration)
        return;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (melodyDragPattern < 0 || melodyDragPattern >= 3)
        return;

    if (melodyDragStartStep < 0 || melodyDragStartStep >= gridStepCount)
        return;

    int hoverStepIndex = -1;
    int hoverMidiNote = -1;

    if (!getMelodyPaintCellAtPosition(position, hoverStepIndex, hoverMidiNote))
        return;

    const int velocity = audioProcessor.stepHasNote(melodyDragPattern, melodyDragStartStep)
        ? juce::jlimit(1, 127, audioProcessor.getStepVelocity(melodyDragPattern, melodyDragStartStep))
        : melodyDragOriginalVelocity;

    int requestedDuration = audioProcessor.stepHasNote(melodyDragPattern, melodyDragStartStep)
        ? juce::jlimit(1, gridStepCount - melodyDragStartStep, audioProcessor.getStepDurationSteps(melodyDragPattern, melodyDragStartStep))
        : melodyDragOriginalDuration;

    if (melodyEditDragMode == MelodyEditDragMode::MoveExistingPitchAndDuration)
    {
        const int endStep = juce::jlimit(melodyDragStartStep, gridStepCount - 1, hoverStepIndex);

        requestedDuration = juce::jlimit(
            1,
            gridStepCount - melodyDragStartStep,
            endStep - melodyDragStartStep + 1);
    }

    applyMelodyNoteEditSafely(
        melodyDragPattern,
        melodyDragStartStep,
        hoverMidiNote,
        velocity,
        requestedDuration,
        melodyDragStartStep);
}

void MidiPatternLauncherAudioProcessorEditor::updateMelodyDragMoveAtMousePosition(
    juce::Point<int> position)
{
    if (!isDraggingMelodyDuration)
        return;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (melodyDragPattern < 0 || melodyDragPattern >= 3)
        return;

    if (melodyDragOriginalStep < 0 || melodyDragOriginalStep >= gridStepCount)
        return;

    int hoverStepIndex = -1;
    int hoverMidiNote = -1;

    if (!getMelodyPaintCellAtPosition(position, hoverStepIndex, hoverMidiNote))
        return;

    juce::ignoreUnused(hoverMidiNote);

    const int duration = juce::jlimit(
        1,
        gridStepCount,
        melodyDragOriginalDuration);

    const int maxStartStep = juce::jmax(0, gridStepCount - duration);

    const int requestedStartStep = juce::jlimit(
        0,
        maxStartStep,
        hoverStepIndex - melodyDragClickOffsetSteps);

    if (requestedStartStep == melodyDragStartStep)
        return;

    if (melodyStepRangeOverlapsExistingNote(
            melodyDragPattern,
            requestedStartStep,
            duration,
            melodyDragOriginalStep))
    {
        return;
    }

    const int note = juce::jlimit(0, 127, melodyDragOriginalNote);
    const int velocity = juce::jlimit(1, 127, melodyDragOriginalVelocity);

    if (melodyDragStartStep >= 0 && melodyDragStartStep < gridStepCount)
        audioProcessor.clearStep(melodyDragPattern, melodyDragStartStep);

    melodyDragStartStep = requestedStartStep;
    melodyDragStartNote = note;

    audioProcessor.setStepValues(
        melodyDragPattern,
        melodyDragStartStep,
        note,
        velocity,
        duration);

    selectedEditPattern = melodyDragPattern;
    selectedStep = melodyDragStartStep;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    audioProcessor.updateTargetParametersFromStep();

    lastEditedPattern = melodyDragPattern;
    lastEditedStep = melodyDragStartStep;

    updateEditPatternButtonHighlights();
    repaint();
}

void MidiPatternLauncherAudioProcessorEditor::updateSelectedStepFromMousePosition(juce::Point<int> position)
{
    auto matrixArea = getPatternMatrixArea();

    const int gridStepCount = audioProcessor.getGridStepCount();

    const int rowHeight = 64;
    const int rowGap = 8;
    const int labelWidth = 34;

    for (int patternIndex = 0; patternIndex < 3; ++patternIndex)
    {
        const int rowY = matrixArea.getY() + patternIndex * (rowHeight + rowGap);

        auto labelBox = juce::Rectangle<int>(matrixArea.getX(),
            rowY,
            labelWidth,
            rowHeight);

        if (labelBox.contains(position))
        {
            selectedEditPattern = patternIndex;

            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);

            updateEditPatternButtonHighlights();
            repaint();
            return;
        }

        for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
        {
            if (getPatternStepBox(patternIndex, stepIndex).contains(position))
            {
                selectedEditPattern = patternIndex;
                selectedStep = stepIndex;

                audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);

                updateEditPatternButtonHighlights();
                repaint();
                return;
            }
        }
    }
}

void MidiPatternLauncherAudioProcessorEditor::toggleStepAtMousePosition(juce::Point<int> position)
{
    int patternIndex = -1;
    int stepIndex = -1;

    if (!getPatternStepAtPosition(position, patternIndex, stepIndex))
        return;

    selectedEditPattern = patternIndex;
    selectedStep = stepIndex;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);

    if (audioProcessor.stepHasNote(patternIndex, stepIndex))
        audioProcessor.clearStep(patternIndex, stepIndex);
    else
        audioProcessor.makeStepNote(patternIndex, stepIndex);

    updateEditPatternButtonHighlights();
    repaint();
}

int MidiPatternLauncherAudioProcessorEditor::getMatrixPitchForMousePosition(
    int patternIndex,
    int stepIndex,
    juce::Point<int> position) const
{
    static constexpr int matrixLowestMidiNote = 48;
    static constexpr int matrixHighestMidiNote = 71;
    static constexpr int matrixNoteRange = matrixHighestMidiNote - matrixLowestMidiNote;

    if (patternIndex < 0 || patternIndex >= 3)
        return matrixLowestMidiNote;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (stepIndex < 0 || stepIndex >= gridStepCount)
        return matrixLowestMidiNote;

    const auto stepBox = getPatternStepBox(patternIndex, stepIndex);

    if (stepBox.getHeight() <= 1)
        return matrixLowestMidiNote;

    const int relativeY = juce::jlimit(
        0,
        stepBox.getHeight() - 1,
        position.y - stepBox.getY());

    const float normalisedFromTop = static_cast<float>(relativeY)
        / static_cast<float>(stepBox.getHeight() - 1);

    const int noteOffsetFromTop = juce::roundToInt(normalisedFromTop * static_cast<float>(matrixNoteRange));

    return juce::jlimit(
        matrixLowestMidiNote,
        matrixHighestMidiNote,
        matrixHighestMidiNote - noteOffsetFromTop);
}

void MidiPatternLauncherAudioProcessorEditor::setMatrixStepPitchAtMousePosition(
    juce::Point<int> position)
{
    int patternIndex = -1;
    int stepIndex = -1;

    if (!getPatternStepAtPosition(position, patternIndex, stepIndex))
        return;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (patternIndex < 0 || patternIndex >= 3 || stepIndex < 0 || stepIndex >= gridStepCount)
        return;

    if (!audioProcessor.stepHasNote(patternIndex, stepIndex))
        return;

    const int midiNote = getMatrixPitchForMousePosition(patternIndex, stepIndex, position);
    const int velocity = juce::jlimit(1, 127, audioProcessor.getStepVelocity(patternIndex, stepIndex));
    const int duration = juce::jlimit(
        1,
        gridStepCount - stepIndex,
        audioProcessor.getStepDurationSteps(patternIndex, stepIndex));

    audioProcessor.setStepValues(
        patternIndex,
        stepIndex,
        midiNote,
        velocity,
        duration);

    selectedEditPattern = patternIndex;
    selectedStep = stepIndex;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    audioProcessor.updateTargetParametersFromStep();

    lastEditedPattern = patternIndex;
    lastEditedStep = stepIndex;

    updateEditPatternButtonHighlights();
    repaint();
}

int MidiPatternLauncherAudioProcessorEditor::getMatrixVelocityForMousePosition(
    int patternIndex,
    int stepIndex,
    juce::Point<int> position) const
{
    if (patternIndex < 0 || patternIndex >= 3)
        return 100;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (stepIndex < 0 || stepIndex >= gridStepCount)
        return 100;

    const auto stepBox = getPatternStepBox(patternIndex, stepIndex);

    if (stepBox.getHeight() <= 1)
        return 100;

    const int relativeY = juce::jlimit(
        0,
        stepBox.getHeight() - 1,
        position.y - stepBox.getY());

    const float normalisedFromTop = static_cast<float>(relativeY)
        / static_cast<float>(stepBox.getHeight() - 1);

    const int velocityOffsetFromTop = juce::roundToInt(normalisedFromTop * 126.0f);

    return juce::jlimit(1, 127, 127 - velocityOffsetFromTop);
}

void MidiPatternLauncherAudioProcessorEditor::setMatrixStepVelocityAtMousePosition(
    juce::Point<int> position)
{
    int patternIndex = -1;
    int stepIndex = -1;

    if (!getPatternStepAtPosition(position, patternIndex, stepIndex))
        return;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (patternIndex < 0 || patternIndex >= 3 || stepIndex < 0 || stepIndex >= gridStepCount)
        return;

    if (!audioProcessor.stepHasNote(patternIndex, stepIndex))
        return;

    const int midiNote = audioProcessor.getStepNote(patternIndex, stepIndex);
    const int velocity = getMatrixVelocityForMousePosition(patternIndex, stepIndex, position);
    const int duration = juce::jlimit(
        1,
        gridStepCount - stepIndex,
        audioProcessor.getStepDurationSteps(patternIndex, stepIndex));

    audioProcessor.setStepValues(
        patternIndex,
        stepIndex,
        midiNote,
        velocity,
        duration);

    selectedEditPattern = patternIndex;
    selectedStep = stepIndex;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    audioProcessor.updateTargetParametersFromStep();

    lastEditedPattern = patternIndex;
    lastEditedStep = stepIndex;

    updateEditPatternButtonHighlights();
    repaint();
}

void MidiPatternLauncherAudioProcessorEditor::moveMatrixStepAtMousePosition(
    juce::Point<int> position)
{
    if (matrixMoveSourcePattern < 0 || matrixMoveSourcePattern >= 3)
        return;

    const int gridStepCount = audioProcessor.getGridStepCount();

    if (matrixMoveSourceStep < 0 || matrixMoveSourceStep >= gridStepCount)
        return;

    int targetPattern = -1;
    int targetStep = -1;

    if (!getPatternStepAtPosition(position, targetPattern, targetStep))
        return;

    if (targetPattern != matrixMoveSourcePattern)
        return;

    if (targetStep < 0 || targetStep >= gridStepCount)
        return;

    if (targetStep == matrixMoveSourceStep)
        return;

    if (targetPattern == lastEditedPattern && targetStep == lastEditedStep)
        return;

    if (audioProcessor.stepHasNote(targetPattern, targetStep))
        return;

    const int safeDuration = juce::jlimit(
        1,
        gridStepCount - targetStep,
        matrixMoveDuration);

    audioProcessor.setStepValues(
        targetPattern,
        targetStep,
        matrixMoveNote,
        matrixMoveVelocity,
        safeDuration);

    setPatternStepValue(matrixMoveSourcePattern, matrixMoveSourceStep, false);

    selectedEditPattern = targetPattern;
    selectedStep = targetStep;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    audioProcessor.updateTargetParametersFromStep();

    matrixMoveSourcePattern = targetPattern;
    matrixMoveSourceStep = targetStep;
    matrixMoveDuration = safeDuration;

    lastEditedPattern = targetPattern;
    lastEditedStep = targetStep;

    updateEditPatternButtonHighlights();
    repaint();
}

void MidiPatternLauncherAudioProcessorEditor::setStepAtMousePosition(
    juce::Point<int> position,
    bool shouldHaveNote)
{
    int patternIndex = -1;
    int stepIndex = -1;

    if (!getPatternStepAtPosition(position, patternIndex, stepIndex))
        return;

    setPatternStepValue(patternIndex, stepIndex, shouldHaveNote);

    if (shouldHaveNote)
        setMatrixStepPitchAtMousePosition(position);
}

void MidiPatternLauncherAudioProcessorEditor::setPatternStepValue(
    int patternIndex,
    int stepIndex,
    bool shouldHaveNote)
{
    const int gridStepCount = audioProcessor.getGridStepCount();

    if (patternIndex < 0 || patternIndex >= 3 || stepIndex < 0 || stepIndex >= gridStepCount)
        return;


    if (lastEditedPattern == patternIndex && lastEditedStep == stepIndex)
        return;

    selectedEditPattern = patternIndex;
    selectedStep = stepIndex;

    audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);

    const bool currentlyHasNote = audioProcessor.stepHasNote(patternIndex, stepIndex);

    if (shouldHaveNote && !currentlyHasNote)
        audioProcessor.makeStepNote(patternIndex, stepIndex);
    else if (!shouldHaveNote && currentlyHasNote)
        audioProcessor.clearStep(patternIndex, stepIndex);

    lastEditedPattern = patternIndex;
    lastEditedStep = stepIndex;

    updateEditPatternButtonHighlights();
    repaint();
}

void MidiPatternLauncherAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto position = event.getPosition();

    lastEditedPattern = -1;
    lastEditedStep = -1;
    matrixEditDragMode = MatrixEditDragMode::None;
    matrixMoveSourcePattern = -1;
    matrixMoveSourceStep = -1;

    if (audioProcessor.isMelodyPaintMode())
    {
        int melodyStepIndex = -1;
        int melodyMidiNote = -1;

        isDraggingMelodyDuration = false;
        melodyEditDragMode = MelodyEditDragMode::None;
        melodyDragPattern = -1;
        melodyDragStartStep = -1;
        melodyDragStartNote = -1;
        melodyDragOriginalStep = -1;
        melodyDragOriginalNote = -1;
        melodyDragOriginalDuration = 1;
        melodyDragOriginalVelocity = 100;
        melodyDragClickOffsetSteps = 0;

        if (getMelodyPaintCellAtPosition(position, melodyStepIndex, melodyMidiNote))
        {
            const bool rightClickErase = event.mods.isRightButtonDown();
            const bool shiftDown = event.mods.isShiftDown();
            const bool altDown = event.mods.isAltDown();

            const int melodyPattern = getDisplayedPattern();
            const int existingNoteStart = findMelodyNoteStartAtCell(
                melodyPattern,
                melodyStepIndex,
                melodyMidiNote);

            if (rightClickErase)
            {
                dragPaintValue = false;
                isDraggingPatternStepEdit = true;
                setMelodyPaintCellValue(melodyStepIndex, melodyMidiNote, false);
            }
            else if ((altDown || shiftDown) && existingNoteStart >= 0)
            {
                melodyDragPattern = melodyPattern;
                melodyDragStartStep = existingNoteStart;
                melodyDragStartNote = audioProcessor.getStepNote(melodyDragPattern, melodyDragStartStep);

                melodyDragOriginalStep = melodyDragStartStep;
                melodyDragOriginalNote = melodyDragStartNote;
                melodyDragOriginalDuration = juce::jlimit(
                    1,
                    audioProcessor.getGridStepCount(),
                    audioProcessor.getStepDurationSteps(melodyDragPattern, melodyDragStartStep));
                melodyDragOriginalVelocity = juce::jlimit(
                    1,
                    127,
                    audioProcessor.getStepVelocity(melodyDragPattern, melodyDragStartStep));

                melodyDragClickOffsetSteps = juce::jlimit(
                    0,
                    juce::jmax(0, melodyDragOriginalDuration - 1),
                    melodyStepIndex - existingNoteStart);

                isDraggingMelodyDuration = true;
                isDraggingPatternStepEdit = false;

                if (altDown)
                {
                    melodyEditDragMode = shiftDown
                        ? MelodyEditDragMode::MoveExistingPitchAndDuration
                        : MelodyEditDragMode::MoveExistingPitch;
                }
                else
                {
                    melodyEditDragMode = MelodyEditDragMode::MoveExistingTime;
                }

                selectedEditPattern = melodyDragPattern;
                selectedStep = melodyDragStartStep;

                audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
                audioProcessor.updateTargetParametersFromStep();

                updateEditPatternButtonHighlights();
                repaint();
            }
            else
            {
                dragPaintValue = true;
                isDraggingPatternStepEdit = false;

                startMelodyDurationDrag(melodyStepIndex, melodyMidiNote);
            }

            return;
        }

        isDraggingPatternStepEdit = false;
        return;
    }

    int patternIndex = -1;
    int stepIndex = -1;

    if (getPatternStepAtPosition(position, patternIndex, stepIndex))
    {
        const bool rightClickErase = event.mods.isRightButtonDown();
        const bool altDown = event.mods.isAltDown();
        const bool ctrlDown = event.mods.isCtrlDown();
        const bool shiftDown = event.mods.isShiftDown();

        // Matrix mode is paint/edit oriented:
        // - Left click creates/keeps a note and applies pitch from mouse Y.
        // - Right click is the dedicated erase gesture.
        // - Alt click/drag pitch-paints existing notes only.
        // - Ctrl click/drag velocity-paints existing notes only.
        // - Shift drag moves an existing note horizontally within the same row.
        if (shiftDown && !rightClickErase && !audioProcessor.stepHasNote(patternIndex, stepIndex))
        {
            matrixEditDragMode = MatrixEditDragMode::None;
            isDraggingPatternStepEdit = false;
            return;
        }

        if (shiftDown && !rightClickErase && audioProcessor.stepHasNote(patternIndex, stepIndex))
            matrixEditDragMode = MatrixEditDragMode::MoveExistingStep;
        else if (ctrlDown && !rightClickErase)
            matrixEditDragMode = MatrixEditDragMode::VelocityExistingOnly;
        else if (altDown && !rightClickErase)
            matrixEditDragMode = MatrixEditDragMode::PitchExistingOnly;
        else
            matrixEditDragMode = MatrixEditDragMode::PaintOrErase;

        dragPaintValue = !rightClickErase;
        isDraggingPatternStepEdit = true;

        if (matrixEditDragMode == MatrixEditDragMode::MoveExistingStep)
        {
            const int gridStepCount = audioProcessor.getGridStepCount();

            matrixMoveSourcePattern = patternIndex;
            matrixMoveSourceStep = stepIndex;
            matrixMoveNote = audioProcessor.getStepNote(patternIndex, stepIndex);
            matrixMoveVelocity = juce::jlimit(1, 127, audioProcessor.getStepVelocity(patternIndex, stepIndex));
            matrixMoveDuration = juce::jlimit(
                1,
                gridStepCount - stepIndex,
                audioProcessor.getStepDurationSteps(patternIndex, stepIndex));

            selectedEditPattern = patternIndex;
            selectedStep = stepIndex;

            audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
            audioProcessor.updateTargetParametersFromStep();

            updateEditPatternButtonHighlights();
            repaint();
        }
        else if (matrixEditDragMode == MatrixEditDragMode::PitchExistingOnly)
        {
            setMatrixStepPitchAtMousePosition(position);
        }
        else if (matrixEditDragMode == MatrixEditDragMode::VelocityExistingOnly)
        {
            setMatrixStepVelocityAtMousePosition(position);
        }
        else
        {
            setPatternStepValue(patternIndex, stepIndex, dragPaintValue);

            if (dragPaintValue)
                setMatrixStepPitchAtMousePosition(position);
        }

        return;
    }

    isDraggingPatternStepEdit = false;
    updateSelectedStepFromMousePosition(position);
}

void MidiPatternLauncherAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (audioProcessor.isMelodyPaintMode())
    {
        if (isDraggingMelodyDuration)
        {
            if (melodyEditDragMode == MelodyEditDragMode::MoveExistingTime)
            {
                updateMelodyDragMoveAtMousePosition(event.getPosition());
                return;
            }

            if (melodyEditDragMode == MelodyEditDragMode::MoveExistingPitch
                || melodyEditDragMode == MelodyEditDragMode::MoveExistingPitchAndDuration)
            {
                updateMelodyDragPitchAtMousePosition(event.getPosition());
                return;
            }

            updateMelodyDragDurationAtMousePosition(event.getPosition());
            return;
        }

        if (isDraggingPatternStepEdit)
        {
            setMelodyPaintCellAtMousePosition(event.getPosition(), dragPaintValue);
            return;
        }

        return;
    }

    if (!isDraggingPatternStepEdit)
        return;

    if (matrixEditDragMode == MatrixEditDragMode::PitchExistingOnly)
    {
        setMatrixStepPitchAtMousePosition(event.getPosition());
        return;
    }

    if (matrixEditDragMode == MatrixEditDragMode::VelocityExistingOnly)
    {
        setMatrixStepVelocityAtMousePosition(event.getPosition());
        return;
    }

    if (matrixEditDragMode == MatrixEditDragMode::MoveExistingStep)
    {
        moveMatrixStepAtMousePosition(event.getPosition());
        return;
    }

    setStepAtMousePosition(event.getPosition(), dragPaintValue);
}

void MidiPatternLauncherAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    isDraggingPatternStepEdit = false;
    matrixEditDragMode = MatrixEditDragMode::None;
    matrixMoveSourcePattern = -1;
    matrixMoveSourceStep = -1;
    isDraggingMelodyDuration = false;
    melodyEditDragMode = MelodyEditDragMode::None;

    melodyDragPattern = -1;
    melodyDragStartStep = -1;
    melodyDragStartNote = -1;
    melodyDragOriginalStep = -1;
    melodyDragOriginalNote = -1;
    melodyDragOriginalDuration = 1;
    melodyDragOriginalVelocity = 100;
    melodyDragClickOffsetSteps = 0;

    lastEditedPattern = -1;
    lastEditedStep = -1;
}

void MidiPatternLauncherAudioProcessorEditor::paint(juce::Graphics& g)
{
    const int gridStepCount = audioProcessor.getGridStepCount();
    const int editGridHeight = 330;

    if (selectedStep >= gridStepCount)
    {
        selectedStep = gridStepCount - 1;
        audioProcessor.setTargetPatternAndStep(selectedEditPattern, selectedStep);
    }

    g.fillAll(juce::Colour(0xff2f3f45));

    const int activePattern = audioProcessor.getDisplayActivePattern();
    const int pendingPattern = audioProcessor.getDisplayPendingPattern();
    const int currentStep = audioProcessor.getDisplayCurrentStep();
    const int currentBar = audioProcessor.getDisplayCurrentBar();
    const int activeLoopLength = activePattern >= 0
        ? juce::jmin(audioProcessor.getPatternLoopLength(activePattern), gridStepCount)
        : gridStepCount;

    const int displayedPattern = getDisplayedPattern();
    const int patternTranspose = audioProcessor.getPatternTranspose(displayedPattern);
    const int patternRotation = audioProcessor.getPatternRotation(displayedPattern);
    const bool patternInverted = audioProcessor.getPatternInverted(displayedPattern);
    const bool patternRetrograde = audioProcessor.getPatternRetrograde(displayedPattern);
    const bool patternM7 = audioProcessor.getPatternM7(displayedPattern);

    auto bounds = getLocalBounds().reduced(24);

    //==========================================================================
    // Compact title/header

    auto titleRow = bounds.removeFromTop(26);

    g.setColour(juce::Colours::white);
    g.setFont(22.0f);
    g.drawFittedText("MIDI Pattern Launcher",
        titleRow.removeFromLeft(titleRow.getWidth() / 2),
        juce::Justification::centredLeft,
        1);

    g.setFont(14.0f);
    g.setColour(juce::Colour(0xffcfe8ef));
    g.drawFittedText("v1.28.1 - Composer Bridge v2",
        titleRow,
        juce::Justification::centredRight,
        1);

    auto statusRow = bounds.removeFromTop(22);

    g.setColour(juce::Colour(0xffcfe8ef));
    g.setFont(15.0f);

    juce::String compactStatus;
    compactStatus << "Active: " << patternNameFromValue(activePattern)
        << "    Pending: " << pendingNameFromValue(pendingPattern)
        << "    Bar: " << currentBar
        << "    Step: " << currentStep << " / " << activeLoopLength;

    g.drawFittedText(compactStatus,
        statusRow,
        juce::Justification::centred,
        1);

    bounds.removeFromTop(4);

    //==========================================================================
    // Pattern matrix title

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    juce::String matrixTitle;
    matrixTitle << (audioProcessor.isMelodyPaintMode()
        ? "Melody Paint / Target Edit"
        : "Pattern Matrix / Target Edit")
        << "    Selected: P" << (displayedPattern + 1)
        << " Step " << (selectedStep + 1);

    g.drawFittedText(matrixTitle,
        bounds.removeFromTop(22),
        juce::Justification::centredLeft,
        1);

    bounds.removeFromTop(4);

    //==========================================================================
    // Pattern matrix / Melody paint grid

    if (audioProcessor.isMelodyPaintMode())
    {
        auto melodyArea = getPatternMatrixArea();

        static constexpr int melodyLowestMidiNote = 48;
        static constexpr int melodyHighestMidiNote = 71;
        static constexpr int melodyNoteCount = melodyHighestMidiNote - melodyLowestMidiNote + 1;

        const int noteLabelWidth = 38;
        const int stepLabelHeight = 16;
        const int stepGap = 3;
        const int noteGap = 1;

        auto noteLabelArea = melodyArea.removeFromLeft(noteLabelWidth);
        juce::ignoreUnused(noteLabelArea);

        auto stepHeaderArea = melodyArea.removeFromTop(stepLabelHeight);
        auto gridArea = melodyArea;

        const int cellWidth = (gridArea.getWidth() - ((gridStepCount - 1) * stepGap)) / gridStepCount;
        const int cellHeight = (gridArea.getHeight() - ((melodyNoteCount - 1) * noteGap)) / melodyNoteCount;

        g.setColour(juce::Colour(0xff1f2d32));
        g.fillRoundedRectangle(gridArea.toFloat(), 6.0f);

        // Step labels
        for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
        {
            const int x = gridArea.getX() + stepIndex * (cellWidth + stepGap);

            auto stepLabelBox = juce::Rectangle<int>(
                x,
                stepHeaderArea.getY(),
                cellWidth,
                stepHeaderArea.getHeight());

            g.setColour(juce::Colour(0xffcfe8ef));
            g.setFont(10.0f);
            g.drawFittedText(juce::String(stepIndex + 1),
                stepLabelBox,
                juce::Justification::centred,
                1);
        }

        // MIDI note rows: highest note at top, lowest note at bottom.
        for (int noteOffset = 0; noteOffset < melodyNoteCount; ++noteOffset)
        {
            const int midiNote = melodyHighestMidiNote - noteOffset;
            const int y = gridArea.getY() + noteOffset * (cellHeight + noteGap);

            auto noteLabelBox = juce::Rectangle<int>(
                getPatternMatrixArea().getX(),
                y,
                noteLabelWidth - 6,
                cellHeight);

            const bool isReferenceOctaveNote = (midiNote % 12) == 0;

            g.setColour(isReferenceOctaveNote
                ? juce::Colour(0xfff5c542)
                : juce::Colour(0xffcfe8ef));

            g.setFont(isReferenceOctaveNote ? 11.0f : 10.0f);
            g.drawFittedText(juce::String(midiNote),
                noteLabelBox,
                juce::Justification::centredRight,
                1);

            for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
            {
                const int x = gridArea.getX() + stepIndex * (cellWidth + stepGap);

                auto cellBox = juce::Rectangle<int>(
                    x,
                    y,
                    cellWidth,
                    cellHeight);

                const int loopLength = juce::jmin(audioProcessor.getPatternLoopLength(displayedPattern), gridStepCount);
                const bool isInsideLoop = stepIndex < loopLength;
                const bool stepHasNote = audioProcessor.stepHasNote(displayedPattern, stepIndex);
                const int stepNote = audioProcessor.getStepNote(displayedPattern, stepIndex);
                const bool isNoteCell = stepHasNote && stepNote == midiNote;
                const bool isCurrentStep = activePattern == displayedPattern && currentStep == (stepIndex + 1);
                const bool isSelectedColumn = selectedEditPattern == displayedPattern && selectedStep == stepIndex;

                if (isCurrentStep)
                    g.setColour(juce::Colour(0xff5a5124));
                else if (isReferenceOctaveNote)
                    g.setColour(isInsideLoop ? juce::Colour(0xff263f45) : juce::Colour(0xff1c292d));
                else
                    g.setColour(isInsideLoop ? juce::Colour(0xff26363b) : juce::Colour(0xff1c292d));

                g.fillRoundedRectangle(cellBox.toFloat(), 2.0f);

                g.setColour(juce::Colour(0xff3b5056));
                g.drawRoundedRectangle(cellBox.toFloat(), 2.0f, 0.5f);

                if (isSelectedColumn)
                {
                    g.setColour(juce::Colour(0x66ffffff));
                    g.drawRoundedRectangle(cellBox.toFloat().reduced(1.0f), 2.0f, 1.0f);
                }
            }
        }

        // Draw duration bars after the background grid has been drawn.
        for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
        {
            if (!audioProcessor.stepHasNote(displayedPattern, stepIndex))
                continue;

            const int midiNote = audioProcessor.getStepNote(displayedPattern, stepIndex);

            if (midiNote < melodyLowestMidiNote || midiNote > melodyHighestMidiNote)
                continue;

            const int duration = juce::jlimit(1,
                gridStepCount,
                audioProcessor.getStepDurationSteps(displayedPattern, stepIndex));

            const int endStep = juce::jlimit(stepIndex,
                gridStepCount - 1,
                stepIndex + duration - 1);

            const int noteOffset = melodyHighestMidiNote - midiNote;

            const int x = gridArea.getX() + stepIndex * (cellWidth + stepGap);
            const int endX = gridArea.getX() + endStep * (cellWidth + stepGap) + cellWidth;
            const int y = gridArea.getY() + noteOffset * (cellHeight + noteGap);

            auto noteBar = juce::Rectangle<int>(
                x,
                y,
                endX - x,
                cellHeight).reduced(1, 1);

            const bool isCurrentStep = activePattern == displayedPattern
                && currentStep >= (stepIndex + 1)
                && currentStep <= (endStep + 1);

            const bool isSelectedNote = selectedEditPattern == displayedPattern
                && selectedStep == stepIndex;

            g.setColour(isCurrentStep
                ? juce::Colour(0xfff5c542)
                : juce::Colour(0xff5aa6b8));

            g.fillRoundedRectangle(noteBar.toFloat(), 3.0f);

            g.setColour(isCurrentStep ? juce::Colours::black : juce::Colours::white);
            g.drawRoundedRectangle(noteBar.toFloat(), 3.0f, 1.0f);

            g.setFont(10.0f);
            g.drawFittedText(juce::String(midiNote),
                noteBar.reduced(3, 1),
                juce::Justification::centredLeft,
                1);

            if (isSelectedNote)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(noteBar.toFloat().reduced(1.0f), 3.0f, 2.0f);
            }
        }

        g.setColour(juce::Colour(0xffcfe8ef));
        g.setFont(12.0f);

        juce::String melodyInfo;
        melodyInfo << "MIDI notes 48-71    P" << (displayedPattern + 1)
            << "    Grid: " << gridStepCount << " steps";

        g.drawFittedText(melodyInfo,
            getPatternMatrixArea().withTrimmedTop(getPatternMatrixArea().getHeight() - 18),
            juce::Justification::centredRight,
            1);
    }
    else
    {
        //==========================================================================
        // Pattern matrix

        auto matrixArea = getPatternMatrixArea();

        const int rowHeight = 64;
        const int rowGap = 8;
        const int labelWidth = 34;
        const int transformWidth = 132;
        const int labelGap = 8;
        const int transformGap = 12;

        for (int patternIndex = 0; patternIndex < 3; ++patternIndex)
        {
            const int rowY = matrixArea.getY() + patternIndex * (rowHeight + rowGap);

            auto labelBox = juce::Rectangle<int>(matrixArea.getX(),
                rowY,
                labelWidth,
                rowHeight);

            const bool isActivePattern = activePattern == patternIndex;
            const bool isPendingPattern = pendingPattern == patternIndex;
            const bool isSelectedPattern = displayedPattern == patternIndex;

            if (isActivePattern)
                g.setColour(juce::Colour(0xfff5c542));
            else if (isSelectedPattern)
                g.setColour(juce::Colour(0xff5aa6b8));
            else
                g.setColour(juce::Colour(0xff26363b));

            g.fillRoundedRectangle(labelBox.toFloat(), 5.0f);

            if (isPendingPattern)
            {
                g.setColour(juce::Colour(0xffff9f43));
                g.drawRoundedRectangle(labelBox.toFloat().reduced(1.0f), 5.0f, 2.0f);
            }
            else
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(labelBox.toFloat(), 5.0f, 1.0f);
            }

            g.setColour(isActivePattern ? juce::Colours::black : juce::Colours::white);
            g.setFont(15.0f);
            g.drawFittedText("P" + juce::String(patternIndex + 1),
                labelBox.reduced(3),
                juce::Justification::centred,
                1);

            for (int stepIndex = 0; stepIndex < gridStepCount; ++stepIndex)
            {
                auto stepBox = getPatternStepBox(patternIndex, stepIndex);

                const int loopLength = juce::jmin(audioProcessor.getPatternLoopLength(patternIndex), gridStepCount);
                const bool isInsideLoop = stepIndex < loopLength;

                const bool hasNote = audioProcessor.stepHasNote(patternIndex, stepIndex);
                const bool isCurrentStep = activePattern == patternIndex && currentStep == (stepIndex + 1);
                const bool isSelectedStep = selectedEditPattern == patternIndex && selectedStep == stepIndex;

                if (isCurrentStep)
                {
                    g.setColour(juce::Colour(0xfff5c542));
                }
                else if (hasNote)
                {
                    g.setColour(isInsideLoop
                        ? juce::Colour(0xff5aa6b8)
                        : juce::Colour(0xff35545c));
                }
                else
                {
                    g.setColour(isInsideLoop
                        ? juce::Colour(0xff26363b)
                        : juce::Colour(0xff1c292d));
                }

                g.fillRoundedRectangle(stepBox.toFloat(), 5.0f);

                if (isCurrentStep)
                    g.setColour(juce::Colours::black);
                else if (isInsideLoop)
                    g.setColour(juce::Colours::white);
                else
                    g.setColour(juce::Colour(0xff607278));

                g.drawRoundedRectangle(stepBox.toFloat(), 5.0f, 1.0f);

                if (isSelectedStep)
                {
                    g.setColour(juce::Colours::white);
                    g.drawRoundedRectangle(stepBox.toFloat().reduced(1.5f), 5.0f, 3.0f);
                }

                if (isCurrentStep)
                    g.setColour(juce::Colours::black);
                else if (isInsideLoop)
                    g.setColour(juce::Colours::white);
                else
                    g.setColour(juce::Colour(0xff8fa4aa));

                g.setFont(12.0f);

                juce::String stepText;
                stepText << (stepIndex + 1);

                if (hasNote)
                {
                    const int note = audioProcessor.getStepNote(patternIndex, stepIndex);
                    const int velocity = juce::jlimit(1, 127, audioProcessor.getStepVelocity(patternIndex, stepIndex));
                    const int duration = audioProcessor.getStepDurationSteps(patternIndex, stepIndex);

                    stepText << "\n" << note;
                    stepText << "\n" << "v" << velocity << " d" << duration;
                }
                else
                {
                    stepText << "\n-";
                }

                g.drawFittedText(stepText,
                    stepBox.reduced(2),
                    juce::Justification::centred,
                    3);
            }

            const int transformX = matrixArea.getRight() - transformWidth;

            auto transformBox = juce::Rectangle<int>(transformX,
                rowY,
                transformWidth,
                rowHeight);

            const int rowTranspose = audioProcessor.getPatternTranspose(patternIndex);
            const int rowRotation = audioProcessor.getPatternRotation(patternIndex);
            const int rowLength = juce::jmin(audioProcessor.getPatternLoopLength(patternIndex), gridStepCount);
            const bool rowInverted = audioProcessor.getPatternInverted(patternIndex);
            const bool rowRetrograde = audioProcessor.getPatternRetrograde(patternIndex);
            const bool rowM7 = audioProcessor.getPatternM7(patternIndex);

            juce::String transformText;
            transformText << "Tr " << transposeTextFromValue(rowTranspose)
                << " Rot " << rotationTextFromValue(rowRotation)
                << " Len " << rowLength
                << "\nInv " << inversionTextFromValue(rowInverted)
                << " Retro " << retrogradeTextFromValue(rowRetrograde)
                << "\nM7 " << m7TextFromValue(rowM7);

            juce::ignoreUnused(transformGap);

            g.setColour(juce::Colour(0xffcfe8ef));
            g.setFont(13.0f);
            g.drawFittedText(transformText,
                transformBox,
                juce::Justification::centredLeft,
                3);
        }

    }

    bounds.removeFromTop(editGridHeight);
    bounds.removeFromTop(4);

    //==========================================================================
    // Selected step editor display

    g.setColour(juce::Colour(0xff1f2d32));
    if (!audioProcessor.isMelodyPaintMode())
    {
        auto dictionaryArea = getPatternMatrixArea();

        const int rowHeight = 64;
        const int rowGap = 8;

        const int dictionaryTop = dictionaryArea.getY() + (3 * rowHeight) + (2 * rowGap) + 18;
        const int availableHeight = dictionaryArea.getBottom() - dictionaryTop - 6;
        const int dictionaryHeight = juce::jlimit(72, 108, availableHeight);

        auto dictionaryBox = juce::Rectangle<int>(
            dictionaryArea.getX(),
            dictionaryTop,
            dictionaryArea.getWidth(),
            dictionaryHeight).reduced(2, 0);

        if (dictionaryBox.getHeight() > 64)
        {
            g.setColour(juce::Colour(0xff1c292d).withAlpha(0.92f));
            g.fillRoundedRectangle(dictionaryBox.toFloat(), 8.0f);

            g.setColour(juce::Colour(0xff3b5056));
            g.drawRoundedRectangle(dictionaryBox.toFloat(), 8.0f, 1.0f);

            auto textBox = dictionaryBox.reduced(14, 8);

            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xffcfe8ef));
            g.drawFittedText("Gesture Dictionary",
                textBox.removeFromTop(18),
                juce::Justification::centredLeft,
                1);

            g.setFont(12.0f);
            g.setColour(juce::Colour(0xffedf6f8));

            g.drawFittedText(
                "Matrix: Left paint+pitch | Right erase | Alt pitch existing | Ctrl velocity | Shift move",
                textBox.removeFromTop(17),
                juce::Justification::centredLeft,
                1);


            g.setColour(juce::Colour(0xffb8cbd0));

            g.drawFittedText(
                "Melody: Left create/length | Right erase | Shift move | Alt pitch | Alt+Shift pitch+length | Ctrl velocity",
                textBox.removeFromTop(17),
                juce::Justification::centredLeft,
                1);

            textBox.removeFromTop(4);

            g.setColour(juce::Colour(0xffcfe8ef));
            g.setFont(12.0f);
            g.drawFittedText(
                midiDebugLabel.getText() + "    |    External Control: "
                    + juce::String(externalControlToggle.getToggleState() ? "On" : "Off"),
                textBox.removeFromTop(18),
                juce::Justification::centredLeft,
                1);

        }
    }

    auto editorBox = bounds.removeFromTop(42);
    g.setColour(juce::Colour(0xff1c292d));
    g.fillRoundedRectangle(editorBox.toFloat(), 8.0f);

    const bool selectedHasNote = audioProcessor.stepHasNote(displayedPattern, selectedStep);
    const int selectedNote = audioProcessor.getStepNote(displayedPattern, selectedStep);
    const int selectedTransformedNote = audioProcessor.getTransformedStepNote(displayedPattern, selectedStep);
    const int selectedVelocity = audioProcessor.getStepVelocity(displayedPattern, selectedStep);
    const int selectedDuration = audioProcessor.getStepDurationSteps(displayedPattern, selectedStep);

    g.setColour(juce::Colours::white);
    g.setFont(15.0f);

    juce::String editorText;
    editorText << "Selected: P" << (displayedPattern + 1)
        << " Step " << (selectedStep + 1) << "    ";
    editorText << "Tr: " << transposeTextFromValue(patternTranspose) << "    ";
    editorText << "Rot: " << rotationTextFromValue(patternRotation) << "    ";
    editorText << "Inv: " << inversionTextFromValue(patternInverted) << "    ";
    editorText << "Retro: " << retrogradeTextFromValue(patternRetrograde) << "    ";
    editorText << "M7: " << m7TextFromValue(patternM7) << "    ";

    if (selectedHasNote)
    {
        editorText << "Note: " << selectedNote;

        if (patternTranspose != 0 || patternInverted || patternM7)
            editorText << " -> " << selectedTransformedNote;

        editorText << "    ";
        editorText << "Velocity: " << selectedVelocity << "    ";
        editorText << "Duration: d" << selectedDuration;
    }
    else
    {
        editorText << "Rest step";
    }

    g.drawFittedText(editorText,
        editorBox.reduced(16),
        juce::Justification::centredLeft,
        2);

    inversionToggleButton.setButtonText(patternInverted ? "Inv On" : "Inv Off");
    retrogradeToggleButton.setButtonText(patternRetrograde ? "Retro On" : "Retro Off");
    m7ToggleButton.setButtonText(patternM7 ? "M7 On" : "M7 Off");

    // Bottom controls are real buttons laid out in resized().
}

void MidiPatternLauncherAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);

    bounds.removeFromTop(26);   // title/version row
    bounds.removeFromTop(18);   // compact status row
    bounds.removeFromTop(4);
    bounds.removeFromTop(22);   // matrix title
    bounds.removeFromTop(4);
    const int editGridHeight = 330;
    bounds.removeFromTop(editGridHeight);  // matrix/melody area
    bounds.removeFromTop(4);
    bounds.removeFromTop(42);   // selected step display
    bounds.removeFromTop(16);

    //==========================================================================
    // Step edit buttons

    auto buttonRow = bounds.removeFromTop(36);

    const int gap = 8;
    const int buttonWidth = (buttonRow.getWidth() - (7 * gap)) / 8;

    noteDownButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    noteUpButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    velocityDownButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    velocityUpButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    durationDownButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    durationUpButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    clearButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(gap);

    makeNoteButton.setBounds(buttonRow.removeFromLeft(buttonWidth));

    bounds.removeFromTop(10);

    //==========================================================================
    // Pattern selection/tools row

    auto patternToolsRow = bounds.removeFromTop(36);

    const int toolsGap = 8;
    const int toolCount = 7;
    const int toolsButtonWidth = (patternToolsRow.getWidth() - ((toolCount - 1) * toolsGap)) / toolCount;

    editPattern1Button.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));
    patternToolsRow.removeFromLeft(toolsGap);

    editPattern2Button.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));
    patternToolsRow.removeFromLeft(toolsGap);

    editPattern3Button.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));
    patternToolsRow.removeFromLeft(toolsGap);

    copyToP1Button.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));
    patternToolsRow.removeFromLeft(toolsGap);

    copyToP2Button.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));
    patternToolsRow.removeFromLeft(toolsGap);

    copyToP3Button.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));
    patternToolsRow.removeFromLeft(toolsGap);

    clearPatternButton.setBounds(patternToolsRow.removeFromLeft(toolsButtonWidth));

    bounds.removeFromTop(10);

    //==========================================================================
    // Transform row

    auto transformRow = bounds.removeFromTop(36);

    const int transformGap = 6;
    const int transformButtonWidth = 66;
    const int transformSectionGap = 14;

    transposeDownOctaveButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    transposeDownButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    transposeResetButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    transposeUpButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    transposeUpOctaveButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));

    transformRow.removeFromLeft(transformSectionGap);

    rotationDownButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    rotationResetButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    rotationUpButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));

    transformRow.removeFromLeft(transformSectionGap);

    inversionToggleButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    retrogradeToggleButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));
    transformRow.removeFromLeft(transformGap);

    m7ToggleButton.setBounds(transformRow.removeFromLeft(transformButtonWidth));

    bounds.removeFromTop(4);
    //==========================================================================
    // Host target parameters

    auto targetTitleRow = bounds.removeFromTop(22);
    targetTitleLabel.setBounds(targetTitleRow);

    bounds.removeFromTop(6);

    auto targetRow1 = bounds.removeFromTop(26);

    const int targetGap = 8;
    const int targetLabelWidth = 52;
    const int targetControlWidth = 150;

    targetPatternLabel.setBounds(targetRow1.removeFromLeft(targetLabelWidth));
    targetPatternBox.setBounds(targetRow1.removeFromLeft(targetControlWidth));
    targetRow1.removeFromLeft(targetGap);

    gridModeLabel.setBounds(targetRow1.removeFromLeft(targetLabelWidth));
    gridModeBox.setBounds(targetRow1.removeFromLeft(targetControlWidth));
    targetRow1.removeFromLeft(targetGap);

    editorViewModeLabel.setBounds(targetRow1.removeFromLeft(targetLabelWidth));
    editorViewModeBox.setBounds(targetRow1.removeFromLeft(targetControlWidth));
    targetRow1.removeFromLeft(targetGap);

    targetStepLabel.setBounds(targetRow1.removeFromLeft(targetLabelWidth));
    targetStepSlider.setBounds(targetRow1.removeFromLeft(targetControlWidth));

    bounds.removeFromTop(6);

    auto targetRow2 = bounds.removeFromTop(26);

    targetNoteLabel.setBounds(targetRow2.removeFromLeft(targetLabelWidth));
    targetNoteSlider.setBounds(targetRow2.removeFromLeft(targetControlWidth));
    targetRow2.removeFromLeft(targetGap);

    targetVelocityLabel.setBounds(targetRow2.removeFromLeft(targetLabelWidth));
    targetVelocitySlider.setBounds(targetRow2.removeFromLeft(targetControlWidth));
    targetRow2.removeFromLeft(targetGap);

    targetDurationLabel.setBounds(targetRow2.removeFromLeft(targetLabelWidth));
    targetDurationSlider.setBounds(targetRow2.removeFromLeft(targetControlWidth));

    bounds.removeFromTop(6);

    auto targetRow3 = bounds.removeFromTop(26);

    globalSwingLabel.setBounds(targetRow3.removeFromLeft(targetLabelWidth));
    globalSwingBox.setBounds(targetRow3.removeFromLeft(targetControlWidth));
    targetRow3.removeFromLeft(targetGap);

    // v1.29.0: Rate sits next to Swing - both are "groove" controls that shape
    // WHEN steps land rather than WHICH note plays.
    globalRateLabel.setBounds(targetRow3.removeFromLeft(targetLabelWidth));
    globalRateBox.setBounds(targetRow3.removeFromLeft(targetControlWidth));
    targetRow3.removeFromLeft(targetGap);

    externalControlLabel.setBounds(targetRow3.removeFromLeft(64));
    externalControlEnabledButton.setBounds(targetRow3.removeFromLeft(90));
    targetRow3.removeFromLeft(targetGap);

    externalControlChannelLabel.setBounds(targetRow3.removeFromLeft(64));
    externalControlChannelBox.setBounds(targetRow3.removeFromLeft(90));
}


