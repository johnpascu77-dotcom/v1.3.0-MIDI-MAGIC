#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ComposerBridgeIpcClient.h"

#include <cmath>
#include <algorithm>

//==============================================================================
MidiPatternLauncherAudioProcessor::MidiPatternLauncherAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#else
    : apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    initialisePatterns();

#if JUCE_DEBUG
    runComposerBridgeProtocolSelfTest();
#endif

    composerBridgeIpcClient = std::make_unique<ComposerBridgeIpcClient>(*this);
}

MidiPatternLauncherAudioProcessor::~MidiPatternLauncherAudioProcessor()
{
    // Explicit reset (not just relying on unique_ptr's default destruction
    // order) so the connection is torn down before anything it might still
    // call back into is gone.
    composerBridgeIpcClient.reset();
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiPatternLauncherAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("activePatternParam", 1),
        "Active Pattern",
        juce::StringArray{ "Stopped", "Pattern 1", "Pattern 2", "Pattern 3" },
        0));

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("targetPatternParam", 1),
        "Target Pattern",
        juce::StringArray{ "Pattern 1", "Pattern 2", "Pattern 3" },
        0));
    
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("gridModeParam", 1),
        "Grid Mode",
        juce::StringArray{ "Binary 16", "Ternary 12" },
        0));
    
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("editorViewModeParam", 1),
        "Editor View Mode",
        juce::StringArray{ "Matrix", "Melody" },
        0));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("externalControlEnabledParam", 1),
        "External Control Enabled",
        false));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("composerBridgeEnabledParam", 1),
        "Composer Bridge Enabled",
        true));

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("externalControlChannelParam", 1),
        "External Control Channel",
        juce::StringArray{
            "All",
            "1", "2", "3", "4",
            "5", "6", "7", "8",
            "9", "10", "11", "12",
            "13", "14", "15", "16"
        },
        0));

    // v1.28.0: Swing narrowed from a continuous slider to a 3-state choice
    // (Off/Triplet/Shuffle) - a continuous value almost always landed
    // somewhere between the range's only clean ratios, musically ambiguous
    // rather than a genuine third feel. Shuffle's own percent
    // (kShuffleSwingPercent) is 100%, not the range's old 75% historical
    // maximum - see that constant's own comment for why. See
    // getGlobalSwingAmount().
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("globalSwingParam", 1),
        "Swing",
        juce::StringArray{ "Off", "Triplet", "Shuffle" },
        0));

    // v1.29.0: Rate - a real classical augmentation/diminution device (the
    // whole active pattern restated in longer or shorter note values,
    // onsets AND durations both scaling together, not just a faster/slower
    // scrub) - global per-instance rather than per-pattern, same footprint
    // as Swing above and for the same reasons: "voice" in the conductor
    // plugin's own vocabulary already means "one MPL instance," and every CC
    // slot in the per-pattern 30-35/40-45/50-55 blocks is already spoken for
    // (see handleExternalControlCC). Kept to 3 states on purpose, the same
    // restraint Swing itself went through - a continuous rate knob would
    // read as sloppy next to three clean, musically-legible choices. See
    // getGlobalRateMultiplier().
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("globalRateParam", 1),
        "Rate",
        juce::StringArray{ "Augmented", "Normal", "Diminished" },
        1));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("targetStepParam", 1),
        "Target Step",
        1,
        patternLength,
        1));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("targetNoteParam", 1),
        "Target Note",
        0,
        127,
        60));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("targetVelocityParam", 1),
        "Target Velocity",
        1,
        127,
        100));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("targetDurationParam", 1),
        "Target Duration",
        1,
        patternLength,
        1));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("targetEnabledParam", 1),
        "Target Enabled",
        false));

    for (int patternIndex = 0; patternIndex < numPatterns; ++patternIndex)
    {
        const int patternNumber = patternIndex + 1;

        parameters.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID("p" + juce::String(patternNumber) + "TransposeParam", 1),
            "P" + juce::String(patternNumber) + " Transpose",
            -48,
            48,
            0));

        parameters.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID("p" + juce::String(patternNumber) + "RotationParam", 1),
            "P" + juce::String(patternNumber) + " Rotation",
            0,
            patternLength - 1,
            0));

        parameters.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID("p" + juce::String(patternNumber) + "LengthParam", 1),
            "P" + juce::String(patternNumber) + " Length",
            minPatternLoopLength,
            patternLength,
            defaultPatternLoopLength));

        parameters.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("p" + juce::String(patternNumber) + "InversionParam", 1),
            "P" + juce::String(patternNumber) + " Inversion",
            false));

        parameters.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("p" + juce::String(patternNumber) + "RetrogradeParam", 1),
            "P" + juce::String(patternNumber) + " Retrograde",
            false));

        parameters.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("p" + juce::String(patternNumber) + "M7Param", 1),
            "P" + juce::String(patternNumber) + " M7",
            false));
    }

    return { parameters.begin(), parameters.end() };
}

juce::AudioProcessorValueTreeState& MidiPatternLauncherAudioProcessor::getAPVTS()
{
    return apvts;
}

int MidiPatternLauncherAudioProcessor::getGridModeIndex() const
{
    return juce::jlimit(0, 1, getChoiceParameterIndex("gridModeParam"));
}

int MidiPatternLauncherAudioProcessor::getGridStepCount() const
{
    return isTernaryGridMode() ? 12 : patternLength;
}

bool MidiPatternLauncherAudioProcessor::isTernaryGridMode() const
{
    return getGridModeIndex() == 1;
}

int MidiPatternLauncherAudioProcessor::getGlobalSwingIndex() const
{
    return juce::jlimit(0, 2, getChoiceParameterIndex("globalSwingParam"));
}

float MidiPatternLauncherAudioProcessor::getGlobalSwingAmount() const
{
    switch (getGlobalSwingIndex())
    {
        case 1:  return kTripletSwingPercent;
        case 2:  return kShuffleSwingPercent;
        default: return 0.0f;
    }
}

int MidiPatternLauncherAudioProcessor::getGlobalRateIndex() const
{
    return juce::jlimit(0, 2, getChoiceParameterIndex("globalRateParam"));
}

float MidiPatternLauncherAudioProcessor::getGlobalRateMultiplier() const
{
    switch (getGlobalRateIndex())
    {
        case 0:  return 0.5f;  // Augmented - restated in longer values, slower
        case 2:  return 2.0f;  // Diminished - restated in shorter values, faster
        default: return 1.0f;  // Normal
    }
}

bool MidiPatternLauncherAudioProcessor::isExternalControlEnabled() const
{
    return getBoolParameterValue("externalControlEnabledParam");
}

bool MidiPatternLauncherAudioProcessor::isComposerBridgeEnabled() const
{
    return getBoolParameterValue("composerBridgeEnabledParam");
}

int MidiPatternLauncherAudioProcessor::getExternalControlChannel() const
{
    // 0 = All, 1..16 = specific MIDI channel.
    return juce::jlimit(0, 16, getChoiceParameterIndex("externalControlChannelParam"));
}

int MidiPatternLauncherAudioProcessor::getEditorViewModeIndex() const
{
    return juce::jlimit(0, 1, getChoiceParameterIndex("editorViewModeParam"));
}

int MidiPatternLauncherAudioProcessor::getDebugLastCCNumber() const
{
    return debugLastCCNumber.load();
}

int MidiPatternLauncherAudioProcessor::getDebugLastCCChannel() const
{
    return debugLastCCChannel.load();
}

int MidiPatternLauncherAudioProcessor::getDebugLastCCValue() const
{
    return debugLastCCValue.load();
}

bool MidiPatternLauncherAudioProcessor::getDebugLastCCAccepted() const
{
    return debugLastCCAccepted.load();
}

bool MidiPatternLauncherAudioProcessor::isMelodyPaintMode() const
{
    return getEditorViewModeIndex() == 1;
}

void MidiPatternLauncherAudioProcessor::updateAutomationParametersForPattern(int patternIndex)
{
    syncParametersFromPattern(patternIndex);
}

void MidiPatternLauncherAudioProcessor::setParameterPlainValueNotifyingHost(const juce::String& parameterID,
    float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterID))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

int MidiPatternLauncherAudioProcessor::getTargetPatternIndex() const
{
    return juce::jlimit(0, numPatterns - 1, getChoiceParameterIndex("targetPatternParam"));
}

int MidiPatternLauncherAudioProcessor::getTargetStepIndex() const
{
    return juce::jlimit(0, patternLength - 1, getIntParameterValue("targetStepParam") - 1);
}

int MidiPatternLauncherAudioProcessor::getTargetNote() const
{
    return juce::jlimit(0, 127, getIntParameterValue("targetNoteParam"));
}

int MidiPatternLauncherAudioProcessor::getTargetVelocity() const
{
    return juce::jlimit(1, 127, getIntParameterValue("targetVelocityParam"));
}

int MidiPatternLauncherAudioProcessor::getTargetDurationSteps() const
{
    return juce::jlimit(1, patternLength, getIntParameterValue("targetDurationParam"));
}

bool MidiPatternLauncherAudioProcessor::getTargetEnabled() const
{
    return getBoolParameterValue("targetEnabledParam");
}

void MidiPatternLauncherAudioProcessor::setTargetPatternAndStep(int patternIndex, int stepIndex)
{
    patternIndex = juce::jlimit(0, numPatterns - 1, patternIndex);
    stepIndex = juce::jlimit(0, patternLength - 1, stepIndex);

    suppressParameterSync.store(true);

    setParameterPlainValueNotifyingHost("targetPatternParam", static_cast<float>(patternIndex));
    setParameterPlainValueNotifyingHost("targetStepParam", static_cast<float>(stepIndex + 1));

    suppressParameterSync.store(false);

    lastTargetPatternParam = patternIndex;
    lastTargetStepParam = stepIndex + 1;

    updateTargetParametersFromStep();
}

void MidiPatternLauncherAudioProcessor::updateTargetParametersFromStep()
{
    syncTargetParametersFromStep();
}

void MidiPatternLauncherAudioProcessor::applyTargetParametersToStep()
{
    syncTargetStepFromParameters();
}

