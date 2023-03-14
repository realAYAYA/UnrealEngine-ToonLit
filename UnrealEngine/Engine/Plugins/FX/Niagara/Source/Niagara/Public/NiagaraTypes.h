// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/UserDefinedStruct.h"
#include "Misc/SecureHash.h"
#include "UObject/GCObject.h"
#include "UObject/UnrealType.h"
#include "Engine/Texture2D.h"

#include "NiagaraTypes.generated.h"

struct FNiagaraTypeDefinition;
class UNiagaraDataInterfaceBase;

DECLARE_LOG_CATEGORY_EXTERN(LogNiagara, Log, Verbose);

// basic type struct definitions

USTRUCT(meta = (DisplayName = "Wildcard"))
struct FNiagaraWildcard
{
	GENERATED_BODY()
	
};

USTRUCT(meta = (DisplayName = "float"))
struct FNiagaraFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Parameters)
	float Value = 0;
};

USTRUCT(meta = (DisplayName = "int32"))
struct FNiagaraInt32
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	int32 Value = 0;
};

USTRUCT(meta=(DisplayName="bool"))
struct FNiagaraBool
{
	GENERATED_USTRUCT_BODY()

	// The Niagara VM expects this bitmask for its compare and select operators for false.
	enum BoolValues { 
		True = INDEX_NONE,
		False = 0
	}; 

	void SetValue(bool bValue) { Value = bValue ? True : False; }
	bool GetValue() const { return Value != False; }

	/** Sets this niagara bool's raw integer value directly using the special raw integer values expected by the VM and HLSL. */
	FORCEINLINE void SetRawValue(int32 RawValue) { Value = RawValue; }

	/** Gets this niagara bools raw integer value expected by the VM and HLSL. */
	FORCEINLINE int32 GetRawValue() const { return Value; }

	bool IsValid() const { return Value == True || Value == False; }
	
	FNiagaraBool() : Value(False) {}
	FNiagaraBool(bool bInValue) : Value(bInValue ? True : False) {}
	FORCEINLINE operator bool() const { return GetValue(); }

private:
	UPROPERTY(EditAnywhere, Category = Parameters)// Must be either FNiagaraBool::True or FNiagaraBool::False.
	int32 Value = False;
};

USTRUCT(meta = (DisplayName = "Position"))
struct FNiagaraPosition : public FVector3f
{
	GENERATED_USTRUCT_BODY()

	FNiagaraPosition() {}
	
	explicit FORCEINLINE FNiagaraPosition(EForceInit Init) : Super(Init) {}

	FORCEINLINE FNiagaraPosition(const float& X, const float& Y, const float& Z) : Super(X, Y, Z) {}

	FORCEINLINE FNiagaraPosition(const FVector3f& Other) : Super(Other) {}
	FORCEINLINE FNiagaraPosition(const FVector& Other) : Super(Other) {}
};


USTRUCT(meta = (DisplayName = "Half", NiagaraInternalType = "true"))
struct FNiagaraHalf
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 Value = 0;
};

USTRUCT(meta = (DisplayName = "Half Vector2", NiagaraInternalType = "true"))
struct FNiagaraHalfVector2
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 x = 0;

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 y = 0;
};

USTRUCT(meta = (DisplayName = "Half Vector3", NiagaraInternalType = "true"))
struct FNiagaraHalfVector3
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 x = 0;

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 y = 0;

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 z = 0;
};

USTRUCT(meta = (DisplayName = "Half Vector4", NiagaraInternalType = "true"))
struct FNiagaraHalfVector4
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 x = 0;

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 y = 0;

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 z = 0;

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint16 w = 0;
};

USTRUCT()
struct FNiagaraNumeric
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct FNiagaraParameterMap
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT(meta = (DisplayName = "Matrix"))
struct FNiagaraMatrix
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=NiagaraMatrix)
	FVector4f Row0 = FVector4f(ForceInitToZero);

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4f Row1 = FVector4f(ForceInitToZero);

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4f Row2 = FVector4f(ForceInitToZero);

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4f Row3 = FVector4f(ForceInitToZero);
};

USTRUCT()
struct NIAGARA_API FNiagaraAssetVersion
{
	GENERATED_USTRUCT_BODY()

	/** The major version is used to track breaking changes between asset versions */
	UPROPERTY(VisibleAnywhere, Category = "Version Control")
	int32 MajorVersion = 1;

	/** The minor version is used to track non-breaking changes between asset versions */
	UPROPERTY(VisibleAnywhere, Category = "Version Control")
	int32 MinorVersion = 0;

	/** The guid is used to keep track of specific asset version references. The minor and major versions do not provide enough uniqueness to guard against collisions when e.g. the same version was created in different branches. */
	UPROPERTY(VisibleAnywhere, Category = "Version Control", meta=(IgnoreForMemberInitializationTest))
	FGuid VersionGuid = FGuid::NewGuid();
	
	/** If false then this version is not visible in the version selector dropdown menu of the stack. */
	UPROPERTY()
	bool bIsVisibleInVersionSelector = true;

	bool operator==(const FNiagaraAssetVersion& Other) const { return VersionGuid == Other.VersionGuid; }
	bool operator!=(const FNiagaraAssetVersion& Other) const { return !(*this == Other); }
	bool operator<(const FNiagaraAssetVersion& Other) const { return MajorVersion < Other.MajorVersion || (MajorVersion == Other.MajorVersion && MinorVersion < Other.MinorVersion); }
	bool operator<=(const FNiagaraAssetVersion& Other) const { return *this < Other || (MajorVersion == Other.MajorVersion && MinorVersion == Other.MinorVersion); }

	static FGuid CreateStableVersionGuid(UObject* Object);
};

struct NIAGARA_API FNiagaraLWCConverter
{
	FNiagaraLWCConverter(FVector InSystemWorldPos = FVector::ZeroVector);

	FVector3f ConvertWorldToSimulationVector(FVector WorldPosition) const;
	FNiagaraPosition ConvertWorldToSimulationPosition(FVector WorldPosition) const;
	
	FVector ConvertSimulationPositionToWorld(FNiagaraPosition SimulationPosition) const;
	FVector ConvertSimulationVectorToWorld(FVector3f SimulationPosition) const;

private:
	FVector SystemWorldPos;
};

UENUM()
enum class ENiagaraStructConversionType : uint8
{
	CopyOnly, // no conversion, just copy the data
	
	DoubleToFloat,
	Vector2,
	Vector3,
	Vector4,
	Quat,
};

USTRUCT()
struct FNiagaraStructConversionStep
{
	GENERATED_USTRUCT_BODY();
	
	UPROPERTY()
	int32 SourceBytes = 0;

	UPROPERTY()
	int32 SourceOffset = 0;
	
	UPROPERTY()
	int32 SimulationBytes = 0;

	UPROPERTY()
	int32 SimulationOffset = 0;
	
	UPROPERTY()
	ENiagaraStructConversionType ConversionType = ENiagaraStructConversionType::CopyOnly;

	FNiagaraStructConversionStep();
	FNiagaraStructConversionStep(int32 InSourceBytes, int32 InSourceOffset, int32 InSimulationBytes, int32 InSimulationOffset, ENiagaraStructConversionType InConversionType);

	void CopyToSim(uint8* DestinationData, const uint8* SourceData) const;
	void CopyFromSim(uint8* DestinationData, const uint8* SourceData) const;
};

/** Can convert struct data from custom structs containing LWC data such as FVector3d into struct data suitable for Niagara simulations and vice versa. */
USTRUCT()
struct NIAGARA_API FNiagaraLwcStructConverter
{
	GENERATED_USTRUCT_BODY();
	
	void ConvertDataToSimulation(uint8* DestinationData, const uint8* SourceData) const;
	void ConvertDataFromSimulation(uint8* DestinationData, const uint8* SourceData) const;
	
	void AddConversionStep(int32 InSourceBytes, int32 InSourceOffset, int32 InSimulationBytes, int32 InSimulationOffset, ENiagaraStructConversionType ConversionType);
	bool IsValid() const { return ConversionSteps.Num() > 0; }
	
private:
	UPROPERTY()
	TArray<FNiagaraStructConversionStep> ConversionSteps;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraAssetVersion& Version)
{
	return HashCombine(GetTypeHash(Version.MajorVersion), GetTypeHash(Version.MinorVersion));
}

/** Data controlling the spawning of particles */
USTRUCT(BlueprintType, meta = (DisplayName = "Spawn Info", NiagaraClearEachFrame = "true"))
struct FNiagaraSpawnInfo
{
	GENERATED_USTRUCT_BODY();
	
	/** How many particles to spawn. */
	UPROPERTY(BlueprintReadWrite, Category = SpawnInfo)
	int32 Count = 0;
	/** The sub frame delta time at which to spawn the first particle. */
	UPROPERTY(BlueprintReadWrite, Category = SpawnInfo)
	float InterpStartDt = 0.0f;
	/** The sub frame delta time between each particle. */
	UPROPERTY(BlueprintReadWrite, Category = SpawnInfo)
	float IntervalDt = 1.0f;
	/**
	 * An integer used to identify this spawn info.
	 * Typically this is unused.
	 * An example usage is when using multiple spawn modules to spawn from multiple discreet locations.
	 */
	UPROPERTY(BlueprintReadWrite, Category = SpawnInfo)
	int32 SpawnGroup = 0;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Niagara ID"))
struct FNiagaraID
{
	GENERATED_USTRUCT_BODY()

	/** 
	Index in the indirection table for this particle. Allows fast access to this particles data.
	Is always unique among currently living particles but will be reused after the particle dies.
	*/
	UPROPERTY(BlueprintReadWrite, Category = ID)
	int32 Index = 0;

	/** 
	A unique tag for when this ID was acquired. 
	Allows us to differentiate between particles when one dies and another reuses it's Index.
	*/
	UPROPERTY(BlueprintReadWrite, Category = ID)
	int32 AcquireTag = 0;

	FNiagaraID() : Index(INDEX_NONE), AcquireTag(INDEX_NONE) {}
	FNiagaraID(int32 InIndex, int32 InAcquireTag): Index(InIndex), AcquireTag(InAcquireTag){}

