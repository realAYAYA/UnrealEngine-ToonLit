// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMNativized.h"
#include "UObject/Package.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "HAL/PlatformTLS.h"
#include "Async/ParallelFor.h"
#include "GenericPlatform/GenericPlatformSurvey.h"
#include "RigVMCore/RigVMStruct.h"
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
	, CurrentExecuteResult(ERigVMExecuteResult::Failed)
	, CurrentEntryName(NAME_None)
	, bCurrentlyRunningRootEntry(false)
#if WITH_EDITOR
	, StartCycles(0)
	, OverallCycles(0)
#endif
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

	// store the context public data object path
	FString ExecuteContextPath = GetContextPublicDataStruct()->GetPathName();
	Ar << ExecuteContextPath;

	// we rely on Ar.IsIgnoringArchetypeRef for determining if we are currently performing
	// CPFUO (Copy Properties for unrelated objects). During a reinstance pass we don't
	// want to overwrite the bytecode and some other properties - since that's handled already
	// by the RigVMCompiler.
	if(!Ar.IsIgnoringArchetypeRef())
	{
		ResolveFunctionsIfRequired();
		
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

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::RigVMSerializeExecuteContextStruct)
	{
		FString ExecuteContextPath;
		Ar << ExecuteContextPath;

		const FTopLevelAssetPath AssetPath(*ExecuteContextPath);
		UScriptStruct* ExecuteContextScriptStruct = FindObject<UScriptStruct>(AssetPath);
		if(ExecuteContextScriptStruct == nullptr)
		{
			Reset();
			return;
		}

		SetContextPublicDataStruct(ExecuteContextScriptStruct);
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

	// ensure to load the required functions
	if(!ResolveFunctionsIfRequired())
	{
		Reset();
		return;
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
			{
				const FRigVMBinaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMBinaryOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.ArgA);
				CheckOperandValidity(Op.ArgB);
				break;
			}
			case ERigVMOpCode::JumpToBranch:
			{
				const FRigVMJumpToBranchOp& Op = ByteCodeStorage.GetOpAt<FRigVMJumpToBranchOp>(ByteCodeInstruction);
				CheckOperandValidity(Op.Arg);
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
	return GetFunctions().Add(Function);
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

bool URigVM::CanExecuteEntry(const FName& InEntryName, bool bLogErrorForMissingEntry) const
{
	const int32 EntryIndex = FindEntry(InEntryName);
	if(EntryIndex == INDEX_NONE)
	{
		if(bLogErrorForMissingEntry)
		{
			static constexpr TCHAR MissingEntry[] = TEXT("Entry('%s') cannot be found.");
			Context.GetPublicData<>().Logf(EMessageSeverity::Error, MissingEntry, *InEntryName.ToString());
		}
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
		Context.GetPublicData<>().Logf(EMessageSeverity::Error, RecursiveEntry, *InEntryName.ToString(), *FString::Join(EntryNamesBeingExecuted, TEXT(" -> ")));
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

bool URigVM::ResumeExecution(TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName)
{
	ResumeExecution();
	return Execute(Memory, InEntryName) != ERigVMExecuteResult::Failed;
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

const UScriptStruct* URigVM::GetContextPublicDataStruct() const
{
	return Cast<UScriptStruct>(Context.PublicDataScope.GetStruct());
}

void URigVM::SetContextPublicDataStruct(UScriptStruct* InScriptStruct)
{
	if(GetContextPublicDataStruct() == InScriptStruct)
	{
		return;
	}
	Context.Initialize(InScriptStruct);
}

bool URigVM::ResolveFunctionsIfRequired()
{
	FScopeLock ResolveFunctionsScopeLock(&ResolveFunctionsMutex);
	
	bool bSuccess = true;

	if (GetFunctions().Num() != GetFunctionNames().Num())
	{
		GetFunctions().Reset();
		GetFunctions().SetNumZeroed(GetFunctionNames().Num());
		GetFactories().Reset();
		GetFactories().SetNumZeroed(GetFunctionNames().Num());

		TArray<FName>& FunctionNames = GetFunctionNames();
		for (int32 FunctionIndex = 0; FunctionIndex < FunctionNames.Num(); FunctionIndex++)
		{
			const FString FunctionNameString = FunctionNames[FunctionIndex].ToString();
			if(const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*FunctionNameString))
			{
				GetFunctions()[FunctionIndex] = Function;
				GetFactories()[FunctionIndex] = Function->Factory;
				
				// update the name in the function name list. the resolved function
				// may differ since it may rely on a core redirect.
				if(!FunctionNameString.Equals(Function->Name, ESearchCase::CaseSensitive))
				{
					FunctionNames[FunctionIndex] = *Function->Name;
					UE_LOG(LogRigVM, Verbose, TEXT("Redirected function '%s' to '%s' for VM '%s'"), *FunctionNameString, *Function->Name, *GetPathName());
				}
			}
			else
			{
				// We cannot recover from missing functions.
				UE_LOG(LogRigVM, Error, TEXT("No handler found for function '%s' for VM '%s'"), *FunctionNameString, *GetPathName());
				bSuccess = false;
			}
		}
	}

	return bSuccess;
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
	LazyBranches.Reset();
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
	TArray<const FRigVMFunction*>& Functions = GetFunctions();

	// force to update the map of branch infos once
	(void)ByteCode.GetBranchInfo({0, 0});
	LazyBranches.Reset();
	LazyBranches.SetNumZeroed(ByteCode.BranchInfos.Num());

	auto InstructionOpEval = [&](
		int32 InstructionIndex,
		int32 InHandleBaseIndex,
		TFunctionRef<void(int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg)> InOpFunc
		) -> void
	{
		const ERigVMOpCode OpCode = Instructions[InstructionIndex].OpCode; 

		if (OpCode >= ERigVMOpCode::Execute_0_Operands && OpCode <= ERigVMOpCode::Execute_64_Operands)
		{
			const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instructions[InstructionIndex]);
			FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[InstructionIndex]);
			const FRigVMFunction* Function = Functions[Op.FunctionIndex];

			for (int32 ArgIndex = 0; ArgIndex < Operands.Num(); ArgIndex++)
			{
				InOpFunc(
					InHandleBaseIndex++,
					{InstructionIndex, ArgIndex, Function->GetArgumentNameForOperandIndex(ArgIndex, Operands.Num())},
					Operands[ArgIndex]);
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
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex, {}, Op.Arg);
					break;
				}
			case ERigVMOpCode::Copy:
				{
					const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, {}, Op.Source);
					InOpFunc(InHandleBaseIndex + 1, {}, Op.Target);
					break;
				}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
				{
					const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
					FRigVMOperand Arg = Op.A;
					InOpFunc(InHandleBaseIndex + 0, {}, Arg);
					Arg = Op.B;
					InOpFunc(InHandleBaseIndex + 1, {}, Arg);
					Arg = Op.Result;
					InOpFunc(InHandleBaseIndex + 2, {}, Arg);
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
					InOpFunc(InHandleBaseIndex, {}, Arg);
					break;
				}
			case ERigVMOpCode::ChangeType:
				{
					const FRigVMChangeTypeOp& Op = ByteCode.GetOpAt<FRigVMChangeTypeOp>(Instructions[InstructionIndex]);
					const FRigVMOperand& Arg = Op.Arg;
					InOpFunc(InHandleBaseIndex, {}, Arg);
					break;
				}
			case ERigVMOpCode::Exit:
				{
					break;
				}
			case ERigVMOpCode::BeginBlock:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
					InOpFunc(InHandleBaseIndex + 0, {}, Op.ArgA);
					InOpFunc(InHandleBaseIndex + 1, {}, Op.ArgB);
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
			case ERigVMOpCode::JumpToBranch:
				{
					const FRigVMJumpToBranchOp& Op = ByteCode.GetOpAt<FRigVMJumpToBranchOp>(Instructions[InstructionIndex]);
					const FRigVMOperand& Arg = Op.Arg;
					InOpFunc(InHandleBaseIndex, {}, Arg);
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
		InstructionOpEval(InstructionIndex, INDEX_NONE,
			[&HandleCount](int32, const FRigVMBranchInfoKey&, const FRigVMOperand& )
			{
				HandleCount++;
			});
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
			InstructionOpEval(InstructionIndex, FirstHandleForInstruction[InstructionIndex],
				[&](int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InOp)
				{
					CacheSingleMemoryHandle(InHandleIndex, InBranchInfoKey, InOp);
				});
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
	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();

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
						const TArray<UObject*>* FullCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
						if (FullCallstack)
						{
							DebugInfo->SetCurrentActiveBreakpointCallstack(TArray<UObject*>(FullCallstack->GetData(), FullCallstack->Find((UObject*)Breakpoint.Subject)+1));
						}
						ExecutionHalted().Broadcast(ContextPublicData.InstructionIndex, Breakpoint.Subject, InEventName);
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
						const TArray<UObject*>* FullCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
						
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
		const TArray<UObject*>* CurrentCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
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
				const FRigVMBreakpoint& NewBreakpoint = DebugInfo->AddBreakpoint(ContextPublicData.InstructionIndex, NewBreakpointNode, 0, true);
				DebugInfo->SetBreakpointHits(NewBreakpoint, GetInstructionVisitedCount(ContextPublicData.InstructionIndex));
				DebugInfo->SetBreakpointActivationOnHit(NewBreakpoint, GetInstructionVisitedCount(ContextPublicData.InstructionIndex));
				CurrentBreakpointAction = ERigVMBreakpointAction::None;					

				HaltedAtBreakpoint = NewBreakpoint;
				HaltedAtBreakpointHit = DebugInfo->GetBreakpointHits(HaltedAtBreakpoint);
				ExecutionHalted().Broadcast(ContextPublicData.InstructionIndex, NewBreakpointNode, InEventName);
		
				return true;
			}
		}
	}

	return false;
}
#endif

