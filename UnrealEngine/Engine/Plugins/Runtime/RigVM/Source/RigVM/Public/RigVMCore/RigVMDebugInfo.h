// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "UObject/ObjectMacros.h"

#include "RigVMDebugInfo.generated.h"

class UObject;
class URigVMNode;

// Possible actions when the VM is halted at a breakpoint
UENUM()
enum class ERigVMBreakpointAction : uint8
{
	None,
	Resume,
	StepOver,
	StepInto,
	StepOut,
	Max UMETA(Hidden),
};

USTRUCT()
struct RIGVM_API FRigVMBreakpoint
{
	GENERATED_BODY()

	FRigVMBreakpoint()
	: bIsActive(true)
	, InstructionIndex(INDEX_NONE)
	, Subject(nullptr)
	, Depth(0)
	{
		Guid.Invalidate();
	}
	
	FRigVMBreakpoint(const uint16 InInstructionIndex, TWeakObjectPtr<UObject> InNode, const uint16 InDepth)
	: bIsActive(true)
	, Guid(FGuid::NewGuid())
	, InstructionIndex(InInstructionIndex)
	, Subject(InNode)
	, Depth(InDepth)
	{

	}

	bool IsValid() const
	{
		return InstructionIndex != INDEX_NONE && Guid.IsValid();
	}

	void Reset()
	{
		bIsActive = true;
		InstructionIndex = INDEX_NONE;
		Guid.Invalidate();
		Subject = nullptr;
		Depth = 0;
	}

	operator bool() const
	{
		return IsValid();
	}

	// Whether or not the breakpoint is active
	bool bIsActive;

	// guid used to identify the breakpoint
	FGuid Guid;
	
	// Instruction where this breakpoint is set
	uint16 InstructionIndex;

	// Node related to the breakpoint
	TWeakObjectPtr<UObject> Subject; 

	// The same instruction might be related to multiple breakpoints at different callstack depths
	uint16 Depth; 

	bool operator==(const FRigVMBreakpoint& Other) const
	{
		return Guid == Other.Guid;
	}

	bool operator!=(const FRigVMBreakpoint& Other) const
	{
		return Guid != Other.Guid;
	}
};

USTRUCT()
struct RIGVM_API FRigVMDebugInfo
{
	GENERATED_BODY()

	FRigVMDebugInfo()
	{
	}

	void ResetState();

	void StartExecution();

	void Reset()
	{
		Breakpoints.Reset();

		// Do not remove state
		OnExecutionHalted.Clear();

		HaltedAtBreakpoint = FRigVMBreakpoint();
		HaltedAtBreakpointHit = INDEX_NONE;
		CurrentBreakpointAction = ERigVMBreakpointAction::None;
	}

	bool IsEmpty() const
	{
		return GetBreakpoints().IsEmpty() && !TemporaryBreakpoint.IsValid();
	}

	const FRigVMBreakpoint& FindBreakpoint(const uint16 InInstructionIndex, const UObject* InSubject) const;
	TArray<FRigVMBreakpoint> FindBreakpointsAtInstruction(const uint16 InInstructionIndex) const;
	const FRigVMBreakpoint& FindBreakpoint(const FGuid& InGuid) const;
	
	const FRigVMBreakpoint& AddBreakpoint(const uint16 InstructionIndex, TWeakObjectPtr<UObject> InNode, const uint16 InDepth, const bool bIsTemporary = false);

	bool RemoveBreakpoint(const FRigVMBreakpoint& Breakpoint);

	const TArray<FRigVMBreakpoint>& GetBreakpoints() const { return Breakpoints; }

	void SetBreakpoints(const TArray<FRigVMBreakpoint>& InBreakpoints)
	{
		Breakpoints = InBreakpoints;

		if (CurrentActiveBreakpoint.IsValid() && !FindBreakpoint(CurrentActiveBreakpoint).IsValid())
		{
			CurrentActiveBreakpoint.Invalidate();
			CurrentActiveBreakpointCallstack.Reset();
		}
	}

