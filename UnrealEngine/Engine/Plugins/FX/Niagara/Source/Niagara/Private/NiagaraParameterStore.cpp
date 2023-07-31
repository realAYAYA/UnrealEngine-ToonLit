// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterStore.h"
#include "NiagaraDataSet.h"
#include "NiagaraComponent.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraStats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraParameterStore)

DECLARE_CYCLE_STAT(TEXT("Parameter store bind"), STAT_NiagaraParameterStoreBind, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Parameter store rebind"), STAT_NiagaraParameterStoreRebind, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Parameter store tick"), STAT_NiagaraParameterStoreTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Parameter store find var"), STAT_NiagaraParameterStoreFindVar, STATGROUP_Niagara);
DECLARE_MEMORY_STAT(TEXT("Niagara parameter store memory"), STAT_NiagaraParamStoreMemory, STATGROUP_Niagara);

#if WITH_EDITORONLY_DATA
static int32 GbDumpParticleParameterStores = 0;
static FAutoConsoleVariableRef CVarNiagaraDumpParticleParameterStores(
	TEXT("fx.DumpParticleParameterStores"),
	GbDumpParticleParameterStores,
	TEXT("If > 0 current frame particle parameter stores will be dumped when updated. \n"),
	ECVF_Default
);
#endif

//////////////////////////////////////////////////////////////////////////

struct FNiagaraVariableSearch
{
	typedef int32(*VariableCompareFunction)(const FNiagaraVariableBase&, const FNiagaraVariableBase&);

	static FORCEINLINE int32 Compare(const FNiagaraVariableBase& A, const FNiagaraVariableBase& B)
	{
#if NIAGARA_VARIABLE_LEXICAL_SORTING
		int32 ComparisonDiff = A.GetName().Compare(B.GetName());
#else
		int32 ComparisonDiff = A.GetName().CompareIndexes(B.GetName());
#endif
		if (ComparisonDiff != 0)
		{
			return ComparisonDiff;
		}
		else
		{
#if NIAGARA_VARIABLE_LEXICAL_SORTING
			return ComparisonDiff = A.GetType().GetFName().Compare(B.GetType().GetFName());
#else
			return ComparisonDiff = A.GetType().GetFName().CompareIndexes(B.GetType().GetFName());
#endif
		}
	}

	static FORCEINLINE int32 CompareIgnoreType(const FNiagaraVariableBase& A, const FNiagaraVariableBase& B)
	{
#if NIAGARA_VARIABLE_LEXICAL_SORTING
		int32 ComparisonDiff = A.GetName().Compare(B.GetName());
#else
		int32 ComparisonDiff = A.GetName().CompareIndexes(B.GetName());
#endif

		return ComparisonDiff;
	}

	static bool FindInternal(VariableCompareFunction CompareFn, const FNiagaraVariableWithOffset* Variables, const FNiagaraVariableBase& Ref, int32 Start, int32 Num, int32& CheckIndex)
	{
		while (Num)
		{
			const int32 LeftoverSize = Num % 2;
			Num = Num / 2;

			CheckIndex = Start + Num;
			const int32 StartIfLess = CheckIndex + LeftoverSize;

			const int32 ComparisonDiff = CompareFn(Variables[CheckIndex], Ref);
			if (ComparisonDiff < 0)
			{
				Start = CheckIndex + 1;
				Num += LeftoverSize - 1;
			}
			else if (ComparisonDiff == 0)
			{
				return true;
			}
		}
		CheckIndex = Start;
		return false;
	}

	static FORCEINLINE bool Find(const FNiagaraVariableWithOffset* Variables, const FNiagaraVariableBase& Ref, int32 Start, int32 Num, bool IgnoreType, int32& CheckIndex)
	{
		if (IgnoreType)
		{
			return FindInternal(CompareIgnoreType, Variables, Ref, Start, Num, CheckIndex);
		}
		return FindInternal(Compare, Variables, Ref, Start, Num, CheckIndex);
	}
};

bool FNiagaraVariableWithOffset::Serialize(FArchive& Ar)
{
	FNiagaraVariableBase::Serialize(Ar);

	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
	const int32 NiagaraVersion = Ar.CustomVer(FNiagaraCustomVersion::GUID);

	if (!Ar.IsLoading() || NiagaraVersion >= FNiagaraCustomVersion::VariablesUseTypeDefRegistry)
	{
		Ar << Offset;
		return true;
	}

	return false;
}

#if WITH_EDITORONLY_DATA
void FNiagaraVariableWithOffset::PostSerialize(const FArchive& Ar)
{
	FNiagaraVariableBase::PostSerialize(Ar);
}
#endif

//////////////////////////////////////////////////////////////////////////