bool URigVM::Initialize(TArrayView<URigVMMemoryStorage*> Memory)
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

	// re-initialize work memory from CDO
	if(URigVMMemoryStorage* WorkMemory = Memory[(int32)ERigVMMemoryType::Work])
	{
		if(!WorkMemory->HasAnyFlags(RF_ClassDefaultObject))
		{
			if(const URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(WorkMemory->GetClass()))
			{
				if(URigVMMemoryStorage* WorkMemoryCDO = MemoryClass->GetDefaultObject<URigVMMemoryStorage>())
				{
					for(const FProperty* Property : MemoryClass->LinkedProperties)
					{
						Property->CopyCompleteValue_InContainer(WorkMemory, WorkMemoryCDO);
#if UE_RIGVM_DEBUG_EXECUTION
						FString DefaultValue;
						const uint8* PropertyMemory = Property->ContainerPtrToValuePtr<uint8>(WorkMemory);
						Property->ExportText_Direct(
							DefaultValue,
							PropertyMemory,
							PropertyMemory,
							nullptr,
							PPF_None,
							nullptr);

						UE_LOG(LogRigVM, Warning, TEXT("Property %s defaults to '%s'."), *Property->GetName(), *DefaultValue);
						UE_LOG(LogRigVM, Warning, TEXT("Property %s defaults to '%s'."), *Property->GetName(), *DefaultValue);
#endif
					}
				}
			}
		}
	}
	
	CacheMemoryHandlesIfRequired(Memory);

	return true;
}

