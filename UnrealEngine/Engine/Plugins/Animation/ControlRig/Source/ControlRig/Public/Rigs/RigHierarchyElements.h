// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyMetadata.h"
#include "RigHierarchyElements.generated.h"

struct FRigUnitContext;
struct FRigBaseElement;
struct FRigControlElement;
class URigHierarchy;

DECLARE_DELEGATE_RetVal_ThreeParams(FTransform, FRigReferenceGetWorldTransformDelegate, const FRigUnitContext*, const FRigElementKey& /* Key */, bool /* bInitial */);
DECLARE_DELEGATE_TwoParams(FRigElementMetadataChangedDelegate, const FRigElementKey& /* Key */, const FName& /* Name */);
DECLARE_DELEGATE_ThreeParams(FRigElementMetadataTagChangedDelegate, const FRigElementKey& /* Key */, const FName& /* Tag */, bool /* AddedOrRemoved */);
DECLARE_DELEGATE_RetVal_ThreeParams(FTransform, FRigReferenceGetWorldTransformDelegate, const FRigUnitContext*, const FRigElementKey& /* Key */, bool /* bInitial */);

#define DECLARE_RIG_ELEMENT_METHODS(ElementType) \
template<typename T> \
friend FORCEINLINE const T* Cast(const ElementType* InElement) \
{ \
   return Cast<T>((const FRigBaseElement*) InElement); \
} \
template<typename T> \
friend FORCEINLINE T* Cast(ElementType* InElement) \
{ \
   return Cast<T>((FRigBaseElement*) InElement); \
} \
template<typename T> \
friend FORCEINLINE const T* CastChecked(const ElementType* InElement) \
{ \
	return CastChecked<T>((const FRigBaseElement*) InElement); \
} \
template<typename T> \
friend FORCEINLINE T* CastChecked(ElementType* InElement) \
{ \
	return CastChecked<T>((FRigBaseElement*) InElement); \
}

UENUM()
namespace ERigTransformType
{
	enum Type
	{
		InitialLocal,
		CurrentLocal,
		InitialGlobal,
		CurrentGlobal,
		NumTransformTypes
	};
}

