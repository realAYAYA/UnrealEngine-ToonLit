// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Animation/RigUnit_AnimAttribute.h"

#include "Animation/AttributeTypes.h"

#include "Units/RigUnitContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/UserDefinedStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimAttribute)

FName FRigDispatch_AnimAttributeBase::NameArgName = TEXT("Name");
FName FRigDispatch_AnimAttributeBase::BoneNameArgName = TEXT("BoneName");
FName FRigDispatch_AnimAttributeBase::CachedBoneNameArgName = TEXT("CachedBoneName");
FName FRigDispatch_AnimAttributeBase::CachedBoneIndexArgName = TEXT("CachedBoneIndex");
FName FRigDispatch_AnimAttributeBase::DefaultArgName = TEXT("Default");
FName FRigDispatch_AnimAttributeBase::ValueArgName = TEXT("Value");
FName FRigDispatch_AnimAttributeBase::FoundArgName = TEXT("Found");
FName FRigDispatch_AnimAttributeBase::SuccessArgName = TEXT("Success");


bool FRigDispatch_AnimAttributeBase::IsTypeSupported(const TRigVMTypeIndex& InTypeIndex)
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();

	static const TArray<TRigVMTypeIndex> SpecialTypes = {
		RigVMTypeUtils::TypeIndex::Float,
		RigVMTypeUtils::TypeIndex::Int32,
		RigVMTypeUtils::TypeIndex::FString,
		Registry.GetTypeIndex<FTransform>(false),
		Registry.GetTypeIndex<FVector>(false),
		Registry.GetTypeIndex<FQuat>(false),
	};

	if (SpecialTypes.Contains(InTypeIndex))
	{
		return true;
	}

	const FRigVMTemplateArgumentType& InType = Registry.GetType(InTypeIndex);

	// cpp type object can become invalid because users can choose to delete
	// user defined structs
	if (IsValid(InType.CPPTypeObject))
	{
		if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InType.CPPTypeObject))
		{
			static const TArray<TWeakObjectPtr<const UScriptStruct>> SpecialAttributeTypes = {
				FFloatAnimationAttribute::StaticStruct(),
				FIntegerAnimationAttribute::StaticStruct(),
				FStringAnimationAttribute::StaticStruct(),
				FTransformAnimationAttribute::StaticStruct(),
				FVectorAnimationAttribute::StaticStruct(),
				FQuaternionAnimationAttribute::StaticStruct()
			};

			if (SpecialAttributeTypes.Contains(ScriptStruct))
			{
				// these type have been added, above, rejecting them here so we don't have duplicated types
				return false;
			}

			if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(ScriptStruct))
			{
				// allow all user defined structs because even if a struct is not registered with anim attribute system,
				// it could be added to or removed from the system easily. allowing all of them as valid permutations
				// avoids having to create orphan pins.
				return true;
			}
			
			return UE::Anim::AttributeTypes::IsTypeRegistered(ScriptStruct);
		}
	}

	return false;
}


const TArray<FRigVMTemplateArgument::ETypeCategory>& FRigDispatch_AnimAttributeBase::GetValueTypeCategory()
{
	static const TArray<FRigVMTemplateArgument::ETypeCategory> TypeCategories = {
		FRigVMTemplateArgument::ETypeCategory_SingleSimpleValue,
		FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue,
		FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue
	};
	return TypeCategories;
}


static uint8* GetAnimAttributeValue(
	bool bAddIfNotFound,
	UScriptStruct* InAttributeScriptStruct,
	const FRigUnitContext& Context,
	const FName& Name,
	const FName& BoneName,
	FName& CachedBoneName,
	int32& CachedBoneIndex)
{
	if (!ensure(InAttributeScriptStruct))
	{
		return nullptr;
	}
	
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
		uint8* Attribute = bAddIfNotFound ?
			Context.AnimAttributeContainer->FindOrAdd(InAttributeScriptStruct, Id) :
			Context.AnimAttributeContainer->Find(InAttributeScriptStruct, Id);
		
		if (Attribute)
		{
			return Attribute;
		}
	}

	return nullptr;
}

void FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch(FRigVMExtendedExecuteContext& InContext,
	FRigVMMemoryHandleArray Handles)
{
	const FRigDispatch_GetAnimAttribute* Factory = static_cast<const FRigDispatch_GetAnimAttribute*>(InContext.Factory);
		
#if WITH_EDITOR
	if(!Factory->CheckArgumentTypes(Handles))
	{
		return;
	}
#endif

	// unpack the memory
	const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
	const FName& BoneName = *(const FName*)Handles[Factory->BoneNameArgIndex].GetData();
	const uint8* Default = (const uint8*)Handles[Factory->DefaultArgIndex].GetData();
	
	uint8* Value = (uint8*)Handles[Factory->ValueArgIndex].GetData();
	bool& Found = *(bool*)Handles[Factory->FoundArgIndex].GetData();
	Found = false;
	
	FName& CachedBoneName = *(FName*)Handles[Factory->CachedBoneNameArgIndex].GetData(false, InContext.GetSlice().GetIndex());
	int32& CachedBoneIndex = *(int32*)Handles[Factory->CachedBoneIndexArgIndex].GetData(false, InContext.GetSlice().GetIndex());
	
	if (const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Handles[Factory->ValueArgIndex].GetResolvedProperty()))
	{
		UScriptStruct* ScriptStruct = StructProperty->Struct;
		bool Registered = true;
		const FRigUnitContext& Context = GetRigUnitContext(InContext);

#if WITH_EDITOR
		{
			FRigVMExecuteContext& RigVMExecuteContext = InContext.PublicData;
			if (!UE::Anim::AttributeTypes::IsTypeRegistered(ScriptStruct))
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(
					TEXT("Type: '%s' is not registered with the Animation Attribute System. "
						"Please register the type in Project Settings - Animation - CustomAttributes - User Defined Struct Attributes."),
					*ScriptStruct->GetAuthoredName());
				Registered = false;
			}
		}
#endif
		
		if (Registered)
		{
			// extract the animation attribute
			const uint8* ValuePtr = GetAnimAttributeValue(false, ScriptStruct, Context, Name, BoneName, CachedBoneName, CachedBoneIndex);
			Found = ValuePtr ? true : false;

			if (Found)
			{
				ScriptStruct->CopyScriptStruct(Value, ValuePtr);		
			}
		}

		if (!Found)
		{
			ScriptStruct->CopyScriptStruct(Value, Default);		
		}
	}
}

void FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch(FRigVMExtendedExecuteContext& InContext,
	FRigVMMemoryHandleArray Handles)
{
	const FRigDispatch_SetAnimAttribute* Factory = static_cast<const FRigDispatch_SetAnimAttribute*>(InContext.Factory);
		
#if WITH_EDITOR
	if(!Factory->CheckArgumentTypes(Handles))
	{
		return;
	}
#endif

	// unpack the memory
	const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
	const FName& BoneName = *(const FName*)Handles[Factory->BoneNameArgIndex].GetData();
	const uint8* Value = (uint8*)Handles[Factory->ValueArgIndex].GetData();
	
	bool& Success = *(bool*)Handles[Factory->SuccessArgIndex].GetData();
	Success = false;
	
	FName& CachedBoneName = *(FName*)Handles[Factory->CachedBoneNameArgIndex].GetData(false, InContext.GetSlice().GetIndex());
	int32& CachedBoneIndex = *(int32*)Handles[Factory->CachedBoneIndexArgIndex].GetData(false, InContext.GetSlice().GetIndex());
	
	if (const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Handles[Factory->ValueArgIndex].GetResolvedProperty()))
	{
		UScriptStruct* ScriptStruct = StructProperty->Struct;
		bool Registered = true;
		const FRigUnitContext& Context = GetRigUnitContext(InContext);

#if WITH_EDITOR
		{
			FRigVMExecuteContext& RigVMExecuteContext = InContext.PublicData;
			if (!UE::Anim::AttributeTypes::IsTypeRegistered(ScriptStruct))
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(
					TEXT("Type: '%s' is not registered in the Animation Attribute System. "
						"Please register the type in Project Settings - Animation - CustomAttributes - User Defined Struct Animation Attributes."),
					*ScriptStruct->GetAuthoredName());
				Registered = false;
			}
		}
#endif
		
		if (Registered)
		{
			// extract the animation attribute
			uint8* ValuePtr = GetAnimAttributeValue(true, ScriptStruct, Context, Name, BoneName, CachedBoneName, CachedBoneIndex);

			if (ValuePtr)
			{
				Success = true;
				ScriptStruct->CopyScriptStruct(ValuePtr, Value);		
			}
		}
	}	
}

void FRigDispatch_AnimAttributeBase::RegisterDependencyTypes() const
{
	Super::RegisterDependencyTypes();

	FRigVMRegistry& Registry = FRigVMRegistry::Get();
	
	TArray<TWeakObjectPtr<const UScriptStruct>> AttributeTypes = UE::Anim::AttributeTypes::GetRegisteredTypes();
	for (TWeakObjectPtr<const UScriptStruct> Type : AttributeTypes)
	{
		UScriptStruct* TypePtr = const_cast<UScriptStruct*>(Type.Get());
		if (TypePtr)
		{
			Registry.FindOrAddType(FRigVMTemplateArgumentType(TypePtr));
		}
	}
}

