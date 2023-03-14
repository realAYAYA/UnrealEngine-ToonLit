// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMNativized.h"
#include "UObject/Package.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "HAL/PlatformTLS.h"
#include "Async/ParallelFor.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVM)

void FRigVMParameter::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::StoreMarkerNamesOnSkeleton)
	{
		return;
	}

	if (Ar.IsSaving() || Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Save(Ar);
	}
	else if (Ar.IsLoading())
	{
		Load(Ar);
	}
	else
	{
		// remove due to FPIEFixupSerializer hitting this checkNoEntry();
	}
}

void FRigVMParameter::Save(FArchive& Ar)
{
	Ar << Type;
	Ar << Name;
	Ar << RegisterIndex;
	Ar << CPPType;
	Ar << ScriptStructPath;
}

void FRigVMParameter::Load(FArchive& Ar)
{
	Ar << Type;
	Ar << Name;
	Ar << RegisterIndex;
	Ar << CPPType;
	Ar << ScriptStructPath;

	ScriptStruct = nullptr;
}

UScriptStruct* FRigVMParameter::GetScriptStruct() const
{
	if (ScriptStruct == nullptr)
	{
		if (ScriptStructPath != NAME_None)
		{
			FRigVMParameter* MutableThis = (FRigVMParameter*)this;
			MutableThis->ScriptStruct = FindObject<UScriptStruct>(nullptr, *ScriptStructPath.ToString());
		}
	}
	return ScriptStruct;
}

URigVM::URigVM()
	: WorkMemoryStorageObject(nullptr)
	, LiteralMemoryStorageObject(nullptr)
	, DebugMemoryStorageObject(nullptr)
	, ByteCodePtr(&ByteCodeStorage)
	, NumExecutions(0)
#if WITH_EDITOR
	, DebugInfo(nullptr)
	, HaltedAtBreakpointHit(INDEX_NONE)
#endif
    , FunctionNamesPtr(&FunctionNamesStorage)
    , FunctionsPtr(&FunctionsStorage)
    , FactoriesPtr(&FactoriesStorage)
#if WITH_EDITOR
	, FirstEntryEventInQueue(NAME_None)
#endif
	, ExecutingThreadId(INDEX_NONE)
	, DeferredVMToCopy(nullptr)
{
}

URigVM::~URigVM()
{
	Reset();

	ExecutionReachedExit().Clear();
#if WITH_EDITOR
	ExecutionHalted().Clear();
#endif
}

void URigVM::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::StoreMarkerNamesOnSkeleton)
	{
		return;
	}

	// call into the super class to serialize any uproperty
	if(Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Super::Serialize(Ar);
	}
	
	ensure(ExecutingThreadId == INDEX_NONE);

	if (Ar.IsSaving() || Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Save(Ar);
	}
	else if (Ar.IsLoading())
	{
		Load(Ar);
	}
	else
	{
		// remove due to FPIEFixupSerializer hitting this checkNoEntry();
	}
}

void URigVM::Save(FArchive& Ar)
{
	CopyDeferredVMIfRequired();

	int32 RigVMUClassBasedStorageDefine = 0; // this used to 
	Ar << RigVMUClassBasedStorageDefine;

	// we rely on Ar.IsIgnoringArchetypeRef for determining if we are currently performing
	// CPFUO (Copy Properties for unrelated objects). During a reinstance pass we don't
	// want to overwrite the bytecode and some other properties - since that's handled already
	// by the RigVMCompiler.
	if(!Ar.IsIgnoringArchetypeRef())
	{
		Ar << ExternalPropertyPathDescriptions;
		Ar << FunctionNamesStorage;
		Ar << ByteCodeStorage;
		Ar << Parameters;
	}
}

void URigVM::Load(FArchive& Ar)
{
	// we rely on Ar.IsIgnoringArchetypeRef for determining if we are currently performing
	// CPFUO (Copy Properties for unrelated objects). During a reinstance pass we don't
	// want to overwrite the bytecode and some other properties - since that's handled already
	// by the RigVMCompiler.
	Reset(Ar.IsIgnoringArchetypeRef());

	int32 RigVMUClassBasedStorageDefine = 1;
	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::RigVMMemoryStorageObject)
	{
		Ar << RigVMUClassBasedStorageDefine;
	}

	if(RigVMUClassBasedStorageDefine == 1)
	{
		FRigVMMemoryContainer WorkMemoryStorage;
		FRigVMMemoryContainer LiteralMemoryStorage;
		
		Ar << WorkMemoryStorage;
		Ar << LiteralMemoryStorage;
		Ar << FunctionNamesStorage;
		Ar << ByteCodeStorage;
		Ar << Parameters;
		
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMCopyOpStoreNumBytes)
		{
			Reset();
			return;
		}
	}

	// we only deal with virtual machines now that use the new memory infrastructure.
	ensure(UE_RIGVM_UCLASS_BASED_STORAGE_DISABLED == 0);
	if(RigVMUClassBasedStorageDefine != UE_RIGVM_UCLASS_BASED_STORAGE_DISABLED)
	{
		Reset();
		return;
	}

	// requesting the memory types will create them
	// Cooked platforms will just load the objects and do no need to clear the referenes
	// In certain scenarios RequiresCookedData wil be false but the PKG_FilterEditorOnly will still be set (UEFN)
	if (!FPlatformProperties::RequiresCookedData() && !GetClass()->RootPackageHasAnyFlags(PKG_FilterEditorOnly))
	{
		ClearMemory();
	}

	if(!Ar.IsIgnoringArchetypeRef())
	{
		Ar << ExternalPropertyPathDescriptions;
		Ar << FunctionNamesStorage;
		Ar << ByteCodeStorage;
		Ar << Parameters;
	}
}

void URigVM::PostLoad()
{
	Super::PostLoad();
	
	ClearMemory();

	TArray<ERigVMMemoryType> MemoryTypes;
	MemoryTypes.Add(ERigVMMemoryType::Literal);
	MemoryTypes.Add(ERigVMMemoryType::Work);
	MemoryTypes.Add(ERigVMMemoryType::Debug);

	for(ERigVMMemoryType MemoryType : MemoryTypes)
	{
		if(URigVMMemoryStorageGeneratorClass* Class = URigVMMemoryStorageGeneratorClass::GetStorageClass(this, MemoryType))
		{
			if(Class->LinkedProperties.Num() == 0)
			{
				Class->RefreshLinkedProperties();
			}
			if(Class->PropertyPathDescriptions.Num() != Class->PropertyPaths.Num())
			{
				Class->RefreshPropertyPaths();
			}
		}
	}
	
	RefreshExternalPropertyPaths();

	if (!ValidateAllOperandsDuringLoad())
	{
		Reset();
	}
	else
	{
		Instructions.Reset();
		FunctionsStorage.Reset();
		FactoriesStorage.Reset();
		ParametersNameMap.Reset();

		for (int32 Index = 0; Index < Parameters.Num(); Index++)
		{
			ParametersNameMap.Add(Parameters[Index].Name, Index);
		}

		// rebuild the bytecode to adjust for byte shifts in shipping
		RebuildByteCodeOnLoad();

		InvalidateCachedMemory();
	}
}

#if WITH_EDITORONLY_DATA
void URigVM::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(URigVMMemoryStorage::StaticClass()));
}
#endif

uint32 URigVM::GetVMHash() const
{
	uint32 Hash = 0;
	for(const FName& FunctionName : GetFunctionNames())
	{
		Hash = HashCombine(Hash, GetTypeHash(FunctionName.ToString()));
	}

	Hash = HashCombine(Hash, GetTypeHash(GetByteCode()));

	for(const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		Hash = HashCombine(Hash, GetTypeHash(ExternalVariable.Name.ToString()));
		Hash = HashCombine(Hash, GetTypeHash(ExternalVariable.TypeName.ToString()));
	}

	if(LiteralMemoryStorageObject)
	{
		Hash = HashCombine(Hash, LiteralMemoryStorageObject->GetMemoryHash());
	}
	if(WorkMemoryStorageObject)
	{
		Hash = HashCombine(Hash, WorkMemoryStorageObject->GetMemoryHash());
	}
	
	return Hash;
}

UClass* URigVM::GetNativizedClass(const TArray<FRigVMExternalVariable>& InExternalVariables)
{
	TSharedPtr<TGuardValue<TArray<FRigVMExternalVariable>>> GuardPtr;
	if(!InExternalVariables.IsEmpty())
	{
		GuardPtr = MakeShareable(new TGuardValue<TArray<FRigVMExternalVariable>>(ExternalVariables, InExternalVariables));
	}

	const uint32 VMHash = GetVMHash();
	
	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		if (ClassIterator->IsChildOf(URigVMNativized::StaticClass()) && (*ClassIterator != URigVMNativized::StaticClass()))
		{
			if(const URigVM* NativizedVMCDO = ClassIterator->GetDefaultObject<URigVM>())
			{
				if(NativizedVMCDO->GetVMHash() == VMHash)
				{
					return *ClassIterator;
				}
			}
		}
	}

	return nullptr;
}

bool URigVM::ValidateAllOperandsDuringLoad()
{
	// check all operands on all ops for validity
	bool bAllOperandsValid = true;

	TArray<URigVMMemoryStorage*> LocalMemory = { GetWorkMemory(), GetLiteralMemory(), GetDebugMemory() };
	
	auto CheckOperandValidity = [LocalMemory, &bAllOperandsValid, this](const FRigVMOperand& InOperand) -> bool
	{
		if(InOperand.GetContainerIndex() < 0 || InOperand.GetContainerIndex() >= (int32)ERigVMMemoryType::Invalid)
		{
			bAllOperandsValid = false;
			return false;
		}


		const URigVMMemoryStorage* MemoryForOperand = LocalMemory[InOperand.GetContainerIndex()];
		if(InOperand.GetMemoryType() != ERigVMMemoryType::External)
		{
			if(!MemoryForOperand->IsValidIndex(InOperand.GetRegisterIndex()))
			{
				bAllOperandsValid = false;
				return false;
			}

			if(InOperand.GetRegisterOffset() != INDEX_NONE)
			{
				if(!MemoryForOperand->GetPropertyPaths().IsValidIndex(InOperand.GetRegisterOffset()))
				{
					bAllOperandsValid = false;
					return false;
				}
			}
		}
		else if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
		{
			// given that external variables array is populated at runtime
			// checking for property path is the best we can do
			if(InOperand.GetRegisterOffset() != INDEX_NONE)
			{
				if (!ExternalPropertyPathDescriptions.IsValidIndex(InOperand.GetRegisterOffset()))
				{
					bAllOperandsValid = false;
					return false;
				}
			}
		}
		return true;
	};
	
	const FRigVMInstructionArray ByteCodeInstructions = ByteCodeStorage.GetInstructions();
	for(const FRigVMInstruction& ByteCodeInstruction : ByteCodeInstructions)
	{
		switch (ByteCodeInstruction.OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				const FRigVMExecuteOp& Op = ByteCodeStorage.GetOpAt<FRigVMExecuteOp>(ByteCodeInstruction);
				FRigVMOperandArray Operands = ByteCodeStorage.GetOperandsForExecuteOp(ByteCodeInstruction);
				for (const FRigVMOperand& Arg : Operands)
				{
					CheckOperandValidity(Arg);
				}
				break;
			}
			case ERigVMOpCode::Zero:
			case ERigVMOpCode::BoolFalse:
			case ERigVMOpCode::BoolTrue:
			case ERigVMOpCode::Increment:
			case ERigVMOpCode::Decrement:
			case ERigVMOpCode::ArrayReset:
			case ERigVMOpCode::ArrayReverse:
			{
				const FRigVMUnaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMUnaryOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.Arg);
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCodeStorage.GetOpAt<FRigVMCopyOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.Source);
				CheckOperandValidity(Op.Target);
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCodeStorage.GetOpAt<FRigVMComparisonOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.A);
				CheckOperandValidity(Op.B);
				CheckOperandValidity(Op.Result);
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCodeStorage.GetOpAt<FRigVMJumpIfOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.Arg);
				break;
			}
			case ERigVMOpCode::BeginBlock:
			case ERigVMOpCode::ArrayGetNum:
			case ERigVMOpCode::ArraySetNum:
			case ERigVMOpCode::ArrayAppend:
			case ERigVMOpCode::ArrayClone:
			case ERigVMOpCode::ArrayRemove:
			case ERigVMOpCode::ArrayUnion:
			{
				const FRigVMBinaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMBinaryOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.ArgA);
				CheckOperandValidity(Op.ArgB);
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			case ERigVMOpCode::ArrayGetAtIndex:
			case ERigVMOpCode::ArraySetAtIndex:
			case ERigVMOpCode::ArrayInsert:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
			{
				const FRigVMTernaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMTernaryOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.ArgA);
				CheckOperandValidity(Op.ArgB);
				CheckOperandValidity(Op.ArgC);
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				const FRigVMQuaternaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMQuaternaryOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.ArgA);
				CheckOperandValidity(Op.ArgB);
				CheckOperandValidity(Op.ArgC);
				CheckOperandValidity(Op.ArgD);
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				const FRigVMSenaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMSenaryOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.ArgA);
				CheckOperandValidity(Op.ArgB);
				CheckOperandValidity(Op.ArgC);
				CheckOperandValidity(Op.ArgD);
				CheckOperandValidity(Op.ArgE);
				CheckOperandValidity(Op.ArgF);
				break;
			}
			case ERigVMOpCode::Invalid:
			{
				ensure(false);
				break;
			}
			default:
			{
				break;
			}
		}
	}

	return bAllOperandsValid;
}