void FNiagaraParameterStore::CopySortedParameterOffsets(TArrayView<const FNiagaraVariableWithOffset> Src)
{
	SortedParameterOffsets = TArray<FNiagaraVariableWithOffset>(Src.GetData(), Src.Num());
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraParameterStore::SetPositionData(const FName& Name, const FVector& Position)
{
	for (FNiagaraPositionSource& Data : OriginalPositionData)
	{
		if (Data.Name == Name)
		{
			Data.Value = Position;
			return;
		}
	}
	OriginalPositionData.Emplace(Name, Position);
}

bool FNiagaraParameterStore::HasPositionData(const FName& Name) const
{
	for (const FNiagaraPositionSource& Data : OriginalPositionData)
	{
		if (Data.Name == Name)
		{
			return true;
		}
	}
	return false;
}

const FVector* FNiagaraParameterStore::GetPositionData(const FName& Name) const
{
	for (const FNiagaraPositionSource& Data : OriginalPositionData)
	{
		if (Data.Name == Name)
		{
			return &Data.Value;
		}
	}
	return nullptr;
}

void FNiagaraParameterStore::RemovePositionData(const FName& Name)
{
	for (int i = 0; i < OriginalPositionData.Num(); i++)
	{
		if (OriginalPositionData[i].Name == Name)
		{
			OriginalPositionData.RemoveAtSwap(i);
			return;
		}
	}
}

FNiagaraParameterStore::FNiagaraParameterStore()
	: Owner(nullptr)
	, bParametersDirty(true)
	, bInterfacesDirty(true)
	, bUObjectsDirty(true)
	, bPositionDataDirty(true)
	, LayoutVersion(0)
{
}

void FNiagaraParameterStore::SetOwner(UObject* InOwner)
{
	Owner = InOwner;
#if WITH_EDITORONLY_DATA
	if (InOwner != nullptr)
	{
		DebugName = *InOwner->GetFullName();
	}
#endif
}

FNiagaraParameterStore::FNiagaraParameterStore(const FNiagaraParameterStore& Other)
{
	*this = Other;
}

FNiagaraParameterStore& FNiagaraParameterStore::operator=(const FNiagaraParameterStore& Other)
{
#if WITH_EDITORONLY_DATA
	ParameterOffsets = Other.ParameterOffsets;
#endif // WITH_EDITORONLY_DATA
	CopySortedParameterOffsets(Other.ReadParameterVariables());
	AssignParameterData(Other.ParameterData);
	DataInterfaces = Other.DataInterfaces;
	UObjects = Other.UObjects;
	SetOriginalPositionData(Other.OriginalPositionData);
	++LayoutVersion;
#if WITH_EDITOR
	if (bSuppressOnChanged == false)
	{
		OnChangedDelegate.Broadcast();
	}
#endif
	//Don't copy bindings. We just want the data.
	return *this;
}

FNiagaraParameterStore::~FNiagaraParameterStore()
{
	DEC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());
	
	checkf(IsInGameThread() || (SourceStores.Num() == 0), TEXT("ParameterStore destruction must be on the GameThread(%d) or SourceStores(%d) must be unbound"), IsInGameThread() ? 1 : 0, SourceStores.Num());

	UnbindAll();
}

void FNiagaraParameterStore::Bind(FNiagaraParameterStore* DestStore, const FNiagaraBoundParameterArray* BoundParameters)
{
	check(DestStore);
	//SCOPE_CYCLE_COUNTER(STAT_NiagaraParameterStoreBind);

	if (!Algo::FindBy(Bindings, DestStore, &BindingPair::Key))
	{
		FNiagaraParameterStoreBinding HeapBinding;
		if (HeapBinding.Initialize(DestStore, this, BoundParameters))
		{
			Bindings.Emplace(DestStore, HeapBinding);
		}
	}
}

template <typename TVisitor>
void FNiagaraParameterStoreBinding::MatchParameters(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, TVisitor Visitor)
{
	TArrayView<const FNiagaraVariableWithOffset> SrcParamWithOffsets = SrcStore->ReadParameterVariables();
	TArrayView<const FNiagaraVariableWithOffset> DestParamWithOffsets = DestStore->ReadParameterVariables();

	const int32 SrcNum = SrcParamWithOffsets.Num();
	const int32 DestNum = DestParamWithOffsets.Num();
	const int32 BinarySearchComplexity = FMath::Min<int32>(SrcNum, DestNum) * FMath::RoundToInt(FMath::Log2((float)FMath::Max<int32>(SrcNum, DestNum)));
	if (BinarySearchComplexity >= SrcNum + DestNum)
	{
		int32 SrcIndex = 0;
		int32 DestIndex = 0;
		while (SrcIndex < SrcNum && DestIndex < DestNum)
		{
			const FNiagaraVariableWithOffset& SrcParamWithOffset = SrcParamWithOffsets[SrcIndex];
			const FNiagaraVariableWithOffset& DestParamWithOffset = DestParamWithOffsets[DestIndex];

			int32 CompValue;
			if (FNiagaraTypeHelper::IsLWCType(SrcParamWithOffset.GetType()) || FNiagaraTypeHelper::IsLWCType(DestParamWithOffset.GetType()))
			{
				CompValue = FNiagaraVariableSearch::CompareIgnoreType(SrcParamWithOffset, DestParamWithOffset);
			}
			else
			{
				CompValue = FNiagaraVariableSearch::Compare(SrcParamWithOffset, DestParamWithOffset);
			}
			
			if (CompValue < 0)
			{
				++SrcIndex;
			}
			else if (CompValue > 0)
			{
				++DestIndex;
			}
			else // CompValue == 0
			{
				Visitor(DestParamWithOffset, SrcParamWithOffset.Offset, DestParamWithOffset.Offset);
				++SrcIndex;
				++DestIndex;
			}

		}

	}
	// Process the smaller parameter store the get the least amount of iterations when it is small (often empty).
	else if (DestNum <= SrcNum)
	{
		for (const FNiagaraVariableWithOffset& ParamWithOffset : DestParamWithOffsets)
		{
			if (const FNiagaraVariableWithOffset* Var = SrcStore->FindParameterVariable(ParamWithOffset, true))
			{
				Visitor(ParamWithOffset, Var->Offset, ParamWithOffset.Offset);
			}
		}
	}
	else
	{
		for (const FNiagaraVariableWithOffset& ParamWithOffset : SrcParamWithOffsets)
		{
			if (const FNiagaraVariableWithOffset* Var = DestStore->FindParameterVariable(ParamWithOffset, true))
			{
				Visitor(*Var, ParamWithOffset.Offset, Var->Offset);
			}
		}
	}
}

void FNiagaraParameterStoreBinding::GetBindingData(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, FNiagaraBoundParameterArray& OutBoundParameters)
{
	OutBoundParameters.Empty();

	auto AddVariable = [&](const FNiagaraVariable& InParameter, int32 SrcOffset, int32 DestOffset)
	{
		if (SrcOffset != INDEX_NONE && DestOffset != INDEX_NONE)
		{
			FNiagaraBoundParameter BoundParameter;
			BoundParameter.Parameter = InParameter;
			BoundParameter.SrcOffset = SrcOffset;
			BoundParameter.DestOffset = DestOffset;

			OutBoundParameters.Add(BoundParameter);
		}
	};

	MatchParameters(DestStore, SrcStore, AddVariable);
}

