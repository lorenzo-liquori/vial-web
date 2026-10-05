#include "daw_engine.h"

#include "sound_engine.h"
#include "synth_constants.h"
#include "load_save.h"

#include <algorithm>
#include <cmath>
#include <cstring>

DawEngine* DawEngine::instance = nullptr;
extern std::atomic<int> flweb_render_position;

DawSynth::DawSynth() { }

void DawSynth::prepare(double sample_rate) {
  ScopedLock lock(getCriticalSection());
  sample_rate_ = sample_rate;
  engine_->setSampleRate(sample_rate);
  engine_->updateAllModulationSwitches();
  midi_manager_->setSampleRate(sample_rate);
}

void DawSynth::render(AudioSampleBuffer& buffer, MidiBuffer& midi, int num_samples, float bpm) {
  ScopedLock lock(getCriticalSection());

  if (!midi.isEmpty()) {
    sleeping_ = false;
    silent_samples_ = 0;
  }

  if (sleeping_) {
    time_ += num_samples / sample_rate_;
    return;
  }

  processModulationChanges();
  engine_->setBpm(bpm);

  int chunk = std::min(num_samples, vital::kMaxBufferSize);
  for (int b = 0; b < num_samples; b += chunk) {
    int current = std::min(chunk, num_samples - b);
    engine_->correctToTime(time_);
    processMidi(midi, b, b + current);
    processAudio(&buffer, vital::kNumChannels, current, b);
    time_ += current / sample_rate_;
  }

  float peak = std::max(buffer.getMagnitude(0, 0, num_samples), buffer.getMagnitude(1, 0, num_samples));
  if (peak < 1e-5f && engine_->getNumActiveVoices() == 0) {
    silent_samples_ += num_samples;
    if (silent_samples_ > sample_rate_ * 3.0)
      sleeping_ = true;
  }
  else
    silent_samples_ = 0;
}

std::string DawSynth::getState() {
  return saveToJson().dump();
}

bool DawSynth::setState(const std::string& state) {
  try {
    json parsed = json::parse(state, nullptr, false);
    if (parsed.is_discarded())
      return false;
    bool result = loadFromJson(parsed);
    sleeping_ = false;
    silent_samples_ = 0;
    return result;
  }
  catch (...) {
    return false;
  }
}

void DawSynth::initPreset() {
  loadInitPreset();
  sleeping_ = false;
  silent_samples_ = 0;
}

DawEngine::DawEngine(DawHost* host) : host_(host) {
  scratch_.setSize(2, kMaxBlock);
  mix_.setSize(2, kMaxBlock);
  for (auto& midi : synth_midi_)
    midi.ensureSize(8192);
  status_[kStatusSampleRate] = sample_rate_;
  for (int& route : live_route_)
    route = -1;
}

DawEngine::~DawEngine() {
  if (instance == this)
    instance = nullptr;
}

void DawEngine::prepare(double sample_rate) {
  sample_rate_ = sample_rate;
  status_[kStatusSampleRate] = sample_rate;
  for (int i = 1; i < kMaxSynths; ++i) {
    if (synths_[i])
      synths_[i]->prepare(sample_rate);
  }
}

void DawEngine::play(double tick) {
  double start = tick;
  if (recording_ && count_in_bars_ > 0)
    start = tick - count_in_bars_ * beats_per_bar_ * (double)kPpq;
  record_start_ = tick;
  seek_request_ = start;
  playing_ = true;
}

void DawEngine::stop() {
  playing_ = false;
  recording_ = false;
  stop_requested_ = true;
}

void DawEngine::locate(double tick) {
  seek_request_ = tick;
}

void DawEngine::setRecording(bool on, int count_in_bars) {
  count_in_bars_ = std::max(0, count_in_bars);
  recording_ = on;
}

void DawEngine::setLoop(bool on, double start, double end) {
  loop_start_ = start;
  loop_end_ = std::max(start + 1.0, end);
  loop_ = on;
}

void DawEngine::setTrack(int index, int type, int slot, float volume, float pan, bool mute, bool solo) {
  if (index < 0 || index >= kMaxTracks)
    return;
  Track& track = tracks_[index];
  track.type = type;
  track.slot = slot;
  track.volume = volume;
  track.pan = pan;
  track.mute = mute;
  track.solo = solo;
}

void DawEngine::setEvents(const int* data, int count) {
  std::vector<SeqEvent> events;
  events.reserve(count);
  for (int i = 0; i < count; ++i) {
    const int* e = data + i * 5;
    SeqEvent event { e[0], std::max(1, e[1]), e[2], e[3], std::max(1, std::min(127, e[4])) };
    if (event.track >= 0 && event.track < kMaxTracks && event.note >= 0 && event.note < 128)
      events.push_back(event);
  }
  std::stable_sort(events.begin(), events.end(), [](const SeqEvent& a, const SeqEvent& b) { return a.start < b.start; });

  {
    SpinLock::ScopedLockType lock(events_lock_);
    events_.swap(events);
  }
}

