// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Units/RigUnit.h"
#include "Units/RigDispatchFactory.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "RigUnit_AnimAttribute.generated.h"

namespace RigUnit_AnimAttribute
{
	template<typename T>
	class TAnimAttributeType;

	template<>
	class TAnimAttributeType<int>
	{
	public:
		using Type = FIntegerAnimationAttribute;
	};

	template<>
	class TAnimAttributeType<float>
	{
	public:
		using Type = FFloatAnimationAttribute;
	};

	template<>
	class TAnimAttributeType<FString>
	{
	public:
		using Type = FStringAnimationAttribute;
	};

	template<>
	class TAnimAttributeType<FTransform>
	{
	public:
		using Type = FTransformAnimationAttribute;
	};

	template<>
	class TAnimAttributeType<FVector>
	{
	public:
		using Type = FVectorAnimationAttribute;
	};

	template<>
	class TAnimAttributeType<FQuat>
	{
	public:
		using Type = FQuaternionAnimationAttribute;
	};


	template<typename T>
	T* GetAnimAttributeValue(
		bool bAddIfNotFound,
		const FRigUnitContext& Context,
		const FName& Name,
		const FName& BoneName,
		FName& CachedBoneName,
		int32& CachedBoneIndex)
	{
		if (Name.IsNone())
		{
			return nullptr;
		}

		if (!Context.AnimAttributeContainer)
		{
			return nullptr;
		}
	
		const USkeletalMeshComponent* OwningComponent = Cast<USkeletalMeshComponent>(Context.OwningComponent);

		if (!OwningComponent ||
			!OwningComponent->GetSkeletalMeshAsset())
		{
			return nullptr;
		}

		if (BoneName == NAME_None)
		{
			// default to use root bone
			CachedBoneIndex = 0;
		}
		else
		{
			// Invalidate cache if input changed
			if (CachedBoneName != BoneName)
			{
				CachedBoneIndex = OwningComponent->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(BoneName);
			}
		}
	
		CachedBoneName = BoneName;

		if (CachedBoneIndex != INDEX_NONE)
		{
			const UE::Anim::FAttributeId Id = {Name, FCompactPoseBoneIndex(CachedBoneIndex)} ;
			typename TAnimAttributeType<T>::Type* Attribute = bAddIfNotFound ?
				Context.AnimAttributeContainer->FindOrAdd<typename TAnimAttributeType<T>::Type>(Id) :
				Context.AnimAttributeContainer->Find<typename TAnimAttributeType<T>::Type>(Id);
			if (Attribute)
			{
				return &Attribute->Value;
			}
		}

		return nullptr;
	}
}




/**
 * Animation Attributes allow dynamically added data to flow from
 * one Anim Node to other Anim Nodes downstream in the Anim Graph
 * and accessible from deformer graph
 */
USTRUCT(meta=(Abstract, Category="Animation Attribute", NodeColor="0.462745, 1,0, 0.329412", Varying))
struct CONTROLRIG_API FRigDispatch_AnimAttributeBase : public FRigDispatchFactory
{
	GENERATED_BODY()

	virtual void RegisterDependencyTypes() const override;

#if WITH_EDITOR
	virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const override;;
#endif
	
	virtual TArray<FRigVMTemplateArgument> GetArguments() const override;
	virtual bool IsSet() const { return false; }

#if WITH_EDITOR
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#endif

protected:
	static bool IsTypeSupported(const TRigVMTypeIndex& InTypeIndex);
	static const TArray<FRigVMTemplateArgument::ETypeCategory>& GetValueTypeCategory();
	
	mutable TArray<FRigVMTemplateArgument> Arguments;

	// IO
	mutable int32 ExecuteArgIndex = INDEX_NONE;
	
	// input
	mutable int32 NameArgIndex = INDEX_NONE;
	mutable int32 BoneNameArgIndex = INDEX_NONE;
	mutable int32 DefaultArgIndex = INDEX_NONE;