bool FNiagaraParameterStoreBinding::BindParameters(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, const FNiagaraBoundParameterArray* BoundParameters)
{
	InterfaceBindings.Reset();
	ParameterBindings.Reset();
	UObjectBindings.Reset();

	bool bAnyBinding = false;

	auto BindVariable = [&](const FNiagaraVariable& InParameter, int32 SrcOffset, int32 DestOffset)
	{
		if (SrcOffset != INDEX_NONE && DestOffset != INDEX_NONE)
		{
			bAnyBinding = true;

			if (InParameter.IsDataInterface())
			{
				InterfaceBindings.Add(FInterfaceBinding(SrcOffset, DestOffset));
			}
			else if (InParameter.IsUObject())
			{
				UObjectBindings.Add(FUObjectBinding(SrcOffset, DestOffset));
			}
			else
			{
				ParameterBindings.Add(FParameterBinding(SrcOffset, DestOffset, InParameter.GetSizeInBytes()));
			}
		}
	};

	if (!BoundParameters)
	{
		MatchParameters(DestStore, SrcStore, BindVariable);
	}
	else if (BoundParameters->Num())
	{
		for (const FNiagaraBoundParameter& BoundParameter : *BoundParameters)
		{
			checkSlow(SrcStore->IndexOf(BoundParameter.Parameter) == BoundParameter.SrcOffset && DestStore->IndexOf(BoundParameter.Parameter) == BoundParameter.DestOffset);
			BindVariable(BoundParameter.Parameter, BoundParameter.SrcOffset, BoundParameter.DestOffset);
		}
		bAnyBinding = true;
	}

	if (bAnyBinding)
	{
		//Force an initial tick to prime our values in the destination store.
		Tick(DestStore, SrcStore, true);
	}
	return bAnyBinding;
}

void FNiagaraParameterStore::Unbind(FNiagaraParameterStore* DestStore)
{
	const int32 BindingIndex = Bindings.IndexOfByPredicate([DestStore](const BindingPair& Binding)
	{
		return Binding.Key == DestStore;
	});

	if (BindingIndex != INDEX_NONE)
	{
		Bindings[BindingIndex].Value.Empty(DestStore, this);
		Bindings.RemoveAtSwap(BindingIndex);
	}
}