void DawEngine::liveNote(int note, int velocity, bool on) {
  int start1, size1, start2, size2;
  live_fifo_.prepareToWrite(1, start1, size1, start2, size2);
  if (size1 > 0)
    live_events_[start1] = { note, velocity, on ? 1 : 0 };
  else if (size2 > 0)
    live_events_[start2] = { note, velocity, on ? 1 : 0 };
  live_fifo_.finishedWrite(size1 + size2);
}

int DawEngine::pollRecorded(double* out, int max_events) {
  int start1, size1, start2, size2;
  rec_fifo_.prepareToRead(max_events, start1, size1, start2, size2);
  int n = 0;
  for (int i = 0; i < size1; ++i, ++n) {
    const RecEvent& e = rec_events_[start1 + i];
    out[n * 4] = e.on;
    out[n * 4 + 1] = e.note;
    out[n * 4 + 2] = e.velocity;
    out[n * 4 + 3] = e.tick;
  }
  for (int i = 0; i < size2; ++i, ++n) {
    const RecEvent& e = rec_events_[start2 + i];
    out[n * 4] = e.on;
    out[n * 4 + 1] = e.note;
    out[n * 4 + 2] = e.velocity;
    out[n * 4 + 3] = e.tick;
  }
  rec_fifo_.finishedRead(size1 + size2);
  return n;
}

void DawEngine::setPadSample(int kit, int pad, const float* data, int frames, int channels, double rate) {
  if (kit < 0 || kit >= kMaxKits || pad < 0 || pad >= kNumPads)
    return;

  std::shared_ptr<const PadSample> next;
  if (data && frames > 0) {
    auto sample = std::make_shared<PadSample>();
    sample->rate = rate > 0.0 ? rate : 44100.0;
    sample->left.assign(data, data + frames);
    if (channels > 1)
      sample->right.assign(data + frames, data + 2 * frames);
    sample->left.push_back(0.0f);
    if (!sample->right.empty())
      sample->right.push_back(0.0f);
    next = sample;
  }

  std::shared_ptr<const PadSample> old;
  {
    SpinLock::ScopedLockType lock(pad_lock_);
    old = std::move(pads_[kit][pad].sample);
    pads_[kit][pad].sample = next;
  }
}

void DawEngine::setPad(int kit, int pad, float gain, float pan, float pitch, int choke) {
  if (kit < 0 || kit >= kMaxKits || pad < 0 || pad >= kNumPads)
    return;
  Pad& p = pads_[kit][pad];
  p.gain = gain;
  p.pan = pan;
  p.pitch = pitch;
  p.choke = choke;
}

bool DawEngine::ensureSynth(int slot) {
  if (slot == 0)
    return true;
  if (slot < 0 || slot >= kMaxSynths)
    return false;
  if (synths_[slot])
    return true;

  auto synth = std::make_unique<DawSynth>();
  synth->prepare(sample_rate_);
  synth->initPreset();

  ScopedLock lock(host_->getAudioLock());
  synths_[slot] = std::move(synth);
  return true;
}

void DawEngine::releaseSynth(int slot) {
  if (slot <= 0 || slot >= kMaxSynths)
    return;
  std::unique_ptr<DawSynth> old;
  {
    ScopedLock lock(host_->getAudioLock());
    old = std::move(synths_[slot]);
  }
}

std::string DawEngine::getSynthState(int slot) {
  if (slot == 0)
    return host_->getGuiSynthState();
  if (slot > 0 && slot < kMaxSynths && synths_[slot])
    return synths_[slot]->getState();
  return {};
}

bool DawEngine::setSynthState(int slot, const std::string& state) {
  if (slot == 0)
    return host_->setGuiSynthState(state);
  if (slot > 0 && slot < kMaxSynths && ensureSynth(slot))
    return synths_[slot]->setState(state);
  return false;
}

void DawEngine::initSynth(int slot) {
  if (slot == 0)
    host_->initGuiSynth();
  else if (slot > 0 && slot < kMaxSynths && ensureSynth(slot))
    synths_[slot]->initPreset();
}

String DawEngine::getSynthName(int slot) {
  if (slot == 0)
    return host_->getGuiPresetName();
  if (slot > 0 && slot < kMaxSynths && synths_[slot])
    return synths_[slot]->presetName();
  return {};
}

void DawEngine::startCapture(int max_frames) {
  capturing_ = false;
  {
    ScopedLock lock(host_->getAudioLock());
    capture_[0].assign((size_t)std::max(0, max_frames), 0.0f);
    capture_[1].assign((size_t)std::max(0, max_frames), 0.0f);
    capture_max_ = max_frames;
    capture_frames_ = 0;
  }
  capturing_ = true;
}

int DawEngine::stopCapture() {
  ScopedLock lock(host_->getAudioLock());
  capturing_ = false;
  return capture_frames_;
}

float* DawEngine::captureData(int channel) {
  if (channel < 0 || channel > 1 || capture_[channel].empty())
    return nullptr;
  return capture_[channel].data();
}

void DawEngine::freeCapture() {
  ScopedLock lock(host_->getAudioLock());
  capturing_ = false;
  capture_[0] = std::vector<float>();
  capture_[1] = std::vector<float>();
  capture_max_ = 0;
  capture_frames_ = 0;
}

