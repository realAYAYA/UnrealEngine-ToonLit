// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMTemplate.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMModule.h"
#include "Algo/Accumulate.h"
#include "Algo/ForEach.h"
#include "Algo/Sort.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMTemplate)

////////////////////////////////////////////////////////////////////////////////////////////////

FRigVMTemplateArgumentType::FRigVMTemplateArgumentType(const FName& InCPPType, UObject* InCPPTypeObject)
		: CPPType(InCPPType)
		, CPPTypeObject(InCPPTypeObject)
{
	// InCppType is unreliable because not all caller knows that
	// we use generated unique names for user defined structs
	// so here we override the CppType name with the actual name used in the registry
	const FString InCPPTypeString = CPPType.ToString();
	CPPType = *RigVMTypeUtils::PostProcessCPPType(InCPPTypeString, CPPTypeObject);
#if WITH_EDITOR
	if (CPPType.IsNone())
	{
		UE_LOG(LogRigVM, Warning, TEXT("FRigVMTemplateArgumentType(): Input CPPType '%s' could not be resolved."), *InCPPTypeString);
	}
#endif
}

FRigVMTemplateArgument::FRigVMTemplateArgument()
{}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection)
	: Name(InName)
	, Direction(InDirection)
{}

FRigVMTemplateArgument::FRigVMTemplateArgument(FProperty* InProperty):
	FRigVMTemplateArgument(InProperty, FRigVMRegistry::Get())
{
	
}

FRigVMTemplateArgument::FRigVMTemplateArgument(FProperty* InProperty, FRigVMRegistry& InRegistry)
	: Name(InProperty->GetFName())
{
#if WITH_EDITOR
	Direction = FRigVMStruct::GetPinDirectionFromProperty(InProperty);
#endif

	FString ExtendedType;
	const FString CPPType = InProperty->GetCPPType(&ExtendedType);
	const FName CPPTypeName = *(CPPType + ExtendedType);
	UObject* CPPTypeObject = nullptr;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		InProperty = ArrayProperty->Inner;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		CPPTypeObject = StructProperty->Struct;
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		CPPTypeObject = EnumProperty->GetEnum();
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		CPPTypeObject = ByteProperty->Enum;
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
	{
		if(RigVMCore::SupportsUObjects())
		{
			CPPTypeObject = ObjectProperty->PropertyClass;
		}
	}
	
	const FRigVMTemplateArgumentType Type(CPPTypeName, CPPTypeObject);
	const TRigVMTypeIndex TypeIndex = InRegistry.FindOrAddType(Type, true); 

	TypeIndices.Add(TypeIndex);
	EnsureValidExecuteType(InRegistry);
	UpdateTypeToPermutations();
}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InTypeIndex)
	: Name(InName)
	, Direction(InDirection)
	, TypeIndices({InTypeIndex})
{
	EnsureValidExecuteType(FRigVMRegistry::Get());
	UpdateTypeToPermutations();
}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<TRigVMTypeIndex>& InTypeIndices)
	: Name(InName)
	, Direction(InDirection)
	, TypeIndices(InTypeIndices)
{
	check(TypeIndices.Num() > 0);
	EnsureValidExecuteType(FRigVMRegistry::Get());
	UpdateTypeToPermutations();
}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<ETypeCategory>& InTypeCategories, TFunction<bool(const TRigVMTypeIndex&)> InFilterType)
	: Name(InName)
	, Direction(InDirection)
	, TypeCategories(InTypeCategories)
	, FilterType(InFilterType)
{
	const int32 NumCategories = TypeCategories.Num();
	if (NumCategories > 0)
	{
		TSet<TRigVMTypeIndex> AllTypes;

		TArray<int32> NumTypesByCategory, TypesAddedByCategory;
		TypesAddedByCategory.Reserve(NumCategories);
		NumTypesByCategory.Reserve(NumCategories);

		bUseCategories = NumCategories == 1;
		if (!bUseCategories)
		{
			for (const ETypeCategory TypeCategory : TypeCategories)
			{
				const TArray<TRigVMTypeIndex>& Types = FRigVMRegistry::Get().GetTypesForCategory(TypeCategory);
				AllTypes.Reserve(AllTypes.Num() + Types.Num());
				for (const TRigVMTypeIndex Type: Types)
				{
					AllTypes.Add(Type);
				}
			
				NumTypesByCategory.Add(Types.Num());
				TypesAddedByCategory.Add(AllTypes.Num());
			}
			bUseCategories = NumTypesByCategory[0] == TypesAddedByCategory[0] && Algo::Accumulate(NumTypesByCategory, 0) == AllTypes.Num();
		}

		if (bUseCategories)
		{
			TypeIndices.Reset();
		}
		else
		{
			TArray<TRigVMTypeIndex> Indices = AllTypes.Array();
			if (FilterType)
			{
				Indices = Indices.FilterByPredicate([this](const TRigVMTypeIndex& Type)
				{
					return FilterType(Type);
				});
			}
			TypeIndices = MoveTemp(Indices);
			EnsureValidExecuteType(FRigVMRegistry::Get());
		}

		UpdateTypeToPermutations();
	}
}

void FRigVMTemplateArgument::EnsureValidExecuteType(FRigVMRegistry& InRegistry)
{
	for(TRigVMTypeIndex& TypeIndex : TypeIndices)
	{
		InRegistry.ConvertExecuteContextToBaseType(TypeIndex);
	}
}

void FRigVMTemplateArgument::UpdateTypeToPermutations()
{
	TypeToPermutations.Reset();
	TypeToPermutations.Reserve(GetNumTypes());

	int32 TypeIndex = 0;
	ForEachType([&](const TRigVMTypeIndex Type)
	{
		TypeToPermutations.FindOrAdd(Type).Add(TypeIndex++);
		return true;
	});
}

bool FRigVMTemplateArgument::SupportsTypeIndex(TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex) const
{
	if(InTypeIndex == INDEX_NONE)
	{
		return false;
	}
	
	// convert any execute type into the base execute
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	if(Registry.IsExecuteType(InTypeIndex))
	{
		const bool bIsArray = Registry.IsArrayType(InTypeIndex);
		InTypeIndex = RigVMTypeUtils::TypeIndex::Execute;
		if(bIsArray)
		{
			InTypeIndex = Registry.GetArrayTypeFromBaseTypeIndex(InTypeIndex);
		}
	}

	const TArray<int32>& Permutations = GetPermutations(InTypeIndex);
	if (!Permutations.IsEmpty())
	{
		if(OutTypeIndex)
		{
			(*OutTypeIndex) = GetTypeIndex(Permutations[0]);
		}
		return true;
	}

	// Try to find compatible type
	const TArray<TRigVMTypeIndex>& CompatibleTypes = Registry.GetCompatibleTypes(InTypeIndex);
	for (const TRigVMTypeIndex& CompatibleTypeIndex : CompatibleTypes)
	{
		const TArray<int32>& CompatiblePermutations = GetPermutations(CompatibleTypeIndex);
		if (!CompatiblePermutations.IsEmpty())
		{
			if(OutTypeIndex)
			{
				(*OutTypeIndex) = GetTypeIndex(CompatiblePermutations[0]);
			}
			return true;
		}
	}

	return false;
}

bool FRigVMTemplateArgument::IsSingleton(const TArray<int32>& InPermutationIndices) const
{
	if (TypeToPermutations.Num() == 1)
	{
		return true;
	}
	else if(InPermutationIndices.Num() == 0)
	{
		return false;
	}

	const TRigVMTypeIndex InType0 = GetTypeIndex(InPermutationIndices[0]);
	for (int32 PermutationIndex = 1; PermutationIndex < InPermutationIndices.Num(); PermutationIndex++)
	{
		if (GetTypeIndex(InPermutationIndices[PermutationIndex]) != InType0)
		{
			return false;
		}
	}
	return true;
}

bool FRigVMTemplateArgument::IsExecute() const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const int32 FoundAnyNotExec = IndexOfByPredicate([&](const TRigVMTypeIndex Type)
	{
		return !Registry.IsExecuteType(Type);
	});
	return FoundAnyNotExec == INDEX_NONE;
}