namespace ERigTransformType
{
	FORCEINLINE ERigTransformType::Type SwapCurrentAndInitial(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
			{
				return InitialLocal;
			}
			case CurrentGlobal:
			{
				return InitialGlobal;
			}
			case InitialLocal:
			{
				return CurrentLocal;
			}
			default:
			{
				break;
			}
		}
		return CurrentGlobal;
	}

	FORCEINLINE Type SwapLocalAndGlobal(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
			{
				return CurrentGlobal;
			}
			case CurrentGlobal:
			{
				return CurrentLocal;
			}
			case InitialLocal:
			{
				return InitialGlobal;
			}
			default:
			{
				break;
			}
		}
		return InitialLocal;
	}

	FORCEINLINE Type MakeLocal(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
			case CurrentGlobal:
			{
				return CurrentLocal;
			}
			default:
			{
				break;
			}
		}
		return InitialLocal;
	}

	FORCEINLINE Type MakeGlobal(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
			case CurrentGlobal:
			{
				return CurrentGlobal;
			}
			default:
			{
				break;
			}
		}
		return InitialGlobal;
	}

	FORCEINLINE Type MakeInitial(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
			case InitialLocal:
			{
				return InitialLocal;
			}
			default:
			{
				break;
			}
		}
		return InitialGlobal;
	}

	FORCEINLINE Type MakeCurrent(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
        	case InitialLocal:
			{
				return CurrentLocal;
			}
			default:
			{
				break;
			}
		}
		return CurrentGlobal;
	}

	FORCEINLINE bool IsLocal(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
        	case InitialLocal:
			{
				return true;
			}
			default:
			{
				break;
			}
		}
		return false;
	}

	FORCEINLINE bool IsGlobal(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentGlobal:
        	case InitialGlobal:
			{
				return true;;
			}
			default:
			{
				break;
			}
		}
		return false;
	}

	FORCEINLINE bool IsInitial(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case InitialLocal:
        	case InitialGlobal:
			{
				return true;
			}
			default:
			{
				break;
			}
		}
		return false;
	}

	FORCEINLINE bool IsCurrent(const Type InTransformType)
	{
		switch(InTransformType)
		{
			case CurrentLocal:
        	case CurrentGlobal:
			{
				return true;
			}
			default:
			{
				break;
			}
		}
		return false;
	}
}

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigComputedTransform
{
	GENERATED_BODY()

	FRigComputedTransform()
	: Transform(FTransform::Identity)
	, bDirty(false)
	{}

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);

	FORCEINLINE_DEBUGGABLE void Set(const FTransform& InTransform)
	{
#if WITH_EDITOR
		ensure(InTransform.GetRotation().IsNormalized());
#endif
		// ensure(!FMath::IsNearlyZero(InTransform.GetScale3D().X));
		// ensure(!FMath::IsNearlyZero(InTransform.GetScale3D().Y));
		// ensure(!FMath::IsNearlyZero(InTransform.GetScale3D().Z));
		Transform = InTransform;
		bDirty = false;
	}

	FORCEINLINE static bool Equals(const FTransform& A, const FTransform& B, const float InTolerance = 0.0001f)
	{
		return (A.GetTranslation() - B.GetTranslation()).IsNearlyZero(InTolerance) &&
			A.GetRotation().Equals(B.GetRotation(), InTolerance) &&
			(A.GetScale3D() - B.GetScale3D()).IsNearlyZero(InTolerance);
	}

	FORCEINLINE bool operator == (const FRigComputedTransform& Other) const
	{
		return bDirty == Other.bDirty && Equals(Transform, Other.Transform);
    }

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FTransform Transform;

	bool bDirty;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigLocalAndGlobalTransform
{
	GENERATED_BODY()

	FRigLocalAndGlobalTransform()
    : Local()
    , Global()
	{}

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);

	FORCEINLINE bool operator == (const FRigLocalAndGlobalTransform& Other) const
	{
		return Local == Other.Local && Global == Other.Global;
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FRigComputedTransform Local;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FRigComputedTransform Global;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigCurrentAndInitialTransform
{
	GENERATED_BODY()

	FRigCurrentAndInitialTransform()
    : Current()
    , Initial()
	{}

	FORCEINLINE const FRigComputedTransform& operator[](const ERigTransformType::Type InTransformType) const
	{
		switch(InTransformType)
		{
			case ERigTransformType::CurrentLocal:
			{
				return Current.Local;
			}
			case ERigTransformType::CurrentGlobal:
			{
				return Current.Global;
			}
			case ERigTransformType::InitialLocal:
			{
				return Initial.Local;
			}
			default:
			{
				break;
			}
		}
		return Initial.Global;
	}

	FORCEINLINE FRigComputedTransform& operator[](const ERigTransformType::Type InTransformType)
	{
		switch(InTransformType)
		{
			case ERigTransformType::CurrentLocal:
			{
				return Current.Local;
			}
			case ERigTransformType::CurrentGlobal:
			{
				return Current.Global;
			}
			case ERigTransformType::InitialLocal:
			{
				return Initial.Local;
			}
			default:
			{
				break;
			}
		}
		return Initial.Global;
	}

	FORCEINLINE const FTransform& Get(const ERigTransformType::Type InTransformType) const
	{
		return operator[](InTransformType).Transform;
	}

	FORCEINLINE void Set(const ERigTransformType::Type InTransformType, const FTransform& InTransform)
	{
		operator[](InTransformType).Set(InTransform);
	}

	FORCEINLINE bool IsDirty(const ERigTransformType::Type InTransformType) const
	{
		return operator[](InTransformType).bDirty;
	}

	FORCEINLINE void MarkDirty(const ERigTransformType::Type InTransformType)
	{
		ensure(!(operator[](ERigTransformType::SwapLocalAndGlobal(InTransformType)).bDirty));
		operator[](InTransformType).bDirty = true;
	}

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);

	FORCEINLINE bool operator == (const FRigCurrentAndInitialTransform& Other) const
	{
		return Current == Other.Current && Initial == Other.Initial;
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FRigLocalAndGlobalTransform Current;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FRigLocalAndGlobalTransform Initial;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigPreferredEulerAngles
{
	GENERATED_BODY()

	static constexpr EEulerRotationOrder DefaultRotationOrder = EEulerRotationOrder::YZX;

	FRigPreferredEulerAngles()
	: RotationOrder(DefaultRotationOrder) // default for rotator
	, Current(FVector::ZeroVector)
	, Initial(FVector::ZeroVector)
	{}

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);

	FORCEINLINE bool operator == (const FRigPreferredEulerAngles& Other) const
	{
		return RotationOrder == Other.RotationOrder &&
			Current == Other.Current &&
			Initial == Other.Initial;
	}

	void Reset();
	FORCEINLINE FVector& Get(bool bInitial = false) { return bInitial ? Initial : Current; }
	FORCEINLINE const FVector& Get(bool bInitial = false) const { return bInitial ? Initial : Current; }
	FRotator GetRotator(bool bInitial = false) const;
	FRotator SetRotator(const FRotator& InValue, bool bInitial = false, bool bFixEulerFlips = false);
	FVector GetAngles(bool bInitial = false, EEulerRotationOrder InRotationOrder = DefaultRotationOrder) const;
	void SetAngles(const FVector& InValue, bool bInitial = false, EEulerRotationOrder InRotationOrder = DefaultRotationOrder);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	EEulerRotationOrder RotationOrder;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FVector Current;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pose")
	FVector Initial;
};


struct FRigBaseElement;
//typedef TArray<FRigBaseElement*> FRigBaseElementChildrenArray;
typedef TArray<FRigBaseElement*, TInlineAllocator<3>> FRigBaseElementChildrenArray;
//typedef TArray<FRigBaseElement*> FRigBaseElementParentArray;
typedef TArray<FRigBaseElement*, TInlineAllocator<1>> FRigBaseElementParentArray;

struct CONTROLRIG_API FRigElementHandle
{
public:

	FRigElementHandle()
		: Hierarchy(nullptr)
		, Key()
	{}

	FRigElementHandle(URigHierarchy* InHierarchy, const FRigElementKey& InKey);
	FRigElementHandle(URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

	bool IsValid() const { return Get() != nullptr; }
	operator bool() const { return IsValid(); }
	
	const URigHierarchy* GetHierarchy() const { return Hierarchy.Get(); }
	URigHierarchy* GetHierarchy() { return Hierarchy.Get(); }
	const FRigElementKey& GetKey() const { return Key; }

	const FRigBaseElement* Get() const;
	FRigBaseElement* Get();

	template<typename T>
	T* Get()
	{
		return Cast<T>(Get());
	}

	template<typename T>
	const T* Get() const
	{
		return Cast<T>(Get());
	}

	template<typename T>
	T* GetChecked()
	{
		return CastChecked<T>(Get());
	}

	template<typename T>
	const T* GetChecked() const
	{
		return CastChecked<T>(Get());
	}

private:

	TWeakObjectPtr<URigHierarchy> Hierarchy;
	FRigElementKey Key;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigBaseElement
{
	GENERATED_BODY()

public:

	FRigBaseElement()
    : Key()
	, NameString()
    , Index(INDEX_NONE)
	, SubIndex(INDEX_NONE)
	, bSelected(false)
	, CreatedAtInstructionIndex(INDEX_NONE)
	, TopologyVersion(0)
	, MetadataVersion(0)
	, OwnedInstances(0)
	{}

	FRigBaseElement(const FRigBaseElement& InOther);
	FRigBaseElement& operator= (const FRigBaseElement& InOther);

	virtual ~FRigBaseElement();

	enum ESerializationPhase
	{
		StaticData,
		InterElementData
	};

protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = RigElement, meta = (AllowPrivateAccess = "true"))
	FRigElementKey Key;

	UPROPERTY(transient)
	FString NameString;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = RigElement, meta = (AllowPrivateAccess = "true"))
	int32 Index;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = RigElement, meta = (AllowPrivateAccess = "true"))
	int32 SubIndex;

	UPROPERTY(BlueprintReadOnly, Transient, Category = RigElement, meta = (AllowPrivateAccess = "true"))
	bool bSelected;

	UPROPERTY(BlueprintReadOnly, Transient, Category = RigElement, meta = (AllowPrivateAccess = "true"))
	int32 CreatedAtInstructionIndex;

	TArray<FRigBaseMetadata*> Metadata;
	TMap<FName,int32> MetadataNameToIndex;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return true;
	}

