#include "ComposerBridgeIpcClient.h"
#include "PluginProcessor.h"

namespace
{
    // Must match Composer Mastermind's composer/PatternSyncServer.cpp.
    constexpr int kPatternSyncPort = 47823;
    constexpr int kReconnectIntervalMs = 2000;
    constexpr int kConnectTimeoutMs = 200;
    constexpr int kStepsPerPattern = 16; // matches the SysEx Composer Bridge protocol's own fixed patternLength

    void sendJson(juce::InterprocessConnection& connection, const juce::var& value)
    {
        const auto json = juce::JSON::toString(value, true);
        connection.sendMessage(juce::MemoryBlock(json.toRawUTF8(), json.getNumBytesAsUTF8()));
    }
}

ComposerBridgeIpcClient::ComposerBridgeIpcClient(MidiPatternLauncherAudioProcessor& processor)
    : juce::InterprocessConnection(true), processorRef(processor)
{
    startTimer(kReconnectIntervalMs);
}

ComposerBridgeIpcClient::~ComposerBridgeIpcClient()
{
    stopTimer();
    disconnect(); // required by InterprocessConnection's own contract
}

void ComposerBridgeIpcClient::connectionMade()
{
    sendHello();
}

void ComposerBridgeIpcClient::connectionLost()
{
    // Nothing to do here - timerCallback() retries on its own schedule.
}

void ComposerBridgeIpcClient::messageReceived(const juce::MemoryBlock& message)
{
    const auto json = juce::String::fromUTF8(static_cast<const char*>(message.getData()),
        static_cast<int>(message.getSize()));

    juce::var parsed;
    if (juce::JSON::parse(json, parsed).failed() || !parsed.isObject())
        return;

    const auto type = parsed["type"].toString();

    if (type == "requestPatternDump")
    {
        sendPatternDumpResponse(static_cast<int>(parsed["patternIndex"]));
        return;
    }

    if (type == "writeStep")
    {
        const int patternIndex = static_cast<int>(parsed["patternIndex"]);
        const int stepIndex = static_cast<int>(parsed["stepIndex"]);

        if (static_cast<bool>(parsed["enabled"]))
        {
            processorRef.setStepValues(patternIndex, stepIndex,
                static_cast<int>(parsed["note"]), static_cast<int>(parsed["velocity"]),
                static_cast<int>(parsed["duration"]));
        }
        else
        {
            processorRef.clearStep(patternIndex, stepIndex);
        }

        return;
    }

    if (type == "writeFullPattern")
    {
        const int patternIndex = static_cast<int>(parsed["patternIndex"]);

        if (auto* stepsArray = parsed["steps"].getArray())
        {
            for (int stepIndex = 0; stepIndex < stepsArray->size(); ++stepIndex)
            {
                const auto& stepVar = (*stepsArray)[stepIndex];

                if (static_cast<bool>(stepVar["enabled"]))
                {
                    processorRef.setStepValues(patternIndex, stepIndex,
                        static_cast<int>(stepVar["note"]), static_cast<int>(stepVar["velocity"]),
                        static_cast<int>(stepVar["duration"]));
                }
                else
                {
                    processorRef.clearStep(patternIndex, stepIndex);
                }
            }
        }

        return;
    }
}

void ComposerBridgeIpcClient::sendHello()
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("type", "hello");
    obj->setProperty("channel", processorRef.getExternalControlChannel());
    sendJson(*this, juce::var(obj));
}

void ComposerBridgeIpcClient::sendPatternDumpResponse(int patternIndex)
{
    juce::Array<juce::var> steps;

    for (int stepIndex = 0; stepIndex < kStepsPerPattern; ++stepIndex)
    {
        auto* step = new juce::DynamicObject();
        const bool enabled = processorRef.stepHasNote(patternIndex, stepIndex);
        step->setProperty("enabled", enabled);
        step->setProperty("note", enabled ? processorRef.getStepNote(patternIndex, stepIndex) : 0);
        step->setProperty("velocity", processorRef.getStepVelocity(patternIndex, stepIndex));
        step->setProperty("duration", processorRef.getStepDurationSteps(patternIndex, stepIndex));
        steps.add(juce::var(step));
    }

    auto* response = new juce::DynamicObject();
    response->setProperty("type", "patternDumpResponse");
    response->setProperty("patternIndex", patternIndex);
    response->setProperty("steps", steps);
    sendJson(*this, juce::var(response));
}

void ComposerBridgeIpcClient::timerCallback()
{
    if (isConnected())
    {
        if (!processorRef.isComposerBridgeEnabled())
        {
            disconnect();
            return;
        }

        // Re-announce on every tick, not just at connectionMade() - the
        // instance's channel can change (e.g. "All" -> a specific number)
        // while the socket stays open, and Composer Mastermind only knows
        // whichever channel the most recent hello reported.
        sendHello();
        return;
    }

    if (!processorRef.isComposerBridgeEnabled())
        return;

    connectToSocket("127.0.0.1", kPatternSyncPort, kConnectTimeoutMs);
}