void MidiPatternLauncherAudioProcessor::syncTargetParametersFromStep()
{
    const int patternIndex = getTargetPatternIndex();
    const int stepIndex = getTargetStepIndex();

    const Step step = getStepForPattern(patternIndex, stepIndex);

    const bool enabled = step.note >= 0 && step.velocity > 0 && step.durationSteps > 0;
    const int note = enabled ? juce::jlimit(0, 127, step.note) : lastTargetNoteParam;
    const int velocity = enabled ? juce::jlimit(1, 127, step.velocity) : lastTargetVelocityParam;
    const int duration = enabled ? juce::jlimit(1, patternLength, step.durationSteps) : lastTargetDurationParam;

    suppressParameterSync.store(true);

    setParameterPlainValueNotifyingHost("targetNoteParam", static_cast<float>(note));
    setParameterPlainValueNotifyingHost("targetVelocityParam", static_cast<float>(velocity));
    setParameterPlainValueNotifyingHost("targetDurationParam", static_cast<float>(duration));
    setParameterPlainValueNotifyingHost("targetEnabledParam", enabled ? 1.0f : 0.0f);

    suppressParameterSync.store(false);

    lastTargetPatternParam = patternIndex;
    lastTargetStepParam = stepIndex + 1;
    lastTargetNoteParam = note;
    lastTargetVelocityParam = velocity;
    lastTargetDurationParam = duration;
    lastTargetEnabledParam = enabled;
}

void MidiPatternLauncherAudioProcessor::syncTargetStepFromParameters()
{
    const int patternIndex = getTargetPatternIndex();
    const int stepIndex = getTargetStepIndex();

    const int note = getTargetNote();
    const int velocity = getTargetVelocity();
    const int duration = getTargetDurationSteps();
    const bool enabled = getTargetEnabled();

    if (enabled)
        setStepValues(patternIndex, stepIndex, note, velocity, duration);
    else
        setStepValues(patternIndex, stepIndex, -1, 0, 0);

    lastTargetPatternParam = patternIndex;
    lastTargetStepParam = stepIndex + 1;
    lastTargetNoteParam = note;
    lastTargetVelocityParam = velocity;
    lastTargetDurationParam = duration;
    lastTargetEnabledParam = enabled;
}

void MidiPatternLauncherAudioProcessor::syncParametersFromPattern(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    const int transpose = getPatternTranspose(patternIndex);
    const int rotation = getPatternRotation(patternIndex);
    const int length = getPatternLoopLength(patternIndex);
    const bool inversion = getPatternInverted(patternIndex);
    const bool retrograde = getPatternRetrograde(patternIndex);
    const bool m7 = getPatternM7(patternIndex);

    suppressParameterSync.store(true);

    setParameterPlainValueNotifyingHost(getTransposeParameterID(patternIndex),
        static_cast<float>(transpose));

    setParameterPlainValueNotifyingHost(getRotationParameterID(patternIndex),
        static_cast<float>(rotation));

    setParameterPlainValueNotifyingHost(getLengthParameterID(patternIndex),
        static_cast<float>(length));

    setParameterPlainValueNotifyingHost(getInversionParameterID(patternIndex),
        inversion ? 1.0f : 0.0f);

    setParameterPlainValueNotifyingHost(getRetrogradeParameterID(patternIndex),
        retrograde ? 1.0f : 0.0f);

    setParameterPlainValueNotifyingHost(getM7ParameterID(patternIndex),
        m7 ? 1.0f : 0.0f);

    suppressParameterSync.store(false);
}

int MidiPatternLauncherAudioProcessor::getChoiceParameterIndex(const juce::String& parameterID) const
{
    if (auto* parameter = apvts.getParameter(parameterID))
    {
        if (auto* choiceParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter))
            return choiceParameter->getIndex();

        return juce::roundToInt(parameter->convertFrom0to1(parameter->getValue()));
    }

    return 0;
}

int MidiPatternLauncherAudioProcessor::getIntParameterValue(const juce::String& parameterID) const
{
    if (auto* parameter = apvts.getParameter(parameterID))
    {
        if (auto* intParameter = dynamic_cast<juce::AudioParameterInt*>(parameter))
            return intParameter->get();

        return juce::roundToInt(parameter->convertFrom0to1(parameter->getValue()));
    }

    return 0;
}

float MidiPatternLauncherAudioProcessor::getFloatParameterValue(const juce::String& parameterID) const
{
    if (auto* parameter = apvts.getRawParameterValue(parameterID))
        return parameter->load();

    return 0.0f;
}

bool MidiPatternLauncherAudioProcessor::getBoolParameterValue(const juce::String& parameterID) const
{
    if (auto* parameter = apvts.getParameter(parameterID))
    {
        if (auto* boolParameter = dynamic_cast<juce::AudioParameterBool*>(parameter))
            return boolParameter->get();

        return parameter->getValue() >= 0.5f;
    }

    return false;
}

void MidiPatternLauncherAudioProcessor::syncEngineFromParameters()
{
    if (suppressParameterSync.load())
        return;

    const int activeParam = juce::jlimit(0,
        numPatterns,
        getChoiceParameterIndex("activePatternParam"));

    if (activeParam != lastActivePatternParam)
    {
        const int requestedPattern = activeParam - 1; // 0 = stopped, 1..3 = patterns 0..2

        if (requestedPattern != activePattern)
        {
            pendingPattern = requestedPattern;
            displayPendingPattern.store(pendingPattern);
        }

        lastActivePatternParam = activeParam;
    }

    for (int patternIndex = 0; patternIndex < numPatterns; ++patternIndex)
    {
        const int transposeParam = juce::jlimit(-48,
            48,
            getIntParameterValue(getTransposeParameterID(patternIndex)));

        const int rotationParam = juce::jlimit(0,
            patternLength - 1,
            getIntParameterValue(getRotationParameterID(patternIndex)));

        const int lengthParam = juce::jlimit(minPatternLoopLength,
            patternLength,
            getIntParameterValue(getLengthParameterID(patternIndex)));

        const bool inversionParam = getBoolParameterValue(getInversionParameterID(patternIndex));
        const bool retrogradeParam = getBoolParameterValue(getRetrogradeParameterID(patternIndex));
        const bool m7Param = getBoolParameterValue(getM7ParameterID(patternIndex));

        patternTranspose[static_cast<size_t>(patternIndex)] = transposeParam;
        patternRotation[static_cast<size_t>(patternIndex)] = rotationParam;
        patternLoopLength[static_cast<size_t>(patternIndex)] = lengthParam;
        patternInverted[static_cast<size_t>(patternIndex)] = inversionParam;
        patternRetrograde[static_cast<size_t>(patternIndex)] = retrogradeParam;
        patternM7[static_cast<size_t>(patternIndex)] = m7Param;
    }

    const int targetPatternParam = juce::jlimit(0,
        numPatterns - 1,
        getChoiceParameterIndex("targetPatternParam"));

    const int targetStepParam = juce::jlimit(1,
        patternLength,
        getIntParameterValue("targetStepParam"));

    const int targetNoteParam = juce::jlimit(0,
        127,
        getIntParameterValue("targetNoteParam"));

    const int targetVelocityParam = juce::jlimit(1,
        127,
        getIntParameterValue("targetVelocityParam"));

    const int targetDurationParam = juce::jlimit(1,
        patternLength,
        getIntParameterValue("targetDurationParam"));

    const bool targetEnabledParam = getBoolParameterValue("targetEnabledParam");

    const bool targetSelectionChanged =
        targetPatternParam != lastTargetPatternParam
        || targetStepParam != lastTargetStepParam;

    const bool targetValuesChanged =
        targetNoteParam != lastTargetNoteParam
        || targetVelocityParam != lastTargetVelocityParam
        || targetDurationParam != lastTargetDurationParam
        || targetEnabledParam != lastTargetEnabledParam;

    // Commit takes priority over browse when both change in the same poll.
    // A human using the plugin's own UI never hits this ambiguity - clicking
    // a step calls setTargetPatternAndStep() directly (guarded by
    // suppressParameterSync, bypassing this poll entirely), so pure
    // navigation only ever shows up here as a selection change with no
    // accompanying value change. An external CC burst (Composer
    // Mastermind's Target Step protocol, CC 21/60-64) is different: it
    // always sends a new step selection *and* new note/velocity/duration/
    // enabled together as one atomic edit, and by the time this poll next
    // runs both flags are true simultaneously. Checking selection first
    // used to mean browse always won that race, silently reloading the new
    // step's pre-existing content over the incoming values - every
    // programmatic edit that also changed step selection (i.e. nearly all
    // of them) was discarded without any error, confirmed via Composer
    // Mastermind's motif engine writing hundreds of steps at bar 44 with
    // subsequent zero audible change. Commit-first fixes this: "select this
    // new step and set it to these values" is treated as one edit, not
    // browse-then-lose-the-values.
    if (targetValuesChanged)
    {
        syncTargetStepFromParameters();
    }
    else if (targetSelectionChanged)
    {
        syncTargetParametersFromStep();
    }
}