public:

	UScriptStruct* GetElementStruct() const;
	void Serialize(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase);
	virtual void Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase);
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase);

	FORCEINLINE const FName& GetName() const { return Key.Name; }
	FORCEINLINE const FString& GetNameString() const { return NameString; }
	FORCEINLINE virtual const FName& GetDisplayName() const { return GetName(); }
	FORCEINLINE ERigElementType GetType() const { return Key.Type; }
	FORCEINLINE const FRigElementKey& GetKey() const { return Key; }
	FORCEINLINE int32 GetIndex() const { return Index; }
	FORCEINLINE int32 GetSubIndex() const { return SubIndex; }
	FORCEINLINE bool IsSelected() const { return bSelected; }
	FORCEINLINE int32 GetCreatedAtInstructionIndex() const { return CreatedAtInstructionIndex; }
	FORCEINLINE bool IsProcedural() const { return CreatedAtInstructionIndex != INDEX_NONE; }
	FORCEINLINE int32 GetMetadataVersion() const { return MetadataVersion; }

	FORCEINLINE int32 NumMetadata() const { return Metadata.Num(); }
	FORCEINLINE FRigBaseMetadata* GetMetadata(int32 InIndex) const { return Metadata[InIndex]; }
	FORCEINLINE FRigBaseMetadata* GetMetadata(const FName& InName) const
	{
		if(const int32* MetadataIndex = MetadataNameToIndex.Find(InName))
		{
			return GetMetadata(*MetadataIndex);
		}
		return nullptr;
	}
	FORCEINLINE FRigBaseMetadata* GetMetadata(const FName& InName, ERigMetadataType InType) const
	{
		if(const int32* MetadataIndex = MetadataNameToIndex.Find(InName))
		{
			FRigBaseMetadata* Md = GetMetadata(*MetadataIndex);
			if(Md->GetType() == InType)
			{
				return Md;
			}
		}
		return nullptr;
	}
	FORCEINLINE bool SetMetaData(const FName& InName, ERigMetadataType InType, const void* InData, int32 InSize)
	{
		if(FRigBaseMetadata* Md = SetupValidMetadata(InName, InType))
		{
			return Md->SetValueData(InData, InSize);
		}
		return false;
	}
	bool RemoveMetadata(const FName& InName);
	bool RemoveAllMetadata();

	template<typename T>
	FORCEINLINE bool IsA() const { return T::IsClassOf(this); }

	FORCEINLINE bool IsTypeOf(ERigElementType InElementType) const
	{
		return Key.IsTypeOf(InElementType);
	}

	template<typename T>
    friend FORCEINLINE const T* Cast(const FRigBaseElement* InElement)
	{
		if(InElement)
		{
			if(InElement->IsA<T>())
			{
				return static_cast<const T*>(InElement);
			}
		}
		return nullptr;
	}

	template<typename T>
    friend FORCEINLINE T* Cast(FRigBaseElement* InElement)
	{
		if(InElement)
		{
			if(InElement->IsA<T>())
			{
				return static_cast<T*>(InElement);
			}
		}
		return nullptr;
	}

	template<typename T>
    friend FORCEINLINE const T* CastChecked(const FRigBaseElement* InElement)
	{
		const T* Element = Cast<T>(InElement);
		check(Element);
		return Element;
	}

	template<typename T>
    friend FORCEINLINE T* CastChecked(FRigBaseElement* InElement)
	{
		T* Element = Cast<T>(InElement);
		check(Element);
		return Element;
	}

	virtual void CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights) {}