	bool operator==(const FNiagaraID& Other)const { return Index == Other.Index && AcquireTag == Other.AcquireTag; }
	bool operator!=(const FNiagaraID& Other)const { return !(*this == Other); }
	bool operator<(const FNiagaraID& Other)const { return Index < Other.Index || (Index == Other.Index && AcquireTag < Other.AcquireTag); }
};

USTRUCT()
struct FNiagaraRandInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Random")
	int32 Seed1 = 0;
	
	UPROPERTY(EditAnywhere, Category = "Random")
	int32 Seed2 = 0;

	UPROPERTY(EditAnywhere, Category = "Random")
	int32 Seed3 = 0;
};

#define NIAGARA_INVALID_ID (FNiagaraID({(INDEX_NONE), (INDEX_NONE)}))

FORCEINLINE uint32 GetTypeHash(const FNiagaraID& ID)
{
	return HashCombine(GetTypeHash(ID.Index), GetTypeHash(ID.AcquireTag));
}

enum class ENiagaraStructConversion : uint8
{
	/** Do not modify struct data members, even if they are not compatible with the Niagara VM, as the struct is user facing (or from an external api). */
	UserFacing,
	
	/** Convert struct members that are not compatible with the simulation (e.g. lwc types) into compatible types and return a simulation-friendly struct for the VM. */
	Simulation,
};

/*
*  Can convert a UStruct with fields of base types only (float, int... - will likely add native vector types here as well)
*	to an FNiagaraTypeDefinition (internal representation)
*/
class NIAGARA_API FNiagaraTypeHelper
{
public:
	static FString ToString(const uint8* ValueData, const UObject* StructOrEnum);
	static bool IsLWCStructure(UStruct* InStruct);
	static bool IsLWCType(const FNiagaraTypeDefinition& InType);
	static UScriptStruct* FindNiagaraFriendlyTopLevelStruct(UScriptStruct* InStruct, ENiagaraStructConversion StructConversion);
	static bool IsNiagaraFriendlyTopLevelStruct(UScriptStruct* InStruct, ENiagaraStructConversion StructConversion);
	static UScriptStruct* GetSWCStruct(UScriptStruct* LWCStruct);
	static UScriptStruct* GetLWCStruct(UScriptStruct* LWCStruct);
	static void TickTypeRemap();

private:
	struct FRemapEntry
	{
		UScriptStruct* Get(UScriptStruct* InStruct) const
		{
#if WITH_EDITORONLY_DATA
			return SerialNumber == InStruct->FieldPathSerialNumber ? Struct.Get() : nullptr;
#else
			return Struct.Get();
#endif
		}

		TWeakObjectPtr<UScriptStruct>	Struct;
#if WITH_EDITORONLY_DATA
		int32 SerialNumber = 0;
#endif
	};

	static FRWLock RemapTableLock;
	static TMap<TWeakObjectPtr<UScriptStruct>, FRemapEntry> RemapTable;
	static std::atomic<bool> RemapTableDirty;
};

/** Information about how this type should be laid out in an FNiagaraDataSet */
USTRUCT()
struct FNiagaraTypeLayoutInfo
{
	GENERATED_BODY()

	FNiagaraTypeLayoutInfo()
	{}

	/** Byte offset of each float component in a structured layout. */
	UPROPERTY()
	TArray<uint32> FloatComponentByteOffsets;

	/** Offset into register table for each float component. */
	UPROPERTY()
	TArray<uint32> FloatComponentRegisterOffsets;

	/** Byte offset of each int32 component in a structured layout. */
	UPROPERTY()
	TArray<uint32> Int32ComponentByteOffsets;

	/** Offset into register table for each int32 component. */
	UPROPERTY()
	TArray<uint32> Int32ComponentRegisterOffsets;

	/** Byte offset of each half component in a structured layout. */
	UPROPERTY()
	TArray<uint32> HalfComponentByteOffsets;

	/** Offset into register table for each half component. */
	UPROPERTY()
	TArray<uint32> HalfComponentRegisterOffsets;

	static void GenerateLayoutInfo(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct)
	{
		Layout.FloatComponentByteOffsets.Empty();
		Layout.FloatComponentRegisterOffsets.Empty();
		Layout.Int32ComponentByteOffsets.Empty();
		Layout.Int32ComponentRegisterOffsets.Empty();
		Layout.HalfComponentByteOffsets.Empty();
		Layout.HalfComponentRegisterOffsets.Empty();
		GenerateLayoutInfoInternal(Layout, Struct);
	}

private:
	static void GenerateLayoutInfoInternal(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct, int32 BaseOffest = 0)
	{
		for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			int32 PropOffset = BaseOffest + Property->GetOffset_ForInternal();
			if (Property->IsA(FFloatProperty::StaticClass()))
			{
				Layout.FloatComponentRegisterOffsets.Add(Layout.FloatComponentByteOffsets.Num());
				Layout.FloatComponentByteOffsets.Add(PropOffset);
			}
			else if (Property->IsA(FUInt16Property::StaticClass()))
			{
				Layout.HalfComponentRegisterOffsets.Add(Layout.HalfComponentByteOffsets.Num());
				Layout.HalfComponentByteOffsets.Add(PropOffset);
			}
			else if (Property->IsA(FIntProperty::StaticClass()) || Property->IsA(FBoolProperty::StaticClass()))
			{
				Layout.Int32ComponentRegisterOffsets.Add(Layout.Int32ComponentByteOffsets.Num());
				Layout.Int32ComponentByteOffsets.Add(PropOffset);
			}
			//Should be able to support double easily enough
			else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				GenerateLayoutInfoInternal(Layout, FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(StructProp->Struct, ENiagaraStructConversion::Simulation), PropOffset);
			}
			else
			{
				checkf(false, TEXT("Property(%s) Class(%s) is not a supported type"), *Property->GetName(), *Property->GetClass()->GetName());
			}
		}
	}
};


/** Defines different modes for selecting the output numeric type of a function or operation based on the types of the inputs. */
UENUM()
enum class ENiagaraNumericOutputTypeSelectionMode : uint8
{
	/** Output type selection not supported. */
	None UMETA(Hidden),
	/** Select the largest of the numeric inputs. */
	Largest,
	/** Select the smallest of the numeric inputs. */
	Smallest,
	/** Selects the base scalar type for this numeric inputs. */
	Scalar,
	/** Selects the type based on custom logic from the node. */
	Custom,
};

/** 
The source from which a script execution state was set. Used to allow scalability etc to change the state but only if the state has not been defined by something with higher precedence. 
If this changes, all scripts must be recompiled by bumping the NiagaraCustomVersion
*/
UENUM()
enum class ENiagaraExecutionStateSource : uint32
{
	Scalability, //State set by Scalability logic. Lowest precedence.
	Internal, //Misc internal state. For example becoming inactive after we finish our set loops.
	Owner, //State requested by the owner. Takes precedence over everything but internal completion logic.
	InternalCompletion, // Internal completion logic. Has to take highest precedence for completion to be ensured.
};

UENUM()
enum class ENiagaraExecutionState : uint32
{
	/**  Run all scripts. Allow spawning.*/
	Active,
	/** Run all scripts but suppress any new spawning.*/
	Inactive,
	/** Clear all existing particles and move to inactive.*/
	InactiveClear,
	/** Complete. When the system or all emitters are complete the effect is considered finished. */
	Complete,
	/** Emitter only. Emitter is disabled. Will not tick or render again until a full re initialization of the system. */
	Disabled UMETA(Hidden),

	// insert new states before
	Num UMETA(Hidden)
};

UENUM()
enum class ENiagaraCoordinateSpace : uint32
{
	/** Use the coordinate space specified by the Emitter*/
	Simulation,
	/** Use the world coordinate space*/
	World,
	/** Use the local coordinate space*/
	Local,

	// insert new states before
	//NewEnumerator0 = 0 UMETA(Hidden),
	//NewEnumerator1 = 1 UMETA(Hidden),
	//NewEnumerator2 = 2 UMETA(Hidden)
};

UENUM()
enum class ENiagaraPythonUpdateScriptReference : uint8
{
	None,
    ScriptAsset,
    DirectTextEntry
};

UENUM()
enum class ENiagaraOrientationAxis : uint32
{
	XAxis UMETA(DisplayName="X Axis"),
	YAxis UMETA(DisplayName = "Y Axis"),
	ZAxis UMETA(DisplayName = "Z Axis"),
};


USTRUCT()
struct NIAGARA_API FNiagaraCompileHashVisitorDebugInfo
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY()
	FString Object;

	UPROPERTY()
	TArray<FString> PropertyKeys;

	UPROPERTY()
	TArray<FString> PropertyValues;
};

/**
Used to store the state of a graph when deciding if it has been dirtied for recompile.
*/
struct NIAGARA_API FNiagaraCompileHashVisitor
{
public:
	FNiagaraCompileHashVisitor(FSHA1& InHashState) : HashState(InHashState) {}

	FSHA1& HashState;
	TArray<const void*> ObjectList;

	static int LogCompileIdGeneration;

#if WITH_EDITORONLY_DATA
	FNiagaraCompileHashVisitorDebugInfo* AddDebugInfo()
	{
		if (LogCompileIdGeneration != 0)
		{
			return &Values.AddDefaulted_GetRef();
		}
		return nullptr;
	}

	// Debug data about the compilation hash, including key value pairs to detect differences.
	TArray<FNiagaraCompileHashVisitorDebugInfo> Values;

	template<typename T>
	void ToDebugString(const T* InData, uint64 InDataCount, FString& OutStr)
	{
		for (int32 i = 0; i < InDataCount; i++)
		{
			FString DataStr = LexToString(InData[i]);
			OutStr.Appendf(TEXT("%s "), *DataStr);
		}
	}
#endif
	/**
	Registers a pointer for later reference in the compile id in a deterministic manner.
	*/
	int32 RegisterReference(const void* InObject)
	{
		if (InObject == nullptr)
		{
			return -1;
		}

		int32 Index = ObjectList.Find(InObject);
		if (Index < 0)
		{
			Index = ObjectList.Add(InObject);
		}
		return Index;
	}

	/**
	We don't usually want to save GUID's or pointer values because they have nondeterministic values. Consider a PostLoad upgrade operation that creates a new node.
	Each pin and node gets a unique ID. If you close the editor and reopen, you'll get a different set of values. One of the characteristics we want for compilation
	behavior is that the same graph structure produces the same compile results, so we only want to embed information that is deterministic. This method is for use
	when registering a pointer to an object that is serialized within the compile hash.
	*/
	bool UpdateReference(const TCHAR* InDebugName, const void* InObject)
	{
		int32 Index = RegisterReference(InObject);
		return UpdatePOD(InDebugName, Index);
	}

