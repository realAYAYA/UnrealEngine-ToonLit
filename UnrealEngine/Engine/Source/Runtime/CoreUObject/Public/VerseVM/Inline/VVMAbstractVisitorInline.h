// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/TypeCompatibleBytes.h"
#include "VerseVM/VVMAbstractVisitor.h"

class UObject;

namespace Verse
{

inline FAbstractVisitor::FReferrerToken::FReferrerToken(VCell* Cell)
	: EncodedBits(BitCast<uint64>(Cell) | static_cast<uint64>(EReferrerType::Cell))
{
}

inline FAbstractVisitor::FReferrerToken::FReferrerToken(UObject* Object)
	: EncodedBits(BitCast<uint64>(Object) | static_cast<uint64>(EReferrerType::UObject))
{
}

inline FAbstractVisitor::EReferrerType FAbstractVisitor::FReferrerToken::GetType() const
{
	return static_cast<EReferrerType>((EncodedBits & EncodingBits));
}

inline bool FAbstractVisitor::FReferrerToken::IsCell() const
{
	return GetType() == EReferrerType::Cell;
}

inline VCell* FAbstractVisitor::FReferrerToken::AsCell() const
{
	checkSlow(IsCell());
	return BitCast<VCell*>(EncodedBits & ~EncodingBits);
}

inline bool FAbstractVisitor::FReferrerToken::IsUObject() const
{
	return GetType() == EReferrerType::UObject;
}

inline UObject* FAbstractVisitor::FReferrerToken::AsUObject() const
{
	checkSlow(IsUObject());
	return BitCast<UObject*>(EncodedBits & ~EncodingBits);
}

inline FAbstractVisitor::FReferrerContext::FReferrerContext(FAbstractVisitor& InVisitor, FReferrerToken InReferrer)
	: Visitor(InVisitor)
	, Referrer(InReferrer)
{
	Previous = Visitor.Context;
	Visitor.Context = this;
}

inline FAbstractVisitor::FReferrerContext::~FReferrerContext()
{
	Visitor.Context = Previous;
}

} // namespace Verse
#endif // WITH_VERSE_VM
