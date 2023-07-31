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
	if (SourcePin == nullptr)
	{
		SourcePin = GetGraph()->FindPin(SourcePinPath);
	}
	return SourcePin;
}

URigVMPin* URigVMLink::GetTargetPin() const
{
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
	return FString::Printf(TEXT("%s -> %s"), *GetSourcePin()->GetPinPath(), *GetTargetPin()->GetPinPath());
}

bool URigVMLink::SplitPinPathRepresentation(const FString& InString, FString& OutSource, FString& OutTarget)
{
	return InString.Split(TEXT(" -> "), &OutSource, &OutTarget);
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