FRigVMTemplateArgument::EArrayType FRigVMTemplateArgument::GetArrayType() const
{
	const int32 NumTypes = GetNumTypes();
	if (GetNumTypes() > 0)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		const EArrayType ArrayType = Registry.IsArrayType(GetTypeIndex(0)) ? EArrayType_ArrayValue : EArrayType_SingleValue;
		
		if(IsSingleton())
		{
			return ArrayType;
		}

		for(int32 PermutationIndex=1; PermutationIndex<NumTypes;PermutationIndex++)
		{
			const TRigVMTypeIndex TypeIndex = GetTypeIndex(PermutationIndex);
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex == INDEX_NONE)
			{
				continue;
			}
			
			const EArrayType OtherArrayType = Registry.IsArrayType(TypeIndex) ? EArrayType_ArrayValue : EArrayType_SingleValue;
			if(OtherArrayType != ArrayType)
			{
				return EArrayType_Mixed;
			}
		}

		return ArrayType;
	}

	return EArrayType_Invalid;
}

const TArray<int32>& FRigVMTemplateArgument::GetPermutations(const TRigVMTypeIndex InType) const
{
	if (const TArray<int32>* Found = TypeToPermutations.Find(InType))
	{
		return *Found;
	}

	int32 IndexInTypes = 0;
	TArray<int32> Permutations;
	ForEachType([&](const TRigVMTypeIndex Type)
	{
		if (Type == InType)
		{
			Permutations.Add(IndexInTypes);
		}
		IndexInTypes++;
		return true;
	});

	if (!Permutations.IsEmpty())
	{
		return TypeToPermutations.Emplace(InType, MoveTemp(Permutations));
	}

	static const TArray<int32> Dummy;
	return Dummy;
}

void FRigVMTemplateArgument::InvalidatePermutations(const TRigVMTypeIndex InType)
{
	TypeToPermutations.Remove(InType);
}

void FRigVMTemplateArgument::GetAllTypes(TArray<TRigVMTypeIndex>& OutTypes) const
{
	if (!bUseCategories)
	{
		OutTypes = TypeIndices;
		return;
	}
	
	OutTypes.Reset();
	for (const ETypeCategory Category: TypeCategories)
	{
		if (FilterType == nullptr)
		{
			OutTypes.Append(FRigVMRegistry::Get().GetTypesForCategory(Category));
		}
		else
		{
			const TArray<TRigVMTypeIndex>& CategoryTypes = FRigVMRegistry::Get().GetTypesForCategory(Category);
			for (const TRigVMTypeIndex& Type : CategoryTypes)
			{
				if (FilterType(Type))
				{
					OutTypes.Add(Type);
				}
			}
		}
	}
}

TRigVMTypeIndex FRigVMTemplateArgument::GetTypeIndex(const int32 InIndex) const
{
	if (!bUseCategories)
	{
		check(!TypeIndices.IsEmpty())
		return TypeIndices.IsValidIndex(InIndex) ? TypeIndices[InIndex] : TypeIndices[0];		
	}

	if (FilterType)
	{
		TRigVMTypeIndex ValidType = INDEX_NONE;
		int32 ValidIndex = 0;
		CategoryViews(TypeCategories).ForEachType([this, &ValidIndex, InIndex, &ValidType](const TRigVMTypeIndex& Type) -> bool
		{
			if (FilterType(Type))
			{
				if (ValidIndex == InIndex)
				{
					ValidType = Type;
					return false;
				}
				ValidIndex++;
			}
			return true;
		});
		return ValidType;
	}

	return CategoryViews(TypeCategories).GetTypeIndex(InIndex);
}

int32 FRigVMTemplateArgument::FindTypeIndex(const TRigVMTypeIndex InTypeIndex) const
{
	if (!bUseCategories)
	{
		return TypeIndices.IndexOfByKey(InTypeIndex);
	}

	if (FilterType)
	{
		bool bFound = false;
		int32 ValidIndex = 0;
		CategoryViews(TypeCategories).ForEachType([this, &ValidIndex, &bFound, InTypeIndex](const TRigVMTypeIndex& Type) -> bool
		{
			if (Type == InTypeIndex)
			{
				bFound = true;
				return false;
			}
			if (FilterType(Type))
			{
				ValidIndex++;
			}
			return true;
		});
		if (bFound)
		{
			return ValidIndex;
		}
		return INDEX_NONE;
	}

	return CategoryViews(TypeCategories).FindIndex(InTypeIndex);
}

int32 FRigVMTemplateArgument::GetNumTypes() const
{
	if (!bUseCategories)
	{
		return TypeIndices.Num();
	}

	if (FilterType)
	{
		int32 NumTypes = 0;
		CategoryViews(TypeCategories).ForEachType([this, &NumTypes](const TRigVMTypeIndex& Type) -> bool
		{
			if (FilterType(Type))
			{
				NumTypes++;
			}
			return true;
		});
		return NumTypes;
	}
	
	return Algo::Accumulate(TypeCategories, 0, [](int32 Sum, const ETypeCategory Category)
	{
		return Sum + FRigVMRegistry::Get().GetTypesForCategory(Category).Num();
	});
}

void FRigVMTemplateArgument::AddTypeIndex(const TRigVMTypeIndex InTypeIndex)
{
	ensure( TypeCategories.IsEmpty() );
	TypeIndices.AddUnique(InTypeIndex);
}

void FRigVMTemplateArgument::RemoveType(const int32 InIndex)
{
	ensure( TypeCategories.IsEmpty() );
	TypeIndices.RemoveAt(InIndex);
}

void FRigVMTemplateArgument::ForEachType(TFunction<bool(const TRigVMTypeIndex InType)>&& InCallback) const
{
	if (!bUseCategories)
	{
		return Algo::ForEach(TypeIndices, InCallback);
	}

	if (FilterType)
	{
		CategoryViews(TypeCategories).ForEachType([this, InCallback](const TRigVMTypeIndex Type)
		{
			if (FilterType(Type))
			{
				return InCallback(Type);
			}
			return true;
		});
		return;
	}
	
	return CategoryViews(TypeCategories).ForEachType(MoveTemp(InCallback));
}

TArray<TRigVMTypeIndex> FRigVMTemplateArgument::GetSupportedTypeIndices(const TArray<int32>& InPermutationIndices) const
{
	TArray<TRigVMTypeIndex> SupportedTypes;
	if(InPermutationIndices.IsEmpty())
	{
		ForEachType([&](const TRigVMTypeIndex TypeIndex)
		{
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex != INDEX_NONE)
			{
				SupportedTypes.AddUnique(TypeIndex);
			}
			return true;
		});
	}
	else
	{
		for(const int32 PermutationIndex : InPermutationIndices)
		{
			// INDEX_NONE indicates deleted permutation
			const TRigVMTypeIndex Type = GetTypeIndex(PermutationIndex);
			if (Type != INDEX_NONE)
			{
				SupportedTypes.AddUnique(Type);
			}
		}
	}
	return SupportedTypes;
}

#if WITH_EDITOR

TArray<FString> FRigVMTemplateArgument::GetSupportedTypeStrings(const TArray<int32>& InPermutationIndices) const
{
	TArray<FString> SupportedTypes;
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	if(InPermutationIndices.IsEmpty())
	{
		ForEachType([&](const TRigVMTypeIndex TypeIndex)
		{
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex != INDEX_NONE)
			{
				const FString TypeString = Registry.GetType(TypeIndex).CPPType.ToString();
				SupportedTypes.AddUnique(TypeString);
			}
			return true;
		});
	}
	else
	{
		for(const int32 PermutationIndex : InPermutationIndices)
		{
			const TRigVMTypeIndex TypeIndex = GetTypeIndex(PermutationIndex);
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex != INDEX_NONE)
			{
				const FString TypeString = Registry.GetType(TypeIndex).CPPType.ToString();
				SupportedTypes.AddUnique(TypeString);
			}
		}
	}
	return SupportedTypes;
}

#endif


FRigVMTemplateArgument::CategoryViews::CategoryViews(const TArray<ETypeCategory>& InCategories)
{
	Types.Reserve(InCategories.Num());
	for (const ETypeCategory Category: InCategories)
	{
		Types.Emplace(FRigVMRegistry::Get().GetTypesForCategory(Category));
	}
}

void FRigVMTemplateArgument::CategoryViews::ForEachType(TFunction<bool(const TRigVMTypeIndex InType)>&& InCallback) const
{
	for (const TArrayView<const TRigVMTypeIndex>& TypeView: Types)
	{
		for (const TRigVMTypeIndex& TypeIndex : TypeView)
		{
			if (!InCallback(TypeIndex))
			{
				return;
			}
		}
	}
}

