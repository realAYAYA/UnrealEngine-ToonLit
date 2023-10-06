// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMNativized.h"
#include "UObject/Package.h"
#include "UObject/AnimObjectVersion.h"
#include "RigVMObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "RigVMObjectVersion.h"
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
    , FunctionNamesPtr(&FunctionNamesStorage)
    , FunctionsPtr(&FunctionsStorage)
    , FactoriesPtr(&FactoriesStorage)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS // required until we eliminate ExecutionReachedExit and ExecutionHalted
URigVM::~URigVM()
{
	Reset();

	ExecutionReachedExit().Clear();
#if WITH_EDITOR
	ExecutionHalted().Clear();
#endif
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS // required until we eliminate ExecutionReachedExit and ExecutionHalted

void URigVM::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FRigVMObjectVersion::GUID);

	if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::StoreMarkerNamesOnSkeleton)
	{
		return;
	}
	
	// call into the super class to serialize any uproperty
	if(Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Super::Serialize(Ar);
	}
	
	ensure(ActiveExecutions.load() == 0);  // we require that no active worker threads are running while we serialize

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
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	
	CopyDeferredVMIfRequired();

	// we rely on Ar.IsIgnoringArchetypeRef for determining if we are currently performing
	// CPFUO (Copy Properties for unrelated objects). During a reinstance pass we don't
	// want to overwrite the bytecode and some other properties - since that's handled already
	// by the RigVMCompiler.
	if(!Ar.IsIgnoringArchetypeRef())
	{
		ResolveFunctionsIfRequired();
	
		Ar << CachedVMHash;
		Ar << ExternalPropertyPathDescriptions;
		Ar << FunctionNamesStorage;
		Ar << ByteCodeStorage;
		Ar << Parameters;

		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::RigVMSaveDebugMapInGraphFunctionData)
		{
			return;
		}

		Ar << OperandToDebugRegisters;
	}
}

void URigVM::Load(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	
	// we rely on Ar.IsIgnoringArchetypeRef for determining if we are currently performing
	// CPFUO (Copy Properties for unrelated objects). During a reinstance pass we don't
	// want to overwrite the bytecode and some other properties - since that's handled already
	// by the RigVMCompiler.
	Reset(Ar.IsIgnoringArchetypeRef());

	if (Ar.CustomVer(FRigVMObjectVersion::GUID) < FRigVMObjectVersion::BeforeCustomVersionWasAdded)
	{
		int32 RigVMUClassBasedStorageDefine = 1;
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::RigVMMemoryStorageObject)
		{
			Ar << RigVMUClassBasedStorageDefine;
		}

		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMExternalExecuteContextStruct
			&& Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::RigVMSerializeExecuteContextStruct)
		{
			FString ExecuteContextPath;
			Ar << ExecuteContextPath;
			// Context is now external to the VM, just serializing the string to keep compatibility
		}

		if (RigVMUClassBasedStorageDefine == 1)
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

			if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::RigVMSaveDebugMapInGraphFunctionData)
			{
				Ar << OperandToDebugRegisters;
			}
		}

		// we only deal with virtual machines now that use the new memory infrastructure.
		ensure(UE_RIGVM_UCLASS_BASED_STORAGE_DISABLED == 0);
		if (RigVMUClassBasedStorageDefine != UE_RIGVM_UCLASS_BASED_STORAGE_DISABLED)
		{
			Reset();
			return;
		}
	}

	// requesting the memory types will create them
	// Cooked platforms will just load the objects and do no need to clear the referenes
	// In certain scenarios RequiresCookedData will be false but the PKG_FilterEditorOnly will still be set (UEFN)
	if (!FPlatformProperties::RequiresCookedData() && !GetClass()->RootPackageHasAnyFlags(PKG_FilterEditorOnly))
	{
		ClearMemory();
	}

	if (!Ar.IsIgnoringArchetypeRef())
	{
		if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::AddedVMHashChecks)
		{
			Ar << CachedVMHash;
		}
		Ar << ExternalPropertyPathDescriptions;
		Ar << FunctionNamesStorage;
		Ar << ByteCodeStorage;
		Ar << Parameters;

		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::RigVMSaveDebugMapInGraphFunctionData)
		{
			Ar << OperandToDebugRegisters;
		}
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

bool URigVM::IsContextValidForExecution(FRigVMExtendedExecuteContext& Context) const
{
	return Context.VMHash == GetVMHash();
}

// Stores the current VM hash
void URigVM::SetVMHash(uint32 InVMHash)
{
	CachedVMHash = InVMHash;
}

uint32 URigVM::GetVMHash() const
{
	return CachedVMHash;
}

