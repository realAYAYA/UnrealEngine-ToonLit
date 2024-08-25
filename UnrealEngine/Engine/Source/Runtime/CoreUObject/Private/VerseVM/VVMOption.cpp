// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMOption.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VOption);
TGlobalTrivialEmergentTypePtr<&VOption::StaticCppClassInfo> VOption::GlobalTrivialEmergentType;

template <typename TVisitor>
void VOption::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Value, TEXT("Value"));
}

uint32 VOption::GetTypeHashImpl()
{
	static constexpr uint32 MagicNumber = 0x9e3779b9;
	return ::HashCombineFast(static_cast<uint32>(MagicNumber), GetTypeHash(GetValue()));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)