int DawEngine::keyboardOffset(int offset) {
  int previous = host_->getComputerKeyboardOffset();
  if (offset >= 0)
    host_->setComputerKeyboardOffset(offset);
  return previous;
}

bool DawEngine::trackMuted(int track) const {
  if (track < 0 || track >= num_tracks_)
    return true;
  if (tracks_[track].mute)
    return true;
  bool any_solo = false;
  for (int i = 0; i < num_tracks_; ++i)
    any_solo = any_solo || (tracks_[i].solo && tracks_[i].type != kNone);
  return any_solo && !tracks_[track].solo;
}

void DawEngine::triggerPad(int track, int kit, int pad, int velocity, int sample) {
  if (kit < 0 || kit >= kMaxKits || pad < 0 || pad >= kNumPads)
    return;

  std::shared_ptr<const PadSample> data;
  {
    SpinLock::ScopedLockType lock(pad_lock_);
    data = pads_[kit][pad].sample;
  }
  if (!data || data->left.size() < 2)
    return;

  const Pad& p = pads_[kit][pad];
  int choke = p.choke;
  if (choke > 0) {
    for (Voice& v : voices_) {
      if (v.active && v.track == track && v.kit == kit && v.choke == choke && v.fade_step == 0.0f) {
        if (v.delay > sample) {
          v.active = false;
          v.sample.reset();
        }
        else
          v.fade_step = 1.0f / (float)(0.006 * sample_rate_);
      }
    }
  }

  Voice* target = nullptr;
  for (Voice& v : voices_) {
    if (!v.active) {
      target = &v;
      break;
    }
  }
  if (target == nullptr) {
    target = &voices_[0];
    for (Voice& v : voices_) {
      if (v.age < target->age)
        target = &v;
    }
  }

  float gain = p.gain * (velocity / 127.0f);
  float pan = p.pan;
  target->active = true;
  target->track = track;
  target->kit = kit;
  target->pad = pad;
  target->choke = choke;
  target->delay = std::max(0, sample);
  target->position = 0.0;
  target->increment = (data->rate / sample_rate_) * std::pow(2.0, p.pitch / 12.0);
  target->gain_l = gain * std::min(1.0f, 1.0f - pan);
  target->gain_r = gain * std::min(1.0f, 1.0f + pan);
  target->fade = 1.0f;
  target->fade_step = 0.0f;
  target->sample = data;
  target->age = ++voice_counter_;
}

void DawEngine::routeNoteOn(int track, int note, int velocity, int sample, MidiBuffer& gui_out) {
  if (track < 0 || track >= num_tracks_ || trackMuted(track))
    return;

  const Track& t = tracks_[track];
  int type = t.type;
  int slot = t.slot;

  if (type == kSynth) {
    MidiMessage message = MidiMessage::noteOn(1, note, (uint8)velocity);
    if (slot == 0)
      gui_out.addEvent(message, sample);
    else if (slot > 0 && slot < kMaxSynths && synths_[slot])
      synth_midi_[slot].addEvent(message, sample);
  }
  else if (type == kDrums) {
    int pad = note - kFirstPadNote;
    if (pad >= 0 && pad < kNumPads)
      triggerPad(track, slot, pad, velocity, sample);
  }
}

void DawEngine::routeNoteOff(int track, int slot, int note, int sample, MidiBuffer& gui_out) {
  MidiMessage message = MidiMessage::noteOff(1, note);
  if (slot == 0)
    gui_out.addEvent(message, sample);
  else if (slot > 0 && slot < kMaxSynths && synths_[slot])
    synth_midi_[slot].addEvent(message, sample);
}

void DawEngine::allNotesOff(int sample, MidiBuffer& gui_out) {
  for (int i = 0; i < num_active_; ++i)
    routeNoteOff(active_[i].track, active_[i].slot, active_[i].note, sample, gui_out);
  num_active_ = 0;
}

void DawEngine::handleLive(int note, int velocity, bool on, int sample, double tick,
                           MidiBuffer& gui_out, MidiBuffer& live_pass) {
  int selected = selected_track_;
  bool valid = selected >= 0 && selected < num_tracks_;
  bool drums = valid && tracks_[selected].type == kDrums;
  note = std::max(0, std::min(127, note));

  if (on) {
    if (drums) {
      int pad = ((note - kFirstPadNote) % kNumPads + kNumPads) % kNumPads;
      triggerPad(selected, tracks_[selected].slot, pad, velocity, sample);
      live_route_[note] = 100;
    }
    else {
      int slot = valid && tracks_[selected].type == kSynth ? tracks_[selected].slot.load() : 0;
      if (slot < 0 || slot >= kMaxSynths || (slot > 0 && !synths_[slot]))
        slot = 0;
      MidiMessage message = MidiMessage::noteOn(1, note, (uint8)std::max(1, velocity));
      if (slot == 0)
        gui_out.addEvent(message, sample);
      else
        synth_midi_[slot].addEvent(message, sample);
      live_route_[note] = slot;
    }
  }
  else {
    int route = live_route_[note];
    live_route_[note] = -1;
    if (route < 0 && !drums)
      route = 0;
    if (route >= 0 && route < kMaxSynths) {
      MidiMessage message = MidiMessage::noteOff(1, note);
      if (route == 0)
        gui_out.addEvent(message, sample);
      else if (synths_[route])
        synth_midi_[route].addEvent(message, sample);
    }
  }

  if (recording_ && playing_ && tick > -kPpq / 4.0) {
    int start1, size1, start2, size2;
    rec_fifo_.prepareToWrite(1, start1, size1, start2, size2);
    RecEvent event { on ? 1 : 0, note, velocity, std::max(0.0, tick) };
    if (size1 > 0)
      rec_events_[start1] = event;
    else if (size2 > 0)
      rec_events_[start2] = event;
    rec_fifo_.finishedWrite(size1 + size2);
  }
  ignoreUnused(live_pass);
}