//==============================================================================
void MidiPatternLauncherAudioProcessor::initialisePatterns()
{
    for (auto& pattern : patterns)
    {
        for (auto& step : pattern)
            step = Step{};
    }

    // Pattern 1:
    // C eighth, E eighth, G quarter, C quarter, G eighth, E eighth
    patterns[0] =
    {
        Step { 60, 110, 2 },
        Step { -1,   0, 0 },
        Step { 64,  90, 2 },
        Step { -1,   0, 0 },
        Step { 67, 100, 4 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { 72, 105, 4 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { 67,  85, 2 },
        Step { -1,   0, 0 },
        Step { 64,  80, 2 },
        Step { -1,   0, 0 }
    };

    // Pattern 2:
    // Bass phrase.
    patterns[1] =
    {
        Step { 48, 120, 4 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { 55,  90, 2 },
        Step { -1,   0, 0 },
        Step { 57, 100, 2 },
        Step { -1,   0, 0 },
        Step { 55,  95, 4 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { 48, 110, 4 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 }
    };

    // Pattern 3:
    // Simple melodic phrase.
    patterns[2] =
    {
        Step { 60, 105, 2 },
        Step { -1,   0, 0 },
        Step { 62,  80, 2 },
        Step { -1,   0, 0 },
        Step { 64,  95, 4 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { -1,   0, 0 },
        Step { 67, 115, 2 },
        Step { -1,   0, 0 },
        Step { 64,  85, 2 },
        Step { -1,   0, 0 },
        Step { 62,  75, 2 },
        Step { -1,   0, 0 },
        Step { 60, 100, 2 },
        Step { -1,   0, 0 }
    };

    patternTranspose = { 0, 0, 0 };
    patternRotation = { 0, 0, 0 };
    patternLoopLength = {
        defaultPatternLoopLength,
        defaultPatternLoopLength,
        defaultPatternLoopLength
    };
    patternInverted = { false, false, false };
}

MidiPatternLauncherAudioProcessor::Step MidiPatternLauncherAudioProcessor::getStepForPattern(int patternIndex,
    int stepIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return Step{};

    stepIndex = ((stepIndex % patternLength) + patternLength) % patternLength;

    return patterns[static_cast<size_t>(patternIndex)]
        [static_cast<size_t>(stepIndex)];
}

//==============================================================================
int MidiPatternLauncherAudioProcessor::getDisplayActivePattern() const
{
    return displayActivePattern.load();
}

int MidiPatternLauncherAudioProcessor::getDisplayPendingPattern() const
{
    return displayPendingPattern.load();
}

int MidiPatternLauncherAudioProcessor::getDisplayCurrentStep() const
{
    return displayCurrentStep.load();
}

int MidiPatternLauncherAudioProcessor::getDisplayCurrentBar() const
{
    return displayCurrentBar.load();
}

bool MidiPatternLauncherAudioProcessor::stepHasNote(int patternIndex, int stepIndex) const
{
    const Step step = getStepForPattern(patternIndex, stepIndex);

    return step.note >= 0
        && step.velocity > 0
        && step.durationSteps > 0;
}

int MidiPatternLauncherAudioProcessor::getStepNote(int patternIndex, int stepIndex) const
{
    return getStepForPattern(patternIndex, stepIndex).note;
}

int MidiPatternLauncherAudioProcessor::getStepVelocity(int patternIndex, int stepIndex) const
{
    return getStepForPattern(patternIndex, stepIndex).velocity;
}

int MidiPatternLauncherAudioProcessor::getStepDurationSteps(int patternIndex, int stepIndex) const
{
    return getStepForPattern(patternIndex, stepIndex).durationSteps;
}

void MidiPatternLauncherAudioProcessor::setStepValues(int patternIndex,
    int stepIndex,
    int note,
    int velocity,
    int durationSteps)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    if (stepIndex < 0 || stepIndex >= patternLength)
        return;

    auto& step = patterns[static_cast<size_t>(patternIndex)]
        [static_cast<size_t>(stepIndex)];

    if (note < 0 || velocity <= 0 || durationSteps <= 0)
    {
        step.note = -1;
        step.velocity = 0;
        step.durationSteps = 0;

        if (patternIndex == getTargetPatternIndex() && stepIndex == getTargetStepIndex())
            updateTargetParametersFromStep();

        return;
    }

    step.note = juce::jlimit(0, 127, note);
    step.velocity = juce::jlimit(1, 127, velocity);
    step.durationSteps = juce::jlimit(1, patternLength, durationSteps);

    if (patternIndex == getTargetPatternIndex() && stepIndex == getTargetStepIndex())
        updateTargetParametersFromStep();
}

void MidiPatternLauncherAudioProcessor::clearStep(int patternIndex, int stepIndex)
{
    setStepValues(patternIndex, stepIndex, -1, 0, 0);
}

void MidiPatternLauncherAudioProcessor::makeStepNote(int patternIndex, int stepIndex)
{
    if (stepHasNote(patternIndex, stepIndex))
        return;

    setStepValues(patternIndex, stepIndex, 60, 100, 1);
}

void MidiPatternLauncherAudioProcessor::changeStepNote(int patternIndex, int stepIndex, int delta)
{
    Step step = getStepForPattern(patternIndex, stepIndex);

    if (step.note < 0)
    {
        setStepValues(patternIndex, stepIndex, 60, 100, 1);
        return;
    }

    setStepValues(patternIndex,
        stepIndex,
        step.note + delta,
        step.velocity,
        step.durationSteps);
}

void MidiPatternLauncherAudioProcessor::changeStepVelocity(int patternIndex, int stepIndex, int delta)
{
    Step step = getStepForPattern(patternIndex, stepIndex);

    if (step.note < 0)
    {
        setStepValues(patternIndex, stepIndex, 60, 100, 1);
        return;
    }

    setStepValues(patternIndex,
        stepIndex,
        step.note,
        step.velocity + delta,
        step.durationSteps);
}

void MidiPatternLauncherAudioProcessor::changeStepDuration(int patternIndex, int stepIndex, int delta)
{
    Step step = getStepForPattern(patternIndex, stepIndex);

    if (step.note < 0)
    {
        setStepValues(patternIndex, stepIndex, 60, 100, 1);
        return;
    }

    setStepValues(patternIndex,
        stepIndex,
        step.note,
        step.velocity,
        step.durationSteps + delta);
}

void MidiPatternLauncherAudioProcessor::copyPattern(int sourcePatternIndex, int destinationPatternIndex)
{
    if (sourcePatternIndex < 0 || sourcePatternIndex >= numPatterns)
        return;

    if (destinationPatternIndex < 0 || destinationPatternIndex >= numPatterns)
        return;

    if (sourcePatternIndex == destinationPatternIndex)
        return;

    patterns[static_cast<size_t>(destinationPatternIndex)] =
        patterns[static_cast<size_t>(sourcePatternIndex)];

    patternTranspose[static_cast<size_t>(destinationPatternIndex)] =
        patternTranspose[static_cast<size_t>(sourcePatternIndex)];

    patternRotation[static_cast<size_t>(destinationPatternIndex)] =
        patternRotation[static_cast<size_t>(sourcePatternIndex)];

    patternLoopLength[static_cast<size_t>(destinationPatternIndex)] =
        patternLoopLength[static_cast<size_t>(sourcePatternIndex)];

    patternInverted[static_cast<size_t>(destinationPatternIndex)] =
        patternInverted[static_cast<size_t>(sourcePatternIndex)];

    patternRetrograde[static_cast<size_t>(destinationPatternIndex)] =
        patternRetrograde[static_cast<size_t>(sourcePatternIndex)];

    patternM7[static_cast<size_t>(destinationPatternIndex)] =
        patternM7[static_cast<size_t>(sourcePatternIndex)];

    updateAutomationParametersForPattern(destinationPatternIndex);
}

void MidiPatternLauncherAudioProcessor::clearPattern(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    auto& pattern = patterns[static_cast<size_t>(patternIndex)];

    for (auto& step : pattern)
        step = Step{};

    patternTranspose[static_cast<size_t>(patternIndex)] = 0;
    patternRotation[static_cast<size_t>(patternIndex)] = 0;
    patternLoopLength[static_cast<size_t>(patternIndex)] = defaultPatternLoopLength;
    patternInverted[static_cast<size_t>(patternIndex)] = false;
    patternRetrograde[static_cast<size_t>(patternIndex)] = false;
    patternM7[static_cast<size_t>(patternIndex)] = false;

    updateAutomationParametersForPattern(patternIndex);
}

//==============================================================================
// v1.3.0 transpose helpers.
// These are non-destructive: they do not change the stored step notes.

int MidiPatternLauncherAudioProcessor::getPatternTranspose(int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return 0;

    return patternTranspose[static_cast<size_t>(patternIndex)];
}

void MidiPatternLauncherAudioProcessor::changePatternTranspose(int patternIndex, int deltaSemitones)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    auto& transpose = patternTranspose[static_cast<size_t>(patternIndex)];
    transpose = juce::jlimit(-48, 48, transpose + deltaSemitones);
}

void MidiPatternLauncherAudioProcessor::resetPatternTranspose(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    patternTranspose[static_cast<size_t>(patternIndex)] = 0;
}

int MidiPatternLauncherAudioProcessor::getTransposedStepNote(int patternIndex, int stepIndex) const
{
    const Step step = getStepForPattern(patternIndex, stepIndex);

    if (step.note < 0)
        return -1;

    return juce::jlimit(0, 127, step.note + getPatternTranspose(patternIndex));
}
//==============================================================================
// v1.5.0 inversion helpers.
// These are non-destructive: stored step notes are never rewritten.
//
// Transform order (v1.27.0 adds M7 ahead of Inversion):
//   Stored Note -> M7 -> Inversion -> Transposition -> MIDI Output
//
// Inversion axis:
//   60 = middle C / C4.
//   Example:
//     64 becomes 56
//     67 becomes 53
//     60 remains 60

bool MidiPatternLauncherAudioProcessor::getPatternInverted(int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return false;

    return patternInverted[static_cast<size_t>(patternIndex)];
}

void MidiPatternLauncherAudioProcessor::togglePatternInverted(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    auto& inverted = patternInverted[static_cast<size_t>(patternIndex)];
    inverted = !inverted;
}

void MidiPatternLauncherAudioProcessor::setPatternInverted(int patternIndex, bool shouldBeInverted)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    patternInverted[static_cast<size_t>(patternIndex)] = shouldBeInverted;
}
//==============================================================================
// v1.27.0 M7 helpers.
// Non-destructive, like Inversion above: stored step notes are never
// rewritten. M7 is interval multiplication by 7 mod 12 - a classic
// twelve-tone/set-theory row operation distinct from Transpose/Inversion/
// Retrograde (each of those is additive or reflective; M7 is a genuine
// permutation of the twelve pitch classes). Only the pitch class (note % 12)
// is transformed; the note's octave register is left alone so the result
// stays in roughly the same range as the stored note.
//
//   pitchClass' = (pitchClass * 7) mod 12
//
//   Example (pitch class only):
//     0  (C)  -> 0  (C)
//     1  (C#) -> 7  (G)
//     2  (D)  -> 2  (D)
//     4  (E)  -> 4  (E)
//     7  (G)  -> 1  (C#)
//
// M7 is its own inverse (7 * 7 mod 12 == 1), so toggling it twice is a
// no-op - the same self-cancelling character as Inversion.

bool MidiPatternLauncherAudioProcessor::getPatternM7(int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return false;

    return patternM7[static_cast<size_t>(patternIndex)];
}

void MidiPatternLauncherAudioProcessor::togglePatternM7(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    auto& m7 = patternM7[static_cast<size_t>(patternIndex)];
    m7 = !m7;
}

void MidiPatternLauncherAudioProcessor::setPatternM7(int patternIndex, bool shouldBeM7)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    patternM7[static_cast<size_t>(patternIndex)] = shouldBeM7;
}

int MidiPatternLauncherAudioProcessor::applyPatternTransformsToNote(int patternIndex, int note) const
{
    if (note < 0)
        return -1;

    int transformedNote = note;

    if (getPatternM7(patternIndex))
    {
        const int pitchClass = transformedNote % 12;
        const int m7PitchClass = (pitchClass * 7) % 12;
        transformedNote = transformedNote - pitchClass + m7PitchClass;
    }

    if (getPatternInverted(patternIndex))
        transformedNote = (inversionAxisNote * 2) - transformedNote;

    transformedNote += getPatternTranspose(patternIndex);

    return juce::jlimit(0, 127, transformedNote);
}

int MidiPatternLauncherAudioProcessor::getInvertedStepNote(int patternIndex, int stepIndex) const
{
    const Step step = getStepForPattern(patternIndex, stepIndex);

    if (step.note < 0)
        return -1;

    if (!getPatternInverted(patternIndex))
        return step.note;

    return juce::jlimit(0, 127, (inversionAxisNote * 2) - step.note);
}

int MidiPatternLauncherAudioProcessor::getTransformedStepNote(int patternIndex, int stepIndex) const
{
    const Step step = getStepForPattern(patternIndex, stepIndex);

    if (step.note < 0)
        return -1;

    return applyPatternTransformsToNote(patternIndex, step.note);
}
//==============================================================================
// v1.26.0 retrograde helpers.
// Non-destructive, like Inversion above: the stored step order is never
// rewritten, only reflected at read time inside getRotatedSourceStepIndex().
// Combining Retrograde with Inversion (both independent toggles) produces the
// classic Retrograde-Inversion (RI) twelve-tone row form without needing a
// dedicated third toggle for it.

bool MidiPatternLauncherAudioProcessor::getPatternRetrograde(int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return false;

    return patternRetrograde[static_cast<size_t>(patternIndex)];
}

void MidiPatternLauncherAudioProcessor::togglePatternRetrograde(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    auto& retrograde = patternRetrograde[static_cast<size_t>(patternIndex)];
    retrograde = !retrograde;
}

void MidiPatternLauncherAudioProcessor::setPatternRetrograde(int patternIndex, bool shouldBeRetrograde)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    patternRetrograde[static_cast<size_t>(patternIndex)] = shouldBeRetrograde;
}
//==============================================================================
// v1.4.0 rotation helpers.
// These are non-destructive: they do not move or rewrite stored step data.
//
// Rotation meaning:
//   0  = no rotation
//   +1 = stored material sounds one step later
//   +2 = stored material sounds two steps later
//
// Therefore, when playback is currently at stepIndex, we read from:
//
//   sourceStepIndex = stepIndex - rotation
//
// with wraparound inside 0..15.
//
// v1.26.0: if Retrograde is on for this pattern, the loop-relative playback
// position is reflected around the loop's midpoint *before* rotation is
// subtracted, so the stored steps are read back to front. Rotation still
// shifts where in that (possibly reversed) traversal playback starts.