uint32 URigVM::ComputeVMHash() const
{
	uint32 Hash = 0;
	for(const FName& FunctionName : GetFunctionNames())
	{
		Hash = HashCombine(Hash, GetTypeHash(FunctionName.ToString()));
	}

	Hash = HashCombine(Hash, GetTypeHash(GetByteCode()));

	for(const FRigVMExternalVariableDef& ExternalVariable : ExternalVariables)
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

UClass* URigVM::GetNativizedClass(const TArray<FRigVMExternalVariableDef>& InExternalVariables)
{
	TSharedPtr<TGuardValue<TArray<FRigVMExternalVariableDef>>> GuardPtr;
	if(!InExternalVariables.IsEmpty())
	{
		GuardPtr = MakeShareable(new TGuardValue<TArray<FRigVMExternalVariableDef>>(ExternalVariables, InExternalVariables));
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
			case ERigVMOpCode::Execute:
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
		CachedVMHash = 0;
		FunctionNamesStorage.Reset();
		FunctionsStorage.Reset();
		FactoriesStorage.Reset();
		ExternalPropertyPathDescriptions.Reset();
		ExternalPropertyPaths.Reset();
		ByteCodeStorage.Reset();
		Instructions.Reset();
		Parameters.Reset();
		ParametersNameMap.Reset();
		OperandToDebugRegisters.Reset();
	}

	if(!IsIgnoringArchetypeRef)
	{
		FunctionNamesPtr = &FunctionNamesStorage;
		FunctionsPtr = &FunctionsStorage;
		FactoriesPtr = &FactoriesStorage;
		ByteCodePtr = &ByteCodeStorage;
	}

	ExternalPropertyPaths.Reset();
	LazyBranches.Reset();

	InvalidateCachedMemory();
	DeferredVMToCopy = nullptr;
}

void URigVM::Empty(FRigVMExtendedExecuteContext& Context)
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
	ExternalVariables.Empty();

	InvalidateCachedMemory();

	OperandToDebugRegisters.Empty();

	DeferredVMToCopy = nullptr;

	Context.CachedMemory.Empty();
	Context.CachedMemoryHandles.Empty();
	Context.ExternalVariableRuntimeData.Reset();
}

