// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMRestValue.h"

namespace Verse
{
void FAbstractVisitor::VisitNonNull(VCell*& InCell, const TCHAR* ElementName)
{
}

void FAbstractVisitor::VisitNonNull(UObject*& InObject, const TCHAR* ElementName)
{
}

void FAbstractVisitor::VisitAuxNonNull(void* InAux, const TCHAR* ElementName)
{
}

void FAbstractVisitor::Visit(bool& bValue, const TCHAR* ElementName)
{
}

void FAbstractVisitor::Visit(FString& Value, const TCHAR* ElementName)
{
}

void FAbstractVisitor::Visit(uint64& Value, const TCHAR* ElementName)
{
}

void FAbstractVisitor::Visit(int64& Value, const TCHAR* ElementName)
{
}

void FAbstractVisitor::BeginArray(const TCHAR* ElementName, uint64& NumElements)
{
}

void FAbstractVisitor::EndArray()
{
}

void FAbstractVisitor::BeginSet(const TCHAR* ElementName, uint64& NumElements)
{
}

void FAbstractVisitor::EndSet()
{
}

void FAbstractVisitor::BeginMap(const TCHAR* ElementName, uint64& NumElements)
{
}

void FAbstractVisitor::EndMap()
{
}

void FAbstractVisitor::BeginObject(const TCHAR* ElementName)
{
}

void FAbstractVisitor::EndObject()
{
}

void FAbstractVisitor::VisitEmergentType(const VCell* InEmergentType)
{
	VCell* Scratch = const_cast<VCell*>(InEmergentType);
	VisitNonNull(Scratch, TEXT("EmergentType"));
}

void FAbstractVisitor::Visit(VCell*& InCell, const TCHAR* ElementName)
{
	if (InCell != nullptr)
	{
		VisitNonNull(InCell, ElementName);
	}
}

void FAbstractVisitor::Visit(UObject*& InObject, const TCHAR* ElementName)
{
	if (InObject != nullptr)
	{
		VisitNonNull(InObject, ElementName);
	}
}

void FAbstractVisitor::VisitAux(void* InAux, const TCHAR* ElementName)
{
	if (InAux != nullptr)
	{
		VisitAuxNonNull(InAux, ElementName);
	}
}

void FAbstractVisitor::Visit(VValue& Value, const TCHAR* ElementName)
{
	if (VCell* Cell = Value.ExtractCell())
	{
		Visit(Cell, ElementName);
	}
	else if (Value.IsUObject())
	{
		UObject* Object = Value.AsUObject();
		Visit(Object, ElementName);
	}
}

void FAbstractVisitor::Visit(VRestValue& Value, const TCHAR* ElementName)
{
	Value.Visit(*this, ElementName);
}

FArchive* FAbstractVisitor::GetUnderlyingArchive()
{
	return nullptr;
}

bool FAbstractVisitor::IsLoading()
{
	return false;
}

bool FAbstractVisitor::IsTextFormat()
{
	return false;
}

FAccessContext FAbstractVisitor::GetLoadingContext()
{
	V_DIE("Subclass must implement GetLoadingContext when loading");
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