	bool IsTemporaryBreakpoint(FRigVMBreakpoint Breakpoint) const
	{
		if (Breakpoint.IsValid())
		{
			return Breakpoint == TemporaryBreakpoint;
		}
		return false;
	}

	bool IsActive(const FRigVMBreakpoint& InBreakpoint) const;

	void SetBreakpointHits(const FRigVMBreakpoint& InBreakpoint, const uint16 InBreakpointHits);

	void HitBreakpoint(const FRigVMBreakpoint& InBreakpoint);

	void SetBreakpointActivationOnHit(const FRigVMBreakpoint& InBreakpoint, const uint16 InActivationOnHit);

	void IncrementBreakpointActivationOnHit(const FRigVMBreakpoint& InBreakpoint);

	uint16 GetBreakpointHits(const FRigVMBreakpoint& InBreakpoint) const;

	const FRigVMBreakpoint& GetCurrentActiveBreakpoint() const { return FindBreakpoint(CurrentActiveBreakpoint); }

	void SetCurrentActiveBreakpoint(const FRigVMBreakpoint& InBreakpoint)
	{
		if(InBreakpoint.IsValid())
		{
			CurrentActiveBreakpoint = InBreakpoint.Guid;
		}
		else
		{
			CurrentActiveBreakpoint.Invalidate();
		}
	}

	inline TArray<TWeakObjectPtr<UObject>>& GetCurrentActiveBreakpointCallstack() { return CurrentActiveBreakpointCallstack; }

	inline void SetCurrentActiveBreakpointCallstack(TArray<TWeakObjectPtr<UObject>> Callstack) { CurrentActiveBreakpointCallstack = Callstack; }

	inline FRigVMBreakpoint& GetHaltedAtBreakpoint() { return HaltedAtBreakpoint; }
	inline const FRigVMBreakpoint& GetHaltedAtBreakpoint() const { return HaltedAtBreakpoint; }
	inline void SetHaltedAtBreakpoint(const FRigVMBreakpoint& InHaltedAtBreakpoint) { HaltedAtBreakpoint = InHaltedAtBreakpoint; }

	inline int32 GetHaltedAtBreakpointHit() const { return HaltedAtBreakpointHit; }
	inline void SetHaltedAtBreakpointHit(int32 InHaltedAtBreakpointHit) { HaltedAtBreakpointHit = InHaltedAtBreakpointHit; }

	inline ERigVMBreakpointAction GetCurrentBreakpointAction() const { return CurrentBreakpointAction; }
	inline void SetCurrentBreakpointAction(ERigVMBreakpointAction InCurrentBreakpointAction) { CurrentBreakpointAction = InCurrentBreakpointAction; }

	bool ResumeExecution();

	DECLARE_EVENT_ThreeParams(URigVM, FExecutionHaltedEvent, int32, UObject*, const FName&);
	inline FExecutionHaltedEvent& ExecutionHalted()
	{
		return OnExecutionHalted;
	}

private:
	TArray<FRigVMBreakpoint> Breakpoints;
	FRigVMBreakpoint TemporaryBreakpoint;
	TMap<FGuid, uint16> BreakpointActivationOnHit; // After how many instruction executions, this breakpoint becomes active
	TMap<FGuid, uint16> BreakpointHits; // How many times this instruction has been executed

	FGuid CurrentActiveBreakpoint;
	TArray<TWeakObjectPtr<UObject>> CurrentActiveBreakpointCallstack;

	// --- ---

	FRigVMBreakpoint HaltedAtBreakpoint;
	int32 HaltedAtBreakpointHit = INDEX_NONE;
	ERigVMBreakpointAction CurrentBreakpointAction = ERigVMBreakpointAction::None;

	FExecutionHaltedEvent OnExecutionHalted;
};
