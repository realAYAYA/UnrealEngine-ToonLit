// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>

namespace AutoRTFM
{

inline void Fence()
{
    atomic_thread_fence(std::memory_order_seq_cst);
}

inline void CompilerFence()
{
    atomic_signal_fence(std::memory_order_seq_cst);
}

template<typename T, typename U, typename V>
bool WeakUnfencedCAS(T& Ref, U PassedExpectedValue, V PassedNewValue)
{
    T ExpectedValue = PassedExpectedValue;
    T NewValue = PassedNewValue;
    std::atomic<T>& AtomicRef = reinterpret_cast<std::atomic<T>&>(Ref);
    return AtomicRef.compare_exchange_weak(ExpectedValue, NewValue, std::memory_order_relaxed);
}

template<typename T, typename U, typename V>
bool WeakFencedCAS(T& Ref, U PassedExpectedValue, V PassedNewValue)
{
    T ExpectedValue = PassedExpectedValue;
    T NewValue = PassedNewValue;
    std::atomic<T>& AtomicRef = reinterpret_cast<std::atomic<T>&>(Ref);
    return AtomicRef.compare_exchange_weak(ExpectedValue, NewValue);
}

template<typename T, typename U, typename V>
T StrongFencedCAS(T& Ref, U PassedExpectedValue, V PassedNewValue)
{
    T ExpectedValue = PassedExpectedValue;
    T NewValue = PassedNewValue;
    std::atomic<T>& AtomicRef = reinterpret_cast<std::atomic<T>&>(Ref);
    AtomicRef.compare_exchange_weak(ExpectedValue, NewValue);
    return ExpectedValue;
}

template<typename T>
T FencedLoad(T& Ref)
{
    std::atomic<T>& AtomicRef = reinterpret_cast<std::atomic<T>&>(Ref);
    return AtomicRef;
}

template<typename T>
T LoadAcquire(T& Ref)
{
    std::atomic<T>& AtomicRef = reinterpret_cast<std::atomic<T>&>(Ref);
    return AtomicRef.load(std::memory_order_acquire);
}

template<typename T>
void FencedStore(T& Ref, T Value)
{
    std::atomic<T>& AtomicRef = reinterpret_cast<std::atomic<T>&>(Ref);
    AtomicRef = Value;
}

class FFenceGuard
{
public:
    FFenceGuard() { Fence(); }
    ~FFenceGuard() { Fence(); }
};

} // namespace AutoRTFM
