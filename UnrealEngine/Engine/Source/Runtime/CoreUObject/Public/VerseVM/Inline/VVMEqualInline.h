// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMOption.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
template <typename ContextType, typename HandlePlaceholderFunction>
inline bool VValue::Equal(ContextType Context, VValue Left, VValue Right, HandlePlaceholderFunction HandlePlaceholder)
{
	if (Left.IsPlaceholder() || Right.IsPlaceholder())
	{
		HandlePlaceholder(Left, Right);
		return true;
	}
	else if (Left == Right)
	{
		return true;
	}
	else if (Left.IsFloat() && Right.IsFloat())
	{
		return Left.AsFloat() == Right.AsFloat();
	}
	else if (Left.IsInt() || Right.IsInt())
	{
		return Left.IsInt() && Right.IsInt() && VInt::Eq(Context, Left.AsInt(), Right.AsInt());
	}
	else if (Left.IsLogic() || Right.IsLogic())
	{
		return Left.IsLogic() && Right.IsLogic() && Left.AsBool() == Right.AsBool();
	}
	else if (Left.IsEnumerator() || Right.IsEnumerator())
	{
		checkSlow(Left != Right);
		return false;
	}
	else if (Left.IsCell() && Right.IsCell())
	{
		VCell* LeftCell = &Left.AsCell();
		VCell* RightCell = &Right.AsCell();

		if (LeftCell->IsA<VUTF8String>())
		{
			return RightCell->IsA<VUTF8String>()
				&& LeftCell->StaticCast<VUTF8String>() == RightCell->StaticCast<VUTF8String>();
		}
		else if (LeftCell->IsA<VOption>())
		{
			return RightCell->IsA<VOption>()
				&& Equal(Context, LeftCell->StaticCast<VOption>().GetValue(), RightCell->StaticCast<VOption>().GetValue(), HandlePlaceholder);
		}

		// This call may do a TLS lookup for the context, calls not requiring one should be inlined above.
		return LeftCell->Equal(FRunningContext(Context), RightCell, HandlePlaceholder);
	}

	return false;
}

} // namespace Verse
#endif // WITH_VERSE_VM
