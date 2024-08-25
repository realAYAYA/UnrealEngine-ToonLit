// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "AutoRTFM/AutoRTFM.h"
#include "Containers/Utf8String.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h"
#include "UObject/UnrealType.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMUClassInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/Inline/VVMVarInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMArrayBase.h"
#include "VerseVM/VVMBytecode.h"
#include "VerseVM/VVMBytecodeOps.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMFailureContext.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMFrame.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMGlobalHeapPtr.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMOption.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMSuspension.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMValuePrinting.h"
#include "VerseVM/VVMVar.h"
#include <stdio.h>

static_assert(UE_AUTORTFM, "New VM depends on AutoRTFM.");

namespace Verse
{

// The Interpreter is organized into two main execution loops: the main loop and the suspension loop.
// The main loop works like a normal interpreter loop. Control flow falls through from one bytecode
// to the next. We also have jump instructions which can divert control flow. However, since Verse
// also has failure, the bytecode has support for any bytecode that fails jumping to the current
// failure context's "on fail" bytecode destination. The way this works is that the BeginFailureContext
// and EndFailureContext bytecodes form a pair. The BeginFailureContext specifies where to jump to in
// the event of failure. Notably, if failure doesn't happen, the EndFailureContext bytecode must execute.
// This means that BeginFailureContext and EndFailureContext should be control equivalent -- we can't
// have jumps that jump over an EndFailureContext bytecode from within the failure context range.
//
// The bytecode also has builtin support for Verse's lenient execution model. This support is fundamental
// to the execution model of the bytecode. Bytecode instructions can suspend when a needed input
// operand is not concrete -- it's a placeholder -- and then resume execution when the input operand
// becomes concrete. Bytecode suspensions will capture their input operands and use the captured operands
// when they resume execution. When a placeholder becomes concrete unlocking a suspension, that suspension
// will execute in the suspension interpreter loop. The reason bytecode suspensions capture their input
// operands is so that those bytecode frame slots can be reused by the rest of the bytecode program.
// Because the operands aren't reloaded from the frame, and instead from the suspension, our bytecode
// generator can have a virtual register allocation algorithm that doesn't need to take into account
// liveness constraints dictated by leniency. This invariant has interesting implications executing a
// failure context leniently. In that scenario, we need to capture everything that's used both in the
// then/else branch. (For now, we implement this by just cloning the entire frame.) It's a goal to
// share as much code as we can between the main and suspension interpreter loops. That's why there
// are overloaded functions and interpreter-loop-specific macros that can handle both bytecode
// structs and suspension captures.
//
// Because of leniency, the interpreter needs to be careful about executing effects in program order. For
// example, if you have two effectful bytecodes one after the other, and the first one suspends, then the
// second one can't execute until the first one finishes. To handle this, we track an effect token that we
// thread through the program. Effectful operations will require the effect token to be concrete. They only
// execute after the token is concrete. Effectful operations always define a new non-concrete effect token.
// Only after the operation executes will it set the effect token to be concrete.
//
// Slots in the bytecode are all unification variables in support of Verse's general unification variable
// semantics. In our runtime, a unification variable is either a normal concrete value or a placeholder.
// A placeholder is used to support leniency. A placeholder can be used to unify two non-concrete variables.
// A placeholder can also point at a list of suspensions to fire when it becomes concrete. And finally, a
// placeholder can be mutated to point at a concrete value. When the runtime mutates a placeholder to
// point at a concrete value, it will fire its list of suspensions.
//
// Logically, a bytecode frame is initialized with empty placeholders. Every local variable in Verse is a
// unification variable. However, we really want to avoid this placeholder allocation for every local. After
// all, most locals will be defined before they're used. We optimize this by making these slots VRestValue
// instead of VPlaceholder. A VRestValue can be thought of a promise to produce a VPlaceholder if it's used
// before it has a concretely defined value. However, if we define a value in a bytecode slot before it's
// used, we can elide the allocation of the VPlaceholder altogether.

// This is used as a special PC to get the interpreter to break out of its loop.
static FOpErr StopInterpreterSentry;

namespace
{
bool CanAllocateUObjects()
{
	// NOTE: This is an arbitrary limit. If we have less than ~10k `UObject`s available for allocation left
	// we're probably in a bad spot anyway. This just makes sure that there is some slack available before the
	// limit gets hit.
	static constexpr int32 MinAvailableObjectCount = 10 * 1024;
	return GUObjectArray.GetObjectArrayEstimatedAvailable() >= MinAvailableObjectCount;
}

struct FExecutionState
{
	VFrame* Frame{nullptr};
	const TWriteBarrier<VValue>* Constants{nullptr};
	FOp* PC{nullptr};
	VFailureContext* FailureContext{nullptr};

	FExecutionState(VFrame* Frame, FOp* PC, VFailureContext* FailureContext)
		: Frame(Frame)
		, Constants(Frame->Procedure->GetConstantsBegin())
		, PC(PC)
		, FailureContext(FailureContext)
	{
	}

	FExecutionState() = default;
	FExecutionState(const FExecutionState&) = default;
	FExecutionState(FExecutionState&&) = default;
	FExecutionState& operator=(const FExecutionState&) = default;
};

// In Verse, all functions conceptually take a single argument tuple
// To avoid unnecessary boxing and unboxing of VValues, we add an optimization where we try to avoid boxing/unboxing as much as possible
// This function reconciles the number of expected parameters with the number of provided arguments and boxes/unboxes only as needed
template <typename ArgFunction, typename StoreFunction>
void UnboxArguments(FAllocationContext Context, uint32 NumParams, uint32 NumArgs, ArgFunction GetArg, StoreFunction StoreArg)
{
	if (NumParams == NumArgs)
	{
		// Calling conventions match - no boxing/unboxing is necessary
		for (uint32 Arg = 0; Arg < NumArgs; ++Arg)
		{
			StoreArg(Arg, GetArg(Arg));
		}
	}
	else if (NumParams)
	{
		if (NumArgs > NumParams)
		{
			V_DIE_UNLESS(NumParams == 1);

			// Function wants arguments in a tuple - box them up
			VArray& ArgArray = VArray::New(Context, NumArgs, GetArg);

			StoreArg(0, ArgArray);
		}
		else
		{
			V_DIE_UNLESS(NumArgs < NumParams);
			V_DIE_UNLESS(NumArgs == 1);

			// Function wants loose arguments but a tuple is provided - unbox them
			VValue IncomingArg = GetArg(0);
			VArray* Args = IncomingArg.DynamicCast<VArray>();

			V_DIE_UNLESS(Args->Num() == NumParams);
			for (uint32 Param = 0; Param < NumParams; ++Param)
			{
				StoreArg(Param, Args->GetValue(Param));
			}
		}
	}
	else
	{
		V_DIE_UNLESS(false);
	}
}

template <typename ArgFunction, typename ReturnSlotType>
VFrame& MakeFrameForCallee(FRunningContext Context, VFrame* CallerFrame, FOp* CallerPC, ReturnSlotType ReturnSlot, VFunction& Function, uint32 NumArgs, ArgFunction GetArg)
{
	VProcedure& Procedure = Function.GetProcedure();
	VFrame& Frame = VFrame::New(Context, Procedure.NumRegisters, CallerFrame, CallerPC, Procedure, ReturnSlot);

	check(1 + Procedure.NumParameters <= Procedure.NumRegisters);

	Frame.Registers[0].Set(Context, Function.ParentScope.Get());

	UnboxArguments(Context, Procedure.NumParameters, NumArgs, GetArg,
		[&](uint32 Param, VValue Value) {
			Frame.Registers[1 + Param].Set(Context, Value);
		});

	return Frame;
}
} // namespace

class FInterpreter
{
	FRunningContext Context;
	FExecutionState State;
	FExecutionState SavedStateForTracing;
	VRestValue EffectToken{0};
	VSuspension* CurrentSuspension{nullptr};
	VFailureContext* const OutermostFailureContext;
	FOp* OutermostStartPC;
	FOp* OutermostEndPC;
	FString ExecutionTrace;

	VValue GetOperand(FValueOperand Operand)
	{
		if (Operand.IsConstant())
		{
			VValue Result = State.Constants[Operand.AsConstant().Index].Get();
			checkSlow(!Result.IsPlaceholder());
			return Result;
		}
		else
		{
			return State.Frame->Registers[Operand.AsRegister().Index].Get(Context);
		}
	}

