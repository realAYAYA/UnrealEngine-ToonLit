// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.generated.h"

// When not cooked, sort by actual name to ensure deterministic cooked data
#define NIAGARA_VARIABLE_LEXICAL_SORTING WITH_EDITORONLY_DATA

struct FNiagaraParameterStore;

USTRUCT()
struct FNiagaraBoundParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FNiagaraVariable Parameter;
	UPROPERTY()
	int32 SrcOffset = 0;
	UPROPERTY()
	int32 DestOffset = 0;

};

USTRUCT()
struct FNiagaraPositionSource
{
	GENERATED_USTRUCT_BODY()

	FNiagaraPositionSource() : Value(FVector::ZeroVector) {}
	FNiagaraPositionSource(FName InName, FVector InValue) : Name(InName), Value(InValue) {}
	
	UPROPERTY()
	FName Name;
	
	UPROPERTY()
	FVector Value;
};

typedef TArray<FNiagaraBoundParameter> FNiagaraBoundParameterArray;

//Binding from one parameter store to another.
//This does no tracking of lifetimes etc so the owner must ensure safe use and rebinding when needed etc.
struct FNiagaraParameterStoreBinding
{
	struct FParameterBinding
	{
		FParameterBinding(int32 InSrcOffset, int32 InDestOffset, int32 InSize)
			: SrcOffset((uint16)InSrcOffset), DestOffset((uint16)InDestOffset), Size((uint16)InSize)
		{
			//No way these offsets could ever be higher but check just in case.
			check(InSrcOffset < (int32)0xFFFF);
			check(InDestOffset < (int32)0xFFFF);
			check(InSize < (int32)0xFFFF);
		}
		FORCEINLINE bool operator==(const FParameterBinding& Other)const
		{
			return SrcOffset == Other.SrcOffset && DestOffset == Other.DestOffset && Size == Other.Size;
		}
		uint16 SrcOffset;
		uint16 DestOffset;
		uint16 Size;
	};
	/** Bindings of parameter data. Src offset, Dest offset and Size. */
	TArray<FParameterBinding> ParameterBindings;

	struct FInterfaceBinding
	{
		FInterfaceBinding(int32 InSrcOffset, int32 InDestOffset) 
			: SrcOffset((uint16)InSrcOffset), DestOffset((uint16)InDestOffset)
		{
			//No way these offsets could ever be higher but check just in case.
			check(InSrcOffset < (int32)0xFFFF);
			check(InDestOffset < (int32)0xFFFF);
		}
		FORCEINLINE bool operator==(const FInterfaceBinding& Other)const
		{
			return SrcOffset == Other.SrcOffset && DestOffset == Other.DestOffset;
		}
		uint16 SrcOffset;
		uint16 DestOffset;
	};
	/** Bindings of data interfaces. Src and Dest offsets.*/
	TArray<FInterfaceBinding> InterfaceBindings;

	struct FUObjectBinding
	{
		FUObjectBinding(int32 InSrcOffset, int32 InDestOffset)
			: SrcOffset((uint16)InSrcOffset), DestOffset((uint16)InDestOffset)
		{
			//No way these offsets could ever be higher but check just in case.
			check(InSrcOffset < (int32)0xFFFF);
			check(InDestOffset < (int32)0xFFFF);
		}
		FORCEINLINE bool operator==(const FUObjectBinding& Other)const
		{
			return SrcOffset == Other.SrcOffset && DestOffset == Other.DestOffset;
		}
		uint16 SrcOffset;
		uint16 DestOffset;
	};
	/** Bindings of UObject params. Src and Dest offsets.*/
	TArray<FUObjectBinding> UObjectBindings;

	FNiagaraParameterStoreBinding()
	{
	}
	
	FORCEINLINE_DEBUGGABLE void Empty(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore);
	FORCEINLINE_DEBUGGABLE bool Initialize(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, const FNiagaraBoundParameterArray* BoundParameters = nullptr);
	FORCEINLINE_DEBUGGABLE bool VerifyBinding(const FNiagaraParameterStore* DestStore, const FNiagaraParameterStore* SrcStore)const;
	FORCEINLINE_DEBUGGABLE void Tick(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, bool bForce = false);
	FORCEINLINE_DEBUGGABLE void Dump(const FNiagaraParameterStore* DestStore, const FNiagaraParameterStore* SrcStore)const;
	/** TODO: Merge contiguous ranges into a single binding? */
	//FORCEINLINE_DEBUGGABLE void Optimize();

	static void GetBindingData(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, FNiagaraBoundParameterArray& OutBoundParameters);

private:
	
	template <typename TVisitor>
	FORCEINLINE_DEBUGGABLE static void MatchParameters(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, TVisitor Visitor);

	bool BindParameters(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, const FNiagaraBoundParameterArray* BoundParameters = nullptr);
};

USTRUCT()
struct FNiagaraVariableWithOffset : public FNiagaraVariableBase
{
	GENERATED_USTRUCT_BODY()

	// Those constructor enforce that there are no data allocated.
	FORCEINLINE FNiagaraVariableWithOffset() : Offset(INDEX_NONE) {}
	FORCEINLINE FNiagaraVariableWithOffset(const FNiagaraVariableWithOffset& InRef) : FNiagaraVariableBase(InRef.GetType(), InRef.GetName()), Offset(InRef.Offset), StructConverter(InRef.StructConverter) {}
	FORCEINLINE FNiagaraVariableWithOffset(const FNiagaraVariableBase& InVariable, int32 InOffset, const FNiagaraLwcStructConverter& InStructConverter) : FNiagaraVariableBase(InVariable.GetType(), InVariable.GetName()), Offset(InOffset), StructConverter(InStructConverter) {}

	bool Serialize(FArchive& Ar);
#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif

	UPROPERTY()
	int32 Offset;

	UPROPERTY()
	FNiagaraLwcStructConverter StructConverter;
};

