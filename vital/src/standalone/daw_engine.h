#pragma once

#include "JuceHeader.h"
#include "synth_base.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class DawHost {
  public:
    virtual ~DawHost() { }
    virtual std::string getGuiSynthState() = 0;
    virtual bool setGuiSynthState(const std::string& state) = 0;
    virtual void initGuiSynth() = 0;
    virtual int getComputerKeyboardOffset() = 0;
    virtual void setComputerKeyboardOffset(int offset) = 0;
    virtual const CriticalSection& getAudioLock() = 0;
    virtual String getGuiPresetName() = 0;
    virtual void renderOffline(AudioSampleBuffer& buffer, int num_samples) = 0;
};

class DawSynth : public HeadlessSynth {
  public:
    DawSynth();

    void prepare(double sample_rate);
    void render(AudioSampleBuffer& buffer, MidiBuffer& midi, int num_samples, float bpm);
    std::string getState();
    bool setState(const std::string& state);
    void initPreset();
    String presetName() { return getPresetName(); }

  private:
    double time_ = 0.0;
    double sample_rate_ = 44100.0;
    int silent_samples_ = 0;
    bool sleeping_ = false;
};

class DawEngine {
  public:
    static constexpr int kPpq = 960;
    static constexpr int kMaxTracks = 16;
    static constexpr int kMaxSynths = 6;
    static constexpr int kMaxKits = 4;
    static constexpr int kNumPads = 16;
    static constexpr int kFirstPadNote = 36;
    static constexpr int kMaxVoices = 48;
    static constexpr int kMaxActiveNotes = 512;
    static constexpr int kStatusSize = 64;
    static constexpr int kMaxBlock = 4096;

    enum TrackType { kNone = 0, kSynth = 1, kDrums = 2 };

    enum StatusIndex {
      kStatusPosition = 0,
      kStatusPlaying,
      kStatusRecording,
      kStatusCounting,
      kStatusLatency,
      kStatusSampleRate,
      kStatusCaptureFrames,
      kStatusMasterL,
      kStatusMasterR,
      kStatusRecordedAvailable,
      kStatusTrackMeters = 16
    };

    struct SeqEvent {
      int start;
      int length;
      int track;
      int note;
      int velocity;
    };

    DawEngine(DawHost* host);
    ~DawEngine();

    static DawEngine* instance;

    void prepare(double sample_rate);
    void beginBlock(MidiBuffer& live, int num_samples, MidiBuffer& gui_out);
    void endBlock(AudioSampleBuffer& buffer, int num_samples);
    void setLatency(int samples) { latency_samples_ = samples; }

    double* status() { return status_; }
    double bpm() const { return bpm_; }
    void play(double tick);
    void stop();
    void locate(double tick);
    void setRecording(bool on, int count_in_bars);
    void setBpm(double bpm) { bpm_ = std::max(20.0, std::min(400.0, bpm)); }
    void setBeatsPerBar(int beats) { beats_per_bar_ = std::max(1, std::min(16, beats)); }
    void setLoop(bool on, double start, double end);
    void setMetronome(bool on) { metronome_ = on; }
    void setMaster(float volume) { master_volume_ = volume; }
    void setNumTracks(int num) { num_tracks_ = std::max(0, std::min(kMaxTracks, num)); }
    void setTrack(int index, int type, int slot, float volume, float pan, bool mute, bool solo);
    void setSelectedTrack(int index) { selected_track_ = index; }
    void setEvents(const int* data, int count);
    void liveNote(int note, int velocity, bool on);
    int pollRecorded(double* out, int max_events);
    void setPadSample(int kit, int pad, const float* data, int frames, int channels, double rate);
    void setPad(int kit, int pad, float gain, float pan, float pitch, int choke);
    bool ensureSynth(int slot);
    void releaseSynth(int slot);
    std::string getSynthState(int slot);
    bool setSynthState(int slot, const std::string& state);
    void initSynth(int slot);
    String getSynthName(int slot);
    void startCapture(int max_frames);
    int stopCapture();
    float* captureData(int channel);
    void freeCapture();
    void panic() { panic_requested_ = true; }
    int keyboardOffset(int offset);

    // FLWeb host: ogni traccia (0..kMaxSynths-1) esce su una sua coppia di canali, note a frame esatto
    void hostEnable(int pairs);
    void hostNote(int track, int note, int velocity, bool on, int frame);
    void offlineBegin();
    void offlineEnd();
    int offlineRender(float* out, int frames);
    bool offlineActive() const { return offline_; }

  private:
    struct Track {
      std::atomic<int> type { kNone };
      std::atomic<int> slot { 0 };
      std::atomic<float> volume { 1.0f };
      std::atomic<float> pan { 0.0f };
      std::atomic<bool> mute { false };
      std::atomic<bool> solo { false };
      float peak_l = 0.0f;
      float peak_r = 0.0f;
    };

    struct PadSample {
      std::vector<float> left;
      std::vector<float> right;
      double rate = 44100.0;
    };