void DawEngine::sequenceRange(double from, double to, int sample_offset, double ticks_per_sample, MidiBuffer& gui_out) {
  for (int i = 0; i < num_active_;) {
    if (active_[i].end < to) {
      int sample = sample_offset + (int)std::max(0.0, (active_[i].end - from) / ticks_per_sample);
      routeNoteOff(active_[i].track, active_[i].slot, active_[i].note, sample, gui_out);
      active_[i] = active_[--num_active_];
    }
    else
      ++i;
  }

  if (to <= 0.0)
    return;

  SpinLock::ScopedLockType lock(events_lock_);
  double start_tick = std::max(0.0, from);
  auto it = std::lower_bound(events_.begin(), events_.end(), start_tick,
                             [](const SeqEvent& e, double t) { return e.start < t; });
  if (from <= 0.0)
    it = events_.begin();

  for (; it != events_.end() && it->start < to; ++it) {
    if (it->start < from)
      continue;
    int sample = sample_offset + (int)((it->start - from) / ticks_per_sample);
    int track = it->track;
    if (track < 0 || track >= num_tracks_ || trackMuted(track))
      continue;

    const Track& t = tracks_[track];
    if (t.type == kSynth) {
      int slot = t.slot;
      for (int a = 0; a < num_active_; ++a) {
        if (active_[a].track == track && active_[a].note == it->note) {
          routeNoteOff(active_[a].track, active_[a].slot, active_[a].note, sample, gui_out);
          active_[a] = active_[--num_active_];
          break;
        }
      }
      if (num_active_ < kMaxActiveNotes) {
        routeNoteOn(track, it->note, it->velocity, sample, gui_out);
        active_[num_active_++] = { track, slot, it->note, (double)it->start + it->length };
      }
    }
    else
      routeNoteOn(track, it->note, it->velocity, sample, gui_out);
  }
}

void DawEngine::scheduleClicks(double from, double to, int sample_offset, double ticks_per_sample) {
  bool counting = recording_ && from < 0.0;
  if (!metronome_ && !counting)
    return;

  int beats = beats_per_bar_;
  long long beat = (long long)std::ceil(from / kPpq);
  for (; beat * (double)kPpq < to && num_clicks_ < 16; ++beat) {
    double tick = beat * (double)kPpq;
    if (!metronome_ && tick >= 0.0)
      break;
    int sample = sample_offset + (int)((tick - from) / ticks_per_sample);
    long long in_bar = ((beat % beats) + beats) % beats;
    clicks_[num_clicks_++] = { sample, in_bar == 0 };
  }
}

