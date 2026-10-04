#pragma once
// LinkBus: shares audio between plugin instances through named shared memory.
// Works across processes, so it also works in DAWs that sandbox plugins.
// No JUCE dependency (so it can be unit tested on its own).

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace lowlink
{
constexpr uint32_t kMagic    = 0x4C4C5331; // "LLS1"
constexpr uint32_t kVersion  = 1;
constexpr int      kMaxSlots = 16;
constexpr int      kRingBits = 19;                       // 524288 samples per slot
constexpr int64_t  kRingSize = int64_t (1) << kRingBits; // ~5.4 s at 96 kHz
constexpr int64_t  kRingMask = kRingSize - 1;
constexpr int      kNameLen  = 64;
constexpr int64_t  kStaleMs  = 3000;                     // slot considered dead after this

enum TimeMode : int32_t
{
    timeline = 0, // ring indexed by host timeline sample position (transport running)
    freeRun  = 1  // ring indexed by a private counter (transport stopped / no host time)
};

struct alignas (64) Slot
{
    std::atomic<uint32_t> state;       // 0 = free, 1 = used
    std::atomic<int64_t>  heartbeatMs;
    std::atomic<uint64_t> instanceId;

    std::atomic<uint32_t> nameSeq;     // seqlock guarding name
    char name[kNameLen];

    std::atomic<uint32_t> metaSeq;     // seqlock guarding the fields below
    std::atomic<double>   sampleRate;
    std::atomic<double>   bpm;
    std::atomic<double>   endPpq;      // ppq position at endPos (valid in timeline mode)
    std::atomic<int64_t>  endPos;      // one past the last written sample position
    std::atomic<int32_t>  timeMode;
    std::atomic<int32_t>  playing;

    float ring[kRingSize];
};

struct Header
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> version;
    uint32_t reserved[14];
    Slot slots[kMaxSlots];
};

struct SlotInfo
{
    int index = -1;
    uint64_t instanceId = 0;
    std::string name;
    double sampleRate = 0.0;
    double bpm = 0.0;
    double endPpq = 0.0;
    int64_t endPos = 0;
    int32_t timeMode = freeRun;
    bool playing = false;
};

int64_t nowMs();

class LinkBus
{
public:
    LinkBus();
    ~LinkBus();

    bool isOpen() const noexcept { return header != nullptr; }
    const std::string& getError() const noexcept { return error; }

    // Slot lifetime (message thread)
    int  claimSlot (uint64_t instanceId);
    void releaseSlot (int slot, uint64_t instanceId);
    void heartbeat (int slot, uint64_t instanceId);
    void setName (int slot, const std::string& name);

    // Audio thread: write samples at absolute positions [pos, pos + n)
    void write (int slot, const float* mono, int n, int64_t pos) noexcept;
    void publishMeta (int slot, double sampleRate, double bpm, double endPpq,
                      int64_t endPos, int32_t mode, bool playing) noexcept;

    // Readers (GUI thread)
    bool getInfo (int slot, SlotInfo& out) const;
    std::vector<SlotInfo> listAlive() const;

    // Copies [start, start + len) into out; positions that are not available
    // (not written yet or already overwritten) become 0. Returns the number of
    // valid samples.
    int readRange (int slot, int64_t start, int len, float* out) const;

private:
    Header* header = nullptr;
    void* platformHandle = nullptr;
    size_t mappedSize = 0;
    std::string error;
};
} // namespace lowlink