TRigVMTypeIndex FRigVMTemplateArgument::CategoryViews::GetTypeIndex(int32 InIndex) const
{
	for (const TArrayView<const TRigVMTypeIndex>& TypeView: Types)
	{
		if (TypeView.IsValidIndex(InIndex))
		{
			return TypeView[InIndex];  
		}
		InIndex -= TypeView.Num();
	}
	return INDEX_NONE;
}
	
int32 FRigVMTemplateArgument::CategoryViews::FindIndex(const TRigVMTypeIndex InTypeIndex) const
{
	int32 Offset = 0;
	for (const TArrayView<const TRigVMTypeIndex>& TypeView: Types)
	{
		const int32 Found = TypeView.IndexOfByKey(InTypeIndex);
		if (Found != INDEX_NONE)
		{
			return Found + Offset;
		}
		Offset += TypeView.Num();
	}
	return INDEX_NONE;
}

/**
 * FRigVMTemplateArgumentInfo 
 */

FRigVMTemplateArgumentInfo::FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, const TArray<TRigVMTypeIndex>& InTypeIndices)
	: Name(InName)
	, Direction(InDirection)
	, FactoryCallback([InTypeIndices](const FName InName, ERigVMPinDirection InDirection){ return FRigVMTemplateArgument(InName, InDirection, InTypeIndices); } )
{}

FRigVMTemplateArgumentInfo::FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InTypeIndex)
	: Name(InName)
	, Direction(InDirection)
	, FactoryCallback( [InTypeIndex](const FName InName, ERigVMPinDirection InDirection){ return FRigVMTemplateArgument(InName, InDirection, InTypeIndex); } )
{}

FRigVMTemplateArgumentInfo::FRigVMTemplateArgumentInfo(
	const FName InName, ERigVMPinDirection InDirection,
	const TArray<FRigVMTemplateArgument::ETypeCategory>& InTypeCategories,
	TypeFilterCallback InTypeFilter)
	: Name(InName)
	, Direction(InDirection)
	, FactoryCallback([InTypeCategories, InTypeFilter](const FName InName, ERigVMPinDirection InDirection){ return FRigVMTemplateArgument(InName, InDirection, InTypeCategories, InTypeFilter); })
{}

FRigVMTemplateArgumentInfo::FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection)
	: Name(InName)
	, Direction(InDirection)
	, FactoryCallback([](const FName InName, ERigVMPinDirection InDirection){ return FRigVMTemplateArgument(InName, InDirection); })
{}

FRigVMTemplateArgumentInfo::FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, ArgumentCallback&& InCallback)
	: Name(InName)
	, Direction(InDirection)
	, FactoryCallback(InCallback)
{}

FRigVMTemplateArgument FRigVMTemplateArgumentInfo::GetArgument() const
{
	FRigVMTemplateArgument Argument = FactoryCallback(Name, Direction);
	return MoveTemp(Argument);
}

FName FRigVMTemplateArgumentInfo::ComputeTemplateNotation(const FName InTemplateName, const TArray<FRigVMTemplateArgumentInfo>& InInfos)
{
	if (InInfos.IsEmpty())
	{
		return NAME_None;	
	}
		
	TArray<FString> ArgumentNotations;
	Algo::TransformIf(
		InInfos,
		ArgumentNotations,
		[](const FRigVMTemplateArgumentInfo& Info){ return Info.Direction != ERigVMPinDirection::Invalid && Info.Direction != ERigVMPinDirection::Hidden; },
		[](const FRigVMTemplateArgumentInfo& Info){ return FRigVMTemplate::GetArgumentNotation(Info.Name, Info.Direction); }
	);

	if (ArgumentNotations.IsEmpty())
	{
		return NAME_None;
	}
	
	const FString NotationStr = FString::Printf(TEXT("%s(%s)"), *InTemplateName.ToString(), *FString::Join(ArgumentNotations, TEXT(",")));
	return *NotationStr;
}

TArray<TRigVMTypeIndex> FRigVMTemplateArgumentInfo::GetTypesFromCategories(
	const TArray<FRigVMTemplateArgument::ETypeCategory>& InTypeCategories,
	const FRigVMTemplateArgument::FTypeFilter& InTypeFilter)
{
	TSet<TRigVMTypeIndex> AllTypes;
	for (const FRigVMTemplateArgument::ETypeCategory TypeCategory : InTypeCategories)
	{
		AllTypes.Append(FRigVMRegistry::Get().GetTypesForCategory(TypeCategory));
	}

	TArray<TRigVMTypeIndex> Types;
	if (!InTypeFilter.IsBound())
	{
		Types = AllTypes.Array();
	}
	else
	{
		Types.Reserve(AllTypes.Num());
		for (const TRigVMTypeIndex& Type : AllTypes)
		{
			if (InTypeFilter.Execute(Type))
			{
				Types.Add(Type);
			}
		}
	}
	
	return MoveTemp(Types);
}

////////////////////////////////////////////////////////////////////////////////////////////////

FRigVMTemplate::FRigVMTemplate()
	: Index(INDEX_NONE)
	, Notation(NAME_None)
	, Hash(UINT32_MAX)
{

}

FRigVMTemplate::FRigVMTemplate(UScriptStruct* InStruct, const FString& InTemplateName, int32 InFunctionIndex)
	: Index(INDEX_NONE)
	, Notation(NAME_None)
	, Hash(UINT32_MAX)
{
	TArray<FString> ArgumentNotations;

	// create the arguments sorted by super -> child struct.
	TArray<UStruct*> Structs = GetSuperStructs(InStruct, true);
	for(UStruct* Struct : Structs)
	{
		// only iterate on this struct's fields, not the super structs'
		for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::None); It; ++It)
		{
			FRigVMTemplateArgument Argument(*It);
			Argument.Index = Arguments.Num();

			if(!Argument.IsExecute() && IsValidArgumentForTemplate(Argument.GetDirection()) && Argument.GetDirection() != ERigVMPinDirection::Hidden)
			{
				Arguments.Add(Argument);
			}
		}
	}

	// the template notation needs to be in the same order as the C++ implementation,
	// which is the order of child -> super class members
	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		if(const FRigVMTemplateArgument* Argument = FindArgument(It->GetFName()))
		{
			if(!Argument->IsExecute() && Argument->GetDirection() != ERigVMPinDirection::Hidden)
			{
				ArgumentNotations.Add(GetArgumentNotation(Argument->Name, Argument->Direction));
			}
		}
	}

	if (ArgumentNotations.Num() > 0)
	{
		const FString NotationStr = FString::Printf(TEXT("%s(%s)"), *InTemplateName, *FString::Join(ArgumentNotations, TEXT(",")));
		Notation = *NotationStr;
		if (InFunctionIndex != INDEX_NONE)
		{
			Permutations.Add(InFunctionIndex);
			for (const FRigVMTemplateArgument& Argument : Arguments)
			{
				check(Argument.TypeIndices.Num() == 1);
			}
		}

		UpdateTypesHashToPermutation(Permutations.Num()-1);
	}
}

FRigVMTemplate::FRigVMTemplate(const FName& InTemplateName, const TArray<FRigVMTemplateArgumentInfo>& InInfos)
	: Index(INDEX_NONE)
	, Notation(NAME_None)
	, Hash(UINT32_MAX)
{
	for (const FRigVMTemplateArgumentInfo& InInfo : InInfos)
	{
		if(IsValidArgumentForTemplate(InInfo.Direction))
		{
			FRigVMTemplateArgument Argument = InInfo.GetArgument();
			Argument.Index = Arguments.Num();
			Arguments.Emplace(MoveTemp(Argument));
		}
	}
	
	Notation = FRigVMTemplateArgumentInfo::ComputeTemplateNotation(InTemplateName, InInfos);
	UpdateTypesHashToPermutation(Permutations.Num()-1);
}

FLinearColor FRigVMTemplate::GetColorFromMetadata(FString InMetadata)
{
	FLinearColor Color = FLinearColor::Black;

	FString Metadata = InMetadata;
	Metadata.TrimStartAndEndInline();
	static const FString SplitString(TEXT(" "));
	FString Red, Green, Blue, GreenAndBlue;
	if (Metadata.Split(SplitString, &Red, &GreenAndBlue))
	{
		Red.TrimEndInline();
		GreenAndBlue.TrimStartInline();
		if (GreenAndBlue.Split(SplitString, &Green, &Blue))
		{
			Green.TrimEndInline();
			Blue.TrimStartInline();

			const float RedValue = FCString::Atof(*Red);
			const float GreenValue = FCString::Atof(*Green);
			const float BlueValue = FCString::Atof(*Blue);
			Color = FLinearColor(RedValue, GreenValue, BlueValue);
		}
	}

	return Color;
}