template<>
struct TStructOpsTypeTraits<FNiagaraVariableWithOffset> : public TStructOpsTypeTraitsBase2<FNiagaraVariableWithOffset>
{
	enum
	{
		WithSerializer = true,
#if WITH_EDITORONLY_DATA
		WithPostSerialize = true,
#endif
	};
};

/** Base storage class for Niagara parameter values. */
USTRUCT()
struct NIAGARA_API FNiagaraParameterStore
{
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	struct FScopedSuppressOnChanged : TGuardValue<bool>
	{
		FScopedSuppressOnChanged(FNiagaraParameterStore& TargetStore)
			: TGuardValue(TargetStore.bSuppressOnChanged, true)
		{
		}
	};
	
	DECLARE_MULTICAST_DELEGATE(FOnStructureChanged)
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnParameterRenamed, FNiagaraVariable OldVariable, FName NewName)
#endif

	GENERATED_USTRUCT_BODY()

	/** The View of the set of variables represented by this ParameterStore.  By default it will be the variables defined by the
	SortedParameterOffsets, but child classes can override it so that we can share the list of Variables across different
	instances as it's generally read only data in game */
	virtual TArrayView<const FNiagaraVariableWithOffset> ReadParameterVariables() const { return MakeArrayView(SortedParameterOffsets); }

private:
	/** Owner of this store. Used to provide an outer to data interfaces in this store. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> Owner;
	
#if WITH_EDITORONLY_DATA
	/** Map from parameter defs to their offset in the data table or the data interface. TODO: Separate out into a layout and instance class to reduce duplicated data for this?  */
	UPROPERTY(meta=(DeprecatedProperty))
	TMap<FNiagaraVariable, int32> ParameterOffsets;
#endif // WITH_EDITORONLY_DATA

	/** Storage for the set of variables that are represented by this ParameterStore.  Shouldn't be accessed directly, instead use
	ReadParameterVariables() */
	UPROPERTY()
	TArray<FNiagaraVariableWithOffset> SortedParameterOffsets;

	/** Buffer containing parameter data. Indexed using offsets in ParameterOffsets */
	UPROPERTY()
	TArray<uint8> ParameterData;
	
	/** Data interfaces for this script. Possibly overridden with externally owned interfaces. Also indexed by ParameterOffsets. */
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraDataInterface>> DataInterfaces;

	/** UObjects referenced by this store. Also indexed by ParameterOffsets.*/
	UPROPERTY()
	TArray<TObjectPtr<UObject>> UObjects;

	/** Holds position type source data to be later converted to LWC format. We use an array here instead of a map to save some memory and because linear search is faster with the few elements in here. */
	UPROPERTY()
	TArray<FNiagaraPositionSource> OriginalPositionData;	
	
	/** Bindings between this parameter store and others we push data into when we tick. */
	typedef TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding> BindingPair;
	TArray<BindingPair> Bindings;

	/** Parameter stores we've been bound to and are feeding data into us. */
	TArray<FNiagaraParameterStore*> SourceStores;

	/** Marks our parameters as dirty. They will be pushed to any bound stores on tick if true. */
	uint32 bParametersDirty : 1;
	/** Marks our interfaces as dirty. They will be pushed to any bound stores on tick if true. */
	uint32 bInterfacesDirty : 1;
	/** Marks our UObjects as dirty. They will be pushed to any bound stores on tick if true. */
	uint32 bUObjectsDirty : 1;

	uint32 bPositionDataDirty : 1; 

	/** Uniquely identifies the current layout of this parameter store for detecting layout changes. */
	uint32 LayoutVersion;

#if WITH_EDITOR
	FOnChanged OnChangedDelegate;
	bool bSuppressOnChanged = false;
	FOnStructureChanged OnStructureChangedDelegate;
	FOnParameterRenamed OnParameterRenamedDelegate;
#endif

	void SetPositionData(const FName& Name, const FVector& Position);
	bool HasPositionData(const FName& Name) const;
	const FVector* GetPositionData(const FName& Name) const;
	void RemovePositionData(const FName& Name);

public:
	FNiagaraParameterStore();
	FNiagaraParameterStore(const FNiagaraParameterStore& Other);
	FNiagaraParameterStore& operator=(const FNiagaraParameterStore& Other);

	virtual ~FNiagaraParameterStore();
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString DebugName;

	/** Guid data to remap rapid iteration parameters after a function input was renamed. */
	UPROPERTY()
	TMap<FNiagaraVariable, FGuid> ParameterGuidMapping;
#endif

	void SetOwner(UObject* InOwner);
	UObject* GetOwner() const { return Owner.Get(); }

	void Dump();
	void DumpParameters(bool bDumpBindings = false)const;

	FORCEINLINE uint32 GetParametersDirty() const { return bParametersDirty; }
	FORCEINLINE uint32 GetInterfacesDirty() const { return bInterfacesDirty; }
	FORCEINLINE uint32 GetUObjectsDirty() const { return bUObjectsDirty; }
	FORCEINLINE uint32 GetPositionDataDirty() const { return bPositionDataDirty; }

	FORCEINLINE void MarkParametersDirty() { bParametersDirty = true; }
	FORCEINLINE void MarkInterfacesDirty() { bInterfacesDirty = true; }
	FORCEINLINE void MarkUObjectsDirty() { bUObjectsDirty = true; }
	FORCEINLINE void MarkPositionDataDirty() { bPositionDataDirty = true; }

	FORCEINLINE uint32 GetLayoutVersion() const { return LayoutVersion; }

	/** Binds this parameter store to another, by default if we find no matching parameters we will not maintain a pointer to the store. */
	void Bind(FNiagaraParameterStore* DestStore, const FNiagaraBoundParameterArray* BoundParameters = nullptr);
	/** Unbinds this store form one it's bound to. */
	void Unbind(FNiagaraParameterStore* DestStore);
	/** Unbinds this store from all source and destination stores. */
	void UnbindAll();
	/** Recreates any bindings to reflect a layout change etc. */
	void Rebind();
	/** Recreates any bindings to reflect a layout change etc. */
	void TransferBindings(FNiagaraParameterStore& OtherStore);
	/** Handles any update such as pushing parameters to bound stores etc. */
	FORCEINLINE_DEBUGGABLE void Tick();
	/** Unbinds this store from all stores it's being driven by. */
	void UnbindFromSourceStores();
	
	bool VerifyBinding(const FNiagaraParameterStore* InDestStore) const;

	void CheckForNaNs() const;

	/**
	Adds the passed parameter to this store.
	Does nothing if this parameter is already present.
	Returns true if we added a new parameter.
	*/
	virtual bool AddParameter(const FNiagaraVariable& Param, bool bInitialize = true, bool bTriggerRebind = true, int32* OutOffset = nullptr);