void FNiagaraParameterStore::UnbindAll()
{
	UnbindFromSourceStores();
	for (TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
	{
		Binding.Value.Empty(Binding.Key, this);
	}
	Bindings.Empty();
}

void FNiagaraParameterStore::Rebind()
{
	//SCOPE_CYCLE_COUNTER(STAT_NiagaraParameterStoreRebind);
	for (TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
	{
		Binding.Value.Initialize(Binding.Key, this);
	}
}

void FNiagaraParameterStore::TransferBindings(FNiagaraParameterStore& OtherStore)
{
	for (TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
	{
		OtherStore.Bind(Binding.Key);
	}

	UnbindAll();
}

bool FNiagaraParameterStore::VerifyBinding(const FNiagaraParameterStore* DestStore)const
{
#if WITH_EDITORONLY_DATA
	const BindingPair* Binding = Algo::FindBy(Bindings, DestStore, &BindingPair::Key);
	if (Binding)
	{
		return Binding->Value.VerifyBinding(DestStore, this);
	}
	else
	{
		UE_LOG(LogNiagara, Warning, TEXT("Invalid ParameterStore Binding: % was not bound to %s."), *DebugName, *DestStore->DebugName);
	}

	return false;
#else
	return true;
#endif
}

void FNiagaraParameterStore::CheckForNaNs()const
{
	for (const FNiagaraVariableWithOffset& Var : ReadParameterVariables())
	{
		const int32 Offset = Var.Offset;

		bool bContainsNans = false;
		if (Var.GetType() == FNiagaraTypeDefinition::GetFloatDef())
		{
			float Val = *(float*)GetParameterData(Offset);
			bContainsNans = FMath::IsNaN(Val) || !FMath::IsFinite(Val);
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec2Def())
		{
			FVector2f Val = *(FVector2f*)GetParameterData(Offset);
			bContainsNans = Val.ContainsNaN();
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec3Def())
		{
			FVector3f Val = *(FVector3f*)GetParameterData(Offset);
			bContainsNans = Val.ContainsNaN();
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec4Def())
		{
			FVector4f Val = *(FVector4f*)GetParameterData(Offset);
			bContainsNans = Val.ContainsNaN();
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetMatrix4Def())
		{
			FMatrix Val;
			FMemory::Memcpy(&Val, GetParameterData(Offset), sizeof(FMatrix44f));
			bContainsNans = Val.ContainsNaN();
		}

		if (bContainsNans)
		{
			ensureAlwaysMsgf(false, TEXT("Niagara Parameter Store containts Nans!\n"));
			DumpParameters(false);
		}
	}
}

#if WITH_EDITORONLY_DATA
void FNiagaraParameterStore::ConvertParameterType(const FNiagaraVariable& ExistingParam, const FNiagaraTypeDefinition& NewType)
{
	ensure(ExistingParam.GetType().GetSize() == NewType.GetSize());
	int32 Idx = IndexOf(ExistingParam);
	if (!ensure(Idx != INDEX_NONE))
	{
		return;
	}

	for (FNiagaraVariableWithOffset& OffsetVar : SortedParameterOffsets)
	{
		if (OffsetVar.GetName() == ExistingParam.GetName() && OffsetVar.GetType() == ExistingParam.GetType())
		{
			OffsetVar.SetType(NewType);
			break;
		}
	}
	if (NewType == FNiagaraTypeDefinition::GetPositionDef())
	{
		FVector CurrentValue = (FVector)*reinterpret_cast<FVector3f*>(GetParameterData_Internal(Idx));
		SetPositionData(ExistingParam.GetName(), CurrentValue);
		bPositionDataDirty = true;
	}
	
	OnParameterChange();
}
#endif

void FNiagaraParameterStore::TickBindings()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraParameterStoreTick);
	for (TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
	{
		Binding.Value.Tick(Binding.Key, this);
	}
	Dump();
}

void FNiagaraParameterStore::UnbindFromSourceStores()
{
	//UE_LOG(LogNiagara, Log, TEXT("FNiagaraParameterStore::UnUnbindFromSourceStoresbind() - Src: 0x%p - SrcName: %s"), this, *DebugName);
	//Each source store will remove itself from this array as it is unbound so after N unbinds the array should be empty.
	int32 NumSourceStores = SourceStores.Num();
	while (NumSourceStores--)
	{
		SourceStores[0]->Unbind(this);
	}
	ensureMsgf(SourceStores.Num() == 0, TEXT("Parameter store source array was not empty after unbinding all sources. Something seriously wrong."));
	SourceStores.Empty();
}

void FNiagaraParameterStore::DumpParameters(bool bDumpBindings)const
{
	for (const FNiagaraVariableWithOffset& VariableBase : ReadParameterVariables())
	{
		FNiagaraVariable Var(VariableBase);
		Var.SetData(GetParameterData(VariableBase.Offset));
		UE_LOG(LogNiagara, Log, TEXT("Param: %s Offset: %d Type : %s"), *Var.ToString(), IndexOf(Var), *Var.GetType().GetName());
	}

	if (bDumpBindings)
	{
		for (const TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
		{
			Binding.Value.Dump(Binding.Key, this);
		}
	}
}

FString FNiagaraParameterStore::ToString() const
{
	FString Value;

	for (const FNiagaraVariableWithOffset& VariableBase : ReadParameterVariables())
	{
		FNiagaraVariable Var(VariableBase);
		Var.SetData(GetParameterData(VariableBase.Offset));
		Value += FString::Printf(TEXT("Param: %s Offset: %d Type : %s\n"), *Var.ToString(), IndexOf(Var), *Var.GetType().GetName());
	}

	return Value;
}

void FNiagaraParameterStore::Dump()
{
#if WITH_EDITORONLY_DATA
	if (GbDumpParticleParameterStores && GetParametersDirty())
	{
		UE_LOG(LogNiagara, Log, TEXT("\nSource Store: %s\n========================\n"), *DebugName);
		DumpParameters(true);
		
		UE_LOG(LogNiagara, Log, TEXT("\n========================\n"));
	}
#endif
}

/**
Adds the passed parameter to this store.
Does nothing if this parameter is already present.
Returns true if we added a new parameter.
*/
bool FNiagaraParameterStore::AddParameter(const FNiagaraVariable& Param, bool bInitInterfaces, bool bTriggerRebind, int32* OutOffset)
{
#if WITH_EDITORONLY_DATA
	if (ParameterOffsets.Num())
	{
		UE_LOG(LogNiagara, Warning, TEXT("AddParameter: ParameterOffsets.Num() should be 0 is %d, please investigate for %s."), ParameterOffsets.Num() , *GetPathNameSafe(Owner.Get()));
	}
	//check(!ParameterOffsets.Num()); // Migration to SortedParameterOffsets
#endif

	auto ParameterVariables = ReadParameterVariables();

	int32 InsertPos = 0;
	if (ParameterVariables.Num())
	{
		const bool bAlreadyIn = FNiagaraVariableSearch::Find(ParameterVariables.GetData(), Param, 0, ParameterVariables.Num(), false /* IgnoreType */, InsertPos);
		if (bAlreadyIn)
		{
			if (OutOffset)
			{
				*OutOffset = ParameterVariables[InsertPos].Offset;
			}
			return false;
		}
	}

	FNiagaraTypeDefinition ParamType = Param.GetType();
	FNiagaraLwcStructConverter StructConverter;
	if (FNiagaraTypeHelper::IsLWCType(ParamType))
	{
		StructConverter = FNiagaraTypeRegistry::GetStructConverter(ParamType);
	}

	int32& Offset = SortedParameterOffsets.EmplaceAt_GetRef(InsertPos, Param, (int32)INDEX_NONE, StructConverter).Offset;

	if (ParamType.IsDataInterface())
	{
		Offset = DataInterfaces.AddZeroed();
		DataInterfaces[Offset] = bInitInterfaces ? NewObject<UNiagaraDataInterface>(Owner.Get(), const_cast<UClass*>(ParamType.GetClass()), NAME_None, RF_Transactional | RF_Public) : nullptr;
		bInterfacesDirty = true;
	}
	else if (ParamType.IsUObject())
	{
		Offset = UObjects.AddDefaulted();
		bUObjectsDirty = true;
	}
	else
	{
		DEC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());

		// the size here is too large for custom structs with lwc types, as we only write data for the swc type. But since the api allows direct access to the memory,
		// we allocate a bit more than necessary to prevent access violations due to wrong access. Note that the script execution store actually uses the correct size for the allocated data.
		int32 ParamSize = ParamType.GetSize(); 
		//int32 ParamAlignment = Param.GetAlignment();
		//int32 Offset = AlignArbitrary(ParameterData.Num(), ParamAlignment);//TODO: We need to handle alignment better here. Need to both satisfy CPU and GPU alignment concerns. VM doesn't care but the VM complier needs to be aware. Probably best to have everything adhere to GPU alignment rules.
		Offset = ParameterData.Num();
				
		//Temporary to init param data from FNiagaraVariable storage. This will be removed when we change the UNiagaraScript to use a parameter store too.
		if (Param.IsDataAllocated())
		{
			ParameterData.AddUninitialized(ParamSize);
			if (StructConverter.IsValid())
			{
				StructConverter.ConvertDataToSimulation(GetParameterData_Internal(Offset), Param.GetData());
			}
			else
			{
				FMemory::Memcpy(GetParameterData_Internal(Offset), Param.GetData(), ParamSize);	
			}			
		}
		else
		{
			// Memory must be initialized in order to have deterministic cooking. 
			// This is because some system parameters never get initialized otherwise (particle count, owner rotation, ...)
			ParameterData.AddZeroed(ParamSize);
		}

		bParametersDirty = true;

		if (ParamType == FNiagaraTypeDefinition::GetPositionDef() && !HasPositionData(Param.GetName()))
		{
			SetPositionData(Param.GetName(), FVector::ZeroVector);
			bPositionDataDirty = true;
		}

		INC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());
	}

	if (bTriggerRebind)
	{
		OnLayoutChange();
	}
	else
	{
		++LayoutVersion;
	}
	
	if (OutOffset)
	{
		*OutOffset = Offset;
	}
	return true;
}

bool FNiagaraParameterStore::RemoveParameter(const FNiagaraVariableBase& ToRemove)
{
#if WITH_EDITORONLY_DATA
	if (ParameterOffsets.Num())
	{
		UE_LOG(LogNiagara, Warning, TEXT("RemoveParameter: ParameterOffsets.Num() should be 0 is %d, please investigate for %s"), ParameterOffsets.Num(), *GetPathNameSafe(Owner.Get()));
	}
	//check(!ParameterOffsets.Num()); // Migration to SortedParameterOffsets
#endif
	RemovePositionData(ToRemove.GetName());
	
	if (IndexOf(ToRemove) != INDEX_NONE)
	{
		//TODO: Ensure direct bindings are either updated or disallowed here.
		//We have to regenerate the store and the offsets on removal. This shouldn't happen at runtime!
		TArray<FNiagaraVariableWithOffset> NewOffsets;
		TArray<uint8> NewData;
		TArray<UNiagaraDataInterface*> NewInterfaces;
		TArray<UObject*> NewUObjects;
		for (const FNiagaraVariableWithOffset& Existing : ReadParameterVariables())
		{
			const FNiagaraVariable& ExistingVar = Existing;
			const int32 ExistingOffset = Existing.Offset;

			//Add all but the one to remove to our
			if (static_cast<const FNiagaraVariableBase&>(ExistingVar) != ToRemove)
			{
				if (ExistingVar.GetType().IsDataInterface())
				{
					int32 Offset = NewInterfaces.AddZeroed();
					NewOffsets.Add(FNiagaraVariableWithOffset(ExistingVar, Offset, FNiagaraLwcStructConverter()));
					NewInterfaces[Offset] = DataInterfaces[ExistingOffset];
				}
				else if (ExistingVar.IsUObject())
				{
					int32 Offset = NewUObjects.AddDefaulted();
					NewOffsets.Add(FNiagaraVariableWithOffset(ExistingVar, Offset, FNiagaraLwcStructConverter()));
					NewUObjects[Offset] = UObjects[ExistingOffset];
				}
				else
				{
					int32 Offset = NewData.Num();
					int32 ParamSize = ExistingVar.GetSizeInBytes();
					NewOffsets.Add(FNiagaraVariableWithOffset(ExistingVar, Offset, Existing.StructConverter));
					NewData.AddUninitialized(ParamSize);
					FMemory::Memcpy(NewData.GetData() + Offset, ParameterData.GetData() + ExistingOffset, ParamSize);
				}
			}
		}

		CopySortedParameterOffsets(MakeArrayView(NewOffsets));
		AssignParameterData(NewData);
		DataInterfaces = NewInterfaces;
		UObjects = NewUObjects;

		OnLayoutChange();
		return true;
	}

	return false;
}

void FNiagaraParameterStore::RenameParameter(const FNiagaraVariableBase& Param, FName NewName)
{
#if WITH_EDITORONLY_DATA
	if (ParameterOffsets.Num())
	{
		UE_LOG(LogNiagara, Warning, TEXT("RenameParameter: ParameterOffsets.Num() should be 0 is %d, please investigate for %s"), ParameterOffsets.Num(), *GetPathNameSafe(Owner.Get()));
	}
	//check(!ParameterOffsets.Num()); // Migration to SortedParameterOffsets
#endif

	if (Param.GetName() == NewName)
	{
		// Early out here to prevent crashes later on due to delta size mismatches when the newly named
		// parameter isn't added.
		return;
	}

	int32 Idx = IndexOf(Param);
	if(Idx != INDEX_NONE)
	{
		FNiagaraVariable NewParam = Param;
		NewParam.SetName(NewName);

		bool bInitInterfaces = false;
		bool bTriggerRebind = false;

		int32 NumBytesBefore = ParameterData.Num();
		int32 NewIdx = INDEX_NONE;
		if (AddParameter(NewParam, bInitInterfaces, bTriggerRebind, &NewIdx))
		{
			int32 NumBytesAfter = ParameterData.Num();
			int32 DeltaBytes = NumBytesAfter - NumBytesBefore;
			int32 SizeInBytes = Param.GetSizeInBytes();

			check(DeltaBytes == SizeInBytes);

			if (Param.IsDataInterface())
			{
				SetDataInterface(GetDataInterface(Idx), NewIdx);
			}
			else if (Param.IsUObject())
			{
				SetUObject(GetUObject(Idx), NewIdx);
			}
			else
			{
				SetParameterData(GetParameterData_Internal(Idx), NewIdx, Param.GetSizeInBytes());
			}
			RemoveParameter(Param);

			OnLayoutChange();
#if WITH_EDITORONLY_DATA
			OnParameterRenamedDelegate.Broadcast(Param, NewName);
#endif
		}
		else
		{
			UE_LOG(LogNiagara, Warning, TEXT("Ignored attempt to rename a parameter overtop of an existing parameter!  Old name: %s, New name: %s"), *Param.GetName().ToString(), *NewName.ToString());
		}
	}
}

void FNiagaraParameterStore::ChangeParameterType(const FNiagaraVariableBase& Param,	const FNiagaraTypeDefinition& NewType)
{
	ensure(Param.GetType().GetSize() == NewType.GetSize());
	ensure(Param.GetType() != NewType);

	for (FNiagaraVariableWithOffset& OffsetVar : SortedParameterOffsets)
	{
		if (OffsetVar == Param)
		{
			OffsetVar.SetType(NewType);
			OnParameterChange();
			return;
		}
	}
}

void FNiagaraParameterStore::SanityCheckData(bool bInitInterfaces)
{
	// This function exists to patch up the issue seen in FORT-208391, where we had entries for DataInterfaces in the offset array but not in the actual DataInterface array entries.
	// Additional protections were added for safety.
	bool OwnerDirtied = false;

	int32 ParameterDataSize = 0;

	for (const FNiagaraVariableWithOffset& Parameter : ReadParameterVariables())
	{
		const int32 SrcIndex = Parameter.Offset;

		if (Parameter.IsValid())
		{
			if (Parameter.IsDataInterface())
			{
				if (DataInterfaces.Num() <= SrcIndex)
				{
					int32 NewNum = SrcIndex - DataInterfaces.Num() + 1;
					DataInterfaces.AddZeroed(NewNum);
					UE_LOG(LogNiagara, Verbose, TEXT("Missing data interfaces! Had to add %d data interface entries to ParameterStore on %s"), NewNum , Owner != nullptr ? *Owner->GetPathName() : TEXT("Unknown owner"));

					OwnerDirtied = true;
				}
				if (DataInterfaces[SrcIndex] == nullptr && bInitInterfaces && Owner.IsValid())
				{
					DataInterfaces[SrcIndex] = NewObject<UNiagaraDataInterface>(Owner.Get(), const_cast<UClass*>(Parameter.GetType().GetClass()), NAME_None, RF_Transactional | RF_Public);
					UE_LOG(LogNiagara, Verbose, TEXT("Had to initialize data interface! %s on %s"), *Parameter.GetName().ToString(), Owner != nullptr ? *Owner->GetPathName() : TEXT("Unknown owner"));

					OwnerDirtied = true;
				}
			}
			else if (Parameter.IsUObject())
			{
				if (UObjects.Num() <= SrcIndex)
				{
					int32 NewNum = SrcIndex - UObjects.Num() + 1;
					UObjects.AddZeroed(NewNum);
					UE_LOG(LogNiagara, Verbose, TEXT("Missing UObject interfaces! Had to add %d UObject entries for %s on %s"), NewNum , *Parameter.GetName().ToString(), Owner != nullptr ? *Owner->GetPathName() : TEXT("Unknown owner"));

					OwnerDirtied = true;
				}
			}
			else
			{
				int32 Size = Parameter.GetType().GetSize();
				if (ParameterData.Num() < (SrcIndex + Size))
				{
					UE_LOG(LogNiagara, Verbose, TEXT("Missing parameter data! %s on %s"), *Parameter.GetName().ToString(), Owner != nullptr ? *Owner->GetPathName() : TEXT("Unknown owner"));

					OwnerDirtied = true;
				}
				ParameterDataSize = FMath::Max(ParameterDataSize, SrcIndex + Size);
			}
		}
	}

	if (ParameterData.Num() < ParameterDataSize)
	{
		ParameterData.AddZeroed(ParameterDataSize - ParameterData.Num());
	}

	if (Owner.IsValid() && OwnerDirtied)
	{
		UE_LOG(LogNiagara, Verbose, TEXT("%s needs to be resaved to prevent above warnings due to the parameter state being stale."), *Owner->GetFullName());
	}
}

void FNiagaraParameterStore::CopyParametersTo(FNiagaraParameterStore& DestStore, bool bOnlyAdd, EDataInterfaceCopyMethod DataInterfaceCopyMethod) const
{
	for (const FNiagaraVariableWithOffset& Parameter : ReadParameterVariables())
	{
		int32 SrcIndex = Parameter.Offset;

		if (Parameter.IsValid() == false)
		{
			FString StoreDebugName;
#if WITH_EDITORONLY_DATA
			StoreDebugName = DebugName.IsEmpty() == false ? DebugName : TEXT("Unknown");
#else
			StoreDebugName = TEXT("Unknown");
#endif
			FString StoreName;
			if (Owner != nullptr)
			{
				StoreName = Owner->GetPathName() + TEXT(".") + StoreDebugName;
			}
			else
			{
				StoreName = StoreDebugName;
			}

			UE_LOG(LogNiagara, Error, TEXT("Invalid parameter found while attempting to copy parameters from one parameter store to another.  Parameter Store: %s Parameter Name: %s Parameter Type: %s"), 
				*StoreName, *Parameter.GetName().ToString(), Parameter.GetType().IsValid() ? *Parameter.GetType().GetName() : TEXT("Unknown"));
			continue;
		}

		int32 DestIndex = DestStore.IndexOf(Parameter);
		bool bWrite = false;
		if (DestIndex == INDEX_NONE)
		{
			bool bInitInterfaces = bOnlyAdd == false && Parameter.IsDataInterface() && DataInterfaceCopyMethod == EDataInterfaceCopyMethod::Value;
			bool bTriggerRebind = false;
			DestStore.AddParameter(Parameter, bInitInterfaces, bTriggerRebind, &DestIndex);
			bWrite = !bOnlyAdd;
		}
		else if (!bOnlyAdd)
		{
			bWrite = true;
		}

		if (bWrite && DestIndex != INDEX_NONE && SrcIndex != INDEX_NONE)
		{
			if (Parameter.IsDataInterface())
			{
				ensure(DataInterfaces.IsValidIndex(SrcIndex));
				ensure(DestStore.DataInterfaces.IsValidIndex(DestIndex));
				if (DataInterfaceCopyMethod == EDataInterfaceCopyMethod::Reference)
				{
					DestStore.SetDataInterface(DataInterfaces[SrcIndex], DestIndex);
				}
				else if(DataInterfaceCopyMethod == EDataInterfaceCopyMethod::Value)
				{
					UNiagaraDataInterface* SourceInterface = DataInterfaces[SrcIndex];
					SourceInterface->CopyTo(DestStore.GetDataInterface(DestIndex));
				}
				else
				{
					checkf(false, TEXT("A data interface copy method must be specified if the parameter store has data interfaces."));
				}
			}
			else if (Parameter.IsUObject())
			{
				DestStore.SetUObject(GetUObject(SrcIndex), DestIndex);//UObjects are just refs to external objects. They never need to be deep copied.
			}
			else
			{
				if (ParameterData.Num() != 0)
				{
					DestStore.SetParameterData(GetParameterData(SrcIndex), DestIndex, Parameter.GetSizeInBytes());
				}
			}
		}
	}
	DestStore.OnLayoutChange();
}


void FNiagaraParameterStore::SetParameterDataArray(const TArray<uint8>& InParameterDataArray, bool bNotifyAsDirty)
{
	AssignParameterData(InParameterDataArray);

	if (bNotifyAsDirty)
	{
		MarkParametersDirty();
	}
}

void FNiagaraParameterStore::SetDataInterfaces(const TArray<UNiagaraDataInterface*>& InDataInterfaces, bool bNotifyAsDirty)
{
	DataInterfaces = InDataInterfaces;

	if (bNotifyAsDirty)
	{
		MarkInterfacesDirty();
	}
}

void FNiagaraParameterStore::SetUObjects(const TArray<UObject*>& InUObjects, bool bNotifyAsDirty)
{
	UObjects = InUObjects;

	if (bNotifyAsDirty)
	{
		MarkUObjectsDirty();
	}
}

void FNiagaraParameterStore::SetOriginalPositionData(const TArray<FNiagaraPositionSource>& InOriginalPositionData)
{
	OriginalPositionData = InOriginalPositionData;
	bPositionDataDirty = true;
}

void FNiagaraParameterStore::InitFromSource(const FNiagaraParameterStore* SrcStore, bool bNotifyAsDirty)
{
	Empty(false);
	if (SrcStore == nullptr)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	ParameterOffsets = SrcStore->ParameterOffsets;
	if (ParameterOffsets.Num())
	{
		UE_LOG(LogNiagara, Warning, TEXT("InitFromSource: ParameterOffsets.Num() should be 0 is %d, please investigate for %s"), ParameterOffsets.Num(), *GetPathNameSafe(Owner.Get()));
	}
#endif // WITH_EDITORONLY_DATA
	CopySortedParameterOffsets(SrcStore->ReadParameterVariables());
	AssignParameterData(SrcStore->ParameterData);

	DataInterfaces = SrcStore->DataInterfaces;
	UObjects = SrcStore->UObjects;
	SetOriginalPositionData(SrcStore->OriginalPositionData);

	if (bNotifyAsDirty)
	{
		MarkParametersDirty();
		MarkInterfacesDirty();
		MarkUObjectsDirty();
		OnLayoutChange();
	}
}

const FNiagaraVariableWithOffset* FNiagaraParameterStore::FindParameterVariable(const FNiagaraVariable& Parameter, bool IgnoreType) const
{
	auto ParameterVariables = ReadParameterVariables();
	if (ParameterVariables.Num())
	{
		int32 MatchingIndex = 0;
		if (FNiagaraVariableSearch::Find(ParameterVariables.GetData(), Parameter, 0, ParameterVariables.Num(), IgnoreType, MatchingIndex))
		{
			return &ParameterVariables[MatchingIndex];
		}
	}
	return nullptr;
}

void FNiagaraParameterStore::RemoveParameters(FNiagaraParameterStore& DestStore)
{
	for (const FNiagaraVariableWithOffset& Parameter : ReadParameterVariables())
	{
		DestStore.RemoveParameter(Parameter);
	}
}

void FNiagaraParameterStore::Empty(bool bClearBindings)
{
#if WITH_EDITORONLY_DATA
	ParameterOffsets.Empty();
#endif // WITH_EDITORONLY_DATA

	SortedParameterOffsets.Empty();

	DEC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());
	ParameterData.Empty();
	INC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());

	DataInterfaces.Empty();
	UObjects.Empty();
	OriginalPositionData.Empty();

	if (bClearBindings)
	{
		UnbindAll();
	}
}