void URigVM::CopyFrom(URigVM* InVM, bool bDeferCopy, bool bReferenceLiteralMemory, bool bReferenceByteCode, bool bCopyExternalVariables, bool bCopyDynamicRegisters)
{
	check(InVM);

	// if this vm is currently executing on a worker thread
	// we defer the copy until the next execute
	if (ActiveExecutions.load() > 0 || bDeferCopy)
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

	CachedVMHash = InVM->CachedVMHash;

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

bool URigVM::CanExecuteEntry(const FRigVMExtendedExecuteContext& Context, const FName& InEntryName, bool bLogErrorForMissingEntry) const
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
	
	if(Context.EntriesBeingExecuted.Contains(EntryIndex))
	{
		TArray<FString> EntryNamesBeingExecuted;
		for(const int32 EntryBeingExecuted : Context.EntriesBeingExecuted)
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

bool URigVM::ResumeExecution(FRigVMExtendedExecuteContext& Context)
{
	Context.HaltedAtBreakpoint.Reset();
	Context.HaltedAtBreakpointHit = INDEX_NONE;
	if (Context.DebugInfo)
	{
		if (const FRigVMBreakpoint& CurrentBreakpoint = Context.DebugInfo->GetCurrentActiveBreakpoint())
		{
			Context.DebugInfo->IncrementBreakpointActivationOnHit(CurrentBreakpoint);
			Context.DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
			return true;
		}
	}

	return false;
}

bool URigVM::ResumeExecution(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName)
{
	ResumeExecution(Context);
	return Execute(Context, Memory, InEntryName) != ERigVMExecuteResult::Failed;
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
	FirstHandleForInstruction.Reset();
	ExternalPropertyPaths.Reset();
	LazyBranches.Reset();
}

void URigVM::InvalidateCachedMemory(FRigVMExtendedExecuteContext& Context)
{
	InvalidateCachedMemory();
	Context.InvalidateCachedMemory();
}

void URigVM::CopyDeferredVMIfRequired()
{
	ensure(ActiveExecutions.load() == 0);  // we require that no active worker threads are running while we serialize

	URigVM* VMToCopy = nullptr;
	Swap(VMToCopy, DeferredVMToCopy);

	if (VMToCopy)
	{
		CopyFrom(VMToCopy);
	}
}

void URigVM::CacheMemoryHandlesIfRequired(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> InMemory)
{
	ensureMsgf(Context.ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::CacheMemoryHandlesIfRequired from multiple threads (%d and %d)"), Context.ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());

	RefreshInstructionsIfRequired();

	if (Instructions.Num() == 0 || InMemory.Num() == 0)
	{
		InvalidateCachedMemory(Context);
		return;
	}

	if ((Instructions.Num() + 1) != FirstHandleForInstruction.Num())
	{
		InvalidateCachedMemory(Context);
	}
	else if (InMemory.Num() != Context.CachedMemory.Num())
	{
		InvalidateCachedMemory(Context);
	}
	else
	{
		for (int32 Index = 0; Index < InMemory.Num(); Index++)
		{
			if (InMemory[Index] != Context.CachedMemory[Index])
			{
				InvalidateCachedMemory(Context);
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
		Context.CachedMemory.Add(InMemory[Index]);
	}

	RefreshExternalPropertyPaths();

	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<const FRigVMFunction*>& Functions = GetFunctions();

	// force to update the map of branch infos once
	(void)ByteCode.GetBranchInfo({0, 0});
	LazyBranches.Reset();
	Context.LazyBranchInstanceData.Reset();
	LazyBranches.SetNumZeroed(ByteCode.BranchInfos.Num());
	Context.LazyBranchInstanceData.SetNumZeroed(ByteCode.BranchInfos.Num());

	auto InstructionOpEval = [&](
		int32 InstructionIndex,
		int32 InHandleBaseIndex,
		TFunctionRef<void(FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg)> InOpFunc
		) -> void
	{
		const ERigVMOpCode OpCode = Instructions[InstructionIndex].OpCode; 

		if (OpCode == ERigVMOpCode::Execute)
		{
			const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instructions[InstructionIndex]);
			FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[InstructionIndex]);
			const FRigVMFunction* Function = Functions[Op.FunctionIndex];

			for (int32 ArgIndex = 0; ArgIndex < Operands.Num(); ArgIndex++)
			{
				InOpFunc(
					Context,
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
					InOpFunc(Context, InHandleBaseIndex, {}, Op.Arg);
					break;
				}
			case ERigVMOpCode::Copy:
				{
					const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instructions[InstructionIndex]);
					InOpFunc(Context, InHandleBaseIndex + 0, {}, Op.Source);
					InOpFunc(Context, InHandleBaseIndex + 1, {}, Op.Target);
					break;
				}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
				{
					const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
					FRigVMOperand Arg = Op.A;
					InOpFunc(Context, InHandleBaseIndex + 0, {}, Arg);
					Arg = Op.B;
					InOpFunc(Context, InHandleBaseIndex + 1, {}, Arg);
					Arg = Op.Result;
					InOpFunc(Context, InHandleBaseIndex + 2, {}, Arg);
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
					InOpFunc(Context, InHandleBaseIndex, {}, Arg);
					break;
				}
			case ERigVMOpCode::ChangeType:
				{
					const FRigVMChangeTypeOp& Op = ByteCode.GetOpAt<FRigVMChangeTypeOp>(Instructions[InstructionIndex]);
					const FRigVMOperand& Arg = Op.Arg;
					InOpFunc(Context, InHandleBaseIndex, {}, Arg);
					break;
				}
			case ERigVMOpCode::Exit:
				{
					break;
				}
			case ERigVMOpCode::BeginBlock:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Instructions[InstructionIndex]);
					InOpFunc(Context, InHandleBaseIndex + 0, {}, Op.ArgA);
					InOpFunc(Context, InHandleBaseIndex + 1, {}, Op.ArgB);
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
					InOpFunc(Context, InHandleBaseIndex, {}, Arg);
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
			[&HandleCount](FRigVMExtendedExecuteContext& Context, int32, const FRigVMBranchInfoKey&, const FRigVMOperand& )
			{
				HandleCount++;
			});
		FirstHandleForInstruction.Add(HandleCount);
	}
	
	// Allocate all the space and zero it out to ensure all pages required for it are paged in
	// immediately.
	Context.CachedMemoryHandles.SetNumUninitialized(HandleCount);

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
				[&](FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InOp)
				{
					CacheSingleMemoryHandle(Context, InHandleIndex, InBranchInfoKey, InOp);
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
bool URigVM::ShouldHaltAtInstruction(FRigVMExtendedExecuteContext& Context, const FName& InEventName, const uint16 InstructionIndex)
{
	if(Context.DebugInfo == nullptr)
	{
		return false;
	}

	if(Context.DebugInfo->IsEmpty())
	{
		return false;
	}
	
	FRigVMByteCode& ByteCode = GetByteCode();
	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();

	TArray<FRigVMBreakpoint> BreakpointsAtInstruction = Context.DebugInfo->FindBreakpointsAtInstruction(InstructionIndex);
	for (FRigVMBreakpoint Breakpoint : BreakpointsAtInstruction)
	{
		if (Context.DebugInfo->IsActive(Breakpoint))
		{
			switch (Context.CurrentBreakpointAction)
			{
				case ERigVMBreakpointAction::None:
				{
					// Halted at breakpoint. Check if this is a new breakpoint different from the previous halt.
					if (Context.HaltedAtBreakpoint != Breakpoint ||
						Context.HaltedAtBreakpointHit != Context.DebugInfo->GetBreakpointHits(Breakpoint))
					{
						Context.HaltedAtBreakpoint = Breakpoint;
						Context.HaltedAtBreakpointHit = Context.DebugInfo->GetBreakpointHits(Breakpoint);
						Context.DebugInfo->SetCurrentActiveBreakpoint(Breakpoint);
						
						// We want to keep the callstack up to the node that produced the halt
						const TArray<UObject*>* FullCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
						if (FullCallstack)
						{
							Context.DebugInfo->SetCurrentActiveBreakpointCallstack(TArray<UObject*>(FullCallstack->GetData(), FullCallstack->Find((UObject*)Breakpoint.Subject)+1));
						}
						Context.ExecutionHalted().Broadcast(ContextPublicData.InstructionIndex, Breakpoint.Subject, InEventName);
					}
					return true;
				}
				case ERigVMBreakpointAction::Resume:
				{
					Context.CurrentBreakpointAction = ERigVMBreakpointAction::None;

					if (Context.DebugInfo->IsTemporaryBreakpoint(Breakpoint))
					{
						Context.DebugInfo->RemoveBreakpoint(Breakpoint);
					}
					else
					{
						Context.DebugInfo->IncrementBreakpointActivationOnHit(Breakpoint);
						Context.DebugInfo->HitBreakpoint(Breakpoint);
					}
					break;
				}
				case ERigVMBreakpointAction::StepOver:
				case ERigVMBreakpointAction::StepInto:
				case ERigVMBreakpointAction::StepOut:
				{
					// If we are stepping, check if we were halted at the current instruction, and remember it 
					if (!Context.DebugInfo->GetCurrentActiveBreakpoint())
					{
						Context.DebugInfo->SetCurrentActiveBreakpoint(Breakpoint);
						const TArray<UObject*>* FullCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
						
						// We want to keep the callstack up to the node that produced the halt
						if (FullCallstack)
						{
							Context.DebugInfo->SetCurrentActiveBreakpointCallstack(TArray<UObject*>(FullCallstack->GetData(), FullCallstack->Find((UObject*)Context.DebugInfo->GetCurrentActiveBreakpoint().Subject)+1));
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
			Context.DebugInfo->HitBreakpoint(Breakpoint);
		}
	}

	// If we are stepping, and the last active breakpoint was set, check if this is the new temporary breakpoint
	if (Context.CurrentBreakpointAction != ERigVMBreakpointAction::None && Context.DebugInfo->GetCurrentActiveBreakpoint())
	{
		const TArray<UObject*>* CurrentCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
		if (CurrentCallstack && !CurrentCallstack->IsEmpty())
		{
			UObject* NewBreakpointNode = nullptr;

			// Find the first difference in the callstack
			int32 DifferenceIndex = INDEX_NONE;
			TArray<UObject*>& PreviousCallstack = Context.DebugInfo->GetCurrentActiveBreakpointCallstack();
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

			if (Context.CurrentBreakpointAction == ERigVMBreakpointAction::StepOver)
			{
				if (DifferenceIndex != INDEX_NONE)
				{
					NewBreakpointNode = CurrentCallstack->operator[](DifferenceIndex);
				}
			}
			else if (Context.CurrentBreakpointAction == ERigVMBreakpointAction::StepInto)
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
			else if (Context.CurrentBreakpointAction == ERigVMBreakpointAction::StepOut)
			{
				if (DifferenceIndex != INDEX_NONE && DifferenceIndex <= PreviousCallstack.Num() - 2)
                {
                	NewBreakpointNode = CurrentCallstack->operator[](DifferenceIndex);
                }
			}
			
			if (NewBreakpointNode)
			{
				// Remove or hit previous breakpoint
				if (Context.DebugInfo->IsTemporaryBreakpoint(Context.DebugInfo->GetCurrentActiveBreakpoint()))
				{
					Context.DebugInfo->RemoveBreakpoint(Context.DebugInfo->GetCurrentActiveBreakpoint());
				}
				else
				{
					Context.DebugInfo->IncrementBreakpointActivationOnHit(Context.DebugInfo->GetCurrentActiveBreakpoint());
					Context.DebugInfo->HitBreakpoint(Context.DebugInfo->GetCurrentActiveBreakpoint());
				}

				// Create new temporary breakpoint
				const FRigVMBreakpoint& NewBreakpoint = Context.DebugInfo->AddBreakpoint(ContextPublicData.InstructionIndex, NewBreakpointNode, 0, true);
				Context.DebugInfo->SetBreakpointHits(NewBreakpoint, GetInstructionVisitedCount(Context, ContextPublicData.InstructionIndex));
				Context.DebugInfo->SetBreakpointActivationOnHit(NewBreakpoint, GetInstructionVisitedCount(Context, ContextPublicData.InstructionIndex));
				Context.CurrentBreakpointAction = ERigVMBreakpointAction::None;					

				Context.HaltedAtBreakpoint = NewBreakpoint;
				Context.HaltedAtBreakpointHit = Context.DebugInfo->GetBreakpointHits(Context.HaltedAtBreakpoint);
				Context.ExecutionHalted().Broadcast(ContextPublicData.InstructionIndex, NewBreakpointNode, InEventName);
		
				return true;
			}
		}
	}

	return false;
}
#endif

bool URigVM::Initialize(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory)
{
	if (Context.ExecutingThreadId != INDEX_NONE)
	{
		ensureMsgf(Context.ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::Initialize from multiple threads (%d and %d)"), Context.ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());
	}
	
	CopyDeferredVMIfRequired();

	TGuardValue<int32> GuardThreadId(Context.ExecutingThreadId, FPlatformTLS::GetCurrentThreadId());

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
						if (Context.GetPublicData<FRigVMExecuteContext>().bDebugExecution)
						{
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
						}
#endif
					}
				}
			}
		}
	}

	// make sure to update all argument name caches
	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	for(const FRigVMInstruction& Instruction : Instructions)
	{
		if(Instruction.OpCode == ERigVMOpCode::Execute)
		{
			const FRigVMExecuteOp& Op = ByteCodeStorage.GetOpAt<FRigVMExecuteOp>(Instruction);
			if(Functions.IsValidIndex(Op.FunctionIndex))
			{
				if(const FRigVMDispatchFactory* Factory = Functions[Op.FunctionIndex]->Factory)
				{
					FRigVMOperandArray Operands = ByteCodeStorage.GetOperandsForExecuteOp(Instruction);
					(void)Factory->UpdateArgumentNameCache(Operands.Num());
				}
			}
		}
	}
	
	CacheMemoryHandlesIfRequired(Context, Memory);

	return true;
}

ERigVMExecuteResult URigVM::Execute(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName)
{
	// if this the first entry being executed - get ready for execution
	const bool bIsRootEntry = Context.EntriesBeingExecuted.IsEmpty();
	ON_SCOPE_EXIT
	{
		if (bIsRootEntry)
		{
			ActiveExecutions--;
		}
	};

	TGuardValue<FName> EntryNameGuard(Context.CurrentEntryName, InEntryName);
	TGuardValue<bool> RootEntryGuard(Context.bCurrentlyRunningRootEntry, bIsRootEntry);

	if(bIsRootEntry)
	{
		Context.CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
		
		if (Context.ExecutingThreadId != INDEX_NONE)
		{
			ensureMsgf(false, TEXT("RigVM::Execute from multiple threads (%d and %d)"), Context.ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());
		}

		CopyDeferredVMIfRequired();

		ActiveExecutions++;

		TGuardValue<int32> GuardThreadId(Context.ExecutingThreadId, FPlatformTLS::GetCurrentThreadId());

		ResolveFunctionsIfRequired();
		RefreshInstructionsIfRequired();

		if (Instructions.Num() == 0)
		{
			return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
		}

		// changes to the layout of memory array should be reflected in GetContainerIndex()
		TArray<URigVMMemoryStorage*> LocalMemory;
		if (Memory.Num() == 0)
		{
			LocalMemory = GetLocalMemoryArray();
			Memory = LocalMemory;
		}
	
		CacheMemoryHandlesIfRequired(Context, Memory);
		Context.CurrentMemory = Memory;
	}

	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();

	if(bIsRootEntry)
	{
		if (Context.GetFirstEntryEventInEventQueue() == NAME_None || Context.GetFirstEntryEventInEventQueue() == InEntryName)
		{
			SetupInstructionTracking(Context, Instructions.Num());
		}
	}
#endif

	if(bIsRootEntry)
	{
		Context.ResetExecutionState();
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
			return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
		}
		
		SetInstructionIndex(Context, (uint16)ByteCode.GetEntry(EntryIndex).InstructionIndex);
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

	FEntryExecuteGuard EntryExecuteGuard(Context.EntriesBeingExecuted, EntryIndexToPush);

#if WITH_EDITOR
	if (Context.DebugInfo && bIsRootEntry)
	{
		Context.DebugInfo->StartExecution();
	}
#endif

	if(bIsRootEntry)
	{
		Context.NumExecutions++;
	}

#if WITH_EDITOR
	StartProfiling(Context);
	
#if UE_RIGVM_DEBUG_EXECUTION
	if (ContextPublicData.bDebugExecution)
	{
		ContextPublicData.InstanceOpCodeEnum = StaticEnum<ERigVMOpCode>();
		URigVMMemoryStorage* LiteralMemory = GetLiteralMemory(false);
		ContextPublicData.DebugMemoryString = FString("\n\nLiteral Memory\n\n");
		for (int32 PropertyIndex=0; PropertyIndex<LiteralMemory->Num(); ++PropertyIndex)
		{
			ContextPublicData.DebugMemoryString += FString::Printf(TEXT("%s: %s\n"), *LiteralMemory->GetProperties()[PropertyIndex]->GetFullName(), *LiteralMemory->GetDataAsString(PropertyIndex));				
		}
		ContextPublicData.DebugMemoryString += FString(TEXT("\n\nWork Memory\n\n"));
	}
	
#endif
	
#endif

	Context.CurrentExecuteResult = ExecuteInstructions(Context, ContextPublicData.InstructionIndex, Instructions.Num() - 1);

#if WITH_EDITOR
	if(bIsRootEntry)
	{
		Context.CurrentMemory = TArrayView<URigVMMemoryStorage*>();
		
		if (Context.HaltedAtBreakpoint.IsValid() && Context.CurrentExecuteResult != ERigVMExecuteResult::Halted)
		{
			Context.DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
			Context.HaltedAtBreakpoint.Reset();
			Context.ExecutionHalted().Broadcast(INDEX_NONE, nullptr, InEntryName);
		}
	}
#endif

	return Context.CurrentExecuteResult;
}

ERigVMExecuteResult URigVM::ExecuteInstructions(FRigVMExtendedExecuteContext& Context, int32 InFirstInstruction, int32 InLastInstruction)
{
	// make we are already executing this VM
	check(!Context.CurrentMemory.IsEmpty());
	
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
		if (ShouldHaltAtInstruction(Context, Context.CurrentEntryName, ContextPublicData.InstructionIndex))
		{
#if UE_RIGVM_DEBUG_EXECUTION
			if (ContextPublicData.bDebugExecution)
			{
				ContextPublicData.Log(EMessageSeverity::Info, ContextPublicData.DebugMemoryString);
			}
#endif
			// we'll recursively exit all invoked
			// entries here.
			return Context.CurrentExecuteResult = ERigVMExecuteResult::Halted;
		}

		if(Context.CurrentExecuteResult == ERigVMExecuteResult::Halted)
		{
			return Context.CurrentExecuteResult;
		}

#endif
		
		if(ContextPublicData.InstructionIndex > (uint16)InLastInstruction)
		{
			return Context.CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
		}

#if WITH_EDITOR

		const int32 CurrentInstructionIndex = ContextPublicData.InstructionIndex;
		Context.InstructionVisitedDuringLastRun[ContextPublicData.InstructionIndex]++;
		Context.InstructionVisitOrder.Add(ContextPublicData.InstructionIndex);
	
#endif

		const FRigVMInstruction& Instruction = Instructions[ContextPublicData.InstructionIndex];

#if WITH_EDITOR
#if UE_RIGVM_DEBUG_EXECUTION
		if (ContextPublicData.bDebugExecution)
		{
			if (Instruction.OpCode == ERigVMOpCode::Execute)
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
				static auto FormatFunction = [](const FString& RegisterName, const FString& RegisterOffsetName) -> FString
				{
					return FString::Printf(TEXT("%s%s"), *RegisterName, *RegisterOffsetName);
				};
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);
				ContextPublicData.DebugMemoryString += FString::Printf(TEXT("Instruction %d: Copy %s -> %s\n"), ContextPublicData.InstructionIndex, *GetOperandLabel(Op.Source, FormatFunction), *GetOperandLabel(Op.Target, FormatFunction));
			}
			else
			{
				ContextPublicData.DebugMemoryString += FString::Printf(TEXT("Instruction %d: %s\n"), ContextPublicData.InstructionIndex, *ContextPublicData.InstanceOpCodeEnum->GetNameByIndex((uint8)Instruction.OpCode).ToString());
			}
		}
#endif
#endif

		switch (Instruction.OpCode)
		{
			case ERigVMOpCode::Execute:
			{
				const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
				const int32 OperandCount = FirstHandleForInstruction[ContextPublicData.InstructionIndex + 1] - FirstHandleForInstruction[ContextPublicData.InstructionIndex];
				FRigVMMemoryHandleArray Handles;
				if(OperandCount > 0)
				{
					Handles = FRigVMMemoryHandleArray(&Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]], OperandCount);
				}

				FRigVMPredicateBranchArray Predicates;
				if (Op.PredicateCount > 0)
				{
					Predicates = FRigVMPredicateBranchArray(&ByteCode.PredicateBranches[Op.FirstPredicateIndex], Op.PredicateCount);
				}
#if WITH_EDITOR
				ContextPublicData.FunctionName = FunctionNames[Op.FunctionIndex];
#endif
				Context.Factory = Factories[Op.FunctionIndex];
				(*Functions[Op.FunctionIndex]->FunctionPtr)(Context, Handles, Predicates);

#if WITH_EDITOR

				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instruction);
					for(int32 OperandIndex = 0, HandleIndex = 0; OperandIndex < Operands.Num() && HandleIndex < Handles.Num(); HandleIndex++)
					{
						CopyOperandForDebuggingIfNeeded(Context, Operands[OperandIndex++], Handles[HandleIndex]);
					}
				}
#endif

				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Zero:
			{
				const FRigVMMemoryHandle& Handle = Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]];
				if(Handle.GetProperty()->IsA<FIntProperty>())
				{
					*((int32*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = 0;
				}
				else if(Handle.GetProperty()->IsA<FNameProperty>())
				{
					*((FName*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = NAME_None;
				}
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Context, Op.Arg, Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]]);
				}