int MidiPatternLauncherAudioProcessor::getPatternRotation(int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return 0;

    return patternRotation[static_cast<size_t>(patternIndex)];
}

void MidiPatternLauncherAudioProcessor::changePatternRotation(int patternIndex, int deltaSteps)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    auto& rotation = patternRotation[static_cast<size_t>(patternIndex)];

    rotation += deltaSteps;
    rotation = ((rotation % patternLength) + patternLength) % patternLength;
}

void MidiPatternLauncherAudioProcessor::resetPatternRotation(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    patternRotation[static_cast<size_t>(patternIndex)] = 0;
}

int MidiPatternLauncherAudioProcessor::getRotatedSourceStepIndex(int patternIndex,
    int playbackStepIndex) const
{
    const int loopLength = juce::jmin(getPatternLoopLength(patternIndex), getGridStepCount());
    const int rotation = getPatternRotation(patternIndex);

    const int effectiveRotation = ((rotation % loopLength) + loopLength) % loopLength;

    const int reflectedStepIndex = getPatternRetrograde(patternIndex)
        ? (loopLength - 1 - playbackStepIndex)
        : playbackStepIndex;

    return ((reflectedStepIndex - effectiveRotation) % loopLength + loopLength) % loopLength;
}

int MidiPatternLauncherAudioProcessor::getPatternLoopLength(int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return defaultPatternLoopLength;

    return juce::jlimit(minPatternLoopLength,
        patternLength,
        patternLoopLength[static_cast<size_t>(patternIndex)]);
}

void MidiPatternLauncherAudioProcessor::setPatternLoopLength(int patternIndex, int length)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    patternLoopLength[static_cast<size_t>(patternIndex)] =
        juce::jlimit(minPatternLoopLength, patternLength, length);

    updateAutomationParametersForPattern(patternIndex);
}

void MidiPatternLauncherAudioProcessor::changePatternLoopLength(int patternIndex, int deltaSteps)
{
    if (patternIndex < 0 || patternIndex >= numPatterns)
        return;

    setPatternLoopLength(patternIndex,
        getPatternLoopLength(patternIndex) + deltaSteps);
}

//==============================================================================
const juce::String MidiPatternLauncherAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MidiPatternLauncherAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool MidiPatternLauncherAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool MidiPatternLauncherAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double MidiPatternLauncherAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

juce::String MidiPatternLauncherAudioProcessor::getTransposeParameterID(int patternIndex) const
{
    return "p" + juce::String(patternIndex + 1) + "TransposeParam";
}

juce::String MidiPatternLauncherAudioProcessor::getRotationParameterID(int patternIndex) const
{
    return "p" + juce::String(patternIndex + 1) + "RotationParam";
}

juce::String MidiPatternLauncherAudioProcessor::getLengthParameterID(int patternIndex) const
{
    return "p" + juce::String(patternIndex + 1) + "LengthParam";
}

juce::String MidiPatternLauncherAudioProcessor::getInversionParameterID(int patternIndex) const
{
    return "p" + juce::String(patternIndex + 1) + "InversionParam";
}

juce::String MidiPatternLauncherAudioProcessor::getRetrogradeParameterID(int patternIndex) const
{
    return "p" + juce::String(patternIndex + 1) + "RetrogradeParam";
}

juce::String MidiPatternLauncherAudioProcessor::getM7ParameterID(int patternIndex) const
{
    return "p" + juce::String(patternIndex + 1) + "M7Param";
}

int MidiPatternLauncherAudioProcessor::getNumPrograms()
{
    return 1;
}

int MidiPatternLauncherAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MidiPatternLauncherAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String MidiPatternLauncherAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void MidiPatternLauncherAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void MidiPatternLauncherAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    mySampleRate = sampleRate;

    activePattern = -1;
    pendingPattern = -2;

    // Sentinel outside activeParam's clamped [0, numPatterns] range, so the
    // very next syncEngineFromParameters() call is guaranteed to see it as a
    // "change" and queue whatever pattern the Active Pattern knob currently
    // shows - without this, activePattern/pendingPattern get reset above but
    // lastActivePatternParam doesn't, so if the knob's value already matches
    // it (e.g. right after setStateInformation restored it), the queue never
    // fires and playback stays silently stopped until the knob is nudged.
    lastActivePatternParam = -1;

    patternStartStep = 0;
    lastPlayedStep = -1;
    lastLaunchBar = -1;

    wasHostPlaying = false;

    pendingNoteOffs.clear();

    displayActivePattern.store(-1);
    displayPendingPattern.store(-2);
    displayCurrentStep.store(0);
    displayCurrentBar.store(0);
}

void MidiPatternLauncherAudioProcessor::releaseResources()
{
    pendingNoteOffs.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MidiPatternLauncherAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    juce::ignoreUnused(layouts);
    return true;
}
#endif

//==============================================================================
// Central MIDI safety cleanup.
//
// This intentionally sends individual note-offs for all 128 notes and an
// All Notes Off message. The individual note-offs are the important part for
// stubborn instruments/hosts that do not fully react to CC-style panic messages.


void MidiPatternLauncherAudioProcessor::sendComposerBridgeResponse(juce::MidiBuffer& midiMessages,
    int sampleOffset,
    int channel,
    int status,
    int originalCommand,
    int detail)
{
    juce::uint8 payload[] =
    {
        0x7D,             // Non-commercial SysEx manufacturer ID
        0x4D, 0x50, 0x4C, // ASCII "MPL"
        0x02,             // Protocol version
        static_cast<juce::uint8>(juce::jlimit(0, 127, channel)), // Echo of the request's target channel
        0x7F,             // Response command
        static_cast<juce::uint8>(juce::jlimit(0, 127, status)),
        static_cast<juce::uint8>(juce::jlimit(0, 127, originalCommand)),
        static_cast<juce::uint8>(juce::jlimit(0, 127, detail))
    };

    midiMessages.addEvent(juce::MidiMessage::createSysExMessage(payload, sizeof(payload)),
        juce::jmax(0, sampleOffset));
}

