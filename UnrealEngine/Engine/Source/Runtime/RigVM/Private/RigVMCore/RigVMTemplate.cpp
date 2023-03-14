// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMTemplate.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMModule.h"
#include "Algo/Sort.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMTemplate)

////////////////////////////////////////////////////////////////////////////////////////////////

FRigVMTemplateArgument::FRigVMTemplateArgument()
	: Index(INDEX_NONE)
	, Name(NAME_None)
	, Direction(ERigVMPinDirection::IO)
{
}

FRigVMTemplateArgument::FRigVMTemplateArgument(FProperty* InProperty)
	: Index(INDEX_NONE)
	, Name(InProperty->GetFName())
	, Direction(ERigVMPinDirection::IO)
{
#if WITH_EDITOR
	Direction = FRigVMStruct::GetPinDirectionFromProperty(InProperty);
#endif

	FString ExtendedType;
	const FString CPPType = InProperty->GetCPPType(&ExtendedType);
	const FName CPPTypeName = *(CPPType + ExtendedType);
	FRigVMTemplateArgumentType Type(CPPTypeName);

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		InProperty = ArrayProperty->Inner;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		Type.CPPTypeObject = StructProperty->Struct;
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		Type.CPPTypeObject = EnumProperty->GetEnum();
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		Type.CPPTypeObject = ByteProperty->Enum;
	}
	Type.CPPType = *RigVMTypeUtils::PostProcessCPPType(Type.CPPType.ToString(), Type.CPPTypeObject);

	const TRigVMTypeIndex TypeIndex = FRigVMRegistry::Get().FindOrAddType_Internal(Type, true); 

	TypeIndices.Add(TypeIndex);
	EnsureValidExecuteType();
	UpdateTypeToPermutations();
}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InTypeIndex)
: Index(INDEX_NONE)
, Name(InName)
, Direction(InDirection)
, TypeIndices({InTypeIndex})
{
	EnsureValidExecuteType();
	UpdateTypeToPermutations();
}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<TRigVMTypeIndex>& InTypeIndices)
: Index(INDEX_NONE)
, Name(InName)
, Direction(InDirection)
, TypeIndices(InTypeIndices)
{
	check(TypeIndices.Num() > 0);

	EnsureValidExecuteType();
	UpdateTypeToPermutations();
}

FRigVMTemplateArgument::FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<ETypeCategory>& InTypeCategories, const FTypeFilter& InTypeFilter)
: Index(INDEX_NONE)
, Name(InName)
, Direction(InDirection)
, TypeCategories(InTypeCategories)
{
	TSet<TRigVMTypeIndex> AllTypes;
	for(ETypeCategory TypeCategory : TypeCategories)
	{
		AllTypes.Append(FRigVMRegistry::Get().GetTypesForCategory(TypeCategory));
	}
	
	TypeIndices.Reserve(AllTypes.Num());
	for (const TRigVMTypeIndex& Type : AllTypes)
	{
		if (InTypeFilter.IsBound() && !InTypeFilter.Execute(Type))
		{
			continue;
		}
		
		TypeIndices.Add(Type);
	}

	EnsureValidExecuteType();
	UpdateTypeToPermutations();
}