void URigVM::Reset(bool IsIgnoringArchetypeRef)
{
	if(!IsIgnoringArchetypeRef)
	{
		FunctionNamesStorage.Reset();
		FunctionsStorage.Reset();
		FactoriesStorage.Reset();
		ExternalPropertyPathDescriptions.Reset();
		ExternalPropertyPaths.Reset();
		ByteCodeStorage.Reset();
		Instructions.Reset();
		Parameters.Reset();
		ParametersNameMap.Reset();
	}
	DeferredVMToCopy = nullptr;

	if(!IsIgnoringArchetypeRef)
	{
		FunctionNamesPtr = &FunctionNamesStorage;
		FunctionsPtr = &FunctionsStorage;
		FactoriesPtr = &FactoriesStorage;
		ByteCodePtr = &ByteCodeStorage;
	}

	InvalidateCachedMemory();
	
	OperandToDebugRegisters.Reset();
	NumExecutions = 0;
}

void URigVM::Empty()
{
	FunctionNamesStorage.Empty();
	FunctionsStorage.Empty();
	FactoriesStorage.Empty();
	ExternalPropertyPathDescriptions.Empty();
	ExternalPropertyPaths.Empty();
	ByteCodeStorage.Empty();
	Instructions.Empty();
	Parameters.Empty();
	ParametersNameMap.Empty();
	DeferredVMToCopy = nullptr;
	ExternalVariables.Empty();

	InvalidateCachedMemory();

	CachedMemory.Empty();
	FirstHandleForInstruction.Empty();
	CachedMemoryHandles.Empty();

	OperandToDebugRegisters.Empty();
}

void URigVM::CopyFrom(URigVM* InVM, bool bDeferCopy, bool bReferenceLiteralMemory, bool bReferenceByteCode, bool bCopyExternalVariables, bool bCopyDynamicRegisters)
{
	check(InVM);

	// if this vm is currently executing on a worker thread
	// we defer the copy until the next execute
	if (ExecutingThreadId != INDEX_NONE || bDeferCopy)
	{
		DeferredVMToCopy = InVM;
		return;
	}
	
	Reset();

	auto CopyMemoryStorage = [](TObjectPtr<URigVMMemoryStorage>& TargetMemory, URigVMMemoryStorage* SourceMemory, UObject* Outer)
	{
		if(SourceMemory != nullptr)
		{
			if(TargetMemory == nullptr)
			{
				TargetMemory = NewObject<URigVMMemoryStorage>(Outer, SourceMemory->GetClass());
			}
			else if(TargetMemory->GetClass() != SourceMemory->GetClass())
			{
				TargetMemory->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				TargetMemory = NewObject<URigVMMemoryStorage>(Outer, SourceMemory->GetClass());
			}

			TargetMemory->CopyFrom(SourceMemory);
		}
		else if(TargetMemory != nullptr)
		{
			TargetMemory->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			TargetMemory = nullptr;
		}
	};

	// we don't need to copy the literals since they are shared
	// between all instances of the VM
	LiteralMemoryStorageObject = Cast<URigVMMemoryStorage>(InVM->GetLiteralMemory()->GetClass()->GetDefaultObject());
	CopyMemoryStorage(WorkMemoryStorageObject, InVM->GetWorkMemory(), this);
	CopyMemoryStorage(DebugMemoryStorageObject, InVM->GetDebugMemory(), this);

	ExternalPropertyPathDescriptions = InVM->ExternalPropertyPathDescriptions;
	ExternalPropertyPaths.Reset();
	
	if(InVM->FunctionNamesPtr == &InVM->FunctionNamesStorage && !bReferenceByteCode)
	{
		FunctionNamesStorage = InVM->FunctionNamesStorage;
		FunctionNamesPtr = &FunctionNamesStorage;
	}
	else
	{
		FunctionNamesPtr = InVM->FunctionNamesPtr;
	}
	
	if(InVM->FunctionsPtr == &InVM->FunctionsStorage && !bReferenceByteCode)
	{
		FunctionsStorage = InVM->FunctionsStorage;
		FunctionsPtr = &FunctionsStorage;
	}
	else
	{
		FunctionsPtr = InVM->FunctionsPtr;
	}
	
	if(InVM->FactoriesPtr == &InVM->FactoriesStorage && !bReferenceByteCode)
	{
		FactoriesStorage = InVM->FactoriesStorage;
		FactoriesPtr = &FactoriesStorage;
	}
	else
	{
		FactoriesPtr = InVM->FactoriesPtr;
	}

	if(InVM->ByteCodePtr == &InVM->ByteCodeStorage && !bReferenceByteCode)
	{
		ByteCodeStorage = InVM->ByteCodeStorage;
		ByteCodePtr = &ByteCodeStorage;
		ByteCodePtr->bByteCodeIsAligned = InVM->ByteCodeStorage.bByteCodeIsAligned;
	}
	else
	{
		ByteCodePtr = InVM->ByteCodePtr;
	}
	
	Instructions = InVM->Instructions;
	Parameters = InVM->Parameters;
	ParametersNameMap = InVM->ParametersNameMap;
	OperandToDebugRegisters = InVM->OperandToDebugRegisters;

	if (bCopyExternalVariables)
	{
		ExternalVariables = InVM->ExternalVariables;
	}
}

int32 URigVM::AddRigVMFunction(UScriptStruct* InRigVMStruct, const FName& InMethodName)
{
	check(InRigVMStruct);
	const FString FunctionKey = FString::Printf(TEXT("F%s::%s"), *InRigVMStruct->GetName(), *InMethodName.ToString());
	return AddRigVMFunction(FunctionKey);
}

int32 URigVM::AddRigVMFunction(const FString& InFunctionName)
{
	const FName FunctionName = *InFunctionName;
	const int32 FunctionIndex = GetFunctionNames().Find(FunctionName);
	if (FunctionIndex != INDEX_NONE)
	{
		return FunctionIndex;
	}

	const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*InFunctionName);
	if (Function == nullptr)
	{
		return INDEX_NONE;
	}

	GetFunctionNames().Add(FunctionName);
	GetFactories().Add(Function->Factory);
	return GetFunctions().Add(Function->FunctionPtr);
}

FString URigVM::GetRigVMFunctionName(int32 InFunctionIndex) const
{
	return GetFunctionNames()[InFunctionIndex].ToString();
}

URigVMMemoryStorage* URigVM::GetMemoryByType(ERigVMMemoryType InMemoryType, bool bCreateIfNeeded)
{
	switch(InMemoryType)
	{
		case ERigVMMemoryType::Literal:
		{
			if(bCreateIfNeeded)
			{
				if(LiteralMemoryStorageObject == nullptr)
				{
					if(UClass* Class = URigVMMemoryStorageGeneratorClass::GetStorageClass(this, InMemoryType))
					{
						// for literals we share the CDO between all VMs
						LiteralMemoryStorageObject = Cast<URigVMMemoryStorage>(Class->GetDefaultObject(true));
					}
					else
					{
						// since literal memory object can be shared across packages, it needs to have the RF_Public flag
						// for example, a control rig instance in a level sequence pacakge can references
						// the literal memory object in the control rig package
						LiteralMemoryStorageObject = NewObject<URigVMMemoryStorage>(this, FName(), RF_Public);
					}
				}
			}
			return LiteralMemoryStorageObject;
		}
		case ERigVMMemoryType::Work:
		{
			if(bCreateIfNeeded)
			{
				if(WorkMemoryStorageObject)
				{
					if(WorkMemoryStorageObject->GetOuter() != this)
					{
						WorkMemoryStorageObject = nullptr;
					}
				}
				if(WorkMemoryStorageObject == nullptr)
				{
					if(UClass* Class = URigVMMemoryStorageGeneratorClass::GetStorageClass(this, InMemoryType))
					{
						WorkMemoryStorageObject = NewObject<URigVMMemoryStorage>(this, Class);
					}
					else
					{
						WorkMemoryStorageObject = NewObject<URigVMMemoryStorage>(this);
					}
				}
			}
			check(WorkMemoryStorageObject->GetOuter() == this);
			return WorkMemoryStorageObject;
		}
		case ERigVMMemoryType::Debug:
		{
			if(bCreateIfNeeded)
			{
				if(DebugMemoryStorageObject)
				{
					if(DebugMemoryStorageObject->GetOuter() != this)
					{
						DebugMemoryStorageObject = nullptr;
					}
				}
				if(DebugMemoryStorageObject == nullptr)
				{
#if WITH_EDITOR
					if(UClass* Class = URigVMMemoryStorageGeneratorClass::GetStorageClass(this, InMemoryType))
					{
						DebugMemoryStorageObject = NewObject<URigVMMemoryStorage>(this, Class);
					}
					else
#endif
					{
						DebugMemoryStorageObject = NewObject<URigVMMemoryStorage>(this);
					}
				}
			}
			check(DebugMemoryStorageObject->GetOuter() == this);
			return DebugMemoryStorageObject;
		}
		default:
		{
			break;
		}
	}
	return nullptr;
}

void URigVM::ClearMemory()
{
	// At one point our memory objects were saved with RF_Public, so to truly clear them, we have to also clear the flags
	// RF_Public will make them stay around as zombie unreferenced objects, and get included in SavePackage and cooking.
	// Clear their flags so they are not included by editor or cook SavePackage calls.

	// we now make sure that only the literal memory object on the CDO is marked as RF_Public
	// and work memory objects are no longer marked as RF_Public
	// We don't do this for packaged builds, though.

#if WITH_EDITOR
	// Running with `-game` will set GIsEditor to nullptr.
	if (GIsEditor)
	{
		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(this, SubObjects);
		for (UObject* SubObject : SubObjects)
		{
			if (URigVMMemoryStorage* MemoryObject = Cast<URigVMMemoryStorage>(SubObject))
			{
				// we don't care about memory type here because
				// 
				// if "this" is not CDO, its subobjects will not include the literal memory and
				// thus only clears the flag for work mem
				// 
				// if "this" is CDO, its subobjects will include the literal memory and this allows
				// us to actually clear the literal memory
				MemoryObject->ClearFlags(RF_Public);
			}
		}
	}
#endif

	LiteralMemoryStorageObject = nullptr;

	if(WorkMemoryStorageObject)
	{
		WorkMemoryStorageObject->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		WorkMemoryStorageObject = nullptr;
	}

	if(DebugMemoryStorageObject)
	{
		DebugMemoryStorageObject->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		DebugMemoryStorageObject = nullptr;
	}

	InvalidateCachedMemory();
}

const FRigVMInstructionArray& URigVM::GetInstructions()
{
	RefreshInstructionsIfRequired();
	return Instructions;
}

