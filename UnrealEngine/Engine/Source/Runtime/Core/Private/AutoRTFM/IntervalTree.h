// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include "Containers/PagedArray.h"
#include "Utils.h"

namespace AutoRTFM
{

template<typename T> struct TIntervalTreeArray final
{
	FORCEINLINE void Empty()
	{
		Payload.Empty();
	}

	FORCEINLINE uint32 Num() const
	{
		return Payload.Num();
	}

	FORCEINLINE bool IsEmpty() const
	{
		return Payload.IsEmpty();
	}

	FORCEINLINE void Push(T Thing)
	{
		Payload.Push(Thing);
	}

	FORCEINLINE uint32 Add(T Thing)
	{
		return Payload.Add(Thing);
	}

	FORCEINLINE T Pop()
	{
		return Payload.Pop();
	}

	FORCEINLINE T& operator[](uint32 Index)
	{
		// A bit nasty - but since we know we are in bounds, skip the check.
		// This change alone accounted for a 13% uplift in performance in a
		// tight loop of critical performance importance!
		return Payload.GetData()[Index];
	}

	FORCEINLINE const T& operator[](uint32 Index) const
	{
		// Same as non-const version.
		return Payload.GetData()[Index];
	}

private:
	TArray<T> Payload;
};

struct FIntervalTree final
{
    FIntervalTree() = default;

    FIntervalTree(const FIntervalTree&) = delete;
    FIntervalTree& operator=(const FIntervalTree&) = delete;

	FORCENOINLINE bool Insert(const void* const Address, const size_t Size)
    {        
        FRange NewRange(Address, Size);
        return Insert(MoveTemp(NewRange));
    }

    FORCENOINLINE bool Contains(const void* const Address, const size_t Size) const
    {
        const FRange NewRange(Address, Size);

        if (UNLIKELY(IntervalTreeNodeIndexNone == Root))
        {
            return false;
        }

        FIntervalTreeNodeIndex Current = Root;

        do
        {
            const FIntervalTreeNode& Node = Nodes[Current];

            const FRange Range = Node.Range;

            // This check does not need to prove that NewRange is entirely
            // enclosed within Range, because if any byte of NewRange was in the
            // original Range then it **must** already have been new memory.
            if ((NewRange.Start < Range.End) && (Range.Start < NewRange.End))
            {
                return true;
            }

			Current = (NewRange.Start < Range.Start) ? Node.Left : Node.Right;
        } while (IntervalTreeNodeIndexNone != Current);

        return false;
    }

    bool IsEmpty() const
    {
        return IntervalTreeNodeIndexNone == Root;
    }

    void Reset()
    {
        Root = IntervalTreeNodeIndexNone;
        Nodes.Empty();
    }

    void Merge(const FIntervalTree& Other)
    {
        if (Other.IsEmpty())
        {
            return;
        }

		ArrayType<FIntervalTreeNodeIndex> ToProcess;

        ToProcess.Push(Other.Root);

        do
        {
            const FIntervalTreeNodeIndex Current = ToProcess.Pop();

            FRange Range = Other.Nodes[Current].Range;

            Insert(MoveTemp(Range));

            const FIntervalTreeNodeIndex Left = Other.Nodes[Current].Left;
            const FIntervalTreeNodeIndex Right = Other.Nodes[Current].Right;

            if (IntervalTreeNodeIndexNone != Left)
            {
                ToProcess.Push(Left);
            }

            if (IntervalTreeNodeIndexNone != Right)
            {
                ToProcess.Push(Right);
            }
        } while (!ToProcess.IsEmpty());
    }

private:
	template<typename T> using ArrayType = TIntervalTreeArray<T>;

    struct FRange final
    {
        FRange(const void* const Address, const size_t Size) :
            Start(reinterpret_cast<uintptr_t>(Address)), End(reinterpret_cast<uintptr_t>(Address) + Size) {}

        uintptr_t Start;
        uintptr_t End;
    };

    using FIntervalTreeNodeIndex = uint32_t;

    static constexpr FIntervalTreeNodeIndex IntervalTreeNodeIndexNone = UINT32_MAX;

    struct FIntervalTreeNode final
    {
        explicit FIntervalTreeNode(const FRange Range) :
            Range(Range), Parent(IntervalTreeNodeIndexNone), bIsBlack(true) {}

