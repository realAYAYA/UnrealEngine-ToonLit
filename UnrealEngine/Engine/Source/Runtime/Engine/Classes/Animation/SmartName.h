// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "Animation/BoneReference.h"
#include "Animation/AnimTypes.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "Animation/AnimCurveMetadata.h"
#include "SmartName.generated.h"

struct FSmartName;

// DEPRECATED - smart names and their mappings are no longer used
USTRUCT()
struct FSmartNameMapping
{
	friend struct FSmartNameMappingIterator;
	friend class USkeleton;
	
	GENERATED_USTRUCT_BODY();

	ENGINE_API FSmartNameMapping();
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API FSmartName AddName(FName InName);

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool GetName(const SmartName::UID_Type& Uid, FName& OutName) const;
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void FillUidArray(TArray<SmartName::UID_Type>& Array) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void FillNameArray(TArray<FName>& Array) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void FillUIDToNameArray(TArray<FName>& Array) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void FillCurveTypeArray(TArray<FAnimCurveType>& Array) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void FillUIDToCurveTypeArray(TArray<FAnimCurveType>& Array) const;

#if WITH_EDITOR
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool Rename(const SmartName::UID_Type& Uid, FName NewName);

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool Remove(const SmartName::UID_Type& Uid);

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool Remove(const FName& Name);
#endif

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API SmartName::UID_Type FindUID(const FName& Name) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool Exists(const SmartName::UID_Type& Uid) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool Exists(const FName& Name) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool FindSmartName(FName Name, FSmartName& OutName) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API bool FindSmartNameByUID(SmartName::UID_Type UID, FSmartName& OutName) const;

	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API FCurveMetaData* GetCurveMetaData(FName CurveName);
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API const FCurveMetaData* GetCurveMetaData(FName CurveName) const;
	
#if !WITH_EDITOR
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API const FCurveMetaData& GetCurveMetaData(SmartName::UID_Type CurveUID) const;
#endif
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void Serialize(FArchive& Ar);
	
	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& Elem);
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void InitializeCurveMetaData(class USkeleton* Skeleton);
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	SmartName::UID_Type GetMaxUID() const { return (SmartName::UID_Type)(CurveNameList.Num() - 1); }
	
	UE_DEPRECATED(5.3, "FSmartNameMapping functions are no longer used.")
	ENGINE_API void Iterate(TFunction<void(const struct FSmartNameMappingIterator& Iterator)> Callback) const;

private:
	/*Internal no lock function to prevent re-entrant locking, see API function GetName for documentation.*/
	ENGINE_API bool GetName_NoLock(const SmartName::UID_Type& Uid, FName& OutName) const;

	/*Internal no lock function to prevent re-entrant locking, see API function Exists for documentation.*/
	ENGINE_API bool Exists_NoLock(const SmartName::UID_Type& Uid) const;
	
	/*Internal no lock function to prevent re-entrant locking, see API function Exists for documentation.*/
	ENGINE_API bool Exists_NoLock(const FName& Name) const;

	/*Internal no lock function to prevent re-entrant locking, see API function FindUID for documentation.*/
	ENGINE_API SmartName::UID_Type FindUID_NoLock(const FName& Name) const;
	
	/*Internal no lock function to prevent re-entrant locking, see API function GetCurveMetaData for documentation.*/
	ENGINE_API const FCurveMetaData* GetCurveMetaData_NoLock(FName CurveName) const;

	// List of curve names, indexed by UID
	TArray<FName> CurveNameList;

#if !WITH_EDITOR
	// List of curve metadata, indexed by UID
	TArray<FCurveMetaData> CurveMetaDataList;
#endif

	TMap<FName, FCurveMetaData> CurveMetaDataMap;
};

// Struct for providing access to SmartNameMapping data within FSmartNameMapping::Iterate callback functions
struct FSmartNameMappingIterator
{
	public:
		friend struct FSmartNameMapping;