bool FRigVMTemplate::IsValidArgumentForTemplate(const ERigVMPinDirection InDirection)
{
	return InDirection != ERigVMPinDirection::Invalid;
}


const FString& FRigVMTemplate::GetDirectionPrefix(const ERigVMPinDirection InDirection)
{
	static const FString EmptyPrefix = FString();
	static const FString InPrefix = TEXT("in ");
	static const FString OutPrefix = TEXT("out ");
	static const FString IOPrefix = TEXT("io ");

	switch(InDirection)
	{
	case ERigVMPinDirection::Input:
	case ERigVMPinDirection::Visible:
		{
			return InPrefix;
		}
	case ERigVMPinDirection::Output:
		{
			return OutPrefix;
		}
	case ERigVMPinDirection::IO:
		{
			return IOPrefix;
		}
	default:
		{
			break;
		}
	}

	return EmptyPrefix;
}

FString FRigVMTemplate::GetArgumentNotation(const FName InName, const ERigVMPinDirection InDirection)
{
	return FString::Printf(TEXT("%s%s"), *GetDirectionPrefix(InDirection), *InName.ToString());
}

void FRigVMTemplate::ComputeNotationFromArguments(const FString& InTemplateName)
{
	TArray<FString> ArgumentNotations;			
	for (const FRigVMTemplateArgument& Argument : Arguments)
	{
		if(IsValidArgumentForTemplate(Argument.GetDirection()))
		{
			ArgumentNotations.Add(GetArgumentNotation(Argument.Name, Argument.Direction));
		}
	}

	const FString NotationStr = FString::Printf(TEXT("%s(%s)"), *InTemplateName, *FString::Join(ArgumentNotations, TEXT(",")));
	Notation = *NotationStr;
}

TArray<UStruct*> FRigVMTemplate::GetSuperStructs(UStruct* InStruct, bool bIncludeLeaf)
{
	// Create an array of structs, ordered super -> child struct
	TArray<UStruct*> SuperStructs = {InStruct};
	while(true)
	{
		if(UStruct* SuperStruct = SuperStructs[0]->GetSuperStruct())
		{
			SuperStructs.Insert(SuperStruct, 0);
		}
		else
		{
			break;
		}
	}

	if(!bIncludeLeaf)
	{
		SuperStructs.Remove(SuperStructs.Last());
	}

	return SuperStructs;
}

FRigVMTemplate::FTypeMap FRigVMTemplate::GetArgumentTypesFromString(const FString& InTypeString, const FRigVMUserDefinedTypeResolver* InTypeResolver) const
{
	FTypeMap Types;
	if(!InTypeString.IsEmpty())
	{
		FRigVMRegistry& Registry = FRigVMRegistry::Get();

		FString Left, Right = InTypeString;
		while(!Right.IsEmpty())
		{
			if(!Right.Split(TEXT(","), &Left, &Right))
			{
				Left = Right;
				Right.Reset();
			}

			FString ArgumentName, TypeName;
			if(Left.Split(TEXT(":"), &ArgumentName, &TypeName))
			{
				if(const FRigVMTemplateArgument* Argument = FindArgument(*ArgumentName))
				{
					TRigVMTypeIndex TypeIndex = Registry.GetTypeIndexFromCPPType(TypeName);

					// If the type was not found, check if it's a user-defined type that hasn't been registered yet.
					if (TypeIndex == INDEX_NONE && RigVMTypeUtils::RequiresCPPTypeObject(TypeName))
					{
						UObject* CPPTypeObject = RigVMTypeUtils::ObjectFromCPPType(TypeName, true, InTypeResolver);
						
						FRigVMTemplateArgumentType ArgType(*TypeName, CPPTypeObject);
						TypeIndex = Registry.FindOrAddType(ArgType);
					}
					
					if(TypeIndex != INDEX_NONE)
					{
						Types.Add(Argument->GetName(), TypeIndex);
					}
				}
			}
		}
	}
	return Types;
}

FString FRigVMTemplate::GetStringFromArgumentTypes(const FTypeMap& InTypes)
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	
	TArray<FString> TypePairStrings;
	for(const TPair<FName,TRigVMTypeIndex>& Pair : InTypes)
	{
		const FRigVMTemplateArgumentType& Type = Registry.GetType(Pair.Value);
		static constexpr TCHAR Format[] = TEXT("%s:%s");
		TypePairStrings.Add(FString::Printf(Format, *Pair.Key.ToString(), *Type.CPPType.ToString()));
	}
			
	return FString::Join(TypePairStrings, TEXT(","));
}

bool FRigVMTemplate::IsValid() const
{
	return !Notation.IsNone();
}

const FName& FRigVMTemplate::GetNotation() const
{
	return Notation;
}

FName FRigVMTemplate::GetName() const
{
	FString Left;
	if (GetNotation().ToString().Split(TEXT("::"), &Left, nullptr))
	{
		return *Left;
	}
	if (GetNotation().ToString().Split(TEXT("("), &Left, nullptr))
	{
		return *Left;
	}
	return NAME_None;
}

#if WITH_EDITOR

FLinearColor FRigVMTemplate::GetColor(const TArray<int32>& InPermutationIndices) const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		return Factory->GetNodeColor();
	}

	bool bFirstColorFound = false;
	FLinearColor ResolvedColor = FLinearColor::White;

	auto VisitPermutation = [&bFirstColorFound, &ResolvedColor, this](int32 InPermutationIndex) -> bool
	{
		static const FName NodeColorName = TEXT("NodeColor");
		FString NodeColorMetadata;

		// if we can't find one permutation we are not going to find any, so it's ok to return false here
		const FRigVMFunction* ResolvedFunction = GetPermutation(InPermutationIndex);
		if(ResolvedFunction == nullptr)
		{
			return false;
		}

		ResolvedFunction->Struct->GetStringMetaDataHierarchical(NodeColorName, &NodeColorMetadata);
		if (!NodeColorMetadata.IsEmpty())
		{
			if(bFirstColorFound)
			{
				const FLinearColor NodeColor = GetColorFromMetadata(NodeColorMetadata);
				if(!ResolvedColor.Equals(NodeColor, 0.01f))
				{
					ResolvedColor = FLinearColor::White;
					return false;
				}
			}
			else
			{
				ResolvedColor = GetColorFromMetadata(NodeColorMetadata);
				bFirstColorFound = true;
			}
		}
		return true;
	};

	if(InPermutationIndices.IsEmpty())
	{
		for(int32 PermutationIndex = 0; PermutationIndex < Permutations.Num(); PermutationIndex++)
		{
			if(!VisitPermutation(PermutationIndex))
			{
				break;
			}
		}
	}
	else
	{
		for(const int32 PermutationIndex : InPermutationIndices)
		{
			if(!VisitPermutation(PermutationIndex))
			{
				break;
			}
		}
	}
	return ResolvedColor;
}

FText FRigVMTemplate::GetTooltipText(const TArray<int32>& InPermutationIndices) const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		FTypeMap Types;
		if(InPermutationIndices.Num() == 1)
		{
			Types = GetTypesForPermutation(InPermutationIndices[0]);
		}
		return Factory->GetNodeTooltip(Types);
	}

	FText ResolvedTooltipText;

	auto VisitPermutation = [&ResolvedTooltipText, this](int32 InPermutationIndex) -> bool
	{
		if (InPermutationIndex >= NumPermutations())
		{
			return false;
		}
		
		// if we can't find one permutation we are not going to find any, so it's ok to return false here
		const FRigVMFunction* ResolvedFunction = GetPermutation(InPermutationIndex);
		if(ResolvedFunction == nullptr)
		{
			return false;
		}
		
		const FText TooltipText = ResolvedFunction->Struct->GetToolTipText();
		
		if (!ResolvedTooltipText.IsEmpty())
		{
			if(!ResolvedTooltipText.EqualTo(TooltipText))
			{
				ResolvedTooltipText = FText::FromName(GetName());
				return false;
			}
		}
		else
		{
			ResolvedTooltipText = TooltipText;
		}
		return true;
	};

	for(int32 PermutationIndex = 0; PermutationIndex < Permutations.Num(); PermutationIndex++)
	{
		if(!VisitPermutation(PermutationIndex))
		{
			break;
		}
	}

	return ResolvedTooltipText;
}

