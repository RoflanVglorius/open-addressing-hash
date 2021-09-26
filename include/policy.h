#pragma once
#include <cmath>

struct LinearProbing
{
    std::size_t current;
    std::size_t mod;

    LinearProbing(std::size_t start, std::size_t mod)
        : current(start)
        , mod(mod)
    {
    }

    std::size_t operator++()
    {
        current = (current + 1) % mod;
        return current;
    }
};
struct QuadraticProbing
{
    std::size_t current;
    std::size_t mod;
    std::size_t step_number = 1;

    QuadraticProbing(std::size_t start, std::size_t mod)
        : current(start)
        , mod(mod)
    {
    }

    std::size_t operator++()
    {
        current = (current + step_number * step_number) % mod;
        ++step_number;
        return current;
    }
};