#if WITH_EDITORONLY_DATA
	virtual void ConvertParameterType(const FNiagaraVariable& ExistingParam, const FNiagaraTypeDefinition& NewType);
	
	template<typename BufferType>
	void AddConstantBuffer()
	{
		for (const FNiagaraVariable& BufferVariable : BufferType::GetVariables())
		{
			AddParameter(BufferVariable, true, false);
		}
	}
#endif

	/** Removes the passed parameter if it exists in the store. */
	virtual bool RemoveParameter(const FNiagaraVariableBase& Param);

	/** Renames the passed parameter. */
	virtual void RenameParameter(const FNiagaraVariableBase& Param, FName NewName);

	/** Changes the type of the passed parameter. */
	virtual void ChangeParameterType(const FNiagaraVariableBase& Param, const FNiagaraTypeDefinition& NewType);

	/** Removes all parameters from this store and releases any data. */
	virtual void Empty(bool bClearBindings = true);

	/** Removes all parameters from this store but doesn't change memory allocations. */
	virtual void Reset(bool bClearBindings = true);

	FORCEINLINE void GetParameters(TArray<FNiagaraVariable>& OutParameters) const
	{
		auto ParameterVariables = ReadParameterVariables();

		OutParameters.Reserve(ParameterVariables.Num());
		for (const FNiagaraVariableWithOffset& ParamWithOffset : ParameterVariables)
		{
			OutParameters.Add(ParamWithOffset);
		}
	}

	FORCEINLINE TArray<FNiagaraParameterStore*>& GetSourceParameterStores() { return SourceStores; }

	FORCEINLINE const TArray<UObject*>& GetUObjects()const { return UObjects; }
	FORCEINLINE const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const { return DataInterfaces; }
	FORCEINLINE const TArray<uint8>& GetParameterDataArray()const { return ParameterData; }

	FORCEINLINE int32 Num() const {return SortedParameterOffsets.Num(); }
	FORCEINLINE bool IsEmpty() const { return SortedParameterOffsets.Num() == 0; }

	virtual void SanityCheckData(bool bInitInterfaces = true);

	// Called to initially set up the parameter store to *exactly* match the input store (other than any bindings and the internal name of it).
	virtual void InitFromSource(const FNiagaraParameterStore* SrcStore, bool bNotifyAsDirty);

	/** Gets the index of the passed parameter. If it is a data interface, this is an offset into the data interface table, otherwise a byte offset into the parameter data buffer. */
	FORCEINLINE_DEBUGGABLE int32 IndexOf(const FNiagaraVariable& Parameter) const
	{
		const int32* Off = FindParameterOffset(Parameter);
		return Off ? *Off : (int32)INDEX_NONE;
	}
	
	virtual const FNiagaraVariableWithOffset* FindParameterVariable(const FNiagaraVariable& Parameter, bool IgnoreType = false) const;

	/** Gets the typed parameter data. */
	template<typename T>
	FORCEINLINE_DEBUGGABLE void GetParameterValue(T& OutValue, const FNiagaraVariable& Parameter)const
	{
		check(Parameter.GetSizeInBytes() == sizeof(T));
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			OutValue = *(const T*)(GetParameterData(Offset));
		}
	}

	template<typename T>
	FORCEINLINE_DEBUGGABLE T GetParameterValue(const FNiagaraVariable& Parameter)const
	{
		check(Parameter.GetSizeInBytes() == sizeof(T));
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			return *(const T*)(GetParameterData(Offset));
		}
		return T();
	}

	FORCEINLINE const uint8* GetParameterData(int32 Offset)const
	{
		return ParameterData.GetData() + Offset;
	}

	/** Returns the parameter data for the passed parameter if it exists in this store. Null if not. */
	FORCEINLINE_DEBUGGABLE const uint8* GetParameterData(const FNiagaraVariable& Parameter)const
	{
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			return GetParameterData(Offset);
		}
		return nullptr;
	}

	/** Copies the data stored for the given variable to the target pointer. This method automatically converts custom struct data back to lwc types.
	 * Just using the raw parameter store pointer via IndexOf() will be wrong if the target struct contains a lwc type like FVector.
	 * Returns true if the data was copied, false if the parameter could not be found in the store.
	 */
	bool CopyParameterData(const FNiagaraVariable& Parameter, uint8* DestinationData) const;

	/** Returns the data interface at the passed offset. */
	FORCEINLINE UNiagaraDataInterface* GetDataInterface(int32 Offset)const
	{
		if (DataInterfaces.IsValidIndex(Offset))
		{
			return DataInterfaces[Offset];
		}

		return nullptr;
	}

	/** Returns the data interface for the passed parameter if it exists in this store. */
	FORCEINLINE_DEBUGGABLE UNiagaraDataInterface* GetDataInterface(const FNiagaraVariable& Parameter)const
	{
		int32 Offset = IndexOf(Parameter);
		UNiagaraDataInterface* Interface = GetDataInterface(Offset);
		checkSlow(!Interface || Parameter.GetType().GetClass() == Interface->GetClass());
		return Interface;
	}

	/** Returns a struct converter for the given variable, if the store contains the variable and it's a LWC type. */
	FNiagaraLwcStructConverter GetStructConverter(const FNiagaraVariable& Parameter) const;

	/** Returns the associated FNiagaraVariable for the passed data interface if it exists in the store. Null if not.*/
	const FNiagaraVariableBase* FindVariable(const UNiagaraDataInterface* Interface) const;

	virtual const int32* FindParameterOffset(const FNiagaraVariableBase& Parameter, bool IgnoreType = false) const;

	void PostLoad();
	void SortParameters();

	/** Returns the UObject at the passed offset. */
	FORCEINLINE UObject* GetUObject(int32 Offset)const
	{
		if (UObjects.IsValidIndex(Offset))
		{
			return UObjects[Offset];
		}

		return nullptr;
	}

	FORCEINLINE UObject* GetUObject(const FNiagaraVariable& Parameter)const
	{
		int32 Offset = IndexOf(Parameter);
		UObject* Obj = GetUObject(Offset);
		checkSlow(!Obj || Obj->IsA(Parameter.GetType().GetClass()));
		return Obj;
	}

	/** Copies the passed parameter from this parameter store into another. */
	FORCEINLINE_DEBUGGABLE void CopyParameterData(FNiagaraParameterStore& DestStore, const FNiagaraVariable& Parameter) const
	{
		int32 DestIndex = DestStore.IndexOf(Parameter);
		int32 SrcIndex = IndexOf(Parameter);
		if (DestIndex != INDEX_NONE && SrcIndex != INDEX_NONE)
		{
			if (Parameter.IsDataInterface())
			{
				ensure(DestStore.DataInterfaces.IsValidIndex(DestIndex));
				UNiagaraDataInterface* DestDataInterface = DestStore.DataInterfaces[DestIndex];
				if (DestDataInterface == nullptr)
				{
					if (ensureMsgf(DestStore.Owner != nullptr, TEXT("Destination data interface pointer was null and a new one couldn't be created because the destination store's owner pointer was also null.")))
					{
						UE_LOG(LogNiagara, Warning, TEXT("While trying to copy parameter data the destination data interface was null, creating a new one.  Parameter: %s Destination Store Owner: %s"), *Parameter.GetName().ToString(), *GetPathNameSafe(DestStore.Owner.Get()));
						DestDataInterface = NewObject<UNiagaraDataInterface>(DestStore.Owner.Get(), Parameter.GetType().GetClass(), NAME_None, RF_Transactional | RF_Public);
						DestStore.DataInterfaces[DestIndex] = DestDataInterface;
					}
					else
					{
						return;
					}
				}
				DataInterfaces[SrcIndex]->CopyTo(DestDataInterface);
				DestStore.OnInterfaceChange();
			}
			else if (Parameter.IsUObject())
			{
				DestStore.SetUObject(GetUObject(SrcIndex), DestIndex);
			}
			else if (Parameter.GetType() == FNiagaraTypeDefinition::GetPositionDef())
			{
				const FVector* SourceVector = GetPositionParameterValue(Parameter.GetName());
				DestStore.SetPositionParameterValue(SourceVector ? *SourceVector : FVector::ZeroVector, Parameter.GetName());
			}
			else
			{
				DestStore.SetParameterData(GetParameterData(SrcIndex), DestIndex, Parameter.GetSizeInBytes());
			}
		}
	}

	enum class EDataInterfaceCopyMethod
	{
		/** A new data interface will be created and it will be synchronized using the CopyTo method. */
		Value,
		/** A reference to the source data interface will be added to the destination. */
		Reference,
		/** Do not copy data interfaces.  This will cause an assert if there are data interfaces in the source
		  * store, and bOnlyAdd is false. */
		None
	};

	/** Copies all parameters from this parameter store into another.*/
	void CopyParametersTo(FNiagaraParameterStore& DestStore, bool bOnlyAdd, EDataInterfaceCopyMethod DataInterfaceCopyMethod = EDataInterfaceCopyMethod::None) const;

	/** Remove all parameters from this parameter store from another.*/
	void RemoveParameters(FNiagaraParameterStore& DestStore);

	FString ToString() const;

	virtual bool SetPositionParameterValue(const FVector& InValue, const FName& ParamName, bool bAdd=false);
	virtual const FVector* GetPositionParameterValue(const FName& ParamName) const;
	void ResolvePositions(FNiagaraLWCConverter LwcConverter);

	template<typename T>
	FORCEINLINE_DEBUGGABLE bool SetParameterValue(const T& InValue, const FNiagaraVariable& Param, bool bAdd=false)
	{
		check(Param.GetSizeInBytes() == sizeof(T));
#if WITH_EDITOR
		ensure(FNiagaraTypeHelper::IsLWCType(Param.GetType()) == false);
		if (Param.GetType() == FNiagaraTypeDefinition::GetPositionDef())
		{
			check(HasPositionData(Param.GetName()));
		}
#endif
		int32 Offset = IndexOf(Param);
		if (Offset != INDEX_NONE)
		{
			//*(T*)(GetParameterData_Internal(Offset)) = InValue;
			//Until we solve our alignment issues, temporarily just doing a memcpy here.
			T* ParamData = reinterpret_cast<T*>( GetParameterData_Internal(Offset) );
			FMemory::Memcpy(ParamData, &InValue, sizeof(T));
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
				//Until we solve our alignment issues, temporarily just doing a memcpy here.
				//*(T*)(GetParameterData_Internal(Offset)) = InValue;
				T* ParamData = reinterpret_cast<T*>(GetParameterData_Internal(Offset));
				FMemory::Memcpy(ParamData, &InValue, sizeof(T));
				OnLayoutChange();
				return true;
			}
		}
		return false;
	}

	FORCEINLINE_DEBUGGABLE void SetParameterData(const uint8* Data, int32 Offset, int32 Size)
	{
		checkSlow(Data != nullptr);
		checkSlow((Offset + Size) <= ParameterData.Num());
		uint8* Dest = GetParameterData_Internal(Offset);
		if (Dest != Data)
		{
			FMemory::Memcpy(Dest, Data, Size);
		}
		OnParameterChange();
	}

	bool SetParameterData(const uint8* Data, FNiagaraVariable Param, bool bAdd = false);

	FORCEINLINE_DEBUGGABLE void SetDataInterface(UNiagaraDataInterface* InInterface, int32 Offset)
	{
		DataInterfaces[Offset] = InInterface;
		OnInterfaceChange();
	}

	FORCEINLINE_DEBUGGABLE void SetDataInterface(UNiagaraDataInterface* InInterface, const FNiagaraVariable& Parameter)
	{
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			DataInterfaces[Offset] = InInterface;
			OnInterfaceChange();
		}
	}

	FORCEINLINE_DEBUGGABLE void SetUObject(UObject* InObject, int32 Offset)
	{
		UObjects[Offset] = InObject;
		OnUObjectChange();
	}

	FORCEINLINE_DEBUGGABLE void SetUObject(UObject* InObject, const FNiagaraVariable& Parameter)
	{
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			UObjects[Offset] = InObject;
			OnUObjectChange();
		}
	}

	FORCEINLINE void OnParameterChange() 
	{ 
		bParametersDirty = true;
#if WITH_EDITOR
		if (bSuppressOnChanged == false)
		{
			OnChangedDelegate.Broadcast();
		}
#endif
	}

	FORCEINLINE void OnInterfaceChange() 
	{ 
		bInterfacesDirty = true;
#if WITH_EDITOR
		if (bSuppressOnChanged == false)
		{
			OnChangedDelegate.Broadcast();
		}
#endif
	}

	FORCEINLINE void OnUObjectChange()
	{
		bUObjectsDirty = true;
#if WITH_EDITOR
		if (bSuppressOnChanged == false)
		{
			OnChangedDelegate.Broadcast();
		}
#endif
	}

	FORCEINLINE void PostGenericEditChange()
	{
		bUObjectsDirty = true;
		bInterfacesDirty = true;
		bParametersDirty = true;
#if WITH_EDITOR
		if (bSuppressOnChanged == false)
		{
			OnChangedDelegate.Broadcast();
		}
#endif
	}