    struct Pad {
      std::shared_ptr<const PadSample> sample;
      std::atomic<float> gain { 1.0f };
      std::atomic<float> pan { 0.0f };
      std::atomic<float> pitch { 0.0f };
      std::atomic<int> choke { 0 };
    };

    struct Voice {
      bool active = false;
      int track = 0;
      int kit = 0;
      int pad = 0;
      int choke = 0;
      int delay = 0;
      double position = 0.0;
      double increment = 1.0;
      float gain_l = 1.0f;
      float gain_r = 1.0f;
      float fade = 1.0f;
      float fade_step = 0.0f;
      std::shared_ptr<const PadSample> sample;
      uint64_t age = 0;
    };

    struct ActiveNote {
      int track;
      int slot;
      int note;
      double end;
    };

    struct RecEvent {
      int on;
      int note;
      int velocity;
      double tick;
    };

    struct LiveEvent {
      int note;
      int velocity;
      int on;
    };

    struct Click {
      int offset;
      bool accent;
    };

    bool trackMuted(int track) const;
    void routeNoteOn(int track, int note, int velocity, int sample, MidiBuffer& gui_out);
    void routeNoteOff(int track, int slot, int note, int sample, MidiBuffer& gui_out);
    void handleLive(int note, int velocity, bool on, int sample, double tick, MidiBuffer& gui_out, MidiBuffer& live_pass);
    void triggerPad(int track, int kit, int pad, int velocity, int sample);
    void allNotesOff(int sample, MidiBuffer& gui_out);
    void sequenceRange(double from, double to, int sample_offset, double ticks_per_sample, MidiBuffer& gui_out);
    void scheduleClicks(double from, double to, int sample_offset, double ticks_per_sample);
    void renderDrums(int track, AudioSampleBuffer& out, int num_samples);
    void renderClicks(AudioSampleBuffer& out, int num_samples);
    void meter(int track, const AudioSampleBuffer& buffer, int num_samples);
    MidiBuffer& synthMidi(int slot) { return synth_midi_[slot]; }

    DawHost* host_;
    double sample_rate_ = 44100.0;
    double status_[kStatusSize] = {};
    std::atomic<int> latency_samples_ { 0 };

    std::atomic<double> bpm_ { 120.0 };
    std::atomic<int> beats_per_bar_ { 4 };
    std::atomic<bool> playing_ { false };
    std::atomic<bool> recording_ { false };
    std::atomic<bool> metronome_ { false };
    std::atomic<bool> loop_ { false };
    std::atomic<double> loop_start_ { 0.0 };
    std::atomic<double> loop_end_ { 4.0 * 4 * kPpq };
    std::atomic<float> master_volume_ { 1.0f };
    std::atomic<int> num_tracks_ { 0 };
    std::atomic<int> selected_track_ { 0 };
    std::atomic<bool> panic_requested_ { false };
    std::atomic<bool> stop_requested_ { false };
    std::atomic<double> seek_request_ { -1e18 };
    std::atomic<bool> start_request_ { false };
    std::atomic<int> count_in_bars_ { 0 };
    double position_ = 0.0;
    double record_start_ = 0.0;
    bool was_playing_ = false;

    Track tracks_[kMaxTracks];
    Pad pads_[kMaxKits][kNumPads];
    SpinLock pad_lock_;

    std::vector<SeqEvent> events_;
    SpinLock events_lock_;

    ActiveNote active_[kMaxActiveNotes];
    int num_active_ = 0;
    Voice voices_[kMaxVoices];
    uint64_t voice_counter_ = 0;

    int live_route_[128];
    Click clicks_[16];
    int num_clicks_ = 0;
    double click_phase_ = 0.0;
    double click_freq_ = 1000.0;
    float click_amp_ = 0.0f;
    int click_delay_ = -1;
    bool click_pending_accent_ = false;

    AbstractFifo rec_fifo_ { 4096 };
    RecEvent rec_events_[4096];
    AbstractFifo live_fifo_ { 1024 };
    LiveEvent live_events_[1024];

    std::unique_ptr<DawSynth> synths_[kMaxSynths];
    MidiBuffer synth_midi_[kMaxSynths];
    AudioSampleBuffer scratch_;
    AudioSampleBuffer mix_;

    struct HostEvent {
      int track;
      int note;
      int velocity;
      int on;
      int frame;
    };
    void dispatchHost(int base, int num_samples, MidiBuffer& gui_out);
    void endBlockHost(AudioSampleBuffer& buffer, int num_samples);
    std::atomic<int> host_pairs_ { 0 };
    AbstractFifo host_fifo_ { 8192 };
    HostEvent host_events_[8192];
    std::vector<HostEvent> host_pending_;
    std::atomic<bool> offline_ { false };
    bool offline_rendering_ = false;
    int offline_pos_ = 0;

    std::vector<float> capture_[2];
    std::atomic<bool> capturing_ { false };
    std::atomic<int> capture_frames_ { 0 };
    int capture_max_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DawEngine)
};