	// output
	mutable int32 ValueArgIndex = INDEX_NONE;
	mutable int32 FoundArgIndex = INDEX_NONE;
	mutable int32 SuccessArgIndex = INDEX_NONE;

	// hidden
	mutable int32 CachedBoneNameArgIndex = INDEX_NONE;
	mutable int32 CachedBoneIndexArgIndex = INDEX_NONE;
	
	static FName NameArgName;
	static FName BoneNameArgName;
	static FName CachedBoneNameArgName;
	static FName CachedBoneIndexArgName;
	static FName DefaultArgName;
	static FName ValueArgName;
	static FName FoundArgName;
	static FName SuccessArgName;

};


/*
 * Get the value of an animation attribute from the skeletal mesh
 */
USTRUCT(meta=(DisplayName="Get Animation Attribute"))
struct CONTROLRIG_API FRigDispatch_GetAnimAttribute: public FRigDispatch_AnimAttributeBase
{
	GENERATED_BODY()

	virtual TArray<FRigVMTemplateArgument> GetArguments() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;

	
protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override;

#if WITH_EDITOR
	template<typename ValueType>
	FORCEINLINE_DEBUGGABLE bool CheckArgumentTypes(FRigVMMemoryHandleArray Handles) const
	{
		return CheckArgumentType(Handles[NameArgIndex].IsType<FName>(), NameArgName) &&
			CheckArgumentType(Handles[BoneNameArgIndex].IsType<FName>(), BoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneNameArgIndex].IsType<FName>(true), CachedBoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneIndexArgIndex].IsType<int32>(true), CachedBoneIndexArgName) &&
			CheckArgumentType(Handles[DefaultArgIndex].IsType<ValueType>(), DefaultArgName) &&
			CheckArgumentType(Handles[ValueArgIndex].IsType<ValueType>(), ValueArgName) &&
			CheckArgumentType(Handles[FoundArgIndex].IsType<bool>(), FoundArgName);
	}

	FORCEINLINE_DEBUGGABLE bool CheckArgumentTypes(FRigVMMemoryHandleArray Handles) const
	{
		return CheckArgumentType(Handles[NameArgIndex].IsType<FName>(), NameArgName) &&
			CheckArgumentType(Handles[BoneNameArgIndex].IsType<FName>(), BoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneNameArgIndex].IsType<FName>(true), CachedBoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneIndexArgIndex].IsType<int32>(true), CachedBoneIndexArgName) &&
			CheckArgumentType(Handles[FoundArgIndex].IsType<bool>(), FoundArgName);
	}
#endif

	// dispatch function for built-in types
	template<typename ValueType>	
	FORCEINLINE_DEBUGGABLE static void GetAnimAttributeDispatch(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
	{
		const FRigDispatch_GetAnimAttribute* Factory = static_cast<const FRigDispatch_GetAnimAttribute*>(InContext.Factory);
		
#if WITH_EDITOR
		if(!Factory->CheckArgumentTypes<ValueType>(Handles))
		{
			return;
		}
#endif

		// unpack the memory
		const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
		const FName& BoneName = *(const FName*)Handles[Factory->BoneNameArgIndex].GetData();
		const ValueType& Default = *(const ValueType*)Handles[Factory->DefaultArgIndex].GetData();
		
		ValueType& Value = *(ValueType*)Handles[Factory->ValueArgIndex].GetData();
		bool& Found = *(bool*)Handles[Factory->FoundArgIndex].GetData();
		
		FName& CachedBoneName = *(FName*)Handles[Factory->CachedBoneNameArgIndex].GetData(false, InContext.GetSlice().GetIndex());
		int32& CachedBoneIndex = *(int32*)Handles[Factory->CachedBoneIndexArgIndex].GetData(false, InContext.GetSlice().GetIndex());

		// extract the animation attribute
		const FRigUnitContext& Context = GetRigUnitContext(InContext);
		const ValueType* ValuePtr = RigUnit_AnimAttribute::GetAnimAttributeValue<ValueType>(false, Context, Name, BoneName, CachedBoneName, CachedBoneIndex);
		Found = ValuePtr ? true : false;
		Value = ValuePtr ? *ValuePtr : Default;
	}

	// dispatch function for user/dev defined types
	static void GetAnimAttributeDispatch(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles);
	
};