#if WITH_EDITOR
	FDelegateHandle AddOnChangedHandler(FOnChanged::FDelegate InOnChanged);
	void RemoveOnChangedHandler(FDelegateHandle DelegateHandle);
	void RemoveAllOnChangedHandlers(const void* InUserObject);

	FOnStructureChanged& OnStructureChanged() { return OnStructureChangedDelegate; }
	FOnParameterRenamed& OnParameterRenamed() { return OnParameterRenamedDelegate; }
#endif

	void TriggerOnLayoutChanged() { OnLayoutChange(); }

protected:
	void TickBindings();
	void OnLayoutChange();
	void CopySortedParameterOffsets(TArrayView<const FNiagaraVariableWithOffset> Src);
	void AssignParameterData(TConstArrayView<uint8> SourceParameterData);
	static int32 PaddedParameterSize(int32 ParameterSize);

	/** Returns the parameter data at the passed offset. */
	FORCEINLINE uint8* GetParameterData_Internal(int32 Offset) 
	{
		return ParameterData.GetData() + Offset;
	}

	template<typename ParamType>
	void SetParameterByOffset(uint32 ParamOffset, const ParamType& Param)
	{
		ParamType* ParamPtr = (ParamType*)(GetParameterDataArray().GetData() + ParamOffset);
		*ParamPtr = Param;
		//SetParameterData((const uint8*)&Param, ParamOffset, sizeof(ParamType)); // TODO why aren't we using this path instead of SetParametersByOffset?
	}

	void SetParameterDataArray(const TArray<uint8>& InParameterDataArray, bool bNotifyAsDirty = true);
	void SetDataInterfaces(const TArray<UNiagaraDataInterface*>& InDataInterfaces, bool bNotifyAsDirty = true);
	void SetUObjects(const TArray<UObject*>& InUObjects, bool bNotifyAsDirty = true);
	void SetOriginalPositionData(const TArray<FNiagaraPositionSource>& InOriginalPositionData);

	friend struct FNiagaraParameterStoreToDataSetBinding;    // this should be the only class calling SetParameterByOffset
};