void FNiagaraParameterStore::Reset(bool bClearBindings)
{
#if WITH_EDITORONLY_DATA
	ParameterOffsets.Reset();
#endif // WITH_EDITORONLY_DATA

	SortedParameterOffsets.Reset();

	DEC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());
	ParameterData.Reset();
	INC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());

	DataInterfaces.Reset();
	UObjects.Reset();
	OriginalPositionData.Reset();

	if (bClearBindings)
	{
		UnbindAll();
	}
}

bool FNiagaraParameterStore::SetPositionParameterValue(const FVector& InValue, const FName& ParamName, bool bAdd)
{
	SetPositionData(ParamName, InValue);
	bPositionDataDirty = true;

	FNiagaraPosition Position = InValue;
	FNiagaraVariable Param(FNiagaraTypeDefinition::GetPositionDef(), ParamName);
	return SetParameterValue(Position, Param, bAdd);
}

const FVector* FNiagaraParameterStore::GetPositionParameterValue(const FName& ParamName) const
{
	if (const FVector* SourceData = GetPositionData(ParamName))
	{
		return SourceData;
	}
	return nullptr;
}

void FNiagaraParameterStore::ResolvePositions(FNiagaraLWCConverter LwcConverter)
{
	if (!bPositionDataDirty)
	{
		return;
	}
	bPositionDataDirty = false;
	
	for (const FNiagaraPositionSource& Data : OriginalPositionData)
	{
		FNiagaraVariable Param(FNiagaraTypeDefinition::GetPositionDef(), Data.Name);
		int32 Offset = IndexOf(Param);
		if (Offset != INDEX_NONE)
		{
			FNiagaraPosition SimPosition = LwcConverter.ConvertWorldToSimulationPosition(Data.Value);
			FNiagaraPosition* ParamData = reinterpret_cast<FNiagaraPosition*>( GetParameterData_Internal(Offset) );
			FMemory::Memcpy(ParamData, &SimPosition, sizeof(FNiagaraPosition));
		}
	}
	OnParameterChange();
}