void DawEngine::beginBlock(MidiBuffer& live, int num_samples, MidiBuffer& gui_out) {
  gui_out.clear();
  for (auto& midi : synth_midi_)
    midi.clear();
  num_clicks_ = 0;

  double ticks_per_sample = bpm_ / 60.0 * kPpq / sample_rate_;

  if (panic_requested_.exchange(false)) {
    allNotesOff(0, gui_out);
    for (Voice& v : voices_) {
      v.active = false;
      v.sample.reset();
    }
    gui_out.addEvent(MidiMessage::allNotesOff(1), 0);
    for (int slot = 1; slot < kMaxSynths; ++slot) {
      if (synths_[slot])
        synth_midi_[slot].addEvent(MidiMessage::allNotesOff(1), 0);
    }
    for (int& route : live_route_)
      route = -1;
  }

  if (host_pairs_ > 0)
    dispatchHost(offline_rendering_ ? offline_pos_ : flweb_render_position.load(), num_samples, gui_out);

  double seek = seek_request_.exchange(-1e18);
  if (seek > -1e17) {
    allNotesOff(0, gui_out);
    position_ = seek;
  }

  if (stop_requested_.exchange(false))
    allNotesOff(0, gui_out);

  bool playing = playing_;
  if (!playing && was_playing_)
    allNotesOff(0, gui_out);
  was_playing_ = playing;

  double latency_ticks = latency_samples_ * ticks_per_sample;
  double loop_start = loop_start_, loop_end = loop_end_;
  bool loop = loop_ && loop_end > loop_start;

  MidiBuffer pass;
  for (const MidiMessageMetadata metadata : live) {
    MidiMessage message = metadata.getMessage();
    int sample = std::max(0, std::min(num_samples - 1, metadata.samplePosition));
    double tick = position_ + sample * ticks_per_sample - latency_ticks;
    if (loop && position_ >= loop_start && position_ < loop_end && tick < loop_start)
      tick += loop_end - loop_start;

    if (message.isNoteOn())
      handleLive(message.getNoteNumber(), message.getVelocity(), true, sample, tick, gui_out, pass);
    else if (message.isNoteOff())
      handleLive(message.getNoteNumber(), message.getVelocity(), false, sample, tick, gui_out, pass);
    else {
      int selected = selected_track_;
      int slot = 0;
      if (selected >= 0 && selected < num_tracks_ && tracks_[selected].type == kSynth)
        slot = tracks_[selected].slot;
      if (slot > 0 && slot < kMaxSynths && synths_[slot])
        synth_midi_[slot].addEvent(message, sample);
      else
        gui_out.addEvent(message, sample);
    }
  }

  int start1, size1, start2, size2;
  live_fifo_.prepareToRead(live_fifo_.getNumReady(), start1, size1, start2, size2);
  for (int i = 0; i < size1 + size2; ++i) {
    const LiveEvent& e = i < size1 ? live_events_[start1 + i] : live_events_[start2 + i - size1];
    double tick = position_ - latency_ticks;
    if (loop && position_ >= loop_start && position_ < loop_end && tick < loop_start)
      tick += loop_end - loop_start;
    handleLive(e.note, e.velocity, e.on != 0, 0, tick, gui_out, pass);
  }
  live_fifo_.finishedRead(size1 + size2);

  if (playing) {
    double from = position_;
    int done = 0;
    int guard = 0;
    while (done < num_samples && guard++ < 8) {
      double to = from + (num_samples - done) * ticks_per_sample;
      if (loop && from < loop_end && to >= loop_end) {
        int segment = (int)std::ceil((loop_end - from) / ticks_per_sample);
        segment = std::max(0, std::min(num_samples - done, segment));
        sequenceRange(from, loop_end, done, ticks_per_sample, gui_out);
        scheduleClicks(from, loop_end, done, ticks_per_sample);
        allNotesOff(std::min(num_samples - 1, done + segment), gui_out);
        double overshoot = from + segment * ticks_per_sample - loop_end;
        done += segment;
        from = loop_start + std::max(0.0, overshoot);
      }
      else {
        sequenceRange(from, to, done, ticks_per_sample, gui_out);
        scheduleClicks(from, to, done, ticks_per_sample);
        from = to;
        done = num_samples;
      }
    }
    position_ = from;
  }
}

void DawEngine::renderDrums(int track, AudioSampleBuffer& out, int num_samples) {
  float* left = out.getWritePointer(0);
  float* right = out.getWritePointer(1);

  for (Voice& v : voices_) {
    if (!v.active || v.track != track)
      continue;

    const PadSample* data = v.sample.get();
    const float* sl = data->left.data();
    const float* sr = data->right.empty() ? sl : data->right.data();
    double length = (double)data->left.size() - 1.0;

    for (int i = v.delay; i < num_samples; ++i) {
      if (v.position >= length - 1.0) {
        v.active = false;
        break;
      }
      int index = (int)v.position;
      float frac = (float)(v.position - index);
      float l = sl[index] + (sl[index + 1] - sl[index]) * frac;
      float r = sr[index] + (sr[index + 1] - sr[index]) * frac;
      left[i] += l * v.gain_l * v.fade;
      right[i] += r * v.gain_r * v.fade;
      v.position += v.increment;
      if (v.fade_step > 0.0f) {
        v.fade -= v.fade_step;
        if (v.fade <= 0.0f) {
          v.active = false;
          break;
        }
      }
    }
    v.delay = 0;
    if (!v.active)
      v.sample.reset();
  }
}

void DawEngine::renderClicks(AudioSampleBuffer& out, int num_samples) {
  float* left = out.getWritePointer(0);
  float* right = out.getWritePointer(1);
  float decay = (float)std::exp(-1.0 / (0.03 * sample_rate_));
  int next = 0;

  for (int i = 0; i < num_samples; ++i) {
    while (next < num_clicks_ && clicks_[next].offset <= i) {
      click_amp_ = clicks_[next].accent ? 0.55f : 0.35f;
      click_freq_ = clicks_[next].accent ? 1760.0 : 1320.0;
      click_phase_ = 0.0;
      ++next;
    }
    if (click_amp_ > 1e-4f) {
      float value = (float)std::sin(click_phase_) * click_amp_;
      click_phase_ += 2.0 * MathConstants<double>::pi * click_freq_ / sample_rate_;
      click_amp_ *= decay;
      left[i] += value;
      right[i] += value;
    }
  }
}

void DawEngine::meter(int track, const AudioSampleBuffer& buffer, int num_samples) {
  float peak_l = buffer.getMagnitude(0, 0, num_samples);
  float peak_r = buffer.getMagnitude(1, 0, num_samples);
  float decay = (float)std::exp(-num_samples / (0.25 * sample_rate_));
  double* l = &status_[kStatusTrackMeters + track * 2];
  *l = std::max((double)peak_l, *l * decay);
  *(l + 1) = std::max((double)peak_r, *(l + 1) * decay);
}