FText FRigVMTemplate::GetDisplayNameForArgument(const FName& InArgumentName, const TArray<int32>& InPermutationIndices) const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		const FName DisplayName = Factory->GetDisplayNameForArgument(InArgumentName);
		if(DisplayName.IsNone())
		{
			return FText();
		}
		return FText::FromName(DisplayName);
	}

	if(const FRigVMTemplateArgument* Argument = FindArgument(InArgumentName))
	{
		FText ResolvedDisplayName;

		auto VisitPermutation = [InArgumentName, &ResolvedDisplayName, this](const int32 InPermutationIndex) -> bool
		{
			// if we can't find one permutation we are not going to find any, so it's ok to return false here
			const FRigVMFunction* ResolvedFunction = GetPermutation(InPermutationIndex);
			if(ResolvedFunction == nullptr)
			{
				return false;
			}
			
			if(const FProperty* Property = ResolvedFunction->Struct->FindPropertyByName(InArgumentName))
			{
				const FText DisplayName = Property->GetDisplayNameText();
				if (!ResolvedDisplayName.IsEmpty())
				{
					if(!ResolvedDisplayName.EqualTo(DisplayName))
					{
						ResolvedDisplayName = FText::FromName(InArgumentName);
						return false;
					}
				}
				else
				{
					ResolvedDisplayName = DisplayName;
				}
			}
			return true;
		};

		if(InPermutationIndices.IsEmpty())
		{
			for(int32 PermutationIndex = 0; PermutationIndex < Permutations.Num(); PermutationIndex++)
			{
				if(!VisitPermutation(PermutationIndex))
				{
					break;
				}
			}
		}
		else
		{
			for(const int32 PermutationIndex : InPermutationIndices)
			{
				if(!VisitPermutation(PermutationIndex))
				{
					break;
				}
			}
		}

		return ResolvedDisplayName;
	}
	return FText();
}

FString FRigVMTemplate::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey, const TArray<int32>& InPermutationIndices) const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		return Factory->GetArgumentMetaData(InArgumentName, InMetaDataKey);
	}

	if(const FRigVMTemplateArgument* Argument = FindArgument(InArgumentName))
	{
		FString ResolvedMetaData;

		auto VisitPermutation = [InArgumentName, &ResolvedMetaData, InMetaDataKey, this](const int32 InPermutationIndex) -> bool
		{
			// if we can't find one permutation we are not going to find any, so it's ok to return false here
			const FRigVMFunction* ResolvedFunction = GetPermutation(InPermutationIndex);
			if(ResolvedFunction == nullptr)
			{
				return false;
			}
			
			if(const FProperty* Property = ResolvedFunction->Struct->FindPropertyByName(InArgumentName))
			{
				const FString MetaData = Property->GetMetaData(InMetaDataKey);
				if (!ResolvedMetaData.IsEmpty())
				{
					if(!ResolvedMetaData.Equals(MetaData))
					{
						ResolvedMetaData = FString();
						return false;
					}
				}
				else
				{
					ResolvedMetaData = MetaData;
				}
			}
			return true;
		};

		if(InPermutationIndices.IsEmpty())
		{
			for(int32 PermutationIndex = 0; PermutationIndex < Permutations.Num(); PermutationIndex++)
			{
				if(!VisitPermutation(PermutationIndex))
				{
					break;
				}
			}
		}
		else
		{
			for(const int32 PermutationIndex : InPermutationIndices)
			{
				if(!VisitPermutation(PermutationIndex))
				{
					break;
				}
			}
		}

		return ResolvedMetaData;
	}
	return FString();
}

#endif

bool FRigVMTemplate::Merge(const FRigVMTemplate& InOther)
{
	if (!IsValid() || !InOther.IsValid())
	{
		return false;
	}

	if(Notation != InOther.Notation)
	{
		return false;
	}

	if(InOther.GetExecuteContextStruct() != GetExecuteContextStruct())
	{
		// find the previously defined permutation.
		UE_LOG(LogRigVM, Display, TEXT("RigVMFunction '%s' cannot be merged into the '%s' template. ExecuteContext Types differ ('%s' vs '%s' from '%s')."),
			*InOther.GetPrimaryPermutation()->Name,
			*GetNotation().ToString(),
			*InOther.GetExecuteContextStruct()->GetStructCPPName(),
			*GetExecuteContextStruct()->GetStructCPPName(),
			*GetPrimaryPermutation()->Name);
		return false;
	}

	if (InOther.Permutations.Num() != 1)
	{
		return false;
	}

	// find colliding permutations
	for(int32 PermutationIndex = 0;PermutationIndex<NumPermutations();PermutationIndex++)
	{
		int32 MatchingArguments = 0;
		for(int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ArgumentIndex++)
		{
			if (Arguments[ArgumentIndex].GetTypeIndex(PermutationIndex) == InOther.Arguments[ArgumentIndex].GetTypeIndex(0))
			{
				MatchingArguments++;
			}
		}
		if(MatchingArguments == Arguments.Num())
		{
			// find the previously defined permutation.
			UE_LOG(LogRigVM, Display, TEXT("RigVMFunction '%s' cannot be merged into the '%s' template. It collides with '%s'."),
				*InOther.GetPrimaryPermutation()->Name,
				*GetNotation().ToString(),
				*GetPermutation(PermutationIndex)->Name);
			return false;
		}
	}

	TArray<FRigVMTemplateArgument> NewArgs;

	for (int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ArgumentIndex++)
	{
		const FRigVMTemplateArgument& OtherArg = InOther.Arguments[ArgumentIndex];
		if (OtherArg.GetNumTypes() != 1)
		{
			return false;
		}

		// Add Other argument information into the TypeToPermutations map
		{
			FRigVMTemplateArgument& NewArg = NewArgs.Add_GetRef(Arguments[ArgumentIndex]);
			const TRigVMTypeIndex OtherTypeIndex = OtherArg.GetTypeIndex(0);
			const int32 NewPermutationIndex = NewArg.GetNumTypes();
			if (TArray<int32>* ArgTypePermutations = NewArg.TypeToPermutations.Find(OtherTypeIndex))
			{
				ArgTypePermutations->Add(NewPermutationIndex);
			}
			else
			{
				NewArg.TypeToPermutations.Add(OtherTypeIndex, {NewPermutationIndex});
			}
			NewArg.TypeIndices.Add(OtherTypeIndex);
		}
	}

	Arguments = NewArgs;

	Permutations.Add(InOther.Permutations[0]);

	UpdateTypesHashToPermutation(Permutations.Num()-1);
	
	return true;
}

const FRigVMTemplateArgument* FRigVMTemplate::FindArgument(const FName& InArgumentName) const
{
	return Arguments.FindByPredicate([InArgumentName](const FRigVMTemplateArgument& Argument) -> bool
	{
		return Argument.GetName() == InArgumentName;
	});
}

int32 FRigVMTemplate::NumExecuteArguments(const FRigVMDispatchContext& InContext) const
{
	return GetExecuteArguments(InContext).Num();
}

const FRigVMExecuteArgument* FRigVMTemplate::GetExecuteArgument(int32 InIndex, const FRigVMDispatchContext& InContext) const
{
	const TArray<FRigVMExecuteArgument>& Args = GetExecuteArguments(InContext);
	if(Args.IsValidIndex(InIndex))
	{
		return &Args[InIndex];
	}
	return nullptr;
}

const FRigVMExecuteArgument* FRigVMTemplate::FindExecuteArgument(const FName& InArgumentName, const FRigVMDispatchContext& InContext) const
{
	const TArray<FRigVMExecuteArgument>& Args = GetExecuteArguments(InContext);
	return Args.FindByPredicate([InArgumentName](const FRigVMExecuteArgument& Arg) -> bool
	{
		return Arg.Name == InArgumentName;
	});
}

