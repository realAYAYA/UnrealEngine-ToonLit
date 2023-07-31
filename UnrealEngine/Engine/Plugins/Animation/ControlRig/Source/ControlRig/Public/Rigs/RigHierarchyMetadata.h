// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyMetadata.generated.h"

struct FRigBaseElement;
class URigHierarchyController;

#define DECLARE_RIG_METADATA_METHODS(MetadataType) \
template<typename T> \
friend FORCEINLINE const T* Cast(const MetadataType* InMetadata) \
{ \
   return Cast<T>((const FRigBaseMetadata*) InMetadata); \
} \
template<typename T> \
friend FORCEINLINE T* Cast(MetadataType* InMetadata) \
{ \
   return Cast<T>((FRigBaseMetadata*) InMetadata); \
} \
template<typename T> \
friend FORCEINLINE const T* CastChecked(const MetadataType* InMetadata) \
{ \
	return CastChecked<T>((const FRigBaseMetadata*) InMetadata); \
} \
template<typename T> \
friend FORCEINLINE T* CastChecked(MetadataType* InMetadata) \
{ \
	return CastChecked<T>((FRigBaseMetadata*) InMetadata); \
}

USTRUCT()
struct CONTROLRIG_API FRigBaseMetadata
{
	GENERATED_BODY()

public:

	FRigBaseMetadata()
    	: Element(nullptr)
		, Name(NAME_None)
		, Type(ERigMetadataType::Invalid)
		, ValueProperty(nullptr)
	{}

	virtual ~FRigBaseMetadata(){}

protected:

	const FRigBaseElement* Element;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	ERigMetadataType Type;

	mutable const FProperty* ValueProperty;

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return true;
	}

public:

	UScriptStruct* GetMetadataStruct() const;
	virtual void Serialize(FArchive& Ar, bool bIsLoading);

	FORCEINLINE bool IsValid() const { return (GetType() != ERigMetadataType::Invalid) && GetKey().IsValid(); }
	FORCEINLINE const FRigBaseElement* GetElement() const { return Element; }
	const FRigElementKey& GetKey() const;
	FORCEINLINE const FName& GetName() const { return Name; }
	FORCEINLINE const ERigMetadataType& GetType() const { return Type; }
	FORCEINLINE bool IsArray() const
	{
		return GetValueProperty()->IsA<FArrayProperty>();
	}
	FORCEINLINE const void* GetValueData() const
	{
		return GetValueProperty()->ContainerPtrToValuePtr<void>(this);
	}
	FORCEINLINE const int32& GetValueSize() const
	{
		return GetValueProperty()->ElementSize;
	}
	FORCEINLINE bool SetValueData(const void* InData, int32 InSize)
	{
		if(InData)
		{
			if(InSize == GetValueSize())
			{
				return SetValueDataImpl(InData);
			}
		}
		return false;
	}

	template<typename T>
	FORCEINLINE bool IsA() const { return T::IsClassOf(this); }

	template<typename T>
    friend FORCEINLINE const T* Cast(const FRigBaseMetadata* InMetadata)
	{
		if(InMetadata)
		{
			if(InMetadata->IsA<T>())
			{
				return static_cast<const T*>(InMetadata);
			}
		}
		return nullptr;
	}

	template<typename T>
    friend FORCEINLINE T* Cast(FRigBaseMetadata* InMetadata)
	{
		if(InMetadata)
		{
			if(InMetadata->IsA<T>())
			{
				return static_cast<T*>(InMetadata);
			}
		}
		return nullptr;
	}

	template<typename T>
    friend FORCEINLINE const T* CastChecked(const FRigBaseMetadata* InMetadata)
	{
		const T* CastElement = Cast<T>(InMetadata);
		check(CastElement);
		return CastElement;
	}

	template<typename T>
    friend FORCEINLINE T* CastChecked(FRigBaseMetadata* InMetadata)
	{
		T* CastElement = Cast<T>(InMetadata);
		check(CastElement);
		return CastElement;
	}

protected:

	static UScriptStruct* GetMetadataStruct(const ERigMetadataType& InType);
	static FRigBaseMetadata* MakeMetadata(const FRigBaseElement* InElement, const FName& InName, ERigMetadataType InType);
	static void DestroyMetadata(FRigBaseMetadata** Metadata);

	FORCEINLINE const FProperty* GetValueProperty() const
	{
		if(ValueProperty == nullptr)
		{
			static constexpr TCHAR ValueName[] = TEXT("Value");
			ValueProperty = GetMetadataStruct()->FindPropertyByName(ValueName);
		}
		return ValueProperty;
	}

	FORCEINLINE void* GetValueData()
	{
		return GetValueProperty()->ContainerPtrToValuePtr<void>(this);
	}

	FORCEINLINE virtual bool SetValueDataImpl(const void* InData)
	{
		const FProperty* Property = GetValueProperty();
		if(!Property->Identical(GetValueData(), InData))
		{
			Property->CopyCompleteValue(GetValueData(), InData);
			return true;
		}
		return false;
	}

	friend struct FRigBaseElement;
	friend class URigHierarchy;
	friend class URigHierarchyController;
};