void DawEngine::hostEnable(int pairs) {
  host_pairs_ = std::max(0, std::min(kMaxSynths, pairs));
  num_tracks_ = host_pairs_ > 0 ? kMaxSynths : 0;
  host_pending_.reserve(8192);
}

void DawEngine::hostNote(int track, int note, int velocity, bool on, int frame) {
  int start1, size1, start2, size2;
  host_fifo_.prepareToWrite(1, start1, size1, start2, size2);
  HostEvent event { track, note, velocity, on ? 1 : 0, frame };
  if (size1 > 0)
    host_events_[start1] = event;
  else if (size2 > 0)
    host_events_[start2] = event;
  host_fifo_.finishedWrite(size1 + size2);
}

void DawEngine::dispatchHost(int base, int num_samples, MidiBuffer& gui_out) {
  int start1, size1, start2, size2;
  host_fifo_.prepareToRead(host_fifo_.getNumReady(), start1, size1, start2, size2);
  for (int i = 0; i < size1; ++i)
    host_pending_.push_back(host_events_[start1 + i]);
  for (int i = 0; i < size2; ++i)
    host_pending_.push_back(host_events_[start2 + i]);
  host_fifo_.finishedRead(size1 + size2);
  if (host_pending_.empty())
    return;

  // prima le note in ordine di tempo; a parita' di frame il note-off prima del note-on
  std::stable_sort(host_pending_.begin(), host_pending_.end(), [](const HostEvent& a, const HostEvent& b) {
    return (a.frame - b.frame) < 0 || (a.frame == b.frame && a.on < b.on);
  });

  size_t keep = 0;
  for (size_t i = 0; i < host_pending_.size(); ++i) {
    const HostEvent& e = host_pending_[i];
    int offset = e.frame - base;
    if (offset >= num_samples) {
      host_pending_[keep++] = e;
      continue;
    }
    int sample = std::max(0, offset);
    if (e.track < 0 || e.track >= kMaxSynths || tracks_[e.track].type != kSynth)
      continue;
    int note = std::max(0, std::min(127, e.note));
    if (e.on)
      routeNoteOn(e.track, note, std::max(1, std::min(127, e.velocity)), sample, gui_out);
    else
      routeNoteOff(e.track, tracks_[e.track].slot, note, sample, gui_out);
  }
  host_pending_.resize(keep);
}

void DawEngine::endBlockHost(AudioSampleBuffer& buffer, int num_samples) {
  float bpm = (float)bpm_.load();
  int channels = buffer.getNumChannels();
  for (int c = 0; c < 2; ++c)
    scratch_.copyFrom(c, 0, buffer, std::min(c, channels - 1), 0, num_samples);
  for (int c = 0; c < channels; ++c)
    buffer.clear(c, 0, num_samples);

  int pairs = std::min((int)host_pairs_, channels / 2);
  for (int track = 0; track < kMaxSynths; ++track) {
    if (tracks_[track].type != kSynth)
      continue;
    int slot = tracks_[track].slot;
    AudioSampleBuffer* source = nullptr;
    if (slot == 0)
      source = &scratch_;
    else if (slot > 0 && slot < kMaxSynths && synths_[slot]) {
      mix_.clear(0, 0, num_samples);
      mix_.clear(1, 0, num_samples);
      synths_[slot]->render(mix_, synth_midi_[slot], num_samples, bpm);
      source = &mix_;
    }
    if (source == nullptr || track >= pairs)
      continue;
    meter(track, *source, num_samples);
    buffer.copyFrom(track * 2, 0, *source, 0, 0, num_samples);
    buffer.copyFrom(track * 2 + 1, 0, *source, 1, 0, num_samples);
  }
  status_[kStatusSampleRate] = sample_rate_;
  status_[kStatusLatency] = latency_samples_;
}

void DawEngine::offlineBegin() {
  offline_ = true;
  ScopedLock lock(host_->getAudioLock());
  offline_pos_ = 0;
  host_pending_.clear();
  panic_requested_ = true;
}

void DawEngine::offlineEnd() {
  {
    ScopedLock lock(host_->getAudioLock());
    host_pending_.clear();
    panic_requested_ = true;
  }
  offline_ = false;
}

int DawEngine::offlineRender(float* out, int frames) {
  int channels = std::max(2, (int)host_pairs_ * 2);
  std::vector<float*> pointers((size_t)channels);
  int done = 0;
  offline_rendering_ = true;
  while (done < frames) {
    int n = std::min(512, frames - done);
    for (int c = 0; c < channels; ++c)
      pointers[(size_t)c] = out + (size_t)c * frames + done;
    AudioSampleBuffer block(pointers.data(), channels, n);
    host_->renderOffline(block, n);
    offline_pos_ += n;
    done += n;
  }
  offline_rendering_ = false;
  return done;
}