bool FNiagaraParameterStore::SetParameterData(const uint8* Data, FNiagaraVariable Param, bool bAdd)
{
	checkSlow(Data != nullptr);
	FNiagaraTypeDefinition SourceType = Param.GetType();
	if (SourceType == FNiagaraTypeDefinition::GetPositionDef())
	{
		FNiagaraPosition Value = *reinterpret_cast<const FNiagaraPosition*>(Data);
		return SetPositionParameterValue((FVector)Value, Param.GetName(), bAdd);
	}
	
	int32 Offset = IndexOf(Param);
	if (Offset != INDEX_NONE)
	{
		checkSlow(!Param.IsDataInterface());
		uint8* Dest = GetParameterData_Internal(Offset);
		if (Dest != Data)
		{
			FNiagaraLwcStructConverter StructConverter = GetStructConverter(Param);
			if (StructConverter.IsValid())
			{
				StructConverter.ConvertDataToSimulation(Dest, Data);
			}
			else
			{
				FMemory::Memcpy(Dest, Data, Param.GetSizeInBytes());
			}
		}
		OnParameterChange();
		return true;
	}
	else
	{
		if (bAdd)
		{
			bool bInitInterfaces = false;
			bool bTriggerRebind = false;
			AddParameter(Param, bInitInterfaces, bTriggerRebind, &Offset);
			check(Offset != INDEX_NONE);
			uint8* Dest = GetParameterData_Internal(Offset);
			if (Dest != Data)
			{
				FNiagaraLwcStructConverter StructConverter = GetStructConverter(Param);
				if (StructConverter.IsValid())
				{
					StructConverter.ConvertDataToSimulation(Dest, Data);
				}
				else
				{
					FMemory::Memcpy(Dest, Data, Param.GetSizeInBytes());
				}
			}
			OnLayoutChange();
			return true;
		}
	}
	return false;
}