protected:

	// helper function to be called as part of URigHierarchy::CopyHierarchy
	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy);
	
	// sets up the metadata and ensures the right type
	FRigBaseMetadata* SetupValidMetadata(const FName& InName, ERigMetadataType InType);

	void NotifyMetadataChanged(const FName& InName);
	void NotifyMetadataTagChanged(const FName& InTag, bool bAdded);

	mutable uint16 TopologyVersion;
	mutable uint16 MetadataVersion;
	mutable FRigBaseElementChildrenArray CachedChildren;

	// used for constructing / destructing the memory. typically == 1
	int32 OwnedInstances;

	FRigElementMetadataChangedDelegate MetadataChangedDelegate;
	FRigElementMetadataTagChangedDelegate MetadataTagChangedDelegate;

	friend class URigHierarchy;
	friend class URigHierarchyController;
	friend struct FRigDispatch_SetMetadata;
	friend struct FRigUnit_SetMetadataTag;
	friend struct FRigUnit_SetMetadataTagArray;
	friend struct FRigUnit_RemoveMetadataTag;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigTransformElement : public FRigBaseElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigTransformElement)

	virtual ~FRigTransformElement(){}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = RigElement, meta = (DisplayAfter = "Index"))
	FRigCurrentAndInitialTransform Pose;

