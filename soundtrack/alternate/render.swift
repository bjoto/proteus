// macOS offline sampler bridge. Build/run through render.py; no audio device needed.
import AVFoundation
import Foundation

struct Track: Decodable {
    let name: String
    let program: UInt8?
    let synth: SynthPatch?
    let channel: UInt8
    let volume: UInt8
    let pan: UInt8
    let notes: [[Double]] // beat, duration in beats, MIDI pitch, velocity
}

// Original oscillator patches; settings live in the project, with no sample bank dependency.
struct SynthPatch: Decodable {
    let kind: String
    let gain: Double
    let detune_cents: Double
    let cutoff_start: Double
    let cutoff_end: Double
    let motion_hz: Double
    let motion_depth: Double
    let drive: Double
    let sub_mix: Double
    let attack: Double
    let release: Double
}

func synthesize(_ track: Track, project: Project, format: AVAudioFormat) -> AVAudioPCMBuffer {
    let patch = track.synth!
    let rate = project.sample_rate
    let secondsPerBeat = 60 / project.bpm
    let duration = project.beats * secondsPerBeat + project.tail_seconds
    let length = Int((duration * rate).rounded())
    let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(length))!
    buffer.frameLength = AVAudioFrameCount(length)
    let data = buffer.floatChannelData!
    for side in 0..<2 { data[side].initialize(repeating: 0, count: length) }
    let pan = Double(track.pan) / 127 * .pi / 2
    let volume = pow(Double(track.volume) / 127, 2) * patch.gain
    let musicalLength = project.beats * secondsPerBeat
    for note in track.notes {
        let start = Int((note[0] * secondsPerBeat * rate).rounded())
        let held = note[1] * secondsPerBeat
        let count = min(length - start, Int(((held + patch.release) * rate).rounded()))
        let frequency = 440 * pow(2, (note[2] - 69) / 12)
        let velocity = pow(note[3] / 127, 1.4)
        // Synthesize at twice the output rate. Finite harmonics avoid discontinuous
        // saw edges; low-pass after saturation, then average pairs when downsampling.
        for side in 0..<2 {
            let detune = pow(2, (side == 0 ? -1 : 1) * patch.detune_cents / 1200)
            let hz = frequency * detune
            let harmonics = min(24, Int(rate * 0.4 / hz))
            let scale = volume * velocity * (side == 0 ? cos(pan) : sin(pan))
            var filter1 = 0.0
            var filter2 = 0.0
            for frame in 0..<count {
                var sum = 0.0
                for subframe in 0..<2 {
                    let t = (Double(frame) + Double(subframe) / 2) / rate
                    let global = Double(start) / rate + t
                    let progress = min(1, global / musicalLength)
                    let sweep = patch.cutoff_start * pow(patch.cutoff_end / patch.cutoff_start,
                                                        progress)
                    let motion = 1 + patch.motion_depth * sin(2 * .pi * patch.motion_hz * global)
                    let pluck = patch.kind == "pulse" ? 1 + 2 * exp(-t / 0.075) : 1
                    let cutoff = min(rate * 0.2, sweep * motion * pluck)
                    let phase = 2 * .pi * hz * t
                    var tone = 0.0
                    if harmonics > 0 {
                        for harmonic in 1...harmonics {
                            // A closing harmonic spectrum is also the pre-drive filter.
                            let h = Double(harmonic)
                            let weight = 1 / (h * sqrt(1 + pow(h * hz / cutoff, 4)))
                            tone += sin(h * phase) * weight
                        }
                    }
                    let shaped = tanh(patch.drive * tone * 0.65) / tanh(patch.drive)
                    let coefficient = 1 - exp(-2 * .pi * cutoff / (2 * rate))
                    filter1 += coefficient * (shaped - filter1)
                    filter2 += coefficient * (filter1 - filter2)
                    // Shared-phase sub remains centred and undistorted.
                    let sub = sin(2 * .pi * frequency * t)
                    let attack = min(1, t / patch.attack)
                    let release = max(0, min(1, (held + patch.release - t) / patch.release))
                    let contour = patch.kind == "pulse" ? 0.30 + 0.70 * exp(-t / 0.16) : 1
                    let envelope = attack * release * release * contour
                    sum += ((1 - patch.sub_mix) * filter2 + patch.sub_mix * sub) * envelope
                }
                data[side][start + frame] += Float(sum * 0.5 * scale)
            }
        }
    }
    return buffer
}

struct Project: Decodable {
    let bpm: Double
    let beats: Double
    let tail_seconds: Double
    let sample_rate: Double
    let master_gain: Float
    let reverb_percent: Float
    let tracks: [Track]
}

struct Event {
    let frame: Int64
    let track: Int
    let pitch: UInt8
    let velocity: UInt8 // zero = note off
}

enum RenderError: Error {
    case failed(String)
}