const TArray<FRigVMExecuteArgument>& FRigVMTemplate::GetExecuteArguments(const FRigVMDispatchContext& InContext) const
{
	if(ExecuteArguments.IsEmpty())
	{
		if(UsesDispatch())
		{
			const FRigVMDispatchFactory* Factory = Delegates.GetDispatchFactoryDelegate.Execute();
			check(Factory);

			ExecuteArguments = Factory->GetExecuteArguments(InContext);
		}
		else if(const FRigVMFunction* PrimaryPermutation = GetPrimaryPermutation())
		{
			if(PrimaryPermutation->Struct)
			{
				TArray<UStruct*> Structs = GetSuperStructs(PrimaryPermutation->Struct, true);
				for(const UStruct* Struct : Structs)
				{
					// only iterate on this struct's fields, not the super structs'
					for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::None); It; ++It)
					{
						FRigVMTemplateArgument Argument(*It);
						if(Argument.IsExecute())
						{
							ExecuteArguments.Emplace(Argument.Name, Argument.Direction, Argument.GetTypeIndex(0));
						}
					}
				}
			}
		}
	}
	return ExecuteArguments;
}

const UScriptStruct* FRigVMTemplate::GetExecuteContextStruct() const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		return Factory->GetExecuteContextStruct();
	}
	check(!Permutations.IsEmpty());
	return GetPrimaryPermutation()->GetExecuteContextStruct();
}

bool FRigVMTemplate::SupportsExecuteContextStruct(const UScriptStruct* InExecuteContextStruct) const
{
	return InExecuteContextStruct->IsChildOf(GetExecuteContextStruct());
}

bool FRigVMTemplate::ArgumentSupportsTypeIndex(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex) const
{
	if (const FRigVMTemplateArgument* Argument = FindArgument(InArgumentName))
	{
		return Argument->SupportsTypeIndex(InTypeIndex, OutTypeIndex);
	}
	return false;
}

const FRigVMFunction* FRigVMTemplate::GetPrimaryPermutation() const
{
	if (NumPermutations() > 0)
	{
		return GetPermutation(0);
	}
	return nullptr;
}

const FRigVMFunction* FRigVMTemplate::GetPermutation(int32 InIndex) const
{
	FScopeLock FunctionRegistryScopeLock(&FRigVMRegistry::FunctionRegistryMutex);
	return GetPermutation_NoLock(InIndex);
}

const FRigVMFunction* FRigVMTemplate::GetPermutation_NoLock(int32 InIndex) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const int32 FunctionIndex = Permutations[InIndex];
	if(Registry.GetFunctions().IsValidIndex(FunctionIndex))
	{
		return &Registry.GetFunctions()[Permutations[InIndex]];
	}
	return nullptr;
}

const FRigVMFunction* FRigVMTemplate::GetOrCreatePermutation(int32 InIndex)
{
	FScopeLock FunctionRegistryScopeLock(&FRigVMRegistry::FunctionRegistryMutex);

	return GetOrCreatePermutation_NoLock(InIndex);
}

const FRigVMFunction* FRigVMTemplate::GetOrCreatePermutation_NoLock(int32 InIndex)
{
	if(const FRigVMFunction* Function = GetPermutation_NoLock(InIndex))
	{
		return Function;
	}

	if(Permutations[InIndex] == INDEX_NONE && UsesDispatch())
	{
		FRigVMRegistry& Registry = FRigVMRegistry::Get();
		
		FTypeMap Types;
		for(const FRigVMTemplateArgument& Argument : Arguments)
		{
			Types.Add(Argument.GetName(), Argument.GetTypeIndex(InIndex));
		}

		FRigVMDispatchFactory* Factory = Delegates.GetDispatchFactoryDelegate.Execute();
		if (ensure(Factory))
		{
			const FRigVMFunctionPtr DispatchFunction = Factory->CreateDispatchFunction(Types);

			TArray<FRigVMFunctionArgument> FunctionArguments;
			for(const FRigVMTemplateArgument& Argument : Arguments)
			{
				const FRigVMTemplateArgumentType& Type = Registry.GetType(Argument.GetTypeIndex(InIndex));
				FunctionArguments.Add(FRigVMFunctionArgument(Argument.Name.ToString(), Type.CPPType.ToString()));
			}
			
			static constexpr TCHAR Format[] = TEXT("%s::%s");
			const FString PermutationName = Factory->GetPermutationNameImpl(Types);
			const int32 FunctionIndex = Permutations[InIndex] = Registry.Functions.Num();
			
			Registry.Functions.AddElement(
				FRigVMFunction(
					PermutationName,
					DispatchFunction,
					Factory,
					FunctionIndex,
					FunctionArguments
				)
			);
			Registry.Functions[FunctionIndex].TemplateIndex = Index;
			Registry.FunctionNameToIndex.Add(*PermutationName, FunctionIndex);

			TArray<FRigVMFunction> Predicates = Factory->CreateDispatchPredicates(Types);
			Registry.StructNameToPredicates.Add(*PermutationName, MoveTemp(Predicates));

			return &Registry.Functions[FunctionIndex];
		}
	}
	
	return nullptr;
}

bool FRigVMTemplate::ContainsPermutation(const FRigVMFunction* InPermutation) const
{
	return FindPermutation(InPermutation) != INDEX_NONE;
}

int32 FRigVMTemplate::FindPermutation(const FRigVMFunction* InPermutation) const
{
	check(InPermutation);
	return Permutations.Find(InPermutation->Index);
}

int32 FRigVMTemplate::FindPermutation(const FTypeMap& InTypes) const
{
	FTypeMap Types = InTypes;
	int32 PermutationIndex = INDEX_NONE;
	if(FullyResolve(Types, PermutationIndex))
	{
		return PermutationIndex;
	}
	return INDEX_NONE;
}

bool FRigVMTemplate::FullyResolve(FRigVMTemplate::FTypeMap& InOutTypes, int32& OutPermutationIndex) const
{
	TArray<int32> PermutationIndices;
	Resolve(InOutTypes, PermutationIndices, false);
	if(PermutationIndices.Num() == 1)
	{
		OutPermutationIndex = PermutationIndices[0];
	}
	else
	{
		OutPermutationIndex = INDEX_NONE;
	}
	return OutPermutationIndex != INDEX_NONE;
}

bool FRigVMTemplate::Resolve(FTypeMap& InOutTypes, TArray<int32>& OutPermutationIndices, bool bAllowFloatingPointCasts) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	
	FTypeMap InputTypes = InOutTypes;
	InOutTypes.Reset();

	OutPermutationIndices.Reset();
	for (int32 PermutationIndex = 0; PermutationIndex < NumPermutations(); PermutationIndex++)
	{
		OutPermutationIndices.Add(PermutationIndex);
	}
	
	for (const FRigVMTemplateArgument& Argument : Arguments)
	{
		if (Argument.IsSingleton())
		{
			InOutTypes.Add(Argument.Name, Argument.GetTypeIndex(0));
			continue;
		}
		else if (const TRigVMTypeIndex* InputType = InputTypes.Find(Argument.Name))
		{
			TRigVMTypeIndex MatchedType = *InputType;
			bool bFoundMatch = false;
			bool bFoundPerfectMatch = false;

			// Using a map to collect all permutations that we can keep/remove
			// instead of removing them one by one, which can be costly
			TMap<int32, bool> PermutationsToKeep;

			for (int32 PermutationIndex = 0; PermutationIndex < Argument.GetNumTypes(); PermutationIndex++)
			{
				const TRigVMTypeIndex Type = Argument.GetTypeIndex(PermutationIndex);
				if(!Registry.CanMatchTypes(Type, *InputType, bAllowFloatingPointCasts))
				{
					PermutationsToKeep.FindOrAdd(PermutationIndex) = false;
				}
				else
				{
					PermutationsToKeep.FindOrAdd(PermutationIndex) = true;
					bFoundMatch = true;

					// if the type matches - but it's not the exact same
					if(!bFoundPerfectMatch)
					{
						MatchedType = Type;

						// if we found the perfect match - let's stop here
						if(Type == *InputType)
						{
							bFoundPerfectMatch = true;
						}
					}
				}
			}

			OutPermutationIndices = OutPermutationIndices.FilterByPredicate([PermutationsToKeep](int32 PermutationIndex) -> bool
			{
				const bool* Value = PermutationsToKeep.Find(PermutationIndex);
				return Value ? *Value : false;
			});
			
			if(bFoundMatch)
			{
				InOutTypes.Add(Argument.Name,MatchedType);

				continue;
			}
		}

		const FRigVMTemplateArgument::EArrayType ArrayType = Argument.GetArrayType();
		if(ArrayType == FRigVMTemplateArgument::EArrayType_Mixed)
		{
			InOutTypes.Add(Argument.Name, RigVMTypeUtils::TypeIndex::WildCard);

			if(const TRigVMTypeIndex* InputType = InputTypes.Find(Argument.Name))
			{
				if(Registry.IsArrayType(*InputType))
				{
					InOutTypes.FindChecked(Argument.Name) = RigVMTypeUtils::TypeIndex::WildCardArray;
				}
			}
		}
		else if(ArrayType == FRigVMTemplateArgument::EArrayType_ArrayValue)
		{
			InOutTypes.Add(Argument.Name, RigVMTypeUtils::TypeIndex::WildCardArray);
		}
		else
		{
			InOutTypes.Add(Argument.Name, RigVMTypeUtils::TypeIndex::WildCard);
		}
	}

	if (OutPermutationIndices.Num() == 1)
	{
		InOutTypes.Reset();
		for (const FRigVMTemplateArgument& Argument : Arguments)
		{
			InOutTypes.Add(Argument.Name, Argument.GetTypeIndex(OutPermutationIndices[0]));
		}
	}
	else if (OutPermutationIndices.Num() > 1)
	{
		for (const FRigVMTemplateArgument& Argument : Arguments)
		{
			if (Argument.IsSingleton(OutPermutationIndices))
			{
				InOutTypes.FindChecked(Argument.Name) = Argument.GetTypeIndex(OutPermutationIndices[0]);
			}
		}
	}

	return !OutPermutationIndices.IsEmpty();
}

