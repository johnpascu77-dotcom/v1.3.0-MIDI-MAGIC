# Composer Bridge Protocol

This document describes the MIDI SysEx protocol used by MIDI Pattern Launcher for external Composer Bridge control.

The Composer Bridge protocol allows external tools, scripts, or hardware controllers to modify and inspect pattern data inside the plugin.

## SysEx Frame

All Composer Bridge messages use this SysEx header:

    F0 7D 4D 50 4C 02 channel ... F7

| Byte | Meaning |
|---:|---|
| F0 | SysEx start |
| 7D | Non-commercial manufacturer ID |
| 4D 50 4C | Project identifier: MPL |
| 02 | Protocol version |
| channel | Target instance channel, 0..16 |
| ... | Command and payload |
| F7 | SysEx end |

**`channel`** (added in protocol v2, replacing v1's plain `01` version byte with no channel field) mirrors the
existing `externalControlChannelParam`'s own convention: `0` = respond regardless of configured channel ("All"),
`1`..`16` = only respond if `externalControlChannelParam` equals this value. Every instance with Composer Bridge
enabled listens to the same broadcast MIDI stream (there is no per-instance MIDI input in typical routing), and
unlike Control Change messages, SysEx carries no MIDI channel of its own — without this field, every instance
would answer every request, and a client addressing multiple instances (e.g. Composer Mastermind polling three
MPL instances for their current pattern content) would have no way to tell which instance a given response came
from. Every response echoes back whichever `channel` value arrived in the request it's answering, so a client can
always attribute a response correctly without relying on request/response ordering, even when addressing several
instances in close succession.

## Commands

| Command | Direction | Name |
|---:|---|---|
| 0x01 | External -> Plugin | Write step |
| 0x02 | External -> Plugin | Clear pattern |
| 0x03 | External -> Plugin | Copy pattern |
| 0x04 | External -> Plugin | Write full pattern |
| 0x05 | External -> Plugin | Request pattern dump |
| 0x06 | Plugin -> External | Pattern dump response |
| 0x7F | Plugin -> External | ACK/NACK feedback |

## ACK/NACK Feedback

The plugin responds to supported external commands with a feedback message:

    F0 7D 4D 50 4C 02 channel 7F status originalCommand detail F7

| Byte | Meaning |
|---:|---|
| channel | Echo of the request's target channel |
| 7F | Feedback response command |
| status | ACK/NACK status code |
| originalCommand | Command being acknowledged/rejected |
| detail | Optional detail byte |

### Status codes

| Status | Name | Meaning |
|---:|---|---|
| 0x00 | ACK | Command accepted |
| 0x01 | NACK malformed | Payload length or format was invalid |
| 0x02 | NACK invalid pattern | Pattern index was out of range |
| 0x03 | NACK invalid step | Step index was out of range |
| 0x04 | NACK unsupported command | Command is not supported |

A request whose `channel` byte doesn't match the instance's configured `externalControlChannelParam` (and isn't
`0`/"All") receives **no response at all** — not even NACK. Silence, not rejection, is the correct behavior for
an instance that wasn't addressed; anything else would defeat the purpose of the channel field.

## Command 0x01 - Write Step

Writes one step in one pattern.

    F0 7D 4D 50 4C 02 channel 01 pattern step enabled note velocity duration F7

Payload:

| Byte | Meaning |
|---:|---|
| pattern | Pattern index |
| step | Step index |
| enabled | 0 = disabled, non-zero = enabled |
| note | MIDI note number |
| velocity | MIDI velocity |
| duration | Step duration value |

Expected response:

    F0 7D 4D 50 4C 02 channel 7F status 01 detail F7

## Command 0x02 - Clear Pattern

Clears one pattern.

    F0 7D 4D 50 4C 02 channel 02 pattern F7

Expected response:

    F0 7D 4D 50 4C 02 channel 7F status 02 detail F7

## Command 0x03 - Copy Pattern

Copies one pattern into another pattern slot.

    F0 7D 4D 50 4C 02 channel 03 sourcePattern destinationPattern F7

Expected response:

    F0 7D 4D 50 4C 02 channel 7F status 03 detail F7

## Command 0x04 - Write Full Pattern

Writes all 16 steps of one pattern.

    F0 7D 4D 50 4C 02 channel 04 pattern stepCount stepData... F7

stepCount is expected to be 16.

Each step uses 4 bytes:

    enabled note velocity duration

Total step data size is 16 * 4 = 64 bytes.

Expected response:

    F0 7D 4D 50 4C 02 channel 7F status 04 detail F7

## Command 0x05 - Request Pattern Dump

Requests the plugin to send back the contents of one pattern.

    F0 7D 4D 50 4C 02 channel 05 pattern F7

Expected responses:

    F0 7D 4D 50 4C 02 channel 06 pattern stepCount stepData... F7
    F0 7D 4D 50 4C 02 channel 7F status 05 detail F7

Depending on host timing and MIDI routing, external clients should be prepared to receive the dump and ACK in either practical order.

## Response 0x06 - Pattern Dump

The pattern dump response contains one complete 16-step pattern.

    F0 7D 4D 50 4C 02 channel 06 pattern stepCount stepData... F7

Payload:

| Byte | Meaning |
|---:|---|
| channel | Echo of the request's target channel |
| pattern | Pattern index |
| stepCount | Number of steps, currently 16 |
| stepData | Repeated step records |

Each step record uses 4 bytes:

    enabled note velocity duration

Example interpretation:

| Step | Enabled | Note | Velocity | Duration |
|---:|---:|---:|---:|---:|
| 0 | 1 | 60 | 100 | 1 |
| 1 | 0 | 60 | 100 | 1 |

## Notes

- All values must be 7-bit MIDI data bytes: 0..127.
- SysEx start F0 and end F7 are not counted as payload bytes.
- The manufacturer ID 7D is intended for non-commercial/development use.
- Composer Bridge must be enabled in the plugin for external Composer Bridge SysEx handling.
- **Protocol v1 (no `channel` byte, version byte `01`) is not accepted by v2 plugin builds** — the version byte
  is checked exactly, so a v1 client's messages are silently ignored rather than partially misparsed. This was a
  deliberate breaking change (added 2026-08-19), not a compatibility oversight: v1 had no way for an external
  client to address one specific instance among several sharing a broadcast MIDI stream, which made multi-instance
  reads/writes via this protocol unsafe.
