// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeCompatibleBytes.h"
#include "VVMMarkStack.h"
#include "VerseVM/VVMFloat.h"
#include <cinttypes>

class UObject;

namespace Verse
{
struct FPlaceholder;
struct VCell;
struct VContext;
struct VFrame;
struct VInt;
struct FAllocationContext;
struct FRunningContext;
struct VPlaceholder;
struct VSuspension;
struct FCellFormatter;
struct FOpResult;

struct VValue
{
	// Default constructor: initializes to an uninitialized sentry value.
	VValue()
		: EncodedBits(UninitializedValue)
	{
	}

	// Untagged value coercion constructors.
	VValue(VCell& Cell);

	VValue(UObject* Object);

	VValue(VInt Int);
	static VValue FromInt32(int32 Int32)
	{
		VValue Result;
		Result.EncodedBits = static_cast<uint32>(Int32) | Int32Tag;
		checkSlow(Result.IsInt32());
		return Result;
	}

	VValue(VFloat Float)
	{
		uint64 FloatAsU64 = Float.ReinterpretAsUInt64();
		EncodedBits = FloatAsU64 + FloatOffset;
		checkfSlow(FloatAsU64 <= MaxPureNaN, TEXT("Casting impure NaN to VValue: 0x%" PRIx64), FloatAsU64);
		checkSlow(IsFloat());
	}

	static VValue FromBool(bool Value);

	static VValue EffectDoneMarker()
	{
		return FromInt32(static_cast<int32>(0xeffec7));
	}

	// Copy constructor/assignment operator
	VValue(const VValue& Copyee)
		: EncodedBits(Copyee.EncodedBits)
	{
	}
	VValue& operator=(const VValue& Copyee)
	{
		EncodedBits = Copyee.EncodedBits;
		return *this;
	}

	// Note: This isn't what you want if you want a deep equality check.
	bool operator==(const VValue& Other) const { return EncodedBits == Other.EncodedBits; }
	bool operator!=(const VValue& Other) const { return !(*this == Other); }
	explicit operator bool() const { return EncodedBits != UninitializedValue; }

	// Note: This is what you want if you want a deep equality check.
	// This will return true if left and/or right are placeholders.
	template <typename ContextType, typename HandlePlaceholderFunction>
	static bool Equal(ContextType Context, VValue Left, VValue Right, HandlePlaceholderFunction HandlePlaceholder);

	template <typename ContextType>
	static FOpResult Melt(ContextType Context, VValue Value);
	template <typename ContextType>
	static FOpResult Freeze(ContextType Context, VValue Value);

	static VValue Decode(uint64 EncodedBits)
	{
		VValue Result;
		Result.EncodedBits = EncodedBits;
		return Result;
	}

	VValue(VPlaceholder&) = delete;
	bool IsPlaceholder() const { return (EncodedBits & NonCellTagMask) == PlaceholderTag; }
	static VValue Placeholder(const VPlaceholder& Placeholder)
	{
		VValue Result = VValue::Decode(BitCast<uint64>(&Placeholder) | PlaceholderTag);
		checkSlow(Result.IsPlaceholder());
		return Result;
	}

	VValue Follow();
	// This assumes we're a placeholder, does Follow(), and assumes we don't point
	// (even transitively) to a concrete value. So we must be a placeholder and the
	// placeholder we point at (transitively) isn't resolved.
	VPlaceholder& GetRootPlaceholder(); // This does Follow()
	VPlaceholder& AsPlaceholder() const
	{
		checkSlow(IsPlaceholder());
		return *BitCast<VPlaceholder*>(EncodedBits & ~PlaceholderTag);
	}

	bool IsInt32() const { return (EncodedBits & NumberTagMask) == Int32Tag; }
	bool IsUint32() const;
	uint32 AsUint32() const;

	// Returns Value as C++ style int32
	int32 AsInt32() const
	{
		checkSlow(IsInt32());
		return Bits.Payload;
	}

	bool IsCell() const { return !(EncodedBits & NonCellTagMask) && EncodedBits != UninitializedValue; }
	VCell& AsCell() const
	{
		checkSlow(IsCell());
		return *Cell;
	}

	VCell* ExtractCell()
	{
		if (IsCell())
		{
			return Cell;
		}
		if (IsPlaceholder())
		{
			return reinterpret_cast<VCell*>(&AsPlaceholder());
		}
		else
		{
			return nullptr;
		}
	}

	template <typename ObjectType>
	bool IsCellOfType() const;

	template <typename ObjectType>
	ObjectType& StaticCast() const;

	template <typename ObjectType>
	ObjectType* DynamicCast() const;

	bool IsUObject() const { return (EncodedBits & NonCellTagMask) == UObjectTag; }
	UObject* AsUObject() const
	{
		checkSlow(IsUObject());
		return BitCast<UObject*>(EncodedBits & ~UObjectTag);
	}

	bool IsInt() const;
	VInt AsInt() const;

	bool IsLogic() const;

	// Returns Value as C++ style bool
	bool AsBool() const;