uint32 FRigVMTemplate::GetTypesHashFromTypes(const FTypeMap& InTypes) const
{
	// It is only a valid type map if it includes all arguments, and non of the types is a wildcard
	
	uint32 TypeHash = 0;
	if (InTypes.Num() != NumArguments())
	{
		return TypeHash;
	}

	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	for (const TPair<FName, TRigVMTypeIndex>& Pair : InTypes)
	{
		if (Registry.IsWildCardType(Pair.Value))
		{
			return TypeHash;
		}
	}

	for (const FRigVMTemplateArgument& Argument : Arguments)
	{
		const TRigVMTypeIndex* ArgType = InTypes.Find(Argument.Name);
		if (!ArgType)
		{
			return 0;
		}
		TypeHash = HashCombine(TypeHash, GetTypeHash(*ArgType));
	}
	return TypeHash;
}

bool FRigVMTemplate::ContainsPermutation(const FTypeMap& InTypes) const
{
	// If they type map is valid (full description of arguments), then we can rely on
	// the TypesHashToPermutation cache. Otherwise, we will have to search for a specific permutation
	// by filtering types
	const uint32 TypesHash = GetTypesHashFromTypes(InTypes);
	if (TypesHash != 0)
	{
		return TypesHashToPermutation.Contains(TypesHash);
	}
	
	TArray<int32> PossiblePermutations;
	for (const TPair<FName, TRigVMTypeIndex>& Pair : InTypes)
	{
		if (const FRigVMTemplateArgument* Argument = FindArgument(Pair.Key))
		{
			const TArray<int32>& ArgumentPermutations = Argument->GetPermutations(Pair.Value);
			if (!ArgumentPermutations.IsEmpty())
			{
				// If possible permutations is empty, initialize it
				if (PossiblePermutations.IsEmpty())
				{
					PossiblePermutations = ArgumentPermutations;
				}
				else
				{
					// Intersect possible permutations and the permutations found for this argument
					PossiblePermutations = ArgumentPermutations.FilterByPredicate([PossiblePermutations](const int32& ArgPermutation)
					{
						return PossiblePermutations.Contains(ArgPermutation);
					});
					if (PossiblePermutations.IsEmpty())
					{
						return false;
					}
				}
			}
			else
			{
				// The argument does not support the given type
				return false;
			}
		}
		else
		{
			// The argument cannot be found
			return false;
		}
	}
	
	return true;
}

bool FRigVMTemplate::ResolveArgument(const FName& InArgumentName, const TRigVMTypeIndex InTypeIndex,
	FTypeMap& InOutTypes) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();

	auto RemoveWildCardTypes = [&Registry](const FTypeMap& InTypes)
	{
		FTypeMap FilteredTypes;
		for(const FTypePair& Pair: InTypes)
		{
			if(!Registry.IsWildCardType(Pair.Value))
			{
				FilteredTypes.Add(Pair);
			}
		}
		return FilteredTypes;
	};

	// remove all wildcards from map
	InOutTypes = RemoveWildCardTypes(InOutTypes);

	// first resolve with no types given except for the new argument type
	FTypeMap ResolvedTypes;
	ResolvedTypes.Add(InArgumentName, InTypeIndex);
	TArray<int32> PermutationIndices;
	FTypeMap RemainingTypesToResolve;
	
	if(Resolve(ResolvedTypes, PermutationIndices, true))
	{
		// let's see if the input argument resolved into the expected type
		const TRigVMTypeIndex ResolvedInputType = ResolvedTypes.FindChecked(InArgumentName);
		if(!Registry.CanMatchTypes(ResolvedInputType, InTypeIndex, true))
		{
			return false;
		}
		
		ResolvedTypes = RemoveWildCardTypes(ResolvedTypes);
		
		// remove all argument types from the reference list
		// provided from the outside. we cannot resolve these further
		auto RemoveResolvedTypesFromRemainingList = [](
			FTypeMap& InOutTypes, const FTypeMap& InResolvedTypes, FTypeMap& InOutRemainingTypesToResolve)
		{
			InOutRemainingTypesToResolve = InOutTypes;
			for(const FTypePair& Pair: InOutTypes)
			{
				if(InResolvedTypes.Contains(Pair.Key))
				{
					InOutRemainingTypesToResolve.Remove(Pair.Key);
				}
			}
			InOutTypes = InResolvedTypes;
		};

		RemoveResolvedTypesFromRemainingList(InOutTypes, ResolvedTypes, RemainingTypesToResolve);

		// if the type hasn't been specified we need to slowly resolve the template
		// arguments until we hit a match. for this we'll create a list of arguments
		// to resolve and reduce the list slowly.
		bool bSuccessFullyResolvedRemainingTypes = true;
		while(!RemainingTypesToResolve.IsEmpty())
		{
			PermutationIndices.Reset();

			const FTypePair TypeToResolve = *RemainingTypesToResolve.begin();
			FTypeMap NewResolvedTypes = RemoveWildCardTypes(ResolvedTypes);
			NewResolvedTypes.FindOrAdd(TypeToResolve.Key) = TypeToResolve.Value;

			if(Resolve(NewResolvedTypes, PermutationIndices, true))
			{
				ResolvedTypes = NewResolvedTypes;
				RemoveResolvedTypesFromRemainingList(InOutTypes, ResolvedTypes, RemainingTypesToResolve);
			}
			else
			{
				// we were not able to resolve this argument, remove it from the resolved types list.
				RemainingTypesToResolve.Remove(TypeToResolve.Key);
				bSuccessFullyResolvedRemainingTypes = false;
			}
		}

		// if there is nothing left to resolve we were successful
		return RemainingTypesToResolve.IsEmpty() && bSuccessFullyResolvedRemainingTypes;
	}

	return false;
}

FRigVMTemplateTypeMap FRigVMTemplate::GetTypesForPermutation(const int32 InPermutationIndex) const
{
	FTypeMap TypeMap;
	for (int32 ArgIndex = 0; ArgIndex < NumArguments(); ++ArgIndex)
	{
		const FRigVMTemplateArgument* Argument = GetArgument(ArgIndex);
		if (Argument->GetNumTypes() > InPermutationIndex)
		{
			TypeMap.Add(Argument->GetName(), Argument->GetTypeIndex(InPermutationIndex));
		}
		else if (Argument->IsSingleton())
		{
			TypeMap.Add(Argument->GetName(), Argument->GetTypeIndex(0));
		}
		else
		{
			TypeMap.Reset();
			return TypeMap;
		}
	}
	return TypeMap;
}

#if WITH_EDITOR

FString FRigVMTemplate::GetCategory() const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		return Factory->GetCategory();
	}
	
	FString Category;
	GetPrimaryPermutation()->Struct->GetStringMetaDataHierarchical(FRigVMStruct::CategoryMetaName, &Category);

	if (Category.IsEmpty())
	{
		return Category;
	}

	for (int32 PermutationIndex = 1; PermutationIndex < NumPermutations(); PermutationIndex++)
	{
		FString OtherCategory;
		if (GetPermutation(PermutationIndex)->Struct->GetStringMetaDataHierarchical(FRigVMStruct::CategoryMetaName, &OtherCategory))
		{
			while (!OtherCategory.StartsWith(Category, ESearchCase::IgnoreCase))
			{
				FString Left;
				if (Category.Split(TEXT("|"), &Left, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					Category = Left;
				}
				else
				{
					return FString();
				}

			}
		}
	}

	return Category;
}

