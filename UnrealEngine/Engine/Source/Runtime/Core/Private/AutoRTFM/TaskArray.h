// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

namespace AutoRTFM
{

template<typename T> class TBackwards final
{
	T& Obj;
public:
	TBackwards(T& Obj) : Obj(Obj) {}
	auto begin() { return Obj.rbegin(); }
	auto end() { return Obj.rend(); }
};

template<typename T>
class TTaskArray
{
	template<typename U> using USharedPtr = TSharedPtr<U, ESPMode::NotThreadSafe>;

public:
	TTaskArray() : Latest(new TArray<T>()) {}
	TTaskArray(const TTaskArray&) = delete;
	void operator=(const TTaskArray&) = delete;

    bool IsEmpty() const { return Latest->IsEmpty() && Stash.IsEmpty(); }

    void Add(T&& value)
    {
        Latest->Push(MoveTemp(value));
    }

    void Add(const T& value)
    {
        Latest->Push(value);
    }

    void AddAll(TTaskArray<T>&& Other)
    {
        Canonicalize();

        for (USharedPtr<TArray<T>>& StashedVectorBox : Other.Stash)
        {
            Stash.Push(MoveTemp(StashedVectorBox));
        }

        Other.Stash.Empty();

        if (!Other.Latest->IsEmpty())
        {
            Stash.Push(MoveTemp(Other.Latest));
			USharedPtr<TArray<T>> NewLatest(new TArray<T>());
			Other.Latest = MoveTemp(NewLatest);
        }
    }

    void AddAll(const TTaskArray<T>& Other)
    {
        Canonicalize();
        Other.Canonicalize();
        for (const USharedPtr<TArray<T>>& StashedVectorBox : Other.Stash)
        {
            Stash.Push(StashedVectorBox);
        }


		ASSERT(Latest != nullptr);
    }

    template<typename TFunc>
    bool ForEachForward(const TFunc& Func) const
    {
        for (USharedPtr<TArray<T>>& StashedVectorBox : Stash)
        {
            for (const T& Entry : *StashedVectorBox)
            {
                if (!Func(Entry))
                {
                    return false;
                }
            }
        }

        for (const T& Entry : *Latest)
        {
            if (!Func(Entry))
            {
                return false;
            }
        }
        return true;
    }

    template<typename TFunc>
    bool ForEachBackward(const TFunc& Func) const
    {
		for (const T& Entry : TBackwards(*Latest))
		{
			if (!Func(Entry))
			{
				return false;
			}
		}

		for (USharedPtr<TArray<T>>& StashedVectorBox : TBackwards(Stash))
		{
			for (const T& Entry : TBackwards(*StashedVectorBox))
			{
				if (!Func(Entry))
				{
					return false;
				}
			}
		}

        return true;
    }

    void Reset()
    {
        Latest->Empty();
        Stash.Empty();
    }

    size_t Num() const
    {
        size_t Result = Latest->Num();

        for (size_t Index = 0; Index < Stash.Num(); Index++)
        {
            const TArray<T>& StashedVector = *Stash[Index];
            Result += StashedVector.Num();
        }

        return Result;
    }

private:
    // We don't want to do this too often; currently we just do it where it's asymptitically relevant like AddAll. This doesn't
    // logically change the TaskArray but it changes its internal representation. Hence the use of `mutable` and hence why this
    // method is `const`.
    void Canonicalize() const
    {
        if (!Latest->IsEmpty())
        {
			Stash.Push(MoveTemp(Latest));
			USharedPtr<TArray<T>> NewLatest(new TArray<T>());
			Latest = MoveTemp(NewLatest);
        }

		ASSERT(Latest != nullptr);
    }
    
    mutable USharedPtr<TArray<T>> Latest;
    
    mutable TArray<USharedPtr<TArray<T>>> Stash;
};

} // namespace AutoRTFM