void FRigVMTemplateArgument::Serialize(FArchive& Ar)
{
	Ar << Index;
	Ar << Name;
	Ar << Direction;

#if UE_RIGVM_DEBUG_TYPEINDEX
	if (Ar.IsSaving())
	{
		TArray<int32> TypeIndicesInt;
		TypeIndicesInt.Reserve(TypeIndices.Num());
		for (TRigVMTypeIndex& TypeIndex : TypeIndices)
		{
			TypeIndicesInt.Add(TypeIndex.GetIndex());
		}
		Ar << TypeIndicesInt;
	}
	else if (Ar.IsLoading())
	{
		TArray<int32> TypeIndicesInt;
		Ar << TypeIndicesInt;
		TypeIndices.Reset();
		TypeIndices.SetNum(TypeIndicesInt.Num());
		static FRigVMRegistry& Registry = FRigVMRegistry::Get();
		for (int32 i=0; i<TypeIndicesInt.Num(); ++i)
		{
			TypeIndices[i].Index = TypeIndicesInt[i];
			TypeIndices[i].Name = Registry.GetType(TypeIndices[i]).CPPType;
		}
	}
#else
	Ar << TypeIndices;
#endif

	if (Ar.IsLoading())
	{
		for (int32 i=0; i<TypeIndices.Num(); ++i)
		{
			if (TArray<int32>* Permutations = TypeToPermutations.Find(TypeIndices[i]))
			{
				Permutations->Add(i);
			}
			else
			{
				TypeToPermutations.Add(TypeIndices[i], {i});
			}
		}
	}

	if (Ar.IsSaving())
	{
		int32 CategoriesNum = TypeCategories.Num();
		Ar << CategoriesNum;
		for (int32 i=0; i<CategoriesNum; ++i)
		{
			uint8 CategoryIndex = (int8)TypeCategories[i]; 
			Ar << CategoryIndex;
		}
	}
	else if (Ar.IsLoading())
	{
		int32 CategoriesNum = 0;
		Ar << CategoriesNum;
		TypeCategories.SetNumUninitialized(CategoriesNum);
		
		for (int32 i=0; i<CategoriesNum; ++i)
		{
			uint8 CategoryIndex = INDEX_NONE;
			Ar << CategoryIndex;
			TypeCategories[i] = (FRigVMTemplateArgument::ETypeCategory) CategoryIndex;
		}
	}	
}

void FRigVMTemplateArgument::EnsureValidExecuteType()
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	for(TRigVMTypeIndex& TypeIndex : TypeIndices)
	{
		Registry.ConvertExecuteContextToBaseType(TypeIndex);
	}
}

void FRigVMTemplateArgument::UpdateTypeToPermutations()
{
	TypeToPermutations.Reset();
	for(int32 TypeIndex=0;TypeIndex<TypeIndices.Num();TypeIndex++)
	{
		if (TArray<int32>* Permutations = TypeToPermutations.Find(TypeIndices[TypeIndex]))
		{
			Permutations->Add(TypeIndex);
		}
		else
		{
			TypeToPermutations.Add(TypeIndices[TypeIndex], {TypeIndex});
		}
	}
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
	
	if (const TArray<int32>* Permutations = TypeToPermutations.Find(InTypeIndex))
	{
		if(OutTypeIndex)
		{
			(*OutTypeIndex) = TypeIndices[(*Permutations)[0]];
		}
		return true;		
	}

	// Try to find compatible type
	const TArray<TRigVMTypeIndex>& CompatibleTypes = Registry.GetCompatibleTypes(InTypeIndex);
	for (const TRigVMTypeIndex& CompatibleTypeIndex : CompatibleTypes)
	{
		if (const TArray<int32>* Permutations = TypeToPermutations.Find(CompatibleTypeIndex))
		{
			if(OutTypeIndex)
			{
				(*OutTypeIndex) = TypeIndices[(*Permutations)[0]];
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

	const TRigVMTypeIndex TRigVMTypeIndex = TypeIndices[InPermutationIndices[0]];
	for (int32 PermutationIndex = 1; PermutationIndex < InPermutationIndices.Num(); PermutationIndex++)
	{
		if (TypeIndices[InPermutationIndices[PermutationIndex]] != TRigVMTypeIndex)
		{
			return false;
		}
	}
	return true;
}

bool FRigVMTemplateArgument::IsExecute() const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	for (int32 PermutationIndex = 0; PermutationIndex < TypeIndices.Num(); PermutationIndex++)
	{
		if (!Registry.IsExecuteType(TypeIndices[PermutationIndex]))
		{
			return false;
		}
	}
	return true;
}

FRigVMTemplateArgument::EArrayType FRigVMTemplateArgument::GetArrayType() const
{
	if(TypeIndices.Num() > 0)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		const EArrayType ArrayType = Registry.IsArrayType(TypeIndices[0]) ? EArrayType::EArrayType_ArrayValue : EArrayType::EArrayType_SingleValue;
		
		if(IsSingleton())
		{
			return ArrayType;
		}

		for(int32 PermutationIndex=1;PermutationIndex<TypeIndices.Num();PermutationIndex++)
		{
			// INDEX_NONE indicates deleted permutation
			if (TypeIndices[PermutationIndex] == INDEX_NONE)
			{
				continue;
			}
			
			const EArrayType OtherArrayType = Registry.IsArrayType(TypeIndices[PermutationIndex]) ? EArrayType::EArrayType_ArrayValue : EArrayType::EArrayType_SingleValue;
			if(OtherArrayType != ArrayType)
			{
				return EArrayType::EArrayType_Mixed;
			}
		}

		return ArrayType;
	}

	return EArrayType_Invalid;
}

const TArray<TRigVMTypeIndex>& FRigVMTemplateArgument::GetTypeIndices() const
{
	return TypeIndices;
}

TArray<TRigVMTypeIndex> FRigVMTemplateArgument::GetSupportedTypeIndices(const TArray<int32>& InPermutationIndices) const
{
	TArray<TRigVMTypeIndex> SupportedTypes;
	if(InPermutationIndices.IsEmpty())
	{
		for (const TRigVMTypeIndex& TypeIndex : TypeIndices)
		{
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex != INDEX_NONE)
			{
				SupportedTypes.AddUnique(TypeIndex);
			}
		}
	}
	else
	{
		for(const int32 PermutationIndex : InPermutationIndices)
		{
			// INDEX_NONE indicates deleted permutation
			if (TypeIndices[PermutationIndex] != INDEX_NONE)
			{
				SupportedTypes.AddUnique(TypeIndices[PermutationIndex]);
			}
		}
	}
	return SupportedTypes;
}