bool URigVM::ContainsEntry(const FName& InEntryName) const
{
	const FRigVMByteCode& ByteCode = GetByteCode();
	return ByteCode.FindEntryIndex(InEntryName) != INDEX_NONE;
}

int32 URigVM::FindEntry(const FName& InEntryName) const
{
	const FRigVMByteCode& ByteCode = GetByteCode();
	return ByteCode.FindEntryIndex(InEntryName);
}

const TArray<FName>& URigVM::GetEntryNames() const
{
	EntryNames.Reset();
	
	const FRigVMByteCode& ByteCode = GetByteCode();
	for (int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
	{
		EntryNames.Add(ByteCode.GetEntry(EntryIndex).Name);
	}

	return EntryNames;
}

bool URigVM::CanExecuteEntry(const FName& InEntryName) const
{
	const int32 EntryIndex = FindEntry(InEntryName);
	if(EntryIndex == INDEX_NONE)
	{
		static constexpr TCHAR MissingEntry[] = TEXT("Entry('%s') cannot be found.");
		Context.PublicData.Logf(EMessageSeverity::Error, MissingEntry, *InEntryName.ToString());
		return false;
	}
	
	if(EntriesBeingExecuted.Contains(EntryIndex))
	{
		TArray<FString> EntryNamesBeingExecuted;
		for(const int32 EntryBeingExecuted : EntriesBeingExecuted)
		{
			EntryNamesBeingExecuted.Add(GetEntryNames()[EntryBeingExecuted].ToString());
		}
		EntryNamesBeingExecuted.Add(InEntryName.ToString());

		static constexpr TCHAR RecursiveEntry[] = TEXT("Entry('%s') is being invoked recursively (%s).");
		Context.PublicData.Logf(EMessageSeverity::Error, RecursiveEntry, *InEntryName.ToString(), *FString::Join(EntryNamesBeingExecuted, TEXT(" -> ")));
		return false;
	}

	return true;
}

#if WITH_EDITOR

bool URigVM::ResumeExecution()
{
	HaltedAtBreakpoint.Reset();
	HaltedAtBreakpointHit = INDEX_NONE;
	if (DebugInfo)
	{
		if (const FRigVMBreakpoint& CurrentBreakpoint = DebugInfo->GetCurrentActiveBreakpoint())
		{
			DebugInfo->IncrementBreakpointActivationOnHit(CurrentBreakpoint);
			DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
			return true;
		}
	}

	return false;
}

bool URigVM::ResumeExecution(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName)
{
	ResumeExecution();
	return Execute(Memory, AdditionalArguments, InEntryName);
}

#endif

const TArray<FRigVMParameter>& URigVM::GetParameters() const
{
	return Parameters;
}

FRigVMParameter URigVM::GetParameterByName(const FName& InParameterName)
{
	if (ParametersNameMap.Num() == Parameters.Num())
	{
		const int32* ParameterIndex = ParametersNameMap.Find(InParameterName);
		if (ParameterIndex)
		{
			Parameters[*ParameterIndex].GetScriptStruct();
			return Parameters[*ParameterIndex];
		}
		return FRigVMParameter();
	}

	for (FRigVMParameter& Parameter : Parameters)
	{
		if (Parameter.GetName() == InParameterName)
		{
			Parameter.GetScriptStruct();
			return Parameter;
		}
	}

	return FRigVMParameter();
}

void URigVM::ResolveFunctionsIfRequired()
{
	if (GetFunctions().Num() != GetFunctionNames().Num())
	{
		GetFunctions().Reset();
		GetFunctions().SetNumZeroed(GetFunctionNames().Num());
		GetFactories().Reset();
		GetFactories().SetNumZeroed(GetFunctionNames().Num());

		for (int32 FunctionIndex = 0; FunctionIndex < GetFunctionNames().Num(); FunctionIndex++)
		{
			if(const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*GetFunctionNames()[FunctionIndex].ToString()))
			{
				GetFunctions()[FunctionIndex] = Function->FunctionPtr;
				GetFactories()[FunctionIndex] = Function->Factory;
			}
			else
			{
				// We cannot recover from missing functions. 
				UE_LOG(LogRigVM, Fatal, TEXT("No handler found for function '%s'"), *GetFunctionNames()[FunctionIndex].ToString());
			}
		}
	}
}

void URigVM::RefreshInstructionsIfRequired()
{
	if (GetByteCode().Num() == 0 && Instructions.Num() > 0)
	{
		Instructions.Reset();
	}
	else if (Instructions.Num() == 0)
	{
		Instructions = GetByteCode().GetInstructions();
	}
}

void URigVM::InvalidateCachedMemory()
{
	CachedMemory.Reset();
	FirstHandleForInstruction.Reset();
	CachedMemoryHandles.Reset();
	ExternalPropertyPaths.Reset();
}

void URigVM::CopyDeferredVMIfRequired()
{
	ensure(ExecutingThreadId == INDEX_NONE);

	URigVM* VMToCopy = nullptr;
	Swap(VMToCopy, DeferredVMToCopy);

	if (VMToCopy)
	{
		CopyFrom(VMToCopy);
	}
}

void URigVM::CacheMemoryHandlesIfRequired(TArrayView<URigVMMemoryStorage*> InMemory)
{
	ensureMsgf(ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::CacheMemoryHandlesIfRequired from multiple threads (%d and %d)"), ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());

	RefreshInstructionsIfRequired();

	if (Instructions.Num() == 0 || InMemory.Num() == 0)
	{
		InvalidateCachedMemory();
		return;
	}

	if ((Instructions.Num() + 1) != FirstHandleForInstruction.Num())
	{
		InvalidateCachedMemory();
	}
	else if (InMemory.Num() != CachedMemory.Num())
	{
		InvalidateCachedMemory();
	}
	else
	{
		for (int32 Index = 0; Index < InMemory.Num(); Index++)
		{
			if (InMemory[Index] != CachedMemory[Index])
			{
				InvalidateCachedMemory();
				break;
			}
		}
	}

	if ((Instructions.Num() + 1) == FirstHandleForInstruction.Num())
	{
		return;
	}

	for (int32 Index = 0; Index < InMemory.Num(); Index++)
	{
		CachedMemory.Add(InMemory[Index]);
	}

	RefreshExternalPropertyPaths();

	FRigVMByteCode& ByteCode = GetByteCode();

	auto InstructionOpEval = [&](
		int32 InstructionIndex,
		int32 InHandleBaseIndex,
		TFunctionRef<void(int32 InHandleIndex, const FRigVMOperand& InArg)> InOpFunc
		) -> void
	{
		const ERigVMOpCode OpCode = Instructions[InstructionIndex].OpCode; 

		if (OpCode >= ERigVMOpCode::Execute_0_Operands && OpCode <= ERigVMOpCode::Execute_64_Operands)
		{
			FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[InstructionIndex]);

			for (const FRigVMOperand& Arg : Operands)
			{
				InOpFunc(InHandleBaseIndex++, Arg);
			}
		}
		else
		{
			switch (OpCode)
			{
			case ERigVMOpCode::Zero:
			case ERigVMOpCode::BoolFalse:
			case ERigVMOpCode::BoolTrue:
			case ERigVMOpCode::Increment:
			case ERigVMOpCode::Decrement:
			case ERigVMOpCode::ArrayReset:
			case ERigVMOpCode::ArrayReverse:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex, Op.Arg);
					break;
				}
			case ERigVMOpCode::Copy:
				{
					const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, Op.Source);
					InOpFunc(InHandleBaseIndex + 1, Op.Target);
					break;
				}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
				{
					const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
					FRigVMOperand Arg = Op.A;
					InOpFunc(InHandleBaseIndex + 0, Arg);
					Arg = Op.B;
					InOpFunc(InHandleBaseIndex + 1, Arg);
					Arg = Op.Result;
					InOpFunc(InHandleBaseIndex + 2, Arg);
					break;
				}
			case ERigVMOpCode::JumpAbsolute:
			case ERigVMOpCode::JumpForward:
			case ERigVMOpCode::JumpBackward:
				{
					break;
				}
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instructions[InstructionIndex]);
					const FRigVMOperand& Arg = Op.Arg;
					InOpFunc(InHandleBaseIndex, Arg);
					break;
				}
			case ERigVMOpCode::ChangeType:
				{
					const FRigVMChangeTypeOp& Op = ByteCode.GetOpAt<FRigVMChangeTypeOp>(Instructions[InstructionIndex]);
					const FRigVMOperand& Arg = Op.Arg;
					InOpFunc(InHandleBaseIndex, Arg);
					break;
				}
			case ERigVMOpCode::Exit:
				{
					break;
				}
			case ERigVMOpCode::BeginBlock:
			case ERigVMOpCode::ArrayGetNum:
			case ERigVMOpCode::ArraySetNum:
			case ERigVMOpCode::ArrayAppend:
			case ERigVMOpCode::ArrayClone:
			case ERigVMOpCode::ArrayRemove:
			case ERigVMOpCode::ArrayUnion:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, Op.ArgA);
					InOpFunc(InHandleBaseIndex + 1, Op.ArgB);
					break;
				}
			case ERigVMOpCode::ArrayAdd:
			case ERigVMOpCode::ArrayGetAtIndex:
			case ERigVMOpCode::ArraySetAtIndex:
			case ERigVMOpCode::ArrayInsert:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, Op.ArgA);
					InOpFunc(InHandleBaseIndex + 1, Op.ArgB);
					InOpFunc(InHandleBaseIndex + 2, Op.ArgC);
					break;
				}
			case ERigVMOpCode::ArrayFind:
				{
					const FRigVMQuaternaryOp& Op = ByteCode.GetOpAt<FRigVMQuaternaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, Op.ArgA);
					InOpFunc(InHandleBaseIndex + 1, Op.ArgB);
					InOpFunc(InHandleBaseIndex + 2, Op.ArgC);
					InOpFunc(InHandleBaseIndex + 3, Op.ArgD);
					break;
				}
			case ERigVMOpCode::ArrayIterator:
				{
					const FRigVMSenaryOp& Op = ByteCode.GetOpAt<FRigVMSenaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, Op.ArgA);
					InOpFunc(InHandleBaseIndex + 1, Op.ArgB);
					InOpFunc(InHandleBaseIndex + 2, Op.ArgC);
					InOpFunc(InHandleBaseIndex + 3, Op.ArgD);
					InOpFunc(InHandleBaseIndex + 4, Op.ArgE);
					InOpFunc(InHandleBaseIndex + 5, Op.ArgF);
					break;
				}
			case ERigVMOpCode::EndBlock:
				{
					break;
				}
			case ERigVMOpCode::InvokeEntry:
				{
					break;
				}
			case ERigVMOpCode::Invalid:
			default:
				{
					checkNoEntry();
					break;
				}
			}
		}
	};

	// Make sure we have enough room to prevent repeated allocations.
	FirstHandleForInstruction.Reset(Instructions.Num() + 1);

	// Count how many handles we need and set up the indirection offsets for the handles.
	int32 HandleCount = 0;
	FirstHandleForInstruction.Add(0);
	for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		InstructionOpEval(InstructionIndex, INDEX_NONE, [&HandleCount](int32, const FRigVMOperand& ){ HandleCount++; });
		FirstHandleForInstruction.Add(HandleCount);
	}
	
	// Allocate all the space and zero it out to ensure all pages required for it are paged in
	// immediately.
	CachedMemoryHandles.SetNumUninitialized(HandleCount);

	// Prefetch the memory types to ensure they exist.
	for (int32 MemoryType = 0; MemoryType < int32(ERigVMMemoryType::Invalid); MemoryType++)
	{
		GetMemoryByType(ERigVMMemoryType(MemoryType), /* bCreateIfNeeded = */true);
	}
	

	// Now cache the handles as needed.
	ParallelFor(Instructions.Num(),
		[&](int32 InstructionIndex)
		{
			InstructionOpEval(InstructionIndex, FirstHandleForInstruction[InstructionIndex], [&](int32 InHandleIndex, const FRigVMOperand& InOp){ CacheSingleMemoryHandle(InHandleIndex, InOp); });
		}
	);

}