	/**
	Adds an array of POD (plain old data) values to the hash.
	*/
	template<typename T>
	bool UpdateArray(const TCHAR* InDebugName, const T* InData, uint64 InDataCount = 1)
	{
		static_assert(TIsPODType<T>::Value, "UpdateArray does not support a constructor / destructor on the element class.");
		HashState.Update((const uint8 *)InData, sizeof(T)*InDataCount);
#if WITH_EDITORONLY_DATA
		if (LogCompileIdGeneration != 0)
		{
			FString ValuesStr = InDebugName;
			ValuesStr.Append(TEXT(" = "));
			ToDebugString(InData, InDataCount, ValuesStr);
			Values.Top().PropertyKeys.Push(InDebugName);
			Values.Top().PropertyValues.Push(ValuesStr);
		}
#endif
		return true;
	}

	/**
	 * Adds the string representation of an FName to the hash
	 */
	bool UpdateName(const TCHAR* InDebugName, FName InName)
	{
		FNameBuilder NameString(InName);
		return UpdateString(InDebugName, NameString.ToView());
	}

	/**
	Adds a single value of typed POD (plain old data) to the hash.
	*/
	template<typename T>
	bool UpdatePOD(const TCHAR* InDebugName, const T& InData)
	{
		static_assert(TIsPODType<T>::Value, "Update does not support a constructor / destructor on the element class.");
		static_assert(!TIsPointer<T>::Value, "Update does not support pointer types.");
		HashState.Update((const uint8 *)&InData, sizeof(T));
#if WITH_EDITORONLY_DATA
		if (LogCompileIdGeneration != 0)
		{
			FString ValuesStr;
			ToDebugString(&InData, 1, ValuesStr);
			Values.Top().PropertyKeys.Push(InDebugName);
			Values.Top().PropertyValues.Push(ValuesStr);
		}
#endif
		return true;
	}

	template<typename T>
	bool UpdateShaderParameters()
	{
		const FShaderParametersMetadata* ShaderParametersMetadata = TShaderParameterStructTypeInfo<T>::GetStructMetadata();
		return UpdatePOD(ShaderParametersMetadata->GetStructTypeName(), ShaderParametersMetadata->GetLayoutHash());
	}

	/**
	Adds an string value to the hash.
	*/
	bool UpdateString(const TCHAR* InDebugName, FStringView InData)
	{
		HashState.Update((const uint8 *)InData.GetData(), sizeof(TCHAR)*InData.Len());
#if WITH_EDITORONLY_DATA
		if (LogCompileIdGeneration != 0)
		{
			Values.Top().PropertyKeys.Push(InDebugName);
			Values.Top().PropertyValues.Push(FString(InData));
		}
#endif
		return true;
	}
};

/** Defines options for conditionally editing and showing script inputs in the UI. */
USTRUCT()
struct NIAGARA_API FNiagaraInputConditionMetadata
{
	GENERATED_USTRUCT_BODY()
public:
	/** The name of the input to use for matching the target values. */
	UPROPERTY(EditAnywhere, Category="Input Condition")
	FName InputName;

	/** The list of target values which will satisfy the input condition.  If this is empty it's assumed to be a single value of "true" for matching bool inputs. */
	UPROPERTY(EditAnywhere, Category="Input Condition")
	TArray<FString> TargetValues;
};

/** Defines override data for enum parameters displayed in the UI. */
USTRUCT()
struct NIAGARA_API FNiagaraEnumParameterMetaData
{
	GENERATED_BODY()
	
	/** If specified, this name will be used for the given enum entry. Useful for shortening names. */
	UPROPERTY(EditAnywhere, Category="Enum Override")
	FName OverrideName;

	/** If specified, this icon will be used for the given enum entry. If OverrideName isn't empty, the icon takes priority. */
	UPROPERTY(EditAnywhere, Category="Enum Override")
	TObjectPtr<UTexture2D> IconOverride = nullptr;

	UPROPERTY(EditAnywhere, Category="Enum Override", meta=(InlineEditConditionToggle))
	bool bUseColorOverride = false;
	
	UPROPERTY(EditAnywhere, Category="Enum Override", meta=(EditCondition="bUseColorOverride"))
	FLinearColor ColorOverride = FLinearColor::White;
};

UENUM()
enum class ENiagaraBoolDisplayMode : uint8
{
	DisplayAlways,
	DisplayIfTrue,
	DisplayIfFalse
};

USTRUCT()
struct NIAGARA_API FNiagaraBoolParameterMetaData
{
	GENERATED_BODY()

	/** The mode used determines the cases in which a bool parameter is displayed.
	 *  If set to DisplayAlways, both True and False cases will display. 
	 *  If set to DisplayIfTrue, it will only display if the bool evaluates to True.
	 */
	UPROPERTY(EditAnywhere, Category="Bool Override")
	ENiagaraBoolDisplayMode DisplayMode = ENiagaraBoolDisplayMode::DisplayAlways;
	
	/** If specified, this name will be used for the given bool if it evaluates to True. */
	UPROPERTY(EditAnywhere, Category="Bool Override")
	FName OverrideNameTrue;

	/** If specified, this name will be used for the given bool if it evaluates to False. */
	UPROPERTY(EditAnywhere, Category="Bool Override")
	FName OverrideNameFalse;

	/** If specified, this icon will be used for the given bool if it evaluates to True. If OverrideName isn't empty, the icon takes priority. */
	UPROPERTY(EditAnywhere, Category="Bool Override")
	TObjectPtr<UTexture2D> IconOverrideTrue = nullptr;

	/** If specified, this icon will be used for the given bool if it evaluates to False. If OverrideName isn't empty, the icon takes priority. */
	UPROPERTY(EditAnywhere, Category="Bool Override")
	TObjectPtr<UTexture2D> IconOverrideFalse = nullptr;
};

USTRUCT()
struct NIAGARA_API FNiagaraVariableMetaData
{
	GENERATED_USTRUCT_BODY()

	FNiagaraVariableMetaData()
		: bAdvancedDisplay(false)
		, bDisplayInOverviewStack(false)
		, InlineParameterSortPriority(0)
		, bOverrideColor(false)
		, InlineParameterColorOverride(FLinearColor(ForceInit))
		, bEnableBoolOverride(false)
		, EditorSortPriority(0)
		, bInlineEditConditionToggle(false)
		, bIsStaticSwitch_DEPRECATED(false)
		, StaticSwitchDefaultValue_DEPRECATED(0)
	{};

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (MultiLine = true, SkipForCompileHash = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (SkipForCompileHash = "true"))
	FText CategoryName;

	/** Declares that this input is advanced and should only be visible if expanded inputs have been expanded. */
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (SkipForCompileHash = "true"))
	bool bAdvancedDisplay;

	/** Declares that this parameter's value will be shown in the overview node if it's set to a local value. */
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (SkipForCompileHash = "true"))
	bool bDisplayInOverviewStack;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (EditCondition="bDisplayInOverviewStack", ToolTip = "Affects the sort order for parameters shown inline in the overview. Use a smaller number to push it to the top. Defaults to zero.", SkipForCompileHash = "true"))
	int32 InlineParameterSortPriority;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (InlineEditConditionToggle, ToolTip = "The color used to display a parameter in the overview. If no color is specified, the type color is used.", SkipForCompileHash = "true"))
	bool bOverrideColor;
	
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (EditCondition="bOverrideColor", ToolTip = "The color used to display a parameter in the overview. If no color is specified, the type color is used.", SkipForCompileHash = "true"))
	FLinearColor InlineParameterColorOverride;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (EditCondition="bDisplayInOverviewStack", ToolTip = "The index of the entry maps to the index of an enum value. Useful for overriding how an enum parameter is displayed in the overview.", SkipForCompileHash = "true"))
	TArray<FNiagaraEnumParameterMetaData> InlineParameterEnumOverrides;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (InlineEditConditionToggle, ToolTip = "Useful to override inline bool visualization in the overview.", SkipForCompileHash = "true"))
	bool bEnableBoolOverride;
	
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (EditCondition="bEnableBoolOverride", ToolTip = "Useful to override inline bool visualization in the overview.", SkipForCompileHash = "true"))
	FNiagaraBoolParameterMetaData InlineParameterBoolOverride;
	
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (ToolTip = "Affects the sort order in the editor stacks. Use a smaller number to push it to the top. Defaults to zero.", SkipForCompileHash = "true"))
	int32 EditorSortPriority;

	/** Declares the associated input is used as an inline edit condition toggle, so it should be hidden and edited as a 
	checkbox inline with the input which was designated as its edit condition. */
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (SkipForCompileHash = "true"))
	bool bInlineEditConditionToggle;

	/** Declares the associated input should be conditionally editable based on the value of another input. */
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (SkipForCompileHash = "true"))
	FNiagaraInputConditionMetadata EditCondition;

	/** Declares the associated input should be conditionally visible based on the value of another input. */
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (SkipForCompileHash = "true"))
	FNiagaraInputConditionMetadata VisibleCondition;

	UPROPERTY(EditAnywhere, Category = "Variable", DisplayName = "Property Metadata", meta = (ToolTip = "Property Metadata", SkipForCompileHash = "true"))
	TMap<FName, FString> PropertyMetaData;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (ToolTip = "If set, this attribute is visually displayed as a child under the given parent attribute. Currently, only static switches are supported as parent attributes!", SkipForCompileHash = "true"))
	FName ParentAttribute;

	UPROPERTY(EditAnywhere, Category = "Variable", DisplayName = "Alternate Aliases For Variable", AdvancedDisplay, meta = (ToolTip = "List of alternate/previous names for this variable. Note that this is not normally needed if you rename through the UX. However, if you delete and then add a different variable, intending for it to match, you will likely want to add the prior name here.\n\nYou may need to restart and reload assets after making this change to have it take effect on already loaded assets."))
	TArray<FName> AlternateAliases;

	bool GetIsStaticSwitch_DEPRECATED() const { return bIsStaticSwitch_DEPRECATED; };

	int32 GetStaticSwitchDefaultValue_DEPRECATED() const { return StaticSwitchDefaultValue_DEPRECATED; };

	/** Copies all the properties that are marked as editable for the user (e.g. EditAnywhere). */
	void CopyUserEditableMetaData(const FNiagaraVariableMetaData& OtherMetaData);

	FGuid GetVariableGuid() const { return VariableGuid; };

	/** Note, the Variable Guid is generally expected to be immutable. This method is provided to upgrade existing variables to have the same Guid as variable definitions. */
	void SetVariableGuid(const FGuid& InVariableGuid ) { VariableGuid = InVariableGuid; };
	void CreateNewGuid() { VariableGuid = FGuid::NewGuid(); };