void FNiagaraParameterStore::OnLayoutChange()
{
	const int32 ExpectedSlack = PaddedParameterSize(ParameterData.Num());
	if (ParameterData.Max() < ExpectedSlack)
	{
		ParameterData.Reserve(ExpectedSlack);
	}
	Rebind();
	++LayoutVersion;

#if WITH_EDITOR
	if (bSuppressOnChanged == false)
	{
		OnChangedDelegate.Broadcast();
		OnStructureChangedDelegate.Broadcast();
	}
#endif
}

const FNiagaraVariableBase* FNiagaraParameterStore::FindVariable(const UNiagaraDataInterface* Interface) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraParameterStoreFindVar);
	int32 Idx = DataInterfaces.IndexOfByKey(Interface);
	if (Idx != INDEX_NONE)
	{
		for (const FNiagaraVariableWithOffset& ParamWithOffset : ReadParameterVariables())
		{
			if (ParamWithOffset.Offset == Idx && ParamWithOffset.GetType().GetClass() == Interface->GetClass())
			{
				return &ParamWithOffset;
			}
		}
	}
	return nullptr;
}

bool FNiagaraParameterStore::CopyParameterData(const FNiagaraVariable& Parameter, uint8* DestinationData) const
{
	if (const FNiagaraVariableWithOffset* NiagaraVariableWithOffset = FindParameterVariable(Parameter))
	{
		const uint8* SourceData = GetParameterData(NiagaraVariableWithOffset->Offset);
		if (NiagaraVariableWithOffset->StructConverter.IsValid())
		{
			NiagaraVariableWithOffset->StructConverter.ConvertDataFromSimulation(DestinationData, SourceData);
		}
		else
		{
			FMemory::Memcpy(DestinationData, SourceData, Parameter.GetSizeInBytes());
		}
		return true;
	}
	return false;
}