void URigVM::RebuildByteCodeOnLoad()
{
	Instructions = GetByteCode().GetInstructions();
	for(int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
		switch(Instruction.OpCode)
		{
			case ERigVMOpCode::Copy:
			{
				FRigVMCopyOp OldCopyOp = GetByteCode().GetOpAt<FRigVMCopyOp>(Instruction);
				if((OldCopyOp.Source.GetMemoryType() == ERigVMMemoryType::External) ||
					(OldCopyOp.Target.GetMemoryType() == ERigVMMemoryType::External))
				{
					if(ExternalVariables.IsEmpty())
					{
						break;
					}
				}
					
				// create a local copy of the original op
				FRigVMCopyOp& NewCopyOp = GetByteCode().GetOpAt<FRigVMCopyOp>(Instruction);
				NewCopyOp = GetCopyOpForOperands(OldCopyOp.Source, OldCopyOp.Target);
				check(OldCopyOp.Source == NewCopyOp.Source);
				check(OldCopyOp.Target == NewCopyOp.Target);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

#if WITH_EDITOR
bool URigVM::ShouldHaltAtInstruction(const FName& InEventName, const uint16 InstructionIndex)
{
	if(DebugInfo == nullptr)
	{
		return false;
	}

	if(DebugInfo->IsEmpty())
	{
		return false;
	}
	
	FRigVMByteCode& ByteCode = GetByteCode();

	TArray<FRigVMBreakpoint> BreakpointsAtInstruction = DebugInfo->FindBreakpointsAtInstruction(InstructionIndex);
	for (FRigVMBreakpoint Breakpoint : BreakpointsAtInstruction)
	{
		if (DebugInfo->IsActive(Breakpoint))
		{
			switch (CurrentBreakpointAction)
			{
				case ERigVMBreakpointAction::None:
				{
					// Halted at breakpoint. Check if this is a new breakpoint different from the previous halt.
					if (HaltedAtBreakpoint != Breakpoint ||
						HaltedAtBreakpointHit != DebugInfo->GetBreakpointHits(Breakpoint))
					{
						HaltedAtBreakpoint = Breakpoint;
						HaltedAtBreakpointHit = DebugInfo->GetBreakpointHits(Breakpoint);
						DebugInfo->SetCurrentActiveBreakpoint(Breakpoint);
						
						// We want to keep the callstack up to the node that produced the halt
						const TArray<UObject*>* FullCallstack = ByteCode.GetCallstackForInstruction(Context.PublicData.InstructionIndex);
						if (FullCallstack)
						{
							DebugInfo->SetCurrentActiveBreakpointCallstack(TArray<UObject*>(FullCallstack->GetData(), FullCallstack->Find((UObject*)Breakpoint.Subject)+1));
						}
						ExecutionHalted().Broadcast(Context.PublicData.InstructionIndex, Breakpoint.Subject, InEventName);
					}
					return true;
				}
				case ERigVMBreakpointAction::Resume:
				{
					CurrentBreakpointAction = ERigVMBreakpointAction::None;

					if (DebugInfo->IsTemporaryBreakpoint(Breakpoint))
					{
						DebugInfo->RemoveBreakpoint(Breakpoint);
					}
					else
					{
						DebugInfo->IncrementBreakpointActivationOnHit(Breakpoint);
						DebugInfo->HitBreakpoint(Breakpoint);
					}
					break;
				}
				case ERigVMBreakpointAction::StepOver:
				case ERigVMBreakpointAction::StepInto:
				case ERigVMBreakpointAction::StepOut:
				{
					// If we are stepping, check if we were halted at the current instruction, and remember it 
					if (!DebugInfo->GetCurrentActiveBreakpoint())
					{
						DebugInfo->SetCurrentActiveBreakpoint(Breakpoint);
						const TArray<UObject*>* FullCallstack = ByteCode.GetCallstackForInstruction(Context.PublicData.InstructionIndex);
						
						// We want to keep the callstack up to the node that produced the halt
						if (FullCallstack)
						{
							DebugInfo->SetCurrentActiveBreakpointCallstack(TArray<UObject*>(FullCallstack->GetData(), FullCallstack->Find((UObject*)DebugInfo->GetCurrentActiveBreakpoint().Subject)+1));
						}
					}							
					
					break;	
				}
				default:
				{
					ensure(false);
					break;
				}
			}
		}
		else
		{
			DebugInfo->HitBreakpoint(Breakpoint);
		}
	}

	// If we are stepping, and the last active breakpoint was set, check if this is the new temporary breakpoint
	if (CurrentBreakpointAction != ERigVMBreakpointAction::None && DebugInfo->GetCurrentActiveBreakpoint())
	{
		const TArray<UObject*>* CurrentCallstack = ByteCode.GetCallstackForInstruction(Context.PublicData.InstructionIndex);
		if (CurrentCallstack && !CurrentCallstack->IsEmpty())
		{
			UObject* NewBreakpointNode = nullptr;

			// Find the first difference in the callstack
			int32 DifferenceIndex = INDEX_NONE;
			TArray<UObject*>& PreviousCallstack = DebugInfo->GetCurrentActiveBreakpointCallstack();
			for (int32 i=0; i<PreviousCallstack.Num(); ++i)
			{
				if (CurrentCallstack->Num() == i)
				{
					DifferenceIndex = i-1;
					break;
				}
				if (PreviousCallstack[i] != CurrentCallstack->operator[](i))
				{
					DifferenceIndex = i;
					break;
				}
			}

			if (CurrentBreakpointAction == ERigVMBreakpointAction::StepOver)
			{
				if (DifferenceIndex != INDEX_NONE)
				{
					NewBreakpointNode = CurrentCallstack->operator[](DifferenceIndex);
				}
			}
			else if (CurrentBreakpointAction == ERigVMBreakpointAction::StepInto)
			{
				if (DifferenceIndex == INDEX_NONE)
				{
					if (!CurrentCallstack->IsEmpty() && !PreviousCallstack.IsEmpty() && CurrentCallstack->Last() != PreviousCallstack.Last())
					{
						NewBreakpointNode = CurrentCallstack->operator[](FMath::Min(PreviousCallstack.Num(), CurrentCallstack->Num()-1));
					}
				}
				else
				{
					NewBreakpointNode = CurrentCallstack->operator[](DifferenceIndex);
				}
			}
			else if (CurrentBreakpointAction == ERigVMBreakpointAction::StepOut)
			{
				if (DifferenceIndex != INDEX_NONE && DifferenceIndex <= PreviousCallstack.Num() - 2)
                {
                	NewBreakpointNode = CurrentCallstack->operator[](DifferenceIndex);
                }
			}
			
			if (NewBreakpointNode)
			{
				// Remove or hit previous breakpoint
				if (DebugInfo->IsTemporaryBreakpoint(DebugInfo->GetCurrentActiveBreakpoint()))
				{
					DebugInfo->RemoveBreakpoint(DebugInfo->GetCurrentActiveBreakpoint());
				}
				else
				{
					DebugInfo->IncrementBreakpointActivationOnHit(DebugInfo->GetCurrentActiveBreakpoint());
					DebugInfo->HitBreakpoint(DebugInfo->GetCurrentActiveBreakpoint());
				}

				// Create new temporary breakpoint
				const FRigVMBreakpoint& NewBreakpoint = DebugInfo->AddBreakpoint(Context.PublicData.InstructionIndex, NewBreakpointNode, 0, true);
				DebugInfo->SetBreakpointHits(NewBreakpoint, GetInstructionVisitedCount(Context.PublicData.InstructionIndex));
				DebugInfo->SetBreakpointActivationOnHit(NewBreakpoint, GetInstructionVisitedCount(Context.PublicData.InstructionIndex));
				CurrentBreakpointAction = ERigVMBreakpointAction::None;					

				HaltedAtBreakpoint = NewBreakpoint;
				HaltedAtBreakpointHit = DebugInfo->GetBreakpointHits(HaltedAtBreakpoint);
				ExecutionHalted().Broadcast(Context.PublicData.InstructionIndex, NewBreakpointNode, InEventName);
		
				return true;
			}
		}
	}

	return false;
}
#endif

bool URigVM::Initialize(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, bool bInitializeMemory)
{
	if (ExecutingThreadId != INDEX_NONE)
	{
		ensureMsgf(ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::Initialize from multiple threads (%d and %d)"), ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());
	}
	CopyDeferredVMIfRequired();
	TGuardValue<int32> GuardThreadId(ExecutingThreadId, FPlatformTLS::GetCurrentThreadId());

	ResolveFunctionsIfRequired();
	RefreshInstructionsIfRequired();

	if (Instructions.Num() == 0)
	{
		return true;
	}

	TArray<URigVMMemoryStorage*> LocalMemory;
	if (Memory.Num() == 0)
	{
		LocalMemory = GetLocalMemoryArray();
		Memory = LocalMemory;
	}

	CacheMemoryHandlesIfRequired(Memory);

	if(!bInitializeMemory)
	{
		return true;
	}
	
	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<FRigVMFunctionPtr>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();
#endif

	Context.Reset();
	Context.SliceOffsets.AddZeroed(Instructions.Num());
	Context.OpaqueArguments = AdditionalArguments;

	TGuardValue<URigVM*> VMInContext(Context.VM, this);
	
	while (Instructions.IsValidIndex(Context.PublicData.InstructionIndex))
	{
		const FRigVMInstruction& Instruction = Instructions[Context.PublicData.InstructionIndex];

		switch (Instruction.OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
				int32 OperandCount = FirstHandleForInstruction[Context.PublicData.InstructionIndex + 1] - FirstHandleForInstruction[Context.PublicData.InstructionIndex];
				FRigVMMemoryHandleArray OpHandles(&CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]], OperandCount);
#if WITH_EDITOR
				Context.PublicData.FunctionName = FunctionNames[Op.FunctionIndex];
#endif
				Context.Factory = Factories[Op.FunctionIndex];

				// find out the largest slice count
				int32 MaxSliceCount = 1;

				// todo Deal with slice counts

				Context.BeginSlice(MaxSliceCount);
				for (int32 SliceIndex = 0; SliceIndex < MaxSliceCount; SliceIndex++)
				{
					(*Functions[Op.FunctionIndex])(Context, OpHandles);
					Context.IncrementSlice();
				}
				Context.EndSlice();

				break;
			}
			case ERigVMOpCode::Zero:
			case ERigVMOpCode::BoolFalse:
			case ERigVMOpCode::BoolTrue:
			{
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);

				FRigVMMemoryHandle& SourceHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				FRigVMMemoryHandle& TargetHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				URigVMMemoryStorage::CopyProperty(TargetHandle, SourceHandle);
				break;
			}
			case ERigVMOpCode::Increment:
			case ERigVMOpCode::Decrement:
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			case ERigVMOpCode::JumpAbsolute:
			case ERigVMOpCode::JumpForward:
			case ERigVMOpCode::JumpBackward:
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
			case ERigVMOpCode::ChangeType:
			case ERigVMOpCode::BeginBlock:
			case ERigVMOpCode::EndBlock:
			case ERigVMOpCode::Exit:
			case ERigVMOpCode::ArrayGetNum:
			case ERigVMOpCode::ArraySetNum:
			case ERigVMOpCode::ArrayAppend:
			case ERigVMOpCode::ArrayClone:
			case ERigVMOpCode::ArrayGetAtIndex:
			case ERigVMOpCode::ArraySetAtIndex:
			case ERigVMOpCode::ArrayInsert:
			case ERigVMOpCode::ArrayRemove:
			case ERigVMOpCode::ArrayAdd:
			case ERigVMOpCode::ArrayFind:
			case ERigVMOpCode::ArrayIterator:
			case ERigVMOpCode::ArrayUnion:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
			case ERigVMOpCode::ArrayReverse:
			case ERigVMOpCode::InvokeEntry:
			{
				break;
			}
			case ERigVMOpCode::Invalid:
			{
				ensure(false);
				return false;
			}
		}
		Context.PublicData.InstructionIndex++;
	}
	
	return true;
}

bool URigVM::Execute(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName)
{
	// if this the first entry being executed - get ready for execution
	const bool bIsRootEntry = EntriesBeingExecuted.IsEmpty(); 
	if(bIsRootEntry)
	{
		if (ExecutingThreadId != INDEX_NONE)
		{
			ensureMsgf(ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::Execute from multiple threads (%d and %d)"), ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());
		}
		CopyDeferredVMIfRequired();
		TGuardValue<int32> GuardThreadId(ExecutingThreadId, FPlatformTLS::GetCurrentThreadId());

		ResolveFunctionsIfRequired();
		RefreshInstructionsIfRequired();

		if (Instructions.Num() == 0)
		{
			return true;
		}

		// changes to the layout of memory array should be reflected in GetContainerIndex()
		TArray<URigVMMemoryStorage*> LocalMemory;
		if (Memory.Num() == 0)
		{
			LocalMemory = GetLocalMemoryArray();
			Memory = LocalMemory;
		}
	
		CacheMemoryHandlesIfRequired(Memory);
	}
	
	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<FRigVMFunctionPtr>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();

	if(bIsRootEntry)
	{
		if (FirstEntryEventInQueue == NAME_None || FirstEntryEventInQueue == InEntryName)
		{
			InstructionVisitedDuringLastRun.Reset();
			InstructionVisitOrder.Reset();
			InstructionVisitedDuringLastRun.SetNumZeroed(Instructions.Num());
			InstructionCyclesDuringLastRun.Reset();
			if(Context.PublicData.RuntimeSettings.bEnableProfiling)
			{
				InstructionCyclesDuringLastRun.SetNumUninitialized(Instructions.Num());
				for(int32 DurationIndex=0;DurationIndex<InstructionCyclesDuringLastRun.Num();DurationIndex++)
				{
					InstructionCyclesDuringLastRun[DurationIndex] = UINT64_MAX;
				}
			}
		}
	}
#endif

	if(bIsRootEntry)
	{
		Context.Reset();
		Context.SliceOffsets.AddZeroed(Instructions.Num());
		Context.OpaqueArguments = AdditionalArguments;
	}

	TGuardValue<URigVM*> VMInContext(Context.VM, this);

	if(bIsRootEntry)
	{
		ClearDebugMemory();
	}

	int32 EntryIndexToPush = INDEX_NONE;
	if (!InEntryName.IsNone())
	{
		int32 EntryIndex = ByteCode.FindEntryIndex(InEntryName);
		if (EntryIndex == INDEX_NONE)
		{
			return false;
		}
		
		SetInstructionIndex((uint16)ByteCode.GetEntry(EntryIndex).InstructionIndex);
		EntryIndexToPush = EntryIndex;

		if(bIsRootEntry)
		{
			Context.PublicData.EventName = InEntryName;
		}
	}
	else
	{
		for(int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
		{
			if(ByteCode.GetEntry(EntryIndex).InstructionIndex == 0)
			{
				EntryIndexToPush = EntryIndex;
				break;
			}
		}
		
		if(bIsRootEntry)
		{
			if(ByteCode.Entries.IsValidIndex(EntryIndexToPush))
			{
				Context.PublicData.EventName = ByteCode.GetEntry(EntryIndexToPush).Name;
			}
			else
			{
				Context.PublicData.EventName = NAME_None;
			}
		}
	}

	FEntryExecuteGuard EntryExecuteGuard(EntriesBeingExecuted, EntryIndexToPush);

#if WITH_EDITOR
	if (DebugInfo && bIsRootEntry)
	{
		DebugInfo->StartExecution();
	}
#endif

	if(bIsRootEntry)
	{
		NumExecutions++;
	}

#if WITH_EDITOR
	uint64 StartCycles = 0;
	uint64 OverallCycles = 0;
	if(Context.PublicData.RuntimeSettings.bEnableProfiling)
	{
		StartCycles = FPlatformTime::Cycles64();
	}
	
#if UE_RIGVM_DEBUG_EXECUTION
	FString DebugMemoryString;
	TArray<FString> PreviousWorkMemory;
	UEnum* InstanceOpCodeEnum = StaticEnum<ERigVMOpCode>();
	URigVMMemoryStorage* LiteralMemory = GetLiteralMemory(false);
	DebugMemoryString = FString("\n\nLiteral Memory\n\n");
	for (int32 PropertyIndex=0; PropertyIndex<LiteralMemory->Num(); ++PropertyIndex)
	{
		DebugMemoryString += FString::Printf(TEXT("%s: %s\n"), *LiteralMemory->GetProperties()[PropertyIndex]->GetFullName(), *LiteralMemory->GetDataAsString(PropertyIndex));				
	}
	DebugMemoryString += FString(TEXT("\n\nWork Memory\n\n"));
	
#endif
	
#endif

	while (Instructions.IsValidIndex(Context.PublicData.InstructionIndex))
	{
#if WITH_EDITOR
		if (ShouldHaltAtInstruction(InEntryName, Context.PublicData.InstructionIndex))
		{
#if UE_RIGVM_DEBUG_EXECUTION
			Context.PublicData.Log(EMessageSeverity::Info, DebugMemoryString);					
#endif
			// we'll recursively exit all invoked
			// entries here.
			return true;
		}

		const int32 CurrentInstructionIndex = Context.PublicData.InstructionIndex;
		InstructionVisitedDuringLastRun[Context.PublicData.InstructionIndex]++;
		InstructionVisitOrder.Add(Context.PublicData.InstructionIndex);
	
#endif

		const FRigVMInstruction& Instruction = Instructions[Context.PublicData.InstructionIndex];

#if WITH_EDITOR
#if UE_RIGVM_DEBUG_EXECUTION
		if (Instruction.OpCode >= ERigVMOpCode::Execute_0_Operands && Instruction.OpCode <= ERigVMOpCode::Execute_64_Operands)
		{
			const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
			FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[Context.PublicData.InstructionIndex]);

			TArray<FString> Labels;
			for (const FRigVMOperand& Operand : Operands)
			{
				Labels.Add(GetOperandLabel(Operand));
			}

			DebugMemoryString += FString::Printf(TEXT("Instruction %d: %s(%s)\n"), Context.PublicData.InstructionIndex, *FunctionNames[Op.FunctionIndex].ToString(), *FString::Join(Labels, TEXT(", ")));				
		}
		else if(Instruction.OpCode == ERigVMOpCode::Copy)
		{
			const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);
			DebugMemoryString += FString::Printf(TEXT("Instruction %d: Copy %s -> %s\n"), Context.PublicData.InstructionIndex, *GetOperandLabel(Op.Source), *GetOperandLabel(Op.Target));				
		}
		else
		{
			DebugMemoryString += FString::Printf(TEXT("Instruction %d: %s\n"), Context.PublicData.InstructionIndex, *InstanceOpCodeEnum->GetNameByIndex((uint8)Instruction.OpCode).ToString());				
		}
		
#endif
#endif

		switch (Instruction.OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
				const int32 OperandCount = FirstHandleForInstruction[Context.PublicData.InstructionIndex + 1] - FirstHandleForInstruction[Context.PublicData.InstructionIndex];
				FRigVMMemoryHandleArray Handles(&CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]], OperandCount);
#if WITH_EDITOR
				Context.PublicData.FunctionName = FunctionNames[Op.FunctionIndex];
#endif
				Context.Factory = Factories[Op.FunctionIndex];
				(*Functions[Op.FunctionIndex])(Context, Handles);

#if WITH_EDITOR

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instruction);
					for(int32 OperandIndex = 0, HandleIndex = 0; OperandIndex < Operands.Num() && HandleIndex < Handles.Num(); HandleIndex++)
					{
						CopyOperandForDebuggingIfNeeded(Operands[OperandIndex++], Handles[HandleIndex]);
					}
				}
