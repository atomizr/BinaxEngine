#pragma once

#include "imgui.h"

namespace Binax {

class Console
{
public:
    void Draw();

    void SetVisible(bool v) { m_Visible = v; }
    bool IsVisible() const  { return m_Visible; }
    void ToggleVisible()    { m_Visible = !m_Visible; }

private:
    bool m_Visible      = true;
    bool m_AutoScroll   = true;
    char m_Filter[128]  = {};
    int  m_MinLevel     = 0;
};

} 