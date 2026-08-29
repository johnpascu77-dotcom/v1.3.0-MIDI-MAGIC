#pragma once

#include <JuceHeader.h>

class MidiPatternLauncherAudioProcessor;

// Duplex local-socket connection to Composer Mastermind's PatternSyncServer -
// built because Bitwig does not deliver incoming SysEx to a hosted VST
// instrument's plugin code (community-reported, confirmed empirically
// 2026-08-19: CC round-trips fine over the same routing, SysEx never does),
// which makes the existing Composer Bridge SysEx protocol unusable for this
// specific purpose while both plugins run inside Bitwig. Runs alongside,
// not instead of, that SysEx protocol - which stays genuinely useful for
// other hosts, hardware controllers, and the Standalone test harness this
// project already validated it against.
//
// One instance per MidiPatternLauncherAudioProcessor. Connects to a fixed
// local TCP port and reconnects on a timer if the connection drops or was
// never made (Composer Mastermind might not be running or added to the
// project yet). Respects the same "Composer Bridge Enabled" toggle the
// SysEx protocol already uses - disconnects if it's turned off.
//
// Protocol (JSON over InterprocessConnection's own message framing):
//   MPL -> CM, on connect and then re-sent every reconnect-timer tick
//   while connected (so a later channel change is picked up without
//   needing a manual reconnect): {"type":"hello","channel":<int>}
//   CM -> MPL:                  {"type":"requestPatternDump","patternIndex":<int>}
//   MPL -> CM:                  {"type":"patternDumpResponse","patternIndex":<int>,
//                                 "steps":[{"enabled":bool,"note":int,"velocity":int,
//                                 "duration":int}, ...]}
//   CM -> MPL:                  {"type":"writeStep","patternIndex":<int>,"stepIndex":<int>,
//                                 "enabled":bool,"note":int,"velocity":int,"duration":int}
//   CM -> MPL:                  {"type":"writeFullPattern","patternIndex":<int>,
//                                 "steps":[{"enabled":bool,"note":int,"velocity":int,
//                                 "duration":int}, ...16 entries]}
// The two write message types (added 2026-08-21) commit directly via
// setStepValues/clearStep - the same calls the SysEx Composer Bridge
// protocol's own commands 0x01/0x04 make - deliberately bypassing the
// externalControlChannelParam CC 60-64 Target Step Editing path entirely.
// That CC path drives ordinary APVTS parameters, committed via
// syncEngineFromParameters()'s once-per-block polling/edge-detection (built
// for one human turning one knob at a time); Composer Mastermind's own
// motif engine writing several steps in quick succession was found to
// silently lose every write but the last within a given block, since the
// poll only ever observes the parameters' state at the moment it runs, not
// every intermediate value they passed through (confirmed live, 2026-08-21).
// These two message types have no such ambiguity - each is a single
// complete record, applied synchronously and atomically the instant it's
// received, matching how the SysEx protocol's equivalent commands already
// behave for any other caller that can actually deliver SysEx to this
// plugin (Bitwig can't, for a hosted instrument - see the class comment
// above; a standalone host or hardware controller can).
// Must be kept in sync by hand with Composer Mastermind's composer/PatternSyncServer -
// the two projects don't share a header.
class ComposerBridgeIpcClient : public juce::InterprocessConnection,
                                 private juce::Timer
{
public:
    explicit ComposerBridgeIpcClient(MidiPatternLauncherAudioProcessor& processor);
    ~ComposerBridgeIpcClient() override;

    void connectionMade() override;
    void connectionLost() override;
    void messageReceived(const juce::MemoryBlock& message) override;

private:
    void timerCallback() override;
    void sendHello();
    void sendPatternDumpResponse(int patternIndex);

    MidiPatternLauncherAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComposerBridgeIpcClient)
};