#if WITH_EDITOR

TArray<FString> FRigVMTemplateArgument::GetSupportedTypeStrings(const TArray<int32>& InPermutationIndices) const
{
	TArray<FString> SupportedTypes;
	if(InPermutationIndices.IsEmpty())
	{
		for (const TRigVMTypeIndex& TypeIndex : TypeIndices)
		{
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex != INDEX_NONE)
			{
				const FString TypeString = FRigVMRegistry::Get().GetType(TypeIndex).CPPType.ToString();
				SupportedTypes.AddUnique(TypeString);
			}
		}
	}
	else
	{
		for(const int32 PermutationIndex : InPermutationIndices)
		{
			const TRigVMTypeIndex TypeIndex = TypeIndices[PermutationIndex];
			// INDEX_NONE indicates deleted permutation
			if (TypeIndex != INDEX_NONE)
			{
				const FString TypeString = FRigVMRegistry::Get().GetType(TypeIndex).CPPType.ToString();
				SupportedTypes.AddUnique(TypeString);
			}
		}
	}
	return SupportedTypes;
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////

FRigVMTemplate::FRigVMTemplate()
	: Index(INDEX_NONE)
	, Notation(NAME_None)
{

}

void FRigVMTemplate::Serialize(FArchive& Ar)
{
	Ar << Index;
	Ar << Notation;
	Ar << Permutations;

	if (Ar.IsSaving())
	{
		int32 ArgumentsNum = Arguments.Num();
		Ar << ArgumentsNum;

		for (FRigVMTemplateArgument& Argument : Arguments)
		{
			Argument.Serialize(Ar);
		}
	}
	else if(Ar.IsLoading())
	{
		int32 ArgumentsNum = 0;
		Ar << ArgumentsNum;

		Arguments.Reset();
		Arguments.Reserve(ArgumentsNum);
		for (int32 i=0; i<ArgumentsNum; ++i)
		{
			FRigVMTemplateArgument Argument;
			Argument.Serialize(Ar);
			Arguments.Add(Argument);
		}
	}	
}

FRigVMTemplate::FRigVMTemplate(UScriptStruct* InStruct, const FString& InTemplateName, int32 InFunctionIndex)
	: Index(INDEX_NONE)
	, Notation(NAME_None)
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

			if(IsValidArgumentForTemplate(Argument) && Argument.GetDirection() != ERigVMPinDirection::Hidden)
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
			if(Argument->GetDirection() != ERigVMPinDirection::Hidden)
			{
				ArgumentNotations.Add(GetArgumentNotation(*Argument));
			}
		}
	}

	if (ArgumentNotations.Num() > 0)
	{
		const FString NotationStr = FString::Printf(TEXT("%s(%s)"), *InTemplateName, *FString::Join(ArgumentNotations, TEXT(",")));
		Notation = *NotationStr;
		Permutations.Add(InFunctionIndex);
	}
}