void DawEngine::endBlock(AudioSampleBuffer& buffer, int num_samples) {
  num_samples = std::min(num_samples, kMaxBlock);
  if (host_pairs_ > 0) {
    endBlockHost(buffer, num_samples);
    return;
  }
  int num_tracks = num_tracks_;
  float bpm = (float)bpm_.load();

  for (int c = 0; c < 2; ++c)
    mix_.clear(c, 0, num_samples);

  auto applyTrack = [&](int track, AudioSampleBuffer& source) {
    const Track& t = tracks_[track];
    float gain = trackMuted(track) ? 0.0f : t.volume.load();
    float pan = t.pan;
    float gl = gain * std::min(1.0f, 1.0f - pan);
    float gr = gain * std::min(1.0f, 1.0f + pan);
    source.applyGain(0, 0, num_samples, gl);
    source.applyGain(1, 0, num_samples, gr);
    meter(track, source, num_samples);
    mix_.addFrom(0, 0, source, 0, 0, num_samples);
    mix_.addFrom(1, 0, source, 1, 0, num_samples);
  };

  int gui_track = -1;
  for (int i = 0; i < num_tracks; ++i) {
    if (tracks_[i].type == kSynth && tracks_[i].slot == 0) {
      gui_track = i;
      break;
    }
  }

  int channels = std::min(2, buffer.getNumChannels());
  for (int c = 0; c < 2; ++c)
    scratch_.copyFrom(c, 0, buffer, std::min(c, channels - 1), 0, num_samples);
  if (gui_track >= 0)
    applyTrack(gui_track, scratch_);
  else {
    mix_.addFrom(0, 0, scratch_, 0, 0, num_samples);
    mix_.addFrom(1, 0, scratch_, 1, 0, num_samples);
  }

  for (int slot = 1; slot < kMaxSynths; ++slot) {
    if (!synths_[slot])
      continue;
    int owner = -1;
    for (int i = 0; i < num_tracks; ++i) {
      if (tracks_[i].type == kSynth && tracks_[i].slot == slot) {
        owner = i;
        break;
      }
    }
    if (owner < 0)
      continue;

    scratch_.clear(0, 0, num_samples);
    scratch_.clear(1, 0, num_samples);
    synths_[slot]->render(scratch_, synth_midi_[slot], num_samples, bpm);
    applyTrack(owner, scratch_);
  }

  for (Voice& v : voices_) {
    if (v.active && (v.track >= num_tracks || tracks_[v.track].type != kDrums)) {
      v.active = false;
      v.sample.reset();
    }
  }

  for (int i = 0; i < num_tracks; ++i) {
    if (tracks_[i].type != kDrums)
      continue;
    scratch_.clear(0, 0, num_samples);
    scratch_.clear(1, 0, num_samples);
    renderDrums(i, scratch_, num_samples);
    applyTrack(i, scratch_);
  }

  float master = master_volume_;
  mix_.applyGain(0, num_samples, master);
  renderClicks(mix_, num_samples);

  for (int c = 0; c < 2; ++c) {
    float* data = mix_.getWritePointer(c);
    for (int i = 0; i < num_samples; ++i) {
      float x = data[i];
      float ax = std::abs(x);
      if (ax > 0.9f)
        data[i] = std::copysign(0.9f + 0.1f * std::tanh((ax - 0.9f) * 10.0f), x);
    }
  }

  for (int c = 0; c < buffer.getNumChannels(); ++c)
    buffer.copyFrom(c, 0, mix_, std::min(c, 1), 0, num_samples);

  float decay = (float)std::exp(-num_samples / (0.25 * sample_rate_));
  status_[kStatusMasterL] = std::max((double)mix_.getMagnitude(0, 0, num_samples), status_[kStatusMasterL] * decay);
  status_[kStatusMasterR] = std::max((double)mix_.getMagnitude(1, 0, num_samples), status_[kStatusMasterR] * decay);

  if (capturing_ && playing_) {
    int frames = capture_frames_;
    int count = std::min(num_samples, capture_max_ - frames);
    if (count > 0) {
      memcpy(capture_[0].data() + frames, mix_.getReadPointer(0), sizeof(float) * count);
      memcpy(capture_[1].data() + frames, mix_.getReadPointer(1), sizeof(float) * count);
      capture_frames_ = frames + count;
    }
  }

  for (int i = num_tracks; i < kMaxTracks; ++i) {
    status_[kStatusTrackMeters + i * 2] = 0.0;
    status_[kStatusTrackMeters + i * 2 + 1] = 0.0;
  }

  status_[kStatusPosition] = position_;
  status_[kStatusPlaying] = playing_ ? 1.0 : 0.0;
  status_[kStatusRecording] = recording_ ? 1.0 : 0.0;
  status_[kStatusCounting] = (recording_ && playing_ && position_ < 0.0) ? 1.0 : 0.0;
  status_[kStatusLatency] = latency_samples_;
  status_[kStatusSampleRate] = sample_rate_;
  status_[kStatusCaptureFrames] = capture_frames_;
  status_[kStatusRecordedAvailable] = rec_fifo_.getNumReady();
}

#if JUCE_EMSCRIPTEN
#include <emscripten.h>

namespace {
  DawEngine* daw() { return DawEngine::instance; }

  char* copyString(const std::string& text) {
    char* result = (char*)malloc(text.size() + 1);
    memcpy(result, text.data(), text.size());
    result[text.size()] = 0;
    return result;
  }
}

