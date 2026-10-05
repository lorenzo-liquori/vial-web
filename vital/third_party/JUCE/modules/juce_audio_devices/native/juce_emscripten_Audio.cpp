// FLWeb host: posizione (frame dell'anello) del blocco che si sta calcolando, letta da DawEngine
std::atomic<int> flweb_render_position { 0 };

namespace juce
{

EM_JS (int, juce_webaudio_hostChannels, (), {
    return (Module.flwebHost && Module.flwebHost.channels) || 0;
});

EM_JS (void, juce_webaudio_open, (int deviceId, double sampleRate, int numChannels, int capacity,
                                  float* channelData, int* readIndex, int* writeIndex, int* underrunIndex), {
    var A = Module.juceAudio = Module.juceAudio || { devices: {} };

    if (Module.flwebHost)
    {
        A.devices[deviceId] = { ctx: Module.flwebHost.ctx, node: null, closed: false, host: true };
        Module.flwebHost.attach ({ buffer: wasmMemory.buffer, channels: numChannels, capacity: capacity,
                                   data: channelData >> 2, readIndex: readIndex >> 2, writeIndex: writeIndex >> 2,
                                   underrunIndex: underrunIndex >> 2, offsetIndex: (underrunIndex >> 2) + 4 });
        return;
    }

    if (! A.gestureHooked)
    {
        A.gestureHooked = true;
        var resumeAll = function() {
            for (var k in A.devices)
            {
                var d = A.devices[k];
                if (d && d.ctx && d.ctx.state != "running" && d.ctx.state != "closed")
                    d.ctx.resume().catch (function() {});
            }
        };
        ["pointerdown", "pointerup", "keydown", "touchend", "mousedown"].forEach (function (type) {
            window.addEventListener (type, resumeAll, true);
        });
        A.resumeAll = resumeAll;
    }

    if (! A.workletUrl)
    {
        var source = [
            "class JuceRingReader extends AudioWorkletProcessor {",
            "  constructor() {",
            "    super();",
            "    this.ready = false;",
            "    this.underruns = 0;",
            "    this.port.onmessage = (e) => {",
            "      const d = e.data;",
            "      if (d.type === 'init') {",
            "        this.f32 = new Float32Array(d.buffer);",
            "        this.i32 = new Int32Array(d.buffer);",
            "        this.channels = d.channels;",
            "        this.capacity = d.capacity;",
            "        this.mask = d.capacity - 1;",
            "        this.data = d.data;",
            "        this.readIndex = d.readIndex;",
            "        this.writeIndex = d.writeIndex;",
            "        this.underrunIndex = d.underrunIndex;",
            "        this.ready = true;",
            "      } else if (d.type === 'stop') {",
            "        this.ready = false;",
            "      }",
            "    };",
            "  }",
            "  process(inputs, outputs) {",
            "    const out = outputs[0];",
            "    if (!out || out.length === 0) return true;",
            "    const frames = out[0].length;",
            "    if (!this.ready) {",
            "      for (let c = 0; c < out.length; c++) out[c].fill(0);",
            "      return true;",
            "    }",
            "    const r = Atomics.load(this.i32, this.readIndex);",
            "    const w = Atomics.load(this.i32, this.writeIndex);",
            "    const available = (w - r) | 0;",
            "    const take = available < 0 ? 0 : Math.min(available, frames);",
            "    if (take > 0) {",
            "      const pos = r & this.mask;",
            "      const first = Math.min(take, this.capacity - pos);",
            "      for (let c = 0; c < out.length; c++) {",
            "        const base = this.data + Math.min(c, this.channels - 1) * this.capacity;",
            "        const dst = out[c];",
            "        dst.set(this.f32.subarray(base + pos, base + pos + first), 0);",
            "        if (first < take) dst.set(this.f32.subarray(base, base + take - first), first);",
            "        if (take < frames) {",
            "          const last = dst[take - 1];",
            "          for (let i = take; i < frames; i++) dst[i] = last * (1 - (i - take + 1) / (frames - take + 1));",
            "        }",
            "      }",
            "      Atomics.store(this.i32, this.readIndex, (r + take) | 0);",
            "    } else {",
            "      for (let c = 0; c < out.length; c++) out[c].fill(0);",
            "    }",
            "    if (take < frames && this.started) {",
            "      this.underruns++;",
            "      Atomics.add(this.i32, this.underrunIndex, 1);",
            "    }",
            "    if (take > 0) this.started = true;",
            "    Atomics.notify(this.i32, this.readIndex);",
            "    return true;",
            "  }",
            "}",
            "registerProcessor('juce-ring-reader', JuceRingReader);"
        ].join (String.fromCharCode (10));
        A.workletUrl = URL.createObjectURL (new Blob ([source], { type: "application/javascript" }));
    }

    var ctx;
    try {
        ctx = new AudioContext ({ sampleRate: sampleRate, latencyHint: "interactive" });
    } catch (err) {
        ctx = new AudioContext ({ latencyHint: "interactive" });
    }

    var dev = { ctx: ctx, node: null, closed: false };
    A.devices[deviceId] = dev;

    ctx.audioWorklet.addModule (A.workletUrl).then (function() {
        if (dev.closed)
            return;
        var node = new AudioWorkletNode (ctx, "juce-ring-reader", {
            numberOfInputs: 0,
            numberOfOutputs: 1,
            outputChannelCount: [numChannels]
        });
        node.port.postMessage ({
            type: "init",
            buffer: wasmMemory.buffer,
            channels: numChannels,
            capacity: capacity,
            data: channelData >> 2,
            readIndex: readIndex >> 2,
            writeIndex: writeIndex >> 2,
            underrunIndex: underrunIndex >> 2
        });
        node.connect (ctx.destination);
        dev.node = node;
        if (ctx.state != "running")
            ctx.resume().catch (function() {});
    }).catch (function (err) {
        console.error ("Audio worklet failed to load", err);
    });
});

EM_JS (int, juce_webaudio_isMobile, (), {
    try {
        if (window.matchMedia && window.matchMedia ("(pointer: coarse)").matches)
            return 1;
        if (navigator.hardwareConcurrency && navigator.hardwareConcurrency <= 4)
            return 1;
    } catch (err) {}
    return 0;
});

EM_JS (int, juce_webaudio_storedFill, (), {
    try {
        var v = parseInt (localStorage.getItem ("vial-audio-fill") || "0");
        return isFinite (v) ? Math.min (v, 8192) : 0;
    } catch (err) {}
    return 0;
});

static void juce_webaudio_storeFill (int fill)
{
    MAIN_THREAD_ASYNC_EM_ASM ({
        try { localStorage.setItem ("vial-audio-fill", String ($0)); } catch (err) {}
    }, fill);
}

EM_JS (double, juce_webaudio_actualSampleRate, (int deviceId), {
    var A = Module.juceAudio;
    if (! A || ! A.devices[deviceId]) return 0;
    return A.devices[deviceId].ctx.sampleRate;
});

EM_JS (double, juce_webaudio_outputLatency, (int deviceId), {
    var A = Module.juceAudio;
    if (! A || ! A.devices[deviceId]) return 0;
    var ctx = A.devices[deviceId].ctx;
    return (ctx.baseLatency || 0) + (ctx.outputLatency || 0);
});

EM_JS (void, juce_webaudio_close, (int deviceId), {
    var A = Module.juceAudio;
    if (! A || ! A.devices[deviceId]) return;
    var dev = A.devices[deviceId];
    dev.closed = true;
    if (dev.host)
    {
        try { Module.flwebHost.detach(); } catch (err) {}
        delete A.devices[deviceId];
        return;
    }
    if (dev.node)
    {
        try { dev.node.port.postMessage ({ type: "stop" }); } catch (err) {}
        try { dev.node.disconnect(); } catch (err) {}
    }
    try { dev.ctx.close(); } catch (err) {}
    delete A.devices[deviceId];
});

EM_JS (double, juce_webaudio_defaultSampleRate, (), {
    var A = Module.juceAudio = Module.juceAudio || { devices: {} };
    if (Module.flwebHost)
        return Module.flwebHost.ctx.sampleRate;
    if (! A.defaultRate)
    {
        try {
            var probe = new AudioContext();
            A.defaultRate = probe.sampleRate;
            probe.close();
        } catch (err) {
            A.defaultRate = 48000;
        }
    }
    return A.defaultRate;
});

class WebAudioIODevice  : public AudioIODevice,
                          private Thread
{
public:
    WebAudioIODevice (const String& deviceName)
        : AudioIODevice (deviceName, "Web Audio"),
          Thread ("Web Audio Render"),
          deviceId (nextDeviceId()++)
    {
    }

    ~WebAudioIODevice() override
    {
        close();
    }

    StringArray getOutputChannelNames() override
    {
        int n = juce_webaudio_hostChannels();
        if (n <= 2)
            return { "Left", "Right" };
        StringArray names;
        for (int i = 0; i < n; ++i)
            names.add ("Out " + String (i + 1));
        return names;
    }
    StringArray getInputChannelNames() override    { return {}; }

    Array<double> getAvailableSampleRates() override
    {
        Array<double> rates { 44100.0, 48000.0, 88200.0, 96000.0 };
        rates.addIfNotAlreadyThere (juce_webaudio_defaultSampleRate());
        rates.sort();
        return rates;
    }

    Array<int> getAvailableBufferSizes() override  { return { 128, 256, 512, 1024, 2048 }; }
    int getDefaultBufferSize() override            { return 256; }

    String open (const BigInteger&, const BigInteger& outputChannels, double sampleRate, int bufferSizeSamples) override
    {
        close();

        if (sampleRate <= 0)
            sampleRate = juce_webaudio_defaultSampleRate();

        if (bufferSizeSamples <= 0)
            bufferSizeSamples = getDefaultBufferSize();

        blockSize = jlimit (32, 4096, bufferSizeSamples);
        activeOutputs = outputChannels;
        activeOutputs.setRange (jmax (2, juce_webaudio_hostChannels()), activeOutputs.getHighestBit() + 1, false);
        numOutputs = jmax (1, activeOutputs.countNumberOfSetBits());

        minimumFill = jmax (blockSize * 2, juce_webaudio_isMobile() ? 2048 : 1024);
        targetFill = jmax (minimumFill, juce_webaudio_storedFill());
        maximumFill = jmax (minimumFill, 8192);
        capacity = 1;

        while (capacity < maximumFill + blockSize * 2 + 256)
            capacity <<= 1;

        ringData.calloc ((size_t) (capacity * numOutputs));
        indices.calloc (16);
        indices[0] = 0;
        indices[4] = 0;
        indices[8] = 0;

        renderBuffer.setSize (numOutputs, blockSize);

        juce_webaudio_open (deviceId, sampleRate, numOutputs, capacity, ringData.getData(),
                            indices.getData(), indices.getData() + 4, indices.getData() + 8);

        currentSampleRate = juce_webaudio_actualSampleRate (deviceId);

        if (currentSampleRate <= 0)
            currentSampleRate = sampleRate;

        isOpenFlag = true;
        return {};
    }

    void close() override
    {
        stop();

        if (isOpenFlag)
        {
            juce_webaudio_close (deviceId);
            isOpenFlag = false;
        }
    }

    bool isOpen() override                                  { return isOpenFlag; }
    bool isPlaying() override                               { return callback != nullptr; }
    String getLastError() override                          { return {}; }
    int getCurrentBufferSizeSamples() override              { return blockSize; }
    double getCurrentSampleRate() override                  { return currentSampleRate; }
    int getCurrentBitDepth() override                       { return 32; }
    BigInteger getActiveOutputChannels() const override     { return activeOutputs; }
    BigInteger getActiveInputChannels() const override      { return {}; }
    int getInputLatencyInSamples() override                 { return 0; }

    int getOutputLatencyInSamples() override
    {
        return targetFill + roundToInt (juce_webaudio_outputLatency (deviceId) * currentSampleRate);
    }

    void start (AudioIODeviceCallback* newCallback) override
    {
        if (! isOpenFlag || newCallback == nullptr)
            return;

        if (callback == newCallback)
            return;

        stop();

        newCallback->audioDeviceAboutToStart (this);

        {
            const ScopedLock sl (callbackLock);
            callback = newCallback;
        }

        startThread (9);
    }

    void stop() override
    {
        AudioIODeviceCallback* old = nullptr;

        {
            const ScopedLock sl (callbackLock);
            old = callback;
        }

        if (old == nullptr)
            return;

        signalThreadShouldExit();
        stopThread (2000);

        {
            const ScopedLock sl (callbackLock);
            callback = nullptr;
        }

        old->audioDeviceStopped();
    }

private:
    static int& nextDeviceId()
    {
        static int id = 1;
        return id;
    }

    void run() override
    {
        auto* readIndex = indices.getData();
        auto* writeIndex = indices.getData() + 4;
        auto* underrunCount = indices.getData() + 8;
        auto mask = capacity - 1;
        int lastUnderruns = 0;
        int64 lastGrowth = 0;
        int64 lastEpisode = -100000;
        int64 cleanSince = Time::getMillisecondCounter();

        while (! threadShouldExit())
        {
            auto underruns = __atomic_load_n (underrunCount, __ATOMIC_SEQ_CST);

            if (underruns != lastUnderruns)
            {
                lastUnderruns = underruns;
                auto now = (int64) Time::getMillisecondCounter();
                cleanSince = now;

                bool repeated = false;

                if (now - lastEpisode > 300)
                {
                    repeated = now - lastEpisode < 5000;
                    lastEpisode = now;
                }

                if (repeated && now - lastGrowth > 250 && targetFill < maximumFill)
                {
                    lastGrowth = now;
                    targetFill = jmin (maximumFill, targetFill + jmax (512, targetFill / 2));
                    juce_webaudio_storeFill (targetFill);
                }
            }
            else if (targetFill > minimumFill && (int64) Time::getMillisecondCounter() - cleanSince > 60000)
            {
                cleanSince = Time::getMillisecondCounter();
                targetFill = jmax (minimumFill, targetFill - 256);
                juce_webaudio_storeFill (targetFill);
            }

            auto r = __atomic_load_n (readIndex, __ATOMIC_SEQ_CST);
            auto w = __atomic_load_n (writeIndex, __ATOMIC_SEQ_CST);
            auto filled = w - r;

            if (filled < 0 || filled > capacity)
            {
                __atomic_store_n (writeIndex, r, __ATOMIC_SEQ_CST);
                continue;
            }

            if (filled + blockSize > targetFill)
            {
                emscripten_futex_wait ((void*) readIndex, (uint32_t) r, 4.0);
                continue;
            }

            renderBuffer.clear();
            flweb_render_position.store (w);

            {
                const ScopedLock sl (callbackLock);

                if (callback != nullptr)
                    callback->audioDeviceIOCallback (nullptr, 0, renderBuffer.getArrayOfWritePointers(),
                                                     numOutputs, blockSize);
            }

            auto pos = w & mask;
            auto first = jmin (blockSize, capacity - pos);

            for (int c = 0; c < numOutputs; ++c)
            {
                auto* dest = ringData.getData() + c * capacity;
                auto* src = renderBuffer.getReadPointer (c);
                memcpy (dest + pos, src, (size_t) first * sizeof (float));

                if (first < blockSize)
                    memcpy (dest, src + first, (size_t) (blockSize - first) * sizeof (float));
            }

            __atomic_store_n (writeIndex, w + blockSize, __ATOMIC_SEQ_CST);
        }
    }

    const int deviceId;
    HeapBlock<float> ringData;
    HeapBlock<int> indices;
    AudioBuffer<float> renderBuffer;
    BigInteger activeOutputs;
    CriticalSection callbackLock;
    AudioIODeviceCallback* callback = nullptr;
    double currentSampleRate = 44100.0;
    int blockSize = 256, capacity = 4096, targetFill = 512, minimumFill = 512, maximumFill = 8192, numOutputs = 2;
    bool isOpenFlag = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebAudioIODevice)
};

class WebAudioIODeviceType  : public AudioIODeviceType
{
public:
    WebAudioIODeviceType() : AudioIODeviceType ("Web Audio") {}

    void scanForDevices() override {}

    StringArray getDeviceNames (bool wantInputNames) const override
    {
        if (wantInputNames)
            return {};

        return { "Browser Audio Output" };
    }

    int getDefaultDeviceIndex (bool) const override               { return 0; }
    bool hasSeparateInputsAndOutputs() const override              { return true; }

    int getIndexOfDevice (AudioIODevice* device, bool asInput) const override
    {
        if (device == nullptr || asInput)
            return -1;

        return 0;
    }

    AudioIODevice* createDevice (const String& outputDeviceName, const String&) override
    {
        return new WebAudioIODevice (outputDeviceName.isNotEmpty() ? outputDeviceName : String ("Browser Audio Output"));
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebAudioIODeviceType)
};

AudioIODeviceType* AudioIODeviceType::createAudioIODeviceType_WebAudio()
{
    return new WebAudioIODeviceType();
}

}