		FIntervalTreeNode(const FRange Range, FIntervalTreeNodeIndex Parent) :
			Range(Range), Parent(Parent), bIsBlack(false) {}

		FRange Range;
        FIntervalTreeNodeIndex Left = IntervalTreeNodeIndexNone;
        FIntervalTreeNodeIndex Right = IntervalTreeNodeIndexNone;
        FIntervalTreeNodeIndex Parent;
        bool bIsBlack;
    };

    FIntervalTreeNodeIndex Root = IntervalTreeNodeIndexNone;
	ArrayType<FIntervalTreeNode> Nodes;

	static constexpr bool bExtraDebugging = false;

    FORCENOINLINE bool Insert(FRange&& NewRange)
    {
        if (UNLIKELY(IntervalTreeNodeIndexNone == Root))
        {
            ASSERT(0 == Nodes.Num());
            Root = Nodes.Add(FIntervalTreeNode(NewRange));
            return true;
        }

        FIntervalTreeNodeIndex Current = Root;

        for(;;)
        {
            const FRange Range = Nodes[Current].Range;

            if (UNLIKELY((NewRange.Start < Range.End) && (Range.Start < NewRange.End)))
            {
                return false;
            }

            if (NewRange.Start < Range.Start)
            {
                if (UNLIKELY(NewRange.End == Range.Start))
                {
                    ASSERT(NewRange.Start < Range.Start);

					// We can just modify the existing node in place.
					Nodes[Current].Range.Start = NewRange.Start;
                    return true;
                }
                else if (IntervalTreeNodeIndexNone == Nodes[Current].Left)
                {
                    const FIntervalTreeNodeIndex Index = Nodes.Add(FIntervalTreeNode(NewRange, Current));
					Nodes[Current].Left = Index;
                    Current = Index;
                    break;
                }

                Current = Nodes[Current].Left;
            }
            else
            {
                if (UNLIKELY(NewRange.Start == Range.End))
                {
                    ASSERT(NewRange.End > Range.End);

					// We can just modify the existing node in place.
					Nodes[Current].Range.End = NewRange.End;
                    return true;
                }
                else if (IntervalTreeNodeIndexNone == Nodes[Current].Right)
                {
                    const FIntervalTreeNodeIndex Index = Nodes.Add(FIntervalTreeNode(NewRange, Current));
					Nodes[Current].Right = Index;
                    Current = Index;
                    break;
                }

                Current = Nodes[Current].Right;
            }

            ASSERT(Root != Current);
        }

        auto IsBlack = [this](FIntervalTreeNodeIndex Index)
        {
            return IntervalTreeNodeIndexNone == Index || Nodes[Index].bIsBlack;
        };

        for(;;)
        {
            FIntervalTreeNodeIndex Parent = Nodes[Current].Parent;

            UE_ASSUME(Current != Parent);

            // The root will always have a black parent, so this check covers both.
            if (Parent == Root)
            {
                Nodes[Parent].bIsBlack = true;
                break;
            }
            else if (IsBlack(Parent))
            {
                break;
            }

            const FIntervalTreeNodeIndex GrandParent = Nodes[Parent].Parent;

            UE_ASSUME((Parent != GrandParent) && (Current != GrandParent));

            const bool bGrandParentIsRoot = GrandParent == Root;

            const bool bParentIsLeft = Nodes[GrandParent].Left == Parent;

            // The uncle is the other node of our parent.
            const FIntervalTreeNodeIndex Uncle = bParentIsLeft ? Nodes[GrandParent].Right : Nodes[GrandParent].Left;

            UE_ASSUME((GrandParent != Uncle) && (Parent != Uncle) && (Current != Uncle));

            if (!IsBlack(Uncle))
            {
                ASSERT(IsBlack(GrandParent));
                Nodes[Parent].bIsBlack = true;
                Nodes[Uncle].bIsBlack = true;

                if (bGrandParentIsRoot)
                {
                    break;
                }
                else
                {
                    Nodes[GrandParent].bIsBlack = false;
                }

                Current = GrandParent;
                continue;
            }

            // Our uncle is black, so we need to swizzle around.
            const bool CurrentIsLeft = Nodes[Parent].Left == Current;

            if (bParentIsLeft)
            {
                if (!CurrentIsLeft)
                {
                    Nodes[Parent].Right = Nodes[Current].Left;

                    if (IntervalTreeNodeIndexNone != Nodes[Parent].Right)
                    {
                        Nodes[Nodes[Parent].Right].Parent = Parent;
                    }

                    Nodes[Parent].Parent = Current;
                    Nodes[Current].Parent = GrandParent;
                    Nodes[Current].Left = Parent;
                    Nodes[GrandParent].Left = Current;
                    std::swap(Parent, Current);
                }

                Nodes[Parent].Parent = Nodes[GrandParent].Parent;
                Nodes[GrandParent].Left = Nodes[Parent].Right;

                if (IntervalTreeNodeIndexNone != Nodes[GrandParent].Left)
                {
                    Nodes[Nodes[GrandParent].Left].Parent = GrandParent;
                }

                Nodes[Parent].Right = GrandParent;
                Nodes[GrandParent].Parent = Parent;

                std::swap(Nodes[GrandParent].bIsBlack, Nodes[Parent].bIsBlack);
            }
            else
            {
                if (CurrentIsLeft)
                {
                    Nodes[Parent].Left = Nodes[Current].Right;
                    
                    if (IntervalTreeNodeIndexNone != Nodes[Parent].Left)
                    {
                        Nodes[Nodes[Parent].Left].Parent = Parent;
                    }

                    Nodes[Parent].Parent = Current;
                    Nodes[Current].Parent = GrandParent;
                    Nodes[Current].Right = Parent;
                    Nodes[GrandParent].Right = Current;
                    std::swap(Parent, Current);
                }

                Nodes[Parent].Parent = Nodes[GrandParent].Parent;
                Nodes[GrandParent].Right = Nodes[Parent].Left;

                if (IntervalTreeNodeIndexNone != Nodes[GrandParent].Right)
                {
                    Nodes[Nodes[GrandParent].Right].Parent = GrandParent;
                }

                Nodes[Parent].Left = GrandParent;
                Nodes[GrandParent].Parent = Parent;

                std::swap(Nodes[GrandParent].bIsBlack, Nodes[Parent].bIsBlack);
            }

            if (bGrandParentIsRoot)
            {
                Root = Parent;
            }
            else
            {
                FIntervalTreeNode& GreatGrandParent = Nodes[Nodes[Parent].Parent];
                if (GrandParent == GreatGrandParent.Left)
                {
                    GreatGrandParent.Left = Parent;
                }
                else
                {
                    GreatGrandParent.Right = Parent;
                }
            }

            break;
        }

		AssertStructureIsOk();

        return true;
    }

