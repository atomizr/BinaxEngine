
#include "Timer.h"

namespace Binax {

Timer::Timer()
    : m_LastTime(std::chrono::steady_clock::now())
    , m_Delta(0.0f)
    , m_Elapsed(0.0f)
{
}

void Timer::Update()
{
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<float> rawDelta = now - m_LastTime;
    m_LastTime = now;

    float delta = rawDelta.count();

    
    if (delta > kMaxDelta)
        delta = kMaxDelta;

    m_Delta = delta;
    m_Elapsed += m_Delta;
}

} 