#if WITH_EDITOR
FString FRigDispatch_AnimAttributeBase::GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const
{
	if(const TRigVMTypeIndex* ValueTypeIndexPtr = InTypes.Find(ValueArgName))
	{
		const TRigVMTypeIndex& ValueTypeIndex = *ValueTypeIndexPtr;
		if(ValueTypeIndex != RigVMTypeUtils::TypeIndex::WildCard &&
			ValueTypeIndex != RigVMTypeUtils::TypeIndex::WildCardArray)
		{
			static constexpr TCHAR GetFormat[] = TEXT("Get %s Animation Attribute");
			static constexpr TCHAR SetFormat[] = TEXT("Set %s Animation Attribute");

			const FRigVMTemplateArgumentType& ValueType = FRigVMRegistry::Get().GetType(ValueTypeIndex);
			FString ValueName;

			if(ValueType.CPPTypeObject)
			{
				ValueName = ValueType.CPPTypeObject->GetName();
			}
			else
			{
				ValueName = ValueType.GetBaseCPPType();
				ValueName = ValueName.Left(1).ToUpper() + ValueName.Mid(1);
			}

			return FString::Printf(IsSet() ? SetFormat : GetFormat, *ValueName); 
		}
	}
	return FRigDispatchFactory::GetNodeTitle(InTypes);	
}
#endif

TArray<FRigVMTemplateArgument> FRigDispatch_AnimAttributeBase::GetArguments() const
{
	if (Arguments.IsEmpty())
	{
		if (IsSet())
		{
			ExecuteArgIndex = Arguments.Emplace(FRigVMStruct::ExecuteContextName, ERigVMPinDirection::IO, FRigVMRegistry::Get().GetTypeIndex<FControlRigExecuteContext>());
		}

		NameArgIndex = Arguments.Emplace(NameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		BoneNameArgIndex = Arguments.Emplace(BoneNameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);

		CachedBoneNameArgIndex = Arguments.Emplace(CachedBoneNameArgName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::FName);
		CachedBoneIndexArgIndex = Arguments.Emplace(CachedBoneIndexArgName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::Int32);
	}
	
	return Arguments;
}

#if WITH_EDITOR
FText FRigDispatch_AnimAttributeBase::GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == NameArgName)
	{
		return NSLOCTEXT("FRigDispatch_AnimAttributeBase", "NameArgTooltip", "The name of the animation attribute");
	}
	if(InArgumentName == BoneNameArgName)
	{
		return NSLOCTEXT("FRigDispatch_AnimAttributeBase", "BoneNameArgTooltip", "The name of the bone that stores the attribute, default to root bone if set to none");
	}
	if(InArgumentName == DefaultArgName)
	{
		return NSLOCTEXT("FRigDispatch_AnimAttributeBase", "DefaultArgTooltip", "The default value used as a fallback if the animation attribute does not exist");
	}
	if(InArgumentName == ValueArgName)
	{
		return NSLOCTEXT("FRigDispatch_AnimAttributeBase", "ValueArgTooltip", "The value to get / set");
	}
	if(InArgumentName == FoundArgName)
	{
		return NSLOCTEXT("FRigDispatch_AnimAttributeBase", "FoundArgTooltip", "Returns true if the animation attribute exists with the specific type");
	}
	if(InArgumentName == SuccessArgName)
	{
		return NSLOCTEXT("FRigDispatch_AnimAttributeBase", "SuccessArgTooltip", "Returns true if the animation attribute was successfully stored");
	}
	return FRigDispatchFactory::GetArgumentTooltip(InArgumentName, InTypeIndex);	
}




#endif



TArray<FRigVMTemplateArgument> FRigDispatch_GetAnimAttribute::GetArguments() const
{
	if (ValueArgIndex == INDEX_NONE)
	{
		Arguments = Super::GetArguments(); 

		FRigVMTemplateArgument::FTypeFilter	TypeFilter;
		TypeFilter.BindStatic(&FRigDispatch_AnimAttributeBase::IsTypeSupported);
		
		DefaultArgIndex = Arguments.Emplace(DefaultArgName, ERigVMPinDirection::Input, GetValueTypeCategory(), TypeFilter);
		ValueArgIndex = Arguments.Emplace(ValueArgName, ERigVMPinDirection::Output, GetValueTypeCategory(), TypeFilter);
		
		FoundArgIndex = Arguments.Emplace(FoundArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}

	
	return Arguments;
}

FRigVMTemplateTypeMap FRigDispatch_GetAnimAttribute::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;

	// similar pattern to URigVMArrayNode's FRigVMTemplate_NewArgumentTypeDelegate to avoid double registration
	// this is needed since a single type is called for both DefaultArg and ValueArg but we should only
	// register one permutation
	if (InArgumentName == ValueArgName)
	{
		if (IsTypeSupported(InTypeIndex))
		{
			Types.Add(NameArgName, RigVMTypeUtils::TypeIndex::FName);
			Types.Add(BoneNameArgName, RigVMTypeUtils::TypeIndex::FName);
			Types.Add(DefaultArgName, InTypeIndex);
			Types.Add(ValueArgName, InTypeIndex);
			Types.Add(FoundArgName, RigVMTypeUtils::TypeIndex::Bool);
			Types.Add(CachedBoneNameArgName, RigVMTypeUtils::TypeIndex::FName);
			Types.Add(CachedBoneIndexArgName, RigVMTypeUtils::TypeIndex::Int32);
		}
	}
	return Types;
}