void MidiPatternLauncherAudioProcessor::sendComposerBridgePatternDump(juce::MidiBuffer& midiMessages,
    int sampleOffset,
    int channel,
    int patternIndex)
{
    std::vector<juce::uint8> payload;
    payload.reserve(9 + (patternLength * 4));

    payload.push_back(0x7D);             // Non-commercial SysEx manufacturer ID
    payload.push_back(0x4D);             // ASCII "M"
    payload.push_back(0x50);             // ASCII "P"
    payload.push_back(0x4C);             // ASCII "L"
    payload.push_back(0x02);             // Protocol version
    payload.push_back(static_cast<juce::uint8>(juce::jlimit(0, 127, channel))); // Echo of the request's target channel
    payload.push_back(0x06);             // Pattern dump response command
    payload.push_back(static_cast<juce::uint8>(juce::jlimit(0, 127, patternIndex)));
    payload.push_back(static_cast<juce::uint8>(patternLength));

    for (int stepIndex = 0; stepIndex < patternLength; ++stepIndex)
    {
        const Step step = getStepForPattern(patternIndex, stepIndex);
        const bool enabled = step.note >= 0;

        payload.push_back(static_cast<juce::uint8>(enabled ? 1 : 0));
        payload.push_back(static_cast<juce::uint8>(enabled ? juce::jlimit(0, 127, step.note) : 0));
        payload.push_back(static_cast<juce::uint8>(juce::jlimit(0, 127, step.velocity)));
        payload.push_back(static_cast<juce::uint8>(juce::jlimit(0, 127, step.durationSteps)));
    }

    midiMessages.addEvent(juce::MidiMessage::createSysExMessage(payload.data(),
            static_cast<int>(payload.size())),
        juce::jmax(0, sampleOffset));
}


#if JUCE_DEBUG
bool MidiPatternLauncherAudioProcessor::runComposerBridgeProtocolSelfTest()
{
    struct ExpectedResponse
    {
        int channel = -1;
        int status = -1;
        int originalCommand = -1;
        int detail = -1;
    };

    struct ExpectedPatternDump
    {
        bool found = false;
        int channel = -1;
        int patternIndex = -1;
        int stepCount = -1;
        int step0Enabled = -1;
        int step0Note = -1;
        int step0Velocity = -1;
        int step0Duration = -1;
    };

    auto makeMessage = [](std::initializer_list<juce::uint8> payload)
    {
        return juce::MidiMessage::createSysExMessage(payload.begin(),
            static_cast<int>(payload.size()));
    };

    auto readSingleResponse = [](const juce::MidiBuffer& buffer) -> ExpectedResponse
    {
        ExpectedResponse response;

        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();

            if (!message.isSysEx())
                continue;

            const auto* data = message.getSysExData();
            const int dataSize = message.getSysExDataSize();

            if (dataSize < 10)
                continue;

            if (data[0] != 0x7D
                || data[1] != 0x4D
                || data[2] != 0x50
                || data[3] != 0x4C
                || data[4] != 0x02
                || data[6] != 0x7F)
                continue;

            response.channel = data[5];
            response.status = data[7];
            response.originalCommand = data[8];
            response.detail = data[9];
            break;
        }

        return response;
    };

    auto readSinglePatternDump = [](const juce::MidiBuffer& buffer) -> ExpectedPatternDump
    {
        ExpectedPatternDump dump;

        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();

            if (!message.isSysEx())
                continue;

            const auto* data = message.getSysExData();
            const int dataSize = message.getSysExDataSize();

            if (dataSize < 13)
                continue;

            if (data[0] != 0x7D
                || data[1] != 0x4D
                || data[2] != 0x50
                || data[3] != 0x4C
                || data[4] != 0x02
                || data[6] != 0x06)
                continue;

            dump.found = true;
            dump.channel = data[5];
            dump.patternIndex = data[7];
            dump.stepCount = data[8];
            dump.step0Enabled = data[9];
            dump.step0Note = data[10];
            dump.step0Velocity = data[11];
            dump.step0Duration = data[12];
            break;
        }

        return dump;
    };

    auto runCase = [this, &makeMessage, &readSingleResponse](const char* name,
        std::initializer_list<juce::uint8> payload,
        int expectedStatus,
        int expectedOriginalCommand,
        int expectedDetail)
    {
        juce::MidiBuffer responses;
        const auto message = makeMessage(payload);

        const bool handled = handleComposerBridgeSysEx(message, responses, 0);
        const auto response = readSingleResponse(responses);

        const bool passed = handled
            && response.status == expectedStatus
            && response.originalCommand == expectedOriginalCommand
            && response.detail == expectedDetail;

        DBG(juce::String("Composer Bridge self-test: ")
            + name
            + (passed ? " PASS" : " FAIL")
            + " handled=" + juce::String(handled ? "true" : "false")
            + " status=" + juce::String(response.status)
            + " originalCommand=" + juce::String(response.originalCommand)
            + " detail=" + juce::String(response.detail));

        return passed;
    };

    bool allPassed = true;

    // Valid set step, channel 0 ("All" - respond regardless of configured channel):
    // 7D 4D 50 4C 02 channel 01 pattern step enabled note velocity duration
    allPassed &= runCase("set step ACK",
        { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x00, 0x01, 0x00, 0x00, 0x01, 60, 100, 1 },
        0x00, 0x01, 0x00);

    {
        juce::MidiBuffer responses;
        const auto message = makeMessage({ 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x00, 0x05, 0x00 });

        const bool handled = handleComposerBridgeSysEx(message, responses, 0);
        const auto ack = readSingleResponse(responses);
        const auto dump = readSinglePatternDump(responses);

        const bool passed = handled
            && ack.status == 0x00
            && ack.originalCommand == 0x05
            && ack.detail == 0x00
            && dump.found
            && dump.channel == 0x00
            && dump.patternIndex == 0x00
            && dump.stepCount == patternLength
            && dump.step0Enabled == 0x01
            && dump.step0Note == 60
            && dump.step0Velocity == 100
            && dump.step0Duration == 1;

        allPassed &= passed;

        DBG(juce::String("Composer Bridge self-test: request pattern dump ACK + payload ")
            + (passed ? "PASS" : "FAIL")
            + " handled=" + juce::String(handled ? "true" : "false")
            + " ackStatus=" + juce::String(ack.status)
            + " dumpFound=" + juce::String(dump.found ? "true" : "false")
            + " dumpChannel=" + juce::String(dump.channel)
            + " pattern=" + juce::String(dump.patternIndex)
            + " steps=" + juce::String(dump.stepCount)
            + " step0Enabled=" + juce::String(dump.step0Enabled)
            + " step0Note=" + juce::String(dump.step0Note)
            + " step0Velocity=" + juce::String(dump.step0Velocity)
            + " step0Duration=" + juce::String(dump.step0Duration));
    }

    // Malformed set step: command present but missing required payload bytes.
    allPassed &= runCase("malformed set step NACK",
        { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x00, 0x01, 0x00 },
        0x01, 0x01, 0x00);

    // Invalid pattern index.
    allPassed &= runCase("invalid pattern NACK",
        { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x00, 0x01, 0x7F, 0x00, 0x01, 60, 100, 1 },
        0x02, 0x01, 0x7F);

    // Invalid step index.
    allPassed &= runCase("invalid step NACK",
        { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x00, 0x01, 0x00, 0x7F, 0x01, 60, 100, 1 },
        0x03, 0x01, 0x7F);

    // Unsupported command.
    allPassed &= runCase("unsupported command NACK",
        { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x00, 0x7E },
        0x04, 0x7E, 0x00);

    // Channel filtering: force a specific configured channel, then verify a
    // matching request is answered and a mismatched one is silently ignored
    // (no response of any kind - the whole point, since a non-addressed
    // instance must never reply). Restores whatever was configured before,
    // same "leave no trace" spirit as clearStep(0, 0) below.
    {
        const int originalChannelIndex = getChoiceParameterIndex("externalControlChannelParam");
        setParameterPlainValueNotifyingHost("externalControlChannelParam", 3.0f); // channel 3

        allPassed &= runCase("channel match ACK",
            { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x03, 0x01, 0x00, 0x00, 0x01, 60, 100, 1 },
            0x00, 0x01, 0x00);

        {
            juce::MidiBuffer responses;
            const auto message = makeMessage(
                { 0x7D, 0x4D, 0x50, 0x4C, 0x02, 0x05, 0x01, 0x00, 0x00, 0x01, 60, 100, 1 });

            const bool handled = handleComposerBridgeSysEx(message, responses, 0);
            const bool passed = !handled && responses.getNumEvents() == 0;
            allPassed &= passed;

            DBG(juce::String("Composer Bridge self-test: channel mismatch silently ignored ")
                + (passed ? "PASS" : "FAIL")
                + " handled=" + juce::String(handled ? "true" : "false")
                + " responseCount=" + juce::String(responses.getNumEvents()));
        }

        setParameterPlainValueNotifyingHost("externalControlChannelParam", static_cast<float>(originalChannelIndex));
    }

    DBG(juce::String("Composer Bridge self-test overall: ")
        + (allPassed ? "PASS" : "FAIL"));

    // Keep constructor-time debug self-tests from altering the initial pattern state.
    clearStep(0, 0);

    return allPassed;
}
#endif
bool MidiPatternLauncherAudioProcessor::handleComposerBridgeSysEx(const juce::MidiMessage& message,
    juce::MidiBuffer& midiMessages,
    int sampleOffset)
{
    if (!message.isSysEx())
        return false;

    if (!isComposerBridgeEnabled())
        return false;

    const auto* data = message.getSysExData();
    const int dataSize = message.getSysExDataSize();

    // v1.23.0 Composer Bridge SysEx protocol.
    //
    // JUCE exposes only the payload bytes between F0 and F7 via getSysExData().
    //
    // Header:
    //   7D 4D 50 4C 02 channel
    //
    // 7D       = non-commercial/educational SysEx manufacturer ID
    // 4D 50 4C = ASCII "MPL"
    // 02       = protocol version (bumped from 01: adds the channel byte below)
    // channel  = target instance channel, 0..16 - mirrors externalControlChannelParam's
    //            own convention (0 = respond regardless of configured channel, 1..16 =
    //            only respond if externalControlChannelParam equals this value). Every
    //            response echoes back whichever channel value arrived in the request
    //            it's answering, so a client addressing multiple instances can always
    //            attribute a response without relying on request/response ordering.
    //            Added because SysEx carries no MIDI channel of its own the way Control
    //            Change messages do - without this, every instance with Composer Bridge
    //            enabled would answer every request, since they all listen to the same
    //            broadcast MIDI stream from a single external controller track (see
    //            ComposerMastermind's docs/routing_policy_v0_1.md).
    //
    // Commands:
    //   01 pattern step enabled note velocity duration
    //   02 pattern
    //   03 sourcePattern destinationPattern
    //   04 pattern [16 x enabled note velocity duration]
    //   05 pattern
    //
    // Response:
    //   7D 4D 50 4C 02 channel 7F status originalCommand detail
    //
    // Status:
    //   00 ACK / success
    //   01 NACK / malformed message
    //   02 NACK / invalid pattern
    //   03 NACK / invalid step
    //   04 NACK / unsupported command

    constexpr uint8 manufacturerId = 0x7D;
    constexpr uint8 magicM = 0x4D;
    constexpr uint8 magicP = 0x50;
    constexpr uint8 magicL = 0x4C;
    constexpr uint8 protocolVersion = 0x02;

    constexpr int headerSize = 5;
    constexpr int channelIndex = headerSize;
    constexpr int commandIndex = headerSize + 1;

    constexpr int responseAck = 0x00;
    constexpr int responseMalformed = 0x01;
    constexpr int responseInvalidPattern = 0x02;
    constexpr int responseInvalidStep = 0x03;
    constexpr int responseUnsupportedCommand = 0x04;

    if (dataSize < commandIndex + 1)
        return false;

    if (static_cast<uint8>(data[0]) != manufacturerId
        || static_cast<uint8>(data[1]) != magicM
        || static_cast<uint8>(data[2]) != magicP
        || static_cast<uint8>(data[3]) != magicL
        || static_cast<uint8>(data[4]) != protocolVersion)
    {
        return false;
    }

    const int channel = static_cast<int>(static_cast<uint8>(data[channelIndex]));

    // Every MPL instance with Composer Bridge enabled listens to the same broadcast
    // MIDI stream, so without this filter every instance would answer every request.
    // Mirrors handleExternalControlCC's own channel check exactly.
    const int listenChannel = getExternalControlChannel();
    if (listenChannel != 0 && channel != listenChannel)
        return false;

    const int command = static_cast<int>(static_cast<uint8>(data[commandIndex]));

    auto getByte = [data, dataSize](int index) -> int
    {
        if (index < 0 || index >= dataSize)
            return 0;

        return static_cast<int>(static_cast<uint8>(data[index]));
    };

    switch (command)
    {
        case 0x01:
        {
            // Set step:
            // 7D 4D 50 4C 01 01 pattern step enabled note velocity duration
            constexpr int requiredSize = headerSize + 1 + 6;

            if (dataSize < requiredSize)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseMalformed, command, 0);
                return true;
            }

            const int patternIndex = getByte(commandIndex + 1);
            const int stepIndex = getByte(commandIndex + 2);
            const bool enabled = getByte(commandIndex + 3) != 0;
            const int note = getByte(commandIndex + 4);
            const int velocity = getByte(commandIndex + 5);
            const int durationSteps = getByte(commandIndex + 6);

            if (patternIndex < 0 || patternIndex >= numPatterns)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidPattern, command, patternIndex);
                return true;
            }

            if (stepIndex < 0 || stepIndex >= patternLength)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidStep, command, stepIndex);
                return true;
            }

            if (enabled)
                setStepValues(patternIndex, stepIndex, note, velocity, durationSteps);
            else
                clearStep(patternIndex, stepIndex);

            sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseAck, command, 0);
            return true;
        }

        case 0x02:
        {
            // Clear pattern:
            // 7D 4D 50 4C 01 02 pattern
            constexpr int requiredSize = headerSize + 1 + 1;

            if (dataSize < requiredSize)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseMalformed, command, 0);
                return true;
            }

            const int patternIndex = getByte(commandIndex + 1);

            if (patternIndex < 0 || patternIndex >= numPatterns)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidPattern, command, patternIndex);
                return true;
            }

            clearPattern(patternIndex);

            sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseAck, command, 0);
            return true;
        }

        case 0x03:
        {
            // Copy pattern:
            // 7D 4D 50 4C 01 03 sourcePattern destinationPattern
            constexpr int requiredSize = headerSize + 1 + 2;

            if (dataSize < requiredSize)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseMalformed, command, 0);
                return true;
            }

            const int sourcePatternIndex = getByte(commandIndex + 1);
            const int destinationPatternIndex = getByte(commandIndex + 2);

            if (sourcePatternIndex < 0 || sourcePatternIndex >= numPatterns)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidPattern, command, sourcePatternIndex);
                return true;
            }

            if (destinationPatternIndex < 0 || destinationPatternIndex >= numPatterns)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidPattern, command, destinationPatternIndex);
                return true;
            }

            copyPattern(sourcePatternIndex, destinationPatternIndex);

            sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseAck, command, 0);
            return true;
        }

        case 0x04:
        {
            // Write full pattern:
            // 7D 4D 50 4C 01 04 pattern
            //   step0Enabled step0Note step0Velocity step0Duration
            //   ...
            //   step15Enabled step15Note step15Velocity step15Duration
            constexpr int valuesPerStep = 4;
            constexpr int requiredSize = headerSize + 1 + 1 + (patternLength * valuesPerStep);

            if (dataSize < requiredSize)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseMalformed, command, 0);
                return true;
            }

            const int patternIndex = getByte(commandIndex + 1);

            if (patternIndex < 0 || patternIndex >= numPatterns)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidPattern, command, patternIndex);
                return true;
            }

            int dataIndex = commandIndex + 2;

            for (int stepIndex = 0; stepIndex < patternLength; ++stepIndex)
            {
                const bool enabled = getByte(dataIndex) != 0;
                const int note = getByte(dataIndex + 1);
                const int velocity = getByte(dataIndex + 2);
                const int durationSteps = getByte(dataIndex + 3);

                if (enabled)
                    setStepValues(patternIndex, stepIndex, note, velocity, durationSteps);
                else
                    clearStep(patternIndex, stepIndex);

                dataIndex += valuesPerStep;
            }

            sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseAck, command, 0);
            return true;
        }

        case 0x05:
        {
            // Request pattern dump:
            // 7D 4D 50 4C 01 05 pattern
            //
            // Emits:
            // 7D 4D 50 4C 01 06 pattern stepCount
            //   step0Enabled step0Note step0Velocity step0Duration
            //   ...
            //   step15Enabled step15Note step15Velocity step15Duration
            constexpr int requiredSize = headerSize + 1 + 1;

            if (dataSize < requiredSize)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseMalformed, command, 0);
                return true;
            }

            const int patternIndex = getByte(commandIndex + 1);

            if (patternIndex < 0 || patternIndex >= numPatterns)
            {
                sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseInvalidPattern, command, patternIndex);
                return true;
            }

            sendComposerBridgePatternDump(midiMessages, sampleOffset, channel, patternIndex);
            sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseAck, command, 0);
            return true;
        }

        default:
            sendComposerBridgeResponse(midiMessages, sampleOffset, channel, responseUnsupportedCommand, command, 0);
            return true;
    }
}