template<>
FORCEINLINE bool FNiagaraParameterStore::SetParameterValue(const FVector3d& InValue, const FNiagaraVariable& Param, bool bAdd)
{
	return SetParameterValue((FVector3f)InValue, Param, bAdd);
}

template<>
FORCEINLINE bool FNiagaraParameterStore::SetParameterValue(const FVector4d& InValue, const FNiagaraVariable& Param, bool bAdd)
{
	return SetParameterValue((FVector4f)InValue, Param, bAdd);
}

template<>
FORCEINLINE bool FNiagaraParameterStore::SetParameterValue(const FQuat4d& InValue, const FNiagaraVariable& Param, bool bAdd)
{
	return SetParameterValue((FQuat4f)InValue, Param, bAdd);
}

FORCEINLINE_DEBUGGABLE void FNiagaraParameterStore::Tick()
{
#if NIAGARA_NAN_CHECKING
	CheckForNaNs();
#endif
	if (Bindings.Num() > 0 && (bParametersDirty || bInterfacesDirty || bUObjectsDirty))
	{
		TickBindings();
	}

	//We have to have ticked all our source stores before now.
	bParametersDirty = false;
	bInterfacesDirty = false;
	bUObjectsDirty = false;
}

FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::Empty(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore)
{
	if (DestStore)
	{
		//UE_LOG(LogNiagara, Log, TEXT("Remove Src Binding: Src: 0x%p - Dst: 0x%p"), SrcStore, DestStore);
		DestStore->GetSourceParameterStores().RemoveSingleSwap(SrcStore, false);
	}
	DestStore = nullptr;
	ParameterBindings.Reset();
	InterfaceBindings.Reset();
	UObjectBindings.Reset();
}