ERigVMExecuteResult URigVM::Execute(TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName)
{
	// if this the first entry being executed - get ready for execution
	const bool bIsRootEntry = EntriesBeingExecuted.IsEmpty();

	TGuardValue<FName> EntryNameGuard(CurrentEntryName, InEntryName);
	TGuardValue<bool> RootEntryGuard(bCurrentlyRunningRootEntry, bIsRootEntry);

	if(bIsRootEntry)
	{
		CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
		
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
			return CurrentExecuteResult = ERigVMExecuteResult::Failed;
		}

		// changes to the layout of memory array should be reflected in GetContainerIndex()
		TArray<URigVMMemoryStorage*> LocalMemory;
		if (Memory.Num() == 0)
		{
			LocalMemory = GetLocalMemoryArray();
			Memory = LocalMemory;
		}
	
		CacheMemoryHandlesIfRequired(Memory);
		CurrentMemory = Memory;
	}

	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();

	if(bIsRootEntry)
	{
		if (FirstEntryEventInQueue == NAME_None || FirstEntryEventInQueue == InEntryName)
		{
			SetupInstructionTracking(Instructions.Num());
		}
	}
#endif

	if(bIsRootEntry)
	{
		Context.Reset();
		Context.SliceOffsets.AddZeroed(Instructions.Num());
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
			return CurrentExecuteResult = ERigVMExecuteResult::Failed;
		}
		
		SetInstructionIndex((uint16)ByteCode.GetEntry(EntryIndex).InstructionIndex);
		EntryIndexToPush = EntryIndex;

		if(bIsRootEntry)
		{
			ContextPublicData.EventName = InEntryName;
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
				ContextPublicData.EventName = ByteCode.GetEntry(EntryIndexToPush).Name;
			}
			else
			{
				ContextPublicData.EventName = NAME_None;
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
	StartProfiling();
	
#if UE_RIGVM_DEBUG_EXECUTION
	ContextPublicData.DebugMemoryString;
	ContextPublicData.InstanceOpCodeEnum = StaticEnum<ERigVMOpCode>();
	URigVMMemoryStorage* LiteralMemory = GetLiteralMemory(false);
	ContextPublicData.DebugMemoryString = FString("\n\nLiteral Memory\n\n");
	for (int32 PropertyIndex=0; PropertyIndex<LiteralMemory->Num(); ++PropertyIndex)
	{
		ContextPublicData.DebugMemoryString += FString::Printf(TEXT("%s: %s\n"), *LiteralMemory->GetProperties()[PropertyIndex]->GetFullName(), *LiteralMemory->GetDataAsString(PropertyIndex));				
	}
	ContextPublicData.DebugMemoryString += FString(TEXT("\n\nWork Memory\n\n"));
	
#endif
	
#endif

	CurrentExecuteResult = ExecuteInstructions(ContextPublicData.InstructionIndex, Instructions.Num() - 1);

#if WITH_EDITOR
	if(bIsRootEntry)
	{
		CurrentMemory = TArrayView<URigVMMemoryStorage*>();
		
		if (HaltedAtBreakpoint.IsValid() && CurrentExecuteResult != ERigVMExecuteResult::Halted)
		{
			DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
			HaltedAtBreakpoint.Reset();
			ExecutionHalted().Broadcast(INDEX_NONE, nullptr, InEntryName);
		}
	}
#endif

	return CurrentExecuteResult;
}

