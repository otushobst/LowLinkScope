#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LinkBus.h"

namespace ParamIDs
{
    inline constexpr const char* size      = "size";       // window length in beats
    inline constexpr const char* channel   = "channel";    // this / both / linked
    inline constexpr const char* mix       = "mix";        // show summed wave
    inline constexpr const char* lowpass   = "lowpass";    // display + analysis band
    inline constexpr const char* amp       = "amp";
    inline constexpr const char* selfLevel = "selfLevel";
    inline constexpr const char* linkLevel = "linkLevel";
    inline constexpr const char* zoom      = "zoom";
    inline constexpr const char* position  = "position";
    inline constexpr const char* live      = "live";       // continuous sweep vs. hold per window
}

// Choices shared by processor and editor
inline const juce::StringArray kSizeNames { "1/4 Beat", "1/2 Beat", "1 Beat", "2 Beats", "4 Beats" };
inline constexpr double kSizeBeats[] { 0.25, 0.5, 1.0, 2.0, 4.0 };
inline const juce::StringArray kChannelNames { "This", "Both", "Linked" };
inline const juce::StringArray kLowpassNames { "Full", "80 Hz", "120 Hz", "200 Hz", "400 Hz" };
inline constexpr double kLowpassHz[] { 0.0, 80.0, 120.0, 200.0, 400.0 };

class LowLinkProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    LowLinkProcessor();
    ~LowLinkProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void updateTrackProperties (const TrackProperties& properties) override;

    // --- used by the editor -------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    lowlink::LinkBus& getBus() { return *bus; }
    int getSlot() const noexcept { return slot; }
    uint64_t getInstanceId() const noexcept { return instanceId; }

    juce::String getDisplayName() const;
    void setCustomName (const juce::String& name);   // empty = follow DAW track name
    juce::String getCustomName() const { const juce::ScopedLock sl (nameLock); return customName; }
    juce::String getLinkTargetName() const;
    void setLinkTargetName (const juce::String& name);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void pushNameToBus();

    juce::SharedResourcePointer<lowlink::LinkBus> bus;
    uint64_t instanceId = 0;
    int slot = -1;

    std::vector<float> monoBuffer;
    double currentSampleRate = 44100.0;
    int64_t freeRunPos = 0;
    std::atomic<bool> nameDirty { false };

    mutable juce::CriticalSection nameLock;
    juce::String trackName, customName, linkTargetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowLinkProcessor)
};