#endif

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Zero:
			{
				*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData()) = 0;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]);
				}
#endif

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::BoolFalse:
			{
				*((bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData()) = false;
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::BoolTrue:
			{
				*((bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData()) = true;
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);

				FRigVMMemoryHandle& SourceHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				FRigVMMemoryHandle& TargetHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				URigVMMemoryStorage::CopyProperty(TargetHandle, SourceHandle);
					
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					CopyOperandForDebuggingIfNeeded(Op.Source, SourceHandle);
				}
#endif
					
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Increment:
			{
				(*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData()))++;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]);
				}
#endif
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Decrement:
			{
				(*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData()))--;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]);
				}
#endif
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instruction);

				FRigVMMemoryHandle& HandleA = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				FRigVMMemoryHandle& HandleB = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				const bool Result = HandleA.GetProperty()->Identical(HandleA.GetData(true), HandleB.GetData(true));

				*((bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]+2].GetData()) = Result;
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::JumpAbsolute:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instruction);
				Context.PublicData.InstructionIndex = Op.InstructionIndex;
				break;
			}
			case ERigVMOpCode::JumpForward:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instruction);
				Context.PublicData.InstructionIndex += Op.InstructionIndex;
				break;
			}
			case ERigVMOpCode::JumpBackward:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instruction);
				Context.PublicData.InstructionIndex -= Op.InstructionIndex;
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instruction);
				const bool Condition = *(bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData();
				if (Condition == Op.Condition)
				{
					Context.PublicData.InstructionIndex = Op.InstructionIndex;
				}
				else
				{
					Context.PublicData.InstructionIndex++;
				}
				break;
			}
			case ERigVMOpCode::JumpForwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instruction);
				const bool Condition = *(bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData();
				if (Condition == Op.Condition)
				{
					Context.PublicData.InstructionIndex += Op.InstructionIndex;
				}
				else
				{
					Context.PublicData.InstructionIndex++;
				}
				break;
			}
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instruction);
				const bool Condition = *(bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData();
				if (Condition == Op.Condition)
				{
					Context.PublicData.InstructionIndex -= Op.InstructionIndex;
				}
				else
				{
					Context.PublicData.InstructionIndex++;
				}
				break;
			}
			case ERigVMOpCode::ChangeType:
			{
				ensureMsgf(false, TEXT("not implemented."));
				break;
			}
			case ERigVMOpCode::Exit:
			{
				if(bIsRootEntry)
				{
#if WITH_EDITOR
					Context.LastExecutionMicroSeconds = OverallCycles * FPlatformTime::GetSecondsPerCycle() * 1000.0 * 1000.0;
#endif
					ExecutionReachedExit().Broadcast(InEntryName);
#if WITH_EDITOR					
					if (HaltedAtBreakpoint.IsValid())
					{
						HaltedAtBreakpoint.Reset();
						DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
						ExecutionHalted().Broadcast(INDEX_NONE, nullptr, InEntryName);
					}
#if UE_RIGVM_DEBUG_EXECUTION
					Context.PublicData.Log(EMessageSeverity::Info, DebugMemoryString);					
#endif
#endif
				}
				return true;
			}
			case ERigVMOpCode::BeginBlock:
			{
				const int32 Count = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]].GetData()));
				const int32 Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));
				Context.BeginSlice(Count, Index);
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::EndBlock:
			{
				Context.EndSlice();
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayReset:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FScriptArrayHelper ArrayHelper(CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty()), ArrayHandle.GetData());
				ArrayHelper.Resize(0);

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, ArrayHandle);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayGetNum:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FScriptArrayHelper ArrayHelper(CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty()), ArrayHandle.GetData());
				int32& Count = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));
				Count = ArrayHelper.Num();

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArraySetNum:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FScriptArrayHelper ArrayHelper(CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty()), ArrayHandle.GetData());
				const int32 Count = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));
				if(Context.IsValidArraySize(Count))
				{
					ArrayHelper.Resize(Count);
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayAppend:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FRigVMMemoryHandle& OtherArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty()); 
				const FArrayProperty* OtherArrayProperty = CastFieldChecked<FArrayProperty>(OtherArrayHandle.GetProperty()); 

				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				FScriptArrayHelper OtherArrayHelper(OtherArrayProperty, OtherArrayHandle.GetData());

				if(OtherArrayHelper.Num() > 0)
				{
					if(Context.IsValidArraySize(ArrayHelper.Num() + OtherArrayHelper.Num()))
					{
						const FProperty* TargetProperty = ArrayProperty->Inner;
						const FProperty* SourceProperty = OtherArrayProperty->Inner;

						int32 TargetIndex = ArrayHelper.AddValues(OtherArrayHelper.Num());
						for(int32 SourceIndex = 0; SourceIndex < OtherArrayHelper.Num(); SourceIndex++, TargetIndex++)
						{
							uint8* TargetMemory = ArrayHelper.GetRawPtr(TargetIndex);
							const uint8* SourceMemory = OtherArrayHelper.GetRawPtr(SourceIndex);
							URigVMMemoryStorage::CopyProperty(TargetProperty, TargetMemory, SourceProperty, SourceMemory);
						}
					}
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayClone:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FRigVMMemoryHandle& ClonedArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty()); 
				const FArrayProperty* ClonedArrayProperty = CastFieldChecked<FArrayProperty>(ClonedArrayHandle.GetProperty()); 
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				FScriptArrayHelper ClonedArrayHelper(ClonedArrayProperty, ClonedArrayHandle.GetData());

				CopyArray(ClonedArrayHelper, ClonedArrayHandle, ArrayHelper, ArrayHandle);
					
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, ClonedArrayHandle);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayGetAtIndex:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				int32 Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));

				if(Context.IsValidArrayIndex(Index, ArrayHelper))
				{
					FRigVMMemoryHandle& ElementHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2];
					uint8* TargetMemory = ElementHandle.GetData();
					const uint8* SourceMemory = ArrayHelper.GetRawPtr(Index);
					URigVMMemoryStorage::CopyProperty(ElementHandle.GetProperty(), TargetMemory, ArrayProperty->Inner, SourceMemory);
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
					CopyOperandForDebuggingIfNeeded(Op.ArgC, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArraySetAtIndex:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				int32 Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));
				if(Context.IsValidArrayIndex(Index, ArrayHelper))
				{
					FRigVMMemoryHandle& ElementHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2];
					uint8* TargetMemory = ArrayHelper.GetRawPtr(Index);
					const uint8* SourceMemory = ElementHandle.GetData();
					URigVMMemoryStorage::CopyProperty(ArrayProperty->Inner, TargetMemory, ElementHandle.GetProperty(), SourceMemory);
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
					CopyOperandForDebuggingIfNeeded(Op.ArgC, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayInsert:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				if(Context.IsValidArraySize(ArrayHelper.Num() + 1))
				{
					int32 Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));

					// we support wrapping the index around similar to python
					if(Index < 0)
					{
						Index = ArrayHelper.Num() + Index;
					}
					
					Index = FMath::Clamp<int32>(Index, 0, ArrayHelper.Num());
					ArrayHelper.InsertValues(Index, 1);

					FRigVMMemoryHandle& ElementHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2];
					uint8* TargetMemory = ArrayHelper.GetRawPtr(Index);
					const uint8* SourceMemory = ElementHandle.GetData();
					URigVMMemoryStorage::CopyProperty(ArrayProperty->Inner, TargetMemory, ElementHandle.GetProperty(), SourceMemory);
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
					CopyOperandForDebuggingIfNeeded(Op.ArgC, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayRemove:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				int32 Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1].GetData()));
				if(Context.IsValidArrayIndex(Index, ArrayHelper))
				{
					ArrayHelper.RemoveValues(Index, 1);
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());
				int32& Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2].GetData()));
				if(Context.IsValidArraySize(ArrayHelper.Num() + 1))
				{
					FRigVMMemoryHandle& ElementHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
					Index = ArrayHelper.AddValue();

					uint8* TargetMemory = ArrayHelper.GetRawPtr(Index);
					const uint8* SourceMemory = ElementHandle.GetData();
					URigVMMemoryStorage::CopyProperty(ArrayProperty->Inner, TargetMemory, ElementHandle.GetProperty(), SourceMemory);
				}
				else
				{
					Index = INDEX_NONE;
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
					CopyOperandForDebuggingIfNeeded(Op.ArgC, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());

				FRigVMMemoryHandle& ElementHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				int32& FoundIndex = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2].GetData()));
				bool& bFound = (*((bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 3].GetData()));

				FoundIndex = INDEX_NONE;
				bFound = false;

				const FProperty* PropertyA = ElementHandle.GetProperty();
				const FProperty* PropertyB = ArrayProperty->Inner;

				if(PropertyA->SameType(PropertyB))
				{
					const uint8* MemoryA = ElementHandle.GetData();

					for(int32 Index = 0; Index < ArrayHelper.Num(); Index++)
					{
						const uint8* MemoryB = ArrayHelper.GetRawPtr(Index);
						if(PropertyA->Identical(MemoryA, MemoryB))
						{
							FoundIndex = Index;
							bFound = true;
							break;
						}
					}
				}
				else
				{
					static const TCHAR IncompatibleTypes[] = TEXT("Array('%s') doesn't support searching for element('%$s').");
					Context.PublicData.Logf(EMessageSeverity::Error, IncompatibleTypes, *PropertyB->GetCPPType(), *PropertyA->GetCPPType());
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMQuaternaryOp& Op = ByteCode.GetOpAt<FRigVMQuaternaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle);
					CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]);
					CopyOperandForDebuggingIfNeeded(Op.ArgC, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]);
					CopyOperandForDebuggingIfNeeded(Op.ArgD, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 3]);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]];
				const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty());
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayHandle.GetData());

				FRigVMMemoryHandle& ElementHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				const int32& Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2].GetData()));
				int32& Count = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 3].GetData()));
				float& Ratio = (*((float*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 4].GetData()));
				bool& bContinue = (*((bool*)CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 5].GetData()));

				Count = ArrayHelper.Num();
				bContinue = Index >=0 && Index < Count;

				if((Count <= 0) || !bContinue)
				{
					Ratio = 0.f;
				}
				else
				{
					Ratio = float(Index) / float(Count - 1);

					uint8* TargetMemory = ElementHandle.GetData();
					const uint8* SourceMemory = ArrayHelper.GetRawPtr(Index);
					URigVMMemoryStorage::CopyProperty(ElementHandle.GetProperty(), TargetMemory, ArrayProperty->Inner, SourceMemory);

					if(DebugMemoryStorageObject->Num() > 0)
					{
						const FRigVMSenaryOp& Op = ByteCode.GetOpAt<FRigVMSenaryOp>(Instruction);
						CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandle); // array
						CopyOperandForDebuggingIfNeeded(Op.ArgD, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 3]); // count

						Context.BeginSlice(Count, Index);
						CopyOperandForDebuggingIfNeeded(Op.ArgB, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1]); // element
						CopyOperandForDebuggingIfNeeded(Op.ArgC, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]); // index
						CopyOperandForDebuggingIfNeeded(Op.ArgE, CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 4]); // ratio
						Context.EndSlice();
					}
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayUnion:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
			{
				FRigVMMemoryHandle& ArrayHandleA = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FRigVMMemoryHandle& ArrayHandleB = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 1];
				const FArrayProperty* ArrayPropertyA = CastFieldChecked<FArrayProperty>(ArrayHandleA.GetProperty()); 					
				const FArrayProperty* ArrayPropertyB = CastFieldChecked<FArrayProperty>(ArrayHandleB.GetProperty()); 					
				FScriptArrayHelper ArrayHelperA(ArrayPropertyA, ArrayHandleA.GetData());
				FScriptArrayHelper ArrayHelperB(ArrayPropertyB, ArrayHandleB.GetData());
				const FProperty* ElementPropertyA = ArrayPropertyA->Inner;
				const FProperty* ElementPropertyB = ArrayPropertyB->Inner;

				TMap<uint32, int32> HashA, HashB;
				HashA.Reserve(ArrayHelperA.Num());
				HashB.Reserve(ArrayHelperB.Num());

				for(int32 Index = 0; Index < ArrayHelperA.Num(); Index++)
				{
					uint32 HashValue;
					if (ElementPropertyA->PropertyFlags & CPF_HasGetValueTypeHash)
					{
						HashValue = ElementPropertyA->GetValueTypeHash(ArrayHelperA.GetRawPtr(Index));
					}
					else
					{
						FString Value;
						ElementPropertyA->ExportTextItem_Direct(Value, ArrayHelperA.GetRawPtr(Index), nullptr, nullptr, PPF_None);
						HashValue = TextKeyUtil::HashString(Value);
					}
					
					if(!HashA.Contains(HashValue))
					{
						HashA.Add(HashValue, Index);
					}
				}
				for(int32 Index = 0; Index < ArrayHelperB.Num(); Index++)
				{
					uint32 HashValue;
					if (ElementPropertyB->PropertyFlags & CPF_HasGetValueTypeHash)
					{
						HashValue = ElementPropertyB->GetValueTypeHash(ArrayHelperB.GetRawPtr(Index));
					}
					else
					{
						FString Value;
						ElementPropertyB->ExportTextItem_Direct(Value, ArrayHelperB.GetRawPtr(Index), nullptr, nullptr, PPF_None);
						HashValue = TextKeyUtil::HashString(Value);
					}
					if(!HashB.Contains(HashValue))
					{
						HashB.Add(HashValue, Index);
					}
				}

				if(Instruction.OpCode == ERigVMOpCode::ArrayUnion)
				{
					// copy the complete array to a temp storage
					TArray<uint8, TAlignedHeapAllocator<16>> TempStorage;
					const int32 NumElementsA = ArrayHelperA.Num();
					TempStorage.AddZeroed(NumElementsA * ElementPropertyA->GetSize());
					uint8* TempMemory = TempStorage.GetData();
					for(int32 Index = 0; Index < NumElementsA; Index++)
					{
						ElementPropertyA->InitializeValue(TempMemory);
						ElementPropertyA->CopyCompleteValue(TempMemory, ArrayHelperA.GetRawPtr(Index));
						TempMemory += ElementPropertyA->GetSize();
					}

					ArrayHelperA.Resize(0);

					for(const TPair<uint32, int32>& Pair : HashA)
					{
						int32 AddedIndex = ArrayHelperA.AddValue();
						TempMemory = TempStorage.GetData() + Pair.Value * ElementPropertyA->GetSize();
						
						URigVMMemoryStorage::CopyProperty(
							ElementPropertyA,
							ArrayHelperA.GetRawPtr(AddedIndex),
							ElementPropertyA,
							TempMemory
						);
					}

					TempMemory = TempStorage.GetData();
					for(int32 Index = 0; Index < NumElementsA; Index++)
					{
						ElementPropertyA->DestroyValue(TempMemory);
						TempMemory += ElementPropertyA->GetSize();
					}

					for(const TPair<uint32, int32>& Pair : HashB)
					{
						if(!HashA.Contains(Pair.Key))
						{
							int32 AddedIndex = ArrayHelperA.AddValue();
							
							URigVMMemoryStorage::CopyProperty(
								ElementPropertyA,
								ArrayHelperA.GetRawPtr(AddedIndex),
								ElementPropertyB,
								ArrayHelperB.GetRawPtr(Pair.Value)
							);
						}
					}
					
					if(DebugMemoryStorageObject->Num() > 0)
					{
						const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instruction);
						CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandleA);
						CopyOperandForDebuggingIfNeeded(Op.ArgB, ArrayHandleB);
					}
				}
				else
				{
					FRigVMMemoryHandle& ResultArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex] + 2]; 					
					FScriptArrayHelper ResultArrayHelper(CastFieldChecked<FArrayProperty>(ResultArrayHandle.GetProperty()), ResultArrayHandle.GetData());
					const FArrayProperty* ResultArrayProperty = CastFieldChecked<FArrayProperty>(ResultArrayHandle.GetProperty());
					const FProperty* ResultElementProperty = ResultArrayProperty->Inner;

					ResultArrayHelper.Resize(0);
					
					if(Instruction.OpCode == ERigVMOpCode::ArrayDifference)
					{
						for(const TPair<uint32, int32>& Pair : HashA)
						{
							if(!HashB.Contains(Pair.Key))
							{
								int32 AddedIndex = ResultArrayHelper.AddValue();
								URigVMMemoryStorage::CopyProperty(
									ResultElementProperty,
									ResultArrayHelper.GetRawPtr(AddedIndex),
									ElementPropertyA,
									ArrayHelperA.GetRawPtr(Pair.Value)
								);
							}
						}
						for(const TPair<uint32, int32>& Pair : HashB)
						{
							if(!HashA.Contains(Pair.Key))
							{
								int32 AddedIndex = ResultArrayHelper.AddValue();
								URigVMMemoryStorage::CopyProperty(
									ResultElementProperty,
									ResultArrayHelper.GetRawPtr(AddedIndex),
									ElementPropertyB,
									ArrayHelperB.GetRawPtr(Pair.Value)
								);
							}
						}
					}
					else // intersection
					{
						for(const TPair<uint32, int32>& Pair : HashA)
						{
							if(HashB.Contains(Pair.Key))
							{
								int32 AddedIndex = ResultArrayHelper.AddValue();
								URigVMMemoryStorage::CopyProperty(
									ResultElementProperty,
									ResultArrayHelper.GetRawPtr(AddedIndex),
									ElementPropertyA,
									ArrayHelperA.GetRawPtr(Pair.Value)
								);
							}
						}
					}

					if(DebugMemoryStorageObject->Num() > 0)
					{
						const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instruction);
						CopyOperandForDebuggingIfNeeded(Op.ArgA, ArrayHandleA);
						CopyOperandForDebuggingIfNeeded(Op.ArgB, ArrayHandleB);
						CopyOperandForDebuggingIfNeeded(Op.ArgC, ResultArrayHandle);
					}
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::ArrayReverse:
			{
				FRigVMMemoryHandle& ArrayHandle = CachedMemoryHandles[FirstHandleForInstruction[Context.PublicData.InstructionIndex]]; 					
				FScriptArrayHelper ArrayHelper(CastFieldChecked<FArrayProperty>(ArrayHandle.GetProperty()), ArrayHandle.GetData());
				for(int32 A=0, B=ArrayHelper.Num()-1; A<B; A++, B--)
				{
					ArrayHelper.SwapValues(A, B);
				}

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, ArrayHandle);
				}

				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::InvokeEntry:
			{
				const FRigVMInvokeEntryOp& Op = ByteCode.GetOpAt<FRigVMInvokeEntryOp>(Instruction);

				if(!CanExecuteEntry(Op.EntryName))
				{
					return false;
				}
				else
				{
					// this will restore the public data after invoking the entry
					TGuardValue<FRigVMExecuteContext> PublicDataGuard(Context.PublicData, Context.PublicData);
					if(!Execute(Memory, AdditionalArguments, Op.EntryName))
					{
						return false;
					}

#if WITH_EDITOR
					// if we are halted at a break point we need to exit here
					if (ShouldHaltAtInstruction(InEntryName, Context.PublicData.InstructionIndex))
					{
						return true;
					}
#endif
				}
					
				Context.PublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Invalid:
			{
				ensure(false);
				return false;
			}
		}