ERigVMExecuteResult URigVM::ExecuteInstructions(int32 InFirstInstruction, int32 InLastInstruction)
{
	// make we are already executing this VM
	check(!CurrentMemory.IsEmpty());
	
	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();
	TGuardValue<uint16> InstructionIndexGuard(ContextPublicData.InstructionIndex, (uint16)InFirstInstruction);
	
	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();
#endif
	
	while (Instructions.IsValidIndex(ContextPublicData.InstructionIndex))
	{
#if WITH_EDITOR
		if (ShouldHaltAtInstruction(CurrentEntryName, ContextPublicData.InstructionIndex))
		{
#if UE_RIGVM_DEBUG_EXECUTION
			ContextPublicData.Log(EMessageSeverity::Info, ContextPublicData.DebugMemoryString);					
#endif
			// we'll recursively exit all invoked
			// entries here.
			return CurrentExecuteResult = ERigVMExecuteResult::Halted;
		}

		if(CurrentExecuteResult == ERigVMExecuteResult::Halted)
		{
			return CurrentExecuteResult;
		}

#endif
		
		if(ContextPublicData.InstructionIndex > (uint16)InLastInstruction)
		{
			return CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
		}

#if WITH_EDITOR

		const int32 CurrentInstructionIndex = ContextPublicData.InstructionIndex;
		InstructionVisitedDuringLastRun[ContextPublicData.InstructionIndex]++;
		InstructionVisitOrder.Add(ContextPublicData.InstructionIndex);
	
#endif

		const FRigVMInstruction& Instruction = Instructions[ContextPublicData.InstructionIndex];

#if WITH_EDITOR
#if UE_RIGVM_DEBUG_EXECUTION
		if (Instruction.OpCode >= ERigVMOpCode::Execute_0_Operands && Instruction.OpCode <= ERigVMOpCode::Execute_64_Operands)
		{
			const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
			FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[ContextPublicData.InstructionIndex]);

			TArray<FString> Labels;
			for (const FRigVMOperand& Operand : Operands)
			{
				Labels.Add(GetOperandLabel(Operand));
			}

			ContextPublicData.DebugMemoryString += FString::Printf(TEXT("Instruction %d: %s(%s)\n"), ContextPublicData.InstructionIndex, *FunctionNames[Op.FunctionIndex].ToString(), *FString::Join(Labels, TEXT(", ")));				
		}
		else if(Instruction.OpCode == ERigVMOpCode::Copy)
		{
			const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);
			ContextPublicData.DebugMemoryString += FString::Printf(TEXT("Instruction %d: Copy %s -> %s\n"), ContextPublicData.InstructionIndex, *GetOperandLabel(Op.Source), *GetOperandLabel(Op.Target));				
		}
		else
		{
			ContextPublicData.DebugMemoryString += FString::Printf(TEXT("Instruction %d: %s\n"), ContextPublicData.InstructionIndex, *ContextPublicData.InstanceOpCodeEnum->GetNameByIndex((uint8)Instruction.OpCode).ToString());				
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
				const int32 OperandCount = FirstHandleForInstruction[ContextPublicData.InstructionIndex + 1] - FirstHandleForInstruction[ContextPublicData.InstructionIndex];
				FRigVMMemoryHandleArray Handles;
				if(OperandCount > 0)
				{
					Handles = FRigVMMemoryHandleArray(&CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]], OperandCount);
				}
