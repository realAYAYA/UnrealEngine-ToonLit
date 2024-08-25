// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMNativized.h"
#include "UObject/Package.h"
#include "UObject/AnimObjectVersion.h"
#include "RigVMObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "RigVMObjectVersion.h"
#include "HAL/PlatformTLS.h"
#include "Async/ParallelFor.h"
#include "Engine/UserDefinedEnum.h"
#include "GenericPlatform/GenericPlatformSurvey.h"
#include "RigVMCore/RigVMStruct.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ObjectSaveContext.h"
#include "RigVMHost.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVM)

#if UE_RIGVM_DEBUG_EXECUTION
static TAutoConsoleVariable<int32> CVarControlRigDebugAllVMExecutions(
	TEXT("ControlRig.DebugAllVMExecutions"),
	0,
	TEXT("If nonzero we allow to copy the execution of a VM execution."),
	ECVF_Default);
#endif

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
	: ByteCodePtr(&ByteCodeStorage)
    , FunctionNamesPtr(&FunctionNamesStorage)
    , FunctionsPtr(&FunctionsStorage)
    , FactoriesPtr(&FactoriesStorage)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS // required until we eliminate ExecutionReachedExit and ExecutionHalted
URigVM::~URigVM()
{
	Reset_Internal();

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
	// The Save function has to be in sync with CopyDataForSerialization

	Ar << CachedVMHash;
	Ar << ExternalPropertyPathDescriptions;
	Ar << FunctionNamesStorage;
	Ar << ByteCodeStorage;
	Ar << Parameters;

	Ar << OperandToDebugRegisters;
	
	Ar << LiteralMemoryStorage;
	Ar << DefaultWorkMemoryStorage;
	Ar << DefaultDebugMemoryStorage;
}

void URigVM::Load(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FRigVMObjectVersion::GUID);
	
	Reset_Internal();

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
			FRigVMMemoryContainer TmpWorkMemoryStorage;
			FRigVMMemoryContainer TmpLiteralMemoryStorage;

			Ar << TmpWorkMemoryStorage;
			Ar << TmpLiteralMemoryStorage;
			Ar << FunctionNamesStorage;
			Ar << ByteCodeStorage;
			Ar << Parameters;

			if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMCopyOpStoreNumBytes)
			{
				Reset_Internal();
				return;
			}

			if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::RigVMSaveDebugMapInGraphFunctionData ||
			    Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::RigVMSaveDebugMapInGraphFunctionData)
			{
				Ar << OperandToDebugRegisters;
			}
			
			if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::VMStoringUserDefinedStructMap 
				&& Ar.CustomVer(FRigVMObjectVersion::GUID) < FRigVMObjectVersion::HostStoringUserDefinedData)
			{	
				// now serialized at Host
				TMap<FString, FSoftObjectPath> UserDefinedStructGuidToPathName;
				Ar << UserDefinedStructGuidToPathName;
			}
			if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::VMStoringUserDefinedEnumMap
				&& Ar.CustomVer(FRigVMObjectVersion::GUID) < FRigVMObjectVersion::HostStoringUserDefinedData)
			{
				// now serialized at Host
				TMap<FString, FSoftObjectPath> UserDefinedEnumToPathName;
				Ar << UserDefinedEnumToPathName;
			}
		}

		// we only deal with virtual machines now that use the new memory infrastructure.
		ensure(UE_RIGVM_UCLASS_BASED_STORAGE_DISABLED == 0);
		if (RigVMUClassBasedStorageDefine != UE_RIGVM_UCLASS_BASED_STORAGE_DISABLED)
		{
			Reset_Internal();
			return;
		}
	}

	if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::AddedVMHashChecks)
	{
		Ar << CachedVMHash;
	}
	Ar << ExternalPropertyPathDescriptions;
	Ar << FunctionNamesStorage;
	Ar << ByteCodeStorage;
	ByteCodeStorage.AlignByteCode();
	Ar << Parameters;

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::RigVMSaveDebugMapInGraphFunctionData ||
		Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::RigVMSaveDebugMapInGraphFunctionData)
	{
		Ar << OperandToDebugRegisters;
	}
		
	if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::VMStoringUserDefinedStructMap
		&& Ar.CustomVer(FRigVMObjectVersion::GUID) < FRigVMObjectVersion::HostStoringUserDefinedData)
	{
		// now serialized at Host
		TMap<FString, FSoftObjectPath> UserDefinedStructGuidToPathName;
		Ar << UserDefinedStructGuidToPathName;
	}
	if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::VMStoringUserDefinedEnumMap
		&& Ar.CustomVer(FRigVMObjectVersion::GUID) < FRigVMObjectVersion::HostStoringUserDefinedData)
	{
		// now serialized at Host
		TMap<FString, FSoftObjectPath> UserDefinedEnumToPathName;
		Ar << UserDefinedEnumToPathName;
	}

	if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::VMMemoryStorageStructSerialized)
	{
		Ar << LiteralMemoryStorage;
	}

	if (Ar.CustomVer(FRigVMObjectVersion::GUID) >= FRigVMObjectVersion::VMMemoryStorageDefaultsGeneratedAtVM)
	{
		Ar << DefaultWorkMemoryStorage;
		Ar << DefaultDebugMemoryStorage;
	}
}

void URigVM::CopyDataForSerialization(URigVM* InVM)
{
	check(InVM);
	check(ActiveExecutions.load() == 0);

	Reset_Internal();

	CachedVMHash = InVM->CachedVMHash;
	ExternalPropertyPathDescriptions = InVM->ExternalPropertyPathDescriptions;
	FunctionNamesStorage = InVM->FunctionNamesStorage;
	ByteCodeStorage = InVM->ByteCodeStorage;
	Parameters = InVM->Parameters;

	OperandToDebugRegisters = InVM->OperandToDebugRegisters;

	LiteralMemoryStorage = InVM->LiteralMemoryStorage;
	DefaultWorkMemoryStorage = InVM->DefaultWorkMemoryStorage;
	DefaultDebugMemoryStorage = InVM->DefaultDebugMemoryStorage;
}