private:
	/** A unique identifier for the variable that can be used by function call nodes to find renamed variables. */
	UPROPERTY(meta = (SkipForCompileHash = "true"))
	FGuid VariableGuid;

	/** This is a read-only variable that designates if the metadata is tied to a static switch or not.
	 * DEPRECATED: Migrated to UNiagaraScriptVariable::bIsStaticSwitch.
	 */
	UPROPERTY()
	bool bIsStaticSwitch_DEPRECATED;

	/** The default value to use when creating new pins or stack entries for a static switch parameter
	 * DEPRECATED: Migrated to UNiagaraScriptVariable::StaticSwitchDefaultValue.
	 */
	UPROPERTY()
	int32 StaticSwitchDefaultValue_DEPRECATED;  // TODO: This should be moved to the UNiagaraScriptVariable in the future

};

USTRUCT()
struct NIAGARA_API FNiagaraTypeDefinition
{
	GENERATED_USTRUCT_BODY()

	enum FUnderlyingType
	{
		UT_None = 0,
		UT_Class,
		UT_Struct,
		UT_Enum
	};

	enum FTypeFlags
	{
		TF_None			= 0x0000,
		TF_Static		= 0x0001,

		/// indicates that the ClassStructOrEnum property has been serialized as the LWC struct (see FNiagaraTypeHelper)
		/// instead of the Transient SWC version of the struct
		TF_SerializedAsLWC	= 0x0002,
	};

public:

	// Construct blank raw type definition 
	FORCEINLINE FNiagaraTypeDefinition(UClass *ClassDef)
		: ClassStructOrEnum(ClassDef), UnderlyingType(UT_Class), Flags(TF_None), Size(INDEX_NONE), Alignment(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, Struct_DEPRECATED(nullptr), Enum_DEPRECATED(nullptr)
#endif
	{
		checkSlow(ClassStructOrEnum != nullptr);
	}

	FORCEINLINE FNiagaraTypeDefinition(UEnum *EnumDef)
		: ClassStructOrEnum(EnumDef), UnderlyingType(UT_Enum), Flags(TF_None), Size(INDEX_NONE), Alignment(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, Struct_DEPRECATED(nullptr), Enum_DEPRECATED(nullptr)
#endif
	{
		checkSlow(ClassStructOrEnum != nullptr);
	}

	enum class EAllowUnfriendlyStruct : uint8 { Deny, Allow };
	FORCEINLINE FNiagaraTypeDefinition(UScriptStruct* StructDef, EAllowUnfriendlyStruct AllowUnfriendlyStruct)
		: ClassStructOrEnum(StructDef), UnderlyingType(UT_Struct), Flags(TF_None), Size(INDEX_NONE), Alignment(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, Struct_DEPRECATED(nullptr), Enum_DEPRECATED(nullptr)
#endif
	{
		checkSlow(ClassStructOrEnum != nullptr);
		ensureAlwaysMsgf(AllowUnfriendlyStruct == EAllowUnfriendlyStruct::Allow || FNiagaraTypeHelper::IsNiagaraFriendlyTopLevelStruct(StructDef, ENiagaraStructConversion::UserFacing), TEXT("Struct(%s) is not supported."), *StructDef->GetName());
	}
	FORCEINLINE FNiagaraTypeDefinition(UScriptStruct* StructDef) : FNiagaraTypeDefinition(StructDef, EAllowUnfriendlyStruct::Deny) {}

	FORCEINLINE FNiagaraTypeDefinition(const FNiagaraTypeDefinition &Other)
		: ClassStructOrEnum(Other.ClassStructOrEnum), UnderlyingType(Other.UnderlyingType), Flags(Other.Flags), Size(INDEX_NONE), Alignment(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, Struct_DEPRECATED(nullptr), Enum_DEPRECATED(nullptr)
#endif
	{
	}

	// Construct a blank raw type definition
	FORCEINLINE FNiagaraTypeDefinition()
		: ClassStructOrEnum(nullptr), UnderlyingType(UT_None), Flags(TF_None), Size(INDEX_NONE), Alignment(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, Struct_DEPRECATED(nullptr), Enum_DEPRECATED(nullptr)
#endif
	{}

	FORCEINLINE bool operator !=(const FNiagaraTypeDefinition &Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE bool operator == (const FNiagaraTypeDefinition &Other) const
	{
		return ClassStructOrEnum == Other.ClassStructOrEnum && UnderlyingType == Other.UnderlyingType && Flags == Other.Flags;
	}

	FORCEINLINE bool IsSameBaseDefinition(const FNiagaraTypeDefinition& Other) const
	{
		return ClassStructOrEnum == Other.ClassStructOrEnum && UnderlyingType == Other.UnderlyingType;
	}

	FText GetNameText() const
	{
		if (IsValid() == false)
		{
			return NSLOCTEXT("NiagaraTypeDefinition", "InvalidNameText", "Invalid (null type)");
		}

		FText NameText;
#if WITH_EDITOR
		if ( UnderlyingType != UT_Enum )
		{
			// fix names for common types after LWC changes
			if (*this == GetVec2Def())
			{
				NameText = FText::FromString("Vector 2D");
			}
			else if (*this == GetVec3Def())
			{
				NameText = FText::FromString("Vector");
			}
			else if (*this == GetVec4Def())
			{
				NameText = FText::FromString("Vector 4");
			}
			else if (*this == GetQuatDef())
			{
				NameText = FText::FromString("Quat");
			}
			else
			{
				NameText = GetStruct()->GetDisplayNameText();
			}
		}
		else
#endif
		{
			NameText = FText::FromString(ClassStructOrEnum->GetName());
		}

		if (Flags != 0)
		{
			if (IsStatic())
			{
				NameText = FText::Format(NSLOCTEXT("NiagaraTypeDefinition", "FlagFormatNameText", "Static {0}"), NameText);
			}
			else
			{
				NameText = FText::Format(NSLOCTEXT("NiagaraTypeDefinition", "UnknownNameText", "Unknown Flag Type {0}"), NameText);
			}
		}
		
		return NameText;
		
	}

	FName GetFName() const
	{
		if ( IsValid() == false )
		{
			return FName();
		}
		return ClassStructOrEnum->GetFName();
	}

	FString GetName()const
	{
		if (IsValid() == false)
		{
			return TEXT("Invalid");
		}
		return ClassStructOrEnum->GetName();
	}

	UStruct* GetStruct() const
	{
		return UnderlyingType == UT_Enum ? IntStruct : Cast<UStruct>(ClassStructOrEnum);
	}

	UScriptStruct* GetScriptStruct()const
	{
		return Cast<UScriptStruct>(GetStruct());
	}

	/** Gets the class ptr for this type if it is a class. */
	UClass* GetClass() const
	{
		return UnderlyingType == UT_Class ? CastChecked<UClass>(ClassStructOrEnum) : nullptr;
	}

	UEnum* GetEnum() const
	{
		return UnderlyingType == UT_Enum ? CastChecked<UEnum>(ClassStructOrEnum) : nullptr;
	}

	bool IsDataInterface() const;

	FORCEINLINE bool IsUObject() const
	{
		return GetStruct()->IsChildOf<UObject>();
	}

	bool IsEnum() const { return UnderlyingType == UT_Enum; }

	bool IsStatic() const {
		return (GetFlags() & TF_Static) != 0;
	}

	void SetFlags(FTypeFlags InFlags)
	{
		Flags = InFlags;
	}

	uint32 GetFlags()const { return Flags; }
	

	bool IsIndexWildcard() const { return ClassStructOrEnum == FNiagaraTypeDefinition::GetWildcardStruct(); }

	FNiagaraTypeDefinition ToStaticDef() const;
	
	int32 GetSize() const
	{
		if (Size == INDEX_NONE)
		{
			checkfSlow(IsValid(), TEXT("Type definition is not valid."));
			if (ClassStructOrEnum == nullptr || GetClass())
			{
				Size = 0;//TODO: sizeof(void*);//If we're a class then we allocate space for the user to instantiate it. This and stopping it being GCd is up to the user.
			}
			else
			{
				Size = CastChecked<UScriptStruct>(GetStruct())->GetStructureSize();
			}
		}
		return Size;
	}

	int32 GetAlignment() const
	{
		if (Alignment == INDEX_NONE)
		{
			checkfSlow(IsValid(), TEXT("Type definition is not valid."));
			if (ClassStructOrEnum == nullptr || GetClass())
			{
				Alignment = 0;//TODO: sizeof(void*);//If we're a class then we allocate space for the user to instantiate it. This and stopping it being GCd is up to the user.
			}
			else
			{
				Alignment = CastChecked<UScriptStruct>(GetStruct())->GetMinAlignment();
			}
		}
		return Alignment;
	}

	bool IsFloatPrimitive() const
	{
		return ClassStructOrEnum == GetFloatStruct() || ClassStructOrEnum == GetVec2Struct() || ClassStructOrEnum ==
			GetVec3Struct() || ClassStructOrEnum == GetVec4Struct() ||
			ClassStructOrEnum == GetMatrix4Struct() || ClassStructOrEnum == GetColorStruct() || ClassStructOrEnum ==
			GetQuatStruct() || ClassStructOrEnum == GetPositionStruct();
 	}

	bool IsIndexType() const
	{
		return ClassStructOrEnum == GetIntStruct() || ClassStructOrEnum == GetBoolStruct() || IsEnum();
	}
	bool IsValid() const 
	{ 
		return ClassStructOrEnum != nullptr;
	}

	bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const;

#if WITH_EDITORONLY_DATA
	bool IsInternalType() const;
#endif

	/*
	Underlying type for this variable, use FUnderlyingType to determine type without casting
	This can be a UClass, UStruct or UEnum.  Pointing to something like the struct for an FVector, etc.
	In occasional situations this may be a UClass when we're dealing with DataInterface etc.
	*/
	UPROPERTY(EditAnywhere, Category=Type)
	TObjectPtr<UObject> ClassStructOrEnum;

	// See enumeration FUnderlyingType for possible values
	UPROPERTY(EditAnywhere, Category=Type)
	uint16 UnderlyingType;

	bool Serialize(FArchive& Ar);

private:
	UPROPERTY(EditAnywhere, Category = Type)
	uint8 Flags;

	mutable int16 Size;
	mutable int16 Alignment;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UStruct> Struct_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UEnum> Enum_DEPRECATED;
#endif

public:
	static void Init();
#if WITH_EDITOR
	static void RecreateUserDefinedTypeRegistry();
#endif
	static const FNiagaraTypeDefinition& GetFloatDef() { return FloatDef; }
	static const FNiagaraTypeDefinition& GetBoolDef() { return BoolDef; }
	static const FNiagaraTypeDefinition& GetIntDef() { return IntDef; }
	static const FNiagaraTypeDefinition& GetVec2Def() { return Vec2Def; }
	static const FNiagaraTypeDefinition& GetVec3Def() { return Vec3Def; }
	static const FNiagaraTypeDefinition& GetVec4Def() { return Vec4Def; }
	static const FNiagaraTypeDefinition& GetColorDef() { return ColorDef; }
	static const FNiagaraTypeDefinition& GetPositionDef() { return PositionDef; }
	static const FNiagaraTypeDefinition& GetQuatDef() { return QuatDef; }
	static const FNiagaraTypeDefinition& GetMatrix4Def() { return Matrix4Def; }
	static const FNiagaraTypeDefinition& GetGenericNumericDef() { return NumericDef; }
	static const FNiagaraTypeDefinition& GetParameterMapDef() { return ParameterMapDef; }
	static const FNiagaraTypeDefinition& GetIDDef() { return IDDef; }
	static const FNiagaraTypeDefinition& GetRandInfoDef() { return RandInfoDef; }
	static const FNiagaraTypeDefinition& GetUObjectDef() { return UObjectDef; }
	static const FNiagaraTypeDefinition& GetUMaterialDef() { return UMaterialDef; }
	static const FNiagaraTypeDefinition& GetUStaticMeshDef() { return UStaticMeshDef; }
	static const FNiagaraTypeDefinition& GetUTextureDef() { return UTextureDef; }
	static const FNiagaraTypeDefinition& GetUTextureRenderTargetDef() { return UTextureRenderTargetDef; }
	static const FNiagaraTypeDefinition& GetSimCacheClassDef() { return USimCacheClassDef; }
	static const FNiagaraTypeDefinition& GetWildcardDef() { return WildcardDef; }
	static const FNiagaraTypeDefinition& GetHalfDef() { return HalfDef; }
	static const FNiagaraTypeDefinition& GetHalfVec2Def() { return HalfVec2Def; }
	static const FNiagaraTypeDefinition& GetHalfVec3Def() { return HalfVec3Def; }
	static const FNiagaraTypeDefinition& GetHalfVec4Def() { return HalfVec4Def; }

	template<typename T>
	static const FNiagaraTypeDefinition& Get();

	static UScriptStruct* GetFloatStruct() { return FloatStruct; }
	static UScriptStruct* GetBoolStruct() { return BoolStruct; }
	static UScriptStruct* GetIntStruct() { return IntStruct; }
	static UScriptStruct* GetVec2Struct() { return Vec2Struct; }
	static UScriptStruct* GetVec3Struct() { return Vec3Struct; }
	static UScriptStruct* GetVec4Struct() { return Vec4Struct; }
	static UScriptStruct* GetColorStruct() { return ColorStruct; }
	static UScriptStruct* GetPositionStruct() { return PositionStruct; }
	static UScriptStruct* GetQuatStruct() { return QuatStruct; }
	static UScriptStruct* GetMatrix4Struct() { return Matrix4Struct; }
	static UScriptStruct* GetGenericNumericStruct() { return NumericStruct; }
	static UScriptStruct* GetWildcardStruct() { return WildcardStruct; }
	static UScriptStruct* GetParameterMapStruct() { return ParameterMapStruct; }
	static UScriptStruct* GetIDStruct() { return IDStruct; }
	static UScriptStruct* GetRandInfoStruct() { return RandInfoStruct; }

	static UScriptStruct* GetHalfStruct() { return HalfStruct; }
	static UScriptStruct* GetHalfVec2Struct() { return HalfVec2Struct; }
	static UScriptStruct* GetHalfVec3Struct() { return HalfVec3Struct; }
	static UScriptStruct* GetHalfVec4Struct() { return HalfVec4Struct; }

	static UEnum* GetExecutionStateEnum() { return ExecutionStateEnum; }
	static UEnum* GetCoordinateSpaceEnum() { return CoordinateSpaceEnum; }
	static UEnum* GetOrientationAxisEnum() { return OrientationAxisEnum; }
	static UEnum* GetExecutionStateSouceEnum() { return ExecutionStateSourceEnum; }
	static UEnum* GetSimulationTargetEnum() { return SimulationTargetEnum; }
	static UEnum* GetScriptUsageEnum() { return ScriptUsageEnum; }
	static UEnum* GetScriptContextEnum() { return ScriptContextEnum; }
	static UEnum* GetParameterPanelCategoryEnum() { return ParameterPanelCategoryEnum; }
	static UEnum* GetFunctionDebugStateEnum() { return FunctionDebugStateEnum; }

	static UEnum* GetParameterScopeEnum() { return ParameterScopeEnum; }

	static const FNiagaraTypeDefinition& GetCollisionEventDef() { return CollisionEventDef; }

	static bool IsScalarDefinition(const FNiagaraTypeDefinition& Type);

	FString ToString(const uint8* ValueData)const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		if (ValueData == nullptr)
		{
			return TEXT("(null)");
		}
		return FNiagaraTypeHelper::ToString(ValueData, ClassStructOrEnum);
	}
	
	static bool TypesAreAssignable(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB, bool bAllowLossyLWCConversions = false);
	static bool IsLossyConversion(const FNiagaraTypeDefinition& FromType, const FNiagaraTypeDefinition& ToType);
	static FNiagaraTypeDefinition GetNumericOutputType(const TArray<FNiagaraTypeDefinition> TypeDefinintions, ENiagaraNumericOutputTypeSelectionMode SelectionMode);

	static const TArray<FNiagaraTypeDefinition>& GetNumericTypes() { return OrderedNumericTypes; }
	static bool IsValidNumericInput(const FNiagaraTypeDefinition& TypeDef);

	

private:

	static FNiagaraTypeDefinition FloatDef;
	static FNiagaraTypeDefinition BoolDef;
	static FNiagaraTypeDefinition IntDef;
	static FNiagaraTypeDefinition Vec2Def;
	static FNiagaraTypeDefinition Vec3Def;
	static FNiagaraTypeDefinition Vec4Def;
	static FNiagaraTypeDefinition ColorDef;
	static FNiagaraTypeDefinition PositionDef;
	static FNiagaraTypeDefinition QuatDef;
	static FNiagaraTypeDefinition Matrix4Def;
	static FNiagaraTypeDefinition NumericDef;
	static FNiagaraTypeDefinition ParameterMapDef;
	static FNiagaraTypeDefinition IDDef;
	static FNiagaraTypeDefinition RandInfoDef;
	static FNiagaraTypeDefinition UObjectDef;
	static FNiagaraTypeDefinition UMaterialDef;
	static FNiagaraTypeDefinition UTextureDef;
	static FNiagaraTypeDefinition UTextureRenderTargetDef;
	static FNiagaraTypeDefinition UStaticMeshDef;
	static FNiagaraTypeDefinition USimCacheClassDef;
	static FNiagaraTypeDefinition WildcardDef;

	static FNiagaraTypeDefinition HalfDef;
	static FNiagaraTypeDefinition HalfVec2Def;
	static FNiagaraTypeDefinition HalfVec3Def;
	static FNiagaraTypeDefinition HalfVec4Def;

	static UScriptStruct* FloatStruct;
	static UScriptStruct* BoolStruct;
	static UScriptStruct* IntStruct;
	static UScriptStruct* Vec2Struct;
	static UScriptStruct* Vec3Struct;
	static UScriptStruct* Vec4Struct;
	static UScriptStruct* QuatStruct;
	static UScriptStruct* ColorStruct;
	static UScriptStruct* Matrix4Struct;
	static UScriptStruct* NumericStruct;
	static UScriptStruct* WildcardStruct;
	static UScriptStruct* PositionStruct;

	static UScriptStruct* HalfStruct;
	static UScriptStruct* HalfVec2Struct;
	static UScriptStruct* HalfVec3Struct;
	static UScriptStruct* HalfVec4Struct;

	static UClass* UObjectClass;
	static UClass* UMaterialClass;
	static UClass* UTextureClass;
	static UClass* UTextureRenderTargetClass;

	static UEnum* SimulationTargetEnum;
	static UEnum* ScriptUsageEnum;
	static UEnum* ScriptContextEnum;
	static UEnum* ExecutionStateEnum;
	static UEnum* CoordinateSpaceEnum;
	static UEnum* OrientationAxisEnum;
	static UEnum* ExecutionStateSourceEnum;

	static UEnum* ParameterScopeEnum;
	static UEnum* ParameterPanelCategoryEnum;

	static UEnum* FunctionDebugStateEnum;

	static UScriptStruct* ParameterMapStruct;
	static UScriptStruct* IDStruct;
	static UScriptStruct* RandInfoStruct;

	static TSet<UScriptStruct*> NumericStructs;
	static TArray<FNiagaraTypeDefinition> OrderedNumericTypes;

	static TSet<UScriptStruct*> ScalarStructs;

	static TSet<UStruct*> FloatStructs;
	static TSet<UStruct*> IntStructs;
	static TSet<UStruct*> BoolStructs;

	static FNiagaraTypeDefinition CollisionEventDef;
};

template<>
struct TStructOpsTypeTraits<FNiagaraTypeDefinition> : public TStructOpsTypeTraitsBase2<FNiagaraTypeDefinition>
{
	enum
	{
		WithSerializer = true,
	};
};


//Helper to get the correct typedef for templated code.
template<typename T>
const FNiagaraTypeDefinition& FNiagaraTypeDefinition::Get()
{
	if (TIsSame<T, float>::Value) { return GetFloatDef(); }
	if (TIsSame<T, FVector2f>::Value) { return GetVec2Def(); }
	if (TIsSame<T, FVector3f>::Value) { return GetVec3Def(); }	
	if (TIsSame<T, FNiagaraPosition>::Value) { return GetPositionDef(); }	
	if (TIsSame<T, FVector4f>::Value) { return GetVec4Def(); }
	if (TIsSame<T, int32>::Value) { return GetIntDef(); }
	if (TIsSame<T, FNiagaraBool>::Value) { return GetBoolDef(); }
	if (TIsSame<T, FQuat4f>::Value) { return GetQuatDef(); }
	if (TIsSame<T, FMatrix44f>::Value) { return GetMatrix4Def(); }
	if (TIsSame<T, FLinearColor>::Value) { return GetColorDef(); }
	if (TIsSame<T, FNiagaraID>::Value) { return GetIDDef(); }
	if (TIsSame<T, FNiagaraRandInfo>::Value) { return GetRandInfoDef(); }
}

FORCEINLINE uint32 GetTypeHash(const FNiagaraTypeDefinition& Type)
{
	return HashCombine(HashCombine(GetTypeHash(Type.ClassStructOrEnum), GetTypeHash(Type.UnderlyingType)), GetTypeHash(Type.GetFlags()));
}

//////////////////////////////////////////////////////////////////////////

enum class ENiagaraTypeRegistryFlags : uint32
{
	None					= 0,

	AllowUserVariable		= (1 << 0),
	AllowSystemVariable		= (1 << 1),
	AllowEmitterVariable	= (1 << 2),
	AllowParticleVariable	= (1 << 3),
	AllowAnyVariable		= (AllowUserVariable | AllowSystemVariable | AllowEmitterVariable | AllowParticleVariable),

	AllowParameter			= (1 << 4),
	AllowPayload			= (1 << 5),

	IsUserDefined			= (1 << 6),

};

ENUM_CLASS_FLAGS(ENiagaraTypeRegistryFlags)

/* Contains all types currently available for use in Niagara
* Used by UI to provide selection; new uniforms and variables
* may be instanced using the types provided here
*/
class NIAGARA_API FNiagaraTypeRegistry : public FGCObject
{
public:
	enum
	{
		MaxRegisteredTypes = 512,
	};

	using RegisteredTypesArray = TArray<FNiagaraTypeDefinition, TFixedAllocator<MaxRegisteredTypes>>;

	static const RegisteredTypesArray& GetRegisteredTypes()
	{
		return Get().RegisteredTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetRegisteredUserVariableTypes()
	{
		return Get().RegisteredUserVariableTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetRegisteredSystemVariableTypes()
	{
		return Get().RegisteredSystemVariableTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetRegisteredEmitterVariableTypes()
	{
		return Get().RegisteredEmitterVariableTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetRegisteredParticleVariableTypes()
	{
		return Get().RegisteredParticleVariableTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetRegisteredParameterTypes()
	{
		return Get().RegisteredParamTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetRegisteredPayloadTypes()
	{
		return Get().RegisteredPayloadTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetUserDefinedTypes()
	{
		return Get().RegisteredUserDefinedTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetNumericTypes()
	{ 
		return Get().RegisteredNumericTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetIndexTypes()
	{
		return Get().RegisteredIndexTypes;
	}

	static UNiagaraDataInterfaceBase* GetDefaultDataInterfaceByName(const FString& DIClassName);

	static void ClearUserDefinedRegistry()
	{
		FNiagaraTypeRegistry& Registry = Get();

		FRWScopeLock Lock(Registry.RegisteredTypesLock, SLT_Write);

		for (const FNiagaraTypeDefinition& Def : Registry.RegisteredUserDefinedTypes)
		{
			Registry.RegisteredPayloadTypes.Remove(Def);
			Registry.RegisteredParamTypes.Remove(Def);
			Registry.RegisteredNumericTypes.Remove(Def);
			Registry.RegisteredIndexTypes.Remove(Def);
		}

		Registry.RegisteredUserDefinedTypes.Empty();

		// note that we don't worry about cleaning up RegisteredTypes or RegisteredTypeIndexMap because we don't
		// want to invalidate any indexes that are already stored in FNiagaraTypeDefinitionHandle.  If re-registered
		// they will be given the same index, and if they are orphaned we don't want to have invalid indices on the handle.
	}

	UE_DEPRECATED(4.27, "This overload is deprecated, please use the Register function that takes registration flags instead.")
	static void Register(const FNiagaraTypeDefinition& NewType, bool bCanBeParameter, bool bCanBePayload, bool bIsUserDefined)
	{
		ENiagaraTypeRegistryFlags Flags =
			ENiagaraTypeRegistryFlags::AllowUserVariable |
			ENiagaraTypeRegistryFlags::AllowSystemVariable |
			ENiagaraTypeRegistryFlags::AllowEmitterVariable;
		if (bCanBeParameter)
		{
			Flags |= ENiagaraTypeRegistryFlags::AllowParameter;
		}
		if (bCanBePayload)
		{
			Flags |= ENiagaraTypeRegistryFlags::AllowPayload;
		}
		if (bIsUserDefined)
		{
			Flags |= ENiagaraTypeRegistryFlags::IsUserDefined;
		}
										
		Register(NewType, Flags);
	}

	static void Register(const FNiagaraTypeDefinition &NewType, ENiagaraTypeRegistryFlags Flags)
	{
		FNiagaraTypeRegistry& Registry = Get();

		FRWScopeLock Lock(Registry.RegisteredTypesLock, SLT_Write);

		//TODO: Make this a map of type to a more verbose set of metadata? Such as the hlsl defs, offset table for conversions etc.
		Registry.RegisteredTypeIndexMap.Add(GetTypeHash(NewType), Registry.RegisteredTypes.AddUnique(NewType));

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::AllowUserVariable))
		{
			Registry.RegisteredUserVariableTypes.AddUnique(NewType);
		}

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::AllowSystemVariable))
		{
			Registry.RegisteredSystemVariableTypes.AddUnique(NewType);
		}

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::AllowEmitterVariable))
		{
			Registry.RegisteredEmitterVariableTypes.AddUnique(NewType);
		}

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::AllowParticleVariable))
		{
			Registry.RegisteredParticleVariableTypes.AddUnique(NewType);
		}

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::AllowParameter))
		{
			Registry.RegisteredParamTypes.AddUnique(NewType);
		}

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::AllowPayload))
		{
			Registry.RegisteredPayloadTypes.AddUnique(NewType);
		}

		if (EnumHasAnyFlags(Flags, ENiagaraTypeRegistryFlags::IsUserDefined))
		{
			Registry.RegisteredUserDefinedTypes.AddUnique(NewType);
		}

		if (FNiagaraTypeDefinition::IsValidNumericInput(NewType))
		{
			Registry.RegisteredNumericTypes.AddUnique(NewType);
		}

		if(NewType.IsIndexType())
		{
			Registry.RegisteredIndexTypes.AddUnique(NewType);
		}
	}

	static bool IsStaticPossible(const FNiagaraTypeDefinition& InSrc) 
	{
		if (InSrc.IsStatic())
			return true;

		for (const FNiagaraTypeDefinition& TypeDef : Get().RegisteredTypes)
		{
			if (InSrc.IsSameBaseDefinition(TypeDef) && TypeDef.IsStatic())
				return true;
		}

		return false;
	}

	static int32 RegisterIndexed(const FNiagaraTypeDefinition& NewType)
	{
		FNiagaraTypeRegistry& Registry = Get();

		const uint32 TypeHash = GetTypeHash(NewType);
		{
			FReadScopeLock Lock(Registry.RegisteredTypesLock);
			if (const int32* ExistingIndex = Registry.RegisteredTypeIndexMap.Find(TypeHash))
			{
				return *ExistingIndex;
			}
		}

		FRWScopeLock Lock(Registry.RegisteredTypesLock, SLT_Write);
		const int32 Index = Registry.RegisteredTypes.AddUnique(NewType);
		Registry.RegisteredTypeIndexMap.Add(TypeHash, Index);
		return Index;
	}

	static void RegisterStructConverter(const FNiagaraTypeDefinition& SourceType, const FNiagaraLwcStructConverter& StructConverter)
	{
		int32 TypeIndex = RegisterIndexed(SourceType);
		FNiagaraTypeRegistry& Registry = Get();
		
		FRWScopeLock Lock(Registry.RegisteredTypesLock, SLT_Write);
		Registry.RegisteredStructConversionMap.Add(TypeIndex, StructConverter);
	}

	static FNiagaraLwcStructConverter GetStructConverter(const FNiagaraTypeDefinition& SourceType)
	{
		FNiagaraTypeRegistry& Registry = Get();
		
		FReadScopeLock Lock(Registry.RegisteredTypesLock);
		const uint32 TypeHash = GetTypeHash(SourceType);
		if (const int32* TypeIndex = Registry.RegisteredTypeIndexMap.Find(TypeHash))
		{
			if (FNiagaraLwcStructConverter* Converter = Registry.RegisteredStructConversionMap.Find(*TypeIndex))
			{
				return *Converter;
			}
		}
		
		return FNiagaraLwcStructConverter();
	}

	/** LazySingleton interface */
	static FNiagaraTypeRegistry& Get();
	static void TearDown();

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const;

private:
	friend class FLazySingleton;

	FNiagaraTypeRegistry();
	virtual ~FNiagaraTypeRegistry();

	RegisteredTypesArray RegisteredTypes;

	TArray<FNiagaraTypeDefinition> RegisteredUserVariableTypes;
	TArray<FNiagaraTypeDefinition> RegisteredSystemVariableTypes;
	TArray<FNiagaraTypeDefinition> RegisteredEmitterVariableTypes;
	TArray<FNiagaraTypeDefinition> RegisteredParticleVariableTypes;
	TArray<FNiagaraTypeDefinition> RegisteredParamTypes;
	TArray<FNiagaraTypeDefinition> RegisteredPayloadTypes;
	TArray<FNiagaraTypeDefinition> RegisteredUserDefinedTypes;
	TArray<FNiagaraTypeDefinition> RegisteredNumericTypes;
	TArray<FNiagaraTypeDefinition> RegisteredIndexTypes;


	TMap<uint32, int32> RegisteredTypeIndexMap;
	TMap<TWeakObjectPtr<UScriptStruct>, TWeakObjectPtr<UScriptStruct>> LWCRegisteredStructRemapping;
	TMap<uint32, FNiagaraLwcStructConverter> RegisteredStructConversionMap;
	FRWLock RegisteredTypesLock;
};

USTRUCT()
struct FNiagaraTypeDefinitionHandle
{
	GENERATED_USTRUCT_BODY()

	FNiagaraTypeDefinitionHandle() : RegisteredTypeIndex(INDEX_NONE) {}
	explicit FNiagaraTypeDefinitionHandle(const FNiagaraTypeDefinition& Type) : RegisteredTypeIndex(Register(Type)) {}
	explicit FNiagaraTypeDefinitionHandle(const FNiagaraTypeDefinitionHandle& Handle) : RegisteredTypeIndex(Handle.RegisteredTypeIndex) {}

	const FNiagaraTypeDefinition& operator*() const { return Resolve(); }
	const FNiagaraTypeDefinition* operator->() const { return &Resolve(); }

	bool operator==(const FNiagaraTypeDefinitionHandle& Other) const
	{
		return RegisteredTypeIndex == Other.RegisteredTypeIndex;
	}
	bool operator!=(const FNiagaraTypeDefinitionHandle& Other) const
	{
		return RegisteredTypeIndex != Other.RegisteredTypeIndex;
	}

	bool IsSameBase(const FNiagaraTypeDefinition& Other) const
	{
		return Resolve().IsSameBaseDefinition(Other);
	}

	bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
	{
		return Resolve().AppendCompileHash(InVisitor);
	}

private:
	friend FArchive& operator<<(FArchive& Ar, FNiagaraTypeDefinitionHandle& Handle);

	NIAGARA_API const FNiagaraTypeDefinition& Resolve() const;
	NIAGARA_API int32 Register(const FNiagaraTypeDefinition& TypeDef) const;

	UPROPERTY()
	int32 RegisteredTypeIndex = INDEX_NONE;
};

//////////////////////////////////////////////////////////////////////////
USTRUCT()
struct FNiagaraVariableBase
{
	GENERATED_USTRUCT_BODY()

	FORCEINLINE FNiagaraVariableBase()
		: Name(NAME_None)
		, TypeDefHandle(FNiagaraTypeDefinition::GetVec4Def())
#if WITH_EDITORONLY_DATA
		, TypeDef_DEPRECATED(FNiagaraTypeDefinition::GetVec4Def())
#endif
		{}

	FORCEINLINE FNiagaraVariableBase(const FNiagaraVariableBase &Other)
		: Name(Other.Name)
		, TypeDefHandle(Other.TypeDefHandle)
#if WITH_EDITORONLY_DATA
		, TypeDef_DEPRECATED(Other.TypeDef_DEPRECATED)
#endif
		{}

	FORCEINLINE FNiagaraVariableBase(const FNiagaraTypeDefinition& InType, const FName& InName)
		: Name(InName)
		, TypeDefHandle(InType)
#if WITH_EDITORONLY_DATA
		, TypeDef_DEPRECATED(InType)
#endif
		{}

	/** Check if Name and Type definition are the same. The actual stored value is not checked here.*/
	bool operator==(const FNiagaraVariableBase& Other)const
	{
		return Name == Other.Name && TypeDefHandle == Other.TypeDefHandle;
	}

	/** Check if Name and Type definition are not the same. The actual stored value is not checked here.*/
	bool operator!=(const FNiagaraVariableBase& Other)const
	{
		return !(*this == Other);
	}

	/** Variables are the same name but if types are auto-assignable, allow them to match. */
	bool IsEquivalent(const FNiagaraVariableBase& Other, bool bAllowAssignableTypes = true)const
	{
		return Name == Other.Name && (TypeDefHandle == Other.TypeDefHandle || (bAllowAssignableTypes && FNiagaraTypeDefinition::TypesAreAssignable(*TypeDefHandle, *Other.TypeDefHandle)));
	}
	
	FORCEINLINE void SetName(FName InName)
	{
		Name = InName;
	}
	FORCEINLINE const FName& GetName() const
	{
		return Name;
	}

	void SetNamespacedName(const FString& InNamespace, FName InVariableName);

	void SetType(const FNiagaraTypeDefinition& InTypeDef)
	{
		TypeDefHandle = FNiagaraTypeDefinitionHandle(InTypeDef);
	}
	const FNiagaraTypeDefinition& GetType()const
	{
		return *TypeDefHandle;
	}

	FORCEINLINE bool IsDataInterface()const { return GetType().IsDataInterface(); }
	FORCEINLINE bool IsUObject()const { return GetType().IsUObject(); }

	int32 GetSizeInBytes() const
	{
		return TypeDefHandle->GetSize();
	}

	int32 GetAlignment()const
	{
		return TypeDefHandle->GetAlignment();
	}

	bool IsValid() const
	{
		return Name != NAME_None && TypeDefHandle->IsValid();
	}

	/**
	Removes the root namespace from the variable if it matches the expected namespace.
	i.e. MyNamespace.VarName would become VarName but Other.MyNamespace.VarName would not be modified.
	For more complex namespace manipulation refer to FNiagaraUtilities::ResolveAliases
	*/
	bool RemoveRootNamespace(const FStringView& ExpectedNamespace);

	/**
	Replaces the root namespace from the variable if it matches the expected namespace with the NewNamespace.
	i.e. Prev.VarName would become Next.VarName but Other.Prev.VarName would not be modified.
	For more complex namespace manipulation refer to FNiagaraUtilities::ResolveAliases
	*/
	bool ReplaceRootNamespace(const FStringView& ExpectedNamespace, const FStringView& NewNamespace);

	FORCEINLINE bool IsInNameSpace(const FStringView& Namespace) const
	{
		FNameBuilder NameString;
		Name.ToString(NameString);
		
		FStringView NameStringView = NameString.ToView();
		if (Namespace.Len() > 1 && Namespace[Namespace.Len() - 1] == '.') // Includes period in namespace
		{
			return (NameStringView.Len() > Namespace.Len()) && NameStringView.StartsWith(Namespace);
		}
		else // Skips period, makes sure it's in the name string to delineate
		{
			return (NameStringView.Len() > Namespace.Len() + 1) && (NameStringView[Namespace.Len()] == '.') && NameStringView.StartsWith(Namespace);
		}
	}

#if WITH_EDITORONLY_DATA
	// This method should not be used at runtime as we have pre-defined strings in FNiagaraConstants for runtime cases
	FORCEINLINE bool IsInNameSpace(const FName& Namespace) const
	{
		FNameBuilder NamespaceString;
		Namespace.ToString(NamespaceString);
		return IsInNameSpace(NamespaceString.ToView());
	}
#endif

	bool Serialize(FArchive& Ar);
#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif

	bool NIAGARA_API AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Variable")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Variable")
	FNiagaraTypeDefinitionHandle TypeDefHandle;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (DeprecatedProperty))
	FNiagaraTypeDefinition TypeDef_DEPRECATED;
#endif
};

