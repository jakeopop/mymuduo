#pragma once

template<typename To, typename From>
inline To implicit_cast(From const &f)
{
    return f;
}