#if WITH_EDITOR
		if(Context.PublicData.RuntimeSettings.bEnableProfiling && !InstructionVisitOrder.IsEmpty())
		{
			const uint64 EndCycles = FPlatformTime::Cycles64();
			const uint64 Cycles = EndCycles - StartCycles;
			if(InstructionCyclesDuringLastRun[CurrentInstructionIndex] == UINT64_MAX)
			{
				InstructionCyclesDuringLastRun[CurrentInstructionIndex] = Cycles;
			}
			else
			{
				InstructionCyclesDuringLastRun[CurrentInstructionIndex] += Cycles;
			}

			StartCycles = EndCycles;
			OverallCycles += Cycles;
		}

#if UE_RIGVM_DEBUG_EXECUTION
		TArray<FString> CurrentWorkMemory;
		URigVMMemoryStorage* WorkMemory = GetWorkMemory(false);
		int32 LineIndex = 0;
		for (int32 PropertyIndex=0; PropertyIndex<WorkMemory->Num(); ++PropertyIndex, ++LineIndex)
		{
			FString Line = FString::Printf(TEXT("%s: %s"), *WorkMemory->GetProperties()[PropertyIndex]->GetFullName(), *WorkMemory->GetDataAsString(PropertyIndex));
			if (PreviousWorkMemory.Num() > 0 && PreviousWorkMemory[PropertyIndex].StartsWith(TEXT(" -- ")))
			{
				PreviousWorkMemory[PropertyIndex].RightChopInline(4);
			}
			if (PreviousWorkMemory.Num() == 0 || Line == PreviousWorkMemory[PropertyIndex])
			{
				CurrentWorkMemory.Add(Line);
			}
			else
			{
				CurrentWorkMemory.Add(FString::Printf(TEXT(" -- %s"), *Line));
			}
		}
		for (const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
		{
			FString Value;
			ExternalVariable.Property->ExportTextItem_Direct(Value, ExternalVariable.Memory, nullptr, nullptr, PPF_None);
			FString Line = FString::Printf(TEXT("External %s: %s"), *ExternalVariable.Name.ToString(), *Value);
			if (PreviousWorkMemory.Num() > 0 && PreviousWorkMemory[LineIndex].StartsWith(TEXT(" -- ")))
			{
				PreviousWorkMemory[LineIndex].RightChopInline(4);
			}
			if (PreviousWorkMemory.Num() == 0 || Line == PreviousWorkMemory[LineIndex])
			{
				CurrentWorkMemory.Add(Line);
			}
			else
			{
				CurrentWorkMemory.Add(FString::Printf(TEXT(" -- %s"), *Line));
			}
			++LineIndex;
		}
		DebugMemoryString += FString::Join(CurrentWorkMemory, TEXT("\n")) + FString(TEXT("\n\n"));
		PreviousWorkMemory = CurrentWorkMemory;		
#endif
#endif
	}