	VValue GetOperand(const TWriteBarrier<VValue>& Value)
	{
		return Value.Get().Follow();
	}

	// Include autogenerated functions to create captures
#include "VVMMakeCapturesFuncs.gen.h"

	FString TraceOperand(FValueOperand Operand)
	{
		if (Operand.IsConstant())
		{
			return ToString(Context, FDefaultCellFormatter(), State.Constants[Operand.AsConstant().Index].Get());
		}
		else
		{
			return ToString(Context, FDefaultCellFormatter(), State.Frame->Registers[Operand.AsRegister().Index]);
		}
	}

	template <typename T>
	FString StringifyOperandOrValue(TWriteBarrier<T> Operand)
	{
		if constexpr (std::is_same_v<T, VValue>)
		{
			return ToString(Context, FDefaultCellFormatter(), Operand.Get());
		}
		else
		{
			return ToString(Context, FDefaultCellFormatter(), *Operand.Get());
		}
	}
	FString StringifyOperandOrValue(VValue Value) { return ToString(Context, FDefaultCellFormatter(), Value); }
	FString StringifyOperandOrValue(FValueOperand Operand) { return TraceOperand(Operand); }
	FString StringifyOperandOrValue(const TArray<FValueOperand>& Operands)
	{
		FString Result = "";
		for (const FValueOperand& Op : Operands)
		{
			Result += TraceOperand(Op);
		}
		return Result;
	}

	template <typename OpOrCaptures>
	FString TraceOperandsImpl(const OpOrCaptures& Op, TArray<EOperandRole> RolesToPrint)
	{
		FString String;
		FString Separator;
		Op.ForEachOperandWithName([&](EOperandRole Role, auto OperandOrValue, const char* Name) {
			if (RolesToPrint.Find(Role) != INDEX_NONE)
			{
				String += Separator;
				Separator = TEXT(", ");
				String += FString::Printf(TEXT("%s="), *FString(Name));
				String += StringifyOperandOrValue(OperandOrValue);
			}
		});
		return String;
	}

	template <typename OpOrCaptures>
	FString TraceInputs(const OpOrCaptures& Op)
	{
		return TraceOperandsImpl(Op, {EOperandRole::Use});
	}

	template <typename OpOrCaptures>
	FString TraceOutputs(const OpOrCaptures& Op)
	{
		return TraceOperandsImpl(Op, {EOperandRole::UnifyDef, EOperandRole::ClobberDef});
	}

	FString TracePrefix(VProcedure* Procedure, VRestValue* CurrentEffectToken, FOp* PC, bool bLenient)
	{
		FString String;
		String += FString::Printf(TEXT("0x%" PRIxPTR), Procedure);
		String += FString::Printf(TEXT("#%u|"), Procedure->BytecodeOffset(*PC));
		if (CurrentEffectToken)
		{
			String += TEXT("EffectToken=");
			String += ToString(Context, FDefaultCellFormatter(), *CurrentEffectToken);
			String += TEXT("|");
		}
		if (bLenient)
		{
			String += TEXT("Lenient|");
		}
		String += ToString(PC->Opcode);
		String += TEXT("(");
		return String;
	}

