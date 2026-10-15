
#pragma once

#include <chrono>

namespace Binax {

class Timer
{
public:
    Timer();

    void Update();
    float GetDelta() const { return m_Delta; }
    float GetElapsed() const { return m_Elapsed; }

    
    
    static constexpr float kMaxDelta = 0.1f;   

private:
    std::chrono::steady_clock::time_point m_LastTime;
    float m_Delta;
    float m_Elapsed;
};

} 