/*
 * Modify an animation attribute if one is found, otherwise add a new animation attribute
 */
USTRUCT(meta=(DisplayName="Set Animation Attribute"))
struct CONTROLRIG_API FRigDispatch_SetAnimAttribute: public FRigDispatch_AnimAttributeBase
{
	GENERATED_BODY()
	virtual bool IsSet() const override { return true; }
	virtual TArray<FRigVMTemplateArgument> GetArguments() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
	
protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override;

#if WITH_EDITOR
	template<typename ValueType>
	FORCEINLINE_DEBUGGABLE bool CheckArgumentTypes(FRigVMMemoryHandleArray Handles) const
	{
		return CheckArgumentType(Handles[NameArgIndex].IsType<FName>(), NameArgName) &&
			CheckArgumentType(Handles[BoneNameArgIndex].IsType<FName>(), BoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneNameArgIndex].IsType<FName>(true), CachedBoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneIndexArgIndex].IsType<int32>(true), CachedBoneIndexArgName) &&
			CheckArgumentType(Handles[ValueArgIndex].IsType<ValueType>(), ValueArgName) &&
			CheckArgumentType(Handles[SuccessArgIndex].IsType<bool>(), FoundArgName);
	}

	FORCEINLINE_DEBUGGABLE bool CheckArgumentTypes(FRigVMMemoryHandleArray Handles) const
	{
		return CheckArgumentType(Handles[NameArgIndex].IsType<FName>(), NameArgName) &&
			CheckArgumentType(Handles[BoneNameArgIndex].IsType<FName>(), BoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneNameArgIndex].IsType<FName>(true), CachedBoneNameArgName) &&
			CheckArgumentType(Handles[CachedBoneIndexArgIndex].IsType<int32>(true), CachedBoneIndexArgName) &&
			CheckArgumentType(Handles[SuccessArgIndex].IsType<bool>(), FoundArgName);
	}
#endif

	// dispatch function for built-in types
	template<typename ValueType>	
	FORCEINLINE_DEBUGGABLE static void SetAnimAttributeDispatch(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
	{
		const FRigDispatch_SetAnimAttribute* Factory = static_cast<const FRigDispatch_SetAnimAttribute*>(InContext.Factory);
		
#if WITH_EDITOR
		if(!Factory->CheckArgumentTypes<ValueType>(Handles))
		{
			return;
		}
#endif

		// unpack the memory
		const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
		const FName& BoneName = *(const FName*)Handles[Factory->BoneNameArgIndex].GetData();
		const ValueType& Value = *(ValueType*)Handles[Factory->ValueArgIndex].GetData();
		
		bool& Success = *(bool*)Handles[Factory->SuccessArgIndex].GetData();
		Success = false;
		
		FName& CachedBoneName = *(FName*)Handles[Factory->CachedBoneNameArgIndex].GetData(false, InContext.GetSlice().GetIndex());
		int32& CachedBoneIndex = *(int32*)Handles[Factory->CachedBoneIndexArgIndex].GetData(false, InContext.GetSlice().GetIndex());

		// extract the animation attribute
		const FRigUnitContext& Context = GetRigUnitContext(InContext);
		ValueType* ValuePtr = RigUnit_AnimAttribute::GetAnimAttributeValue<ValueType>(true, Context, Name, BoneName, CachedBoneName, CachedBoneIndex);
		if (ValuePtr)
		{
			Success = true;
			*ValuePtr = Value;
		}
	}

	// dispatch function for user/dev defined types
	static void SetAnimAttributeDispatch(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles);
};