	void BeginTrace()
	{
		if (CVarSingleStepTraceExecution.GetValueOnAnyThread())
		{
			getchar();
		}

		SavedStateForTracing = State;
		if (State.PC == &StopInterpreterSentry)
		{
			UE_LOG(LogVerseVM, Display, TEXT("StoppingExecution, encountered StopInterpreterSentry"));
			return;
		}

		ExecutionTrace = TracePrefix(State.Frame->Procedure.Get(), &EffectToken, State.PC, false);

#define VISIT_OP(Name)                                                     \
	case EOpcode::Name:                                                    \
	{                                                                      \
		ExecutionTrace += TraceInputs(*static_cast<FOp##Name*>(State.PC)); \
		break;                                                             \
	}

		switch (State.PC->Opcode)
		{
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
		}

		ExecutionTrace += TEXT(")");
	}

	template <typename CaptureType>
	void BeginTrace(const CaptureType& Captures, VBytecodeSuspension& Suspension)
	{
		if (CVarSingleStepTraceExecution.GetValueOnAnyThread())
		{
			getchar();
		}

		ExecutionTrace = TracePrefix(Suspension.Procedure.Get(), nullptr, Suspension.PC, true);
		ExecutionTrace += TraceInputs(Captures);
		ExecutionTrace += TEXT(")");
	}

	void EndTrace(bool bSuspended, bool bFailed)
	{
		FExecutionState CurrentState = State;
		State = SavedStateForTracing;

		FString Temp;

#define VISIT_OP(Name)                                           \
	case EOpcode::Name:                                          \
	{                                                            \
		Temp = TraceOutputs(*static_cast<FOp##Name*>(State.PC)); \
		break;                                                   \
	}

		switch (State.PC->Opcode)
		{
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
		}

		if (!Temp.IsEmpty())
		{
			ExecutionTrace += TEXT("|");
			ExecutionTrace += Temp;
		}

		if (bSuspended)
		{
			ExecutionTrace += TEXT("|Suspending");
		}

		if (bFailed)
		{
			ExecutionTrace += TEXT("|Failed");
		}

		UE_LOG(LogVerseVM, Display, TEXT("%s"), *ExecutionTrace);

		State = CurrentState;
	}

	template <typename CaptureType>
	void EndTraceWithCaptures(CaptureType& Captures, bool bSuspended, bool bFailed)
	{
		ExecutionTrace += TEXT("|");
		ExecutionTrace += TraceOutputs(Captures);
		if (bSuspended)
		{
			ExecutionTrace += TEXT("|Suspending");
		}

		if (bFailed)
		{
			ExecutionTrace += TEXT("|Failed");
		}
		UE_LOG(LogVerseVM, Display, TEXT("%s"), *ExecutionTrace);
	}

	static bool Def(FRunningContext Context, VValue ResultSlot, VValue Value, VSuspension*& SuspensionsToFire)
	{
		// This returns true if we encounter a placeholder
		return VValue::Equal(Context, ResultSlot, Value, [Context, &SuspensionsToFire](VValue Left, VValue Right) {
			// Given how the interpreter is structured, we know these must be resolved
			// to placeholders. They can't be pointing to values or we should be using
			// the value they point to.
			checkSlow(!Left.IsPlaceholder() || Left.Follow().IsPlaceholder());
			checkSlow(!Right.IsPlaceholder() || Right.Follow().IsPlaceholder());

			if (Left.IsPlaceholder() && Right.IsPlaceholder())
			{
				Left.GetRootPlaceholder().Unify(Context, Right.GetRootPlaceholder());
				return;
			}

			VSuspension* NewSuspensionToFire;
			if (Left.IsPlaceholder())
			{
				NewSuspensionToFire = Left.GetRootPlaceholder().SetValue(Context, Right);
			}
			else
			{
				NewSuspensionToFire = Right.GetRootPlaceholder().SetValue(Context, Left);
			}

			if (!SuspensionsToFire)
			{
				SuspensionsToFire = NewSuspensionToFire;
			}
			else
			{
				SuspensionsToFire->Tail().Next.Set(Context, NewSuspensionToFire);
			}
		});
	}

	bool Def(VValue ResultSlot, VValue Value)
	{
		return Def(Context, ResultSlot, Value, CurrentSuspension);
	}

	static bool Def(FRunningContext Context, VRestValue& ResultSlot, VValue Value, VSuspension*& SuspensionsToFire)
	{
		// TODO: This needs to consider split depth eventually.
		if (LIKELY(ResultSlot.CanDefQuickly()))
		{
			ResultSlot.Set(Context, Value);
			return true;
		}
		return Def(Context, ResultSlot.Get(Context), Value, SuspensionsToFire);
	}

	bool Def(VRestValue& ResultSlot, VValue Value)
	{
		return Def(Context, ResultSlot, Value, CurrentSuspension);
	}

	bool Def(FRegisterIndex ResultSlot, VValue Value)
	{
		return Def(State.Frame->Registers[ResultSlot.Index], Value);
	}

	bool Def(const TWriteBarrier<VValue>& ResultSlot, VValue Value)
	{
		return Def(GetOperand(ResultSlot), Value);
	}

	void BumpEffectEpoch()
	{
		EffectToken.Reset(0);
	}

	void FinishedExecutingFailureContextLeniently(VFailureContext& FailureContext, FOp* StartPC, FOp* EndPC, VValue NextEffectToken)
	{
		if (StartPC < EndPC)
		{
			VFailureContext* ParentContext = FailureContext.Parent.Get();

			VFrame* Frame = FailureContext.Frame.Get();
			// When we cloned the frame for lenient execution, we guarantee the caller info
			// isn't set because when this is done executing, it should not return to the
			// caller at the time of creation of the failure context. It should return back here.
			V_DIE_IF(Frame->CallerFrame || Frame->CallerPC);

			FInterpreter Interpreter(Context,
				FExecutionState(Frame, StartPC, ParentContext),
				NextEffectToken,
				StartPC, EndPC);
			Interpreter.Execute();

			// TODO: We need to think through exactly what control flow inside
			// of the then/else of a failure context means. For example, then/else
			// can contain a break/return, but we might already be executing past
			// that then/else leniently. So we need to somehow find a way to transfer
			// control of the non-lenient execution. This likely means the below
			// def of the effect token isn't always right.

			// This can't fail.
			Def(FailureContext.DoneEffectToken, Interpreter.EffectToken.Get(Context));
		}
		else
		{
			// This can't fail.
			Def(FailureContext.DoneEffectToken, NextEffectToken);
		}

		if (FailureContext.Parent && !FailureContext.Parent->bFailed)
		{
			// We increment the suspension count for our parent failure
			// context when this failure context sees lenient execution.
			// So this is the decrement to balance that out that increment.
			FinishedExecutingSuspensionIn(*FailureContext.Parent);
		}
	}

	void Fail(VFailureContext& FailureContext)
	{
		V_DIE_IF(FailureContext.bFailed);

		FailureContext.Fail(Context);
		FailureContext.FinishedExecuting(Context);

		if (LIKELY(!FailureContext.bExecutedEndFailureContextOpcode))
		{
			return;
		}

		FOp* StartPC = FailureContext.FailurePC;
		FOp* EndPC = FailureContext.DonePC;
		VValue NextEffectToken = FailureContext.IncomingEffectToken.Get();

		FinishedExecutingFailureContextLeniently(FailureContext, StartPC, EndPC, NextEffectToken);
	}

	void FinishedExecutingSuspensionIn(VFailureContext& FailureContext)
	{
		V_DIE_IF(FailureContext.bFailed);

		V_DIE_UNLESS(FailureContext.SuspensionCount);
		uint32 RemainingCount = --FailureContext.SuspensionCount;
		if (RemainingCount)
		{
			return;
		}

		if (LIKELY(!FailureContext.bExecutedEndFailureContextOpcode))
		{
			return;
		}

		FailureContext.FinishedExecuting(Context);
		FOp* StartPC = FailureContext.ThenPC;
		FOp* EndPC = FailureContext.FailurePC;
		// Since we finished executing all suspensions in this failure context without failure, we can now commit the transaction
		VValue NextEffectToken = FailureContext.BeforeThenEffectToken.Get(Context);
		if (NextEffectToken.IsPlaceholder())
		{
			VValue NewNextEffectToken = VValue::Placeholder(VPlaceholder::New(Context, 0));
			DoTransactionActionWhenEffectTokenIsConcrete<TransactAction::Commit>(FailureContext, NextEffectToken, NewNextEffectToken);
			NextEffectToken = NewNextEffectToken;
		}
		else
		{
			FailureContext.Transaction.Commit(Context);
		}

		FinishedExecutingFailureContextLeniently(FailureContext, StartPC, EndPC, NextEffectToken);
	}

	// Returns true if unwinding succeeded. False if we are trying to unwind past
	// the outermost frame of this Interpreter instance.
	bool UnwindIfNeeded()
	{
		if (!State.FailureContext->bFailed)
		{
			return true;
		}

		VFailureContext* FailedContext = State.FailureContext;
		while (true)
		{
			if (FailedContext == OutermostFailureContext)
			{
				return false;
			}

			VFailureContext* Parent = FailedContext->Parent.Get();
			if (!Parent->bFailed)
			{
				break;
			}
			FailedContext = Parent;
		}

		State = FExecutionState(FailedContext->Frame.Get(), FailedContext->FailurePC, FailedContext->Parent.Get());
		EffectToken.Set(Context, FailedContext->IncomingEffectToken.Get());

		return true;
	}

	bool IsOutermostFrame()
	{
		return !State.Frame->CallerFrame;
	}

	enum class TransactAction
	{
		Start,
		Commit
	};

	template <TransactAction Action>
	void DoTransactionActionWhenEffectTokenIsConcrete(VFailureContext& FailureContext, VValue IncomingEffectToken, VValue NextEffectToken)
	{
		VLambdaSuspension& Suspension = VLambdaSuspension::New(
			Context, FailureContext,
			[](FRunningContext TheContext, VLambdaSuspension& LambdaSuspension, VSuspension*& SuspensionsToFire) {
				if constexpr (Action == TransactAction::Start)
				{
					LambdaSuspension.FailureContext->Transaction.Start(TheContext);
				}
				else
				{
					LambdaSuspension.FailureContext->Transaction.Commit(TheContext);
				}
				VValue NextEffectToken = LambdaSuspension.Args()[0].Get();
				FInterpreter::Def(TheContext, NextEffectToken, VValue::EffectDoneMarker(), SuspensionsToFire);
			},
			NextEffectToken);

		IncomingEffectToken.EnqueueSuspension(Context, Suspension);
	}

#define DEF(Result, Value)   \
	if (!Def(Result, Value)) \
	{                        \
		FAIL();              \
	}

#define REQUIRE_CONCRETE(Value)          \
	if (UNLIKELY(Value.IsPlaceholder())) \
	{                                    \
		ENQUEUE_SUSPENSION(Value);       \
	}

#define FAIL()            \
	return                \
	{                     \
		FOpResult::Failed \
	}

#define ENQUEUE_SUSPENSION(Value)       \
	return                              \
	{                                   \
		FOpResult::ShouldSuspend, Value \
	}

#define OP_RESULT_HELPER(Result)                                                            \
	if (Result.Kind != FOpResult::Normal)                                                   \
	{                                                                                       \
		if (Result.Kind == FOpResult::Failed)                                               \
		{                                                                                   \
			FAIL();                                                                         \
		}                                                                                   \
		else if (Result.Kind == FOpResult::ShouldSuspend)                                   \
		{                                                                                   \
			check(Result.Value.IsPlaceholder());                                            \
			ENQUEUE_SUSPENSION(Result.Value);                                               \
		}                                                                                   \
		else                                                                                \
		{                                                                                   \
			check(Result.Kind == FOpResult::RuntimeError);                                  \
			/* TODO: SOL-4563 Implement proper handling of runtime errors */                \
			V_DIE("%s", UTF8_TO_TCHAR(Result.Value.StaticCast<VUTF8String>().AsCString())); \
		}                                                                                   \
	}

	VRational& PrepareRationalSourceHelper(VValue& Source)
	{
		if (VRational* RationalSource = Source.DynamicCast<VRational>())
		{
			return *RationalSource;
		}

		if (!Source.IsInt())
		{
			V_DIE("Unsupported operands were passed to a Rational operation!");
		}

		return VRational::New(Context, Source.AsInt(), VInt(Context, 1));
	}

	template <typename OpType>
	FOpResult AddImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			DEF(Op.Dest, VInt::Add(Context, LeftSource.AsInt(), RightSource.AsInt()));
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			DEF(Op.Dest, LeftSource.AsFloat() + RightSource.AsFloat());
		}
		else if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);

