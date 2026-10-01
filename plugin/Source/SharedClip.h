#pragma once

#include <amt/AudioClip.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <memory>
#include <vector>

namespace amt::plugin
{

/** Hands an immutable object from the message thread to the audio thread.
    The audio thread only copies the shared_ptr; objects that are replaced are parked and freed
    later on the message thread, so the audio thread never deallocates. */
template <typename T>
class SharedObject
{
public:
    void set (std::shared_ptr<const T> object)
    {
        JUCE_ASSERT_MESSAGE_THREAD
        auto previous = std::atomic_exchange (&current, std::move (object));
        if (previous != nullptr)
            retired.push_back (std::move (previous));
        collectGarbage();
    }

    /** Safe on any thread. */
    std::shared_ptr<const T> get() const noexcept { return std::atomic_load (&current); }

    /** Frees parked objects nobody else holds any more (message thread). */
    void collectGarbage()
    {
        retired.erase (std::remove_if (retired.begin(), retired.end(), [] (const auto& p) { return p.use_count() <= 1; }),
                       retired.end());
    }

private:
    std::shared_ptr<const T> current;
    std::vector<std::shared_ptr<const T>> retired;
};

using SharedClip = SharedObject<AudioClip>;

} // namespace amt::plugin