FRigVMTemplate::FRigVMTemplate(const FName& InTemplateName, const TArray<FRigVMTemplateArgument>& InArguments, int32 InFunctionIndex)
	: Index(INDEX_NONE)
	, Notation(NAME_None)
{
	TArray<FString> ArgumentNotations;
	for (const FRigVMTemplateArgument& InArgument : InArguments)
	{
		FRigVMTemplateArgument Argument = InArgument;
		Argument.Index = Arguments.Num();

		if(IsValidArgumentForTemplate(Argument))
		{
			Arguments.Add(Argument);
			if(Argument.GetDirection() != ERigVMPinDirection::Hidden)
			{
				ArgumentNotations.Add(GetArgumentNotation(Argument));
			}
		}
	}

	if (ArgumentNotations.Num() > 0)
	{
		const FString NotationStr = FString::Printf(TEXT("%s(%s)"), *InTemplateName.ToString(), *FString::Join(ArgumentNotations, TEXT(",")));
		Notation = *NotationStr;
		Permutations.Add(InFunctionIndex);
	}
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

bool FRigVMTemplate::IsValidArgumentForTemplate(const FRigVMTemplateArgument& InArgument)
{
	static const TArray<ERigVMPinDirection> ValidDirections = {
		ERigVMPinDirection::Input,
		ERigVMPinDirection::Output,
		ERigVMPinDirection::IO,
		ERigVMPinDirection::Hidden,
		ERigVMPinDirection::Visible
	};

	if(!ValidDirections.Contains(InArgument.Direction))
	{
		return false;
	}
	return true;
}


const FString& FRigVMTemplate::GetArgumentNotationPrefix(const FRigVMTemplateArgument& InArgument)
{
	static const FString EmptyPrefix = FString();
	static const FString InPrefix = TEXT("in ");
	static const FString OutPrefix = TEXT("out ");
	static const FString IOPrefix = TEXT("io ");

	switch(InArgument.Direction)
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

FString FRigVMTemplate::GetArgumentNotation(const FRigVMTemplateArgument& InArgument)
{
	return FString::Printf(TEXT("%s%s"),
		*GetArgumentNotationPrefix(InArgument),
		*InArgument.GetName().ToString());
}

void FRigVMTemplate::ComputeNotationFromArguments(const FString& InTemplateName)
{
	TArray<FString> ArgumentNotations;			
	for (FRigVMTemplateArgument& Argument : Arguments)
	{
		if(FRigVMTemplate::IsValidArgumentForTemplate(Argument))
		{
			ArgumentNotations.Add(FRigVMTemplate::GetArgumentNotation(Argument));
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

FRigVMTemplate::FTypeMap FRigVMTemplate::GetArgumentTypesFromString(const FString& InTypeString) const
{
	FTypeMap Types;
	if(!InTypeString.IsEmpty())
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();

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
					const TRigVMTypeIndex TypeIndex = Registry.GetTypeIndexFromCPPType(TypeName);
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

	return ResolvedTooltipText;
}

FText FRigVMTemplate::GetDisplayNameForArgument(const FName& InArgumentName, const TArray<int32>& InPermutationIndices) const
{
	if(const FRigVMDispatchFactory* Factory = GetDispatchFactory())
	{
		return Factory->GetDisplayNameForArgument(InArgumentName);
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

bool FRigVMTemplate::IsCompatible(const FRigVMTemplate& InOther) const
{
	if (!IsValid() || !InOther.IsValid())
	{
		return false;
	}

	return Notation == InOther.Notation;
}

bool FRigVMTemplate::Merge(const FRigVMTemplate& InOther)
{
	if (!IsCompatible(InOther))
	{
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
			if(Arguments[ArgumentIndex].GetTypeIndices()[PermutationIndex] ==
				InOther.Arguments[ArgumentIndex].TypeIndices[0])
			{
				MatchingArguments++;
			}
		}
		if(MatchingArguments == Arguments.Num())
		{
			// find the previously defined permutation.
			UE_LOG(LogRigVM, Display, TEXT("RigVMFunction '%s' cannot be merged into the '%s' template. It collides with '%s'."),
				*InOther.GetPermutation(0)->Name,
				*GetNotation().ToString(),
				*GetPermutation(PermutationIndex)->Name);
			return false;
		}
	}

	TArray<FRigVMTemplateArgument> NewArgs;

	for (int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ArgumentIndex++)
	{
		if (InOther.Arguments[ArgumentIndex].TypeIndices.Num() != 1)
		{
			return false;
		}

		NewArgs.Add(Arguments[ArgumentIndex]);

		// Add Other argument information into the TypeToPermutations map
		{
			const TRigVMTypeIndex OtherTypeIndex = InOther.Arguments[ArgumentIndex].TypeIndices[0];
			const int32 NewPermutationIndex = NewArgs[ArgumentIndex].TypeIndices.Num();
			if (TArray<int32>* ArgTypePermutations = NewArgs[ArgumentIndex].TypeToPermutations.Find(OtherTypeIndex))
			{
				ArgTypePermutations->Add(NewPermutationIndex);
			}
			else
			{
				NewArgs[ArgumentIndex].TypeToPermutations.Add(OtherTypeIndex, {NewPermutationIndex});
			}
		}
		NewArgs[ArgumentIndex].TypeIndices.Add(InOther.Arguments[ArgumentIndex].TypeIndices[0]);
	}

	Arguments = NewArgs;

	Permutations.Add(InOther.Permutations[0]);
	return true;
}

const FRigVMTemplateArgument* FRigVMTemplate::FindArgument(const FName& InArgumentName) const
{
	return Arguments.FindByPredicate([InArgumentName](const FRigVMTemplateArgument& Argument) -> bool
	{
		return Argument.GetName() == InArgumentName;
	});
}

bool FRigVMTemplate::ArgumentSupportsTypeIndex(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex) const
{
	if (const FRigVMTemplateArgument* Argument = FindArgument(InArgumentName))
	{
		return Argument->SupportsTypeIndex(InTypeIndex, OutTypeIndex);		
	}
	return false;
}

const FRigVMFunction* FRigVMTemplate::GetPermutation(int32 InIndex) const
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
	if(const FRigVMFunction* Function = GetPermutation(InIndex))
	{
		return Function;
	}
	
	if(Permutations[InIndex] == INDEX_NONE && UsesDispatch())
	{
		FRigVMRegistry& Registry = FRigVMRegistry::Get();
		
		FTypeMap Types;
		for(const FRigVMTemplateArgument& Argument : Arguments)
		{
			Types.Add(Argument.GetName(), Argument.TypeIndices[InIndex]);
		}
		
		if(const FRigVMFunctionPtr DispatchFunction = Delegates.RequestDispatchFunctionDelegate.Execute(this, Types))
		{
			FRigVMDispatchFactory* Factory = Delegates.GetDispatchFactoryDelegate.Execute();
			check(Factory);

			TArray<FRigVMFunctionArgument> FunctionArguments;
			for(const FRigVMTemplateArgument& Argument : Arguments)
			{
				const FRigVMTemplateArgumentType& Type = Registry.GetType(Argument.TypeIndices[InIndex]);
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
	for (int32 PermutationIndex = 0; PermutationIndex < Permutations.Num(); PermutationIndex++)
	{
		OutPermutationIndices.Add(PermutationIndex);
	}
	
	for (const FRigVMTemplateArgument& Argument : Arguments)
	{
		if (Argument.IsSingleton())
		{
			InOutTypes.Add(Argument.Name, Argument.TypeIndices[0]);
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
			
			for (int32 PermutationIndex = 0; PermutationIndex < Argument.TypeIndices.Num(); PermutationIndex++)
			{
				if(!Registry.CanMatchTypes(Argument.TypeIndices[PermutationIndex], *InputType, bAllowFloatingPointCasts))
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
						MatchedType = Argument.TypeIndices[PermutationIndex];

						// if we found the perfect match - let's stop here
						if(Argument.TypeIndices[PermutationIndex] == *InputType)
						{
							bFoundPerfectMatch = true;
						}
					}
				}
			}

			OutPermutationIndices = OutPermutationIndices.FilterByPredicate([PermutationsToKeep](int32 PermutationIndex) -> bool
			{
				return PermutationsToKeep[PermutationIndex];
			});
			
			if(bFoundMatch)
			{
				InOutTypes.Add(Argument.Name,MatchedType);

				// if we found a perfect match - remove all permutations which don't match this one
				if(bFoundPerfectMatch)
				{
					const TArray<int32> BeforePermutationIndices = OutPermutationIndices;
					OutPermutationIndices.RemoveAll([Argument, MatchedType](int32 PermutationIndex) -> bool
					{
						return Argument.TypeIndices[PermutationIndex] != MatchedType;
					});
					if (OutPermutationIndices.IsEmpty() && bAllowFloatingPointCasts)
					{
						OutPermutationIndices = BeforePermutationIndices;
						OutPermutationIndices.RemoveAll([Argument, MatchedType, InputType, &Registry](int32 PermutationIndex) -> bool
						{
							return !Registry.CanMatchTypes(Argument.TypeIndices[PermutationIndex], *InputType, true);
						});
					}
				}
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
			InOutTypes.Add(Argument.Name, Argument.TypeIndices[OutPermutationIndices[0]]);
		}
	}
	else if (OutPermutationIndices.Num() > 1)
	{
		for (const FRigVMTemplateArgument& Argument : Arguments)
		{
			if (Argument.IsSingleton(OutPermutationIndices))
			{
				InOutTypes.FindChecked(Argument.Name) = Argument.TypeIndices[OutPermutationIndices[0]];
			}
		}
	}

	return !OutPermutationIndices.IsEmpty();
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
		if (Argument->GetTypeIndices().Num() > InPermutationIndex)
		{
			TypeMap.Add(Argument->GetName(), Argument->GetTypeIndices()[InPermutationIndex]);
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
	GetPermutation(0)->Struct->GetStringMetaDataHierarchical(FRigVMStruct::CategoryMetaName, &Category);

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

bool FRigVMTemplate::AddTypeForArgument(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex)
{
	if(OnNewArgumentType().IsBound())
	{
		FRigVMTemplateTypeMap Types = OnNewArgumentType().Execute(this, InArgumentName, InTypeIndex);
		if(Types.Num() == Arguments.Num())
		{
			const FRigVMRegistry& Registry = FRigVMRegistry::Get();
			for (TPair<FName, TRigVMTypeIndex>& ArgumentAndType : Types)
			{
				// similar to FRigVMTemplateArgument::EnsureValidExecuteType
				Registry.ConvertExecuteContextToBaseType(ArgumentAndType.Value);
			}
			
			// make sure this permutation doesn't exist yet
			FRigVMTemplateTypeMap TempTypes = Types;
			TArray<int32> ExistingPermutations;
			if(Resolve(TempTypes, ExistingPermutations, false))
			{
				if(ExistingPermutations.Num() == 1)
				{
					return false;
				}
			}
			
			for(FRigVMTemplateArgument& Argument : Arguments)
			{
				const TRigVMTypeIndex* TypeIndex = Types.Find(Argument.Name);
				if(TypeIndex == nullptr)
				{
					return false;
				}
				if(*TypeIndex == INDEX_NONE)
				{
					return false;
				}
			}

			// Find if these types were already registered
			FRigVMTemplateTypeMap TestTypes = Types;
			TArray<int32> TestPermutations;
			if (Resolve(TestTypes, TestPermutations, false))
			{
				return false;
			}
			
			for(FRigVMTemplateArgument& Argument : Arguments)
			{
				const TRigVMTypeIndex TypeIndex = Types.FindChecked(Argument.Name);
				Argument.TypeIndices.Add(TypeIndex);
				Argument.TypeToPermutations.FindOrAdd(TypeIndex).Add(Permutations.Num());
			}

			Permutations.Add(INDEX_NONE);
			return true;
		}
	}
	return false;
}

void FRigVMTemplate::HandleTypeRemoval(TRigVMTypeIndex InTypeIndex)
{
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


