// Copyright Epic Games, Inc. All Rights Reserved.


#include "Graph/SControlRigGraphChangePinType.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DetailLayoutBuilder.h"
#include "RigVMModel/RigVMController.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SControlRigGraphChangePinType"

static const FText ControlRigChangePinTypeMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

void SControlRigChangePinType::Construct(const FArguments& InArgs)
{
	this->ModelPins = InArgs._ModelPins;
	this->Blueprint = InArgs._Blueprint;

	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	BindingArgs.CurrentBindingText.BindRaw(this, &SControlRigChangePinType::GetBindingText);
	BindingArgs.CurrentBindingImage.BindRaw(this, &SControlRigChangePinType::GetBindingImage);
	BindingArgs.CurrentBindingColor.BindRaw(this, &SControlRigChangePinType::GetBindingColor);

	BindingArgs.OnCanBindProperty.BindLambda([](const FProperty* InProperty) -> bool { return true; });
	BindingArgs.OnCanBindToClass.BindLambda([](UClass* InClass) -> bool { return false; });

	BindingArgs.bGeneratePureBindings = true;
	BindingArgs.bAllowNewBindings = true;
	BindingArgs.bAllowArrayElementBindings = false;
	BindingArgs.bAllowStructMemberBindings = false;
	BindingArgs.bAllowUObjectFunctions = false;

	BindingArgs.MenuExtender = MakeShareable(new FExtender);
	BindingArgs.MenuExtender->AddMenuExtension(
		"Properties",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateSP(this, &SControlRigChangePinType::FillPinTypeMenu));

	this->ChildSlot
	[
		PropertyAccessEditor.MakePropertyBindingWidget(Blueprint, BindingArgs)
	];
}

FText SControlRigChangePinType::GetBindingText(const FRigVMTemplateArgumentType& InType)
{
	if(UObject* CPPTypeObject = InType.CPPTypeObject)
	{
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
		{
			return ScriptStruct->GetDisplayNameText();
		}
		if(const UEnum* Enum = Cast<UEnum>(CPPTypeObject))
		{
			return Enum->GetDisplayNameText();
		}
	}
	else
	{
		FString CPPType = InType.CPPType.ToString();
		if(RigVMTypeUtils::IsArrayType(CPPType))
		{
			CPPType = RigVMTypeUtils::BaseTypeFromArrayType(CPPType);
		}

		static const FText BoolLabel = LOCTEXT("BoolLabel", "Boolean");
		static const FText FloatLabel = LOCTEXT("FloatLabel", "Float");
		static const FText Int32Label = LOCTEXT("Int32Label", "Integer");
		static const FText FNameLabel = LOCTEXT("FNameLabel", "Name");
		static const FText FStringLabel = LOCTEXT("FStringLabel", "String");

		if(CPPType == RigVMTypeUtils::BoolType)
		{
			return BoolLabel;
		}
		if(CPPType == RigVMTypeUtils::FloatType || CPPType == RigVMTypeUtils::DoubleType)
		{
			return FloatLabel;
		}
		if(CPPType == RigVMTypeUtils::Int32Type)
		{
			return Int32Label;
		}
		if(CPPType == RigVMTypeUtils::FNameType)
		{
			return FNameLabel;
		}
		if(CPPType == RigVMTypeUtils::FStringType)
		{
			return FStringLabel;
		}

		return FText::FromString(CPPType);
	}
	return FText();
}

FText SControlRigChangePinType::GetBindingText(URigVMPin* ModelPin) const
{
	if (ModelPin)
	{
		return GetBindingText(ModelPin->GetTemplateArgumentType());
	}
	return FText();
}

FText SControlRigChangePinType::GetBindingText() const
{
	if (ModelPins.Num() > 0)
	{
		const FText FirstText = GetBindingText(ModelPins[0]);
		for(int32 Index = 1; Index < ModelPins.Num(); Index++)
		{
			if(!GetBindingText(ModelPins[Index]).EqualTo(FirstText))
			{
				return ControlRigChangePinTypeMultipleValues;
			}
		}
		return FirstText;
	}
	return FText();
}

const FSlateBrush* SControlRigChangePinType::GetBindingImage() const
{
	static FName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
	static FName ArrayTypeIcon(TEXT("Kismet.VariableList.ArrayTypeIcon"));

	if (ModelPins.Num() > 0)
	{
		if(ModelPins[0]->IsArray())
		{
			return FAppStyle::GetBrush(ArrayTypeIcon);
		}
	}
	return FAppStyle::GetBrush(TypeIcon);
}