#endif

				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::BoolFalse:
			{
				*((bool*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = false;
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::BoolTrue:
			{
				*((bool*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()) = true;
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instruction);

				FRigVMMemoryHandle& SourceHandle = Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]];
				FRigVMMemoryHandle& TargetHandle = Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex] + 1];
				URigVMMemoryStorage::CopyProperty(TargetHandle, SourceHandle);
					
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					CopyOperandForDebuggingIfNeeded(Context, Op.Source, SourceHandle);
				}
#endif
					
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Increment:
			{
				(*((int32*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()))++;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Context, Op.Arg, Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]]);
				}
#endif
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Decrement:
			{
				(*((int32*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()))--;
#if WITH_EDITOR
				if(DebugMemoryStorageObject->Num() > 0)
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instruction);
					CopyOperandForDebuggingIfNeeded(Context, Op.Arg, Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]]);
				}
#endif
				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instruction);

				FRigVMMemoryHandle& HandleA = Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]];
				FRigVMMemoryHandle& HandleB = Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex] + 1];
				const bool Result = HandleA.GetProperty()->Identical(HandleA.GetData(true), HandleB.GetData(true));

				*((bool*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]+2].GetData()) = Result;
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
				const bool Condition = *(bool*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();
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
				const bool Condition = *(bool*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();
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
				const bool Condition = *(bool*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();
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
				if(Context.bCurrentlyRunningRootEntry)
				{
					StopProfiling(Context);
					Context.ExecutionReachedExit().Broadcast(Context.CurrentEntryName);
#if WITH_EDITOR					
					if (Context.HaltedAtBreakpoint.IsValid())
					{
						Context.HaltedAtBreakpoint.Reset();
						Context.DebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
						Context.ExecutionHalted().Broadcast(INDEX_NONE, nullptr, Context.CurrentEntryName);
					}
#if UE_RIGVM_DEBUG_EXECUTION
					if (ContextPublicData.bDebugExecution)
					{
						ContextPublicData.Log(EMessageSeverity::Info, ContextPublicData.DebugMemoryString);
					}
#endif
#endif
				}
				return ERigVMExecuteResult::Succeeded;
			}
			case ERigVMOpCode::BeginBlock:
			{
				const int32 Count = (*((int32*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData()));
				const int32 Index = (*((int32*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex] + 1].GetData()));
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

				if(!CanExecuteEntry(Context, Op.EntryName))
				{
					return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
				}
				else
				{
					// this will restore the public data after invoking the entry
					TGuardValue<FRigVMExecuteContext> PublicDataGuard(ContextPublicData, ContextPublicData);
					const ERigVMExecuteResult ExecuteResult = Execute(Context, Context.CurrentMemory, Op.EntryName);
					if(ExecuteResult != ERigVMExecuteResult::Succeeded)
					{
						return Context.CurrentExecuteResult = ExecuteResult;
					}

#if WITH_EDITOR
					// if we are halted at a break point we need to exit here
					if (ShouldHaltAtInstruction(Context, Context.CurrentEntryName, ContextPublicData.InstructionIndex))
					{
						return Context.CurrentExecuteResult = ERigVMExecuteResult::Halted;
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
				FName BranchLabel = *(FName*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();

				// iterate over the branches stored in the bytecode,
				// starting at the first branch index stored in the operator.
				// look over all branches matching this instruction index and
				// find the one with the right label - then jump to the branch.
				bool bBranchFound = false;
				const TArray<FRigVMBranchInfo>& Branches = ByteCode.BranchInfos;
				if(Branches.IsEmpty())
				{
					UE_LOG(LogRigVM, Error, TEXT("No branches in ByteCode - but JumpToBranch instruction %d found. Likely a corrupt VM. Exiting."), ContextPublicData.InstructionIndex);
					return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
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
				return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
			}
		}

#if WITH_EDITOR
		if(ContextPublicData.RuntimeSettings.bEnableProfiling && !Context.InstructionVisitOrder.IsEmpty())
		{
			const uint64 EndCycles = FPlatformTime::Cycles64();
			const uint64 Cycles = EndCycles - Context.StartCycles;
			if(Context.InstructionCyclesDuringLastRun[CurrentInstructionIndex] == UINT64_MAX)
			{
				Context.InstructionCyclesDuringLastRun[CurrentInstructionIndex] = Cycles;
			}
			else
			{
				Context.InstructionCyclesDuringLastRun[CurrentInstructionIndex] += Cycles;
			}

			Context.StartCycles = EndCycles;
			Context.OverallCycles += Cycles;
		}

#if UE_RIGVM_DEBUG_EXECUTION
		if (ContextPublicData.bDebugExecution)
		{
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
		}
#endif
#endif
	}

	return Context.CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
}

bool URigVM::Execute(FRigVMExtendedExecuteContext& Context, const FName& InEntryName)
{
	return Execute(Context, TArray<URigVMMemoryStorage*>(), InEntryName) != ERigVMExecuteResult::Failed;
}

ERigVMExecuteResult URigVM::ExecuteBranch(FRigVMExtendedExecuteContext& Context, const FRigVMBranchInfo& InBranchToRun)
{
	// likely have to optimize this
	TGuardValue<FRigVMExtendedExecuteContext> ContextGuard(Context, Context);
	return ExecuteInstructions(Context, InBranchToRun.FirstInstruction, InBranchToRun.LastInstruction);
}

FRigVMExternalVariableDef URigVM::GetExternalVariableDefByName(const FName& InExternalVariableName)
{
	for (const FRigVMExternalVariableDef& ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.Name == InExternalVariableName)
		{
			return ExternalVariable;
		}
	}
	return FRigVMExternalVariableDef();
}

FRigVMExternalVariable URigVM::GetExternalVariableByName(const FRigVMExtendedExecuteContext& Context, const FName& InExternalVariableName)
{
	const int32 NumExternalVariables = ExternalVariables.Num();
	for (int i=0; i<NumExternalVariables; ++i)
	{
		const FRigVMExternalVariableDef& ExternalVariableDef = ExternalVariables[i];
		if (ExternalVariableDef.Name == InExternalVariableName)
		{
			const FRigVMExternalVariableRuntimeData& ExternalVariableData = Context.ExternalVariableRuntimeData[i];
			return FRigVMExternalVariable(ExternalVariableDef, ExternalVariableData.Memory);;
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
			case ERigVMOpCode::Execute:
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
		const FRigVMExternalVariableDef& ExternalVariable = ExternalVariables[InOperand.GetRegisterIndex()];
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

void URigVM::CacheSingleMemoryHandle(FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg, bool bForExecute)
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

		const int32 ExternalVariableIndex = InArg.GetRegisterIndex();
		FRigVMExternalVariableDef& ExternalVariable = ExternalVariables[ExternalVariableIndex];
		check(ExternalVariable.IsValid());

		FRigVMExternalVariableRuntimeData& ExternalVariableRuntimeData = Context.ExternalVariableRuntimeData[ExternalVariableIndex];
		check(ExternalVariableRuntimeData.Memory != nullptr);

		Context.CachedMemoryHandles[InHandleIndex] = {ExternalVariableRuntimeData.Memory, ExternalVariable.Property, PropertyPath};
#if UE_BUILD_DEBUG
		// make sure the handle points to valid memory
		check(Context.CachedMemoryHandles[InHandleIndex].GetData(false) != nullptr);
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
	FRigVMMemoryHandle& Handle = Context.CachedMemoryHandles[InHandleIndex];
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

void URigVM::CopyOperandForDebuggingImpl(FRigVMExtendedExecuteContext& Context, const FRigVMOperand& InArg, const FRigVMMemoryHandle& InHandle, const FRigVMOperand& InDebugOperand)
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
			const int32 ExternalVariableIndex = InArg.GetRegisterIndex();
			FRigVMExternalVariableDef& ExternalVariable = ExternalVariables[ExternalVariableIndex];
			const FProperty* SourceProperty = ExternalVariable.Property;
			FRigVMExternalVariableRuntimeData& ExternalVariableRuntimeData = Context.ExternalVariableRuntimeData[ExternalVariableIndex];
			const uint8* SourcePtr = ExternalVariableRuntimeData.Memory;
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

void URigVM::SetupInstructionTracking(FRigVMExtendedExecuteContext& Context, int32 InInstructionCount)
{
#if WITH_EDITOR
	Context.InstructionVisitedDuringLastRun.Reset();
	Context.InstructionVisitOrder.Reset();
	Context.InstructionVisitedDuringLastRun.SetNumZeroed(InInstructionCount);
	Context.InstructionCyclesDuringLastRun.Reset();

	if(Context.GetPublicData<>().RuntimeSettings.bEnableProfiling)
	{
		Context.InstructionCyclesDuringLastRun.SetNumUninitialized(InInstructionCount);
		for(int32 DurationIndex=0;DurationIndex<Context.InstructionCyclesDuringLastRun.Num();DurationIndex++)
		{
			Context.InstructionCyclesDuringLastRun[DurationIndex] = UINT64_MAX;
		}
	}
#endif
}

void URigVM::StartProfiling(FRigVMExtendedExecuteContext& Context)
{
#if WITH_EDITOR
	Context.OverallCycles = Context.StartCycles = 0;
	if(Context.GetPublicData<>().RuntimeSettings.bEnableProfiling)
	{
		Context.StartCycles = FPlatformTime::Cycles64();
	}
#endif
}

void URigVM::StopProfiling(FRigVMExtendedExecuteContext& Context)
{
#if WITH_EDITOR
	const uint64 Cycles = Context.OverallCycles > 0 ? Context.OverallCycles : (FPlatformTime::Cycles64() - Context.StartCycles); 
	Context.LastExecutionMicroSeconds = Cycles * FPlatformTime::GetSecondsPerCycle() * 1000.0 * 1000.0;
#endif
}
