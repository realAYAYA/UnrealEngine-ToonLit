// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMIntrinsics.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VIntrinsics);
TGlobalTrivialEmergentTypePtr<&VIntrinsics::StaticCppClassInfo> VIntrinsics::GlobalTrivialEmergentType;

FNativeCallResult VIntrinsics::AbsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 1); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	V_RETURN(Arguments[0].IsFloat()
				 ? VValue(VFloat(FMath::Abs(Arguments[0].AsFloat().AsDouble())))
				 : VValue(VInt::Abs(Context, VInt(Arguments[0]))));
}

FNativeCallResult VIntrinsics::CeilImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 1); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	VRational& Argument = Arguments[0].StaticCast<VRational>();
	V_RETURN(Argument.Ceil(Context));
}

FNativeCallResult VIntrinsics::FloorImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 1); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	VRational& Argument = Arguments[0].StaticCast<VRational>();
	V_RETURN(Argument.Floor(Context));
}

FNativeCallResult VIntrinsics::ConcatenateMapsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 2); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	V_REQUIRE_CONCRETE(Arguments[1]);
	VMap& Lhs = Arguments[0].StaticCast<VMap>();
	VMap& Rhs = Arguments[1].StaticCast<VMap>();
	V_RETURN(VMapBase::New<VMap>(Context, Lhs.Num() + Rhs.Num(), [&](int32 I) {
		if (I < Lhs.Num())
		{
			return TPair<VValue, VValue>{Lhs.GetKey(I), Lhs.GetValue(I)};
		}
		checkSlow(I >= Lhs.Num());
		return TPair<VValue, VValue>{Rhs.GetKey(I - Lhs.Num()), Rhs.GetValue(I - Lhs.Num())};
	}));
}

template <typename TVisitor>
void VIntrinsics::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Abs, TEXT("Abs"));
	Visitor.Visit(Ceil, TEXT("Ceil"));
	Visitor.Visit(Floor, TEXT("Floor"));
	Visitor.Visit(ConcatenateMaps, TEXT("ConcatenateMaps"));
}

} // namespace Verse
#endif