FLinearColor SControlRigChangePinType::GetBindingColor() const
{
	if(ModelPins.Num() > 0)
	{
		const FRigVMTemplateArgumentType Type = ModelPins[0]->GetTemplateArgumentType();
		const FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromCPPType(Type.CPPType, Type.CPPTypeObject);
		const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();
		return Schema->GetPinTypeColor(PinType);
	}
	return FLinearColor::White;
}

void SControlRigChangePinType::FillPinTypeMenu(FMenuBuilder& MenuBuilder)
{
	if(ModelPins.Num() == 0)
	{
		return;
	}

	URigVMGraph* Model = ModelPins[0]->GetGraph();
	if(Model == nullptr)
	{
		return;
	}

	struct FArgumentInfo
	{
		static FArgumentInfo Make(const FProperty* InProperty, const bool bInIsFilteredOut)
		{
			FArgumentInfo Info;
			Info.Property = InProperty;
			Info.bIsFilteredOut = bInIsFilteredOut;
			return Info;
		}

		static FArgumentInfo Make(const FRigVMTemplateArgumentType& InType, const bool bInIsFilteredOut)
		{
			FArgumentInfo Info;
			Info.Property = nullptr;
			Info.Type = InType;
			Info.bIsFilteredOut = bInIsFilteredOut;
			return Info;
		}

		const FProperty* Property;
		bool bIsFilteredOut;
		FRigVMTemplateArgumentType Type;
	};

	typedef TPair<TRigVMTypeIndex, FArgumentInfo> FTypePair;
	TMap<TRigVMTypeIndex, FArgumentInfo> Types;
	for(URigVMPin* ModelPin : ModelPins)
	{
		if(!ModelPin->IsRootPin())
		{
			continue;
		}
		
		if(const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(ModelPin->GetNode()))
		{
			if(const FRigVMTemplate* Template = TemplateNode->GetTemplate())
			{
				const FName ArgumentName = ModelPin->GetFName();
				if(const FRigVMTemplateArgument* Argument = Template->FindArgument(ArgumentName))
				{
					const TArray<TRigVMTypeIndex>& AllArgumentTypes = Argument->GetTypeIndices();
					const TArray<TRigVMTypeIndex>& FilteredArgumentTypes = Argument->GetSupportedTypeIndices(TemplateNode->GetFilteredPermutationsIndices());
					
					for(int32 PermutationIndex = 0; PermutationIndex < Template->NumPermutations(); PermutationIndex++)
					{
						TRigVMTypeIndex ArgumentTypeIndex = AllArgumentTypes[PermutationIndex];

						if (ArgumentTypeIndex == INDEX_NONE)
						{
							// skip invalid permutations
							continue;
						}

						bool bIsFilteredOut = false;
						if(!FilteredArgumentTypes.Contains(ArgumentTypeIndex))
						{
							bIsFilteredOut = true;
						}

						if(ArgumentTypeIndex == RigVMTypeUtils::TypeIndex::Float)
						{
							ArgumentTypeIndex = RigVMTypeUtils::TypeIndex::Double;
						}
						else if(ArgumentTypeIndex == RigVMTypeUtils::TypeIndex::FloatArray)
						{
							ArgumentTypeIndex = RigVMTypeUtils::TypeIndex::DoubleArray;
						}
						
						if(Types.Contains(ArgumentTypeIndex))
						{
							continue;
						}

						if(const FRigVMFunction* Permutation = Template->GetPermutation(PermutationIndex))
						{
							if(UScriptStruct* FunctionStruct = Permutation->Struct)
							{
								if(const FProperty* Property = FunctionStruct->FindPropertyByName(ArgumentName))
								{
									Types.Add(ArgumentTypeIndex, FArgumentInfo::Make(Property, bIsFilteredOut));
									continue;
								}
							}
						}
						
						const FRigVMTemplateArgumentType ArgumentType = FRigVMRegistry::Get().GetType(ArgumentTypeIndex);
						Types.Add(ArgumentTypeIndex, FArgumentInfo::Make(ArgumentType, bIsFilteredOut));
					}
				}
			}
		}
	}

	// sort the types and put them into an array
	static const TArray<FString> SortOrder = {
		TEXT("bool"),
		TEXT("int32"),
		TEXT("float"),
		TEXT("double"),
		TEXT("FName"),
		TEXT("FString"),
		TEXT("FVector"),
		TEXT("FRotator"),
		TEXT("FQuat"),
		TEXT("FTransform"),
		TEXT("FEulerTransform"),
		TEXT("FMatrix")
	};
	TArray<FTypePair> SortedTypes;
	for(const FTypePair& Pair : Types)
	{
		SortedTypes.Add(Pair);
	}
	SortedTypes.Sort([](const FTypePair& A, const FTypePair& B) -> bool
	{
		const FRigVMTemplateArgumentType TypeA = FRigVMRegistry::Get().GetType(A.Key);
		const FRigVMTemplateArgumentType TypeB = FRigVMRegistry::Get().GetType(B.Key);
		const FString BaseTypeA = TypeA.GetBaseCPPType();
		const FString BaseTypeB = TypeB.GetBaseCPPType();

		const int32 IndexA = SortOrder.Find(BaseTypeA);  
		const int32 IndexB = SortOrder.Find(BaseTypeB);
		if((IndexA == INDEX_NONE) != (IndexB == INDEX_NONE))
		{
			return IndexA != INDEX_NONE;
		}
		else if((IndexA != INDEX_NONE) && (IndexB != INDEX_NONE))
		{
			return IndexA < IndexB;
		}

		return BaseTypeA.Compare(BaseTypeB) < 0;
	});
	
	MenuBuilder.BeginSection("PinTypes", LOCTEXT("PinTypes", "Pin Types"));
	{
		static FName PropertyIcon(TEXT("Kismet.VariableList.TypeIcon"));
		const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();

		const bool bHasAllTypes =
			SortedTypes.Num() >=
				FRigVMRegistry::Get().GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue).Num();
		
		for(int32 SortedIndex=0; SortedIndex < SortedTypes.Num(); SortedIndex++)
		{
			const TRigVMTypeIndex& TypeIndex = SortedTypes[SortedIndex].Key;
			const FRigVMTemplateArgumentType Type = FRigVMRegistry::Get().GetType(TypeIndex);
			const bool bIsFilteredOut = SortedTypes[SortedIndex].Value.bIsFilteredOut;
			if (Type.CPPTypeObject != nullptr && !IsValid(Type.CPPTypeObject))
			{
				continue;
			}
			
			const FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromCPPType(Type.CPPType, Type.CPPTypeObject);

			if(bHasAllTypes && Type.CPPTypeObject)
			{
				if(Type.CPPTypeObject->IsA<UEnum>() || Type.CPPTypeObject->IsA<UClass>())
				{
					continue;
				}

				if(UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Type.CPPTypeObject))
				{
					const FRigVMTemplateArgumentType MathType(*RigVMTypeUtils::GetUniqueStructTypeName(ScriptStruct), ScriptStruct);
					const TRigVMTypeIndex MathTypeIndex = FRigVMRegistry::Get().GetTypeIndex(MathType);
					
					if(!FRigVMRegistry::Get().GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue).Contains(MathTypeIndex))
					{
						continue;
					}
				}
			}

			if(UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Type.CPPTypeObject))
			{
				if(ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
				{
					continue;
				}
			}

			MenuBuilder.AddMenuEntry(
				FUIAction(FExecuteAction::CreateSP(this, &SControlRigChangePinType::HandlePinTypeChanged, Type)),
				SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSpacer)
						.Size(FVector2D(18.0f, 0.0f))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(1.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FBlueprintEditorUtils::GetIconFromPin(PinType, true))
						.ColorAndOpacity(Schema->GetPinTypeColor(PinType) * (bIsFilteredOut ? 0.5f : 1.f))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(GetBindingText(Type))
						.ColorAndOpacity(FLinearColor::White * (bIsFilteredOut ? 0.5f : 1.f))
						.ToolTipText(bIsFilteredOut ? LOCTEXT("WildcardAvailableTypeTooltip","Will break connections if resolved to this type.") :
							LOCTEXT("WildcardAvailableFilterTypeTooltip", "Available filtered type"))
					]);
		}
	}
	MenuBuilder.EndSection(); // Local Variables
}

void SControlRigChangePinType::HandlePinTypeChanged(FRigVMTemplateArgumentType InType)
{
	if(ModelPins.IsEmpty() || (Blueprint == nullptr))
	{
		return;
	}

	for(URigVMPin* ModelPin : ModelPins)
	{
		URigVMGraph* Model = ModelPin->GetGraph();
		if(Model == nullptr)
		{
			continue;
		}

		URigVMController* Controller = Blueprint->GetOrCreateController(Model);
		if(Controller == nullptr)
		{
			continue;
		}

		const TRigVMTypeIndex TypeIndex = FRigVMRegistry::Get().GetTypeIndex(InType);
		Controller->ResolveWildCardPin(ModelPin->GetPinPath(), TypeIndex, true, true);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SControlRigGraphChangePinType::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	this->ModelPins = InArgs._ModelPins;
	this->Blueprint = InArgs._Blueprint;

	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SControlRigGraphChangePinType::GetDefaultValueWidget()
{
	return SNew(SControlRigChangePinType)
		.Blueprint(Blueprint)
		.ModelPins(ModelPins);
}

#undef LOCTEXT_NAMESPACE