#if WITH_EDITOR
				ContextPublicData.FunctionName = FunctionNames[Op.FunctionIndex];
#endif
				Context.Factory = Factories[Op.FunctionIndex];
				(*Functions[Op.FunctionIndex]->FunctionPtr)(Context, Handles);

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

				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Zero:
			{
				const FRigVMMemoryHandle& Handle = CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]];
				if(Handle.GetProperty()->IsA<FIntProperty>())
				{
					*((int32*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = 0;
				}
				else if(Handle.GetProperty()->IsA<FNameProperty>())
				{
					*((FName*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = NAME_None;
				}
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]]);
				}
#endif

				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::BoolFalse:
			{
				*((bool*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = false;
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::BoolTrue:
			{
				*((bool*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = true;
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);

				FRigVMMemoryHandle& SourceHandle = CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]];
				FRigVMMemoryHandle& TargetHandle = CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex] + 1];
				URigVMMemoryStorage::CopyProperty(TargetHandle, SourceHandle);
					
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					CopyOperandForDebuggingIfNeeded(Op.Source, SourceHandle);
				}
#endif
					
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Increment:
			{
				(*((int32*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()))++;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]]);
				}
#endif
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Decrement:
			{
				(*((int32*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()))--;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Op.Arg, CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]]);
				}
