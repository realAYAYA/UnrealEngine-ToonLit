// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMDebugInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDebugInfo)

void FRigVMDebugInfo::ResetState()
{
	BreakpointHits.Empty();
	BreakpointActivationOnHit.Empty();
	TemporaryBreakpoint.Reset();
	CurrentActiveBreakpoint.Invalidate();
	CurrentActiveBreakpointCallstack.Empty();
}

void FRigVMDebugInfo::StartExecution()
{
	BreakpointHits.Empty();
	CurrentActiveBreakpoint.Invalidate();
	CurrentActiveBreakpointCallstack.Empty();
}

const FRigVMBreakpoint& FRigVMDebugInfo::FindBreakpoint(const uint16 InInstructionIndex, const UObject* InSubject) const
{
	TArray<FRigVMBreakpoint> BreakpointAtInstruction = FindBreakpointsAtInstruction(InInstructionIndex);
	for (const FRigVMBreakpoint& BP : BreakpointAtInstruction)
	{
		if (BP.Subject == InSubject)
		{
			return BP;
		}
	}
	
	static const FRigVMBreakpoint EmptyBreakpoint;
	return EmptyBreakpoint;
}

TArray<FRigVMBreakpoint> FRigVMDebugInfo::FindBreakpointsAtInstruction(const uint16 InInstructionIndex) const
{
	TArray<FRigVMBreakpoint> Result;
	for (const FRigVMBreakpoint& BP : Breakpoints)
	{
		if (BP.InstructionIndex == InInstructionIndex)
		{
			Result.Add(BP);
		}
	}

	if (TemporaryBreakpoint.IsValid() && TemporaryBreakpoint.InstructionIndex == InInstructionIndex)
	{
		Result.Add(TemporaryBreakpoint);
	}
	
	return Result;
}

const FRigVMBreakpoint& FRigVMDebugInfo::FindBreakpoint(const FGuid& InGuid) const
{
	if(InGuid.IsValid())
	{
		if(TemporaryBreakpoint.IsValid())
		{
			if(TemporaryBreakpoint.Guid == InGuid)
			{
				return TemporaryBreakpoint;
			}
		}

		for(const FRigVMBreakpoint& Breakpoint : Breakpoints)
		{
			if(Breakpoint.Guid == InGuid)
			{
				return Breakpoint;
			}
		}
	}
	
	static const FRigVMBreakpoint EmptyBreakpoint;
	return EmptyBreakpoint;
}

const FRigVMBreakpoint& FRigVMDebugInfo::AddBreakpoint(const uint16 InstructionIndex, UObject* InNode, const uint16 InDepth,
                                                            const bool bIsTemporary)
{
	static const FRigVMBreakpoint EmptyBreakpoint;
	
	for (const FRigVMBreakpoint& BP : FindBreakpointsAtInstruction(InstructionIndex))
	{
		if (BP.Subject == InNode)
		{
			return EmptyBreakpoint;
		}
	}
	
	if (bIsTemporary)
	{
		TemporaryBreakpoint = FRigVMBreakpoint(InstructionIndex, InNode, 0);
			
		// Do not override the state if it already exists
		BreakpointActivationOnHit.FindOrAdd(TemporaryBreakpoint.Guid, 0);
		BreakpointHits.FindOrAdd(TemporaryBreakpoint.Guid, 0);
		return TemporaryBreakpoint;
	}

	// Breakpoints are sorted by instruction index and callstack depth
	int32 Index = 0;
	for(; Index<Breakpoints.Num(); ++Index)
	{
		if (InstructionIndex < Breakpoints[Index].InstructionIndex ||
			(InstructionIndex == Breakpoints[Index].InstructionIndex && InDepth < Breakpoints[Index].Depth))
		{
			break;
		}
	}
	const int32 InsertedIndex = Breakpoints.Insert(FRigVMBreakpoint(InstructionIndex, InNode, InDepth), Index);
	const FRigVMBreakpoint& NewBP = Breakpoints[InsertedIndex];
	
	// Do not override the state if it already exists
	BreakpointActivationOnHit.FindOrAdd(NewBP.Guid, 0);
	BreakpointHits.FindOrAdd(NewBP.Guid, 0);
	return NewBP;
}