FNiagaraLwcStructConverter FNiagaraParameterStore::GetStructConverter(const FNiagaraVariable& Parameter) const
{
	auto ParameterVariables = ReadParameterVariables();
	if (ParameterVariables.Num())
	{
		int32 MatchingIndex = 0;
		if (FNiagaraVariableSearch::Find(ParameterVariables.GetData(), Parameter, 0, ParameterVariables.Num(), false, MatchingIndex))
		{
			return ParameterVariables[MatchingIndex].StructConverter;
		}
	}
	return FNiagaraLwcStructConverter();
}

const int32* FNiagaraParameterStore::FindParameterOffset(const FNiagaraVariableBase& Parameter, bool IgnoreType) const
{
#if WITH_EDITORONLY_DATA
	if (ParameterOffsets.Num())
	{
		UE_LOG(LogNiagara, Warning, TEXT("FindParameterOffset: ParameterOffsets.Num() should be 0 is %d, please investigate for %s"), ParameterOffsets.Num(), *GetPathNameSafe(Owner.Get()));
	}
	//check(!ParameterOffsets.Num()); // Migration to SortedParameterOffsets
#endif

	auto ParameterVariables = ReadParameterVariables();
	if (ParameterVariables.Num())
	{
		int32 MatchingIndex = 0;
		if (FNiagaraVariableSearch::Find(ParameterVariables.GetData(), Parameter, 0, ParameterVariables.Num(), IgnoreType, MatchingIndex))
		{
			return &ParameterVariables[MatchingIndex].Offset;
		}
	}
	return nullptr;
}

void FNiagaraParameterStore::PostLoad()
{
#if WITH_EDITORONLY_DATA
	// Convert ParameterOffsets map to the new SortedParameterOffsets array.
	if (ParameterOffsets.Num())
	{
		for (const TPair<FNiagaraVariable, int32>& ParamOffsetPair : ParameterOffsets)
		{
			SortedParameterOffsets.Emplace(ParamOffsetPair.Key, ParamOffsetPair.Value, FNiagaraLwcStructConverter());
		}
		ParameterOffsets.Empty();
	}

	// Check if the parameter guid mapping got tainted somehow and clear it if so
	TSet<FGuid> SeenGuids;
	for (const auto& Entry : ParameterGuidMapping)
	{
		if (SeenGuids.Contains(Entry.Value))
		{
			ParameterGuidMapping.Empty();
			break;
		}
		SeenGuids.Add(Entry.Value);
	}
#endif

	// Not always required if NIAGARA_VARIABLE_LEXICAL_SORTING
	SortParameters();
}

void FNiagaraParameterStore::SortParameters()
{
	SortedParameterOffsets.Sort([](const FNiagaraVariableWithOffset& Lhs, const FNiagaraVariableWithOffset& Rhs)
	{
		return FNiagaraVariableSearch::Compare(Lhs, Rhs) < 0;
	});
}

int32 FNiagaraParameterStore::PaddedParameterSize(int32 ParameterSize)
{
	// The VM require that the parameter data we send it in FNiagaraScriptExecutionContext::Execute
	// is aligned to VECTOR_WIDTH_BYTES *and* is padded with an additional VECTOR_WIDTH_BYTES.
	// This is due to possible unaligned reads
	return Align(ParameterSize, VECTOR_WIDTH_BYTES) + VECTOR_WIDTH_BYTES;
}

void FNiagaraParameterStore::AssignParameterData(TConstArrayView<uint8> SourceParameterData)
{
	DEC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());
	ParameterData.Reserve(PaddedParameterSize(SourceParameterData.Num()));
	ParameterData = SourceParameterData;
	INC_MEMORY_STAT_BY(STAT_NiagaraParamStoreMemory, ParameterData.GetAllocatedSize());
}

#if WITH_EDITOR
FDelegateHandle FNiagaraParameterStore::AddOnChangedHandler(FOnChanged::FDelegate InOnChanged)
{
	return OnChangedDelegate.Add(InOnChanged);
}

void FNiagaraParameterStore::RemoveOnChangedHandler(FDelegateHandle DelegateHandle)
{
	OnChangedDelegate.Remove(DelegateHandle);
}

void FNiagaraParameterStore::RemoveAllOnChangedHandlers(const void* InUserObject)
{
	OnChangedDelegate.RemoveAll(InUserObject);
}
#endif
//////////////////////////////////////////////////////////////////////////