USTRUCT()
struct CONTROLRIG_API FRigBoolMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigBoolMetadata)

	FRigBoolMetadata()
		: FRigBaseMetadata()
		, Value(false)
	{}
	virtual ~FRigBoolMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const bool& GetValue() const { return Value; }
	FORCEINLINE bool& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const bool& InValue) { return SetValueData(&InValue, sizeof(bool)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::Bool;
	}

protected:

	UPROPERTY()
	bool Value;
};

USTRUCT()
struct CONTROLRIG_API FRigBoolArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigBoolArrayMetadata)

	FRigBoolArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigBoolArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<bool>& GetValue() const { return Value; }
	FORCEINLINE TArray<bool>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<bool>& InValue) { return SetValueData(&InValue, sizeof(TArray<bool>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::BoolArray;
	}

protected:

	UPROPERTY()
	TArray<bool> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigFloatMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigFloatMetadata)

	FRigFloatMetadata()
		: FRigBaseMetadata()
		, Value(0.f)
	{}
	virtual ~FRigFloatMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const float& GetValue() const { return Value; } 
	FORCEINLINE float& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const float& InValue) { return SetValueData(&InValue, sizeof(float)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Float;
    }

protected:

	UPROPERTY()
	float Value;
};

USTRUCT()
struct CONTROLRIG_API FRigFloatArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigFloatArrayMetadata)

	FRigFloatArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigFloatArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<float>& GetValue() const { return Value; }
	FORCEINLINE TArray<float>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<float>& InValue) { return SetValueData(&InValue, sizeof(TArray<float>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::FloatArray;
	}

protected:

	UPROPERTY()
	TArray<float> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigInt32Metadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigInt32Metadata)

	FRigInt32Metadata()
		: FRigBaseMetadata()
		, Value(0)
	{}
	virtual ~FRigInt32Metadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const int32& GetValue() const { return Value; } 
	FORCEINLINE int32& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const int32& InValue) { return SetValueData(&InValue, sizeof(int32)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Int32;
    }

protected:

	UPROPERTY()
	int32 Value;
};

USTRUCT()
struct CONTROLRIG_API FRigInt32ArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigInt32ArrayMetadata)

	FRigInt32ArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigInt32ArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<int32>& GetValue() const { return Value; }
	FORCEINLINE TArray<int32>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<int32>& InValue) { return SetValueData(&InValue, sizeof(TArray<int32>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::Int32Array;
	}

protected:

	UPROPERTY()
	TArray<int32> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigNameMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigNameMetadata)

	FRigNameMetadata()
		: FRigBaseMetadata()
		, Value(NAME_None)
	{}
	virtual ~FRigNameMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FName& GetValue() const { return Value; } 
	FORCEINLINE FName& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FName& InValue) { return SetValueData(&InValue, sizeof(FName)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Name;
    }

protected:

	UPROPERTY()
	FName Value;
};

USTRUCT()
struct CONTROLRIG_API FRigNameArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigNameArrayMetadata)

	FRigNameArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigNameArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FName>& GetValue() const { return Value; }
	FORCEINLINE TArray<FName>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FName>& InValue) { return SetValueData(&InValue, sizeof(TArray<FName>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::NameArray;
	}

protected:

	UPROPERTY()
	TArray<FName> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigVectorMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigVectorMetadata)

	FRigVectorMetadata()
		: FRigBaseMetadata()
		, Value(FVector::ZeroVector)
	{}
	virtual ~FRigVectorMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FVector& GetValue() const { return Value; } 
	FORCEINLINE FVector& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FVector& InValue) { return SetValueData(&InValue, sizeof(FVector)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Vector;
    }

protected:

	UPROPERTY()
	FVector Value;
};

USTRUCT()
struct CONTROLRIG_API FRigVectorArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigVectorArrayMetadata)

	FRigVectorArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigVectorArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FVector>& GetValue() const { return Value; }
	FORCEINLINE TArray<FVector>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FVector>& InValue) { return SetValueData(&InValue, sizeof(TArray<FVector>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::VectorArray;
	}

protected:

	UPROPERTY()
	TArray<FVector> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigRotatorMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigRotatorMetadata)

	FRigRotatorMetadata()
		: FRigBaseMetadata()
		, Value(FRotator::ZeroRotator)
	{}
	virtual ~FRigRotatorMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FRotator& GetValue() const { return Value; } 
	FORCEINLINE FRotator& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FRotator& InValue) { return SetValueData(&InValue, sizeof(FRotator)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Rotator;
    }

protected:

	UPROPERTY()
	FRotator Value;
};