#if WITH_EDITOR
	if(bIsRootEntry)
	{
		if (HaltedAtBreakpoint.IsValid())
		{
			DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
			HaltedAtBreakpoint.Reset();
			ExecutionHalted().Broadcast(INDEX_NONE, nullptr, InEntryName);
		}
	}
#endif

	return true;
}

bool URigVM::Execute(const FName& InEntryName)
{
	return Execute(TArray<URigVMMemoryStorage*>(), TArrayView<void*>(), InEntryName);
}

FRigVMExternalVariable URigVM::GetExternalVariableByName(const FName& InExternalVariableName)
{
	for (const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.Name == InExternalVariableName)
		{
			return ExternalVariable;
		}
	}
	return FRigVMExternalVariable();
}

void URigVM::SetPropertyValueFromString(const FRigVMOperand& InOperand, const FString& InDefaultValue)
{
	URigVMMemoryStorage* Memory = GetMemoryByType(InOperand.GetMemoryType());
	if(Memory == nullptr)
	{
		return;
	}

	Memory->SetDataFromString(InOperand.GetRegisterIndex(), InDefaultValue);
}

#if WITH_EDITOR

TArray<FString> URigVM::DumpByteCodeAsTextArray(const TArray<int32>& InInstructionOrder, bool bIncludeLineNumbers, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> OperandFormatFunction)
{
	RefreshInstructionsIfRequired();
	const FRigVMByteCode& ByteCode = GetByteCode();
	const TArray<FName>& FunctionNames = GetFunctionNames();

	TArray<int32> InstructionOrder;
	InstructionOrder.Append(InInstructionOrder);
	if (InstructionOrder.Num() == 0)
	{
		for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
		{
			InstructionOrder.Add(InstructionIndex);
		}
	}

	TArray<FString> Result;

	for (int32 InstructionIndex : InstructionOrder)
	{
		FString ResultLine;

		switch (Instructions[InstructionIndex].OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instructions[InstructionIndex]);
				FString FunctionName = FunctionNames[Op.FunctionIndex].ToString();
				FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[InstructionIndex]);

				TArray<FString> Labels;
				for (const FRigVMOperand& Operand : Operands)
				{
					Labels.Add(GetOperandLabel(Operand, OperandFormatFunction));
				}

				ResultLine = FString::Printf(TEXT("%s(%s)"), *FunctionName, *FString::Join(Labels, TEXT(",")));
				break;
			}
			case ERigVMOpCode::Zero:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to 0"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::BoolFalse:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to False"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::BoolTrue:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to True"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Increment:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Inc %s ++"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Decrement:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Dec %s --"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Copy %s to %s"), *GetOperandLabel(Op.Source, OperandFormatFunction), *GetOperandLabel(Op.Target, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Equals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to %s == %s "), *GetOperandLabel(Op.Result, OperandFormatFunction), *GetOperandLabel(Op.A, OperandFormatFunction), *GetOperandLabel(Op.B, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to %s != %s"), *GetOperandLabel(Op.Result, OperandFormatFunction), *GetOperandLabel(Op.A, OperandFormatFunction), *GetOperandLabel(Op.B, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::JumpAbsolute:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Jump to instruction %d"), Op.InstructionIndex);
				break;
			}
			case ERigVMOpCode::JumpForward:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Jump %d instructions forwards"), Op.InstructionIndex);
				break;
			}
			case ERigVMOpCode::JumpBackward:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Jump %d instructions backwards"), Op.InstructionIndex);
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instructions[InstructionIndex]);
				if (Op.Condition)
				{
					ResultLine = FString::Printf(TEXT("Jump to instruction %d if %s"), Op.InstructionIndex, *GetOperandLabel(Op.Arg, OperandFormatFunction));
				}
				else
				{
					ResultLine = FString::Printf(TEXT("Jump to instruction %d if !%s"), Op.InstructionIndex, *GetOperandLabel(Op.Arg, OperandFormatFunction));
				}
				break;
			}
			case ERigVMOpCode::JumpForwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instructions[InstructionIndex]);
				if (Op.Condition)
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions forwards if %s"), Op.InstructionIndex, *GetOperandLabel(Op.Arg, OperandFormatFunction));
				}
				else
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions forwards if !%s"), Op.InstructionIndex, *GetOperandLabel(Op.Arg, OperandFormatFunction));
				}
				break;
			}
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instructions[InstructionIndex]);
				if (Op.Condition)
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions backwards if %s"), Op.InstructionIndex, *GetOperandLabel(Op.Arg, OperandFormatFunction));
				}
				else
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions backwards if !%s"), Op.InstructionIndex, *GetOperandLabel(Op.Arg, OperandFormatFunction));
				}
				break;
			}
			case ERigVMOpCode::ChangeType:
			{
				const FRigVMChangeTypeOp& Op = ByteCode.GetOpAt<FRigVMChangeTypeOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Change type of %s"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Exit:
			{
				ResultLine = TEXT("Exit");
				break;
			}
			case ERigVMOpCode::BeginBlock:
			{
				ResultLine = TEXT("Begin Block");
				break;
			}
			case ERigVMOpCode::EndBlock:
			{
				ResultLine = TEXT("End Block");
				break;
			}
			case ERigVMOpCode::ArrayReset:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Reset array %s"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayGetNum:
			{
				const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Get size of array %s and assign to %s"), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			} 
			case ERigVMOpCode::ArraySetNum:
			{
				const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set size of array %s to %s"), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayGetAtIndex:
			{
				const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Get item of array %s at index %s and assign to %s"), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction), *GetOperandLabel(Op.ArgC, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArraySetAtIndex:
			{
				const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set item of array %s at index %s to %s"), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction), *GetOperandLabel(Op.ArgC, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			{
				const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Add element %s to array %s and return index %s"),
					*GetOperandLabel(Op.ArgB, OperandFormatFunction),
					*GetOperandLabel(Op.ArgA, OperandFormatFunction),
					*GetOperandLabel(Op.ArgC, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayInsert:
			{
				const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Insert element %s to array %s at index %s"), *GetOperandLabel(Op.ArgC, OperandFormatFunction), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayRemove:
			{
				const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Remove element at index %s from array %s"), *GetOperandLabel(Op.ArgB, OperandFormatFunction), *GetOperandLabel(Op.ArgA, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				const FRigVMQuaternaryOp& Op = ByteCode.GetOpAt<FRigVMQuaternaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Find element %s in array %s and returns index %s and if element was found %s"),
					*GetOperandLabel(Op.ArgB, OperandFormatFunction),
					*GetOperandLabel(Op.ArgA, OperandFormatFunction),
					*GetOperandLabel(Op.ArgC, OperandFormatFunction),
					*GetOperandLabel(Op.ArgD, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayAppend:
			{
				const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Append array %s to array %s"), *GetOperandLabel(Op.ArgB, OperandFormatFunction), *GetOperandLabel(Op.ArgA, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayClone:
			{
				const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Clone array %s to array %s"), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				const FRigVMSenaryOp& Op = ByteCode.GetOpAt<FRigVMSenaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Iterate over array %s, with current element in %s, current index in %s, array count in %s and current ratio in %s"),
					*GetOperandLabel(Op.ArgA, OperandFormatFunction),
					*GetOperandLabel(Op.ArgB, OperandFormatFunction),
					*GetOperandLabel(Op.ArgC, OperandFormatFunction),
					*GetOperandLabel(Op.ArgD, OperandFormatFunction),
					*GetOperandLabel(Op.ArgE, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayUnion:
			{
				const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Merge array %s and array %s"), *GetOperandLabel(Op.ArgA, OperandFormatFunction), *GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayDifference:
			{
				const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Create array %s from differences of array %s and array %s"),
					*GetOperandLabel(Op.ArgC, OperandFormatFunction),
					*GetOperandLabel(Op.ArgA, OperandFormatFunction),
					*GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayIntersection:
			{
				const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Create array %s from intersection of array %s and array %s"),
					*GetOperandLabel(Op.ArgC, OperandFormatFunction), 
					*GetOperandLabel(Op.ArgA, OperandFormatFunction),
					*GetOperandLabel(Op.ArgB, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::ArrayReverse:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Reverse array %s"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::InvokeEntry:
			{
				const FRigVMInvokeEntryOp& Op = ByteCode.GetOpAt<FRigVMInvokeEntryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Invoke entry %s"), *Op.EntryName.ToString());
				break;
			}
			default:
			{
				ensure(false);
				break;
			}
		}

		if (bIncludeLineNumbers)
		{
			FString ResultIndexStr = FString::FromInt(InstructionIndex);
			while (ResultIndexStr.Len() < 3)
			{
				ResultIndexStr = TEXT("0") + ResultIndexStr;
			}
			Result.Add(FString::Printf(TEXT("%s. %s"), *ResultIndexStr, *ResultLine));
		}
		else
		{
			Result.Add(ResultLine);
		}
	}

	return Result;
}

FString URigVM::DumpByteCodeAsText(const TArray<int32>& InInstructionOrder, bool bIncludeLineNumbers)
{
	return FString::Join(DumpByteCodeAsTextArray(InInstructionOrder, bIncludeLineNumbers), TEXT("\n"));
}

FString URigVM::GetOperandLabel(const FRigVMOperand& InOperand, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> FormatFunction)
{
	FString RegisterName;
	FString RegisterOffsetName;
	if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
	{
		const FRigVMExternalVariable& ExternalVariable = ExternalVariables[InOperand.GetRegisterIndex()];
		RegisterName = FString::Printf(TEXT("Variable::%s"), *ExternalVariable.Name.ToString());
		if (InOperand.GetRegisterOffset() != INDEX_NONE)
		{
			if(ensure(ExternalPropertyPaths.IsValidIndex(InOperand.GetRegisterOffset())))
			{
				RegisterOffsetName = ExternalPropertyPaths[InOperand.GetRegisterOffset()].ToString();
			}
		}
	}
	else
	{
		URigVMMemoryStorage* Memory = GetMemoryByType(InOperand.GetMemoryType());
		if(Memory == nullptr)
		{
			return FString();
		}

		check(Memory->IsValidIndex(InOperand.GetRegisterIndex()));
		
		RegisterName = Memory->GetProperties()[InOperand.GetRegisterIndex()]->GetName();
		RegisterOffsetName =
			InOperand.GetRegisterOffset() != INDEX_NONE ?
			Memory->GetPropertyPaths()[InOperand.GetRegisterOffset()].ToString() :
			FString();
	}
	
	FString OperandLabel = RegisterName;
	
	// caller can provide an alternative format to override the default format(optional)
	if (FormatFunction)
	{
		OperandLabel = FormatFunction(RegisterName, RegisterOffsetName);
	}

	return OperandLabel;
}

#endif

void URigVM::ClearDebugMemory()
{
#if WITH_EDITOR
	for(int32 PropertyIndex = 0; PropertyIndex < GetDebugMemory()->Num(); PropertyIndex++)
	{
		if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(GetDebugMemory()->GetProperties()[PropertyIndex]))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, GetDebugMemory()->GetData<uint8>(PropertyIndex));
			ArrayHelper.EmptyValues();
		}
	}
#endif
}

void URigVM::CacheSingleMemoryHandle(int32 InHandleIndex, const FRigVMOperand& InArg, bool bForExecute)
{
	URigVMMemoryStorage* Memory = GetMemoryByType(InArg.GetMemoryType(), false);

	if (InArg.GetMemoryType() == ERigVMMemoryType::External)
	{
		const FRigVMPropertyPath* PropertyPath = nullptr;
		if(InArg.GetRegisterOffset() != INDEX_NONE)
		{
			check(ExternalPropertyPaths.IsValidIndex(InArg.GetRegisterOffset()));
			PropertyPath = &ExternalPropertyPaths[InArg.GetRegisterOffset()];
		}

		check(ExternalVariables.IsValidIndex(InArg.GetRegisterIndex()));

		FRigVMExternalVariable& ExternalVariable = ExternalVariables[InArg.GetRegisterIndex()];
		check(ExternalVariable.IsValid(false));

		CachedMemoryHandles[InHandleIndex] = {ExternalVariable.Memory, ExternalVariable.Property, PropertyPath};
#if UE_BUILD_DEBUG
		// make sure the handle points to valid memory
		check(CachedMemoryHandles[InHandleIndex].GetData(false) != nullptr);
#endif
		return;
	}

	const FRigVMPropertyPath* PropertyPath = nullptr;
	if(InArg.GetRegisterOffset() != INDEX_NONE)
	{
		check(Memory->GetPropertyPaths().IsValidIndex(InArg.GetRegisterOffset()));
		PropertyPath = &Memory->GetPropertyPaths()[InArg.GetRegisterOffset()];
	}

	// if you are hitting this it's likely that the VM was created outside of a valid
	// package. the compiler bases the memory class construction on the package the VM
	// is in - so a VM under GetTransientPackage() can be created - but not run.
	uint8* Data = Memory->GetData<uint8>(InArg.GetRegisterIndex());
	const FProperty* Property = Memory->GetProperties()[InArg.GetRegisterIndex()];
	CachedMemoryHandles[InHandleIndex] = {Data, Property, PropertyPath};

#if UE_BUILD_DEBUG
	// make sure the handle points to valid memory
	FRigVMMemoryHandle& Handle = CachedMemoryHandles[InHandleIndex]; 
	check(Handle.GetData(false) != nullptr);
	if(PropertyPath)
	{
		uint8* MemoryPtr = Handle.GetData(false);
		// don't check the result - since it may be an array element
		// that doesn't exist yet. 
		PropertyPath->GetData<uint8>(MemoryPtr, Property);
	}
#endif
}

void URigVM::CopyOperandForDebuggingImpl(const FRigVMOperand& InArg, const FRigVMMemoryHandle& InHandle, const FRigVMOperand& InDebugOperand)
{
#if WITH_EDITOR

	URigVMMemoryStorage* TargetMemory = GetDebugMemory();
	if(TargetMemory == nullptr)
	{
		return;
	}
	const FProperty* TargetProperty = TargetMemory->GetProperties()[InDebugOperand.GetRegisterIndex()];
	uint8* TargetPtr = TargetMemory->GetData<uint8>(InDebugOperand.GetRegisterIndex());

	// since debug properties are always arrays, we need to divert to the last array element's memory
	const FArrayProperty* TargetArrayProperty = CastField<FArrayProperty>(TargetProperty);
	if(TargetArrayProperty == nullptr)
	{
		return;
	}

	// add an element to the end for debug watching
	FScriptArrayHelper ArrayHelper(TargetArrayProperty, TargetPtr);

	if (Context.GetSlice().GetIndex() == 0)
	{
		ArrayHelper.Resize(0);
	}
	else if(Context.GetSlice().GetIndex() == ArrayHelper.Num() - 1)
	{
		return;
	}

	const int32 AddedIndex = ArrayHelper.AddValue();
	TargetPtr = ArrayHelper.GetRawPtr(AddedIndex);
	TargetProperty = TargetArrayProperty->Inner;

	if(InArg.GetMemoryType() == ERigVMMemoryType::External)
	{
		if(ExternalVariables.IsValidIndex(InArg.GetRegisterIndex()))
		{
			FRigVMExternalVariable& ExternalVariable = ExternalVariables[InArg.GetRegisterIndex()];
			const FProperty* SourceProperty = ExternalVariable.Property;
			const uint8* SourcePtr = ExternalVariable.Memory;
			URigVMMemoryStorage::CopyProperty(TargetProperty, TargetPtr, SourceProperty, SourcePtr);
		}
		return;
	}

	URigVMMemoryStorage* SourceMemory = GetMemoryByType(InArg.GetMemoryType());
	if(SourceMemory == nullptr)
	{
		return;
	}
	const FProperty* SourceProperty = SourceMemory->GetProperties()[InArg.GetRegisterIndex()];
	const uint8* SourcePtr = SourceMemory->GetData<uint8>(InArg.GetRegisterIndex());

	URigVMMemoryStorage::CopyProperty(TargetProperty, TargetPtr, SourceProperty, SourcePtr);
	
#endif
}

FRigVMCopyOp URigVM::GetCopyOpForOperands(const FRigVMOperand& InSource, const FRigVMOperand& InTarget)
{
	return FRigVMCopyOp(InSource, InTarget);
}

void URigVM::RefreshExternalPropertyPaths()
{
	ExternalPropertyPaths.Reset();

	ExternalPropertyPaths.SetNumZeroed(ExternalPropertyPathDescriptions.Num());
	for(int32 PropertyPathIndex = 0; PropertyPathIndex < ExternalPropertyPaths.Num(); PropertyPathIndex++)
	{
		ExternalPropertyPaths[PropertyPathIndex] = FRigVMPropertyPath();

		const int32 PropertyIndex = ExternalPropertyPathDescriptions[PropertyPathIndex].PropertyIndex;
		if(ExternalVariables.IsValidIndex(PropertyIndex))
		{
			check(ExternalVariables[PropertyIndex].Property);
			
			ExternalPropertyPaths[PropertyPathIndex] = FRigVMPropertyPath(
				ExternalVariables[PropertyIndex].Property,
				ExternalPropertyPathDescriptions[PropertyPathIndex].SegmentPath);
		}
	}
}

void URigVM::CopyArray(FScriptArrayHelper& TargetHelper, FRigVMMemoryHandle& TargetHandle,
	FScriptArrayHelper& SourceHelper, FRigVMMemoryHandle& SourceHandle)
{
	const FArrayProperty* TargetArrayProperty = CastFieldChecked<FArrayProperty>(TargetHandle.GetProperty());
	const FArrayProperty* SourceArrayProperty = CastFieldChecked<FArrayProperty>(SourceHandle.GetProperty());

	TargetHelper.Resize(SourceHelper.Num());
	if(SourceHelper.Num() > 0)
	{
		const FProperty* TargetProperty = TargetArrayProperty->Inner;
		const FProperty* SourceProperty = SourceArrayProperty->Inner;
		for(int32 ElementIndex = 0; ElementIndex < SourceHelper.Num(); ElementIndex++)
		{
			uint8* TargetMemory = TargetHelper.GetRawPtr(ElementIndex);
			const uint8* SourceMemory = SourceHelper.GetRawPtr(ElementIndex);
			URigVMMemoryStorage::CopyProperty(TargetProperty, TargetMemory, SourceProperty, SourceMemory);
		}
	}
}