extern "C" {
  EMSCRIPTEN_KEEPALIVE int vial_daw_ready() { return daw() ? 1 : 0; }
  EMSCRIPTEN_KEEPALIVE double* vial_daw_status() { return daw() ? daw()->status() : nullptr; }
  EMSCRIPTEN_KEEPALIVE void vial_daw_play(double tick) { if (daw()) daw()->play(tick); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_stop() { if (daw()) daw()->stop(); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_locate(double tick) { if (daw()) daw()->locate(tick); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_recording(int on, int count_in) { if (daw()) daw()->setRecording(on != 0, count_in); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_bpm(double bpm) { if (daw()) daw()->setBpm(bpm); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_beats_per_bar(int beats) { if (daw()) daw()->setBeatsPerBar(beats); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_loop(int on, double start, double end) { if (daw()) daw()->setLoop(on != 0, start, end); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_metronome(int on) { if (daw()) daw()->setMetronome(on != 0); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_master(float volume) { if (daw()) daw()->setMaster(volume); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_num_tracks(int num) { if (daw()) daw()->setNumTracks(num); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_track(int index, int type, int slot, float volume, float pan, int mute, int solo) {
    if (daw()) daw()->setTrack(index, type, slot, volume, pan, mute != 0, solo != 0);
  }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_selected(int track) { if (daw()) daw()->setSelectedTrack(track); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_events(const int* data, int count) { if (daw()) daw()->setEvents(data, count); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_live_note(int note, int velocity, int on) { if (daw()) daw()->liveNote(note, velocity, on != 0); }
  EMSCRIPTEN_KEEPALIVE int vial_daw_poll_recorded(double* out, int max) { return daw() ? daw()->pollRecorded(out, max) : 0; }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_pad_sample(int kit, int pad, const float* data, int frames, int channels, double rate) {
    if (daw()) daw()->setPadSample(kit, pad, data, frames, channels, rate);
  }
  EMSCRIPTEN_KEEPALIVE void vial_daw_set_pad(int kit, int pad, float gain, float pan, float pitch, int choke) {
    if (daw()) daw()->setPad(kit, pad, gain, pan, pitch, choke);
  }
  EMSCRIPTEN_KEEPALIVE int vial_daw_synth_ensure(int slot) { return daw() && daw()->ensureSynth(slot) ? 1 : 0; }
  EMSCRIPTEN_KEEPALIVE void vial_daw_synth_release(int slot) { if (daw()) daw()->releaseSynth(slot); }
  EMSCRIPTEN_KEEPALIVE char* vial_daw_synth_get_state(int slot) { return copyString(daw() ? daw()->getSynthState(slot) : std::string()); }
  EMSCRIPTEN_KEEPALIVE int vial_daw_synth_set_state(int slot, const char* state) { return daw() && daw()->setSynthState(slot, state) ? 1 : 0; }
  EMSCRIPTEN_KEEPALIVE void vial_daw_synth_init(int slot) { if (daw()) daw()->initSynth(slot); }
  EMSCRIPTEN_KEEPALIVE char* vial_daw_synth_name(int slot) { return copyString(daw() ? daw()->getSynthName(slot).toStdString() : std::string()); }
  EMSCRIPTEN_KEEPALIVE void vial_daw_capture_start(int max_frames) { if (daw()) daw()->startCapture(max_frames); }
  EMSCRIPTEN_KEEPALIVE int vial_daw_capture_stop() { return daw() ? daw()->stopCapture() : 0; }
  EMSCRIPTEN_KEEPALIVE float* vial_daw_capture_data(int channel) { return daw() ? daw()->captureData(channel) : nullptr; }
  EMSCRIPTEN_KEEPALIVE void vial_daw_capture_free() { if (daw()) daw()->freeCapture(); }
  EMSCRIPTEN_KEEPALIVE void vial_host_enable(int pairs) { if (daw()) daw()->hostEnable(pairs); }
  EMSCRIPTEN_KEEPALIVE void vial_host_note(int track, int note, int velocity, int on, int frame) { if (daw()) daw()->hostNote(track, note, velocity, on != 0, frame); }
  EMSCRIPTEN_KEEPALIVE int vial_host_render_position() { return flweb_render_position.load(); }
  EMSCRIPTEN_KEEPALIVE void vial_host_offline_begin() { if (daw()) daw()->offlineBegin(); }
  EMSCRIPTEN_KEEPALIVE void vial_host_offline_end() { if (daw()) daw()->offlineEnd(); }
  EMSCRIPTEN_KEEPALIVE int vial_host_offline_render(float* out, int frames) { return daw() ? daw()->offlineRender(out, frames) : 0; }
  EMSCRIPTEN_KEEPALIVE void vial_daw_panic() { if (daw()) daw()->panic(); }
  EMSCRIPTEN_KEEPALIVE int vial_daw_keyboard_offset(int offset) { return daw() ? daw()->keyboardOffset(offset) : 48; }
  EMSCRIPTEN_KEEPALIVE int vial_daw_text_focused() {
    Component* focused = Component::getCurrentlyFocusedComponent();
    return dynamic_cast<TextEditor*>(focused) != nullptr ? 1 : 0;
  }
}
#endif