protected:

	struct FElementToDirty
	{
		FORCEINLINE FElementToDirty()
			: Element(nullptr)
			, HierarchyDistance(INDEX_NONE)
		{}

		FORCEINLINE FElementToDirty(FRigTransformElement* InElement, int32 InHierarchyDistance = INDEX_NONE)
			: Element(InElement)
			, HierarchyDistance(InHierarchyDistance)
		{}

		FORCEINLINE bool operator ==(const FElementToDirty& Other) const
		{
			return Element == Other.Element;
		}

		FORCEINLINE bool operator !=(const FElementToDirty& Other) const
		{
			return Element != Other.Element;
		}
		
		FRigTransformElement* Element;
		int32 HierarchyDistance;
	};

	//typedef TArray<FElementToDirty> FElementsToDirtyArray;
	typedef TArray<FElementToDirty, TInlineAllocator<3>> FElementsToDirtyArray;  
	FElementsToDirtyArray ElementsToDirty;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Bone ||
			InElement->GetType() == ERigElementType::Null ||
			InElement->GetType() == ERigElementType::Control ||
			InElement->GetType() == ERigElementType::RigidBody ||
			InElement->GetType() == ERigElementType::Reference;
	}

public:
	
	virtual void CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights) override;
	
protected:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

	friend struct FRigBaseElement;
	friend struct FRigSingleParentElement;
	friend struct FRigMultiParentElement;
	friend class URigHierarchy;
	friend class URigHierarchyController;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigSingleParentElement : public FRigTransformElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigSingleParentElement)

	FRigSingleParentElement()
	: FRigTransformElement()
	, ParentElement(nullptr)
	{}

	virtual ~FRigSingleParentElement(){}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

	FRigTransformElement* ParentElement;

protected:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Bone ||
			InElement->GetType() == ERigElementType::RigidBody ||
			InElement->GetType() == ERigElementType::Reference;
	}

	friend struct FRigBaseElement;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigElementWeight
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Weight)
	float Location;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Weight)
	float Rotation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Weight)
	float Scale;

	FORCEINLINE FRigElementWeight()
		: Location(1.f)
		, Rotation(1.f)
		, Scale(1.f)
	{}

	FORCEINLINE FRigElementWeight(float InWeight)
		: Location(InWeight)
		, Rotation(InWeight)
		, Scale(InWeight)
	{}

	FORCEINLINE FRigElementWeight(float InLocation, float InRotation, float InScale)
		: Location(InLocation)
		, Rotation(InRotation)
		, Scale(InScale)
	{}

	FORCEINLINE friend FArchive& operator <<(FArchive& Ar, FRigElementWeight& Weight)
	{
		Ar << Weight.Location;
		Ar << Weight.Rotation;
		Ar << Weight.Scale;
		return Ar;
	}

	FORCEINLINE bool AffectsLocation() const
	{
		return Location > SMALL_NUMBER;
	}

	FORCEINLINE bool AffectsRotation() const
	{
		return Rotation > SMALL_NUMBER;
	}

	FORCEINLINE bool AffectsScale() const
	{
		return Scale > SMALL_NUMBER;
	}

	FORCEINLINE bool IsAlmostZero() const
	{
		return !AffectsLocation() && !AffectsRotation() && !AffectsScale();
	}

	FORCEINLINE friend FRigElementWeight operator *(FRigElementWeight InWeight, float InScale)
	{
		return FRigElementWeight(InWeight.Location * InScale, InWeight.Rotation * InScale, InWeight.Scale * InScale);
	}

	FORCEINLINE friend FRigElementWeight operator *(float InScale, FRigElementWeight InWeight)
	{
		return FRigElementWeight(InWeight.Location * InScale, InWeight.Rotation * InScale, InWeight.Scale * InScale);
	}
};