FRigVMFunctionPtr FRigDispatch_GetAnimAttribute::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const TRigVMTypeIndex& ValueTypeIndex = InTypes.FindChecked(TEXT("Value"));

	if (ValueTypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch<float>;
	}
	if (ValueTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch<int32>;
	}
	if (ValueTypeIndex == RigVMTypeUtils::TypeIndex::FString)
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch<FString>;
	}
	if (ValueTypeIndex == Registry.GetTypeIndex<FTransform>(false))
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch<FTransform>;
	}
	if (ValueTypeIndex == Registry.GetTypeIndex<FQuat>(false))
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch<FQuat>;
	}
	if (ValueTypeIndex == Registry.GetTypeIndex<FVector>(false))
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch<FVector>;
	}
	
	const FRigVMTemplateArgumentType& ValueType = Registry.GetType(ValueTypeIndex);
	
	if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValueType.CPPTypeObject))
	{
		return &FRigDispatch_GetAnimAttribute::GetAnimAttributeDispatch;
	}

	return nullptr;
}

TArray<FRigVMTemplateArgument> FRigDispatch_SetAnimAttribute::GetArguments() const
{
	if (ValueArgIndex == INDEX_NONE)
	{
		Arguments = Super::GetArguments();

		FRigVMTemplateArgument::FTypeFilter	TypeFilter;
		TypeFilter.BindStatic(&FRigDispatch_AnimAttributeBase::IsTypeSupported);
		
		ValueArgIndex = Arguments.Emplace(ValueArgName, ERigVMPinDirection::Input, GetValueTypeCategory(), TypeFilter);
		SuccessArgIndex = Arguments.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	
	return Arguments;
}

FRigVMTemplateTypeMap FRigDispatch_SetAnimAttribute::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;

	// similar pattern to URigVMArrayNode's FRigVMTemplate_NewArgumentTypeDelegate to avoid double registration
	// likely not strictly needed for this node since it only has a single non-singleton argument
	if (InArgumentName == ValueArgName)
	{
		if (IsTypeSupported(InTypeIndex))
		{
			Types.Add(FRigVMStruct::ExecuteContextName, FRigVMRegistry::Get().GetTypeIndex<FControlRigExecuteContext>());
			Types.Add(NameArgName, RigVMTypeUtils::TypeIndex::FName);
			Types.Add(BoneNameArgName, RigVMTypeUtils::TypeIndex::FName);
			Types.Add(ValueArgName, InTypeIndex);
			Types.Add(SuccessArgName, RigVMTypeUtils::TypeIndex::Bool);
			Types.Add(CachedBoneNameArgName, RigVMTypeUtils::TypeIndex::FName);
			Types.Add(CachedBoneIndexArgName, RigVMTypeUtils::TypeIndex::Int32);
		}
	}
	
	return Types;	
}

FRigVMFunctionPtr FRigDispatch_SetAnimAttribute::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const TRigVMTypeIndex& ValueTypeIndex = InTypes.FindChecked(TEXT("Value"));

	if (ValueTypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch<float>;
	}
	if (ValueTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch<int32>;
	}
	if (ValueTypeIndex == RigVMTypeUtils::TypeIndex::FString)
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch<FString>;
	}
	if (ValueTypeIndex == Registry.GetTypeIndex<FTransform>(false))
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch<FTransform>;
	}
	if (ValueTypeIndex == Registry.GetTypeIndex<FQuat>(false))
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch<FQuat>;
	}
	if (ValueTypeIndex == Registry.GetTypeIndex<FVector>(false))
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch<FVector>;
	}
	
	const FRigVMTemplateArgumentType& ValueType = Registry.GetType(ValueTypeIndex);
	
	if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValueType.CPPTypeObject))
	{
		return &FRigDispatch_SetAnimAttribute::SetAnimAttributeDispatch;
	}

	return nullptr;
}