template<>
struct TStructOpsTypeTraits<FNiagaraVariableBase> : public TStructOpsTypeTraitsBase2<FNiagaraVariableBase>
{
	enum
	{
		WithSerializer = true,
#if WITH_EDITORONLY_DATA
		WithPostSerialize = true,
#endif
	};
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraVariableBase& Var)
{
	return HashCombine(GetTypeHash(Var.GetType()), GetTypeHash(Var.GetName()));
}

USTRUCT()
struct FNiagaraVariable : public FNiagaraVariableBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraVariable()
	{
	}

	FNiagaraVariable(const FNiagaraVariable &Other)
		: FNiagaraVariableBase(Other)
	{
		if (Other.GetType().IsValid() && Other.IsDataAllocated())
		{
			SetData(Other.GetData());
		}
	}

	FNiagaraVariable(const FNiagaraVariableBase& Other)
		: FNiagaraVariableBase(Other)
	{
	}

	FORCEINLINE FNiagaraVariable(const FNiagaraTypeDefinition& InType, const FName& InName)
		: FNiagaraVariableBase(InType, InName)
	{
	}
	
	/** Check if Name and Type definition are the same. The actual stored value is not checked here.*/
	bool operator==(const FNiagaraVariable& Other)const
	{
		//-TODO: Should this check the value???
		return Name == Other.Name && TypeDefHandle == Other.TypeDefHandle;
	}

	/** Check if Name and Type definition are not the same. The actual stored value is not checked here.*/
	bool operator!=(const FNiagaraVariable& Other)const
	{
		return !(*this == Other);
	}

	/** Checks if the types match and either both variables are uninitialized or both hold exactly the same data.*/
	bool HoldsSameData(const FNiagaraVariable& Other) const
	{
		if (TypeDefHandle != Other.TypeDefHandle) {
			return false;
		}
		if (!IsDataAllocated() && !Other.IsDataAllocated()) {
			return true;
		}
		return IsDataAllocated() && Other.IsDataAllocated() && VarData.Num() == Other.VarData.Num() && FMemory::Memcmp(VarData.GetData(), Other.VarData.GetData(), VarData.Num()) == 0;
	}

	// Var data operations
	void AllocateData()
	{
		if (VarData.Num() != TypeDefHandle->GetSize())
		{
			VarData.SetNumZeroed(TypeDefHandle->GetSize());
		}
	}

	bool IsDataAllocated() const
	{
		return VarData.Num() > 0 && VarData.Num() == TypeDefHandle->GetSize();
	}

	void CopyTo(uint8* Dest) const
	{
		check(TypeDefHandle->GetSize() == VarData.Num());
		check(IsDataAllocated());
		FMemory::Memcpy(Dest, VarData.GetData(), VarData.Num());
	}
		
	template<typename T>
	void SetValue(const T& Data)
	{
		static_assert(!TIsUECoreVariant<T, double>::Value, "Double core variant, please use SetDoubleValue.");
		check(sizeof(T) == TypeDefHandle->GetSize());
		AllocateData();
		FMemory::Memcpy(VarData.GetData(), &Data, VarData.Num());
	}

	template<typename T>
	T GetValue() const
	{
		static_assert(!TIsUECoreVariant<T, double>::Value, "Double core variant, please use GetDoubleValue.");
		check(sizeof(T) == TypeDefHandle->GetSize());
		check(IsDataAllocated());
		T Value;
		FMemory::Memcpy(&Value, GetData(), TypeDefHandle->GetSize());
		return Value;
	}

	void SetData(const uint8* Data)
	{
		check(Data);
		AllocateData();
		FMemory::Memcpy(VarData.GetData(), Data, VarData.Num());
	}

	const uint8* GetData() const
	{
		return VarData.GetData();
	}

	uint8* GetData()
	{
		return VarData.GetData();
	}

	void ClearData()
	{
		VarData.Empty();
	}

	int32 GetAllocatedSizeInBytes() const
	{
		return VarData.Num();
	}

	FString ToString()const
	{
		FString Ret = Name.ToString() + TEXT("(");
		Ret += TypeDefHandle->ToString(VarData.GetData());
		Ret += TEXT(")");
		return Ret;
	}

	static int32 SearchArrayForPartialNameMatch(const TArray<FNiagaraVariable>& Variables, const FName& VariableName)
	{
		FNameBuilder VarNameBuilder(VariableName);
		FStringView VarNameView(VarNameBuilder);

		int32 BestMatchLength = INDEX_NONE;
		int32 BestMatchIdx = INDEX_NONE;

		const int32 VarNameLength = VarNameView.Len();

		for (int32 i = 0; i < Variables.Num(); i++)
		{
			const FNiagaraVariable& TestVar = Variables[i];
			FNameBuilder TestVarNameBuilder(TestVar.GetName());
			FStringView TestVarNameView(TestVarNameBuilder);

			if (VarNameView.Equals(TestVarNameView))
			{
				return i;
			}
			else
			{
				const int32 TestVarNameLength = TestVarNameView.Len();

				if (VarNameLength > TestVarNameLength && (BestMatchLength == INDEX_NONE || TestVarNameLength > BestMatchLength))
				{
					if (VarNameView.StartsWith(TestVarNameView) && VarNameView[TestVarNameLength] == TCHAR('.'))
					{
						BestMatchIdx = i;
						BestMatchLength = TestVarNameLength;
					}
				}
			}
		}

		return BestMatchIdx;
	}

	bool Serialize(FArchive& Ar);