USTRUCT()
struct CONTROLRIG_API FRigElementParentConstraint
{
public:
	GENERATED_BODY()

	FRigTransformElement* ParentElement;
	FRigElementWeight Weight;
	FRigElementWeight InitialWeight;
	mutable FRigComputedTransform Cache;
		
	FORCEINLINE FRigElementParentConstraint()
		: ParentElement(nullptr)
	{
		Cache.bDirty = true;
	}

	FORCEINLINE const FRigElementWeight& GetWeight(bool bInitial = false) const
	{
		return bInitial ? InitialWeight : Weight;
	}

	FORCEINLINE void CopyPose(const FRigElementParentConstraint& InOther, bool bCurrent, bool bInitial)
	{
		if(bCurrent)
		{
			Weight = InOther.Weight;
		}
		if(bInitial)
		{
			InitialWeight = InOther.InitialWeight;
		}
		Cache.bDirty = true;
	}
};

#if URIGHIERARCHY_ENSURE_CACHE_VALIDITY
typedef TArray<FRigElementParentConstraint, TInlineAllocator<8>> FRigElementParentConstraintArray;
#else
typedef TArray<FRigElementParentConstraint, TInlineAllocator<1>> FRigElementParentConstraintArray;
#endif

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigMultiParentElement : public FRigTransformElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigMultiParentElement)

    FRigMultiParentElement()
    : FRigTransformElement()
	{}

	virtual ~FRigMultiParentElement(){}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	
	FRigElementParentConstraintArray ParentConstraints;
	TMap<FRigElementKey, int32> IndexLookup;

protected:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Null ||
			InElement->GetType() == ERigElementType::Control;
	}

public:
	
	virtual void CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights) override;

protected:
	
	friend struct FRigBaseElement;
};


USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigBoneElement : public FRigSingleParentElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigBoneElement)

	FRigBoneElement()
		: FRigSingleParentElement()
	{
		Key.Type = ERigElementType::Bone;
		BoneType = ERigBoneType::User;
	}
	
	virtual ~FRigBoneElement(){}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = RigElement)
	ERigBoneType BoneType;

protected:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Bone;
	}

	friend struct FRigBaseElement;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigNullElement : public FRigMultiParentElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigNullElement)

	FRigNullElement()
    : FRigMultiParentElement()
	{
		Key.Type = ERigElementType::Null; 
	}

	virtual ~FRigNullElement(){}

protected:
	
	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Null;
	}

	friend struct FRigBaseElement;
};