void URigVM::PostLoad()
{
	Super::PostLoad();

	// In packaged builds, initialize the CDO VM
	// In editor, the VM will be recompiled and initialized at URigVMBlueprint::HandlePackageDone::RecompileVM
#if WITH_EDITOR
	if (GetPackage()->bIsCookedForEditor)
#endif
	{
		Instructions.Reset();
		FunctionsStorage.Reset();
		FactoriesStorage.Reset();
		ParametersNameMap.Reset();

		for (int32 Index = 0; Index < Parameters.Num(); Index++)
		{
			ParametersNameMap.Add(Parameters[Index].Name, Index);
		}

		// Rebuild functions storage from serialized function names
		ResolveFunctionsIfRequired();

		// Rebuild instructions from ByteCodeStorage
		RefreshInstructionsIfRequired();

		// rebuild the bytecode to adjust for byte shifts in shipping
		RebuildByteCodeOnLoad();

		// rebuild the argument name cache, so it is already calculated during init
		RefreshArgumentNameCaches();
	}
}

void URigVM::RefreshArgumentNameCaches()
{
	// make sure to update all argument name caches
	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	for (const FRigVMInstruction& Instruction : Instructions)
	{
		if (Instruction.OpCode == ERigVMOpCode::Execute)
		{
			const FRigVMExecuteOp& Op = ByteCodeStorage.GetOpAt<FRigVMExecuteOp>(Instruction);
			if (Functions.IsValidIndex(Op.FunctionIndex))
			{
				if (const FRigVMDispatchFactory* Factory = Functions[Op.FunctionIndex]->Factory)
				{
					FRigVMOperandArray Operands = ByteCodeStorage.GetOperandsForExecuteOp(Instruction);
					(void)Factory->UpdateArgumentNameCache(Operands.Num());
				}
			}
		}
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

	Hash = HashCombine(Hash, LiteralMemoryStorage.GetMemoryHash());
	Hash = HashCombine(Hash, DefaultWorkMemoryStorage.GetMemoryHash());
	
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

bool URigVM::ValidateBytecode()
{
	// check all operands on all ops for validity
	const TArray<const FRigVMMemoryStorageStruct*> LocalMemory = { &GetDefaultWorkMemory(), &GetDefaultLiteralMemory(), &GetDefaultDebugMemory() };
	
	auto CheckOperandValidity = [LocalMemory, this](const FRigVMOperand& InOperand) -> bool
	{
		if(InOperand.GetContainerIndex() < 0 || InOperand.GetContainerIndex() >= (int32)ERigVMMemoryType::Invalid)
		{
			return false;
		}


		const FRigVMMemoryStorageStruct* MemoryForOperand = LocalMemory[InOperand.GetContainerIndex()];
		if(InOperand.GetMemoryType() != ERigVMMemoryType::External)
		{
			if(!MemoryForOperand->IsValidIndex(InOperand.GetRegisterIndex()))
			{
				return false;
			}

			if(InOperand.GetRegisterOffset() != INDEX_NONE)
			{
				if(!MemoryForOperand->GetPropertyPaths().IsValidIndex(InOperand.GetRegisterOffset()))
				{
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
					return false;
				}
			}
		}
		return true;
	};

	const TArray<const FRigVMFunction*>& Functions = GetFunctions();
	
	const FRigVMInstructionArray ByteCodeInstructions = ByteCodeStorage.GetInstructions();
	for(const FRigVMInstruction& ByteCodeInstruction : ByteCodeInstructions)
	{
		switch (ByteCodeInstruction.OpCode)
		{
			case ERigVMOpCode::Execute:
			{
				const FRigVMExecuteOp& Op = ByteCodeStorage.GetOpAt<FRigVMExecuteOp>(ByteCodeInstruction);
				if (!Functions.IsValidIndex(Op.FunctionIndex))
				{
					return false;
				}
				FRigVMOperandArray Operands = ByteCodeStorage.GetOperandsForExecuteOp(ByteCodeInstruction);
				for (const FRigVMOperand& Arg : Operands)
				{
					if (!CheckOperandValidity(Arg))
					{
						return false;
					}
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
				if (!CheckOperandValidity(Op.Arg))
				{
					return false;
				}
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCodeStorage.GetOpAt<FRigVMCopyOp>(ByteCodeInstruction);
				if (!CheckOperandValidity(Op.Source) ||
					!CheckOperandValidity(Op.Target))
				{
					return false;
				}
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCodeStorage.GetOpAt<FRigVMComparisonOp>(ByteCodeInstruction);
				if (!CheckOperandValidity(Op.A) ||
					!CheckOperandValidity(Op.B) ||
					!CheckOperandValidity(Op.Result))
				{
					return false;
				}
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCodeStorage.GetOpAt<FRigVMJumpIfOp>(ByteCodeInstruction);
				if (!CheckOperandValidity(Op.Arg))
				{
					return false;
				}
				break;
			}
			case ERigVMOpCode::BeginBlock:
			{
				const FRigVMBinaryOp& Op = ByteCodeStorage.GetOpAt<FRigVMBinaryOp>(ByteCodeInstruction);
				if (!CheckOperandValidity(Op.ArgA) ||
					!CheckOperandValidity(Op.ArgB))
				{
					return false;
				}
				break;
			}
			case ERigVMOpCode::JumpToBranch:
			{
				const FRigVMJumpToBranchOp& Op = ByteCodeStorage.GetOpAt<FRigVMJumpToBranchOp>(ByteCodeInstruction);
				if (!CheckOperandValidity(Op.Arg))
				{
					return false;
				}
				break;
			}
			case ERigVMOpCode::RunInstructions:
			{
				const FRigVMRunInstructionsOp& Op = ByteCodeStorage.GetOpAt<FRigVMRunInstructionsOp>(ByteCodeInstruction);
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

	return true;
}

const URigVMHost* URigVM::GetHostCDO() const
{
	return GetTypedOuter<const URigVMHost>();
}

void URigVM::Reset_Internal()
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

	FunctionNamesPtr = &FunctionNamesStorage;
	FunctionsPtr = &FunctionsStorage;
	FactoriesPtr = &FactoriesStorage;
	ByteCodePtr = &ByteCodeStorage;

	ExternalVariables.Reset();
	LazyBranches.Reset();

	InvalidateCachedMemory_Internal();
}

void URigVM::Reset(FRigVMExtendedExecuteContext& Context)
{
	Reset_Internal();
	InvalidateCachedMemory(Context);
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

	InvalidateCachedMemory(Context);

	OperandToDebugRegisters.Empty();

	Context.CachedMemoryHandles.Empty();
	Context.ExternalVariableRuntimeData.Reset();
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

FRigVMMemoryStorageStruct* URigVM::GetMemoryByType(FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType/*, bool bCreateIfNeeded*/)
{
	FRigVMMemoryStorageStruct* MemoryStorage = nullptr;

	switch(InMemoryType)
	{
		case ERigVMMemoryType::Literal:
		{
			MemoryStorage = GetLiteralMemory();
			break;
		}

		case ERigVMMemoryType::Work:
		{
			MemoryStorage = &Context.WorkMemoryStorage;
			break;
		}

		case ERigVMMemoryType::Debug:
		{
			MemoryStorage = &Context.DebugMemoryStorage;
			break;
		}

		default:
		{
			break;
		}
	}

	return MemoryStorage;
}

const FRigVMMemoryStorageStruct* URigVM::GetMemoryByType(const FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType) const
{
	return const_cast<URigVM*>(this)->GetMemoryByType(const_cast<FRigVMExtendedExecuteContext&>(Context), InMemoryType/*, false*/);
}

void URigVM::GenerateMemoryType(FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType, const TArray<FRigVMPropertyDescription>* InProperties)
{
	GenerateDefaultMemoryType(InMemoryType, InProperties);

	switch (InMemoryType)
	{
	case ERigVMMemoryType::Work:
	{
		Context.WorkMemoryStorage = DefaultWorkMemoryStorage;
		break;
	}

	case ERigVMMemoryType::Debug:
	{
#if WITH_EDITOR
		Context.DebugMemoryStorage = DefaultDebugMemoryStorage;
#endif
		break;
	}

	default:
	{
		break;
	}
	}
}

void URigVM::GenerateDefaultMemoryType(ERigVMMemoryType InMemoryType, const TArray<FRigVMPropertyDescription>* InProperties)
{
	FRigVMMemoryStorageStruct* Memory = nullptr;

	switch (InMemoryType)
	{
		case ERigVMMemoryType::Literal:
		{
			Memory = &LiteralMemoryStorage;
			break;
		}

		case ERigVMMemoryType::Work:
		{
			Memory = &DefaultWorkMemoryStorage;
			break;
		}

#if WITH_EDITOR
		case ERigVMMemoryType::Debug:
		{
			Memory = &DefaultDebugMemoryStorage;
			break;
		}
#endif

		default:
		{
			break;
		}
	}

	if (Memory != nullptr)
	{
		*Memory = (InProperties != nullptr) ? FRigVMMemoryStorageStruct(InMemoryType, *InProperties) : FRigVMMemoryStorageStruct(InMemoryType);
	}
}

FRigVMMemoryStorageStruct* URigVM::GetDefaultMemoryByType(ERigVMMemoryType InMemoryType)
{
	FRigVMMemoryStorageStruct* MemoryStorage = nullptr;

	switch (InMemoryType)
	{
		case ERigVMMemoryType::Literal:
		{
			MemoryStorage = &LiteralMemoryStorage;
			break;
		}

		case ERigVMMemoryType::Work:
		{
			MemoryStorage = &DefaultWorkMemoryStorage;
			break;
		}

		case ERigVMMemoryType::Debug:
		{
#if WITH_EDITOR
			MemoryStorage = &DefaultDebugMemoryStorage;
#endif
			break;
		}

		default:
		{
			break;
		}
	}

	return MemoryStorage;
}

const FRigVMMemoryStorageStruct* URigVM::GetDefaultMemoryByType(ERigVMMemoryType InMemoryType) const
{
	return const_cast<URigVM*>(this)->GetDefaultMemoryByType(InMemoryType/*, false*/);
}

void URigVM::ClearMemory_Internal()
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
		GetObjectsWithOuter(GetOuter(), SubObjects);
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

	LiteralMemoryStorage = FRigVMMemoryStorageStruct(ERigVMMemoryType::Invalid);
	DefaultWorkMemoryStorage = FRigVMMemoryStorageStruct(ERigVMMemoryType::Invalid);
#if WITH_EDITOR
	DefaultDebugMemoryStorage = FRigVMMemoryStorageStruct(ERigVMMemoryType::Invalid);
#endif

	InvalidateCachedMemory_Internal();
}

void URigVM::ClearMemory(FRigVMExtendedExecuteContext& Context)
{
	ClearMemory_Internal();

	Context.WorkMemoryStorage = FRigVMMemoryStorageStruct(ERigVMMemoryType::Invalid);
	Context.DebugMemoryStorage = FRigVMMemoryStorageStruct(ERigVMMemoryType::Invalid);

	InvalidateCachedMemory(Context);
}

const FRigVMInstructionArray& URigVM::GetInstructions()
{
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

bool URigVM::ResumeExecution(FRigVMExtendedExecuteContext& Context, const FName& InEntryName)
{
	if (FRigVMDebugInfo* RigVMDebugInfo = Context.GetRigVMDebugInfo())
	{
		RigVMDebugInfo->ResumeExecution();
	}

	return ExecuteVM(Context, InEntryName) != ERigVMExecuteResult::Failed;
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

		FRigVMUserDefinedTypeResolver TypeResolver;
		if (const URigVMHost* HostCDO = GetHostCDO())
		{
			TypeResolver = FRigVMUserDefinedTypeResolver([HostCDO](const FString& InTypeName) -> UObject*
			{
				return HostCDO->ResolveUserDefinedTypeById(InTypeName);
			});
		}

		TArray<FName>& FunctionNames = GetFunctionNames();
		for (int32 FunctionIndex = 0; FunctionIndex < FunctionNames.Num(); FunctionIndex++)
		{
			const FString FunctionNameString = FunctionNames[FunctionIndex].ToString();
			if(const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*FunctionNameString, TypeResolver))
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

void URigVM::InvalidateCachedMemory_Internal()
{
	FirstHandleForInstruction.Reset();
	MemoryHandleCount = 0;
	ExternalPropertyPaths.Reset();
	LazyBranches.Reset();
}

void URigVM::InvalidateCachedMemory(FRigVMExtendedExecuteContext& Context)
{
	InvalidateCachedMemory_Internal();
	Context.InvalidateCachedMemory();
}

void URigVM::InstructionOpEval(FRigVMExtendedExecuteContext& Context, int32 InstructionIndex, int32 InHandleBaseIndex, const TFunctionRef<void(FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg)>& InOpFunc)
{
	const FRigVMByteCode& ByteCode = GetByteCode();
	const TArray<const FRigVMFunction*>& Functions = GetFunctions();

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
				{ InstructionIndex, ArgIndex, Function->GetArgumentNameForOperandIndex(ArgIndex, Operands.Num()) },
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
		case ERigVMOpCode::RunInstructions:
		{
			const FRigVMRunInstructionsOp& Op = ByteCode.GetOpAt<FRigVMRunInstructionsOp>(Instructions[InstructionIndex]);
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
}

void URigVM::PrepareMemoryForExecution(FRigVMExtendedExecuteContext& Context)
{
	ensureMsgf(Context.ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::CacheMemoryHandlesIfRequired from multiple threads (%d and %d)"), Context.ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());

	InvalidateCachedMemory(Context);

	if (Instructions.Num() == 0)
	{
		return;
	}

	RefreshExternalPropertyPaths();

	FRigVMByteCode& ByteCode = GetByteCode();

	// force to update the map of branch infos once
	(void)ByteCode.GetBranchInfo({ 0, 0 });

	const int32 LazyBranchSize = GetByteCode().BranchInfos.Num();
	LazyBranches.Reset(LazyBranchSize);
	LazyBranches.SetNumZeroed(LazyBranchSize);

	// Make sure we have enough room to prevent repeated allocations.
	FirstHandleForInstruction.Reset(Instructions.Num() + 1);

	// Count how many handles we need and set up the indirection offsets for the handles.
	int32 HandleCount = 0;
	FirstHandleForInstruction.Add(0);
	for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		InstructionOpEval(Context, InstructionIndex, INDEX_NONE,
			[&HandleCount](FRigVMExtendedExecuteContext& Context, int32, const FRigVMBranchInfoKey&, const FRigVMOperand&)
			{
				HandleCount++;
			});
		FirstHandleForInstruction.Add(HandleCount);
	}

	MemoryHandleCount = HandleCount;
	Context.CachedMemoryHandles.Reset(MemoryHandleCount);
}

void URigVM::CacheMemoryHandlesIfRequired(FRigVMExtendedExecuteContext& Context)
{
	if (Instructions.Num() == 0)
	{
		return;
	}

	check((Instructions.Num() + 1) == FirstHandleForInstruction.Num());
	
	if (Context.CachedMemoryHandles.Num() == MemoryHandleCount)
	{
		return;
	}

	// Allocate all the space and zero it out to ensure all pages required for it are paged in immediately.
	Context.CachedMemoryHandles.SetNumUninitialized(MemoryHandleCount);
	
	// Now cache the handles as needed.
	ParallelFor(Instructions.Num(),	[&](int32 InstructionIndex)
		{
			InstructionOpEval(Context, InstructionIndex, FirstHandleForInstruction[InstructionIndex],
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
	FRigVMDebugInfo* RigVMDebugInfo = Context.GetRigVMDebugInfo();

	if (RigVMDebugInfo == nullptr)
	{
		return false;
	}

	if(RigVMDebugInfo->IsEmpty())
	{
		return false;
	}
	
	FRigVMByteCode& ByteCode = GetByteCode();
	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();

	TArray<FRigVMBreakpoint> BreakpointsAtInstruction = RigVMDebugInfo->FindBreakpointsAtInstruction(InstructionIndex);
	for (FRigVMBreakpoint Breakpoint : BreakpointsAtInstruction)
	{
		if (RigVMDebugInfo->IsActive(Breakpoint))
		{
			switch (RigVMDebugInfo->GetCurrentBreakpointAction())
			{
				case ERigVMBreakpointAction::None:
				{
					// Halted at breakpoint. Check if this is a new breakpoint different from the previous halt.
					if (RigVMDebugInfo->GetHaltedAtBreakpoint() != Breakpoint ||
						RigVMDebugInfo->GetHaltedAtBreakpointHit() != RigVMDebugInfo->GetBreakpointHits(Breakpoint))
					{
						RigVMDebugInfo->SetHaltedAtBreakpoint(Breakpoint);
						RigVMDebugInfo->SetHaltedAtBreakpointHit(RigVMDebugInfo->GetBreakpointHits(Breakpoint));
						RigVMDebugInfo->SetCurrentActiveBreakpoint(Breakpoint);
						
						// We want to keep the callstack up to the node that produced the halt
						const TArray<TWeakObjectPtr<UObject>>* FullCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
						if (FullCallstack)
						{
							RigVMDebugInfo->SetCurrentActiveBreakpointCallstack(TArray<TWeakObjectPtr<UObject>>(FullCallstack->GetData(), FullCallstack->Find((TWeakObjectPtr<UObject>)Breakpoint.Subject)+1));
						}
						UObject* BreakpointNode = Breakpoint.Subject.IsValid() ? Breakpoint.Subject.Get() : nullptr;
						RigVMDebugInfo->ExecutionHalted().Broadcast(ContextPublicData.InstructionIndex, BreakpointNode, InEventName);
					}
					return true;
				}
				case ERigVMBreakpointAction::Resume:
				{
					RigVMDebugInfo->SetCurrentBreakpointAction(ERigVMBreakpointAction::None);

					if (RigVMDebugInfo->IsTemporaryBreakpoint(Breakpoint))
					{
						RigVMDebugInfo->RemoveBreakpoint(Breakpoint);
					}
					else
					{
						RigVMDebugInfo->IncrementBreakpointActivationOnHit(Breakpoint);
						RigVMDebugInfo->HitBreakpoint(Breakpoint);
					}
					break;
				}
				case ERigVMBreakpointAction::StepOver:
				case ERigVMBreakpointAction::StepInto:
				case ERigVMBreakpointAction::StepOut:
				{
					// If we are stepping, check if we were halted at the current instruction, and remember it 
					if (!RigVMDebugInfo->GetCurrentActiveBreakpoint())
					{
						RigVMDebugInfo->SetCurrentActiveBreakpoint(Breakpoint);
						const TArray<TWeakObjectPtr<UObject>>* FullCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
						
						// We want to keep the callstack up to the node that produced the halt
						if (FullCallstack)
						{
							RigVMDebugInfo->SetCurrentActiveBreakpointCallstack(TArray<TWeakObjectPtr<UObject>>(FullCallstack->GetData(), FullCallstack->Find((TWeakObjectPtr<UObject>)RigVMDebugInfo->GetCurrentActiveBreakpoint().Subject)+1));
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
			RigVMDebugInfo->HitBreakpoint(Breakpoint);
		}
	}

	// If we are stepping, and the last active breakpoint was set, check if this is the new temporary breakpoint
	if (RigVMDebugInfo->GetCurrentBreakpointAction() != ERigVMBreakpointAction::None && RigVMDebugInfo->GetCurrentActiveBreakpoint())
	{
		const TArray<TWeakObjectPtr<UObject>>* CurrentCallstack = ByteCode.GetCallstackForInstruction(ContextPublicData.InstructionIndex);
		if (CurrentCallstack && !CurrentCallstack->IsEmpty())
		{
			UObject* NewBreakpointNode = nullptr;

			// Find the first difference in the callstack
			int32 DifferenceIndex = INDEX_NONE;
			TArray<TWeakObjectPtr<UObject>>& PreviousCallstack = RigVMDebugInfo->GetCurrentActiveBreakpointCallstack();
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

			if (RigVMDebugInfo->GetCurrentBreakpointAction() == ERigVMBreakpointAction::StepOver)
			{
				if (DifferenceIndex != INDEX_NONE)
				{
					if ((*CurrentCallstack)[DifferenceIndex].IsValid())
					{
						NewBreakpointNode = (*CurrentCallstack)[DifferenceIndex].Get();
					}
				}
			}
			else if (RigVMDebugInfo->GetCurrentBreakpointAction() == ERigVMBreakpointAction::StepInto)
			{
				if (DifferenceIndex == INDEX_NONE)
				{
					if (!CurrentCallstack->IsEmpty() && !PreviousCallstack.IsEmpty() && CurrentCallstack->Last() != PreviousCallstack.Last())
					{
						const int32 MinIndex = FMath::Min(PreviousCallstack.Num(), CurrentCallstack->Num()-1);
						if ((*CurrentCallstack)[MinIndex].IsValid())
						{
							NewBreakpointNode = (*CurrentCallstack)[MinIndex].Get();
						}
					}
				}
				else
				{
					if ((*CurrentCallstack)[DifferenceIndex].IsValid())
					{
						NewBreakpointNode = (*CurrentCallstack)[DifferenceIndex].Get();
					}
				}
			}
			else if (RigVMDebugInfo->GetCurrentBreakpointAction() == ERigVMBreakpointAction::StepOut)
			{
				if (DifferenceIndex != INDEX_NONE && DifferenceIndex <= PreviousCallstack.Num() - 2)
                {
					if ((*CurrentCallstack)[DifferenceIndex].IsValid())
					{
						NewBreakpointNode = (*CurrentCallstack)[DifferenceIndex].Get();
					}
                }
			}
			
			if (NewBreakpointNode)
			{
				// Remove or hit previous breakpoint
				if (RigVMDebugInfo->IsTemporaryBreakpoint(RigVMDebugInfo->GetCurrentActiveBreakpoint()))
				{
					RigVMDebugInfo->RemoveBreakpoint(RigVMDebugInfo->GetCurrentActiveBreakpoint());
				}
				else
				{
					RigVMDebugInfo->IncrementBreakpointActivationOnHit(RigVMDebugInfo->GetCurrentActiveBreakpoint());
					RigVMDebugInfo->HitBreakpoint(RigVMDebugInfo->GetCurrentActiveBreakpoint());
				}

				// Create new temporary breakpoint
				const FRigVMBreakpoint& NewBreakpoint = RigVMDebugInfo->AddBreakpoint(ContextPublicData.InstructionIndex, NewBreakpointNode, 0, true);
				RigVMDebugInfo->SetBreakpointHits(NewBreakpoint, GetInstructionVisitedCount(Context, ContextPublicData.InstructionIndex));
				RigVMDebugInfo->SetBreakpointActivationOnHit(NewBreakpoint, GetInstructionVisitedCount(Context, ContextPublicData.InstructionIndex));
				RigVMDebugInfo->SetCurrentBreakpointAction(ERigVMBreakpointAction::None);

				RigVMDebugInfo->SetHaltedAtBreakpoint(NewBreakpoint);
				RigVMDebugInfo->SetHaltedAtBreakpointHit(RigVMDebugInfo->GetBreakpointHits(RigVMDebugInfo->GetHaltedAtBreakpoint()));
				RigVMDebugInfo->ExecutionHalted().Broadcast(ContextPublicData.InstructionIndex, NewBreakpointNode, InEventName);
		
				return true;
			}
		}
	}

	return false;
}
#endif

bool URigVM::Initialize(FRigVMExtendedExecuteContext& Context)
{
	if (Context.ExecutingThreadId != INDEX_NONE)
	{
		ensureMsgf(Context.ExecutingThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("RigVM::Initialize from multiple threads (%d and %d)"), Context.ExecutingThreadId, (int32)FPlatformTLS::GetCurrentThreadId());
	}
	
	TGuardValue<int32> GuardThreadId(Context.ExecutingThreadId, FPlatformTLS::GetCurrentThreadId());

	ResolveFunctionsIfRequired();
	RefreshInstructionsIfRequired();

	if (Instructions.Num() == 0)
	{
		return true;
	}

	RefreshArgumentNameCaches();

	PrepareMemoryForExecution(Context);

	return true;
}

bool URigVM::InitializeInstance(FRigVMExtendedExecuteContext& Context, bool bCopyMemory)
{
	TGuardValue<int32> GuardThreadId(Context.ExecutingThreadId, FPlatformTLS::GetCurrentThreadId());

	Context.CachedMemoryHandles.Reset(MemoryHandleCount);

	const int32 LazyBranchSize = GetByteCode().BranchInfos.Num();
	Context.LazyBranchExecuteState.Reset(LazyBranchSize);
	Context.LazyBranchExecuteState.SetNumZeroed(LazyBranchSize);

	if (bCopyMemory)
	{
		Context.VMHash = GetVMHash();
		Context.WorkMemoryStorage = DefaultWorkMemoryStorage;
#if WITH_EDITOR
		Context.DebugMemoryStorage = DefaultDebugMemoryStorage;
#endif // WITH_EDITOR
	}

	CacheMemoryHandlesIfRequired(Context);

	return true;
}

ERigVMExecuteResult URigVM::ExecuteVM(FRigVMExtendedExecuteContext& Context, const FName& InEntryName)
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

	FRigVMByteCode& ByteCode = GetByteCode();

	if(bIsRootEntry)
	{
		Context.CurrentExecuteResult = ERigVMExecuteResult::Succeeded;
		
		ActiveExecutions++;

		if (Instructions.Num() == 0)
		{
			return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
		}

		Context.CurrentVMMemory = GetInstanceMemory(Context);

		if (ByteCode.HasPublicContextPathName())
		{
			const FString ContextPublicDataPathName = Context.GetContextPublicDataStruct() != nullptr ? Context.GetContextPublicDataStruct()->GetPathName() : FString();
			if (ByteCode.GetPublicContextPathName().IsEmpty() || ByteCode.GetPublicContextPathName() != ContextPublicDataPathName)
			{
				UE_LOG(LogRigVM, Error, TEXT("Context PublicData [%s] does not match ByteCode Public Data [%s]. Likely a corrupt VM. Exiting."), *ContextPublicDataPathName, *ByteCode.GetPublicContextPathName());
				return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
			}
		}
	}


	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();

	if(bIsRootEntry)
	{
		if (FRigVMInstructionVisitInfo* RigVMInstructionVisitInfo = Context.GetRigVMInstructionVisitInfo())
		{
			if (RigVMInstructionVisitInfo->GetFirstEntryEventInEventQueue() == NAME_None || RigVMInstructionVisitInfo->GetFirstEntryEventInEventQueue() == InEntryName)
			{
				RigVMInstructionVisitInfo->SetupInstructionTracking(Instructions.Num());
				if (FRigVMProfilingInfo* RigVMProfilingInfo = Context.GetRigVMProfilingInfo())
				{
					RigVMProfilingInfo->SetupInstructionTracking(Instructions.Num(), true);
				}
			}
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
		ClearDebugMemory(Context);
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
	if (bIsRootEntry)
	{
		if (FRigVMDebugInfo* RigVMDebugInfo = Context.GetRigVMDebugInfo())
		{
			RigVMDebugInfo->StartExecution();
		}
	}
#endif

	if(bIsRootEntry)
	{
		ContextPublicData.NumExecutions++;
	}

#if WITH_EDITOR
	if(Context.bCurrentlyRunningRootEntry)
	{
		StartProfiling(Context);
	}
	
#if UE_RIGVM_DEBUG_EXECUTION
	if (CVarControlRigDebugAllVMExecutions->GetBool() || ContextPublicData.bDebugExecution)
	{
		ContextPublicData.InstanceOpCodeEnum = StaticEnum<ERigVMOpCode>();
		FRigVMMemoryStorageStruct* LiteralMemory = GetLiteralMemory();
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
		Context.CurrentVMMemory = TArrayView<FRigVMMemoryStorageStruct*>();
		
		if (FRigVMDebugInfo* RigVMDebugInfo = Context.GetRigVMDebugInfo())
		{
			if (RigVMDebugInfo->GetHaltedAtBreakpoint().IsValid() && Context.CurrentExecuteResult != ERigVMExecuteResult::Halted)
			{
				RigVMDebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
				RigVMDebugInfo->GetHaltedAtBreakpoint().Reset();
				RigVMDebugInfo->ExecutionHalted().Broadcast(INDEX_NONE, nullptr, InEntryName);
			}
		}
	}
#endif

	return Context.CurrentExecuteResult;
}

ERigVMExecuteResult URigVM::ExecuteInstructions(FRigVMExtendedExecuteContext& Context, int32 InFirstInstruction, int32 InLastInstruction)
{
	// make we are already executing this VM
	check(!Context.CurrentVMMemory.IsEmpty());

	FRigVMExecuteContext& ContextPublicData = Context.GetPublicData<>();
	TGuardValue<uint16> InstructionIndexGuard(ContextPublicData.InstructionIndex, (uint16)InFirstInstruction);
#if WITH_EDITOR
	FInstructionBracketGuard InstructionBracket(Context, InFirstInstruction, InLastInstruction);
	if(!InstructionBracket.ErrorMessage.IsEmpty())
	{
		Context.GetPublicDataSafe<>().Log(EMessageSeverity::Error, InstructionBracket.ErrorMessage);
		return ERigVMExecuteResult::Failed;
	}
#endif
	
	FRigVMByteCode& ByteCode = GetByteCode();
	TArray<const FRigVMFunction*>& Functions = GetFunctions();
	TArray<const FRigVMDispatchFactory*>& Factories = GetFactories();

#if WITH_EDITOR
	TArray<FName>& FunctionNames = GetFunctionNames();
	FRigVMInstructionVisitInfo* RigVMInstructionVisitInfo = Context.GetRigVMInstructionVisitInfo();
	FRigVMDebugInfo* RigVMDebugInfo = Context.GetRigVMDebugInfo();
	FRigVMProfilingInfo* RigVMProfilingInfo = Context.GetRigVMProfilingInfo();
#endif
	
	while (Instructions.IsValidIndex(ContextPublicData.InstructionIndex))
	{
#if WITH_EDITOR
		if (ShouldHaltAtInstruction(Context, Context.CurrentEntryName, ContextPublicData.InstructionIndex))
		{
#if UE_RIGVM_DEBUG_EXECUTION
			if (CVarControlRigDebugAllVMExecutions->GetBool() || ContextPublicData.bDebugExecution)
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

		if (RigVMInstructionVisitInfo != nullptr)
		{
			RigVMInstructionVisitInfo->SetInstructionVisitedDuringLastRun(ContextPublicData.InstructionIndex);
			RigVMInstructionVisitInfo->AddInstructionIndexToVisitOrder(ContextPublicData.InstructionIndex);
		}
	
#endif

		const FRigVMInstruction& Instruction = Instructions[ContextPublicData.InstructionIndex];

#if WITH_EDITOR
#if UE_RIGVM_DEBUG_EXECUTION
		if (CVarControlRigDebugAllVMExecutions->GetBool() || ContextPublicData.bDebugExecution)
		{
			if (Instruction.OpCode == ERigVMOpCode::Execute)
			{
				const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
				FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instructions[ContextPublicData.InstructionIndex]);

				TArray<FString> Labels;
				for (const FRigVMOperand& Operand : Operands)
				{
					Labels.Add(GetOperandLabel(Context, Operand));
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
				ContextPublicData.DebugMemoryString += FString::Printf(TEXT("Instruction %d: Copy %s -> %s\n"), ContextPublicData.InstructionIndex, *GetOperandLabel(Context, Op.Source, FormatFunction), *GetOperandLabel(Context, Op.Target, FormatFunction));
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
				if(GetDebugMemory(Context) && GetDebugMemory(Context)->Num() > 0)
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
				if(GetDebugMemory(Context) && GetDebugMemory(Context)->Num() > 0)
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
				FRigVMMemoryStorageStruct::CopyProperty(TargetHandle, SourceHandle);
					
#if WITH_EDITOR
				if(GetDebugMemory(Context) && GetDebugMemory(Context)->Num() > 0)
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
				if(GetDebugMemory(Context) && GetDebugMemory(Context)->Num() > 0)
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
				if(GetDebugMemory(Context) && GetDebugMemory(Context)->Num() > 0)
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
					if (RigVMDebugInfo != nullptr)
					{
						if (RigVMDebugInfo->GetHaltedAtBreakpoint().IsValid())
						{
							RigVMDebugInfo->GetHaltedAtBreakpoint().Reset();
							RigVMDebugInfo->SetCurrentActiveBreakpoint(FRigVMBreakpoint());
							RigVMDebugInfo->ExecutionHalted().Broadcast(INDEX_NONE, nullptr, Context.CurrentEntryName);
						}
					}
#if UE_RIGVM_DEBUG_EXECUTION
					if (CVarControlRigDebugAllVMExecutions->GetBool() || ContextPublicData.bDebugExecution)
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
					const ERigVMExecuteResult ExecuteResult = ExecuteVM(Context, Op.EntryName);
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
			case ERigVMOpCode::RunInstructions:
			{
				const FRigVMRunInstructionsOp& Op = ByteCode.GetOpAt<FRigVMRunInstructionsOp>(Instruction);
				FRigVMInstructionSetExecuteState& ExecutionState = *(FRigVMInstructionSetExecuteState*)Context.CachedMemoryHandles[FirstHandleForInstruction[ContextPublicData.InstructionIndex]].GetData();

				if((Op.StartInstruction != INDEX_NONE) &&
					(Op.EndInstruction != INDEX_NONE) &&
					(Op.EndInstruction >= Op.StartInstruction))
				{
					const uint32 SliceHash = Context.GetSliceHash();
					uint32& StoredHash = ExecutionState.SliceHashToNumInstruction.FindOrAdd(SliceHash, UINT32_MAX);
						
					const uint32 Hash = GetTypeHash(ContextPublicData.GetNumExecutions());
					if(StoredHash != Hash)
					{
						ExecuteInstructions(Context, Op.StartInstruction, Op.EndInstruction);
						StoredHash = Hash;
					}
				}

				ContextPublicData.InstructionIndex++;
				break;
			}
			case ERigVMOpCode::Invalid:
			{
				ensure(false);
				return Context.CurrentExecuteResult = ERigVMExecuteResult::Failed;
			}
		}

#if WITH_EDITOR
		if ((RigVMInstructionVisitInfo != nullptr) && (RigVMProfilingInfo != nullptr))
		{
			if (!RigVMInstructionVisitInfo->GetInstructionVisitOrder().IsEmpty())
			{
				const uint64 EndCycles = FPlatformTime::Cycles64();
				const uint64 Cycles = EndCycles - RigVMProfilingInfo->GetStartCycles();
				if (RigVMProfilingInfo->GetInstructionCyclesDuringLastRun(CurrentInstructionIndex) == UINT64_MAX)
				{
					RigVMProfilingInfo->SetInstructionCyclesDuringLastRun(CurrentInstructionIndex, Cycles);
				}
				else
				{
					RigVMProfilingInfo->AddInstructionCyclesDuringLastRun(CurrentInstructionIndex, Cycles);
				}

				RigVMProfilingInfo->SetStartCycles(EndCycles);
				RigVMProfilingInfo->AddOverallCycles(Cycles);
			}
		}

#if UE_RIGVM_DEBUG_EXECUTION
		if (CVarControlRigDebugAllVMExecutions->GetBool() || ContextPublicData.bDebugExecution)
		{
			TArray<FString> CurrentWorkMemory;
			FRigVMMemoryStorageStruct* WorkMemory = GetWorkMemory(Context);
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
				if (ExternalVariable.Memory == nullptr)
				{
					continue;
				}
				
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

ERigVMExecuteResult URigVM::ExecuteBranch(FRigVMExtendedExecuteContext& Context, const FRigVMBranchInfo& InBranchToRun)
{
#if WITH_EDITOR
	const double LastExecutionMicroSecondsGuard = Context.GetRigVMProfilingInfo() ? Context.GetRigVMProfilingInfo()->GetLastExecutionMicroSeconds() : 0.0;
	ON_SCOPE_EXIT
	{
		if (Context.GetRigVMProfilingInfo())
		{
			Context.GetRigVMProfilingInfo()->SetLastExecutionMicroSeconds(LastExecutionMicroSecondsGuard);
		}
	};
#endif

	// Maintain all settings on the context - to be reset once the branch has executed. 
	FRigVMExecuteContext& PublicContext = Context.GetPublicData<>();
	TGuardValue<uint32> NumExecutionsGuard(PublicContext.NumExecutions, PublicContext.NumExecutions);
	TGuardValue<ERigVMExecuteResult> CurrentExecuteResultGuard(Context.CurrentExecuteResult, Context.CurrentExecuteResult);
	TGuardValue<FName> CurrentEntryNameGuard(Context.CurrentEntryName, Context.CurrentEntryName);
	TGuardValue<bool> bCurrentlyRunningRootEntryGuard(Context.bCurrentlyRunningRootEntry, Context.bCurrentlyRunningRootEntry);
	TGuardValue<FName> EventNameGuard(PublicContext.EventName, PublicContext.EventName);
	TGuardValue<FName> FunctionNameGuard(PublicContext.FunctionName, PublicContext.FunctionName);
	TGuardValue<uint16> InstructionIndexGuard(PublicContext.InstructionIndex, PublicContext.InstructionIndex);
	TGuardValue<double> DeltaTimeGuard(PublicContext.DeltaTime, PublicContext.DeltaTime);
	TGuardValue<double> AbsoluteTimeGuard(PublicContext.AbsoluteTime, PublicContext.AbsoluteTime);
	TGuardValue<double> FramesPerSecondGuard(PublicContext.FramesPerSecond, PublicContext.FramesPerSecond);

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

void URigVM::SetPropertyValueFromString(FRigVMExtendedExecuteContext& Context, const FRigVMOperand& InOperand, const FString& InDefaultValue)
{
	FRigVMMemoryStorageStruct* Memory = GetMemoryByType(Context, InOperand.GetMemoryType());
	if(Memory == nullptr)
	{
		return;
	}

	Memory->SetDataFromString(InOperand.GetRegisterIndex(), InDefaultValue);
}

#if WITH_EDITOR

TArray<FString> URigVM::DumpByteCodeAsTextArray(FRigVMExtendedExecuteContext& Context, const TArray<int32>& InInstructionOrder, bool bIncludeLineNumbers, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> OperandFormatFunction)
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
					Labels.Add(GetOperandLabel(Context, Operand, OperandFormatFunction));
				}

				ResultLine = FString::Printf(TEXT("%s(%s)"), *FunctionName, *FString::Join(Labels, TEXT(",")));
				break;
			}
			case ERigVMOpCode::Zero:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to 0"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::BoolFalse:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to False"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::BoolTrue:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to True"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Increment:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Inc %s ++"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Decrement:
			{
				const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Dec %s --"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = ByteCode.GetOpAt<FRigVMCopyOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Copy %s to %s"), *GetOperandLabel(Context, Op.Source, OperandFormatFunction), *GetOperandLabel(Context, Op.Target, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::Equals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to %s == %s "), *GetOperandLabel(Context, Op.Result, OperandFormatFunction), *GetOperandLabel(Context, Op.A, OperandFormatFunction), *GetOperandLabel(Context, Op.B, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Set %s to %s != %s"), *GetOperandLabel(Context, Op.Result, OperandFormatFunction), *GetOperandLabel(Context, Op.A, OperandFormatFunction), *GetOperandLabel(Context, Op.B, OperandFormatFunction));
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
					ResultLine = FString::Printf(TEXT("Jump to instruction %d if %s"), Op.InstructionIndex, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				}
				else
				{
					ResultLine = FString::Printf(TEXT("Jump to instruction %d if !%s"), Op.InstructionIndex, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				}
				break;
			}
			case ERigVMOpCode::JumpForwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instructions[InstructionIndex]);
				if (Op.Condition)
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions forwards if %s"), Op.InstructionIndex, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				}
				else
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions forwards if !%s"), Op.InstructionIndex, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				}
				break;
			}
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Instructions[InstructionIndex]);
				if (Op.Condition)
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions backwards if %s"), Op.InstructionIndex, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				}
				else
				{
					ResultLine = FString::Printf(TEXT("Jump %d instructions backwards if !%s"), Op.InstructionIndex, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				}
				break;
			}
			case ERigVMOpCode::ChangeType:
			{
				const FRigVMChangeTypeOp& Op = ByteCode.GetOpAt<FRigVMChangeTypeOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Change type of %s"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
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
				ResultLine = FString::Printf(TEXT("Jump To Branch %s"), *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
				break;
			}
			case ERigVMOpCode::RunInstructions:
			{
				const FRigVMRunInstructionsOp& Op = ByteCode.GetOpAt<FRigVMRunInstructionsOp>(Instructions[InstructionIndex]);
				ResultLine = FString::Printf(TEXT("Run Instructions %d-%d (%s)"), Op.StartInstruction, Op.EndInstruction, *GetOperandLabel(Context, Op.Arg, OperandFormatFunction));
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

FString URigVM::DumpByteCodeAsText(FRigVMExtendedExecuteContext& Context, const TArray<int32>& InInstructionOrder, bool bIncludeLineNumbers)
{
	RefreshExternalPropertyPaths();
	return FString::Join(DumpByteCodeAsTextArray(Context, InInstructionOrder, bIncludeLineNumbers), TEXT("\n"));
}

FString URigVM::GetOperandLabel(FRigVMExtendedExecuteContext& Context, const FRigVMOperand& InOperand, TFunction<FString(const FString& RegisterName, const FString& RegisterOffsetName)> FormatFunction)
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
		FRigVMMemoryStorageStruct* Memory = GetMemoryByType(Context, InOperand.GetMemoryType());
		if(Memory == nullptr)
		{
			return FString();
		}

		if(!Memory->IsValidIndex(InOperand.GetRegisterIndex()))
		{
			if(Memory->Num() == 0)
			{
				Memory = GetDefaultMemoryByType(InOperand.GetMemoryType());
			}
		}

		if(!Memory->IsValidIndex(InOperand.GetRegisterIndex()))
		{
			static const UEnum* MemoryTypeEnum = StaticEnum<ERigVMMemoryType>();
			static constexpr TCHAR Format[] = TEXT("%s_%d");
			RegisterName = FString::Printf(Format, *MemoryTypeEnum->GetDisplayNameTextByValue((int64)InOperand.GetMemoryType()).ToString(), InOperand.GetRegisterIndex());
		}
		else
		{
			RegisterName = Memory->GetProperties()[InOperand.GetRegisterIndex()]->GetName();
			RegisterOffsetName =
				InOperand.GetRegisterOffset() != INDEX_NONE ?
				Memory->GetPropertyPaths()[InOperand.GetRegisterOffset()].ToString() :
				FString();
		}
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

void URigVM::ClearDebugMemory(FRigVMExtendedExecuteContext& Context)
{
#if WITH_EDITOR
	if (GetDebugMemory(Context))
	{
		for (int32 PropertyIndex = 0; PropertyIndex < GetDebugMemory(Context)->Num(); PropertyIndex++)
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(GetDebugMemory(Context)->GetProperties()[PropertyIndex]))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, GetDebugMemory(Context)->GetData<uint8>(PropertyIndex));
				ArrayHelper.EmptyValues();
			}
		}
	}
#endif
}

void URigVM::CacheSingleMemoryHandle(FRigVMExtendedExecuteContext& Context, int32 InHandleIndex, const FRigVMBranchInfoKey& InBranchInfoKey, const FRigVMOperand& InArg, bool bForExecute)
{
	FRigVMMemoryStorageStruct* Memory = GetMemoryByType(Context, InArg.GetMemoryType()/*, false*/);

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

	FRigVMMemoryStorageStruct* TargetMemory = GetDebugMemory(Context);
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
			FRigVMMemoryStorageStruct::CopyProperty(TargetProperty, TargetPtr, SourceProperty, SourcePtr);
		}
		return;
	}

	FRigVMMemoryStorageStruct* SourceMemory = GetMemoryByType(Context, InArg.GetMemoryType());
	if(SourceMemory == nullptr)
	{
		return;
	}
	const FProperty* SourceProperty = SourceMemory->GetProperties()[InArg.GetRegisterIndex()];
	const uint8* SourcePtr = SourceMemory->GetData<uint8>(InArg.GetRegisterIndex());

	FRigVMMemoryStorageStruct::CopyProperty(TargetProperty, TargetPtr, SourceProperty, SourcePtr);
	
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
	if (FRigVMProfilingInfo* RigVMProfilingInfo = Context.GetRigVMProfilingInfo())
	{
		RigVMProfilingInfo->SetupInstructionTracking(InInstructionCount, true);
	}
#endif
}

void URigVM::StartProfiling(FRigVMExtendedExecuteContext& Context)
{
#if WITH_EDITOR
	if (FRigVMProfilingInfo* RigVMProfilingInfo = Context.GetRigVMProfilingInfo())
	{
		RigVMProfilingInfo->StartProfiling(true);
	}
#endif
}

void URigVM::StopProfiling(FRigVMExtendedExecuteContext& Context)
{
#if WITH_EDITOR
	if (FRigVMProfilingInfo* RigVMProfilingInfo = Context.GetRigVMProfilingInfo())
	{
		RigVMProfilingInfo->StopProfiling();
	}
#endif
}