#endif
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instruction);

				FRigVMMemoryHandle& HandleA = CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]];
				FRigVMMemoryHandle& HandleB = CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex] + 1];
				const bool Result = HandleA.GetProperty()->Identical(HandleA.GetData(true), HandleB.GetData(true));

				*((bool*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]+2].GetData()) = Result;
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::JumpAbsolute:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instruction);
				ContextPublicData.InstructionIndex = Op.InstructionIndex;
				break;
			}
			case ERigVMOpCode::JumpForward:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instruction);
				ContextPublicData.InstructionIndex += Op.InstructionIndex;
				break;
			}
			case ERigVMOpCode::JumpBackward:
			{
				const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Instruction);
				ContextPublicData.InstructionIndex -= Op.InstructionIndex;
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instruction);
				const bool Condition = *(bool*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();
				if (Condition == Op.Condition)
				{
					ContextPublicData.InstructionIndex = Op.InstructionIndex;
				}
				else
				{
					ContextPublicData.InstructionIndex++;
				}
				break;
			}
			case ERigVMOpCode::JumpForwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instruction);
				const bool Condition = *(bool*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();
				if (Condition == Op.Condition)
				{
					ContextPublicData.InstructionIndex += Op.InstructionIndex;
				}
				else
				{
					ContextPublicData.InstructionIndex++;
				}
				break;
			}
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instruction);
				const bool Condition = *(bool*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();
				if (Condition == Op.Condition)
				{
					ContextPublicData.InstructionIndex -= Op.InstructionIndex;
				}
				else
				{
					ContextPublicData.InstructionIndex++;
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
				if(bCurrentlyRunningRootEntry)
				{
					StopProfiling();
					ExecutionReachedExit().Broadcast(CurrentEntryName);
#if WITH_EDITOR					
					if (HaltedAtBreakpoint.IsValid())
					{
						HaltedAtBreakpoint.Reset();
						DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
						ExecutionHalted().Broadcast(INDEX_NONE, nullptr, CurrentEntryName);
					}
#if UE_RIGVM_DEBUG_EXECUTION
					ContextPublicData.Log(EMessageSeverity::Info, ContextPublicData.DebugMemoryString);					
#endif
#endif
				}
				return ERigVMExecuteResult::Succeeded;
			}
			case ERigVMOpCode::BeginBlock:
			{
				const int32 Count = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()));
				const int32 Index = (*((int32*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex] + 1].GetData()));
				Context.BeginSlice(Count, Index);
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::EndBlock:
			{
				Context.EndSlice();
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::InvokeEntry:
			{
				const FRigVMInvokeEntryOp& Op = ByteCode.GetOpAt<FRigVMInvokeEntryOp>(Instruction);

				if(!CanExecuteEntry(Op.EntryName))
				{
					return CurrentExecuteResult = ERigVMExecuteResult::Failed;
				}
				else
				{
					// this will restore the public data after invoking the entry
					TGuardValue<FRigVMExecuteContext> PublicDataGuard(ContextPublicData, ContextPublicData);
					const ERigVMExecuteResult ExecuteResult = Execute(CurrentMemory, Op.EntryName);
					if(ExecuteResult != ERigVMExecuteResult::Succeeded)
					{
						return CurrentExecuteResult = ExecuteResult;
					}

#if WITH_EDITOR
					// if we are halted at a break point we need to exit here
					if (ShouldHaltAtInstruction(CurrentEntryName, ContextPublicData.InstructionIndex))
					{
						return CurrentExecuteResult = ERigVMExecuteResult::Halted;
					}
#endif
				}
					
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::JumpToBranch:
			{
				const FRigVMJumpToBranchOp& Op = ByteCode.GetOpAt<FRigVMJumpToBranchOp>(Instruction);
				// BranchLabel = Op.Arg
				FName BranchLabel = *(FName*)CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();

				// iterate over the branches stored in the bytecode,
				// starting at the first branch index stored in the operator.
				// look over all branches matching this instruction index and
				// find the one with the right label - then jump to the branch.
				bool bBranchFound = false;
				const TArray<FRigVMBranchInfo>& Branches = ByteCode.BranchInfos;
				if(Branches.IsEmpty())
				{
					UE_LOG(LogRigVM, Error, TEXT("No branches in ByteCode - but JumpToBranch instruction %d found. Likely a corrupt VM. Exiting."), ContextPublicData.InstructionIndex);
					return CurrentExecuteResult = ERigVMExecuteResult::Failed;
				}

				for(int32 PassIndex = 0; PassIndex < 2; PassIndex++)
				{
					for(int32 BranchIndex = Op.FirstBranchInfoIndex // start at the first branch known to this jump op
						; BranchIndex < Branches.Num(); BranchIndex++)
					{
						const FRigVMBranchInfo& Branch = Branches[BranchIndex];
						if(Branch.InstructionIndex != ContextPublicData.InstructionIndex)
						{
							break;
						}
						if(Branch.Label == BranchLabel)
						{
							ContextPublicData.InstructionIndex = Branch.FirstInstruction;
							bBranchFound = true;
							break;
						}
					}

					// if we don't find the branch - try to jump to the completed branch
					if (!bBranchFound)
					{
						if(PassIndex == 0 && BranchLabel != FRigVMStruct::ControlFlowCompletedName)
						{
							UE_LOG(LogRigVM, Warning, TEXT("Branch '%s' was not found for instruction %d."), *BranchLabel.ToString(), ContextPublicData.InstructionIndex);
							BranchLabel = FRigVMStruct::ControlFlowCompletedName;
							continue;
						}
						
						UE_LOG(LogRigVM, Error, TEXT("Branch '%s' was not found for instruction %d."), *BranchLabel.ToString(), ContextPublicData.InstructionIndex);
						return ERigVMExecuteResult::Failed;
					}
					break;
				}
				break;
			}
			case ERigVMOpCode::Invalid:
			{
				ensure(false);
				return CurrentExecuteResult = ERigVMExecuteResult::Failed;
			}
		}

#if WITH_EDITOR
		if(ContextPublicData.RuntimeSettings.bEnableProfiling && !InstructionVisitOrder.IsEmpty())
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
			if (ContextPublicData.PreviousWorkMemory.Num() > 0 && ContextPublicData.PreviousWorkMemory.IsValidIndex(PropertyIndex) && ContextPublicData.PreviousWorkMemory[PropertyIndex].StartsWith(TEXT(" -- ")))
			{
				ContextPublicData.PreviousWorkMemory[PropertyIndex].RightChopInline(4);
			}
			if (ContextPublicData.PreviousWorkMemory.Num() == 0 || (ContextPublicData.PreviousWorkMemory.IsValidIndex(PropertyIndex) && Line == ContextPublicData.PreviousWorkMemory[PropertyIndex]))
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
			if (ContextPublicData.PreviousWorkMemory.Num() > 0 && ContextPublicData.PreviousWorkMemory.IsValidIndex(LineIndex) && ContextPublicData.PreviousWorkMemory[LineIndex].StartsWith(TEXT(" -- ")))
			{
				ContextPublicData.PreviousWorkMemory[LineIndex].RightChopInline(4);
			}
			if (ContextPublicData.PreviousWorkMemory.Num() == 0 || (ContextPublicData.PreviousWorkMemory.IsValidIndex(LineIndex) && Line == ContextPublicData.PreviousWorkMemory[LineIndex]))
			{
				CurrentWorkMemory.Add(Line);
			}
			else
			{
				CurrentWorkMemory.Add(FString::Printf(TEXT(" -- %s"), *Line));
			}
			++LineIndex;
		}
		ContextPublicData.DebugMemoryString += FString::Join(CurrentWorkMemory, TEXT("\n")) + FString(TEXT("\n\n"));
		ContextPublicData.PreviousWorkMemory = CurrentWorkMemory;