bool FRigVMDebugInfo::RemoveBreakpoint(const FRigVMBreakpoint& InBreakpoint)
{
	bool Found = false;
	int32 Index = Breakpoints.Find(InBreakpoint);
	if (Index != INDEX_NONE)
	{
		BreakpointHits.Remove(InBreakpoint.Guid);
		BreakpointActivationOnHit.Remove(InBreakpoint.Guid);
		Breakpoints.RemoveAt(Index);
		Found = true;
	}
	if (TemporaryBreakpoint.IsValid() && InBreakpoint == TemporaryBreakpoint)
	{
		BreakpointHits.Remove(TemporaryBreakpoint.Guid);
		BreakpointActivationOnHit.Remove(TemporaryBreakpoint.Guid);
		TemporaryBreakpoint.Reset();
		Found = true;
	}
	return Found;
}

bool FRigVMDebugInfo::IsActive(const FRigVMBreakpoint& InBreakpoint) const
{
	if(!InBreakpoint.IsValid())
	{
		if(TemporaryBreakpoint.IsValid())
		{
			return IsActive(TemporaryBreakpoint);
		}
	}
	if (InBreakpoint.IsValid())
	{
		if (InBreakpoint.bIsActive)
		{
			uint16 Hits = 0;
			uint16 OnHit = 0;
			if (BreakpointHits.Contains(InBreakpoint.Guid))
			{
				Hits = BreakpointHits.FindChecked(InBreakpoint.Guid);
			}
			if (BreakpointActivationOnHit.Contains(InBreakpoint.Guid))
			{
				OnHit = BreakpointActivationOnHit.FindChecked(InBreakpoint.Guid);
			}
			return Hits == OnHit;
		}				
	}
	return false;		
}

void FRigVMDebugInfo::SetBreakpointHits(const FRigVMBreakpoint& InBreakpoint, const uint16 InBreakpointHits)
{
	if (BreakpointHits.Contains(InBreakpoint.Guid))
	{
		BreakpointHits[InBreakpoint.Guid] = InBreakpointHits;
	}
	else
	{
		BreakpointHits.Add(InBreakpoint.Guid, InBreakpointHits);
	}
}

void FRigVMDebugInfo::HitBreakpoint(const FRigVMBreakpoint& InBreakpoint)
{
	if (BreakpointHits.Contains(InBreakpoint.Guid))
	{
		BreakpointHits[InBreakpoint.Guid]++;
	}
	else
	{
		BreakpointHits.Add(InBreakpoint.Guid, 1);
	}
}

void FRigVMDebugInfo::SetBreakpointActivationOnHit(const FRigVMBreakpoint& InBreakpoint, const uint16 InActivationOnHit)
{
	if (BreakpointActivationOnHit.Contains(InBreakpoint.Guid))
	{
		BreakpointActivationOnHit[InBreakpoint.Guid] = InActivationOnHit;
	}
	else
	{
		BreakpointActivationOnHit.Add(InBreakpoint.Guid, InActivationOnHit);
	}
}

void FRigVMDebugInfo::IncrementBreakpointActivationOnHit(const FRigVMBreakpoint& InBreakpoint)
{
	if (BreakpointActivationOnHit.Contains(InBreakpoint.Guid))
	{
		BreakpointActivationOnHit[InBreakpoint.Guid]++;
	}
	else
	{
		BreakpointActivationOnHit.Add(InBreakpoint.Guid, 1);
	}
}

uint16 FRigVMDebugInfo::GetBreakpointHits(const FRigVMBreakpoint& InBreakpoint) const
{
	if (BreakpointActivationOnHit.Contains(InBreakpoint.Guid))
	{
		return BreakpointActivationOnHit[InBreakpoint.Guid];
	}
	return 0;
}