USTRUCT(BlueprintType)
struct FRigControlElementCustomization
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Customization)
	TArray<FRigElementKey> AvailableSpaces;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Customization)
	TArray<FRigElementKey> RemovedSpaces;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigControlSettings
{
	GENERATED_BODY()

	FRigControlSettings();

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);

	friend uint32 GetTypeHash(const FRigControlSettings& Settings);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	ERigControlAnimationType AnimationType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control, meta=(DisplayName="Value Type"))
	ERigControlType ControlType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	FName DisplayName;

	/** the primary axis to use for float controls */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	ERigControlAxis PrimaryAxis;

	/** If Created from a Curve  Container*/
	UPROPERTY(transient)
	bool bIsCurve;

	/** True if the control has limits. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits)
	TArray<FRigControlLimitEnabled> LimitEnabled;

	/**
	 * True if the limits should be drawn in debug.
	 * For this to be enabled you need to have at least one min and max limit turned on.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits)
	bool bDrawLimits;

	/** The minimum limit of the control's value */
	UPROPERTY(BlueprintReadWrite, Category = Limits)
	FRigControlValue MinimumValue;

	/** The maximum limit of the control's value */
	UPROPERTY(BlueprintReadWrite, Category = Limits)
	FRigControlValue MaximumValue;

	/** Set to true if the shape is currently visible in 3d */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Shape)
	bool bShapeVisible;

	/** Defines how the shape visibility should be changed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Shape)
	ERigControlVisibility ShapeVisibility;

	/* This is optional UI setting - this doesn't mean this is always used, but it is optional for manipulation layer to use this*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Shape)
	FName ShapeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Shape)
	FLinearColor ShapeColor;

	/** If the control is transient and only visible in the control rig editor */
	UPROPERTY(BlueprintReadWrite, Category = Control)
	bool bIsTransientControl;

	/** If the control is 4transient and only visible in the control rig editor */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Control)
	TObjectPtr<UEnum> ControlEnum;

	/**
	 * The User interface customization used for a control
	 * This will be used as the default content for the space picker and other widgets
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Animation, meta = (DisplayName = "Customization"))
	FRigControlElementCustomization Customization;

	/**
	 * The list of driven controls for this proxy control.
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Animation)
	TArray<FRigElementKey> DrivenControls;

	/**
	 * The list of previously driven controls - prior to a procedural change
	 */
	TArray<FRigElementKey> PreviouslyDrivenControls;

	/**
	 * If set to true the animation channel will be grouped with the parent control in sequencer
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Animation)
	bool bGroupWithParentControl;

	/**
	 * Deprecated properties.
	 */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage = "Use animation_type instead."))
	bool bAnimatable_DEPRECATED = true;
	
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage = "Use animation_type or shape_visible instead."))
	bool bShapeEnabled_DEPRECATED = true;
	
	/** Applies the limits expressed by these settings to a value */
	FORCEINLINE void ApplyLimits(FRigControlValue& InOutValue) const
	{
		InOutValue.ApplyLimits(LimitEnabled, ControlType, MinimumValue, MaximumValue);
	}

	/** Applies the limits expressed by these settings to a transform */
	FORCEINLINE void ApplyLimits(FTransform& InOutValue) const
	{
		FRigControlValue Value;
		Value.SetFromTransform(InOutValue, ControlType, PrimaryAxis);
		ApplyLimits(Value);
		InOutValue = Value.GetAsTransform(ControlType, PrimaryAxis);
	}

	FORCEINLINE FRigControlValue GetIdentityValue() const
	{
		FRigControlValue Value;
		Value.SetFromTransform(FTransform::Identity, ControlType, PrimaryAxis);
		return Value;
	}

	bool operator == (const FRigControlSettings& InOther) const;

	FORCEINLINE bool operator != (const FRigControlSettings& InOther) const
	{
		return !(*this == InOther);
	}

	void SetupLimitArrayForType(bool bLimitTranslation = false, bool bLimitRotation = false, bool bLimitScale = false);

	FORCEINLINE bool IsAnimatable() const
	{
		return (AnimationType == ERigControlAnimationType::AnimationControl) ||
			(AnimationType == ERigControlAnimationType::AnimationChannel);
	}

	FORCEINLINE bool ShouldBeGrouped() const
	{
		return IsAnimatable() && bGroupWithParentControl;
	}

	FORCEINLINE bool SupportsShape() const
	{
		return (AnimationType != ERigControlAnimationType::AnimationChannel) &&
			(ControlType != ERigControlType::Bool);
	}

	FORCEINLINE bool IsVisible() const
	{
		return SupportsShape() && bShapeVisible;
	}
	
	FORCEINLINE bool SetVisible(bool bVisible, bool bForce = false)
	{
		if(!bForce)
		{
			if(AnimationType == ERigControlAnimationType::ProxyControl)
			{
				if(ShapeVisibility == ERigControlVisibility::BasedOnSelection)
				{
					return false;
				}
			}
		}
		
		if(SupportsShape())
		{
			if(bShapeVisible == bVisible)
			{
				return false;
			}
			bShapeVisible = bVisible;
		}
		return SupportsShape();
	}

	FORCEINLINE bool IsSelectable(bool bRespectVisibility = true) const
	{
		return (AnimationType == ERigControlAnimationType::AnimationControl ||
			AnimationType == ERigControlAnimationType::ProxyControl) &&
			(IsVisible() || !bRespectVisibility);
	}

	FORCEINLINE void SetAnimationTypeFromDeprecatedData(bool bAnimatable, bool bShapeEnabled)
	{
		if(bAnimatable)
		{
			if(bShapeEnabled && (ControlType != ERigControlType::Bool))
			{
				AnimationType = ERigControlAnimationType::AnimationControl;
			}
			else
			{
				AnimationType = ERigControlAnimationType::AnimationChannel;
			}
		}
		else
		{
			AnimationType = ERigControlAnimationType::ProxyControl;
		}
	}
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigControlElement : public FRigMultiParentElement
{
	public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigControlElement)

	FRigControlElement()
		: FRigMultiParentElement()
	{
		Key.Type = ERigElementType::Control; 
	}

	virtual ~FRigControlElement(){}
	
	FORCEINLINE virtual const FName& GetDisplayName() const override
	{
		if(!Settings.DisplayName.IsNone())
		{
			return Settings.DisplayName;
		}
		return FRigMultiParentElement::GetDisplayName();
	}

	FORCEINLINE bool IsAnimationChannel() const { return Settings.AnimationType == ERigControlAnimationType::AnimationChannel; }

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

private:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

public:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Control)
	FRigControlSettings Settings;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = RigElement)
	FRigCurrentAndInitialTransform Offset;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = RigElement)
	FRigCurrentAndInitialTransform Shape;

protected:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = RigElement)
	FRigPreferredEulerAngles PreferredEulerAngles;
	
	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Control;
	}

public:
	
	virtual void CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights) override;

protected:

	friend struct FRigBaseElement;
	friend class URigHierarchy;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigCurveElement : public FRigBaseElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigCurveElement)

	FRigCurveElement()
		: FRigBaseElement()
		, bIsValueSet(true)
		, Value(0.f)
	{
		Key.Type = ERigElementType::Curve;
	}

	virtual ~FRigCurveElement() override {}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

private:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

public:
	// Set to true if the value was actually set. Used to carry back and forth blend curve
	// value validity state.
	bool bIsValueSet;
	
	float Value;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Curve;
	}

public:
	
	virtual void CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights) override;

protected:
	
	friend struct FRigBaseElement;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigRigidBodySettings
{
	GENERATED_BODY()

	FRigRigidBodySettings();

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	float Mass;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigRigidBodyElement : public FRigSingleParentElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigRigidBodyElement)

    FRigRigidBodyElement()
        : FRigSingleParentElement()
	{
		Key.Type = ERigElementType::RigidBody;
	}
	
	virtual ~FRigRigidBodyElement(){}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

private:

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

public:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Control, meta=(ShowOnlyInnerProperties))
	FRigRigidBodySettings Settings;

protected:
	
	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::RigidBody;
	}

	friend struct FRigBaseElement;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigReferenceElement : public FRigSingleParentElement
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_ELEMENT_METHODS(FRigReferenceElement)

    FRigReferenceElement()
        : FRigSingleParentElement()
	{
		Key.Type = ERigElementType::Reference;
	}
	
	virtual ~FRigReferenceElement(){}

	virtual void Save(FArchive& A, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;
	virtual void Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase) override;

	FTransform GetReferenceWorldTransform(const FRigUnitContext* InContext, bool bInitial) const;

	virtual void CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights) override;

protected:

	FRigReferenceGetWorldTransformDelegate GetWorldTransformDelegate;

	virtual void CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy) override;

	FORCEINLINE static bool IsClassOf(const FRigBaseElement* InElement)
	{
		return InElement->GetType() == ERigElementType::Reference;
	}

	friend struct FRigBaseElement;
	friend class URigHierarchyController;
};

USTRUCT()
struct CONTROLRIG_API FRigHierarchyCopyPasteContentPerElement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FRigElementKey Key;

	UPROPERTY()
	FString Content;

	UPROPERTY()
	TArray<FRigElementKey> Parents;

	UPROPERTY()
	TArray<FRigElementWeight> ParentWeights;

	UPROPERTY()
	FRigCurrentAndInitialTransform Pose;
};

USTRUCT()
struct CONTROLRIG_API FRigHierarchyCopyPasteContent
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FRigHierarchyCopyPasteContentPerElement> Elements;

	// Maintain properties below for backwards compatibility pre-5.0
	UPROPERTY()
	TArray<ERigElementType> Types;

	UPROPERTY()
	TArray<FString> Contents;

	UPROPERTY()
	TArray<FTransform> LocalTransforms;

	UPROPERTY()
	TArray<FTransform> GlobalTransforms;
};