bool MidiPatternLauncherAudioProcessor::handleExternalControlCC(const juce::MidiMessage& message)
{
    debugLastCCNumber.store(message.getControllerNumber());
    debugLastCCChannel.store(message.getChannel());
    debugLastCCValue.store(message.getControllerValue());
    debugLastCCAccepted.store(false);

    if (!message.isController())
        return false;

    if (!isExternalControlEnabled())
        return false;

    const int listenChannel = getExternalControlChannel();
    const int messageChannel = message.getChannel();

    if (listenChannel != 0 && messageChannel != listenChannel)
        return false;

    const int ccNumber = message.getControllerNumber();
    const int ccValue = juce::jlimit(0, 127, message.getControllerValue());

    auto scaleInt = [ccValue](int minValue, int maxValue)
    {
        if (maxValue <= minValue)
            return minValue;

        const double normalised = static_cast<double>(ccValue) / 127.0;
        return juce::jlimit(minValue,
            maxValue,
            minValue + static_cast<int>(std::round(normalised * static_cast<double>(maxValue - minValue))));
    };


    auto setPlain = [this](const juce::String& parameterID, float plainValue)
    {
        setParameterPlainValueNotifyingHost(parameterID, plainValue);
    };

    switch (ccNumber)
    {
        case 20: // Active Pattern: 0 = Stopped, 1..3 = Pattern 1..3
            setPlain("activePatternParam", static_cast<float>(scaleInt(0, numPatterns)));
            debugLastCCAccepted.store(true);
            return true;

        case 21: // Target Pattern: selects which pattern subsequent Target Step edits apply to
            setPlain("targetPatternParam", static_cast<float>(scaleInt(0, numPatterns - 1)));
            debugLastCCAccepted.store(true);
            return true;

        case 22: // Grid Mode: Binary / Ternary
            setPlain("gridModeParam", static_cast<float>(scaleInt(0, 1)));
            debugLastCCAccepted.store(true);
            return true;

        case 23: // Rate: 0=Augmented, 1=Normal, 2=Diminished
            setPlain("globalRateParam", static_cast<float>(scaleInt(0, 2)));
            debugLastCCAccepted.store(true);
            return true;

        case 24: // Swing: 0=Off, 1=Triplet, 2=Shuffle
            setPlain("globalSwingParam", static_cast<float>(scaleInt(0, 2)));
            debugLastCCAccepted.store(true);
            return true;

        case 30: // P1 Transpose
            setPlain(getTransposeParameterID(0), static_cast<float>(scaleInt(-48, 48)));
            debugLastCCAccepted.store(true);
            return true;

        case 31: // P1 Rotation
            setPlain(getRotationParameterID(0), static_cast<float>(scaleInt(0, patternLength - 1)));
            debugLastCCAccepted.store(true);
            return true;

        case 32: // P1 Length
            setPlain(getLengthParameterID(0), static_cast<float>(scaleInt(minPatternLoopLength, patternLength)));
            debugLastCCAccepted.store(true);
            return true;

        case 33: // P1 Inversion
            setPlain(getInversionParameterID(0), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 34: // P1 Retrograde
            setPlain(getRetrogradeParameterID(0), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 35: // P1 M7
            setPlain(getM7ParameterID(0), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 40: // P2 Transpose
            setPlain(getTransposeParameterID(1), static_cast<float>(scaleInt(-48, 48)));
            debugLastCCAccepted.store(true);
            return true;

        case 41: // P2 Rotation
            setPlain(getRotationParameterID(1), static_cast<float>(scaleInt(0, patternLength - 1)));
            debugLastCCAccepted.store(true);
            return true;

        case 42: // P2 Length
            setPlain(getLengthParameterID(1), static_cast<float>(scaleInt(minPatternLoopLength, patternLength)));
            debugLastCCAccepted.store(true);
            return true;

        case 43: // P2 Inversion
            setPlain(getInversionParameterID(1), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 44: // P2 Retrograde
            setPlain(getRetrogradeParameterID(1), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 45: // P2 M7
            setPlain(getM7ParameterID(1), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 50: // P3 Transpose
            setPlain(getTransposeParameterID(2), static_cast<float>(scaleInt(-48, 48)));
            debugLastCCAccepted.store(true);
            return true;

        case 51: // P3 Rotation
            setPlain(getRotationParameterID(2), static_cast<float>(scaleInt(0, patternLength - 1)));
            debugLastCCAccepted.store(true);
            return true;

        case 52: // P3 Length
            setPlain(getLengthParameterID(2), static_cast<float>(scaleInt(minPatternLoopLength, patternLength)));
            debugLastCCAccepted.store(true);
            return true;

        case 53: // P3 Inversion
            setPlain(getInversionParameterID(2), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 54: // P3 Retrograde
            setPlain(getRetrogradeParameterID(2), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        case 55: // P3 M7
            setPlain(getM7ParameterID(2), ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        // Target Step Editing (60-64): external CC drives the same Target parameters the
        // plugin's own UI uses, so the existing syncEngineFromParameters() polling/edge-detection
        // (selection change -> browse, value change -> commit) is the safety mechanism here too -
        // no separate queue needed. A CC 60 (select step) landing in a different processBlock than
        // its CC 61-64 (note/velocity/duration/enabled) is the same as a human moving one slider at
        // a time in the UI; only a genuine one-block race straddling a boundary is a known,
        // accepted residual risk.
        case 60: // Target Step
            setPlain("targetStepParam", static_cast<float>(scaleInt(1, patternLength)));
            debugLastCCAccepted.store(true);
            return true;

        case 61: // Target Note
            setPlain("targetNoteParam", static_cast<float>(scaleInt(0, 127)));
            debugLastCCAccepted.store(true);
            return true;

        case 62: // Target Velocity
            setPlain("targetVelocityParam", static_cast<float>(scaleInt(1, 127)));
            debugLastCCAccepted.store(true);
            return true;

        case 63: // Target Duration
            setPlain("targetDurationParam", static_cast<float>(scaleInt(1, patternLength)));
            debugLastCCAccepted.store(true);
            return true;

        case 64: // Target Enabled
            setPlain("targetEnabledParam", ccValue >= 64 ? 1.0f : 0.0f);
            debugLastCCAccepted.store(true);
            return true;

        default:
            break;
    }

    return false;
}

void MidiPatternLauncherAudioProcessor::sendAllNotesOffNow(juce::MidiBuffer& midiMessages, int sampleOffset)
{
    for (int note = 0; note < 128; ++note)
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, note), sampleOffset);

    midiMessages.addEvent(juce::MidiMessage::allNotesOff(1), sampleOffset);

    pendingNoteOffs.clear();
}

void MidiPatternLauncherAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    syncEngineFromParameters();

    //==========================================================================
    // 1. Read incoming MIDI trigger notes.
    // Trigger notes are consumed and not passed through.

    juce::MidiBuffer incomingMidi;
    incomingMidi.swapWith(midiMessages);

    midiMessages.clear();

    for (const auto metadata : incomingMidi)
    {
        const auto message = metadata.getMessage();

        if (handleComposerBridgeSysEx(message, midiMessages, metadata.samplePosition))
            continue;

        if (handleExternalControlCC(message))
            continue;

        if (message.isNoteOn())
        {
            const int noteNumber = message.getNoteNumber();

            int requestedPattern = -999;

            if (noteNumber == 36)
                requestedPattern = 0;
            else if (noteNumber == 38)
                requestedPattern = 1;
            else if (noteNumber == 40)
                requestedPattern = 2;

            if (requestedPattern != -999)
            {
                if (requestedPattern == activePattern)
                    pendingPattern = -1;    // Queue stop.
                else
                    pendingPattern = requestedPattern;

                displayPendingPattern.store(pendingPattern);
            }
        }
    }

    //==========================================================================
    // 2. Get host transport position.

    auto* hostPlayHead = getPlayHead();

    if (hostPlayHead == nullptr)
        return;

    auto position = hostPlayHead->getPosition();

    if (!position.hasValue())
        return;

    const auto isPlaying = position->getIsPlaying();

    // Immediate transport-stop panic.
    //
    // This handles the classic stuck-note case:
    // a note-on has been sent, but its scheduled future note-off would never
    // arrive because the host stopped before that musical position.
    if (wasHostPlaying && !isPlaying)
    {
        sendAllNotesOffNow(midiMessages, 0);
        displayCurrentStep.store(0);
    }

    wasHostPlaying = isPlaying;

    if (!isPlaying)
    {
        lastPlayedStep = -1;
        lastLaunchBar = -1;

        // sendAllNotesOffNow() already clears pendingNoteOffs on the actual
        // playing->stopped transition. This extra clear is harmless and keeps
        // internal state clean even if processing starts while stopped.
        pendingNoteOffs.clear();

        displayCurrentStep.store(0);
        return;
    }

    auto ppqPosition = position->getPpqPosition();

    if (!ppqPosition.hasValue())
        return;

    double bpmValue = 120.0;

    auto bpm = position->getBpm();

    if (bpm.hasValue() && *bpm > 0.0)
        bpmValue = *bpm;

    const double ppqStart = *ppqPosition;

    //==========================================================================
    // 3. Timing setup.

    const int numSamples = buffer.getNumSamples();

    const double quarterNotesPerSecond = bpmValue / 60.0;
    const double ppqPerSample = quarterNotesPerSecond / mySampleRate;

    if (ppqPerSample <= 0.0)
        return;

    const double ppqEnd = ppqStart + (ppqPerSample * static_cast<double> (numSamples));

    const int gridStepCount = getGridStepCount();
    const double gridStepLengthInPpq = 4.0 / static_cast<double> (gridStepCount);

    // v1.29.0 Rate: a real classical augmentation/diminution device - the whole
    // active pattern restated in longer (Augmented, 0.5x) or shorter (Diminished,
    // 2x) note values, onsets AND durations both scaling together. Dividing the
    // ppq-per-step value by the rate multiplier is the single substantive engine
    // change this requires: everything below that derives its timing from "how
    // many ppq is one step" (grid-line detection, note-on placement, swing delay,
    // and - via the stepPpq/gridStepLengthInPpq math note-offs re-derive at check
    // time - note-off placement too) scales for free. Which stored step plays is
    // separate, step-index arithmetic (playbackStepIndex/sourceStepIndex below)
    // and is intentionally untouched: Rate changes WHEN steps land, never WHICH
    // step or note plays. See getGlobalRateMultiplier().
    const double effectiveGridStepLengthInPpq = gridStepLengthInPpq / getGlobalRateMultiplier();

    //==============================================================================
    // 4. Determine which metric grid lines occur inside this block.
    //
    // v1.18.0 Swing fix:
    // Note-ons may be delayed past their original grid line and into a later audio
    // block. Therefore, note-on generation also inspects one previous grid step.
    // Bar-boundary actions and note-offs still use the unswung grid lines that
    // actually occur inside this block.

    const int firstGridStepInBlock = static_cast<int> (std::ceil(ppqStart / effectiveGridStepLengthInPpq));
    const int lastStepInBlock = static_cast<int> (std::floor(ppqEnd / effectiveGridStepLengthInPpq));
    const int firstStepForNoteOns = firstGridStepInBlock - 1;

    constexpr int midiChannel = 1;

    for (int step = firstStepForNoteOns; step <= lastStepInBlock; ++step)
    {
        const double stepPpq = static_cast<double> (step) * effectiveGridStepLengthInPpq;
        const bool gridStepIsInsideThisBlock = step >= firstGridStepInBlock;

        int sampleOffset = static_cast<int> (std::round((stepPpq - ppqStart) / ppqPerSample));
        sampleOffset = juce::jlimit(0, numSamples - 1, sampleOffset);

        const int currentBar = step / gridStepCount;
        const bool isAtBarStart = (step % gridStepCount) == 0;

        if (gridStepIsInsideThisBlock)
        {
            displayCurrentBar.store(currentBar + 1);
            displayActivePattern.store(activePattern);
            displayPendingPattern.store(pendingPattern);
        }

        //======================================================================
        // 4a. Send pending note-offs exactly on their musical target step.
        //
        // Important for transpose and rotation:
        // PendingNoteOff stores the exact MIDI note that was originally sent.
        // Therefore, if transpose or rotation changes while a note is held,
        // the correct note-off is still sent.

        if (gridStepIsInsideThisBlock)
        {
            for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end(); )
            {
                if (step >= it->targetStep)
                {
                    midiMessages.addEvent(juce::MidiMessage::noteOff(it->channel, it->note),
                        sampleOffset);

                    it = pendingNoteOffs.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        //======================================================================
        // 4b. Apply queued launch/stop only at the start of a new bar.
        //
        // Important safety behavior:
        // Before a pattern stop or switch is applied, force all currently
        // sounding notes off at the exact bar-boundary sample offset.
        //
        // This prevents long notes from the previous pattern from leaking into
        // the stopped state or into the next pattern.

        if (gridStepIsInsideThisBlock
            && pendingPattern != -2
            && isAtBarStart
            && currentBar != lastLaunchBar)
        {
            sendAllNotesOffNow(midiMessages, sampleOffset);

            activePattern = pendingPattern;
            pendingPattern = -2;

            displayActivePattern.store(activePattern);
            displayPendingPattern.store(pendingPattern);

            patternStartStep = step;
            lastPlayedStep = -1;
            lastLaunchBar = currentBar;

            if (activePattern < 0)
                displayCurrentStep.store(0);
        }

        //======================================================================
        // 4c. If stopped, do not generate new notes.

        if (activePattern < 0)
            continue;

        //======================================================================
        // 4d. Generate note-on at this exact grid point.

        if (step == lastPlayedStep)
            continue;

        const int stepsSincePatternStart = step - patternStartStep;
        const int activeLoopLength = juce::jmin(getPatternLoopLength(activePattern), gridStepCount);
        const int playbackStepIndex = ((stepsSincePatternStart % activeLoopLength) + activeLoopLength) % activeLoopLength;
        if (gridStepIsInsideThisBlock)
            displayCurrentStep.store(playbackStepIndex + 1);

        // v1.4.0/v1.12.0:
        // Rotation is applied only to the source step lookup.
        // The visible playhead follows the loop-relative playback step.
        const int sourceStepIndex = getRotatedSourceStepIndex(activePattern, playbackStepIndex);

        const Step currentStepData = getStepForPattern(activePattern, sourceStepIndex);

        if (currentStepData.note >= 0
            && currentStepData.velocity > 0
            && currentStepData.durationSteps > 0)
        {
            const auto stepVelocity = static_cast<juce::uint8> (
                juce::jlimit(1, 127, currentStepData.velocity)
                );

            const int outputNote = applyPatternTransformsToNote(activePattern, currentStepData.note);

            const float swingPercent = getGlobalSwingAmount();
            const bool shouldSwingStep = swingPercent > 0.0f && (playbackStepIndex % 2) == 1;
            const double swingFraction = static_cast<double>(swingPercent) / 100.0;

            // v1.18.0:
            // Swing delays odd loop-relative playback steps in musical time.
            // 100% Swing (Shuffle) means a delay of exactly half a grid step,
            // because the maximum delay is half a grid step multiplied by
            // the swing amount - which lands the swung note precisely at the
            // midpoint between its neighbours, the true 3:1 dotted-shuffle
            // ratio (v1.28.1).
            const double swingDelayPpq = shouldSwingStep
                ? effectiveGridStepLengthInPpq * 0.5 * swingFraction
                : 0.0;

            const double noteOnPpq = stepPpq + swingDelayPpq;

            if (noteOnPpq >= ppqStart && noteOnPpq < ppqEnd)
            {
                int noteOnSampleOffset = static_cast<int> (
                    std::round((noteOnPpq - ppqStart) / ppqPerSample));

                noteOnSampleOffset = juce::jlimit(
                    0,
                    juce::jmax(0, numSamples - 1),
                    noteOnSampleOffset);

                midiMessages.addEvent(juce::MidiMessage::noteOn(midiChannel,
                        outputNote,
                        stepVelocity),
                    noteOnSampleOffset);

                PendingNoteOff noteOff;
                noteOff.note = outputNote;
                noteOff.channel = midiChannel;
                noteOff.targetStep = step + currentStepData.durationSteps;

                pendingNoteOffs.push_back(noteOff);

                lastPlayedStep = step;
            }
        }

        if (gridStepIsInsideThisBlock
            && (getGlobalSwingAmount() <= 0.0f || (playbackStepIndex % 2) == 0))
            lastPlayedStep = step;
    }
}

//==============================================================================
bool MidiPatternLauncherAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MidiPatternLauncherAudioProcessor::createEditor()
{
    return new MidiPatternLauncherAudioProcessorEditor(*this);
}

//==============================================================================
void MidiPatternLauncherAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement root("MidiPatternLauncher");

    root.setAttribute("version", "1.14.0");
    root.setAttribute("gridMode", getGridModeIndex());

    for (int patternIndex = 0; patternIndex < numPatterns; ++patternIndex)
    {
        auto* patternXml = root.createNewChildElement("Pattern");

        patternXml->setAttribute("index", patternIndex);
        patternXml->setAttribute("transpose", patternTranspose[static_cast<size_t>(patternIndex)]);
        patternXml->setAttribute("rotation", patternRotation[static_cast<size_t>(patternIndex)]);
        patternXml->setAttribute("length", getPatternLoopLength(patternIndex));
        patternXml->setAttribute("inverted", patternInverted[static_cast<size_t>(patternIndex)] ? 1 : 0);
        patternXml->setAttribute("retrograde", patternRetrograde[static_cast<size_t>(patternIndex)] ? 1 : 0);
        patternXml->setAttribute("m7", patternM7[static_cast<size_t>(patternIndex)] ? 1 : 0);

        for (int stepIndex = 0; stepIndex < patternLength; ++stepIndex)
        {
            const auto& step = patterns[static_cast<size_t>(patternIndex)]
                [static_cast<size_t>(stepIndex)];

            auto* stepXml = patternXml->createNewChildElement("Step");

            stepXml->setAttribute("index", stepIndex);
            stepXml->setAttribute("note", step.note);
            stepXml->setAttribute("velocity", step.velocity);
            stepXml->setAttribute("durationSteps", step.durationSteps);
        }
    }

    auto apvtsState = apvts.copyState();
    std::unique_ptr<juce::XmlElement> apvtsXml(apvtsState.createXml());

    if (apvtsXml != nullptr)
    {
        apvtsXml->setTagName("APVTS");
        root.addChildElement(apvtsXml.release());
    }

    copyXmlToBinary(root, destData);
}

void MidiPatternLauncherAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xmlState = getXmlFromBinary(data, sizeInBytes);

    if (xmlState == nullptr)
        return;

    if (!xmlState->hasTagName("MidiPatternLauncher"))
        return;

    juce::MidiBuffer emptyMidiBuffer;
    sendAllNotesOffNow(emptyMidiBuffer, 0);

    activePattern = -1;
    pendingPattern = -2;

    for (auto* patternXml : xmlState->getChildWithTagNameIterator("Pattern"))
    {
        const int patternIndex = patternXml->getIntAttribute("index", -1);

        if (patternIndex < 0 || patternIndex >= numPatterns)
            continue;

        patternTranspose[static_cast<size_t>(patternIndex)] =
            juce::jlimit(-48, 48, patternXml->getIntAttribute("transpose", 0));

        patternRotation[static_cast<size_t>(patternIndex)] =
            juce::jlimit(0, patternLength - 1, patternXml->getIntAttribute("rotation", 0));

        patternLoopLength[static_cast<size_t>(patternIndex)] =
            juce::jlimit(minPatternLoopLength,
                patternLength,
                patternXml->getIntAttribute("length", defaultPatternLoopLength));

        patternInverted[static_cast<size_t>(patternIndex)] =
            patternXml->getIntAttribute("inverted", 0) != 0;

        patternRetrograde[static_cast<size_t>(patternIndex)] =
            patternXml->getIntAttribute("retrograde", 0) != 0;

        patternM7[static_cast<size_t>(patternIndex)] =
            patternXml->getIntAttribute("m7", 0) != 0;

        for (auto* stepXml : patternXml->getChildWithTagNameIterator("Step"))
        {
            const int stepIndex = stepXml->getIntAttribute("index", -1);

            if (stepIndex < 0 || stepIndex >= patternLength)
                continue;

            auto& step = patterns[static_cast<size_t>(patternIndex)]
                [static_cast<size_t>(stepIndex)];

            const int restoredNote = stepXml->getIntAttribute("note", -1);
            const int restoredVelocity = stepXml->getIntAttribute("velocity", 0);
            const int restoredDurationSteps = stepXml->getIntAttribute("durationSteps", 0);

            if (restoredNote < 0)
            {
                step.note = -1;
                step.velocity = 0;
                step.durationSteps = 0;
            }
            else
            {
                step.note = juce::jlimit(0, 127, restoredNote);
                step.velocity = juce::jlimit(1, 127, restoredVelocity);
                step.durationSteps = juce::jlimit(1, patternLength, restoredDurationSteps);
            }
        }
    }

    bool restoredApvtsState = false;

    if (auto* apvtsXml = xmlState->getChildByName("APVTS"))
    {
        auto apvtsState = juce::ValueTree::fromXml(*apvtsXml);

        if (apvtsState.isValid())
        {
            apvts.replaceState(apvtsState);
            restoredApvtsState = true;
        }
    }

    if (!restoredApvtsState && xmlState->hasAttribute("gridMode"))
    {
        const int restoredGridMode = juce::jlimit(0, 1, xmlState->getIntAttribute("gridMode", 0));
        setParameterPlainValueNotifyingHost("gridModeParam", static_cast<float>(restoredGridMode));
    }

    activePattern = -1;
    pendingPattern = -2;

    displayActivePattern.store(-1);
    displayPendingPattern.store(-2);
    displayCurrentStep.store(0);
    displayCurrentBar.store(0);

    for (int patternIndex = 0; patternIndex < numPatterns; ++patternIndex)
        syncParametersFromPattern(patternIndex);

    // Sentinel (see prepareToPlay) rather than syncing to the just-restored
    // knob value directly - that previously hid the "change" from
    // syncEngineFromParameters() and left playback silently stopped until a
    // manual knob nudge. This guarantees the next processBlock's poll sees a
    // mismatch and queues whatever pattern the restored knob shows.
    lastActivePatternParam = -1;

    lastTargetPatternParam = getTargetPatternIndex();
    lastTargetStepParam = getTargetStepIndex() + 1;
    lastTargetNoteParam = getTargetNote();
    lastTargetVelocityParam = getTargetVelocity();
    lastTargetDurationParam = getTargetDurationSteps();
    lastTargetEnabledParam = getTargetEnabled();

    syncTargetParametersFromStep();
}

//==============================================================================
// This creates new instances of the plugin.

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiPatternLauncherAudioProcessor();
}