USTRUCT()
struct CONTROLRIG_API FRigRotatorArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigRotatorArrayMetadata)

	FRigRotatorArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigRotatorArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FRotator>& GetValue() const { return Value; }
	FORCEINLINE TArray<FRotator>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FRotator>& InValue) { return SetValueData(&InValue, sizeof(TArray<FRotator>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::RotatorArray;
	}

protected:

	UPROPERTY()
	TArray<FRotator> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigQuatMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigQuatMetadata)

	FRigQuatMetadata()
		: FRigBaseMetadata()
		, Value(FQuat::Identity)
	{}
	virtual ~FRigQuatMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FQuat& GetValue() const { return Value; } 
	FORCEINLINE FQuat& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FQuat& InValue) { return SetValueData(&InValue, sizeof(FQuat)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Quat;
    }

protected:

	UPROPERTY()
	FQuat Value;
};

USTRUCT()
struct CONTROLRIG_API FRigQuatArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigQuatArrayMetadata)

	FRigQuatArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigQuatArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FQuat>& GetValue() const { return Value; }
	FORCEINLINE TArray<FQuat>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FQuat>& InValue) { return SetValueData(&InValue, sizeof(TArray<FQuat>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::QuatArray;
	}

protected:

	UPROPERTY()
	TArray<FQuat> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigTransformMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigTransformMetadata)

	FRigTransformMetadata()
		: FRigBaseMetadata()
		, Value(FTransform::Identity)
	{}
	virtual ~FRigTransformMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FTransform& GetValue() const { return Value; } 
	FORCEINLINE FTransform& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FTransform& InValue) { return SetValueData(&InValue, sizeof(FTransform)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::Transform;
    }

protected:

	UPROPERTY()
	FTransform Value;
};

USTRUCT()
struct CONTROLRIG_API FRigTransformArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigTransformArrayMetadata)

	FRigTransformArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigTransformArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FTransform>& GetValue() const { return Value; }
	FORCEINLINE TArray<FTransform>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FTransform>& InValue) { return SetValueData(&InValue, sizeof(TArray<FTransform>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::TransformArray;
	}

protected:

	UPROPERTY()
	TArray<FTransform> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigLinearColorMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigLinearColorMetadata)

	FRigLinearColorMetadata()
		: FRigBaseMetadata()
		, Value(FLinearColor::White)
	{}
	virtual ~FRigLinearColorMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FLinearColor& GetValue() const { return Value; } 
	FORCEINLINE FLinearColor& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FLinearColor& InValue) { return SetValueData(&InValue, sizeof(FLinearColor)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::LinearColor;
	}

protected:

	UPROPERTY()
	FLinearColor Value;
};

USTRUCT()
struct CONTROLRIG_API FRigLinearColorArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigLinearColorArrayMetadata)

	FRigLinearColorArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigLinearColorArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FLinearColor>& GetValue() const { return Value; }
	FORCEINLINE TArray<FLinearColor>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FLinearColor>& InValue) { return SetValueData(&InValue, sizeof(TArray<FLinearColor>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::LinearColorArray;
	}

protected:

	UPROPERTY()
	TArray<FLinearColor> Value;
};

USTRUCT()
struct CONTROLRIG_API FRigElementKeyMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigElementKeyMetadata)

	FRigElementKeyMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigElementKeyMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const FRigElementKey& GetValue() const { return Value; } 
	FORCEINLINE FRigElementKey& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const FRigElementKey& InValue) { return SetValueData(&InValue, sizeof(FRigElementKey)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
    {
    	return InMetadata->GetType() == ERigMetadataType::RigElementKey;
    }

protected:

	UPROPERTY()
	FRigElementKey Value;
};

USTRUCT()
struct CONTROLRIG_API FRigElementKeyArrayMetadata : public FRigBaseMetadata
{
public:
	
	GENERATED_BODY()
	DECLARE_RIG_METADATA_METHODS(FRigElementKeyArrayMetadata)

	FRigElementKeyArrayMetadata()
		: FRigBaseMetadata()
	{}
	virtual ~FRigElementKeyArrayMetadata() override {}
	FORCEINLINE virtual void Serialize(FArchive& Ar, bool bIsLoading) override
	{
		Super::Serialize(Ar, bIsLoading);
		Ar << Value;
	}
	FORCEINLINE const TArray<FRigElementKey>& GetValue() const { return Value; }
	FORCEINLINE TArray<FRigElementKey>& GetValue() { return Value; }
	FORCEINLINE bool SetValue(const TArray<FRigElementKey>& InValue) { return SetValueData(&InValue, sizeof(TArray<FRigElementKey>)); }

	FORCEINLINE static bool IsClassOf(const FRigBaseMetadata* InMetadata)
	{
		return InMetadata->GetType() == ERigMetadataType::RigElementKeyArray;
	}

protected:

	UPROPERTY()
	TArray<FRigElementKey> Value;
};