		UE_DEPRECATED(5.3, "FSmartNameMappingIterator functions are no longer used.")
		bool GetName(FName& OutCurveName) const
		{
			return Mapping->GetName_NoLock(Index, OutCurveName);
		}

		UE_DEPRECATED(5.3, "FSmartNameMappingIterator functions are no longer used.")
		const FCurveMetaData* GetCurveMetaData() const
		{
			FName Name;
			if (Mapping->GetName_NoLock(Index, Name))
			{
				return Mapping->GetCurveMetaData_NoLock(Name);
			}
			else
			{
				return nullptr;
			}
		}

		UE_DEPRECATED(5.3, "FSmartNameMappingIterator functions are no longer used.")
		SmartName::UID_Type GetIndex() const { return Index; }
	
	private:
		// This class struct should only be crated by FSmartNameMapping::Iterate
		FSmartNameMappingIterator(const FSmartNameMapping* InMapping, SmartName::UID_Type InIndex):
			Mapping(InMapping), Index(InIndex)
		{}
	
		const FSmartNameMapping* Mapping;
		SmartName::UID_Type Index;
};

USTRUCT()
struct FSmartNameContainer
{
	GENERATED_USTRUCT_BODY();

	UE_DEPRECATED(5.3, "FSmartNameContainer functions are no longer used.")
	ENGINE_API FSmartNameMapping* AddContainer(FName NewContainerName);

	UE_DEPRECATED(5.3, "FSmartNameContainer functions are no longer used.")
	ENGINE_API const FSmartNameMapping* GetContainer(FName ContainerName) const;

	UE_DEPRECATED(5.3, "FSmartNameContainer functions are no longer used.")
	ENGINE_API void Serialize(FArchive& Ar, bool bIsTemplate);

	UE_DEPRECATED(5.3, "FSmartNameContainer functions are no longer used.")
	ENGINE_API void PostLoad();

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& Elem);

	/** Only restricted classes can access the protected interface */
	friend class USkeleton;
protected:
	ENGINE_API FSmartNameMapping* GetContainerInternal(const FName& ContainerName);
	ENGINE_API const FSmartNameMapping* GetContainerInternal(const FName& ContainerName) const;

private:
	TMap<FName, FSmartNameMapping> NameMappings;	// List of smartname mappings

#if WITH_EDITORONLY_DATA
	// Editor copy of the data we loaded, used to preserve determinism during cooking
	TMap<FName, FSmartNameMapping> LoadedNameMappings;
#endif
};

template<>
struct TStructOpsTypeTraits<FSmartNameContainer> : public TStructOpsTypeTraitsBase2<FSmartNameContainer>
{
	enum
	{
		WithCopy = false,
	};
};

USTRUCT()
struct FSmartName
{
	GENERATED_USTRUCT_BODY();

	// name 
	UPROPERTY(VisibleAnywhere, Category=FSmartName)
	FName DisplayName;

	// SmartName::UID_Type - for faster access
	SmartName::UID_Type	UID;

	FSmartName()
		: DisplayName(NAME_None)
		, UID(SmartName::MaxUID)
	{}

	UE_DEPRECATED(5.3, "FSmartName functions are no longer used.")
	FSmartName(const FName& InDisplayName, SmartName::UID_Type InUID)
		: DisplayName(InDisplayName)
		, UID(InUID)
	{}
	
	bool operator==(FSmartName const& Other) const
	{
		return (DisplayName == Other.DisplayName && UID == Other.UID);
	}

	bool operator!=(const FSmartName& Other) const
	{
		return !(*this == Other);
	}

	ENGINE_API bool Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartName& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	UE_DEPRECATED(5.3, "FSmartName functions are no longer used.")
	bool IsValid() const
	{
		return UID != SmartName::MaxUID;
	}
};

template<>
struct TStructOpsTypeTraits<FSmartName> : public TStructOpsTypeTraitsBase2<FSmartName>
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};