func render() throws {
    guard CommandLine.arguments.count == 4 else {
        throw RenderError.failed("usage: render project.json bank.dls output.wav")
    }
    let project = try JSONDecoder().decode(Project.self, from: Data(contentsOf:
        URL(fileURLWithPath: CommandLine.arguments[1])))
    let bank = URL(fileURLWithPath: CommandLine.arguments[2])
    // Sandboxed command runners may hide system Audio Units. Report that before
    // AVAudioUnit initializers raise an Objective-C exception.
    for (type, subtype) in [(kAudioUnitType_MusicDevice, kAudioUnitSubType_Sampler),
                            (kAudioUnitType_Effect, kAudioUnitSubType_Reverb2)] {
        var description = AudioComponentDescription(componentType: type,
            componentSubType: subtype, componentManufacturer: kAudioUnitManufacturer_Apple,
            componentFlags: 0, componentFlagsMask: 0)
        guard AudioComponentFindNext(nil, &description) != nil else {
            throw RenderError.failed("Apple Audio Units unavailable; run outside the sandbox")
        }
    }
    let engine = AVAudioEngine()
    let format = AVAudioFormat(standardFormatWithSampleRate: project.sample_rate, channels: 2)!
    let reverb = AVAudioUnitReverb()
    engine.attach(reverb)
    reverb.loadFactoryPreset(.mediumHall)
    reverb.wetDryMix = project.reverb_percent
    // A separate bus sums the instruments before the shared hall.
    let bus = AVAudioMixerNode()
    engine.attach(bus)
    engine.connect(bus, to: reverb, format: format)
    engine.connect(reverb, to: engine.mainMixerNode, format: format)
    engine.mainMixerNode.outputVolume = project.master_gain

    var samplers: [AVAudioUnitSampler?] = []
    var players: [AVAudioPlayerNode] = []
    var events: [Event] = []
    let framesPerBeat = project.sample_rate * 60 / project.bpm
    for (index, track) in project.tracks.enumerated() {
        if track.synth != nil {
            let player = AVAudioPlayerNode()
            engine.attach(player)
            engine.connect(player, to: bus, format: format)
            player.scheduleBuffer(synthesize(track, project: project, format: format))
            players.append(player)
            samplers.append(nil)
            continue
        }
        let sampler = AVAudioUnitSampler()
        engine.attach(sampler)
        engine.connect(sampler, to: bus, format: format)
        try sampler.loadSoundBankInstrument(at: bank, program: track.program!,
            bankMSB: track.channel == 9 ? UInt8(kAUSampler_DefaultPercussionBankMSB)
                                       : UInt8(kAUSampler_DefaultMelodicBankMSB),
            bankLSB: UInt8(kAUSampler_DefaultBankLSB))
        sampler.sendController(7, withValue: track.volume, onChannel: track.channel)
        sampler.sendController(10, withValue: track.pan, onChannel: track.channel)
        // Disable bank-level ambience; the explicit shared hall owns the effect.
        sampler.sendController(91, withValue: 0, onChannel: track.channel)
        sampler.sendController(93, withValue: 0, onChannel: track.channel)
        samplers.append(sampler)
        for note in track.notes {
            events.append(Event(frame: Int64((note[0] * framesPerBeat).rounded()),
                track: index, pitch: UInt8(note[2]), velocity: UInt8(note[3])))
            events.append(Event(frame: Int64(((note[0] + note[1]) * framesPerBeat).rounded()),
                track: index, pitch: UInt8(note[2]), velocity: 0))
        }
    }
    // Note-offs precede new attacks at the same frame; tie order is stable.
    events.sort {
        if $0.frame != $1.frame { return $0.frame < $1.frame }
        if $0.velocity != $1.velocity { return $0.velocity < $1.velocity }
        if $0.track != $1.track { return $0.track < $1.track }
        return $0.pitch < $1.pitch
    }
    try engine.enableManualRenderingMode(.offline, format: format, maximumFrameCount: 1024)
    try engine.start()
    for player in players { player.play() }
    defer { engine.stop() }
    let output = try AVAudioFile(forWriting: URL(fileURLWithPath: CommandLine.arguments[3]),
        settings: [AVFormatIDKey: kAudioFormatLinearPCM,
                   AVSampleRateKey: project.sample_rate, AVNumberOfChannelsKey: 2,
                   AVLinearPCMBitDepthKey: 16, AVLinearPCMIsFloatKey: false,
                   AVLinearPCMIsBigEndianKey: false])
    let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 1024)!
    let end = Int64((project.beats * framesPerBeat
        + project.tail_seconds * project.sample_rate).rounded())
    var cursor: Int64 = 0
    var next = 0
    var retries = 0
    var peak: Float = 0
    while cursor < end {
        while next < events.count && events[next].frame <= cursor {
            let event = events[next]
            let sampler = samplers[event.track]!
            let channel = project.tracks[event.track].channel
            if event.velocity == 0 {
                sampler.stopNote(event.pitch, onChannel: channel)
            } else {
                sampler.startNote(event.pitch, withVelocity: event.velocity, onChannel: channel)
            }
            next += 1
        }
        let boundary = next < events.count ? min(end, events[next].frame) : end
        let count = AVAudioFrameCount(min(1024, boundary - cursor))
        let status = try engine.renderOffline(count, to: buffer)
        switch status {
        case .success:
            guard buffer.frameLength == count else {
                throw RenderError.failed("short offline render")
            }
            for channel in 0..<2 {
                for frame in 0..<Int(count) {
                    let value = buffer.floatChannelData![channel][frame]
                    guard value.isFinite else { throw RenderError.failed("non-finite PCM") }
                    peak = max(peak, abs(value))
                }
            }
            try output.write(from: buffer)
            cursor += Int64(count)
            retries = 0
        case .cannotDoInCurrentContext:
            retries += 1
            guard retries < 100 else { throw RenderError.failed("offline render stalled") }
        default:
            throw RenderError.failed("offline rendering status: \(status.rawValue)")
        }
    }
    guard peak > 0.0001 && peak < 0.99 else {
        throw RenderError.failed("silent or overloaded mix (peak \(peak)); adjust master_gain")
    }
    print(String(format: "Rendered %.3f s; float peak %.2f dBFS", Double(end) / project.sample_rate,
        20 * log10(peak)))
}

do {
    try render()
} catch {
    FileHandle.standardError.write(Data("render: \(error)\n".utf8))
    exit(1)
}