	bool IsFloat() const
	{
		return (EncodedBits & NumberTagMask) && (EncodedBits & NumberTagMask) != Int32Tag;
	}
	VFloat AsFloat() const
	{
		checkSlow(IsFloat());
		return VFloat(BitCast<double>(EncodedBits - FloatOffset));
	}

	bool IsEnumerator() const;

	bool IsUninitialized() const { return EncodedBits == UninitializedValue; }

	uint64 GetEncodedBits() const { return EncodedBits; }

	void EnqueueSuspension(FRunningContext Context, VSuspension& Suspension);

	FString ToString(FAllocationContext Context, const FCellFormatter& Formatter) const;
	void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter) const;

	static VValue Char(uint8 V)
	{
		VValue R;
		R.EncodedBits = (static_cast<uint64>(V) << NumLowerEncodingBits) | VValue::CharTag;
		return R;
	}
	static VValue Char32(uint32 V)
	{
		VValue R;
		R.EncodedBits = (static_cast<uint64>(V) << NumLowerEncodingBits) | VValue::Char32Tag;
		return R;
	}

	bool IsChar() const { return (EncodedBits & VValue::NonCellTagMask) == VValue::CharTag; }
	bool IsChar32() const { return (EncodedBits & VValue::NonCellTagMask) == VValue::Char32Tag; }

	uint8 AsChar() const
	{
		checkSlow(IsChar());
		return static_cast<uint8>(EncodedBits >> NumLowerEncodingBits);
	}
	uint32 AsChar32() const
	{
		checkSlow(IsChar32());
		return static_cast<uint32>(EncodedBits >> NumLowerEncodingBits);
	}

private:
	friend struct VRestValue;

	union
	{
		uint64 EncodedBits;
		VCell* Cell;

		struct
		{
			int32 Payload;
			int32 Tag;
		} Bits;
	};

	static VValue Root(uint16 SplitDepth)
	{
		VValue Result = VValue::Decode(RootTag | (static_cast<uint64>(SplitDepth) << 32));
		return Result;
	}

	bool IsRoot() const { return (EncodedBits & VValue::NonCellTagMask) == VValue::RootTag; }

	uint16 GetSplitDepth() const
	{
		checkSlow(IsRoot());
		return static_cast<uint16>(EncodedBits >> 32);
	}

public:
	// VValue is a NaN-boxed 64-bit payload representing the fundamental value type in the
	// Verse VM. NaN-boxing allows us to represent immediates like int32s and floats,
	// and also pointers like VCell, all in a 64-bit payload. We do this by encoding
	// non-floats in the IEEE754 NaN space. E.g, all non-double VValues will be NaN when
	// you subtract FloatOffset from them. This means we need to be careful with the
	// actual set of values we use to store NaN. The NaNs that are safe to be boxed
	// as a float we call PureNaNs.
	//
	// On x86-64 and arm64, doing math on PureNaNs has the following properties:
	// - When doing binary operations involving one PureNaN, the output is again a PureNaN.
	// - The exact value you get on newly indefinite results (e.g, Inf - Inf) may vary in the sign, but is a PureNaN.
	// This means that when doing arithmetic on VFloats, which can't be an impure NaN,
	// we don't need to purify the result of the arithmetic operation when boxing them
	// as a VValue. However, when boxing an unknown double value as a VValue, we need to
	// purify any incoming NaNs.

	// Encoding space by top 16 bits:
	// 0x0000... Cell/Placeholder/UObject/Root (see below)
	// 0x0001... \
	// ...        float
	// 0xfffc... /
	// 0xfffd... unused
	// 0xfffe... unused
	// 0xffff... int32

	// If the top 16 bits are 0000, the lower 4 bits encode as follows:
	// 0b0000... Cell
	// 0b0001... Placeholder
	// 0b0010... Root
	// 0b0011... UObject
	// 0b0100... char8
	// 0b0101... char32
	// 0b0110... unused
	// 0b0111... unused
	// 0b1XXX... unused

	// VValue assumes a 48-bit address space for pointers.

	// Special code which means this VValue is uninitialized
	static constexpr uint64 UninitializedValue = 0;

	// Number-related constants
	static constexpr uint64 NumberTagMask = 0xffff'0000'0000'0000ull;
	static constexpr uint64 Int32Tag = 0xffff'0000'0000'0000ull;
	static constexpr uint64 FloatOffset = 0x0001'0000'0000'0000ull;
	static constexpr uint64 MaxPureNaN = 0xfffb'ffff'ffff'ffffull;
	static constexpr uint64 MaxFloatTag = 0xfffc'0000'0000'0000ull;

	// Non-number related constants
	static constexpr uint64 NumLowerEncodingBits = 4;
	static constexpr uint64 NonCellTagMask = NumberTagMask | 0xfull; // Used for Cell, Placeholder, Root, UObject
	static constexpr uint64 PlaceholderTag = 0x1ull;
	static constexpr uint64 RootTag = 0x2ull;
	static constexpr uint64 UObjectTag = 0x3ull;
	static constexpr uint64 CharTag = 0x4ull;
	static constexpr uint64 Char32Tag = 0x5ull;
};

} // namespace Verse
#endif // WITH_VERSE_VM