			DEF(Op.Dest, VRational::Add(Context, LeftRational, RightRational).StaticCast<VCell>());
		}
		else if (LeftSource.IsCellOfType<VUTF8String>() && RightSource.IsCellOfType<VUTF8String>())
		{
			// String concatenation.
			VUTF8String& LeftString = LeftSource.StaticCast<VUTF8String>();
			VUTF8String& RightString = RightSource.StaticCast<VUTF8String>();

			DEF(Op.Dest, VUTF8String::Concat(Context, LeftString, RightString));
		}
		else if (LeftSource.IsCellOfType<VArrayBase>() && RightSource.IsCellOfType<VArrayBase>())
		{
			// Array concatenation.
			VArrayBase& LeftArray = LeftSource.StaticCast<VArrayBase>();
			VArrayBase& RightArray = RightSource.StaticCast<VArrayBase>();

			DEF(Op.Dest, VArrayBase::Concat<VArray>(Context, LeftArray, RightArray));
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Add` operation!");
		}

		return {FOpResult::Normal};
	}

	// TODO: Add the ability for bytecode instructions to have optional arguments so instead of having this bytecode
	//		 we can just have 'Add' which can take a boolean telling it whether the result should be mutable.
	template <typename OpType>
	FOpResult MutableAddImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsCellOfType<VArrayBase>() && RightSource.IsCellOfType<VArrayBase>())
		{
			// Array concatenation.
			VArrayBase& LeftArray = LeftSource.StaticCast<VArrayBase>();
			VArrayBase& RightArray = RightSource.StaticCast<VArrayBase>();

			DEF(Op.Dest, VArrayBase::Concat<VMutableArray>(Context, LeftArray, RightArray));
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `MutableAdd` operation!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult SubImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			DEF(Op.Dest, VInt::Sub(Context, LeftSource.AsInt(), RightSource.AsInt()));
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			DEF(Op.Dest, LeftSource.AsFloat() - RightSource.AsFloat());
		}
		else if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);

			DEF(Op.Dest, VRational::Sub(Context, LeftRational, RightRational).StaticCast<VCell>());
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Sub` operation!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult MulImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt())
		{
			if (RightSource.IsInt())
			{
				DEF(Op.Dest, VInt::Mul(Context, LeftSource.AsInt(), RightSource.AsInt()));
				return {FOpResult::Normal};
			}
			else if (RightSource.IsFloat())
			{
				DEF(Op.Dest, LeftSource.AsInt().ConvertToFloat() * RightSource.AsFloat());
				return {FOpResult::Normal};
			}
		}
		else if (LeftSource.IsFloat())
		{
			if (RightSource.IsInt())
			{
				DEF(Op.Dest, LeftSource.AsFloat() * RightSource.AsInt().ConvertToFloat());
				return {FOpResult::Normal};
			}
			else if (RightSource.IsFloat())
			{
				DEF(Op.Dest, LeftSource.AsFloat() * RightSource.AsFloat());
				return {FOpResult::Normal};
			}
		}

