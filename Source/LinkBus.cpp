#include "LinkBus.h"

#include <chrono>
#include <cstring>
#include <algorithm>

#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#else
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <unistd.h>
#endif

namespace lowlink
{
static_assert (std::atomic<double>::is_always_lock_free, "need lock-free atomic<double>");
static_assert (std::atomic<int64_t>::is_always_lock_free, "need lock-free atomic<int64_t>");
static_assert (std::atomic<uint64_t>::is_always_lock_free, "need lock-free atomic<uint64_t>");

int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds> (system_clock::now().time_since_epoch()).count();
}

namespace
{
    // Seqlock helpers -------------------------------------------------------
    struct SeqWriter
    {
        explicit SeqWriter (std::atomic<uint32_t>& s) : seq (s)
        {
            start = seq.load (std::memory_order_relaxed);
            seq.store (start + 1, std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_release);
        }
        ~SeqWriter() { seq.store (start + 2, std::memory_order_release); }
        std::atomic<uint32_t>& seq;
        uint32_t start;
    };

    template <typename Fn>
    bool seqRead (const std::atomic<uint32_t>& seq, Fn&& fn)
    {
        for (int attempt = 0; attempt < 64; ++attempt)
        {
            const auto s1 = seq.load (std::memory_order_acquire);
            if (s1 & 1u)
                continue;
            fn();
            std::atomic_thread_fence (std::memory_order_acquire);
            if (seq.load (std::memory_order_relaxed) == s1)
                return true;
        }
        return false;
    }

#if defined(_WIN32)
    const wchar_t* kMapName = L"Local\\LowLinkScope_v1";
#else
    const char* kShmName = "/lowlinkscope_v1";
#endif
} // namespace

LinkBus::LinkBus()
{
    mappedSize = sizeof (Header);

#if defined(_WIN32)
    const auto size64 = static_cast<unsigned long long> (mappedSize);
    HANDLE h = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                   static_cast<DWORD> (size64 >> 32),
                                   static_cast<DWORD> (size64 & 0xffffffffull), kMapName);
    if (h == nullptr)
    {
        error = "CreateFileMapping failed";
        return;
    }
    void* p = MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, mappedSize);
    if (p == nullptr)
    {
        CloseHandle (h);
        error = "MapViewOfFile failed";
        return;
    }
    platformHandle = h;
    header = static_cast<Header*> (p);
#else
    int fd = shm_open (kShmName, O_CREAT | O_RDWR, 0600);
    if (fd < 0)
    {
        error = "shm_open failed";
        return;
    }
    struct stat st {};
    if (fstat (fd, &st) == 0 && static_cast<size_t> (st.st_size) < mappedSize)
    {
        if (ftruncate (fd, static_cast<off_t> (mappedSize)) != 0)
        {
            close (fd);
            error = "ftruncate failed";
            return;
        }
    }
    void* p = mmap (nullptr, mappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close (fd);
    if (p == MAP_FAILED)
    {
        error = "mmap failed";
        return;
    }
    header = static_cast<Header*> (p);
#endif

    // Fresh mappings are zero-filled by the OS. First user stamps the magic.
    uint32_t expected = 0;
    if (header->magic.compare_exchange_strong (expected, kMagic))
        header->version.store (kVersion);
    else if (expected != kMagic || header->version.load() != kVersion)
    {
        error = "incompatible Low Link Scope version running";
#if defined(_WIN32)
        UnmapViewOfFile (header);
        CloseHandle (static_cast<HANDLE> (platformHandle));
        platformHandle = nullptr;
#else
        munmap (header, mappedSize);
#endif
        header = nullptr;
    }
}

LinkBus::~LinkBus()
{
    if (header == nullptr)
        return;
#if defined(_WIN32)
    UnmapViewOfFile (header);
    if (platformHandle != nullptr)
        CloseHandle (static_cast<HANDLE> (platformHandle));
#else
    munmap (header, mappedSize);
#endif
}

int LinkBus::claimSlot (uint64_t instanceId)
{
    if (header == nullptr)
        return -1;

    const auto now = nowMs();
    for (int i = 0; i < kMaxSlots; ++i)
    {
        auto& s = header->slots[i];
        auto hb = s.heartbeatMs.load();
        if (now - hb <= kStaleMs)
            continue;
        if (! s.heartbeatMs.compare_exchange_strong (hb, now))
            continue;

        s.instanceId.store (instanceId);
        {
            SeqWriter w (s.nameSeq);
            std::memset (s.name, 0, sizeof (s.name));
        }
        {
            SeqWriter w (s.metaSeq);
            s.sampleRate.store (0.0, std::memory_order_relaxed);
            s.bpm.store (120.0, std::memory_order_relaxed);
            s.endPpq.store (0.0, std::memory_order_relaxed);
            s.endPos.store (0, std::memory_order_relaxed);
            s.timeMode.store (freeRun, std::memory_order_relaxed);
            s.playing.store (0, std::memory_order_relaxed);
        }
        std::memset (s.ring, 0, sizeof (s.ring));
        s.state.store (1);
        return i;
    }
    return -1;
}