#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif

private:
	//This gets serialized but do we need to worry about endianness doing things like this? If not, where does that get handled?
	//TODO: Remove storage here entirely and move everything to an FNiagaraParameterStore.
	UPROPERTY(meta = (SkipForCompileHash = "true"))
	TArray<uint8> VarData;
};

struct FNiagaraVariableMatch
{
	explicit FNiagaraVariableMatch(const FNiagaraTypeDefinition& InType, const FName& InName)
		: VariableBase(InType, InName)
	{
	}

	bool operator()(const FNiagaraVariable& Other) const
	{
		return static_cast<const FNiagaraVariableBase&>(Other) == VariableBase;
	}

private:
	FNiagaraVariableBase VariableBase;
};


template<>
struct TStructOpsTypeTraits<FNiagaraVariable> : public TStructOpsTypeTraitsBase2<FNiagaraVariable>
{
	enum
	{
		WithSerializer = true,
#if WITH_EDITORONLY_DATA
		WithPostSerialize = true,
#endif
	};
};

template<>
inline bool FNiagaraVariable::GetValue<bool>() const
{
	check(TypeDefHandle.IsSameBase(FNiagaraTypeDefinition::GetBoolDef()));
	check(IsDataAllocated());
	FNiagaraBool* BoolStruct = (FNiagaraBool*)GetData();
	return BoolStruct->GetValue();
}