		if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);

			DEF(Op.Dest, VRational::Mul(Context, LeftRational, RightRational).StaticCast<VCell>());
			return {FOpResult::Normal};
		}

		V_DIE("Unsupported operands were passed to a `Mul` operation!");
		VERSE_UNREACHABLE();
	}

	template <typename OpType>
	FOpResult DivImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (RightSource.AsInt().IsZero())
			{
				FAIL();
			}

			DEF(Op.Dest, VRational::New(Context, LeftSource.AsInt(), RightSource.AsInt()).StaticCast<VCell>());
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			DEF(Op.Dest, LeftSource.AsFloat() / RightSource.AsFloat());
		}
		else if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);
			if (RightRational.IsZero())
			{
				FAIL();
			}

			DEF(Op.Dest, VRational::Div(Context, LeftRational, RightRational).StaticCast<VCell>());
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Div` operation!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult ModImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (RightSource.AsInt().IsZero())
			{
				FAIL();
			}

			DEF(Op.Dest, VInt::Mod(Context, LeftSource.AsInt(), RightSource.AsInt()));
		}
		// TODO: VRational could support Mod in limited circumstances
		else
		{
			V_DIE("Unsupported operands were passed to a `Mod` operation!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NegImpl(OpType& Op)
	{
		VValue Source = GetOperand(Op.Source);
		REQUIRE_CONCRETE(Source);

		if (Source.IsInt())
		{
			DEF(Op.Dest, VInt::Neg(Context, Source.AsInt()));
		}
		else if (Source.IsFloat())
		{
			DEF(Op.Dest, -(Source.AsFloat()));
		}
		else if (Source.IsCellOfType<VRational>())
		{
			DEF(Op.Dest, VRational::Neg(Context, Source.StaticCast<VRational>()));
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `Neg` operation");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult QueryImpl(OpType& Op)
	{
		VValue Source = GetOperand(Op.Source);
		REQUIRE_CONCRETE(Source);

		if (Source.ExtractCell() == GlobalFalsePtr.Get())
		{
			FAIL();
		}
		else if (VOption* Option = Source.DynamicCast<VOption>()) // True = VOption(VFalse), which is handled by this case
		{
			DEF(Op.Dest, Option->GetValue());
		}
		else if (!Source.IsUObject())
		{
			V_DIE("Unimplemented type passed to VM `Query` operation");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult MapKeyImpl(OpType& Op)
	{
		VValue Map = GetOperand(Op.Map);
		VValue Index = GetOperand(Op.Index);
		REQUIRE_CONCRETE(Map);
		REQUIRE_CONCRETE(Index);

		if (Map.IsCellOfType<VMapBase>() && Index.IsInt())
		{
			DEF(Op.Dest, Map.StaticCast<VMapBase>().GetKey(Index.AsInt32()));
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `MapKey` operation!");
		}
		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult MapValueImpl(OpType& Op)
	{
		VValue Map = GetOperand(Op.Map);
		VValue Index = GetOperand(Op.Index);
		REQUIRE_CONCRETE(Map);
		REQUIRE_CONCRETE(Index);

		if (Map.IsCellOfType<VMapBase>() && Index.IsInt())
		{
			DEF(Op.Dest, Map.StaticCast<VMapBase>().GetValue(Index.AsInt32()));
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `MapValue` operation!");
		}
		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult LengthImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		// We need this to be concrete before we can attempt to get its size, even if the values in the container
		// might be placeholders.
		REQUIRE_CONCRETE(Container);
		if (const VArrayBase* Array = Container.DynamicCast<VArrayBase>())
		{
			DEF(Op.Dest, VInt{static_cast<int32>(Array->Num())});
		}
		else if (const VMapBase* Map = Container.DynamicCast<VMapBase>())
		{
			DEF(Op.Dest, VInt{static_cast<int32>(Map->Num())});
		}
		else if (const VUTF8String* String = Container.DynamicCast<VUTF8String>())
		{
			DEF(Op.Dest, VInt{static_cast<int32>(String->Num())});
		}
		else
		{
			V_DIE("Unsupported container type passed!");
		}

		return {FOpResult::Normal};
	}

	// TODO (SOL-5813) : Optimize melt to start at the value it suspended on rather
	// than re-doing the entire melt Op again which is what we do currently.
	template <typename OpType>
	FOpResult MeltImpl(OpType& Op)
	{
		VValue Value = GetOperand(Op.Value);
		FOpResult Result = VValue::Melt(Context, Value);
		if (Result.Kind == FOpResult::Normal)
		{
			DEF(Op.Dest, Result.Value);
		}
		return Result;
	}

	template <typename OpType>
	FOpResult FreezeImpl(OpType& Op)
	{
		VValue Value = GetOperand(Op.Value);
		FOpResult Result = VValue::Freeze(Context, Value);
		DEF(Op.Dest, Result.Value);
		return Result;
	}

	template <typename OpType>
	FOpResult VarGetImpl(OpType& Op)
	{
		VValue Var = GetOperand(Op.Var);
		REQUIRE_CONCRETE(Var);
		VValue Result = Var.StaticCast<VVar>().Get(Context);
		DEF(Op.Dest, Result);
		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult VarSetImpl(OpType& Op)
	{
		VValue Var = GetOperand(Op.Var);
		VValue Value = GetOperand(Op.Value);
		REQUIRE_CONCRETE(Var);
		Var.StaticCast<VVar>().Set(Context, Value);
		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult IndexSetImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		const VValue Index = GetOperand(Op.Index);
		const VValue ValueToSet = GetOperand(Op.ValueToSet);
		REQUIRE_CONCRETE(Container);
		REQUIRE_CONCRETE(Index); // Must be an Int32 (although UInt32 is better)
		if (VMutableArray* Array = Container.DynamicCast<VMutableArray>())
		{
			// Bounds check since this index access in Verse is failable.
			if (Index.IsInt32() && Index.AsInt32() >= 0 && Array->IsInBounds(Index.AsInt32()))
			{
				Array->SetValue(Context, static_cast<uint32>(Index.AsInt32()), ValueToSet);
			}
			else
			{
				FAIL();
			}
		}
		else if (VMutableMap* Map = Container.DynamicCast<VMutableMap>())
		{
			Map->Add(Context, Index, ValueToSet);
		}
		else
		{
			V_DIE("Unsupported container type passed!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult CallImpl(OpType& Op, VValue Callee)
	{
		// Handles FOpCall for all cases except VFunction calls which
		// are handled differently for lenient and non-lenient calls.
		check(!Callee.IsPlaceholder());

		if (VNativeFunction* NativeFunction = Callee.DynamicCast<VNativeFunction>())
		{
			VFunction::Args Args;
			Args.AddUninitialized(NativeFunction->NumParameters);
			UnboxArguments(
				Context, NativeFunction->NumParameters, Op.Arguments.Num(),
				[&](uint32 Arg) {
					return GetOperand(Op.Arguments[Arg]);
				},
				[&](uint32 Param, VValue Value) {
					Args[Param] = Value;
				});
			FNativeCallResult Result = (*NativeFunction->Thunk)(Context, NativeFunction->ParentScope.Get(), Args);
			OP_RESULT_HELPER(Result);
			DEF(Op.Dest, Result.Value);
		}
		else
		{
			V_DIE_UNLESS(Op.Arguments.Num() == 1);

			VValue Argument = GetOperand(Op.Arguments[0]);
			// Special cases for known container types.
			if (VArrayBase* Array = Callee.DynamicCast<VArrayBase>())
			{
				REQUIRE_CONCRETE(Argument);
				// Bounds check since this index access in Verse is fallible.
				if (Argument.IsUint32() && Array->IsInBounds(Argument.AsUint32()))
				{
					DEF(Op.Dest, Array->GetValue(Argument.AsUint32()));
				}
				else
				{
					FAIL();
				}
			}
			else if (VMapBase* Map = Callee.DynamicCast<VMapBase>())
			{
				// TODO SOL-5621: We need to ensure the entire Key structure is concrete, not just the top-level.
				REQUIRE_CONCRETE(Argument);
				if (VValue Result = Map->Find(Argument))
				{
					DEF(Op.Dest, Result);
				}
				else
				{
					FAIL();
				}
			}
			else if (VUTF8String* String = Callee.DynamicCast<VUTF8String>())
			{
				REQUIRE_CONCRETE(Argument);
				if (Argument.IsUint32() && Argument.AsUint32() < String->Num())
				{
					DEF(Op.Dest, VValue::Char(String->Get(Argument.AsUint32())));
				}
				else
				{
					FAIL();
				}
			}
			else
			{
				V_DIE("Unknown callee");
			}
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewArrayImpl(OpType& Op)
	{
		const uint32 NumValues = Op.Values.Num();
		VArray& NewArray = VArray::New(Context, NumValues, [this, &Op](uint32 Index) { return GetOperand(Op.Values[Index]); });
		DEF(Op.Dest, NewArray);

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewMutableArrayImpl(OpType& Op)
	{
		const uint32 NumValues = Op.Values.Num();
		VMutableArray& NewArray = VMutableArray::New(Context, NumValues);
		for (uint32 Index = 0; Index < NumValues; ++Index)
		{
			const VValue VarArgValue = GetOperand(Op.Values[Index]);
			NewArray.AddValue(Context, VarArgValue);
		}
		DEF(Op.Dest, NewArray);

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewMutableArrayWithCapacityImpl(OpType& Op)
	{
		const VValue Size = GetOperand(Op.Size);
		REQUIRE_CONCRETE(Size); // Must be an Int32 (although UInt32 is better)
		DEF(Op.Dest, VMutableArray::New(Context, static_cast<uint32>(Size.AsInt32())));

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult ArrayAddImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		const VValue ValueToAdd = GetOperand(Op.ValueToAdd);
		REQUIRE_CONCRETE(Container);
		if (VMutableArray* Array = Container.DynamicCast<VMutableArray>())
		{
			Array->AddValue(Context, ValueToAdd);
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `ArrayAdd` operation!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult InPlaceMakeImmutableImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		REQUIRE_CONCRETE(Container);
		if (Container.IsCellOfType<VMutableArray>())
		{
			Container.StaticCast<VMutableArray>().InPlaceMakeImmutable(Context);
			checkSlow(Container.IsCellOfType<VArray>() && !Container.IsCellOfType<VMutableArray>());
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `InPlaceMakeImmutable` operation!");
		}

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewOptionImpl(OpType& Op)
	{
		VValue Value = GetOperand(Op.Value);

		DEF(Op.Dest, VOption::New(Context, Value));

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewMapImpl(OpType& Op)
	{
		const uint32 NumKeys = Op.Keys.Num();
		V_DIE_UNLESS(NumKeys == static_cast<uint32>(Op.Values.Num()));

		VMapBase& NewMap = VMapBase::New<VMap>(Context, NumKeys, [this, &Op](uint32 Index) {
			return TPair<VValue, VValue>(GetOperand(Op.Keys[Index]), GetOperand(Op.Values[Index]));
		});

		DEF(Op.Dest, NewMap);

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewClassImpl(OpType& Op)
	{
		VConstructor* Constructor = Op.Constructor.Get();
		TArray<VClass*> InheritedClasses = {};
		const uint32 NumInherited = Op.Inherited.Num();
		InheritedClasses.Reserve(NumInherited);
		for (uint32 Index = 0; Index < NumInherited; ++Index)
		{
			const VValue CurrentArg = GetOperand(Op.Inherited[Index]);
			REQUIRE_CONCRETE(CurrentArg);
			InheritedClasses.Add(&CurrentArg.StaticCast<VClass>());
		}
		VClass& NewClass = VClass::New(Context, nullptr, VClass::EKind::Class, *Constructor, InheritedClasses, nullptr);
		DEF(Op.Dest, NewClass);
		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult NewObjectImpl(OpType& Op, VClass& Class, VValue& NewObject, TArray<VProcedure*>& Initializers)
	{
		const uint32 NumFields = Op.Fields->Num();
		const uint32 NumValues = Op.Values.Num();

		V_DIE_UNLESS(NumFields == NumValues);

		TArray<VValue> ArchetypeValues;
		ArchetypeValues.Reserve(NumValues);
		for (uint32 Index = 0; Index < NumValues; ++Index)
		{
			VValue CurrentValue = GetOperand(Op.Values[Index]);
			REQUIRE_CONCRETE(CurrentValue);
			ArchetypeValues.Add(CurrentValue);
		}
		VUniqueStringSet& ArchetypeFields = *Op.Fields.Get();

		// UObject or VObject?
		const float UObjectProbablity = CVarUObjectProbablity.GetValueOnAnyThread();
		const bool bUObjectInsteadOfVObject = UObjectProbablity > 0.0f && (UObjectProbablity > RandomUObjectProbablity.FRand());
		if (bUObjectInsteadOfVObject)
		{
			V_RUNTIME_ERROR_IF(!CanAllocateUObjects(), Context, FUtf8String::Printf("Ran out of memory for allocating `UObject`s while attempting to construct a Verse object of type %s!", *Class.GetName().AsCString()));

			NewObject = Class.NewUObject(Context, ArchetypeFields, ArchetypeValues, Initializers);
		}
		else
		{
			NewObject = Class.NewVObject(Context, ArchetypeFields, ArchetypeValues, Initializers);
		}

		DEF(Op.Dest, NewObject);

		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult LoadFieldImpl(OpType& Op)
	{
		const VValue& ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);
		VUniqueString& FieldName = *Op.Name.Get();
		VValue FieldValue;
		if (!ObjectOperand.IsUObject())
		{
			VObject& Object = ObjectOperand.StaticCast<VObject>();
			FieldValue = Object.LoadField(Context, FieldName);
		}
		else
		{
			UObject* Object = ObjectOperand.AsUObject();
			UVerseVMClass* Class = static_cast<UVerseVMClass*>(Object->GetClass());
			FProperty* FieldProperty = Class->GetPropertyForField(Context, FieldName);
			FieldValue = FieldProperty->ContainerPtrToValuePtr<VRestValue>(Object)->Get(Context);
		}
		if (FieldValue.IsCellOfType<VProcedure>())
		{
			FieldValue = VFunction::New(Context, FieldValue.StaticCast<VProcedure>(), ObjectOperand);
		}
		else if (FieldValue.IsCellOfType<VNativeFunction>())
		{
			FieldValue = FieldValue.StaticCast<VNativeFunction>().Bind(Context, ObjectOperand);
		}
		DEF(Op.Dest, FieldValue);
		return {FOpResult::Normal};
	}

	template <typename OpType>
	FOpResult UnifyFieldImpl(OpType& Op)
	{
		const VValue& ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);
		VValue ValueOperand = GetOperand(Op.Value);
		REQUIRE_CONCRETE(ValueOperand);
		VUniqueString& FieldName = *Op.Name.Get();

		bool bSucceeded = false;
		if (!ObjectOperand.IsUObject())
		{
			VObject& Object = ObjectOperand.StaticCast<VObject>();

			const VEmergentType* EmergentType = Object.GetEmergentType();
			V_DIE_IF(EmergentType == nullptr);
			const VShape* Shape = EmergentType->Shape.Get();
			V_DIE_IF(Shape == nullptr);
			const VShape::VEntry* Field = Shape->GetField(Context, FieldName);
			V_DIE_IF(Field == nullptr);
			switch (Field->Type)
			{
				case EFieldType::Offset:
				{
					VRestValue& Slot = Object.GetFieldSlot(Context, FieldName);
					bSucceeded = Def(Slot, ValueOperand);
					break;
				}
				case EFieldType::Constant:
				{
					bSucceeded = Def(Field->Value.Get(), ValueOperand);
					break;
				}
				default:
					V_DIE("Field: %hs has an unsupported type; cannot unify!", Op.Name.Get()->AsCString());
					break;
			}
		}
		else
		{
			UObject* Object = ObjectOperand.AsUObject();
			UVerseVMClass* Class = static_cast<UVerseVMClass*>(Object->GetClass());
			FProperty* FieldProperty = Class->GetPropertyForField(Context, FieldName);
			VRestValue& Slot = *FieldProperty->ContainerPtrToValuePtr<VRestValue>(Object);
			bSucceeded = Def(Slot, ValueOperand);
		}

		return bSucceeded ? FOpResult{FOpResult::Normal} : FOpResult{FOpResult::Failed};
	}

	FOpResult NeqImplHelper(VValue LeftSource, VValue RightSource)
	{
		VValue ToSuspendOn;
		// This returns true for placeholders, so if we see any placeholders,
		// we're not yet done checking for inequality because we need to
		// check the concrete values.
		bool Result = VValue::Equal(Context, LeftSource, RightSource, [&](VValue Left, VValue Right) {
			checkSlow(Left.IsPlaceholder() || Right.IsPlaceholder());
			if (!ToSuspendOn)
			{
				ToSuspendOn = Left.IsPlaceholder() ? Left : Right;
			}
		});

		if (!Result)
		{
			return {FOpResult::Normal};
		}
		REQUIRE_CONCRETE(ToSuspendOn);
		FAIL();
	}

	FOpResult LtImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Lt(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() < RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Lt(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Lt` operation!");
		}

		return {FOpResult::Normal};
	}

	FOpResult LteImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Lte(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() <= RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Lte(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Lte` operation!");
		}

		return {FOpResult::Normal};
	}

	FOpResult GtImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Gt(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() > RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Gt(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Gt` operation!");
		}

		return {FOpResult::Normal};
	}

	FOpResult GteImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Gte(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() >= RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Gte(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Gte` operation!");
		}

		return {FOpResult::Normal};
	}

#define DECLARE_COMPARISON_OP_IMPL(OpName)                              \
	template <typename OpType>                                          \
	FOpResult OpName##Impl(OpType& Op)                                  \
	{                                                                   \
		VValue LeftSource = GetOperand(Op.LeftSource);                  \
		VValue RightSource = GetOperand(Op.RightSource);                \
		FOpResult Result = OpName##ImplHelper(LeftSource, RightSource); \
		if (Result.Kind == FOpResult::Normal)                           \
		{                                                               \
			/* success returns the left - value */                      \
			Def(Op.Dest, LeftSource);                                   \
		}                                                               \
		return Result;                                                  \
	}

	DECLARE_COMPARISON_OP_IMPL(Neq)
	DECLARE_COMPARISON_OP_IMPL(Lt)
	DECLARE_COMPARISON_OP_IMPL(Lte)
	DECLARE_COMPARISON_OP_IMPL(Gt)
	DECLARE_COMPARISON_OP_IMPL(Gte)

#undef FAIL
#undef ENQUEUE_SUSPENSION

#define OP_IMPL_HELPER(OpName, ...)                     \
	FOpResult Result = OpName##Impl(Op, ##__VA_ARGS__); \
	OP_RESULT_HELPER(Result)

/// Use this macro for defining a new opcode implementation if it can possibly
/// suspend as part of execution.
#define OP_IMPL(OpName)    \
	BEGIN_OP_CASE(OpName){ \
		OP_IMPL_HELPER(OpName)} END_OP_CASE()

/// Use this macro for defining a new opcode implementation if it does not have
/// any capability to suspend execution. (e.g. if there are no
/// `REQUIRE_CONCRETE` macros used in the implementation.)
#define OP_IMPL_NO_SUSPENDS(OpName)           \
	BEGIN_OP_CASE(OpName)                     \
	{                                         \
		FOpResult Result = OpName##Impl(Op);  \
		if (Result.Kind != FOpResult::Normal) \
		{                                     \
			FAIL();                           \
		}                                     \
	}                                         \
	END_OP_CASE()

// We REQUIRE_CONCRETE on the effect token first because it obviates the need to capture
// the incoming effect token. If the incoming effect token is a placeholder, we will
// suspend, and we'll only resume after it becomes concrete.
#define OP_IMPL_THREAD_EFFECTS(OpName)                         \
	BEGIN_OP_CASE(OpName)                                      \
	{                                                          \
		VValue IncomingEffectToken = EffectToken.Get(Context); \
		BumpEffectEpoch();                                     \
		REQUIRE_CONCRETE(IncomingEffectToken);                 \
		OP_IMPL_HELPER(OpName)                                 \
		DEF(EffectToken, VValue::EffectDoneMarker());          \
	}                                                          \
	END_OP_CASE()

	// NOTE: (yiliang.siew) We don't templat-ize `bHasOutermostPCBounds` since it would mean duplicating the codegen
	// where `ExecuteImpl` gets called. Since it's the interpreter loop and a really big function, it bloats compile times.
	template <bool bPrintTrace>
	FORCENOINLINE void ExecuteImpl(const bool bHasOutermostPCBounds)
	{
#define NEXT_OP(bSuspended, bFailed)   \
	if constexpr (bPrintTrace)         \
	{                                  \
		EndTrace(bSuspended, bFailed); \
	}                                  \
	NextOp();                          \
	break

#define BEGIN_OP_CASE(Name)                                 \
	case EOpcode::Name:                                     \
	{                                                       \
		if constexpr (bPrintTrace)                          \
		{                                                   \
			BeginTrace();                                   \
		}                                                   \
		FOp##Name& Op = *static_cast<FOp##Name*>(State.PC); \
		NextPC = BitCast<FOp*>(&Op + 1);

#define END_OP_CASE()      \
	NEXT_OP(false, false); \
	}

#define ENQUEUE_SUSPENSION(Value)                                                                                                                    \
	VBytecodeSuspension& Suspension = VBytecodeSuspension::New(Context, *State.FailureContext, *State.Frame->Procedure, State.PC, MakeCaptures(Op)); \
	Value.EnqueueSuspension(Context, Suspension);                                                                                                    \
	++State.FailureContext->SuspensionCount;                                                                                                         \
	NEXT_OP(true, false);

#define FAIL()                   \
	Fail(*State.FailureContext); \
	if (!UnwindIfNeeded())       \
	{                            \
		return;                  \
	}                            \
	NextPC = State.PC;           \
	NEXT_OP(false, true);

	MainInterpreterLoop:
		while (true)
		{
			FOp* NextPC = nullptr;

			auto UpdateExecutionState = [&](VFrame* Frame, FOp* PC, VFailureContext& FailureContext) {
				State = FExecutionState(Frame, nullptr, &FailureContext);
				NextPC = PC;
			};

			auto ReturnTo = [&](VFrame* Frame, FOp* PC) {
				if (Frame)
				{
					UpdateExecutionState(Frame, PC, *State.FailureContext);
				}
				else
				{
					NextPC = &StopInterpreterSentry;
				}
			};

			auto NextOp = [&] {
				if (bHasOutermostPCBounds)
				{
					if (UNLIKELY(IsOutermostFrame()
								 && (NextPC < OutermostStartPC || NextPC >= OutermostEndPC)))
					{
						NextPC = &StopInterpreterSentry;
					}
				}

				State.PC = NextPC;
			};

			Context.CheckForHandshake();

			switch (State.PC->Opcode)
			{
				OP_IMPL(Add)
				OP_IMPL(Sub)
				OP_IMPL(Mul)
				OP_IMPL(Div)
				OP_IMPL(Mod)
				OP_IMPL(Neg)

				OP_IMPL(MutableAdd)

				OP_IMPL(Neq)
				OP_IMPL(Lt)
				OP_IMPL(Lte)
				OP_IMPL(Gt)
				OP_IMPL(Gte)

				OP_IMPL(Query)

				OP_IMPL_THREAD_EFFECTS(Melt)
				OP_IMPL_THREAD_EFFECTS(Freeze)

				OP_IMPL_THREAD_EFFECTS(VarGet)
				OP_IMPL_THREAD_EFFECTS(VarSet)
				OP_IMPL_THREAD_EFFECTS(IndexSet)

				OP_IMPL(NewOption)
				OP_IMPL(Length)
				OP_IMPL(NewArray)
				OP_IMPL(NewMutableArray)
				OP_IMPL(NewMutableArrayWithCapacity)
				OP_IMPL_THREAD_EFFECTS(ArrayAdd)
				OP_IMPL(InPlaceMakeImmutable)
				OP_IMPL(NewMap)
				OP_IMPL(MapKey)
				OP_IMPL(MapValue)
				OP_IMPL(NewClass)
				OP_IMPL(LoadField)
				OP_IMPL(UnifyField)

				BEGIN_OP_CASE(Err)
				{
					// If this is the stop interpreter sentry op, return.
					if (&Op == &StopInterpreterSentry)
					{
						return;
					}

					UE_LOG(LogVerseVM, Error, TEXT("Interpreted Err op"));
					return;
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Move)
				{
					// TODO SOL-4459: This doesn't work with leniency and failure. For example,
					// if both Dest/Source are placeholders, failure will never be associated
					// to this Move, but that can't be right.
					DEF(Op.Dest, GetOperand(Op.Source));
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Jump)
				{
					NextPC = Op.JumpOffset.GetLabeledPC();
				}
				END_OP_CASE()

				BEGIN_OP_CASE(BeginFailureContext)
				{
					VFailureContext& FailureContext = VFailureContext::New(Context, State.FailureContext, *State.Frame, EffectToken.Get(Context), Op.OnFailure.GetLabeledPC());
					State.FailureContext = &FailureContext;

					if (VValue IncomingEffectToken = EffectToken.Get(Context); IncomingEffectToken.IsPlaceholder())
					{
						BumpEffectEpoch();
						DoTransactionActionWhenEffectTokenIsConcrete<TransactAction::Start>(FailureContext, IncomingEffectToken, EffectToken.Get(Context));
					}
					else
					{
						FailureContext.Transaction.Start(Context);
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(EndFailureContext)
				{
					VFailureContext& FailureContext = *State.FailureContext;
					V_DIE_IF(FailureContext.bFailed); // We shouldn't have failed and still made it here.

					FailureContext.bExecutedEndFailureContextOpcode = true;
					FailureContext.ThenPC = NextPC;
					FailureContext.DonePC = Op.Done.GetLabeledPC();

					if (FailureContext.SuspensionCount)
					{
						if (FailureContext.Parent)
						{
							++FailureContext.Parent->SuspensionCount;
						}
						FailureContext.BeforeThenEffectToken.Set(Context, EffectToken.Get(Context));
						EffectToken.Set(Context, FailureContext.DoneEffectToken.Get(Context));
						NextPC = Op.Done.GetLabeledPC();
						FailureContext.Frame.Set(Context, FailureContext.Frame->CloneWithoutCallerInfo(Context));
					}
					else
					{
						FailureContext.FinishedExecuting(Context);

						if (VValue IncomingEffectToken = EffectToken.Get(Context); IncomingEffectToken.IsPlaceholder())
						{
							BumpEffectEpoch();
							DoTransactionActionWhenEffectTokenIsConcrete<TransactAction::Commit>(FailureContext, IncomingEffectToken, EffectToken.Get(Context));
						}
						else
						{
							FailureContext.Transaction.Commit(Context);
						}
					}

					State.FailureContext = FailureContext.Parent.Get();
				}
				END_OP_CASE()

				// An indexed access (i.e. `B := A[10]`) is just the same as `Call(B, A, 10)`.
				BEGIN_OP_CASE(Call)
				{
					VValue Callee = GetOperand(Op.Callee);
					REQUIRE_CONCRETE(Callee);

					if (VFunction* Function = Callee.DynamicCast<VFunction>())
					{
						VRestValue* ReturnSlot = &State.Frame->Registers[Op.Dest.Index];
						VFrame& NewFrame = MakeFrameForCallee(Context, State.Frame, NextPC, ReturnSlot, *Function, Op.Arguments.Num(),
							[&](uint32 Arg) {
								return GetOperand(Op.Arguments[Arg]);
							});
						UpdateExecutionState(&NewFrame, Function->GetProcedure().GetOpsBegin(), *State.FailureContext);
					}
					else
					{
						OP_IMPL_HELPER(Call, Callee);
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Return)
				{
					// TODO SOL-4461: Return should work with lenient execution of failure contexts.
					// We can't just logically execute the first Return we encounter during lenient
					// execution if the then/else when executed would've returned.
					//
					// We also need to figure out how to properly pop a frame off if the
					// failure context we're leniently executing returns. We could continue
					// to execute the current frame and just not thread through the effect
					// token, so no effects could happen. But that's inefficient.

					VValue IncomingEffectToken = EffectToken.Get(Context);
					DEF(State.Frame->ReturnEffectToken, IncomingEffectToken); // This can't fail.

					VValue Value = GetOperand(Op.Value);
					VFrame& Frame = *State.Frame;

					ReturnTo(State.Frame->CallerFrame.Get(), State.Frame->CallerPC);

					// TODO: Add a test where this unification fails at the top level with no return continuation.
					if (Frame.ReturnKind == VFrame::EReturnKind::RestValue)
					{
						if (Frame.Return.RestValue)
						{
							DEF(*Frame.Return.RestValue, Value);
						}
					}
					else
					{
						checkSlow(Frame.ReturnKind == VFrame::EReturnKind::Value);
						if (Frame.Return.Value)
						{
							DEF(Frame.Return.Value.Get(), Value);
						}
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(NewObject)
				{
					VValue ClassOperand = GetOperand(Op.Class);
					REQUIRE_CONCRETE(ClassOperand);
					VClass& Class = ClassOperand.StaticCast<VClass>();

					VValue Object;
					TArray<VProcedure*> Initializers;
					OP_IMPL_HELPER(NewObject, Class, Object, Initializers);

					// Push initializers onto the stack in reverse order to run them in forward order.
					while (Initializers.Num() > 0)
					{
						VProcedure& Procedure = *Initializers.Pop();
						VFunction& Function = VFunction::New(Context, Procedure, Object);
						VRestValue* ReturnSlot = nullptr;
						VFrame& NewFrame = MakeFrameForCallee(Context, State.Frame, NextPC, ReturnSlot, Function, 0,
							[](uint32 Arg) -> VValue { VERSE_UNREACHABLE(); });
						UpdateExecutionState(&NewFrame, Procedure.GetOpsBegin(), *State.FailureContext);
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Reset)
				{
					State.Frame->Registers[Op.Dest.Index].Reset(0);
				}
				END_OP_CASE()

				BEGIN_OP_CASE(NewVar)
				{
					DEF(Op.Dest, VVar::New(Context));
				}
				END_OP_CASE()

				default:
					V_DIE("Invalid opcode: %u", static_cast<FOpcodeInt>(State.PC->Opcode));
			}

			if (CurrentSuspension)
			{
				goto SuspensionInterpreterLoop;
			}
		}
#undef BEGIN_OP_CASE
#undef END_OP_CASE
#undef ENQUEUE_SUSPENSION
#undef OP_IMPL_THREAD_EFFECTS
#undef FAIL

#define BEGIN_OP_CASE(Name)                                                                              \
	case EOpcode::Name:                                                                                  \
	{                                                                                                    \
		F##Name##SuspensionCaptures& Op = BytecodeSuspension.GetCaptures<F##Name##SuspensionCaptures>(); \
		if constexpr (bPrintTrace)                                                                       \
		{                                                                                                \
			BeginTrace(Op, BytecodeSuspension);                                                          \
		}

#define END_OP_CASE()                                                  \
	FinishedExecutingSuspensionIn(*BytecodeSuspension.FailureContext); \
	if constexpr (bPrintTrace)                                         \
	{                                                                  \
		EndTraceWithCaptures(Op, false, false);                        \
	}                                                                  \
	break;                                                             \
	}

#define ENQUEUE_SUSPENSION(Value)                         \
	Value.EnqueueSuspension(Context, *CurrentSuspension); \
	if constexpr (bPrintTrace)                            \
	{                                                     \
		EndTraceWithCaptures(Op, true, false);            \
	}                                                     \
	break;

#define OP_IMPL_THREAD_EFFECTS(OpName)                   \
	BEGIN_OP_CASE(OpName)                                \
	{                                                    \
		OP_IMPL_HELPER(OpName)                           \
		DEF(Op.EffectToken, VValue::EffectDoneMarker()); \
	}                                                    \
	END_OP_CASE()

#define FAIL()                                 \
	if constexpr (bPrintTrace)                 \
	{                                          \
		EndTraceWithCaptures(Op, false, true); \
	}                                          \
	Fail(*BytecodeSuspension.FailureContext);  \
	break;

	SuspensionInterpreterLoop:
		do
		{
			check(!!CurrentSuspension);
			if (!CurrentSuspension->FailureContext->bFailed)
			{
				if (VLambdaSuspension* LambdaSuspension = CurrentSuspension->DynamicCast<VLambdaSuspension>())
				{
					LambdaSuspension->Callback(Context, *LambdaSuspension, CurrentSuspension);
				}
				else
				{
					VBytecodeSuspension& BytecodeSuspension = CurrentSuspension->StaticCast<VBytecodeSuspension>();
					switch (BytecodeSuspension.PC->Opcode)
					{
						OP_IMPL(Add)
						OP_IMPL(Sub)
						OP_IMPL(Mul)
						OP_IMPL(Div)
						OP_IMPL(Mod)
						OP_IMPL(Neg)

						OP_IMPL(MutableAdd)

						OP_IMPL(Neq)
						OP_IMPL(Lt)
						OP_IMPL(Lte)
						OP_IMPL(Gt)
						OP_IMPL(Gte)

						OP_IMPL(Query)

						OP_IMPL_THREAD_EFFECTS(Melt)
						OP_IMPL_THREAD_EFFECTS(Freeze)

						OP_IMPL_THREAD_EFFECTS(VarGet)
						OP_IMPL_THREAD_EFFECTS(VarSet)
						OP_IMPL_THREAD_EFFECTS(IndexSet)

						OP_IMPL(Length)

						// An indexed access (i.e. `B := A[10]`) is just the same as `Call(B, A, 10)`.
						BEGIN_OP_CASE(Call)
						{
							VValue Callee = GetOperand(Op.Callee);
							REQUIRE_CONCRETE(Callee);

							if (VFunction* Function = Callee.DynamicCast<VFunction>())
							{
								VFrame* CallerFrame = nullptr;
								FOp* CallerPC = nullptr;

								VFrame& NewFrame = MakeFrameForCallee(Context, CallerFrame, CallerPC, GetOperand(Op.Dest), *Function, Op.Arguments.Num(),
									[&](uint32 Arg) {
										return GetOperand(Op.Arguments[Arg]);
									});
								NewFrame.ReturnEffectToken.Set(Context, GetOperand(Op.ReturnEffectToken));
								// TODO SOL-4435: Enact some recursion limit here since we're using the machine stack.
								VFailureContext& FailureContext = *CurrentSuspension->FailureContext.Get();
								FInterpreter::InvokeFunction(Context, NewFrame, FailureContext, *Function, GetOperand(Op.EffectToken));
							}
							else
							{
								OP_IMPL_HELPER(Call, Callee);
								DEF(Op.ReturnEffectToken, GetOperand(Op.EffectToken));
							}
						}
						END_OP_CASE()

						default:
							V_DIE("Invalid opcode: %u", static_cast<FOpcodeInt>(State.PC->Opcode));
					}
				}
			}

			VSuspension* NextSuspension = CurrentSuspension->Next.Get();
			CurrentSuspension->Next.Set(Context, nullptr);
			CurrentSuspension = NextSuspension;
		}
		while (CurrentSuspension);

		if (!UnwindIfNeeded())
		{
			return;
		}

		goto MainInterpreterLoop;

#undef BEGIN_OP_CASE
#undef END_OP_CASE
#undef ENQUEUE_SUSPENSION
#undef FAIL

#undef REQUIRE_CONCRETE
#undef OP_IMPL
#undef DEF
	}

public:
	FInterpreter(FRunningContext Context, FExecutionState State, VValue IncomingEffectToken, FOp* StartPC = nullptr, FOp* EndPC = nullptr)
		: Context(Context)
		, State(State)
		, OutermostFailureContext(State.FailureContext)
		, OutermostStartPC(StartPC)
		, OutermostEndPC(EndPC)
	{
		V_DIE_UNLESS(OutermostFailureContext);
		V_DIE_UNLESS(!!OutermostStartPC == !!OutermostEndPC);
		V_DIE_UNLESS(IsOutermostFrame()); // We should begin execution in the outermost frame.
		EffectToken.Set(Context, IncomingEffectToken);
	}

	void Execute()
	{
		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			if (OutermostStartPC)
			{
				ExecuteImpl<true>(true);
			}
			else
			{
				ExecuteImpl<true>(false);
			}
		}
		else
		{
			if (OutermostStartPC)
			{
				ExecuteImpl<false>(true);
			}
			else
			{
				ExecuteImpl<false>(false);
			}
		}
	}

	static void InvokeFunction(FRunningContext Context, VFrame& Frame, VFailureContext& FailureContext, VFunction& Function, VValue IncomingEffectToken)
	{
		FInterpreter Interpreter(Context, FExecutionState(&Frame, Function.GetProcedure().GetOpsBegin(), &FailureContext), IncomingEffectToken);
		Interpreter.Execute();
	}

	// Upon failure, returns an uninitialized VValue
	static VValue InvokeInTransaction(FRunningContext Context, VFunction::Args&& IncomingArguments, VFunction& Function)
	{
		VRestValue ReturnSlot(0);

		VFunction::Args Arguments = MoveTemp(IncomingArguments);

		VFrame* CallerFrame = nullptr;
		FOp* CallerPC = nullptr;
		VFrame& Frame = MakeFrameForCallee(Context, CallerFrame, CallerPC, &ReturnSlot, Function, Arguments.Num(),
			[&](uint32 Arg) {
				return Arguments[Arg];
			});
		VFailureContext& FailureContext = VFailureContext::New(
			Context,
			/*Parent*/ nullptr,
			Frame,
			VValue(), // IncomingEffectToken doesn't matter here, since we bail out if we fail at the top level.
			&StopInterpreterSentry);

		FInterpreter Interpreter(Context, FExecutionState(&Frame, Function.GetProcedure().GetOpsBegin(), &FailureContext), VValue::EffectDoneMarker());
		AutoRTFM::TransactThenOpen([&] {
			FailureContext.Transaction.Start(Context);
			Interpreter.Execute();
			if (!FailureContext.Transaction.bHasAborted)
			{
				FailureContext.Transaction.Commit(Context);
			}
		});

		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			UE_LOG(LogVerseVM, Display, TEXT("\n"));
		}

		VValue Result = FailureContext.bFailed ? VValue() : ReturnSlot.Get(Context);
		return Result;
	}
};

VValue VFunction::InvokeInTransaction(FRunningContext Context, VFunction::Args&& Args)
{
	VValue Result = FInterpreter::InvokeInTransaction(Context, MoveTemp(Args), *this);
	check(!Result.IsPlaceholder());
	return Result;
}

VValue VFunction::InvokeInTransaction(FRunningContext Context, VValue Argument)
{
	VValue Result = FInterpreter::InvokeInTransaction(Context, VFunction::Args{Argument}, *this);
	check(!Result.IsPlaceholder());
	return Result;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
