// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMLink)

URigVMGraph* URigVMLink::GetGraph() const
{
	return Cast<URigVMGraph>(GetOuter());
}

void URigVMLink::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		Ar << SourcePinPath;
		Ar << TargetPinPath;
	}
	else
	{
		FString SourcePinPathTemp;
		if (SourcePin)
		{
			if (SourcePin->GetNode())
			{
				SourcePinPathTemp = SourcePin->GetPinPath();
			}
		}
		FString TargetPinPathTemp;
		if (TargetPin)
		{
			if (TargetPin->GetNode())
			{
				TargetPinPathTemp = TargetPin->GetPinPath();
			}
		}
		Ar << SourcePinPathTemp;
		Ar << TargetPinPathTemp;
	}
}

int32 URigVMLink::GetLinkIndex() const
{
	int32 Index = INDEX_NONE;
	URigVMGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		Graph->GetLinks().Find((URigVMLink*)this, Index);
	}
	return Index;

}

URigVMPin* URigVMLink::GetSourcePin() const
{
	if(SourcePin)
	{
		if(SourcePin->GetOutermost() == GetTransientPackage())
		{
			if(GetOutermost() != GetTransientPackage())
			{
				SourcePin = nullptr;
			}
		}
	}
	if (SourcePin == nullptr)
	{
		SourcePin = GetGraph()->FindPin(SourcePinPath);
	}
	return SourcePin;
}

URigVMPin* URigVMLink::GetTargetPin() const
{
	if(TargetPin)
	{
		if(TargetPin->GetOutermost() == GetTransientPackage())
		{
			if(GetOutermost() != GetTransientPackage())
			{
				TargetPin = nullptr;
			}
		}
	}
	if (TargetPin == nullptr)
	{
		TargetPin = GetGraph()->FindPin(TargetPinPath);
	}
	return TargetPin;
}

URigVMPin* URigVMLink::GetOppositePin(const URigVMPin* InPin) const
{
	if (InPin == GetSourcePin())
	{
		return GetTargetPin();
	}
	else if (InPin == GetTargetPin())
	{
		return GetSourcePin();
	}
	return nullptr;
}

FString URigVMLink::GetPinPathRepresentation()
{
	return GetPinPathRepresentation(GetSourcePin()->GetPinPath(), GetTargetPin()->GetPinPath());
}

FString URigVMLink::GetPinPathRepresentation(const FString& InSourcePinPath, const FString& InTargetPinPath)
{
	static constexpr TCHAR Format[] = TEXT("%s -> %s"); 
	return FString::Printf(Format, *InSourcePinPath, *InTargetPinPath);
}

bool URigVMLink::SplitPinPathRepresentation(const FString& InString, FString& OutSource, FString& OutTarget)
{
	static constexpr TCHAR Format[] = TEXT(" -> "); 
	return InString.Split(Format, &OutSource, &OutTarget);
}

void URigVMLink::PrepareForCopy()
{
	if (URigVMPin* CurrentSourcePin = GetSourcePin())
	{
		SourcePinPath = CurrentSourcePin->GetPinPath();
	}
	if (URigVMPin* CurrenTargetPin = GetTargetPin())
	{
		TargetPinPath = CurrenTargetPin->GetPinPath();
	}
}