void LinkBus::releaseSlot (int slot, uint64_t instanceId)
{
    if (header == nullptr || slot < 0 || slot >= kMaxSlots)
        return;
    auto& s = header->slots[slot];
    if (s.instanceId.load() != instanceId)
        return;
    s.state.store (0);
    s.instanceId.store (0);
    s.heartbeatMs.store (0);
}

void LinkBus::heartbeat (int slot, uint64_t instanceId)
{
    if (header == nullptr || slot < 0 || slot >= kMaxSlots)
        return;
    auto& s = header->slots[slot];
    if (s.instanceId.load() == instanceId)
        s.heartbeatMs.store (nowMs());
}

void LinkBus::setName (int slot, const std::string& name)
{
    if (header == nullptr || slot < 0 || slot >= kMaxSlots)
        return;
    auto& s = header->slots[slot];
    SeqWriter w (s.nameSeq);
    std::memset (s.name, 0, sizeof (s.name));
    std::memcpy (s.name, name.data(), std::min<size_t> (name.size(), kNameLen - 1));
}

void LinkBus::write (int slot, const float* mono, int n, int64_t pos) noexcept
{
    if (header == nullptr || slot < 0 || slot >= kMaxSlots || n <= 0 || pos < 0)
        return;
    auto& s = header->slots[slot];
    int done = 0;
    while (done < n)
    {
        const auto idx = (pos + done) & kRingMask;
        const auto chunk = static_cast<int> (std::min<int64_t> (n - done, kRingSize - idx));
        std::memcpy (s.ring + idx, mono + done, sizeof (float) * static_cast<size_t> (chunk));
        done += chunk;
    }
}

void LinkBus::publishMeta (int slot, double sampleRate, double bpm, double endPpq,
                           int64_t endPos, int32_t mode, bool playing) noexcept
{
    if (header == nullptr || slot < 0 || slot >= kMaxSlots)
        return;
    auto& s = header->slots[slot];
    SeqWriter w (s.metaSeq);
    s.sampleRate.store (sampleRate, std::memory_order_relaxed);
    s.bpm.store (bpm, std::memory_order_relaxed);
    s.endPpq.store (endPpq, std::memory_order_relaxed);
    s.endPos.store (endPos, std::memory_order_relaxed);
    s.timeMode.store (mode, std::memory_order_relaxed);
    s.playing.store (playing ? 1 : 0, std::memory_order_relaxed);
}

bool LinkBus::getInfo (int slot, SlotInfo& out) const
{
    if (header == nullptr || slot < 0 || slot >= kMaxSlots)
        return false;
    const auto& s = header->slots[slot];
    if (s.state.load() != 1 || nowMs() - s.heartbeatMs.load() > kStaleMs)
        return false;

    out.index = slot;
    out.instanceId = s.instanceId.load();

    char nameCopy[kNameLen] {};
    seqRead (s.nameSeq, [&] { std::memcpy (nameCopy, s.name, kNameLen); });
    nameCopy[kNameLen - 1] = 0;
    out.name = nameCopy;

    return seqRead (s.metaSeq, [&]
    {
        out.sampleRate = s.sampleRate.load (std::memory_order_relaxed);
        out.bpm        = s.bpm.load (std::memory_order_relaxed);
        out.endPpq     = s.endPpq.load (std::memory_order_relaxed);
        out.endPos     = s.endPos.load (std::memory_order_relaxed);
        out.timeMode   = s.timeMode.load (std::memory_order_relaxed);
        out.playing    = s.playing.load (std::memory_order_relaxed) != 0;
    });
}

std::vector<SlotInfo> LinkBus::listAlive() const
{
    std::vector<SlotInfo> result;
    for (int i = 0; i < kMaxSlots; ++i)
    {
        SlotInfo info;
        if (getInfo (i, info))
            result.push_back (std::move (info));
    }
    return result;
}

int LinkBus::readRange (int slot, int64_t start, int len, float* out) const
{
    if (len <= 0)
        return 0;
    std::fill (out, out + len, 0.0f);

    SlotInfo info;
    if (! getInfo (slot, info))
        return 0;

    // Keep a safety margin away from the region the writer may currently overwrite.
    const int64_t margin = 16384;
    const int64_t validFrom = std::max<int64_t> (0, info.endPos - kRingSize + margin);
    const int64_t validTo = info.endPos; // exclusive
    const int64_t from = std::max (start, validFrom);
    const int64_t to = std::min (start + len, validTo);
    if (to <= from)
        return 0;

    const auto& s = header->slots[slot];
    int64_t p = from;
    while (p < to)
    {
        const auto idx = p & kRingMask;
        const auto chunk = std::min<int64_t> (to - p, kRingSize - idx);
        std::memcpy (out + (p - start), s.ring + idx, sizeof (float) * static_cast<size_t> (chunk));
        p += chunk;
    }
    return static_cast<int> (to - from);
}
} // namespace lowlink