FORCEINLINE_DEBUGGABLE bool FNiagaraParameterStoreBinding::Initialize(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, const FNiagaraBoundParameterArray* BoundParameters)
{
	checkSlow(DestStore);
	checkSlow(SrcStore);

	if (BindParameters(DestStore, SrcStore, BoundParameters))
	{
		//UE_LOG(LogNiagara, Log, TEXT("Add Src Binding: Src: 0x%p - Dst: 0x%p"), SrcStore, DestStore);
		DestStore->GetSourceParameterStores().AddUnique(SrcStore);
		return true;
	}
	else
	{
		return false;
	}
}

FORCEINLINE_DEBUGGABLE bool FNiagaraParameterStoreBinding::VerifyBinding(const FNiagaraParameterStore* DestStore, const FNiagaraParameterStore* SrcStore)const
{
	bool bBindingValid = true;
#if WITH_EDITORONLY_DATA
	TArray<FName, TInlineAllocator<32>> MissingParameterNames;
	for (const FNiagaraVariableWithOffset& ParamWithOffset : DestStore->ReadParameterVariables())
	{
		const FNiagaraVariable& Parameter = ParamWithOffset;
		int32 DestOffset = ParamWithOffset.Offset;
		int32 SrcOffset = SrcStore->IndexOf(Parameter);

		if (SrcOffset == INDEX_NONE)
		{
			continue;//Parameter is not present in SrcStore so shouldn't/cant be bound.
		}

		if (Parameter.IsDataInterface())
		{
			if (InterfaceBindings.Find(FInterfaceBinding(SrcOffset, DestOffset)) == INDEX_NONE)
			{
				MissingParameterNames.Add(Parameter.GetName());
				bBindingValid = false;
			}
		}
		else if (Parameter.IsUObject())
		{
			if (UObjectBindings.Find(FUObjectBinding(SrcOffset, DestOffset)) == INDEX_NONE)
			{
				MissingParameterNames.Add(Parameter.GetName());
				bBindingValid = false;
			}
		}
		else
		{
			if (ParameterBindings.Find(FParameterBinding(SrcOffset, DestOffset, Parameter.GetSizeInBytes())) == INDEX_NONE)
			{
				MissingParameterNames.Add(Parameter.GetName());
				bBindingValid = false;
			}
		}
	}

	if(MissingParameterNames.Num())
	{
		UE_LOG(LogNiagara, Warning, TEXT("Invalid ParameterStore Binding: Parameters missing from binding between %s and %s. Stores should have been rebound when one changed!"), *SrcStore->DebugName, *DestStore->DebugName);
		for (FName MissingParam : MissingParameterNames)
		{
			UE_LOG(LogNiagara, Warning, TEXT("%s"), *MissingParam.ToString());
		}
	}
#endif
	return bBindingValid;
}

/** Merge contiguous ranges into a single binding. */
// FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::Optimize()
// {
// 	//TODO
// }

FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::Tick(FNiagaraParameterStore* DestStore, FNiagaraParameterStore* SrcStore, bool bForce)
{
	if (SrcStore->GetParametersDirty() || bForce)
	{
		for (FParameterBinding& Binding : ParameterBindings)
		{
			DestStore->SetParameterData(SrcStore->GetParameterData(Binding.SrcOffset), Binding.DestOffset, Binding.Size);
		}
	}

	if (SrcStore->GetInterfacesDirty() || bForce)
	{
		for (FInterfaceBinding& Binding : InterfaceBindings)
		{
			DestStore->SetDataInterface(SrcStore->GetDataInterface(Binding.SrcOffset), Binding.DestOffset);
		}
	}

	if (SrcStore->GetUObjectsDirty() || bForce)
	{
		for (FUObjectBinding& Binding : UObjectBindings)
		{
			DestStore->SetUObject(SrcStore->GetUObject(Binding.SrcOffset), Binding.DestOffset);
		}
	}

#if NIAGARA_NAN_CHECKING
	DestStore->CheckForNaNs();
#endif
}

FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::Dump(const FNiagaraParameterStore* DestStore, const FNiagaraParameterStore* SrcStore)const
{
#if WITH_EDITORONLY_DATA
	UE_LOG(LogNiagara, Log, TEXT("\n\nDest Store: %s\n"), *DestStore->DebugName);
	for (const FParameterBinding& Binding : ParameterBindings)
	{
		ensure(Binding.Size != 0);
		ensure(Binding.SrcOffset != -1);
		ensure(Binding.DestOffset != -1);
		FNiagaraVariable Param;
		bool bFound = false;
		for (const FNiagaraVariableWithOffset& ParamWithOffset : DestStore->ReadParameterVariables())
		{
			if (ParamWithOffset.Offset == Binding.DestOffset && !ParamWithOffset.IsDataInterface())
			{
				bFound = true;
				Param = ParamWithOffset;
			}
		}
		if (ensure(bFound))
		{
			UE_LOG(LogNiagara, Log, TEXT("| Param | %s %s: Src:%d - Dest:%d - Size:%d\n"), *Param.GetType().GetName(), *Param.GetName().ToString(), Binding.SrcOffset, Binding.DestOffset, Binding.Size);

			//Also ensure the param has been pushed correctly.
			const uint8* SrcData = SrcStore->GetParameterData(Binding.SrcOffset);
			const uint8* DestData = DestStore->GetParameterData(Binding.DestOffset);
			if (!ensure(FMemory::Memcmp(SrcData, DestData, Binding.Size) == 0))
			{
				UE_LOG(LogNiagara, Log, TEXT("Parameter in dest store has incorrect data!\n"));
			}
		}
		else
		{
			UE_LOG(LogNiagara, Log, TEXT("Failed to find matching param in bound store!\n"));
		}
	}

	for (const FInterfaceBinding& Binding : InterfaceBindings)
	{
		ensure(Binding.SrcOffset != -1);
		ensure(Binding.DestOffset != -1);
		FNiagaraVariable Param;
		bool bFound = false;
		for (const FNiagaraVariableWithOffset& ParamWithOffset : DestStore->ReadParameterVariables())
		{
			if (ParamWithOffset.Offset == Binding.DestOffset && ParamWithOffset.IsDataInterface())
			{
				bFound = true;
				Param = ParamWithOffset;
			}
		}
		if (ensure(bFound))
		{
			UE_LOG(LogNiagara, Log, TEXT("| DI | %s %s: Src:%d - Dest:%d\n"), *Param.GetType().GetName(), *Param.GetName().ToString(), Binding.SrcOffset, Binding.DestOffset);

			//Also ensure the param has been pushed correctly.
			const UNiagaraDataInterface* SrcData = SrcStore->GetDataInterfaces()[Binding.SrcOffset];
			const UNiagaraDataInterface* DestData = DestStore->GetDataInterfaces()[Binding.DestOffset];
			if (!ensure(SrcData == DestData))
			{
				UE_LOG(LogNiagara, Log, TEXT("Data interface parameter in dest store is incorrect!\n"));
			}
		}
		else
		{
			UE_LOG(LogNiagara, Log, TEXT("Failed to find matching data interface param in bound store!\n"));
		}
	}

	for (const FUObjectBinding& Binding : UObjectBindings)
	{
		ensure(Binding.SrcOffset != -1);
		ensure(Binding.DestOffset != -1);
		FNiagaraVariable Param;
		bool bFound = false;
		for (const FNiagaraVariableWithOffset& ParamWithOffset : DestStore->ReadParameterVariables())
		{
			if (ParamWithOffset.Offset == Binding.DestOffset && ParamWithOffset.IsUObject())
			{
				bFound = true;
				Param = ParamWithOffset;
			}
		}
		if (ensure(bFound))
		{
			UE_LOG(LogNiagara, Log, TEXT("| UObject | %s %s: Src:%d - Dest:%d\n"), *Param.GetType().GetName(), *Param.GetName().ToString(), Binding.SrcOffset, Binding.DestOffset);

			//Also ensure the param has been pushed correctly.
			const UObject* SrcData = SrcStore->GetUObjects()[Binding.SrcOffset];
			const UObject* DestData = DestStore->GetUObjects()[Binding.DestOffset];
			if (!ensure(SrcData == DestData))
			{
				UE_LOG(LogNiagara, Log, TEXT("Data interface parameter in dest store is incorrect!\n"));
			}
		}
		else
		{
			UE_LOG(LogNiagara, Log, TEXT("Failed to find matching data interface param in bound store!\n"));
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////

#define NIAGARA_VALIDATE_DIRECT_BINDINGS	DO_CHECK

/**
Direct binding to a parameter store to allow efficient gets/sets from code etc. 
Does no tracking of lifetimes etc so users are responsible for safety.
*/
template<typename T>
struct FNiagaraParameterDirectBinding
{
	static_assert(!TIsUECoreVariant<T, double>::Value, "Double core variant. Must be float type!");

	mutable T* ValuePtr;
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
	FNiagaraParameterStore* BoundStore;
	FNiagaraVariable BoundVariable;
	uint32 LayoutVersion;
#endif

	FNiagaraParameterDirectBinding()
		: ValuePtr(nullptr)
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		, BoundStore(nullptr)
#endif
	{}

	T* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		BoundStore = &InStore;
		BoundVariable = DestVariable;
		LayoutVersion = BoundStore->GetLayoutVersion();
#endif
		check(DestVariable.GetSizeInBytes() == sizeof(T));
		ValuePtr = (T*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const T& InValue)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(T));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			*ValuePtr = InValue;
		}
	}

	FORCEINLINE T GetValue()const 
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(T));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			return *ValuePtr;
		}
		return T();
	}
};

template<>
struct FNiagaraParameterDirectBinding<FMatrix44f>
{
	mutable FMatrix44f* ValuePtr;
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
	FNiagaraParameterStore* BoundStore;
	FNiagaraVariable BoundVariable;
	uint32 LayoutVersion;
#endif

	FNiagaraParameterDirectBinding()
		: ValuePtr(nullptr)
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		, BoundStore(nullptr)
#endif
	{}

	FMatrix44f* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		BoundStore = &InStore;
		BoundVariable = DestVariable;
		LayoutVersion = BoundStore->GetLayoutVersion();
#endif
		check(DestVariable.GetSizeInBytes() == sizeof(FMatrix44f));
		ValuePtr = (FMatrix44f*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const FMatrix44f& InValue)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FMatrix44f));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			FMemory::Memcpy(ValuePtr, &InValue, sizeof(FMatrix44f));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
	}

	FORCEINLINE FMatrix44f GetValue()const
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FMatrix44f));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		FMatrix44f Ret;
		if (ValuePtr)
		{
			FMemory::Memcpy(&Ret, ValuePtr, sizeof(FMatrix44f));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
		return Ret;
	}
};

template<>
struct FNiagaraParameterDirectBinding<FVector4f>
{
	mutable FVector4f* ValuePtr;
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
	FNiagaraParameterStore* BoundStore;
	FNiagaraVariable BoundVariable;
	uint32 LayoutVersion;
#endif

	FNiagaraParameterDirectBinding()
		: ValuePtr(nullptr)
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		, BoundStore(nullptr)
#endif
	{}

