/* Copyright 2013-2019 Matt Tytel
 *
 * vital is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vital is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with vital.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "synth_editor.h"

#include "authentication.h"
#include "default_look_and_feel.h"
#include "full_interface.h"
#include "synth_computer_keyboard.h"
#include "synth_constants.h"
#include "sound_engine.h"
#include "load_save.h"
#include "startup.h"
#include "utils.h"

#if JUCE_EMSCRIPTEN
#include <emscripten.h>
EM_JS(int, flweb_host_channels_js, (), { return (Module.flwebHost && Module.flwebHost.channels) || 0; });
static int outputChannels() { int n = flweb_host_channels_js(); return n > 2 ? n : vital::kNumChannels; }
#else
static int outputChannels() { return vital::kNumChannels; }
#endif

SynthEditor::SynthEditor(bool use_gui) : SynthGuiInterface(this, use_gui) {
  static constexpr int kHeightBuffer = 50;

  daw_ = std::make_unique<DawEngine>(this);
  DawEngine::instance = daw_.get();

  computer_keyboard_ = std::make_unique<SynthComputerKeyboard>(engine_.get(), keyboard_state_.get());
  current_time_ = 0.0;

  setAudioChannels(0, outputChannels());

  AudioDeviceManager::AudioDeviceSetup setup;
  deviceManager.getAudioDeviceSetup(setup);
  setup.sampleRate = vital::kDefaultSampleRate;
  deviceManager.initialise(0, outputChannels(), nullptr, true, "", &setup);

  if (deviceManager.getCurrentAudioDevice() == nullptr) {
    const OwnedArray<AudioIODeviceType>& device_types = deviceManager.getAvailableDeviceTypes();

    for (AudioIODeviceType* device_type : device_types) {
      deviceManager.setCurrentAudioDeviceType(device_type->getTypeName(), true);
      if (deviceManager.getCurrentAudioDevice())
        break;
    }
  }

  current_midi_ins_ = StringArray(MidiInput::getDevices());

  for (const String& midi_in : current_midi_ins_)
    deviceManager.setMidiInputEnabled(midi_in, true);

  deviceManager.addMidiInputCallback("", midi_manager_.get());

  if (use_gui) {
    setLookAndFeel(DefaultLookAndFeel::instance());
    addAndMakeVisible(gui_.get());
    gui_->reset();
    gui_->setOscilloscopeMemory(getOscilloscopeMemory());
    gui_->setAudioMemory(getAudioMemory());

    Rectangle<int> total_bounds = Desktop::getInstance().getDisplays().getTotalBounds(true);
    total_bounds.removeFromBottom(kHeightBuffer);

    float window_size = LoadSave::loadWindowSize();
    window_size = std::min(window_size, total_bounds.getWidth() / (1.0f * vital::kDefaultWindowWidth));
    window_size = std::min(window_size, total_bounds.getHeight() / (1.0f * vital::kDefaultWindowHeight));
    int width = std::round(window_size * vital::kDefaultWindowWidth);
    int height = std::round(window_size * vital::kDefaultWindowHeight);
    setSize(width, height);

    setWantsKeyboardFocus(true);
    addKeyListener(computer_keyboard_.get());
    setOpaque(true);
  }

  startTimer(500);
}

SynthEditor::~SynthEditor() {
  PopupMenu::dismissAllActiveMenus();
  shutdownAudio();
  DawEngine::instance = nullptr;
}

void SynthEditor::prepareToPlay(int buffer_size, double sample_rate) {
  engine_->setSampleRate(sample_rate);
  engine_->updateAllModulationSwitches();
  midi_manager_->setSampleRate(sample_rate);
  daw_->prepare(sample_rate);
}

void SynthEditor::renderOffline(AudioSampleBuffer& buffer, int num_samples) {
  AudioSourceChannelInfo info(&buffer, 0, num_samples);
  renderBlock(info);
}

void SynthEditor::getNextAudioBlock(const AudioSourceChannelInfo& buffer) {
  if (daw_->offlineActive()) {
    buffer.clearActiveBufferRegion();
    return;
  }
  renderBlock(buffer);
}

void SynthEditor::renderBlock(const AudioSourceChannelInfo& buffer) {
  ScopedLock lock(getCriticalSection());

  int num_samples = buffer.buffer->getNumSamples();
  int synth_samples = std::min(num_samples, vital::kMaxBufferSize);

  processModulationChanges();
  MidiBuffer midi_messages;
  midi_manager_->removeNextBlockOfMessages(midi_messages, num_samples);
  processKeyboardEvents(midi_messages, num_samples);

  MidiBuffer synth_messages;
  daw_->beginBlock(midi_messages, num_samples, synth_messages);
  engine_->setBpm((float)daw_->bpm());

  double sample_time = 1.0 / getSampleRate();
  for (int b = 0; b < num_samples; b += synth_samples) {
    int current_samples = std::min<int>(synth_samples, num_samples - b);
    engine_->correctToTime(current_time_);

    processMidi(synth_messages, b, b + current_samples);
    processAudio(buffer.buffer, vital::kNumChannels, current_samples, b);
    current_time_ += current_samples * sample_time;
  }

  daw_->endBlock(*buffer.buffer, num_samples);
}

void SynthEditor::releaseResources() {
}

void SynthEditor::resized() {
  if (gui_)
    gui_->setBounds(getLocalBounds());
}

std::string SynthEditor::getGuiSynthState() {
  return saveToJson().dump();
}

bool SynthEditor::setGuiSynthState(const std::string& state) {
  json parsed = json::parse(state, nullptr, false);
  if (parsed.is_discarded())
    return false;

  bool result = false;
  try {
    result = loadFromJson(parsed);
  }
  catch (...) {
    result = false;
  }

  clearActiveFile();
  updateFullGui();
  notifyFresh();
  return result;
}

void SynthEditor::initGuiSynth() {
  loadInitPreset();
  clearActiveFile();
  updateFullGui();
  notifyFresh();
}

int SynthEditor::getComputerKeyboardOffset() {
  return computer_keyboard_->getKeyboardOffset();
}

void SynthEditor::setComputerKeyboardOffset(int offset) {
  computer_keyboard_->changeKeyboardOffset(offset);
}

void SynthEditor::timerCallback() {
  if (AudioIODevice* device = deviceManager.getCurrentAudioDevice())
    daw_->setLatency(device->getOutputLatencyInSamples());

  StringArray midi_ins(MidiInput::getDevices());

  for (int i = 0; i < midi_ins.size(); ++i) {
    if (!current_midi_ins_.contains(midi_ins[i]))
      deviceManager.setMidiInputEnabled(midi_ins[i], true);
  }

  current_midi_ins_ = midi_ins;
}

void SynthEditor::animate(bool animate) {
  if (gui_)
    gui_->animate(animate);
}