template<>
inline void FNiagaraVariable::SetValue<bool>(const bool& Data)
{
	check(TypeDefHandle.IsSameBase(FNiagaraTypeDefinition::GetBoolDef()));
	AllocateData();
	FNiagaraBool* BoolStruct = (FNiagaraBool*)GetData();
	BoolStruct->SetValue(Data);
}



// Any change to this structure, or it's GetVariables implementation will require a bump in the CustomNiagaraVersion so that we
// properly rebuild the scripts
// You must pad this struct and the results of GetVariables() to a 16 byte boundry.
struct alignas(16) FNiagaraGlobalParameters
{
#if WITH_EDITOR
	NIAGARA_API static const TArray<FNiagaraVariable>& GetVariables();
#endif

	float EngineDeltaTime =  0.0f;
	float EngineInvDeltaTime = 0.0f;
	float EngineTime = 0.0f;
	float EngineRealTime = 0.0f;
	int32 QualityLevel = 0;

	int32 _Pad0;
	int32 _Pad1;
	int32 _Pad2;
};

// Any change to this structure, or it's GetVariables implementation will require a bump in the CustomNiagaraVersion so that we
// properly rebuild the scripts
// You must pad this struct and the results of GetVariables() to a 16 byte boundary.
struct alignas(16) FNiagaraSystemParameters
{
#if WITH_EDITOR
	NIAGARA_API static const TArray<FNiagaraVariable>& GetVariables();
#endif

	float EngineTimeSinceRendered = 0.0f;
	float EngineLodDistance = 0.0f;
	float EngineLodDistanceFraction = 0.0f;
	float EngineSystemAge = 0.0f;
	uint32 EngineExecutionState = 0;
	int32 EngineTickCount = 0;
	int32 EngineEmitterCount = 0;
	int32 EngineAliveEmitterCount = 0;
	int32 SignificanceIndex = 0;
	int32 RandomSeed = 0;

	int32 _Pad1;
	int32 _Pad2;
};

// Any change to this structure, or it's GetVariables implementation will require a bump in the CustomNiagaraVersion so that we
// properly rebuild the scripts
// You must pad this struct and the results of GetVariables() to a 16 byte boundary.
struct alignas(16) FNiagaraOwnerParameters
{
#if WITH_EDITOR
	NIAGARA_API static const TArray<FNiagaraVariable>& GetVariables();
#endif

	FMatrix44f	EngineLocalToWorld = FMatrix44f::Identity;
	FMatrix44f	EngineWorldToLocal = FMatrix44f::Identity;
	FMatrix44f	EngineLocalToWorldTransposed = FMatrix44f::Identity;
	FMatrix44f	EngineWorldToLocalTransposed = FMatrix44f::Identity;
	FMatrix44f	EngineLocalToWorldNoScale = FMatrix44f::Identity;
	FMatrix44f	EngineWorldToLocalNoScale = FMatrix44f::Identity;
	FQuat4f		EngineRotation = FQuat4f::Identity;
	FVector3f	EnginePosition = FVector3f(ForceInitToZero);
	float		_Pad0;
	FVector3f	EngineVelocity = FVector3f(ForceInitToZero);
	float		_Pad1;
	FVector3f	EngineXAxis = FVector3f(1.0f, 0.0f, 0.0f);
	float		_Pad2;
	FVector3f	EngineYAxis = FVector3f(0.0f, 1.0f, 0.0f);
	float		_Pad3;
	FVector3f	EngineZAxis = FVector3f(0.0f, 0.0f, 1.0f);
	float		_Pad4;
	FVector3f	EngineScale = FVector3f(1.0f, 1.0f, 1.0f);
	float		_Pad5;
	FVector4f	EngineLWCTile = FVector4f(ForceInitToZero);
};

// Any change to this structure, or it's GetVariables implementation will require a bump in the CustomNiagaraVersion so that we
// properly rebuild the scripts
// You must pad this struct and the results of GetVariables() to a 16 byte boundary.
struct alignas(16) FNiagaraEmitterParameters
{
#if WITH_EDITOR
	NIAGARA_API static const TArray<FNiagaraVariable>& GetVariables();
#endif

	int32 EmitterNumParticles = 0;
	int32 EmitterTotalSpawnedParticles = 0;
	float EmitterSpawnCountScale = 1.0f;
	float EmitterAge = 0.0f;
	int32 EmitterRandomSeed = 0;
	int32 EmitterInstanceSeed = 0;

	// todo - what else should be inserted here?  we could put an array of spawninfos/interp spawn values
	int32 _Pad0;
	int32 _Pad1;
};

// Forward decl FVersionedNiagaraEmitterWeakPtr to support FVersionedNiagaraEmitter::ToWeakPtr().
struct FVersionedNiagaraEmitterWeakPtr;
struct FVersionedNiagaraEmitterData;
class UNiagaraEmitter;

/** Struct combining an emitter with a specific version.*/
USTRUCT()
struct NIAGARA_API FVersionedNiagaraEmitter
{
	GENERATED_USTRUCT_BODY()
	
	FVersionedNiagaraEmitter() {}
	FVersionedNiagaraEmitter(UNiagaraEmitter* InEmitter, const FGuid& InVersion)
		: Emitter(InEmitter), Version(InVersion)
	{};

	FVersionedNiagaraEmitterData* GetEmitterData() const;
	FVersionedNiagaraEmitterWeakPtr ToWeakPtr() const;

	UPROPERTY()
	TObjectPtr<UNiagaraEmitter> Emitter = nullptr;

	UPROPERTY()
	FGuid Version;

	bool operator==(const FVersionedNiagaraEmitter& Other) const
	{
		return Emitter == Other.Emitter && Version == Other.Version;
	}

	bool operator!=(const FVersionedNiagaraEmitter& Other) const
	{
		return !(*this == Other);
	}
};

struct NIAGARA_API FVersionedNiagaraEmitterWeakPtr
{
	FVersionedNiagaraEmitterWeakPtr() : Emitter(nullptr) {}
	FVersionedNiagaraEmitterWeakPtr(UNiagaraEmitter* InEmitter, const FGuid& InVersion);
	FVersionedNiagaraEmitter ResolveWeakPtr() const;
	FVersionedNiagaraEmitterData* GetEmitterData() const;
	bool IsValid() const;
	bool operator==(const FVersionedNiagaraEmitterWeakPtr& Other) const
	{
		return Emitter == Other.Emitter && Version == Other.Version;
	}


	bool operator!=(const FVersionedNiagaraEmitterWeakPtr& Other) const
	{
		return !(*this == Other);
	}

	TWeakObjectPtr<UNiagaraEmitter> Emitter;
	FGuid Version;
};

inline uint32 GetTypeHash(const FVersionedNiagaraEmitterWeakPtr& Item)
{
	return GetTypeHash(Item.Emitter);
}

inline uint32 GetTypeHash(const FVersionedNiagaraEmitter& Item)
{
	return GetTypeHash(Item.Emitter) + GetTypeHash(Item.Version);
}