	FVector4f* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		BoundStore = &InStore;
		BoundVariable = DestVariable;
		LayoutVersion = BoundStore->GetLayoutVersion();
#endif
		check(DestVariable.GetSizeInBytes() == sizeof(FVector4f));
		ValuePtr = (FVector4f*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const FVector4f& InValue)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FVector4f));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			FMemory::Memcpy(ValuePtr, &InValue, sizeof(FVector4f));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
	}

	FORCEINLINE FVector4f GetValue()const
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		check(BoundVariable.GetSizeInBytes() == sizeof(FVector4f));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		FVector4f Ret;
		if (ValuePtr)
		{
			FMemory::Memcpy(&Ret, ValuePtr, sizeof(FVector4f));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
		return Ret;
	}
};

template<>
struct FNiagaraParameterDirectBinding<FQuat4f>
{
	mutable FQuat4f* ValuePtr;
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
	FNiagaraParameterStore* BoundStore;
	FNiagaraVariable BoundVariable;
	uint32 LayoutVersion;
#endif

	FNiagaraParameterDirectBinding()
		: ValuePtr(nullptr)
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		, BoundStore(nullptr)
#endif
	{}

	FQuat4f* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		BoundStore = &InStore;
		BoundVariable = DestVariable;
		LayoutVersion = BoundStore->GetLayoutVersion();
#endif
		check(DestVariable.GetSizeInBytes() == sizeof(FQuat4f));
		ValuePtr = (FQuat4f*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const FQuat4f& InValue)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FQuat4f));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			FMemory::Memcpy(ValuePtr, &InValue, sizeof(FQuat4f));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
	}

	FORCEINLINE FQuat4f GetValue()const
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FQuat4f));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		FQuat4f Ret;
		if (ValuePtr)
		{
			FMemory::Memcpy(&Ret, ValuePtr, sizeof(FQuat4f));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
		return Ret;
	}
};

template<>
struct FNiagaraParameterDirectBinding<FNiagaraBool>
{
	mutable uint32* ValuePtr;
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
	FNiagaraParameterStore* BoundStore;
	FNiagaraVariable BoundVariable;
	uint32 LayoutVersion;
#endif

	FNiagaraParameterDirectBinding()
		: ValuePtr(nullptr)
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		, BoundStore(nullptr)
#endif
	{}

	uint32* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		BoundStore = &InStore;
		BoundVariable = DestVariable;
		LayoutVersion = BoundStore->GetLayoutVersion();
#endif
		check(DestVariable.GetSizeInBytes() == sizeof(FNiagaraBool));
		check(sizeof(uint32) == sizeof(FNiagaraBool));
		ValuePtr = (uint32*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const FNiagaraBool& InValue)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FNiagaraBool));
		checkSlow(sizeof(uint32) == sizeof(FNiagaraBool));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			FMemory::Memcpy(ValuePtr, &InValue, sizeof(FNiagaraBool));
		}
	}

	FORCEINLINE void SetValue(const bool& InValue)
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FNiagaraBool));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		if (ValuePtr)
		{
			if (!InValue)
			{
				*ValuePtr = FNiagaraBool::False;
			}
			else
			{
				*ValuePtr = FNiagaraBool::True;
			}
		}
	}

	FORCEINLINE FNiagaraBool GetValue()const
	{
#if NIAGARA_VALIDATE_DIRECT_BINDINGS
		checkSlow(BoundVariable.GetSizeInBytes() == sizeof(FNiagaraBool));
		checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));
#endif
		FNiagaraBool Ret(false);
		if (ValuePtr)
		{
			if (FNiagaraBool::False == *ValuePtr)
			{
				Ret = FNiagaraBool(false);
			}
			else
			{
				Ret = FNiagaraBool(true);
			}
		}
		return Ret;
	}
};

template<>
struct FNiagaraParameterDirectBinding<UObject*>
{
	int32 UObjectOffset;
	FNiagaraParameterStore* BoundStore;
	FNiagaraVariable BoundVariable;
	uint32 LayoutVersion;

	FNiagaraParameterDirectBinding()
		: UObjectOffset(INDEX_NONE), BoundStore(nullptr)
	{}

	UObject* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
		if (DestVariable.IsValid())
		{
			BoundStore = &InStore;
			BoundVariable = DestVariable;
			LayoutVersion = BoundStore->GetLayoutVersion();

			check(BoundVariable.GetType().IsUObject());
			UObjectOffset = BoundStore->IndexOf(DestVariable);
			UObject* Ret = BoundStore->GetUObject(UObjectOffset);
			return Ret;
		}
		return nullptr;
	}

	FORCEINLINE void SetValue(UObject* InValue)
	{
		if (UObjectOffset != INDEX_NONE)
		{
			checkSlow(BoundVariable.GetType().IsUObject());
			checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));

			BoundStore->SetUObject(InValue, UObjectOffset);
		}
	}

	FORCEINLINE UObject* GetValue() const
	{
		if (UObjectOffset != INDEX_NONE)
		{
			checkSlow(BoundVariable.GetType().IsUObject());
			checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));

			return BoundStore->GetUObject(UObjectOffset);
		}
		return nullptr;
	}

	template<class TObjectType>
	FORCEINLINE TObjectType* GetValueOrDefault(TObjectType* DefaultValue) const
	{
		if (UObjectOffset != INDEX_NONE)
		{
			checkSlow(BoundVariable.GetType().IsUObject());
			checkfSlow(LayoutVersion == BoundStore->GetLayoutVersion(), TEXT("This binding is invalid, its bound parameter store's layout was changed since it was created"));

			return Cast<TObjectType>(BoundStore->GetUObject(UObjectOffset));
		}
		return DefaultValue;
	}

	template<class TObjectType>
	FORCEINLINE TObjectType* GetValue() const
	{
		return GetValueOrDefault<TObjectType>(nullptr);
	}
};