	UE_AUTORTFM_FORCEINLINE void AssertStructureIsOk() const
	{
		if constexpr (bExtraDebugging)
		{
			if (IntervalTreeNodeIndexNone != Root)
			{
				AssertNodeIsOk(Root);
			}
		}
	}

	FORCENOINLINE void AssertNodeIsOk(FIntervalTreeNodeIndex Index) const
	{
		// We need to use recursion to check because we cannot have any
		// allocations within the checker itself!
		if constexpr (bExtraDebugging)
		{
			ASSERT(IntervalTreeNodeIndexNone != Index);
			ASSERT(Index < Nodes.Num());

			const FIntervalTreeNodeIndex Parent = Nodes[Index].Parent;
			const FIntervalTreeNodeIndex Left = Nodes[Index].Left;
			const FIntervalTreeNodeIndex Right = Nodes[Index].Right;

			if (IntervalTreeNodeIndexNone == Parent)
			{
				ASSERT(Root == Index);
			}
			else
			{
				ASSERT(Nodes[Parent].bIsBlack || Nodes[Index].bIsBlack);
				ASSERT((Nodes[Parent].Left == Index) ^ (Nodes[Parent].Right == Index));
			}

			ASSERT(Left != Index);
			ASSERT(Right != Index);

			if (IntervalTreeNodeIndexNone != Left)
			{
				AssertNodeIsOk(Left);
			}

			if (IntervalTreeNodeIndexNone != Right)
			{
				AssertNodeIsOk(Right);
			}
		}
	}
};

} // namespace AutoRTFM
