#ifndef TIMER_HPP
#define TIMER_HPP

#include <SDL_timer.h>

#include "singleton.hpp"

namespace TK
{

class Timer : public Singleton<Timer> {
    Uint64 mTimeValue;

public:
    Timer() : mTimeValue(0) { }

    // The tick count is relative to an unknown point in time. It is not saved
    // between runs and is only used for relative time tracking.
    static Uint32 getTickCount() { return SDL_GetTicks(); }

    // The returned time value is the game time in ticks. It is preserved over
    // a save+load cycle.
    Uint64 getValue() const { return mTimeValue; }
    void setValue(Uint64 value) { mTimeValue = value; }

    Timer& operator+=(Uint64 value) { mTimeValue += value; return *this; }

    static Uint32 TicksPerSecond() { return 1000; }
    static double AsSeconds(Uint32 value) { return value / 1000.0; }
};

} // namespace TK

#endif /* TIMER_HPP */