FString FRigVMTemplate::GetKeywords() const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		return Factory->GetKeywords();
	}

	TArray<FString> KeywordsMetadata;
	KeywordsMetadata.Add(GetName().ToString());

	for (int32 PermutationIndex = 0; PermutationIndex < NumPermutations(); PermutationIndex++)
	{
		if (const FRigVMFunction* Function = GetPermutation(PermutationIndex))
		{
			KeywordsMetadata.Add(Function->Struct->GetDisplayNameText().ToString());
			
			FString FunctionKeyWordsMetadata;
			Function->Struct->GetStringMetaDataHierarchical(FRigVMStruct::KeywordsMetaName, &FunctionKeyWordsMetadata);
			if (!FunctionKeyWordsMetadata.IsEmpty())
			{
				KeywordsMetadata.Add(FunctionKeyWordsMetadata);
			}
		}
	}

	return FString::Join(KeywordsMetadata, TEXT(","));
}

#endif

bool FRigVMTemplate::UpdateArgumentTypes()
{
	const int32 PrimaryArgumentIndex = Arguments.IndexOfByPredicate([](const FRigVMTemplateArgument& Argument)
	{
		return Argument.bUseCategories;
	});

	// this template may not be affected at all by this
	if(PrimaryArgumentIndex == INDEX_NONE)
	{
		return true;
	}

	InvalidateHash();

	for(int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ArgumentIndex++)
	{
		FRigVMTemplateArgument& Argument = Arguments[ArgumentIndex];
		if(Argument.bUseCategories || Argument.IsSingleton())
		{
			continue;
		}

		Argument.TypeIndices.Reset();
		Argument.TypeToPermutations.Reset();
	}

	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const FRigVMTemplateArgument& PrimaryArgument = Arguments[PrimaryArgumentIndex];
	TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>> TypesArray;
	bool bResult = true;

	const FRigVMDispatchFactory* Factory = nullptr;
	if(Delegates.GetDispatchFactoryDelegate.IsBound())
	{
		Factory = Delegates.GetDispatchFactoryDelegate.Execute();
		ensure(Factory);
	}

	PrimaryArgument.ForEachType([&](const TRigVMTypeIndex PrimaryTypeIndex)
	{
		TypesArray.Reset();
		if(Factory)
		{
			Factory->GetPermutationsFromArgumentType(PrimaryArgument.Name, PrimaryTypeIndex, TypesArray);
		}
		else if(OnNewArgumentType().IsBound())
		{
			FRigVMTemplateTypeMap Types = OnNewArgumentType().Execute(PrimaryArgument.Name, PrimaryTypeIndex);
			TypesArray.Add(Types);
		}

		if (!TypesArray.IsEmpty())
		{
			for (FRigVMTemplateTypeMap& Types : TypesArray)
			{
				if(Types.Num() == Arguments.Num())
				{
					for (TPair<FName, TRigVMTypeIndex>& ArgumentAndType : Types)
					{
						// similar to FRigVMTemplateArgument::EnsureValidExecuteType
						Registry.ConvertExecuteContextToBaseType(ArgumentAndType.Value);
					}

					// Find if these types were already registered
					if (ContainsPermutation(Types))
					{
						return true;
					}

					uint32 TypeHash=0;
					for(int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ArgumentIndex++)
					{
						FRigVMTemplateArgument& Argument = Arguments[ArgumentIndex];

						const TRigVMTypeIndex* TypeIndexPtr = Types.Find(Argument.Name);
						if(TypeIndexPtr == nullptr)
						{
							bResult = false;
							return true;
						}
						if(*TypeIndexPtr == INDEX_NONE)
						{
							bResult = false;
							return true;
						}

						const TRigVMTypeIndex& TypeIndex = *TypeIndexPtr;
						TypeHash = HashCombine(TypeHash, GetTypeHash(TypeIndex));

						if(Argument.bUseCategories || Argument.IsSingleton())
						{
							continue;
						}
					
						Argument.TypeIndices.Add(TypeIndex);
						Argument.TypeToPermutations.FindOrAdd(TypeIndex).Add(Permutations.Num());
					}

					Permutations.Add(INDEX_NONE);
					TypesHashToPermutation.Add(TypeHash, Permutations.Num()-1);
				}
				else
				{
					bResult = false;
				}
			}
		}
		else
		{
			bResult = false;
		}
		return true;
	});

	for(int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ArgumentIndex++)
	{
		FRigVMTemplateArgument& Argument = Arguments[ArgumentIndex];
		Argument.UpdateTypeToPermutations();
	}

	return bResult;
}

void FRigVMTemplate::HandleTypeRemoval(TRigVMTypeIndex InTypeIndex)
{
	InvalidateHash();

	TArray<int32> PermutationsToRemove;
	for (int32 PermutationIndex = 0; PermutationIndex < NumPermutations(); PermutationIndex++)
	{
		FRigVMTemplateTypeMap TypeMap = GetTypesForPermutation(PermutationIndex);

		TArray<TRigVMTypeIndex> Types;
		TypeMap.GenerateValueArray(Types);

		if (Types.Contains(InTypeIndex))
		{
			PermutationsToRemove.Add(PermutationIndex);
		}
	}

	for (FRigVMTemplateArgument& Argument : Arguments)
	{
		TArray<TRigVMTypeIndex> NewTypeIndices; 
		for (int32 PermutationIndex = 0; PermutationIndex < Argument.TypeIndices.Num(); PermutationIndex++)
		{
			if (PermutationsToRemove.Contains(PermutationIndex))
			{
				// invalidate the type index for this permutation
				Argument.TypeIndices[PermutationIndex] = INDEX_NONE;
			}
		}
	}
	
	for (FRigVMTemplateArgument& Argument : Arguments)
	{
		Argument.TypeToPermutations.Remove(InTypeIndex);
	}
}

void FRigVMTemplate::RecomputeTypesHashToPermutations()
{
	TypesHashToPermutation.Reset();
	for (int32 PermutationIndex=0; PermutationIndex<NumPermutations(); ++PermutationIndex)
	{
		uint32 TypesHash=0;
		for (int32 ArgIndex=0; ArgIndex<NumArguments(); ++ArgIndex)
		{
			TypesHash = HashCombine(Hash, GetTypeHash(Arguments[ArgIndex].GetTypeIndex(PermutationIndex)));
		}
		TypesHashToPermutation.Add(TypesHash, PermutationIndex);
	}
}

void FRigVMTemplate::UpdateTypesHashToPermutation(const int32 InPermutation)
{
	if (!Permutations.IsValidIndex(InPermutation))
	{
		return;
	}

	uint32 TypeHash=0;
	for (const FRigVMTemplateArgument& Argument : Arguments)
	{
		TypeHash = HashCombine(TypeHash, GetTypeHash(Argument.GetTypeIndex(InPermutation)));
	}
	TypesHashToPermutation.Add(TypeHash, InPermutation);
}

uint32 GetTypeHash(const FRigVMTemplateArgument& InArgument)
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	uint32 Hash = GetTypeHash(InArgument.Name.ToString());
	Hash = HashCombine(Hash, GetTypeHash((int32)InArgument.Direction));
	InArgument.ForEachType([&](const TRigVMTypeIndex TypeIndex)
	{
		Hash = HashCombine(Hash, Registry.GetHashForType(TypeIndex));
		return true;
	});
	return Hash;
}

uint32 GetTypeHash(const FRigVMTemplate& InTemplate)
{
	if(InTemplate.Hash != UINT32_MAX)
	{
		return InTemplate.Hash;
	}

	uint32 Hash = GetTypeHash(InTemplate.GetNotation().ToString());
	for(const FRigVMTemplateArgument& Argument : InTemplate.Arguments)
	{
		Hash = HashCombine(Hash, GetTypeHash(Argument));
	}

	// todo: in Dev-EngineMerge we should add the execute arguments to the hash as well

	if(const FRigVMDispatchFactory* Factory = InTemplate.GetDispatchFactory())
	{
		Hash = HashCombine(Hash, GetTypeHash(Factory->GetFactoryName().ToString()));
	}

	InTemplate.Hash = Hash;
	return Hash;
}