#endif
#endif
	}

	return CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
}

bool URigVM::Execute(const FName& InEntryName)
{
	return Execute(TArray<URigVMMemoryStorage*>(), InEntryName) != ERigVMExecuteResult::Failed;
}

ERigVMExecuteResult URigVM::ExecuteLazyBranch(const FRigVMBranchInfo& InBranchToRun)
{
	// likely have to optimize this
	TGuardValue<FRigVMExtendedExecuteContext> ContextGuard(Context, Context);
	return ExecuteInstructions(InBranchToRun.FirstInstruction, InBranchToRun.LastInstruction);
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
			case ERigVMOpCode::InvokeEntry:
			{
				const FRigVMInvokeEntryOp& Op = ByteCode.GetOpAt<FRigVMInvokeEntryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Invoke entry %s"), *Op.EntryName.ToString());
				break;
			}
			case ERigVMOpCode::JumpToBranch:
			{
				const FRigVMJumpToBranchOp& Op = ByteCode.GetOpAt<FRigVMJumpToBranchOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Jump To Branch %s"), *GetOperandLabel(Op.Arg, OperandFormatFunction));
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
	RefreshExternalPropertyPaths();
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

void URigVM::CacheSingleMemoryHandle(int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg, bool bForExecute)
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
	FRigVMMemoryHandle& Handle = CachedMemoryHandles[InHandleIndex];
	Handle = {Data, Property, PropertyPath};

	// if we are lazy executing update the handle to point to a lazy branch
	if(InBranchInfoKey.IsValid())
	{
		if(const FRigVMBranchInfo* BranchInfo = GetByteCode().GetBranchInfo(InBranchInfoKey))
		{
			FRigVMLazyBranch* LazyBranch = &LazyBranches[BranchInfo->Index];
			LazyBranch->VM = this;
			LazyBranch->BranchInfo = *BranchInfo;
			Handle.LazyBranch = LazyBranch;
		}
	}

#if UE_BUILD_DEBUG
	// make sure the handle points to valid memory
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

void URigVM::SetupInstructionTracking(int32 InInstructionCount)
{
#if WITH_EDITOR
	InstructionVisitedDuringLastRun.Reset();
	InstructionVisitOrder.Reset();
	InstructionVisitedDuringLastRun.SetNumZeroed(InInstructionCount);
	InstructionCyclesDuringLastRun.Reset();

	if(Context.GetPublicData<>().RuntimeSettings.bEnableProfiling)
	{
		InstructionCyclesDuringLastRun.SetNumUninitialized(InInstructionCount);
		for(int32 DurationIndex=0;DurationIndex<InstructionCyclesDuringLastRun.Num();DurationIndex++)
		{
			InstructionCyclesDuringLastRun[DurationIndex] = UINT64_MAX;
		}
	}
#endif
}

void URigVM::StartProfiling()
{
#if WITH_EDITOR
	OverallCycles = StartCycles = 0;
	if(Context.GetPublicData<>().RuntimeSettings.bEnableProfiling)
	{
		StartCycles = FPlatformTime::Cycles64();
	}
#endif
}

void URigVM::StopProfiling()
{
#if WITH_EDITOR
	const uint64 Cycles = OverallCycles > 0 ? OverallCycles : (FPlatformTime::Cycles64() - StartCycles); 
	Context.LastExecutionMicroSeconds = Cycles * FPlatformTime::GetSecondsPerCycle() * 1000.0 * 1000.0;
#endif
}
