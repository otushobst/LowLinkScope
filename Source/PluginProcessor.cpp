#include "PluginProcessor.h"
#include "PluginEditor.h"

LowLinkProcessor::LowLinkProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LowLinkScope", createLayout())
{
    juce::Random rng;
    instanceId = (static_cast<uint64_t> (rng.nextInt64()) & 0x7fffffffffffffffull)
               ^ static_cast<uint64_t> (juce::Time::getHighResolutionTicks());
    if (instanceId == 0)
        instanceId = 1;

    slot = bus->claimSlot (instanceId);
    pushNameToBus();
    startTimerHz (4); // heartbeat
}

LowLinkProcessor::~LowLinkProcessor()
{
    stopTimer();
    bus->releaseSlot (slot, instanceId);
}

juce::AudioProcessorValueTreeState::ParameterLayout LowLinkProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::size, 1 }, "Size", kSizeNames, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::channel, 1 }, "Channel", kChannelNames, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::mix, 1 }, "Mix", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lowpass, 1 }, "Low-pass", kLowpassNames, 2));

    NormalisableRange<float> ampRange (0.5f, 32.0f);
    ampRange.setSkewForCentre (4.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::amp, 1 }, "Amp", ampRange, 2.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::selfLevel, 1 }, "This level", NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::linkLevel, 1 }, "Linked level", NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    NormalisableRange<float> zoomRange (1.0f, 32.0f);
    zoomRange.setSkewForCentre (4.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::zoom, 1 }, "Zoom", zoomRange, 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::position, 1 }, "Position", NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::live, 1 }, "Live", true));
    return layout;
}

void LowLinkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    monoBuffer.assign (static_cast<size_t> (juce::jmax (samplesPerBlock, 512)), 0.0f);
}

bool LowLinkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void LowLinkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    if (numSamples <= 0 || slot < 0 || monoBuffer.empty())
        return;

    // Transport info
    bool playing = false;
    bool hasTime = false;
    int64_t timePos = 0;
    double ppq = 0.0;
    double bpm = 120.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying() || pos->getIsRecording();
            if (auto b = pos->getBpm())
                if (*b > 1.0)
                    bpm = *b;
            if (auto t = pos->getTimeInSamples())
            {
                timePos = *t;
                hasTime = true;
            }
            if (auto p = pos->getPpqPosition())
                ppq = *p;
            else if (hasTime)
                ppq = double (timePos) / currentSampleRate * bpm / 60.0;
        }
    }

    const bool timeline = playing && hasTime && timePos >= 0;
    const int64_t startPos = timeline ? timePos : freeRunPos;
    const double samplesPerBeat = currentSampleRate * 60.0 / bpm;

    // Mono mix, in chunks so we never allocate on the audio thread.
    const int chunkMax = static_cast<int> (monoBuffer.size());
    for (int offset = 0; offset < numSamples; offset += chunkMax)
    {
        const int n = juce::jmin (chunkMax, numSamples - offset);
        auto* m = monoBuffer.data();
        if (numChannels <= 0)
            std::fill (m, m + n, 0.0f);
        else
        {
            juce::FloatVectorOperations::copy (m, buffer.getReadPointer (0, offset), n);
            for (int ch = 1; ch < numChannels; ++ch)
                juce::FloatVectorOperations::add (m, buffer.getReadPointer (ch, offset), n);
            if (numChannels > 1)
                juce::FloatVectorOperations::multiply (m, 1.0f / float (numChannels), n);
        }
        bus->write (slot, m, n, startPos + offset);
    }

    const int64_t endPos = startPos + numSamples;
    const double endPpq = ppq + numSamples / samplesPerBeat;
    bus->publishMeta (slot, currentSampleRate, bpm, endPpq, endPos,
                      timeline ? lowlink::timeline : lowlink::freeRun, playing);

    if (! timeline)
        freeRunPos += numSamples;

    // Audio passes through untouched (analysis only).
}

void LowLinkProcessor::timerCallback()
{
    if (slot < 0)
    {
        // Bus was full or not available when we started; retry.
        slot = bus->claimSlot (instanceId);
        if (slot >= 0)
            pushNameToBus();
        return;
    }
    bus->heartbeat (slot, instanceId);
    if (nameDirty.exchange (false))
        pushNameToBus();
}

juce::String LowLinkProcessor::getDisplayName() const
{
    const juce::ScopedLock sl (nameLock);
    if (customName.isNotEmpty())
        return customName;
    if (trackName.isNotEmpty())
        return trackName;
    return "Track " + juce::String (slot + 1);
}

void LowLinkProcessor::setCustomName (const juce::String& name)
{
    {
        const juce::ScopedLock sl (nameLock);
        customName = name.trim();
    }
    pushNameToBus();
}

juce::String LowLinkProcessor::getLinkTargetName() const
{
    const juce::ScopedLock sl (nameLock);
    return linkTargetName;
}

void LowLinkProcessor::setLinkTargetName (const juce::String& name)
{
    const juce::ScopedLock sl (nameLock);
    linkTargetName = name;
}

void LowLinkProcessor::updateTrackProperties (const TrackProperties& properties)
{
    {
        const juce::ScopedLock sl (nameLock);
        if (properties.name.has_value())
            trackName = *properties.name;
    }
    nameDirty.store (true); // pushed from the timer on the message thread
}

void LowLinkProcessor::pushNameToBus()
{
    if (slot >= 0)
        bus->setName (slot, getDisplayName().toStdString());
}

void LowLinkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (nameLock);
        state.setProperty ("customName", customName, nullptr);
        state.setProperty ("linkTarget", linkTargetName, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LowLinkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid())
            return;
        {
            const juce::ScopedLock sl (nameLock);
            customName = state.getProperty ("customName").toString();
            linkTargetName = state.getProperty ("linkTarget").toString();
        }
        apvts.replaceState (state);
        pushNameToBus();
    }
}

juce::AudioProcessorEditor* LowLinkProcessor::createEditor()
{
    return new LowLinkEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowLinkProcessor();
}
