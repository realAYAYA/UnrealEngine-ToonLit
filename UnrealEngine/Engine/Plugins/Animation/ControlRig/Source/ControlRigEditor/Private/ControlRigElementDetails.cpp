// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigElementDetails.h"
#include "Widgets/SWidget.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "ControlRigBlueprint.h"
#include "Graph/ControlRigGraph.h"
#include "PropertyCustomizationHelpers.h"
#include "SEnumCombo.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Graph/SControlRigGraphPinVariableBinding.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Editor/SRigHierarchyTreeView.h"

#define LOCTEXT_NAMESPACE "ControlRigElementDetails"

static const FText ControlRigDetailsMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

struct FRigElementTransformWidgetSettings
{
	FRigElementTransformWidgetSettings()
	: RotationRepresentation(MakeShareable(new ESlateRotationRepresentation::Type(ESlateRotationRepresentation::Rotator)))
	, IsComponentRelative(MakeShareable(new UE::Math::TVector<float>(1.f, 1.f, 1.f)))
	, IsScaleLocked(TSharedPtr<bool>(new bool(false)))
	{
	}

	TSharedPtr<ESlateRotationRepresentation::Type> RotationRepresentation;
	TSharedRef<UE::Math::TVector<float>> IsComponentRelative;
	TSharedPtr<bool> IsScaleLocked;

	static FRigElementTransformWidgetSettings& FindOrAdd(
		ERigControlValueType InValueType,
		ERigTransformElementDetailsTransform::Type InTransformType,
		const SAdvancedTransformInputBox<FEulerTransform>::FArguments& WidgetArgs)
	{
		uint32 Hash = GetTypeHash(WidgetArgs._ConstructLocation);
		Hash = HashCombine(Hash, GetTypeHash(WidgetArgs._ConstructRotation));
		Hash = HashCombine(Hash, GetTypeHash(WidgetArgs._ConstructScale));
		Hash = HashCombine(Hash, GetTypeHash(WidgetArgs._AllowEditRotationRepresentation));
		Hash = HashCombine(Hash, GetTypeHash(WidgetArgs._DisplayScaleLock));
		Hash = HashCombine(Hash, GetTypeHash(InValueType));
		Hash = HashCombine(Hash, GetTypeHash(InTransformType));
		return sSettings.FindOrAdd(Hash);
	}

	static TMap<uint32, FRigElementTransformWidgetSettings> sSettings;
};

TMap<uint32, FRigElementTransformWidgetSettings> FRigElementTransformWidgetSettings::sSettings;


namespace FRigElementKeyDetailsDefs
{
	// Active foreground pin alpha
	static const float ActivePinForegroundAlpha = 1.f;
	// InActive foreground pin alpha
	static const float InactivePinForegroundAlpha = 0.15f;
	// Active background pin alpha
	static const float ActivePinBackgroundAlpha = 0.8f;
	// InActive background pin alpha
	static const float InactivePinBackgroundAlpha = 0.4f;
};

void RigElementKeyDetails_GetCustomizedInfo(TSharedRef<IPropertyHandle> InStructPropertyHandle, UControlRigBlueprint*& OutBlueprint)
{
	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	for (UObject* Object : Objects)
	{
		if (Object->IsA<UControlRigBlueprint>())
		{
			OutBlueprint = CastChecked<UControlRigBlueprint>(Object);
			break;
		}

		OutBlueprint = Object->GetTypedOuter<UControlRigBlueprint>();
		if(OutBlueprint)
		{
			break;
		}

		if(const UControlRig* ControlRig = Object->GetTypedOuter<UControlRig>())
		{
			OutBlueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy);
			if(OutBlueprint)
			{
				break;
			}
		}
	}

	if (OutBlueprint == nullptr)
	{
		TArray<UPackage*> Packages;
		InStructPropertyHandle->GetOuterPackages(Packages);
		for (UPackage* Package : Packages)
		{
			if (Package == nullptr)
			{
				continue;
			}

			TArray<UObject*> SubObjects;
			Package->GetDefaultSubobjects(SubObjects);
			for (UObject* SubObject : SubObjects)
			{
				if (UControlRig* Rig = Cast<UControlRig>(SubObject))
				{
					UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(Rig->GetClass()->ClassGeneratedBy);
					if (Blueprint)
					{
						if(Blueprint->GetOutermost() == Package)
						{
							OutBlueprint = Blueprint;
							break;
						}
					}
				}
			}

			if (OutBlueprint)
			{
				break;
			}
		}
	}
}

UControlRigBlueprint* RigElementDetails_GetBlueprintFromHierarchy(URigHierarchy* InHierarchy)
{
	if(InHierarchy == nullptr)
	{
		return nullptr;
	}

	UControlRigBlueprint* Blueprint = InHierarchy->GetTypedOuter<UControlRigBlueprint>();
	if(Blueprint == nullptr)
	{
		UControlRig* Rig = InHierarchy->GetTypedOuter<UControlRig>();
		if(Rig)
		{
			Blueprint = Cast<UControlRigBlueprint>(Rig->GetClass()->ClassGeneratedBy);
        }
	}
	return Blueprint;
}

void FRigElementKeyDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	BlueprintBeingCustomized = nullptr;
	RigElementKeyDetails_GetCustomizedInfo(InStructPropertyHandle, BlueprintBeingCustomized);

	UControlRigGraph* RigGraph = nullptr;
	if(BlueprintBeingCustomized)
	{
		for (UEdGraph* Graph : BlueprintBeingCustomized->UbergraphPages)
		{
			RigGraph = Cast<UControlRigGraph>(Graph);
			if (RigGraph)
			{
				break;
			}
		}
	}

	// only allow blueprints with at least one rig graph
	if (RigGraph == nullptr)
	{
		BlueprintBeingCustomized = nullptr;
	}

	if (BlueprintBeingCustomized == nullptr)
	{
		HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			InStructPropertyHandle->CreatePropertyValueWidget()
		];
	}
	else
	{
		TypeHandle = InStructPropertyHandle->GetChildHandle(TEXT("Type"));
		NameHandle = InStructPropertyHandle->GetChildHandle(TEXT("Name"));

		TypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(
			[this]()
			{
				this->UpdateElementNameList();
				SetElementName(FString());
			}
		));

		UpdateElementNameList();

		HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				TypeHandle->CreatePropertyValueWidget()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SAssignNew(SearchableComboBox, SSearchableComboBox)
				.OptionsSource(&ElementNameList)
				.OnSelectionChanged(this, &FRigElementKeyDetails::OnElementNameChanged)
				.OnGenerateWidget(this, &FRigElementKeyDetails::OnGetElementNameWidget)
				.IsEnabled(!NameHandle->IsEditConst())
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &FRigElementKeyDetails::GetElementNameAsText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			// Use button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1,0)
			.VAlign(VAlign_Center)
			[
				SAssignNew(UseSelectedButton, SButton)
				.ButtonStyle( FAppStyle::Get(), "NoBorder" )
				.ButtonColorAndOpacity_Lambda([this]() { return OnGetWidgetBackground(UseSelectedButton); })
				.OnClicked(this, &FRigElementKeyDetails::OnGetSelectedClicked)
				.ContentPadding(1.f)
				.ToolTipText(NSLOCTEXT("ControlRigElementDetails", "ObjectGraphPin_Use_Tooltip", "Use item selected"))
				[
					SNew(SImage)
					.ColorAndOpacity_Lambda( [this]() { return OnGetWidgetForeground(UseSelectedButton); })
					.Image(FAppStyle::GetBrush("Icons.CircleArrowLeft"))
				]
			]
			// Select in hierarchy button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1,0)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SelectElementButton, SButton)
				.ButtonStyle( FAppStyle::Get(), "NoBorder" )
				.ButtonColorAndOpacity_Lambda([this]() { return OnGetWidgetBackground(SelectElementButton); })
				.OnClicked(this, &FRigElementKeyDetails::OnSelectInHierarchyClicked)
				.ContentPadding(0)
				.ToolTipText(NSLOCTEXT("ControlRigElementDetails", "ObjectGraphPin_Browse_Tooltip", "Select in hierarchy"))
				[
					SNew(SImage)
					.ColorAndOpacity_Lambda( [this]() { return OnGetWidgetForeground(SelectElementButton); })
					.Image(FAppStyle::GetBrush("Icons.Search"))
				]
			]			
		];
	}
}

void FRigElementKeyDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (InStructPropertyHandle->IsValidHandle())
	{
		// only fill the children if the blueprint cannot be found
		if (BlueprintBeingCustomized == nullptr)
		{
			uint32 NumChildren = 0;
			InStructPropertyHandle->GetNumChildren(NumChildren);

			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				StructBuilder.AddProperty(InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
			}
		}
	}
}

ERigElementType FRigElementKeyDetails::GetElementType() const
{
	ERigElementType ElementType = ERigElementType::None;
	if (TypeHandle.IsValid())
	{
		uint8 Index = 0;
		TypeHandle->GetValue(Index);
		ElementType = (ERigElementType)Index;
	}
	return ElementType;
}

FString FRigElementKeyDetails::GetElementName() const
{
	FString ElementNameStr;
	if (NameHandle.IsValid())
	{
		for(int32 ObjectIndex = 0; ObjectIndex < NameHandle->GetNumPerObjectValues(); ObjectIndex++)
		{
			FString PerObjectValue;
			NameHandle->GetPerObjectValue(ObjectIndex, PerObjectValue);

			if(ObjectIndex == 0)
			{
				ElementNameStr = PerObjectValue;
			}
			else if(ElementNameStr != PerObjectValue)
			{
				return ControlRigDetailsMultipleValues.ToString();
			}
		}
	}
	return ElementNameStr;
}

void FRigElementKeyDetails::SetElementName(FString InName)
{
	if (NameHandle.IsValid())
	{
		NameHandle->SetValue(InName);
	}
}

void FRigElementKeyDetails::UpdateElementNameList()
{
	if (!TypeHandle.IsValid())
	{
		return;
	}

	ElementNameList.Reset();

	if (BlueprintBeingCustomized)
	{
		for (UEdGraph* Graph : BlueprintBeingCustomized->UbergraphPages)
		{
			if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph))
			{
				ElementNameList = *RigGraph->GetElementNameList(GetElementType());
				if(SearchableComboBox.IsValid())
				{
					SearchableComboBox->RefreshOptions();
				}
				return;
			}
		}
	}
}

void FRigElementKeyDetails::OnElementNameChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectionInfo)
{
	if (InItem.IsValid())
	{
		SetElementName(*InItem);
	}
	else
	{
		SetElementName(FString());
	}
}

TSharedRef<SWidget> FRigElementKeyDetails::OnGetElementNameWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(InItem.IsValid() ? *InItem : FString()))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

FText FRigElementKeyDetails::GetElementNameAsText() const
{
	return FText::FromString(GetElementName());
}

FSlateColor FRigElementKeyDetails::OnGetWidgetForeground(const TSharedPtr<SButton> Button)
{
	float Alpha = (Button.IsValid() && Button->IsHovered()) ? FRigElementKeyDetailsDefs::ActivePinForegroundAlpha : FRigElementKeyDetailsDefs::InactivePinForegroundAlpha;
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

FSlateColor FRigElementKeyDetails::OnGetWidgetBackground(const TSharedPtr<SButton> Button)
{
	float Alpha = (Button.IsValid() && Button->IsHovered()) ? FRigElementKeyDetailsDefs::ActivePinBackgroundAlpha : FRigElementKeyDetailsDefs::InactivePinBackgroundAlpha;
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

FReply FRigElementKeyDetails::OnGetSelectedClicked()
{
	if (BlueprintBeingCustomized)
	{
		const TArray<FRigElementKey>& Selected = BlueprintBeingCustomized->Hierarchy->GetSelectedKeys();
		if (Selected.Num() > 0)
		{
			if (TypeHandle.IsValid())
			{
				uint8 Index = (uint8) Selected[0].Type;
				TypeHandle->SetValue(Index);
			}
			SetElementName(Selected[0].Name.ToString());
		}
	}
	return FReply::Handled();
}

FReply FRigElementKeyDetails::OnSelectInHierarchyClicked()
{
	if (BlueprintBeingCustomized)
	{
		FRigElementKey Key;
		if (TypeHandle.IsValid())
		{
			uint8 Type;
			TypeHandle->GetValue(Type);
			Key.Type = (ERigElementType) Type;
		}

		if (NameHandle.IsValid())
		{
			NameHandle->GetValue(Key.Name);
		}
				
		if (Key.IsValid())
		{
			BlueprintBeingCustomized->GetHierarchyController()->SetSelection({Key});
		}	
	}
	return FReply::Handled();
}

void FRigComputedTransformDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	BlueprintBeingCustomized = nullptr;
	RigElementKeyDetails_GetCustomizedInfo(InStructPropertyHandle, BlueprintBeingCustomized);
}

void FRigComputedTransformDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TransformHandle = InStructPropertyHandle->GetChildHandle(TEXT("Transform"));

	StructBuilder
	.AddProperty(TransformHandle.ToSharedRef())
	.DisplayName(InStructPropertyHandle->GetPropertyDisplayName());

    FString PropertyPath = TransformHandle->GeneratePathToProperty();

	if(PropertyPath.StartsWith(TEXT("Struct.")))
	{
		PropertyPath.RightChopInline(7);
	}

	if(PropertyPath.StartsWith(TEXT("Pose.")))
	{
		PropertyPath.RightChopInline(5);
		PropertyChain.AddTail(FRigTransformElement::StaticStruct()->FindPropertyByName(TEXT("Pose")));
	}
	else if(PropertyPath.StartsWith(TEXT("Offset.")))
	{
		PropertyPath.RightChopInline(7);
		PropertyChain.AddTail(FRigControlElement::StaticStruct()->FindPropertyByName(TEXT("Offset")));
	}
	else if(PropertyPath.StartsWith(TEXT("Shape.")))
	{
		PropertyPath.RightChopInline(6);
		PropertyChain.AddTail(FRigControlElement::StaticStruct()->FindPropertyByName(TEXT("Shape")));
	}

	if(PropertyPath.StartsWith(TEXT("Current.")))
	{
		PropertyPath.RightChopInline(8);
		PropertyChain.AddTail(FRigCurrentAndInitialTransform::StaticStruct()->FindPropertyByName(TEXT("Current")));
	}
	else if(PropertyPath.StartsWith(TEXT("Initial.")))
	{
		PropertyPath.RightChopInline(8);
		PropertyChain.AddTail(FRigCurrentAndInitialTransform::StaticStruct()->FindPropertyByName(TEXT("Initial")));
	}

	if(PropertyPath.StartsWith(TEXT("Local.")))
	{
		PropertyPath.RightChopInline(6);
		PropertyChain.AddTail(FRigLocalAndGlobalTransform::StaticStruct()->FindPropertyByName(TEXT("Local")));
	}
	else if(PropertyPath.StartsWith(TEXT("Global.")))
	{
		PropertyPath.RightChopInline(7);
		PropertyChain.AddTail(FRigLocalAndGlobalTransform::StaticStruct()->FindPropertyByName(TEXT("Global")));
	}

	PropertyChain.AddTail(TransformHandle->GetProperty());
	PropertyChain.SetActiveMemberPropertyNode(PropertyChain.GetTail()->GetValue());

	const FSimpleDelegate OnTransformChangedDelegate = FSimpleDelegate::CreateSP(this, &FRigComputedTransformDetails::OnTransformChanged, &PropertyChain);
	TransformHandle->SetOnPropertyValueChanged(OnTransformChangedDelegate);
	TransformHandle->SetOnChildPropertyValueChanged(OnTransformChangedDelegate);
}

void FRigComputedTransformDetails::OnTransformChanged(FEditPropertyChain* InPropertyChain)
{
	if(BlueprintBeingCustomized && InPropertyChain)
	{
		if(InPropertyChain->Num() > 1)
		{
			FPropertyChangedEvent ChangeEvent(InPropertyChain->GetHead()->GetValue(), EPropertyChangeType::ValueSet);
			ChangeEvent.SetActiveMemberProperty(InPropertyChain->GetTail()->GetValue());
			FPropertyChangedChainEvent ChainEvent(*InPropertyChain, ChangeEvent);
			BlueprintBeingCustomized->BroadcastPostEditChangeChainProperty(ChainEvent);
		}
	}
}

void FRigBaseElementDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PerElementInfos.Reset();

	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailBuilder.GetObjectsBeingCustomized(DetailObjects);
	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		UDetailsViewWrapperObject* WrapperObject = CastChecked<UDetailsViewWrapperObject>(DetailObject.Get());

		const FRigElementKey Key = WrapperObject->GetContent<FRigBaseElement>().GetKey();

		FPerElementInfo Info;
		Info.WrapperObject = WrapperObject;
		Info.Element = Cast<URigHierarchy>(WrapperObject->GetOuter())->GetHandle(Key);

		if(!Info.Element.IsValid())
		{
			return;
		}
		if(const UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			Info.DefaultElement = Blueprint->Hierarchy->GetHandle(Key);
		}

		PerElementInfos.Add(Info);
	}

	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory(TEXT("General"), LOCTEXT("General", "General"));

	const bool bIsProcedural = IsAnyElementProcedural();
	if(bIsProcedural)
	{
		GeneralCategory.AddCustomRow(LOCTEXT("ProceduralElement", "ProceduralElement")).WholeRowContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProceduralElementNote", "This item has been created procedurally."))
			.ToolTipText(LOCTEXT("ProceduralElementTooltip", "You cannot edit the values of the item here.\nPlease change the settings on the node\nthat created the item."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FLinearColor::Red)
		];
	}

	const bool bAllControls = !IsAnyElementNotOfType(ERigElementType::Control);
	const bool bAllAnimationChannels = !IsAnyControlNotOfAnimationType(ERigControlAnimationType::AnimationChannel);
	if(bAllControls && bAllAnimationChannels)
	{
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Parent Control")))
		.NameContent()
		[
			SNew(SInlineEditableTextBlock)
			.Text(FText::FromString(TEXT("Parent Control")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(false)
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SEditableTextBox)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &FRigBaseElementDetails::GetParentElementName)
				.IsEnabled(false)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SelectParentElementButton, SButton)
				.ButtonStyle( FAppStyle::Get(), "NoBorder" )
				.ButtonColorAndOpacity_Lambda([this]() { return FRigElementKeyDetails::OnGetWidgetBackground(SelectParentElementButton); })
				.OnClicked(this, &FRigBaseElementDetails::OnSelectParentElementInHierarchyClicked)
				.ContentPadding(0)
				.ToolTipText(NSLOCTEXT("ControlRigElementDetails", "SelectParentInHierarchyToolTip", "Select Parent in hierarchy"))
				[
					SNew(SImage)
					.ColorAndOpacity_Lambda( [this]() { return FRigElementKeyDetails::OnGetWidgetForeground(SelectParentElementButton); })
					.Image(FAppStyle::GetBrush("Icons.Search"))
				]
			]
		];
	}

	DetailBuilder.HideCategory(TEXT("RigElement"));

	if(!bAllAnimationChannels)
	{
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Name")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Name")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(!bIsProcedural)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigBaseElementDetails::GetName)
			.OnTextCommitted(this, &FRigBaseElementDetails::SetName)
			.OnVerifyTextChanged(this, &FRigBaseElementDetails::OnVerifyNameChanged)
			.IsEnabled(!bIsProcedural && PerElementInfos.Num() == 1 && !bAllAnimationChannels)
		];
	}

	DetailBuilder.HideCategory(TEXT("RigElement"));

	// if we are not a bone, control or null
	if(!IsAnyElementOfType(ERigElementType::Bone) &&
		!IsAnyElementOfType(ERigElementType::Control) &&
		!IsAnyElementOfType(ERigElementType::Null))
	{
		CustomizeMetadata(DetailBuilder);
	}
}

FRigElementKey FRigBaseElementDetails::GetElementKey() const
{
	check(PerElementInfos.Num() == 1);
	if (FRigBaseElement* Element = PerElementInfos[0].GetElement())
	{
		return Element->GetKey();
	}
	return FRigElementKey();
}

FText FRigBaseElementDetails::GetName() const
{
	if(PerElementInfos.Num() > 1)
	{
		return ControlRigDetailsMultipleValues;
	}
	return FText::FromName(GetElementKey().Name);
}

FText FRigBaseElementDetails::GetParentElementName() const
{
	if(PerElementInfos.Num() > 1)
	{
		return ControlRigDetailsMultipleValues;
	}
	return FText::FromName(PerElementInfos[0].GetHierarchy()->GetFirstParent(GetElementKey()).Name);
}

void FRigBaseElementDetails::SetName(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}
	
	if(PerElementInfos.Num() > 1)
	{
		return;
	}

	if(PerElementInfos[0].IsProcedural())
	{
		return;
	}
	
	if (URigHierarchy* Hierarchy = PerElementInfos[0].GetDefaultHierarchy())
	{
		BeginDestroy();
		
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);
		Controller->RenameElement(GetElementKey(), *InNewText.ToString(), true, true);
	}
}

bool FRigBaseElementDetails::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	if(PerElementInfos.Num() > 1)
	{
		return false;
	}

	if(PerElementInfos[0].IsProcedural())
	{
		return false;
	}

	const URigHierarchy* Hierarchy = PerElementInfos[0].GetDefaultHierarchy();
	if (Hierarchy == nullptr)
	{
		return false;
	}

	if (GetElementKey().Name.ToString() == InText.ToString())
	{
		return true;
	}

	FString OutErrorMessageStr;
	if (!Hierarchy->IsNameAvailable(InText.ToString(), GetElementKey().Type, &OutErrorMessageStr))
	{
		OutErrorMessage = FText::FromString(OutErrorMessageStr);
		return false;
	}

	return true;
}

void FRigBaseElementDetails::OnStructContentsChanged(FProperty* InProperty, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	const FPropertyChangedEvent ChangeEvent(InProperty, EPropertyChangeType::ValueSet);
	PropertyUtilities->NotifyFinishedChangingProperties(ChangeEvent);
}

bool FRigBaseElementDetails::IsConstructionModeEnabled() const
{
	if(PerElementInfos.IsEmpty())
	{
		return false;
	}
	
	if(const UControlRigBlueprint* Blueprint = PerElementInfos[0].GetBlueprint())
	{
		if (const UControlRig* DebuggedRig = Cast<UControlRig>(Blueprint ->GetObjectBeingDebugged()))
		{
			return DebuggedRig->IsConstructionModeEnabled();
		}
	}
	return false;
}

TArray<FRigElementKey> FRigBaseElementDetails::GetElementKeys() const
{
	TArray<FRigElementKey> Keys;
	Algo::Transform(PerElementInfos, Keys, [](const FPerElementInfo& Info)
	{
		return Info.GetElement()->GetKey();
	});
	return Keys;
}

const FRigBaseElementDetails::FPerElementInfo& FRigBaseElementDetails::FindElement(const FRigElementKey& InKey) const
{
	const FPerElementInfo* Info = FindElementByPredicate([InKey](const FPerElementInfo& Info)
	{
		return Info.GetElement()->GetKey() == InKey;
	});

	if(Info)
	{
		return *Info;
	}

	static const FPerElementInfo EmptyInfo;
	return EmptyInfo;
}

bool FRigBaseElementDetails::IsAnyElementOfType(ERigElementType InType) const
{
	return ContainsElementByPredicate([InType](const FPerElementInfo& Info)
	{
		return Info.GetElement()->GetType() == InType;
	});
}

bool FRigBaseElementDetails::IsAnyElementNotOfType(ERigElementType InType) const
{
	return ContainsElementByPredicate([InType](const FPerElementInfo& Info)
	{
		return Info.GetElement()->GetType() != InType;
	});
}

bool FRigBaseElementDetails::IsAnyControlOfAnimationType(ERigControlAnimationType InType) const
{
	return ContainsElementByPredicate([InType](const FPerElementInfo& Info)
	{
		if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
		{
			return ControlElement->Settings.AnimationType == InType;
		}
		return false;
	});
}

bool FRigBaseElementDetails::IsAnyControlNotOfAnimationType(ERigControlAnimationType InType) const
{
	return ContainsElementByPredicate([InType](const FPerElementInfo& Info)
	{
		if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
		{
			return ControlElement->Settings.AnimationType != InType;
		}
		return false;
	});
}

bool FRigBaseElementDetails::IsAnyControlOfValueType(ERigControlType InType) const
{
	return ContainsElementByPredicate([InType](const FPerElementInfo& Info)
	{
		if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
		{
			return ControlElement->Settings.ControlType == InType;
		}
		return false;
	});
}

bool FRigBaseElementDetails::IsAnyControlNotOfValueType(ERigControlType InType) const
{
	return ContainsElementByPredicate([InType](const FPerElementInfo& Info)
	{
		if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
		{
			return ControlElement->Settings.ControlType != InType;
		}
		return false;
	});
}

bool FRigBaseElementDetails::IsAnyElementProcedural() const
{
	return ContainsElementByPredicate([](const FPerElementInfo& Info)
	{
		return Info.IsProcedural();
	});
}

const FRigBaseElementDetails::FPerElementInfo* FRigBaseElementDetails::FindElementByPredicate(const TFunction<bool(const FPerElementInfo&)>& InPredicate) const
{
	return PerElementInfos.FindByPredicate(InPredicate);
}

bool FRigBaseElementDetails::ContainsElementByPredicate(const TFunction<bool(const FPerElementInfo&)>& InPredicate) const
{
	return PerElementInfos.ContainsByPredicate(InPredicate);
}

void FRigBaseElementDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule)
{
	FRigBoneElementDetails().RegisterSectionMappings(PropertyEditorModule, UDetailsViewWrapperObject::GetClassForStruct(FRigBoneElement::StaticStruct()));
	FRigNullElementDetails().RegisterSectionMappings(PropertyEditorModule, UDetailsViewWrapperObject::GetClassForStruct(FRigNullElement::StaticStruct()));
	FRigControlElementDetails().RegisterSectionMappings(PropertyEditorModule, UDetailsViewWrapperObject::GetClassForStruct(FRigControlElement::StaticStruct()));
}

void FRigBaseElementDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass)
{
	TSharedRef<FPropertySection> MetadataSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Metadata", LOCTEXT("Metadata", "Metadata"));
	MetadataSection->AddCategory("Metadata");
}

FReply FRigBaseElementDetails::OnSelectParentElementInHierarchyClicked()
{
	if (PerElementInfos.Num() == 1)
	{
		FRigElementKey Key = GetElementKey();
		if (Key.IsValid())
		{
			const FRigElementKey ParentKey = PerElementInfos[0].GetHierarchy()->GetFirstParent(GetElementKey());
			if(ParentKey.IsValid())
			{
				return OnSelectElementClicked(ParentKey);
			}
		}	
	}
	return FReply::Handled();
}

FReply FRigBaseElementDetails::OnSelectElementClicked(const FRigElementKey& InKey)
{
	if (PerElementInfos.Num() == 1)
	{
		if (InKey.IsValid())
		{
			 PerElementInfos[0].GetHierarchy()->GetController(true)->SetSelection({InKey});
		}	
	}
	return FReply::Handled();
}

void FRigBaseElementDetails::CustomizeMetadata(IDetailLayoutBuilder& DetailBuilder)
{
	if(PerElementInfos.Num() != 1)
	{
		return;
	}
	const FRigBaseElement* Element = PerElementInfos[0].Element.Get();
	if(Element->NumMetadata() == 0)
	{
		return;
	}

	IDetailCategoryBuilder& MetadataCategory = DetailBuilder.EditCategory(TEXT("Metadata"), LOCTEXT("Metadata", "Metadata"));
	for(int32 Index=0;Index<Element->NumMetadata();Index++)
	{
		FRigBaseMetadata* Metadata = Element->GetMetadata(Index);
		TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Metadata->GetMetadataStruct(), (uint8*)Metadata));

		FAddPropertyParams Params;
		Params.CreateCategoryNodes(false);
		Params.ForceShowProperty();
		
		IDetailPropertyRow* Row = MetadataCategory.AddExternalStructureProperty(StructOnScope, TEXT("Value"), EPropertyLocation::Default, Params);
		if(Row)
		{
			(*Row)
			.DisplayName(FText::FromName(Metadata->GetName()))
			.IsEnabled(false);
		}
	}
}

TSharedPtr<TArray<ERigTransformElementDetailsTransform::Type>> FRigTransformElementDetails::PickedTransforms;

void FRigTransformElementDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FRigBaseElementDetails::CustomizeDetails(DetailBuilder);
}

void FRigTransformElementDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass)
{
	FRigBaseElementDetails::RegisterSectionMappings(PropertyEditorModule, InClass);
	
	TSharedRef<FPropertySection> TransformSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Transform", LOCTEXT("Transform", "Transform"));
	TransformSection->AddCategory("General");
	TransformSection->AddCategory("Value");
	TransformSection->AddCategory("Transform");
}

void FRigTransformElementDetails::CustomizeTransform(IDetailLayoutBuilder& DetailBuilder)
{
	if(PerElementInfos.IsEmpty())
	{
		return;
	}
	
	TArray<FRigElementKey> Keys = GetElementKeys();
	Keys = PerElementInfos[0].GetHierarchy()->SortKeys(Keys);

	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bAllControls = !IsAnyElementNotOfType(ERigElementType::Control) && !IsAnyControlOfValueType(ERigControlType::Bool);;
	const bool bAllAnimationChannels = !IsAnyControlNotOfAnimationType(ERigControlAnimationType::AnimationChannel);
	if(bAllControls && bAllAnimationChannels)
	{
		return;
	}

	bool bShowLimits = false;
	TArray<ERigTransformElementDetailsTransform::Type> TransformTypes;
	TArray<FText> ButtonLabels;
	TArray<FText> ButtonTooltips;

	if(bAllControls)
	{
		TransformTypes = {
			ERigTransformElementDetailsTransform::Initial,
			ERigTransformElementDetailsTransform::Current,
			ERigTransformElementDetailsTransform::Offset
		};
		ButtonLabels = {
			LOCTEXT("Initial", "Initial"),
			LOCTEXT("Current", "Current"),
			LOCTEXT("Offset", "Offset")
		};
		ButtonTooltips = {
			LOCTEXT("InitialTooltip", "Initial transform in the reference pose"),
			LOCTEXT("CurrentTooltip", "Current animation transform"),
			LOCTEXT("OffsetTooltip", "Offset transform under the control")
		};

		bShowLimits = !IsAnyControlNotOfValueType(ERigControlType::EulerTransform);

		if(bShowLimits)
		{
			TransformTypes.Append({
				ERigTransformElementDetailsTransform::Minimum,
				ERigTransformElementDetailsTransform::Maximum
			});
			ButtonLabels.Append({
				LOCTEXT("Min", "Min"),
				LOCTEXT("Max", "Max")
			});
			ButtonTooltips.Append({
				LOCTEXT("ValueMinimumTooltip", "The minimum limit(s) for the control"),
				LOCTEXT("ValueMaximumTooltip", "The maximum limit(s) for the control")
			});
		}
	}
	else
	{
		TransformTypes = {
			ERigTransformElementDetailsTransform::Initial,
			ERigTransformElementDetailsTransform::Current
		};
		ButtonLabels = {
			LOCTEXT("Initial", "Initial"),
			LOCTEXT("Current", "Current")
		};
		ButtonTooltips = {
			LOCTEXT("InitialTooltip", "Initial transform in the reference pose"),
			LOCTEXT("CurrentTooltip", "Current animation transform")
		};
	}

	TArray<bool> bTransformsEnabled;

	// determine if the transforms are enabled
	for(int32 Index = 0; Index < TransformTypes.Num(); Index++)
	{
		const ERigTransformElementDetailsTransform::Type CurrentTransformType = TransformTypes[Index];

		bool bIsTransformEnabled = true;

		if(bIsProcedural)
		{
			// procedural items only allow editing of the current transform
			bIsTransformEnabled = CurrentTransformType == ERigTransformElementDetailsTransform::Current; 
		}

		if(bIsTransformEnabled)
		{
			if (IsAnyElementOfType(ERigElementType::Control))
			{
				bIsTransformEnabled = IsAnyControlOfValueType(ERigControlType::EulerTransform) ||
					IsAnyControlOfValueType(ERigControlType::Transform) ||
					CurrentTransformType == ERigTransformElementDetailsTransform::Offset;

				if(!bIsTransformEnabled)
				{
					ButtonTooltips[Index] = FText::FromString(
						FString::Printf(TEXT("%s\n%s"),
							*ButtonTooltips[Index].ToString(), 
							TEXT("Only transform controls can be edited here. Refer to the 'Value' section instead.")));
				}
			}
			else if (IsAnyElementOfType(ERigElementType::Bone) && CurrentTransformType == ERigTransformElementDetailsTransform::Initial)
			{
				for(const FPerElementInfo& Info : PerElementInfos)
				{
					if(const FRigBoneElement* BoneElement = Info.GetElement<FRigBoneElement>())
					{
						bIsTransformEnabled = BoneElement->BoneType == ERigBoneType::User;

						if(!bIsTransformEnabled)
						{
							ButtonTooltips[Index] = FText::FromString(
								FString::Printf(TEXT("%s\n%s"),
									*ButtonTooltips[Index].ToString(), 
									TEXT("Imported Bones' initial transform cannot be edited.")));
						}
					}
				}			
			}
		}
		bTransformsEnabled.Add(bIsTransformEnabled);
	}

	if(!PickedTransforms.IsValid())
	{
		PickedTransforms = MakeShareable(new TArray<ERigTransformElementDetailsTransform::Type>({ERigTransformElementDetailsTransform::Current}));
	}

	TSharedPtr<SSegmentedControl<ERigTransformElementDetailsTransform::Type>> TransformChoiceWidget =
		SSegmentedControl<ERigTransformElementDetailsTransform::Type>::Create(
			TransformTypes,
			ButtonLabels,
			ButtonTooltips,
			*PickedTransforms.Get(),
			true,
			SSegmentedControl<ERigTransformElementDetailsTransform::Type>::FOnValuesChanged::CreateLambda(
				[](TArray<ERigTransformElementDetailsTransform::Type> NewSelection)
				{
					(*FRigTransformElementDetails::PickedTransforms.Get()) = NewSelection;
				}
			)
		);

	IDetailCategoryBuilder& TransformCategory = DetailBuilder.EditCategory(TEXT("Transform"), LOCTEXT("Transform", "Transform"));
	AddChoiceWidgetRow(TransformCategory, FText::FromString(TEXT("TransformType")), TransformChoiceWidget.ToSharedRef());

	SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs = SAdvancedTransformInputBox<FEulerTransform>::FArguments()
	.DisplayToggle(false)
	.DisplayRelativeWorld(true)
	.Font(IDetailLayoutBuilder::GetDetailFont());

	for(int32 Index = 0; Index < ButtonLabels.Num(); Index++)
	{
		const ERigTransformElementDetailsTransform::Type CurrentTransformType = TransformTypes[Index];
		ERigControlValueType CurrentValueType = ERigControlValueType::Current;
		switch(CurrentTransformType)
		{
			case ERigTransformElementDetailsTransform::Initial:
			{
				CurrentValueType = ERigControlValueType::Initial;
				break;
			}
			case ERigTransformElementDetailsTransform::Minimum:
			{
				CurrentValueType = ERigControlValueType::Minimum;
				break;
			}
			case ERigTransformElementDetailsTransform::Maximum:
			{
				CurrentValueType = ERigControlValueType::Maximum;
				break;
			}
		}

		TransformWidgetArgs.Visibility_Lambda([TransformChoiceWidget, Index]() -> EVisibility
		{
			return TransformChoiceWidget->HasValue((ERigTransformElementDetailsTransform::Type)Index) ? EVisibility::Visible : EVisibility::Collapsed;
		});

		TransformWidgetArgs.IsEnabled(bTransformsEnabled[Index]);

		CreateEulerTransformValueWidgetRow(
			Keys,
			TransformWidgetArgs,
			TransformCategory,
			ButtonLabels[Index],
			ButtonTooltips[Index],
			CurrentTransformType,
			CurrentValueType);
	}
}

bool FRigTransformElementDetails::IsCurrentLocalEnabled() const
{
	return IsAnyElementOfType(ERigElementType::Control);
}

void FRigTransformElementDetails::AddChoiceWidgetRow(IDetailCategoryBuilder& InCategory, const FText& InSearchText, TSharedRef<SWidget> InWidget)
{
	InCategory.AddCustomRow(FText::FromString(TEXT("TransformType")))
	.ValueContent()
	.MinDesiredWidth(375.f)
	.MaxDesiredWidth(375.f)
	.HAlign(HAlign_Left)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			InWidget
		]
	];
}

FDetailWidgetRow& FRigTransformElementDetails::CreateTransformComponentValueWidgetRow(
	ERigControlType InControlType,
	const TArray<FRigElementKey>& Keys,
	SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs,
	IDetailCategoryBuilder& CategoryBuilder,
	const FText& Label,
	const FText& Tooltip,
	ERigTransformElementDetailsTransform::Type CurrentTransformType,
	ERigControlValueType ValueType,
	TSharedPtr<SWidget> NameContent)
{
	TransformWidgetArgs
	.Font(IDetailLayoutBuilder::GetDetailFont())
	.AllowEditRotationRepresentation(false);

	if(TransformWidgetArgs._DisplayRelativeWorld &&
		!TransformWidgetArgs._OnGetIsComponentRelative.IsBound() &&
		!TransformWidgetArgs._OnIsComponentRelativeChanged.IsBound())
	{
		TSharedRef<UE::Math::TVector<float>> IsComponentRelative = MakeShareable(new UE::Math::TVector<float>(1.f, 1.f, 1.f));
		
		TransformWidgetArgs
		.OnGetIsComponentRelative_Lambda(
			[IsComponentRelative](ESlateTransformComponent::Type InComponent)
			{
				return IsComponentRelative->operator[]((int32)InComponent) > 0.f;
			})
		.OnIsComponentRelativeChanged_Lambda(
			[IsComponentRelative](ESlateTransformComponent::Type InComponent, bool bIsRelative)
			{
				IsComponentRelative->operator[]((int32)InComponent) = bIsRelative ? 1.f : 0.f;
			});
	}

	TransformWidgetArgs.ConstructLocation(InControlType == ERigControlType::Position);
	TransformWidgetArgs.ConstructRotation(InControlType == ERigControlType::Rotator);
	TransformWidgetArgs.ConstructScale(InControlType == ERigControlType::Scale);

	return CreateEulerTransformValueWidgetRow(
		Keys,
		TransformWidgetArgs,
		CategoryBuilder,
		Label,
		Tooltip,
		CurrentTransformType,
		ValueType,
		NameContent);
}

TSharedPtr<TArray<ERigControlValueType>> FRigControlElementDetails::PickedValueTypes;

FDetailWidgetRow& FRigTransformElementDetails::CreateEulerTransformValueWidgetRow(
	const TArray<FRigElementKey>& Keys,
	SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs,
	IDetailCategoryBuilder& CategoryBuilder,
	const FText& Label,
	const FText& Tooltip,
	ERigTransformElementDetailsTransform::Type CurrentTransformType,
	ERigControlValueType ValueType,
	TSharedPtr<SWidget> NameContent)
{
	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();
	if(ValueType == ERigControlValueType::Current)
	{
		HierarchyToChange = Hierarchy;
	}
	
	const FRigElementTransformWidgetSettings& Settings = FRigElementTransformWidgetSettings::FindOrAdd(ValueType, CurrentTransformType, TransformWidgetArgs);

	const bool bDisplayRelativeWorldOnCurrent = TransformWidgetArgs._DisplayRelativeWorld; 
	if(bDisplayRelativeWorldOnCurrent &&
		!TransformWidgetArgs._OnGetIsComponentRelative.IsBound() &&
		!TransformWidgetArgs._OnIsComponentRelativeChanged.IsBound())
	{
		TSharedRef<UE::Math::TVector<float>> IsComponentRelativeStorage = Settings.IsComponentRelative;
		
		TransformWidgetArgs.OnGetIsComponentRelative_Lambda(
			[IsComponentRelativeStorage](ESlateTransformComponent::Type InComponent)
			{
				return IsComponentRelativeStorage->operator[]((int32)InComponent) > 0.f;
			})
		.OnIsComponentRelativeChanged_Lambda(
			[IsComponentRelativeStorage](ESlateTransformComponent::Type InComponent, bool bIsRelative)
			{
				IsComponentRelativeStorage->operator[]((int32)InComponent) = bIsRelative ? 1.f : 0.f;
			});
	}

	const TSharedPtr<ESlateRotationRepresentation::Type> RotationRepresentationStorage = Settings.RotationRepresentation;
	TransformWidgetArgs.RotationRepresentation(RotationRepresentationStorage);
	
	auto IsComponentRelative = [TransformWidgetArgs](int32 Component) -> bool
	{
		if(TransformWidgetArgs._OnGetIsComponentRelative.IsBound())
		{
			return TransformWidgetArgs._OnGetIsComponentRelative.Execute((ESlateTransformComponent::Type)Component);
		}
		return true;
	};

	auto ConformComponentRelative = [TransformWidgetArgs, IsComponentRelative](int32 Component)
	{
		if(TransformWidgetArgs._OnIsComponentRelativeChanged.IsBound())
		{
			bool bRelative = IsComponentRelative(Component);
			TransformWidgetArgs._OnIsComponentRelativeChanged.Execute(ESlateTransformComponent::Location, bRelative);
			TransformWidgetArgs._OnIsComponentRelativeChanged.Execute(ESlateTransformComponent::Rotation, bRelative);
			TransformWidgetArgs._OnIsComponentRelativeChanged.Execute(ESlateTransformComponent::Scale, bRelative);
		}
	};

	TransformWidgetArgs.IsScaleLocked(Settings.IsScaleLocked);

	switch(CurrentTransformType)
	{
		case ERigTransformElementDetailsTransform::Minimum:
		case ERigTransformElementDetailsTransform::Maximum:
		{
			TransformWidgetArgs.AllowEditRotationRepresentation(false);
			TransformWidgetArgs.DisplayRelativeWorld(false);
			TransformWidgetArgs.DisplayToggle(true);
			TransformWidgetArgs.OnGetToggleChecked_Lambda([Keys, Hierarchy, ValueType]
				(
					ESlateTransformComponent::Type Component,
					ESlateRotationRepresentation::Type RotationRepresentation,
					ESlateTransformSubComponent::Type SubComponent
				) -> ECheckBoxState
				{
					TOptional<bool> FirstValue;

					for(const FRigElementKey& Key : Keys)
					{
						if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
						{
							TOptional<bool> Value;

							switch(ControlElement->Settings.ControlType)
							{
								case ERigControlType::Position:
								case ERigControlType::Rotator:
								case ERigControlType::Scale:
								{
									if(ControlElement->Settings.LimitEnabled.Num() == 3)
									{
										const int32 Index = ControlElement->Settings.ControlType == ERigControlType::Rotator ?
											int32(SubComponent) - int32(ESlateTransformSubComponent::Pitch) :
											int32(SubComponent) - int32(ESlateTransformSubComponent::X);

										Value = ControlElement->Settings.LimitEnabled[Index].GetForValueType(ValueType);
									}
									break;
								}
								case ERigControlType::EulerTransform:
								{
									if(ControlElement->Settings.LimitEnabled.Num() == 9)
									{
										switch(Component)
										{
											case ESlateTransformComponent::Location:
											{
												switch(SubComponent)
												{
													case ESlateTransformSubComponent::X:
													{
														Value = ControlElement->Settings.LimitEnabled[0].GetForValueType(ValueType);
														break;
													}
													case ESlateTransformSubComponent::Y:
													{
														Value = ControlElement->Settings.LimitEnabled[1].GetForValueType(ValueType);
														break;
													}
													case ESlateTransformSubComponent::Z:
													{
														Value = ControlElement->Settings.LimitEnabled[2].GetForValueType(ValueType);
														break;
													}
													default:
													{
														break;
													}
												}
												break;
											}
											case ESlateTransformComponent::Rotation:
											{
												switch(SubComponent)
												{
													case ESlateTransformSubComponent::Pitch:
													{
														Value = ControlElement->Settings.LimitEnabled[3].GetForValueType(ValueType);
														break;
													}
													case ESlateTransformSubComponent::Yaw:
													{
														Value = ControlElement->Settings.LimitEnabled[4].GetForValueType(ValueType);
														break;
													}
													case ESlateTransformSubComponent::Roll:
													{
														Value = ControlElement->Settings.LimitEnabled[5].GetForValueType(ValueType);
														break;
													}
													default:
													{
														break;
													}
												}
												break;
											}
											case ESlateTransformComponent::Scale:
											{
												switch(SubComponent)
												{
													case ESlateTransformSubComponent::X:
													{
														Value = ControlElement->Settings.LimitEnabled[6].GetForValueType(ValueType);
														break;
													}
													case ESlateTransformSubComponent::Y:
													{
														Value = ControlElement->Settings.LimitEnabled[7].GetForValueType(ValueType);
														break;
													}
													case ESlateTransformSubComponent::Z:
													{
														Value = ControlElement->Settings.LimitEnabled[8].GetForValueType(ValueType);
														break;
													}
													default:
													{
														break;
													}
												}
												break;
											}
										}
									}
									break;
								}
							}

							if(Value.IsSet())
							{
								if(FirstValue.IsSet())
								{
									if(FirstValue.GetValue() != Value.GetValue())
									{
										return ECheckBoxState::Undetermined;
									}
								}
								else
								{
									FirstValue = Value;
								}
							}
						}
					}

					if(!ensure(FirstValue.IsSet()))
					{
						return ECheckBoxState::Undetermined;
					}
					return FirstValue.GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				});
				
			TransformWidgetArgs.OnToggleChanged_Lambda([ValueType, Keys, this, Hierarchy]
			(
				ESlateTransformComponent::Type Component,
				ESlateRotationRepresentation::Type RotationRepresentation,
				ESlateTransformSubComponent::Type SubComponent,
				ECheckBoxState CheckState
			)
			{
				if(CheckState == ECheckBoxState::Undetermined)
				{
					return;
				}

				const bool Value = CheckState == ECheckBoxState::Checked;

				FScopedTransaction Transaction(LOCTEXT("ChangeLimitToggle", "Change Limit Toggle"));
				Hierarchy->Modify();

				for(const FRigElementKey& Key : Keys)
				{
					if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
					{
						switch(ControlElement->Settings.ControlType)
						{
							case ERigControlType::Position:
							case ERigControlType::Rotator:
							case ERigControlType::Scale:
							{
								if(ControlElement->Settings.LimitEnabled.Num() == 3)
								{
									const int32 Index = ControlElement->Settings.ControlType == ERigControlType::Rotator ?
										int32(SubComponent) - int32(ESlateTransformSubComponent::Pitch) :
										int32(SubComponent) - int32(ESlateTransformSubComponent::X);

									ControlElement->Settings.LimitEnabled[Index].SetForValueType(ValueType, Value);
								}
								break;
							}
							case ERigControlType::EulerTransform:
							{
								if(ControlElement->Settings.LimitEnabled.Num() == 9)
								{
									switch(Component)
									{
										case ESlateTransformComponent::Location:
										{
											switch(SubComponent)
											{
												case ESlateTransformSubComponent::X:
												{
													ControlElement->Settings.LimitEnabled[0].SetForValueType(ValueType, Value);
													break;
												}
												case ESlateTransformSubComponent::Y:
												{
													ControlElement->Settings.LimitEnabled[1].SetForValueType(ValueType, Value);
													break;
												}
												case ESlateTransformSubComponent::Z:
												{
													ControlElement->Settings.LimitEnabled[2].SetForValueType(ValueType, Value);
													break;
												}
												default:
												{
													break;
												}
											}
											break;
										}
										case ESlateTransformComponent::Rotation:
										{
											switch(SubComponent)
											{
												case ESlateTransformSubComponent::Pitch:
												{
													ControlElement->Settings.LimitEnabled[3].SetForValueType(ValueType, Value);
													break;
												}
												case ESlateTransformSubComponent::Yaw:
												{
													ControlElement->Settings.LimitEnabled[4].SetForValueType(ValueType, Value);
													break;
												}
												case ESlateTransformSubComponent::Roll:
												{
													ControlElement->Settings.LimitEnabled[5].SetForValueType(ValueType, Value);
													break;
												}
												default:
												{
													break;
												}
											}
											break;
										}
										case ESlateTransformComponent::Scale:
										{
											switch(SubComponent)
											{
												case ESlateTransformSubComponent::X:
												{
													ControlElement->Settings.LimitEnabled[6].SetForValueType(ValueType, Value);
													break;
												}
												case ESlateTransformSubComponent::Y:
												{
													ControlElement->Settings.LimitEnabled[7].SetForValueType(ValueType, Value);
													break;
												}
												case ESlateTransformSubComponent::Z:
												{
													ControlElement->Settings.LimitEnabled[8].SetForValueType(ValueType, Value);
													break;
												}
												default:
												{
													break;
												}
											}
											break;
										}
									}
								}
								break;
							}
						}
						
						Hierarchy->SetControlSettings(ControlElement, ControlElement->Settings, true, true, true);
					}
				}
			});
			break;
		}
		default:
		{
			TransformWidgetArgs.AllowEditRotationRepresentation(true);
			TransformWidgetArgs.DisplayRelativeWorld(bDisplayRelativeWorldOnCurrent);
			TransformWidgetArgs.DisplayToggle(false);
			TransformWidgetArgs._OnGetToggleChecked.Unbind();
			TransformWidgetArgs._OnToggleChanged.Unbind();
			break;
		}
	}

	auto GetRelativeAbsoluteTransforms = [CurrentTransformType, Keys, Hierarchy](
		const FRigElementKey& Key,
		ERigTransformElementDetailsTransform::Type InTransformType = ERigTransformElementDetailsTransform::Max
		) -> TPair<FEulerTransform, FEulerTransform>
	{
		if(InTransformType == ERigTransformElementDetailsTransform::Max)
		{
			InTransformType = CurrentTransformType;
		}

		FEulerTransform RelativeTransform = FEulerTransform::Identity;
		FEulerTransform AbsoluteTransform = FEulerTransform::Identity;

		const bool bInitial = InTransformType == ERigTransformElementDetailsTransform::Initial; 
		if(bInitial || InTransformType == ERigTransformElementDetailsTransform::Current)
		{
			RelativeTransform.FromFTransform(Hierarchy->GetLocalTransform(Key, bInitial));
			AbsoluteTransform.FromFTransform(Hierarchy->GetGlobalTransform(Key, bInitial));

			if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
			{
				switch(ControlElement->Settings.ControlType)
				{
					case ERigControlType::Rotator:
					case ERigControlType::EulerTransform:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					{
						RelativeTransform.Rotation = Hierarchy->GetControlPreferredRotator(ControlElement, bInitial);
						break;
					}
					default:
					{
						break;
					}
				}
			}
		}
		else
		{
			if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
			{
				const ERigControlType ControlType = ControlElement->Settings.ControlType;

				if(InTransformType == ERigTransformElementDetailsTransform::Offset)
				{
					RelativeTransform.FromFTransform(Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::InitialLocal));
					AbsoluteTransform.FromFTransform(Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::InitialGlobal));
				}
				else if(InTransformType == ERigTransformElementDetailsTransform::Minimum)
				{
					switch(ControlType)
					{
						case ERigControlType::Position:
						{
							const FVector Data = 
								(FVector)Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Minimum)
								.Get<FVector3f>();
							AbsoluteTransform = RelativeTransform = FEulerTransform(Data, FRotator::ZeroRotator, FVector::OneVector);
							break;
						}
						case ERigControlType::Rotator:
						{
							const FVector Data = 
								(FVector)Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Minimum)
								.Get<FVector3f>();
							FRotator Rotator = FRotator::MakeFromEuler(Data);
							AbsoluteTransform = RelativeTransform = FEulerTransform(FVector::ZeroVector, Rotator, FVector::OneVector);
							break;
						}
						case ERigControlType::Scale:
						{
							const FVector Data = 
								(FVector)Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Minimum)
								.Get<FVector3f>();
							AbsoluteTransform = RelativeTransform = FEulerTransform(FVector::ZeroVector, FRotator::ZeroRotator, Data);
							break;
						}
						case ERigControlType::EulerTransform:
						{
							const FRigControlValue::FEulerTransform_Float EulerTransform = 
								Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Minimum)
								.Get<FRigControlValue::FEulerTransform_Float>();
							AbsoluteTransform = RelativeTransform = EulerTransform.ToTransform();
							break;
						}
					}
				}
				else if(InTransformType == ERigTransformElementDetailsTransform::Maximum)
				{
					switch(ControlType)
					{
						case ERigControlType::Position:
						{
							const FVector Data = 
								(FVector)Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Maximum)
								.Get<FVector3f>();
							AbsoluteTransform = RelativeTransform = FEulerTransform(Data, FRotator::ZeroRotator, FVector::OneVector);
							break;
						}
						case ERigControlType::Rotator:
						{
							const FVector Data = 
								(FVector)Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Maximum)
								.Get<FVector3f>();
							FRotator Rotator = FRotator::MakeFromEuler(Data);
							AbsoluteTransform = RelativeTransform = FEulerTransform(FVector::ZeroVector, Rotator, FVector::OneVector);
							break;
						}
						case ERigControlType::Scale:
						{
							const FVector Data = 
								(FVector)Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Maximum)
								.Get<FVector3f>();
							AbsoluteTransform = RelativeTransform = FEulerTransform(FVector::ZeroVector, FRotator::ZeroRotator, Data);
							break;
						}
						case ERigControlType::EulerTransform:
						{
							const FRigControlValue::FEulerTransform_Float EulerTransform = 
								Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Maximum)
								.Get<FRigControlValue::FEulerTransform_Float>();
							AbsoluteTransform = RelativeTransform = EulerTransform.ToTransform();
							break;
						}
					}
				}
			}
		}

		return TPair<FEulerTransform, FEulerTransform>(RelativeTransform, AbsoluteTransform);
	};

	
	auto GetCombinedTransform = [IsComponentRelative, GetRelativeAbsoluteTransforms](
		const FRigElementKey& Key,
		ERigTransformElementDetailsTransform::Type InTransformType = ERigTransformElementDetailsTransform::Max
		) -> FEulerTransform
	{
		const TPair<FEulerTransform, FEulerTransform> TransformPair = GetRelativeAbsoluteTransforms(Key, InTransformType);
		const FEulerTransform RelativeTransform = TransformPair.Key;
		const FEulerTransform AbsoluteTransform = TransformPair.Value;

		FEulerTransform Xfo;
		Xfo.SetLocation((IsComponentRelative(0)) ? RelativeTransform.GetLocation() : AbsoluteTransform.GetLocation());
		Xfo.SetRotator((IsComponentRelative(1)) ? RelativeTransform.Rotator() : AbsoluteTransform.Rotator());
		Xfo.SetScale3D((IsComponentRelative(2)) ? RelativeTransform.GetScale3D() : AbsoluteTransform.GetScale3D());

		return Xfo;
	};

	auto GetSingleTransform = [GetRelativeAbsoluteTransforms](
		const FRigElementKey& Key,
		bool bIsRelative,
		ERigTransformElementDetailsTransform::Type InTransformType = ERigTransformElementDetailsTransform::Max
		) -> FEulerTransform
	{
		const TPair<FEulerTransform, FEulerTransform> TransformPair = GetRelativeAbsoluteTransforms(Key, InTransformType);
		const FEulerTransform RelativeTransform = TransformPair.Key;
		const FEulerTransform AbsoluteTransform = TransformPair.Value;
		return bIsRelative ? RelativeTransform : AbsoluteTransform;
	};

	auto SetSingleTransform = [CurrentTransformType, GetRelativeAbsoluteTransforms, this, Hierarchy](
		const FRigElementKey& Key,
		FEulerTransform InTransform,
		bool bIsRelative,
		bool bSetupUndoRedo)
	{
		const bool bCurrent = CurrentTransformType == ERigTransformElementDetailsTransform::Current; 
		const bool bInitial = CurrentTransformType == ERigTransformElementDetailsTransform::Initial; 

		bool bConstructionModeEnabled = false;
		if (UControlRig* DebuggedRig = Cast<UControlRig>(PerElementInfos[0].GetBlueprint()->GetObjectBeingDebugged()))
		{
			bConstructionModeEnabled = DebuggedRig->IsConstructionModeEnabled();
		}

		TArray<URigHierarchy*> HierarchiesToUpdate;
		HierarchiesToUpdate.Add(Hierarchy);
		if(!bCurrent || bConstructionModeEnabled)
		{
			HierarchiesToUpdate.Add(PerElementInfos[0].GetDefaultHierarchy());
		}

		for(URigHierarchy* HierarchyToUpdate : HierarchiesToUpdate)
		{
			if(bInitial || CurrentTransformType == ERigTransformElementDetailsTransform::Current)
			{
				if(bIsRelative)
				{
					HierarchyToUpdate->SetLocalTransform(Key, InTransform.ToFTransform(), bInitial, true, bSetupUndoRedo);

					if(FRigControlElement* ControlElement = HierarchyToUpdate->Find<FRigControlElement>(Key))
					{
						switch(ControlElement->Settings.ControlType)
						{
							case ERigControlType::Rotator:
							case ERigControlType::EulerTransform:
							case ERigControlType::Transform:
							case ERigControlType::TransformNoScale:
							{
								HierarchyToUpdate->SetControlPreferredRotator(ControlElement, InTransform.Rotator(), bInitial);
								break;
							}
							default:
							{
								break;
							}
						}
					}
				}
				else
				{
					HierarchyToUpdate->SetGlobalTransform(Key, InTransform.ToFTransform(), bInitial, true, bSetupUndoRedo);
				}
			}
			else
			{
				if(FRigControlElement* ControlElement = HierarchyToUpdate->Find<FRigControlElement>(Key))
				{
					const ERigControlType ControlType = ControlElement->Settings.ControlType;

					if(CurrentTransformType == ERigTransformElementDetailsTransform::Offset)
					{
						if(!bIsRelative)
						{
							const FTransform ParentTransform = HierarchyToUpdate->GetParentTransform(Key, bInitial);
							InTransform.FromFTransform(InTransform.ToFTransform().GetRelativeTransform(ParentTransform));
						}
						HierarchyToUpdate->SetControlOffsetTransform(Key, InTransform.ToFTransform(), true, true, bSetupUndoRedo);
					}
					else if(CurrentTransformType == ERigTransformElementDetailsTransform::Minimum)
					{
						switch(ControlType)
						{
							case ERigControlType::Position:
							{
								const FRigControlValue Value = FRigControlValue::Make<FVector3f>((FVector3f)InTransform.GetLocation());
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Minimum, bSetupUndoRedo, true);
								break;
							}
							case ERigControlType::Rotator:
							{
								const FVector3f Euler = (FVector3f)InTransform.Rotator().Euler();
								const FRigControlValue Value = FRigControlValue::Make<FVector3f>(Euler);
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Minimum, bSetupUndoRedo, true);
								break;
							}
							case ERigControlType::Scale:
							{
								const FRigControlValue Value = FRigControlValue::Make<FVector3f>((FVector3f)InTransform.GetScale3D());
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Minimum, bSetupUndoRedo, true);
								break;
							}
							case ERigControlType::EulerTransform:
							{
								const FRigControlValue Value = FRigControlValue::Make<FRigControlValue::FEulerTransform_Float>(InTransform);
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Minimum, bSetupUndoRedo, true);
								break;
							}
						}
					}
					else if(CurrentTransformType == ERigTransformElementDetailsTransform::Maximum)
					{
						switch(ControlType)
						{
							case ERigControlType::Position:
							{
								const FRigControlValue Value = FRigControlValue::Make<FVector3f>((FVector3f)InTransform.GetLocation());
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Maximum, bSetupUndoRedo, true);
								break;
							}
							case ERigControlType::Rotator:
							{
								const FVector3f Euler = (FVector3f)InTransform.Rotator().Euler();
								const FRigControlValue Value = FRigControlValue::Make<FVector3f>(Euler);
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Maximum, bSetupUndoRedo, true);
								break;
							}
							case ERigControlType::Scale:
							{
								const FRigControlValue Value = FRigControlValue::Make<FVector3f>((FVector3f)InTransform.GetScale3D());
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Maximum, bSetupUndoRedo, true);
								break;
							}
							case ERigControlType::EulerTransform:
							{
								const FRigControlValue Value = FRigControlValue::Make<FRigControlValue::FEulerTransform_Float>(InTransform);
								HierarchyToUpdate->SetControlValue(ControlElement, Value, ERigControlValueType::Maximum, bSetupUndoRedo, true);
								break;
							}
						}
					}
				}
			}
		}
	};

	TransformWidgetArgs.OnGetNumericValue_Lambda([Keys, GetCombinedTransform](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent) -> TOptional<FVector::FReal>
	{
		TOptional<FVector::FReal> FirstValue;

		for(int32 Index = 0; Index < Keys.Num(); Index++)
		{
			const FRigElementKey& Key = Keys[Index];
			FEulerTransform Xfo = GetCombinedTransform(Key);

			TOptional<FVector::FReal> CurrentValue = SAdvancedTransformInputBox<FEulerTransform>::GetNumericValueFromTransform(Xfo, Component, Representation, SubComponent);
			if(!CurrentValue.IsSet())
			{
				return CurrentValue;
			}

			if(Index == 0)
			{
				FirstValue = CurrentValue;
			}
			else
			{
				if(!FMath::IsNearlyEqual(FirstValue.GetValue(), CurrentValue.GetValue()))
				{
					return TOptional<FVector::FReal>();
				}
			}
		}
		
		return FirstValue;
	});

	TransformWidgetArgs.OnNumericValueChanged_Lambda(
	[
		Keys,
		this,
		IsComponentRelative,
		GetSingleTransform,
		SetSingleTransform,
		HierarchyToChange
	](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FVector::FReal InNumericValue)
	{
		const bool bIsRelative = IsComponentRelative((int32)Component);

		for(const FRigElementKey& Key : Keys)
		{
			FEulerTransform Transform = GetSingleTransform(Key, bIsRelative);
			FEulerTransform PreviousTransform = Transform;
			SAdvancedTransformInputBox<FEulerTransform>::ApplyNumericValueChange(Transform, InNumericValue, Component, Representation, SubComponent);

			if(!FRigControlElementDetails::Equals(Transform, PreviousTransform))
			{
				if(!SliderTransaction.IsValid())
				{
					SliderTransaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("ControlRigElementDetails", "ChangeNumericValue", "Change Numeric Value")));
					HierarchyToChange->Modify();
				}
							
				SetSingleTransform(Key, Transform, bIsRelative, false);
			}
		}
	});

	TransformWidgetArgs.OnNumericValueCommitted_Lambda(
	[
		Keys,
		this,
		IsComponentRelative,
		GetSingleTransform,
		SetSingleTransform,
		HierarchyToChange
	](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FVector::FReal InNumericValue,
		ETextCommit::Type InCommitType)
	{
		const bool bIsRelative = IsComponentRelative((int32)Component);

		{
			FScopedTransaction Transaction(LOCTEXT("ChangeNumericValue", "Change Numeric Value"));
			if(!SliderTransaction.IsValid())
			{
				HierarchyToChange->Modify();
			}
			
			for(const FRigElementKey& Key : Keys)
			{
				FEulerTransform Transform = GetSingleTransform(Key, bIsRelative);
				SAdvancedTransformInputBox<FEulerTransform>::ApplyNumericValueChange(Transform, InNumericValue, Component, Representation, SubComponent);
				SetSingleTransform(Key, Transform, bIsRelative, true);
			}
		}

		SliderTransaction.Reset();
	});

	TransformWidgetArgs.OnCopyToClipboard_Lambda([Keys, IsComponentRelative, ConformComponentRelative, GetSingleTransform](
		ESlateTransformComponent::Type InComponent
		)
	{
		if(Keys.Num() == 0)
		{
			return;
		}

		// make sure that we use the same relative setting on all components when copying
		ConformComponentRelative(0);
		const bool bIsRelative = IsComponentRelative(0); 

		const FRigElementKey& FirstKey = Keys[0];
		FEulerTransform Xfo = GetSingleTransform(FirstKey, bIsRelative);

		FString Content;
		switch(InComponent)
		{
			case ESlateTransformComponent::Location:
			{
				const FVector Data = Xfo.GetLocation();
				TBaseStructure<FVector>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
				break;
			}
			case ESlateTransformComponent::Rotation:
			{
				const FRotator Data = Xfo.Rotator();
				TBaseStructure<FRotator>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
				break;
			}
			case ESlateTransformComponent::Scale:
			{
				const FVector Data = Xfo.GetScale3D();
				TBaseStructure<FVector>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
				break;
			}
			case ESlateTransformComponent::Max:
			default:
			{
				TBaseStructure<FEulerTransform>::Get()->ExportText(Content, &Xfo, &Xfo, nullptr, PPF_None, nullptr);
				break;
			}
		}

		if(!Content.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Content);
		}
	});

	TransformWidgetArgs.OnPasteFromClipboard_Lambda([this, Keys, IsComponentRelative, ConformComponentRelative, GetSingleTransform, SetSingleTransform, HierarchyToChange](
		ESlateTransformComponent::Type InComponent
		)
	{
		if(Keys.Num() == 0)
		{
			return;
		}
		
		
		// make sure that we use the same relative setting on all components when pasting
		ConformComponentRelative(0);
		const bool bIsRelative = IsComponentRelative(0); 

		FString Content;
		FPlatformApplicationMisc::ClipboardPaste(Content);

		if(Content.IsEmpty())
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("PasteTransform", "Paste Transform"));
		HierarchyToChange->Modify();

		for(const FRigElementKey& Key : Keys)
		{
			FEulerTransform Xfo = GetSingleTransform(Key, bIsRelative);
			{
				class FRigPasteTransformWidgetErrorPipe : public FOutputDevice
				{
				public:

					int32 NumErrors;

					FRigPasteTransformWidgetErrorPipe()
						: FOutputDevice()
						, NumErrors(0)
					{
					}

					virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
					{
						UE_LOG(LogControlRig, Error, TEXT("Error Pasting to Widget: %s"), V);
						NumErrors++;
					}
				};

				FRigPasteTransformWidgetErrorPipe ErrorPipe;
				
				switch(InComponent)
				{
					case ESlateTransformComponent::Location:
					{
						FVector Data = Xfo.GetLocation();
						TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
						Xfo.SetLocation(Data);
						break;
					}
					case ESlateTransformComponent::Rotation:
					{
						FRotator Data = Xfo.Rotator();
						TBaseStructure<FRotator>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FRotator>::Get()->GetName(), true);
						Xfo.SetRotator(Data);
						break;
					}
					case ESlateTransformComponent::Scale:
					{
						FVector Data = Xfo.GetScale3D();
						TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
						Xfo.SetScale3D(Data);
						break;
					}
					case ESlateTransformComponent::Max:
					default:
					{
						TBaseStructure<FEulerTransform>::Get()->ImportText(*Content, &Xfo, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FEulerTransform>::Get()->GetName(), true);
						break;
					}
				}

				if(ErrorPipe.NumErrors == 0)
				{
					SetSingleTransform(Key, Xfo, bIsRelative, true);
				}
			}
		}
	});

	TransformWidgetArgs.DiffersFromDefault_Lambda([
		CurrentTransformType,
		Keys,
		GetSingleTransform
		
	](
		ESlateTransformComponent::Type InComponent) -> bool
	{
		for(const FRigElementKey& Key : Keys)
		{
			const FEulerTransform CurrentTransform = GetSingleTransform(Key, true);
			FEulerTransform DefaultTransform;

			switch(CurrentTransformType)
			{
				case ERigTransformElementDetailsTransform::Current:
				{
					DefaultTransform = GetSingleTransform(Key, true, ERigTransformElementDetailsTransform::Initial);
					break;
				}
				default:
				{
					DefaultTransform = FEulerTransform::Identity; 
					break;
				}
			}

			switch(InComponent)
			{
				case ESlateTransformComponent::Location:
				{
					if(!DefaultTransform.GetLocation().Equals(CurrentTransform.GetLocation()))
					{
						return true;
					}
					break;
				}
				case ESlateTransformComponent::Rotation:
				{
					if(!DefaultTransform.Rotator().Equals(CurrentTransform.Rotator()))
					{
						return true;
					}
					break;
				}
				case ESlateTransformComponent::Scale:
				{
					if(!DefaultTransform.GetScale3D().Equals(CurrentTransform.GetScale3D()))
					{
						return true;
					}
					break;
				}
				default: // also no component whole transform
				{
					if(!DefaultTransform.GetLocation().Equals(CurrentTransform.GetLocation()) ||
						!DefaultTransform.Rotator().Equals(CurrentTransform.Rotator()) ||
						!DefaultTransform.GetScale3D().Equals(CurrentTransform.GetScale3D()))
					{
						return true;
					}
					break;
				}
			}
		}
		return false;
	});

	TransformWidgetArgs.OnResetToDefault_Lambda([this, CurrentTransformType, Keys, GetSingleTransform, SetSingleTransform, HierarchyToChange](
		ESlateTransformComponent::Type InComponent)
	{
		FScopedTransaction Transaction(LOCTEXT("ResetTransformToDefault", "Reset Transform to Default"));
		HierarchyToChange->Modify();

		for(const FRigElementKey& Key : Keys)
		{
			FEulerTransform CurrentTransform = GetSingleTransform(Key, true);
			FEulerTransform DefaultTransform;

			switch(CurrentTransformType)
			{
				case ERigTransformElementDetailsTransform::Current:
				{
					DefaultTransform = GetSingleTransform(Key, true, ERigTransformElementDetailsTransform::Initial);
					break;
				}
				default:
				{
					DefaultTransform = FEulerTransform::Identity; 
					break;
				}
			}

			switch(InComponent)
			{
				case ESlateTransformComponent::Location:
				{
					CurrentTransform.SetLocation(DefaultTransform.GetLocation());
					break;
				}
				case ESlateTransformComponent::Rotation:
				{
					CurrentTransform.SetRotator(DefaultTransform.Rotator());
					break;
				}
				case ESlateTransformComponent::Scale:
				{
					CurrentTransform.SetScale3D(DefaultTransform.GetScale3D());
					break;
				}
				default: // whole transform / max component
				{
					CurrentTransform = DefaultTransform;
					break;
				}
			}

			SetSingleTransform(Key, CurrentTransform, true, true);
		}
	});

	return SAdvancedTransformInputBox<FEulerTransform>::ConstructGroupedTransformRows(
		CategoryBuilder, 
		Label, 
		Tooltip, 
		TransformWidgetArgs,
		NameContent);
}

ERigTransformElementDetailsTransform::Type FRigTransformElementDetails::GetTransformTypeFromValueType(
	ERigControlValueType InValueType)
{
	ERigTransformElementDetailsTransform::Type TransformType = ERigTransformElementDetailsTransform::Current;
	switch(InValueType)
	{
		case ERigControlValueType::Initial:
		{
			TransformType = ERigTransformElementDetailsTransform::Initial;
			break;
		}
		case ERigControlValueType::Minimum:
		{
			TransformType = ERigTransformElementDetailsTransform::Minimum;
			break;
		}
		case ERigControlValueType::Maximum:
		{
			TransformType = ERigTransformElementDetailsTransform::Maximum;
			break;
		}
		default:
		{
			break;
		}
	}
	return TransformType;
}

void FRigBoneElementDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FRigTransformElementDetails::CustomizeDetails(DetailBuilder);
	CustomizeTransform(DetailBuilder);
	CustomizeMetadata(DetailBuilder);
}

void FRigControlElementDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FRigTransformElementDetails::CustomizeDetails(DetailBuilder);

	CustomizeControl(DetailBuilder);
	CustomizeValue(DetailBuilder);
	CustomizeTransform(DetailBuilder);
	CustomizeShape(DetailBuilder);
	CustomizeAnimationChannels(DetailBuilder);
	CustomizeMetadata(DetailBuilder);
}

void FRigControlElementDetails::CustomizeValue(IDetailLayoutBuilder& DetailBuilder)
{
	if(PerElementInfos.IsEmpty())
	{
		return;
	}

	if(IsAnyElementNotOfType(ERigElementType::Control))
	{
		return;
	}

	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();

	// only show this section if all controls are the same type
	const FRigControlElement* FirstControlElement = PerElementInfos[0].GetElement<FRigControlElement>();
	const ERigControlType ControlType = FirstControlElement->Settings.ControlType;
	bool bAllAnimationChannels = true;
	
	for(const FPerElementInfo& Info : PerElementInfos)
	{
		const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>();
		if(ControlElement->Settings.ControlType != ControlType)
		{
			return;
		}
		if(ControlElement->Settings.AnimationType != ERigControlAnimationType::AnimationChannel)
		{
			bAllAnimationChannels = false;
		}
	}

	// transforms don't show their value here - instead they are shown in the transform section
	if((ControlType == ERigControlType::EulerTransform ||
		ControlType == ERigControlType::Transform ||
		ControlType == ERigControlType::TransformNoScale) &&
		!bAllAnimationChannels)
	{
		return;
	}
	
	TArray<FText> Labels = {
		LOCTEXT("Initial", "Initial"),
		LOCTEXT("Current", "Current")
	};
	TArray<FText> Tooltips = {
		LOCTEXT("ValueInitialTooltip", "The initial animation value of the control"),
		LOCTEXT("ValueCurrentTooltip", "The current animation value of the control")
	};
	TArray<ERigControlValueType> ValueTypes = {
		ERigControlValueType::Initial,
		ERigControlValueType::Current
	};

	// bool doesn't have limits,
	// transform types already got filtered out earlier.
	// integers with enums don't have limits either
	if(ControlType != ERigControlType::Bool &&
		(ControlType != ERigControlType::Integer || !FirstControlElement->Settings.ControlEnum))
	{
		Labels.Append({
			LOCTEXT("Min", "Min"),
			LOCTEXT("Max", "Max")
		});
		Tooltips.Append({
			LOCTEXT("ValueMinimumTooltip", "The minimum limit(s) for the control"),
			LOCTEXT("ValueMaximumTooltip", "The maximum limit(s) for the control")
		});
		ValueTypes.Append({
			ERigControlValueType::Minimum,
			ERigControlValueType::Maximum
		});
	}
	
	IDetailCategoryBuilder& ValueCategory = DetailBuilder.EditCategory(TEXT("Value"), LOCTEXT("Value", "Value"));

	if(!PickedValueTypes.IsValid())
	{
		PickedValueTypes = MakeShareable(new TArray<ERigControlValueType>({ERigControlValueType::Current}));
	}

	TSharedPtr<SSegmentedControl<ERigControlValueType>> ValueTypeChoiceWidget =
		SSegmentedControl<ERigControlValueType>::Create(
			ValueTypes,
			Labels,
			Tooltips,
			*PickedValueTypes.Get(),
			true,
			SSegmentedControl<ERigControlValueType>::FOnValuesChanged::CreateLambda(
				[](TArray<ERigControlValueType> NewSelection)
				{
					(*FRigControlElementDetails::PickedValueTypes.Get()) = NewSelection;
				}
			)
		);

	AddChoiceWidgetRow(ValueCategory, FText::FromString(TEXT("ValueType")), ValueTypeChoiceWidget.ToSharedRef());

	TArray<FRigElementKey> Keys = GetElementKeys();
	Keys = Hierarchy->SortKeys(Keys);

	for(int32 Index=0; Index < ValueTypes.Num(); Index++)
	{
		const ERigControlValueType ValueType = ValueTypes[Index];

		const TAttribute<EVisibility> VisibilityAttribute =
			TAttribute<EVisibility>::CreateLambda([ValueType, ValueTypeChoiceWidget]()-> EVisibility
			{
				return ValueTypeChoiceWidget->HasValue(ValueType) ? EVisibility::Visible : EVisibility::Collapsed; 
			});
		
		switch(ControlType)
		{
			case ERigControlType::Bool:
			{
				CreateBoolValueWidgetRow(Keys, ValueCategory, Labels[Index], Tooltips[Index], ValueType, VisibilityAttribute);
				break;
			}
			case ERigControlType::Float:
			{
				CreateFloatValueWidgetRow(Keys, ValueCategory, Labels[Index], Tooltips[Index], ValueType, VisibilityAttribute);
				break;
			}
			case ERigControlType::Integer:
			{
				bool bIsEnum = false;
				for(const FRigElementKey& Key : Keys)
				{
					if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
					{
						if(ControlElement->Settings.ControlEnum)
						{
							bIsEnum = true;
							break;
						}
					}
				}

				if(bIsEnum)
				{
					CreateEnumValueWidgetRow(Keys, ValueCategory, Labels[Index], Tooltips[Index], ValueType, VisibilityAttribute);
				}
				else
				{
					CreateIntegerValueWidgetRow(Keys, ValueCategory, Labels[Index], Tooltips[Index], ValueType, VisibilityAttribute);
				}
				break;
			}
			case ERigControlType::Vector2D:
			{
				CreateVector2DValueWidgetRow(Keys, ValueCategory, Labels[Index], Tooltips[Index], ValueType, VisibilityAttribute);
				break;
			}
			case ERigControlType::Position:
			case ERigControlType::Rotator:
			case ERigControlType::Scale:
			{
				SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs = SAdvancedTransformInputBox<FEulerTransform>::FArguments()
				.DisplayToggle(false)
				.DisplayRelativeWorld(true)
				.Visibility(VisibilityAttribute);

				CreateTransformComponentValueWidgetRow(
					ControlType,
					GetElementKeys(),
					TransformWidgetArgs,
					ValueCategory,
					Labels[Index],
					Tooltips[Index],
					GetTransformTypeFromValueType(ValueType),
					ValueType);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

void FRigControlElementDetails::CustomizeControl(IDetailLayoutBuilder& DetailBuilder)
{
	if(PerElementInfos.IsEmpty())
	{
		return;
	}

	if(IsAnyElementNotOfType(ERigElementType::Control))
	{
		return;
	}

	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bIsEnabled = !bIsProcedural;
	
	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();

	const TSharedPtr<IPropertyHandle> SettingsHandle = DetailBuilder.GetProperty(TEXT("Settings"));
	DetailBuilder.HideProperty(SettingsHandle);

	IDetailCategoryBuilder& ControlCategory = DetailBuilder.EditCategory(TEXT("Control"), LOCTEXT("Control", "Control"));

	const bool bAllAnimationChannels = !IsAnyControlNotOfAnimationType(ERigControlAnimationType::AnimationChannel);
	static const FText DisplayNameText = LOCTEXT("DisplayName", "Display Name");
	static const FText ChannelNameText = LOCTEXT("ChannelName", "Channel Name");
	const FText DisplayNameLabelText = bAllAnimationChannels ? ChannelNameText : DisplayNameText;

	const TSharedPtr<IPropertyHandle> DisplayNameHandle = SettingsHandle->GetChildHandle(TEXT("DisplayName"));
	ControlCategory.AddCustomRow(DisplayNameLabelText)
	.IsEnabled(bIsEnabled)
	.NameContent()
	[
		DisplayNameHandle->CreatePropertyNameWidget(DisplayNameLabelText)
	]
	.ValueContent()
	[
		SNew(SInlineEditableTextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(this, &FRigControlElementDetails::GetDisplayName)
		.OnTextCommitted(this, &FRigControlElementDetails::SetDisplayName)
		.OnVerifyTextChanged_Lambda([this](const FText& InText, FText& OutErrorMessage) -> bool
		{
			return OnVerifyDisplayNameChanged(InText, OutErrorMessage, GetElementKey());
		})
		.IsEnabled(bIsEnabled && (PerElementInfos.Num() == 1))
	];

	const TSharedRef<IPropertyUtilities> PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	// when control type changes, we have to refresh detail panel
	const TSharedPtr<IPropertyHandle> AnimationTypeHandle = SettingsHandle->GetChildHandle(TEXT("AnimationType"));
	AnimationTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(
		[this, PropertyUtilities, HierarchyToChange, Hierarchy]()
		{
			TArray<FRigControlElement> ControlElementsInView = GetElementsInDetailsView<FRigControlElement>();

			if (HierarchyToChange && ControlElementsInView.Num() == PerElementInfos.Num())
			{
				HierarchyToChange->Modify();
				
				for(int32 ControlIndex = 0; ControlIndex< ControlElementsInView.Num(); ControlIndex++)
				{
					const FRigControlElement& ViewElement = ControlElementsInView[ControlIndex];
					FRigControlElement* ControlElement = PerElementInfos[ControlIndex].GetDefaultElement<FRigControlElement>();
					
					ControlElement->Settings.AnimationType = ViewElement.Settings.AnimationType;

					ControlElement->Settings.bGroupWithParentControl =
						ControlElement->Settings.ControlType == ERigControlType::Bool ||
						ControlElement->Settings.ControlType == ERigControlType::Float ||
						ControlElement->Settings.ControlType == ERigControlType::Integer ||
						ControlElement->Settings.ControlType == ERigControlType::Vector2D;

					switch(ControlElement->Settings.AnimationType)
					{
						case ERigControlAnimationType::AnimationControl:
						{
							ControlElement->Settings.ShapeVisibility = ERigControlVisibility::UserDefined;
							ControlElement->Settings.bShapeVisible = true;
							break;
						}
						case ERigControlAnimationType::AnimationChannel:
						{
							ControlElement->Settings.ShapeVisibility = ERigControlVisibility::UserDefined;
							ControlElement->Settings.bShapeVisible = false;
							break;
						}
						case ERigControlAnimationType::ProxyControl:
						{
							ControlElement->Settings.ShapeVisibility = ERigControlVisibility::BasedOnSelection;
							ControlElement->Settings.bShapeVisible = true;
							ControlElement->Settings.bGroupWithParentControl = false;
							break;
						}
						default:
						{
							ControlElement->Settings.ShapeVisibility = ERigControlVisibility::UserDefined;
							ControlElement->Settings.bShapeVisible = true;
							ControlElement->Settings.bGroupWithParentControl = false;
							break;
						}
					}

					HierarchyToChange->SetControlSettings(ControlElement, ControlElement->Settings, true, true, true);
					PerElementInfos[ControlIndex].WrapperObject->SetContent<FRigControlElement>(*ControlElement);

					if (HierarchyToChange != Hierarchy)
					{
						if(FRigControlElement* OtherControlElement = PerElementInfos[0].GetElement<FRigControlElement>())
						{
							OtherControlElement->Settings = ControlElement->Settings;
							Hierarchy->SetControlSettings(OtherControlElement, OtherControlElement->Settings, true, true, true);
						}
					}
				}
				
				PropertyUtilities->ForceRefresh();
			}
		}
	));

	ControlCategory.AddProperty(AnimationTypeHandle.ToSharedRef())
	.IsEnabled(bIsEnabled);

	// when control type changes, we have to refresh detail panel
	const TSharedPtr<IPropertyHandle> ControlTypeHandle = SettingsHandle->GetChildHandle(TEXT("ControlType"));
	ControlTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(
		[this, PropertyUtilities]()
		{
			TArray<FRigControlElement> ControlElementsInView = GetElementsInDetailsView<FRigControlElement>();
			HandleControlTypeChanged(ControlElementsInView[0].Settings.ControlType, TArray<FRigElementKey>(), PropertyUtilities);
		}
	));

	ControlCategory.AddProperty(ControlTypeHandle.ToSharedRef())
	.IsEnabled(bIsEnabled);

	const bool bSupportsShape = !IsAnyControlOfAnimationType(ERigControlAnimationType::AnimationChannel) &&
		!IsAnyControlOfAnimationType(ERigControlAnimationType::VisualCue);

	if (HierarchyToChange != nullptr)
	{
		bool bEnableGroupWithParentControl = true;
		for(const FPerElementInfo& Info : PerElementInfos)
		{
			if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
			{
				bool bSingleEnableGroupWithParentControl = false;
				if(const FRigControlElement* ParentElement =
					Cast<FRigControlElement>(Info.GetHierarchy()->GetFirstParent(ControlElement)))
				{
					if(ControlElement->Settings.IsAnimatable() &&
						Info.GetHierarchy()->GetChildren(ControlElement).IsEmpty())
					{
						bSingleEnableGroupWithParentControl = true;
					}
				}

				if(!bSingleEnableGroupWithParentControl)
				{
					bEnableGroupWithParentControl = false;
					break;
				}
			}
		}
		if(bEnableGroupWithParentControl)
		{
			const TSharedPtr<IPropertyHandle> GroupWithParentControlHandle = SettingsHandle->GetChildHandle(TEXT("bGroupWithParentControl"));
			ControlCategory.AddProperty(GroupWithParentControlHandle.ToSharedRef()).DisplayName(FText::FromString(TEXT("Group Channels")))
			.IsEnabled(bIsEnabled);
		}
	}
	
	if(bSupportsShape &&
		!(IsAnyControlNotOfValueType(ERigControlType::Integer) &&
		IsAnyControlNotOfValueType(ERigControlType::Float) &&
		IsAnyControlNotOfValueType(ERigControlType::Vector2D)))
	{
		const TSharedPtr<IPropertyHandle> PrimaryAxisHandle = SettingsHandle->GetChildHandle(TEXT("PrimaryAxis"));
		ControlCategory.AddProperty(PrimaryAxisHandle.ToSharedRef()).DisplayName(FText::FromString(TEXT("Primary Axis")))
		.IsEnabled(bIsEnabled);
	}

	if(IsAnyControlOfValueType(ERigControlType::Integer))
	{
		const TSharedPtr<IPropertyHandle> ControlEnumHandle = SettingsHandle->GetChildHandle(TEXT("ControlEnum"));
		ControlCategory.AddProperty(ControlEnumHandle.ToSharedRef()).DisplayName(FText::FromString(TEXT("Control Enum")))
		.IsEnabled(bIsEnabled);

		ControlEnumHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(
			[this, PropertyUtilities]()
			{
				PropertyUtilities->ForceRefresh();

				for(int32 ControlIndex = 0; ControlIndex < PerElementInfos.Num(); ControlIndex++)
				{
					FPerElementInfo& Info = PerElementInfos[ControlIndex];
					const FRigControlElement ControlInView = Info.WrapperObject->GetContent<FRigControlElement>();
					FRigControlElement* ControlBeingCustomized = Info.GetDefaultElement<FRigControlElement>();
					
					const UEnum* ControlEnum = ControlBeingCustomized->Settings.ControlEnum;
					if (ControlEnum != nullptr)
					{
						int32 Maximum = (int32)ControlEnum->GetMaxEnumValue() - 1;
						ControlBeingCustomized->Settings.MinimumValue.Set<int32>(0);
						ControlBeingCustomized->Settings.MaximumValue.Set<int32>(Maximum);
						ControlBeingCustomized->Settings.LimitEnabled.Reset();
						ControlBeingCustomized->Settings.LimitEnabled.Add(true);
						Info.GetDefaultHierarchy()->SetControlSettings(ControlBeingCustomized, ControlBeingCustomized->Settings, true, true, true);

						FRigControlValue InitialValue = Info.GetDefaultHierarchy()->GetControlValue(ControlBeingCustomized, ERigControlValueType::Initial);
						FRigControlValue CurrentValue = Info.GetDefaultHierarchy()->GetControlValue(ControlBeingCustomized, ERigControlValueType::Current);

						ControlBeingCustomized->Settings.ApplyLimits(InitialValue);
						ControlBeingCustomized->Settings.ApplyLimits(CurrentValue);
						Info.GetDefaultHierarchy()->SetControlValue(ControlBeingCustomized, InitialValue, ERigControlValueType::Initial, false, false, true);
						Info.GetDefaultHierarchy()->SetControlValue(ControlBeingCustomized, CurrentValue, ERigControlValueType::Current, false, false, true);

						if (UControlRig* DebuggedRig = Cast<UControlRig>(Info.GetBlueprint()->GetObjectBeingDebugged()))
						{
							URigHierarchy* DebuggedHierarchy = DebuggedRig->GetHierarchy();
							if(FRigControlElement* DebuggedControlElement = DebuggedHierarchy->Find<FRigControlElement>(ControlBeingCustomized->GetKey()))
							{
								DebuggedControlElement->Settings.MinimumValue.Set<int32>(0);
                                DebuggedControlElement->Settings.MaximumValue.Set<int32>(Maximum);
								DebuggedHierarchy->SetControlSettings(DebuggedControlElement, DebuggedControlElement->Settings, true, true, true);

                                DebuggedHierarchy->SetControlValue(DebuggedControlElement, InitialValue, ERigControlValueType::Initial);
                                DebuggedHierarchy->SetControlValue(DebuggedControlElement, CurrentValue, ERigControlValueType::Current);
							}
						}
					}

					Info.WrapperObject->SetContent<FRigControlElement>(*ControlBeingCustomized);
				}
			}
		));
		
	}

	const TSharedPtr<IPropertyHandle> CustomizationHandle = SettingsHandle->GetChildHandle(TEXT("Customization"));

	if(bSupportsShape)
	{
		const TSharedPtr<IPropertyHandle> AvailableSpacesHandle = CustomizationHandle->GetChildHandle(TEXT("AvailableSpaces"));
		ControlCategory.AddProperty(AvailableSpacesHandle.ToSharedRef())
		.IsEnabled(bIsEnabled);
	}

	TArray<FRigElementKey> Keys = GetElementKeys();

	if(bSupportsShape)
	{
		const TSharedPtr<IPropertyHandle> DrawLimitsHandle = SettingsHandle->GetChildHandle(TEXT("bDrawLimits"));
		
		ControlCategory
		.AddProperty(DrawLimitsHandle.ToSharedRef()).DisplayName(FText::FromString(TEXT("Draw Limits")))
		.IsEnabled(TAttribute<bool>::CreateLambda([Keys, Hierarchy, bIsEnabled]() -> bool
		{
			if(!bIsEnabled)
			{
				return false;
			}
			
			for(const FRigElementKey& Key : Keys)
			{
				if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
				{
					if(ControlElement->Settings.LimitEnabled.Contains(FRigControlLimitEnabled(true, true)))
					{
						return true;
					}
				}
			}
			return false;
		}));
	}
	
	if(!IsAnyControlNotOfAnimationType(ERigControlAnimationType::ProxyControl))
	{
		ControlCategory.AddProperty(SettingsHandle->GetChildHandle(TEXT("DrivenControls")).ToSharedRef())
		.IsEnabled(bIsEnabled);
	}
}

void FRigControlElementDetails::CustomizeAnimationChannels(IDetailLayoutBuilder& DetailBuilder)
{
	// only show this for non-animation channels
	if(!IsAnyControlNotOfAnimationType(ERigControlAnimationType::AnimationChannel))
	{
		return;
	}

	// only show this if only one control is selected
	if(PerElementInfos.Num() != 1)
	{
		return;
	}

	const FRigControlElement* ControlElement = PerElementInfos[0].GetElement<FRigControlElement>();
	if(ControlElement == nullptr)
	{
		return;
	}

	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bIsEnabled = !bIsProcedural;

	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("AnimationChannels"), LOCTEXT("AnimationChannels", "Animation Channels"));
	
	const TSharedRef<IPropertyUtilities> PropertyUtilities = DetailBuilder.GetPropertyUtilities();
	
	const TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox);
	HeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	[
		SNew(SButton)
		.IsEnabled(bIsEnabled)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(1, 0))
		.OnClicked(this, &FRigControlElementDetails::OnAddAnimationChannelClicked)
		.HAlign(HAlign_Right)
		.ToolTipText(LOCTEXT("AddAnimationChannelToolTip", "Add a new animation channel"))
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
	Category.HeaderContent(HeaderContentWidget);

	bool bHasAnimationChannels = false;
	const FRigBaseElementChildrenArray ChildElements = Hierarchy->GetChildren(ControlElement);
	for(const FRigBaseElement* ChildElement : ChildElements)
	{
		if(const FRigControlElement* ChildControlElement = Cast<FRigControlElement>(ChildElement))
		{
			if(ChildControlElement->IsAnimationChannel())
			{
				bHasAnimationChannels = true;
				const FRigElementKey ChildElementKey = ChildElement->GetKey();
				
				const TPair<const FSlateBrush*, FSlateColor> BrushAndColor = SRigHierarchyItem::GetBrushForElementType(Hierarchy, ChildElementKey);

				static TArray<TSharedPtr<ERigControlType>> ControlValueTypes;
				if(ControlValueTypes.IsEmpty())
				{
					const UEnum* ValueTypeEnum = StaticEnum<ERigControlType>();
					for(int64 EnumValue = 0; EnumValue < ValueTypeEnum->GetMaxEnumValue(); EnumValue++)
					{
						if(ValueTypeEnum->HasMetaData(TEXT("Hidden"), (int32)EnumValue))
						{
							continue;
						}
						ControlValueTypes.Add(MakeShareable(new ERigControlType((ERigControlType)EnumValue)));
					}
				}

				TSharedPtr<SButton> SelectAnimationChannelButton;
				TSharedPtr<SWidget> NameContent;

				SAssignNew(NameContent, SHorizontalBox)
				.IsEnabled(bIsEnabled)

				+ SHorizontalBox::Slot()
				.MaxWidth(32)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
				[
					SNew(SComboButton)
					.ContentPadding(0)
					.HasDownArrow(false)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(BrushAndColor.Key)
						.ColorAndOpacity(BrushAndColor.Value)
					]
					.MenuContent()
					[
						SNew(SListView<TSharedPtr<ERigControlType>>)
						.ListItemsSource( &ControlValueTypes )
						.OnGenerateRow(this, &FRigControlElementDetails::HandleGenerateAnimationChannelTypeRow, ChildElementKey)
						.OnSelectionChanged(this, &FRigControlElementDetails::HandleControlTypeChanged, ChildElementKey, PropertyUtilities)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
				[
					SNew(SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromName(ChildControlElement->GetDisplayName()))
					.OnTextCommitted_Lambda([this, ChildElementKey](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						SetDisplayNameForElement(InNewText, InCommitType, ChildElementKey);
					})
					.OnVerifyTextChanged_Lambda([this, ChildElementKey](const FText& InText, FText& OutErrorMessage) -> bool
					{
						return OnVerifyDisplayNameChanged(InText, OutErrorMessage, ChildElementKey);
					})
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
				[
					SAssignNew(SelectAnimationChannelButton, SButton)
					.ButtonStyle( FAppStyle::Get(), "NoBorder" )
					.ButtonColorAndOpacity_Lambda([SelectAnimationChannelButton]() { return FRigElementKeyDetails::OnGetWidgetBackground(SelectAnimationChannelButton); })
					.OnClicked_Lambda([this, ChildElementKey]() -> FReply
					{
						return OnSelectElementClicked(ChildElementKey);
					})
					.ContentPadding(0)
					.ToolTipText(NSLOCTEXT("ControlRigElementDetails", "SelectParentInHierarchyToolTip", "Select Parent in hierarchy"))
					[
						SNew(SImage)
						.ColorAndOpacity_Lambda( [SelectAnimationChannelButton]() { return FRigElementKeyDetails::OnGetWidgetForeground(SelectAnimationChannelButton); })
						.Image(FAppStyle::GetBrush("Icons.Search"))
					]
				];

				const FText Label = FText::FromString(FString::Printf(TEXT("Channel%s"), *ChildControlElement->GetDisplayName().ToString()));
				const TArray<FRigElementKey> ChildElementKeys = {ChildElementKey};
				TAttribute<EVisibility> Visibility = EVisibility::Visible;

				FDetailWidgetRow* WidgetRow = nullptr; 
				switch(ChildControlElement->Settings.ControlType)
				{
					case ERigControlType::Bool:
					{
						WidgetRow = &CreateBoolValueWidgetRow(ChildElementKeys, Category, Label, FText(), ERigControlValueType::Current, Visibility, NameContent);
						break;
					}
					case ERigControlType::Float:
					{
						WidgetRow = &CreateFloatValueWidgetRow(ChildElementKeys, Category, Label, FText(), ERigControlValueType::Current, Visibility, NameContent);
						break;
					}
					case ERigControlType::Integer:
					{
						if(ChildControlElement->Settings.ControlEnum)
						{
							WidgetRow = &CreateEnumValueWidgetRow(ChildElementKeys, Category, Label, FText(), ERigControlValueType::Current, Visibility, NameContent);
						}
						else
						{
							WidgetRow = &CreateIntegerValueWidgetRow(ChildElementKeys, Category, Label, FText(), ERigControlValueType::Current, Visibility, NameContent);
						}
						break;
					}
					case ERigControlType::Vector2D:
					{
						WidgetRow = &CreateVector2DValueWidgetRow(ChildElementKeys, Category, Label, FText(), ERigControlValueType::Current, Visibility, NameContent);
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Rotator:
					case ERigControlType::Scale:
					{
						SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs =
							SAdvancedTransformInputBox<FEulerTransform>::FArguments()
						.DisplayToggle(false)
						.DisplayRelativeWorld(false)
						.Visibility(EVisibility::Visible);

						WidgetRow = &CreateTransformComponentValueWidgetRow(
							ChildControlElement->Settings.ControlType,
							ChildElementKeys,
							TransformWidgetArgs,
							Category,
							Label,
							FText(),
							GetTransformTypeFromValueType(ERigControlValueType::Current),
							ERigControlValueType::Current,
							NameContent);
						break;
					}
					case ERigControlType::Transform:
					case ERigControlType::EulerTransform:
					{
						SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs =
							SAdvancedTransformInputBox<FEulerTransform>::FArguments()
						.DisplayToggle(false)
						.DisplayRelativeWorld(false)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Visibility(EVisibility::Visible);

						WidgetRow = &CreateEulerTransformValueWidgetRow(
							ChildElementKeys,
							TransformWidgetArgs,
							Category,
							Label,
							FText(),
							ERigTransformElementDetailsTransform::Current,
							ERigControlValueType::Current,
							NameContent);
						break;
					}
					default:
					{
						WidgetRow = &Category.AddCustomRow(Label)
						.NameContent()
						[
							NameContent.ToSharedRef()
						];
						break;
					}
				}

				if(WidgetRow)
				{
					WidgetRow->AddCustomContextMenuAction(FUIAction(
						FExecuteAction::CreateLambda([this, ChildElementKeys, HierarchyToChange]()
						{
							if(URigHierarchyController* Controller = HierarchyToChange->GetController(true))
							{
								FScopedTransaction(LOCTEXT("DeleteAnimationChannels", "Delete Animation Channels"));
								HierarchyToChange->Modify();
								
								for(const FRigElementKey& KeyToRemove : ChildElementKeys)
								{
									Controller->RemoveElement(KeyToRemove, true, true);
								}
							}
						})),
						LOCTEXT("DeleteAnimationChannel", "Delete"),
						LOCTEXT("DeleteAnimationChannelTooltip", "Deletes this animation channel"),
					FSlateIcon());
				}
			}
		}
	}

	Category.InitiallyCollapsed(!bHasAnimationChannels);
	if(!bHasAnimationChannels)
	{
		Category.AddCustomRow(FText()).WholeRowContent()
		[
			SNew(SHorizontalBox)
	
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.IsEnabled(bIsEnabled)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("NoAnimationChannels", "No animation channels"))
			]
		];
	}
}

FReply FRigControlElementDetails::OnAddAnimationChannelClicked()
{
	if(IsAnyElementNotOfType(ERigElementType::Control) || IsAnyElementProcedural())
	{
		return FReply::Handled();
	}

	const FRigElementKey& Key = PerElementInfos[0].GetElement()->GetKey();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();

	static const FString ChannelName = TEXT("Channel");
	FRigControlSettings Settings;
	Settings.AnimationType = ERigControlAnimationType::AnimationChannel;
	Settings.ControlType = ERigControlType::Float;
	Settings.MinimumValue = FRigControlValue::Make<float>(0.f);
	Settings.MaximumValue = FRigControlValue::Make<float>(1.f);
	Settings.DisplayName = HierarchyToChange->GetSafeNewDisplayName(Key, ChannelName);
	HierarchyToChange->GetController(true)->AddAnimationChannel(*ChannelName, Key, Settings, true, true);
	HierarchyToChange->GetController(true)->SelectElement(Key);
	return FReply::Handled();
}

TSharedRef<ITableRow> FRigControlElementDetails::HandleGenerateAnimationChannelTypeRow(TSharedPtr<ERigControlType> ControlType, const TSharedRef<STableViewBase>& OwnerTable, FRigElementKey ControlKey)
{
	const URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();

	TPair<const FSlateBrush*, FSlateColor> BrushAndColor = SRigHierarchyItem::GetBrushForElementType(HierarchyToChange, ControlKey);
	BrushAndColor.Value = SRigHierarchyItem::GetColorForControlType(*ControlType.Get(), nullptr);

	return SNew(STableRow<TSharedPtr<ERigControlType>>, OwnerTable)
	.Content()
	[
		SNew(SHorizontalBox)
	
		+ SHorizontalBox::Slot()
		.MaxWidth(18)
		.FillWidth(1.0)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
		[
			SNew(SImage)
			.Image(BrushAndColor.Key)
			.ColorAndOpacity(BrushAndColor.Value)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(StaticEnum<ERigControlType>()->GetDisplayNameTextByValue((int64)*ControlType.Get()))
		]
	];
}

void FRigControlElementDetails::HandleControlTypeChanged(TSharedPtr<ERigControlType> ControlType, ESelectInfo::Type SelectInfo, FRigElementKey ControlKey, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	HandleControlTypeChanged(*ControlType.Get(), {ControlKey}, PropertyUtilities);
}

void FRigControlElementDetails::HandleControlTypeChanged(ERigControlType ControlType, TArray<FRigElementKey> ControlKeys, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	for(FPerElementInfo& Info : PerElementInfos)
	{
		URigHierarchy* Hierarchy = Info.GetHierarchy();
		URigHierarchy* HierarchyToChange = Info.GetDefaultHierarchy();
		HierarchyToChange->Modify();
		
		FRigControlElement* ControlElement = Info.GetDefaultElement<FRigControlElement>();
		
		FRigControlValue ValueToSet;

		ControlElement->Settings.ControlType = ControlType;
		ControlElement->Settings.LimitEnabled.Reset();
		ControlElement->Settings.bGroupWithParentControl = false;

		switch (ControlElement->Settings.ControlType)
		{
			case ERigControlType::Bool:
			{
				ControlElement->Settings.AnimationType = ERigControlAnimationType::AnimationChannel;
				ValueToSet = FRigControlValue::Make<bool>(false);
				ControlElement->Settings.bGroupWithParentControl = ControlElement->Settings.IsAnimatable();
				break;
			}
			case ERigControlType::Float:
			{
				ValueToSet = FRigControlValue::Make<float>(0.f);
				ControlElement->Settings.SetupLimitArrayForType(true);
				ControlElement->Settings.MinimumValue = FRigControlValue::Make<float>(0.f);
				ControlElement->Settings.MaximumValue = FRigControlValue::Make<float>(100.f);
				ControlElement->Settings.bGroupWithParentControl = ControlElement->Settings.IsAnimatable();
				break;
			}
			case ERigControlType::Integer:
			{
				ValueToSet = FRigControlValue::Make<int32>(0);
				ControlElement->Settings.SetupLimitArrayForType(true);
				ControlElement->Settings.MinimumValue = FRigControlValue::Make<int32>(0);
				ControlElement->Settings.MaximumValue = FRigControlValue::Make<int32>(100);
				ControlElement->Settings.bGroupWithParentControl = ControlElement->Settings.IsAnimatable();
				break;
			}
			case ERigControlType::Vector2D:
			{
				ValueToSet = FRigControlValue::Make<FVector2D>(FVector2D::ZeroVector);
				ControlElement->Settings.SetupLimitArrayForType(true);
				ControlElement->Settings.MinimumValue = FRigControlValue::Make<FVector2D>(FVector2D::ZeroVector);
				ControlElement->Settings.MaximumValue = FRigControlValue::Make<FVector2D>(FVector2D(100.f, 100.f));
				ControlElement->Settings.bGroupWithParentControl = ControlElement->Settings.IsAnimatable();
				break;
			}
			case ERigControlType::Position:
			{
				ValueToSet = FRigControlValue::Make<FVector>(FVector::ZeroVector);
				ControlElement->Settings.SetupLimitArrayForType(false);
				ControlElement->Settings.MinimumValue = FRigControlValue::Make<FVector>(-FVector::OneVector);
				ControlElement->Settings.MaximumValue = FRigControlValue::Make<FVector>(FVector::OneVector);
				break;
			}
			case ERigControlType::Scale:
			{
				ValueToSet = FRigControlValue::Make<FVector>(FVector::OneVector);
				ControlElement->Settings.SetupLimitArrayForType(false);
				ControlElement->Settings.MinimumValue = FRigControlValue::Make<FVector>(FVector::ZeroVector);
				ControlElement->Settings.MaximumValue = FRigControlValue::Make<FVector>(FVector::OneVector);
				break;
			}
			case ERigControlType::Rotator:
			{
				ValueToSet = FRigControlValue::Make<FRotator>(FRotator::ZeroRotator);
				ControlElement->Settings.SetupLimitArrayForType(false, false);
				ControlElement->Settings.MinimumValue = FRigControlValue::Make<FRotator>(FRotator::ZeroRotator);
				ControlElement->Settings.MaximumValue = FRigControlValue::Make<FRotator>(FRotator(180.f, 180.f, 180.f));
				break;
			}
			case ERigControlType::Transform:
			{
				ValueToSet = FRigControlValue::Make<FTransform>(FTransform::Identity);
				ControlElement->Settings.SetupLimitArrayForType(false, false, false);
				ControlElement->Settings.MinimumValue = ValueToSet;
				ControlElement->Settings.MaximumValue = ValueToSet;
				break;
			}
			case ERigControlType::TransformNoScale:
			{
				FTransformNoScale Identity = FTransform::Identity;
				ValueToSet = FRigControlValue::Make<FTransformNoScale>(Identity);
				ControlElement->Settings.SetupLimitArrayForType(false, false, false);
				ControlElement->Settings.MinimumValue = ValueToSet;
				ControlElement->Settings.MaximumValue = ValueToSet;
				break;
			}
			case ERigControlType::EulerTransform:
			{
				FEulerTransform Identity = FEulerTransform::Identity;
				ValueToSet = FRigControlValue::Make<FEulerTransform>(Identity);
				ControlElement->Settings.SetupLimitArrayForType(false, false, false);
				ControlElement->Settings.MinimumValue = ValueToSet;
				ControlElement->Settings.MaximumValue = ValueToSet;
				break;
			}
			default:
			{
				ensure(false);
				break;
			}
		}

		HierarchyToChange->SetControlSettings(ControlElement, ControlElement->Settings, true, true, true);
		HierarchyToChange->SetControlValue(ControlElement, ValueToSet, ERigControlValueType::Initial, true, false, true);
		HierarchyToChange->SetControlValue(ControlElement, ValueToSet, ERigControlValueType::Current, true, false, true);

		Info.WrapperObject->SetContent<FRigControlElement>(*ControlElement);

		if (HierarchyToChange != Hierarchy)
		{
			if(FRigControlElement* OtherControlElement = Info.GetElement<FRigControlElement>())
			{
				OtherControlElement->Settings = ControlElement->Settings;
				Hierarchy->SetControlSettings(OtherControlElement, OtherControlElement->Settings, true, true, true);
				Hierarchy->SetControlValue(OtherControlElement, ValueToSet, ERigControlValueType::Initial, true);
				Hierarchy->SetControlValue(OtherControlElement, ValueToSet, ERigControlValueType::Current, true);
			}
		}
		else
		{
			Info.GetBlueprint()->PropagateHierarchyFromBPToInstances();
		}
	}
	
	PropertyUtilities->ForceRefresh();
}

void FRigControlElementDetails::CustomizeShape(IDetailLayoutBuilder& DetailBuilder)
{
	if(PerElementInfos.IsEmpty())
	{
		return;
	}

	if(ContainsElementByPredicate([](const FPerElementInfo& Info)
	{
		if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
		{
			return !ControlElement->Settings.SupportsShape();
		}
		return true;
	}))
	{
		return;
	}

	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bIsEnabled = !bIsProcedural;

	TSharedPtr<IPropertyHandle> ShapeHandle = DetailBuilder.GetProperty(TEXT("Shape"));
	TSharedPtr<IPropertyHandle> InitialHandle = ShapeHandle->GetChildHandle(TEXT("Initial"));
	TSharedPtr<IPropertyHandle> LocalHandle = InitialHandle->GetChildHandle(TEXT("Local"));
	ShapeTransformHandle = LocalHandle->GetChildHandle(TEXT("Transform"));
	
	ShapeNameList.Reset();
	
	if (UControlRigBlueprint* Blueprint = PerElementInfos[0].GetBlueprint())
	{
		const bool bUseNameSpace = Blueprint->ShapeLibraries.Num() > 1;
		for(TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : Blueprint->ShapeLibraries)
		{
			if (!ShapeLibrary.IsValid())
			{
				ShapeLibrary.LoadSynchronous();
			}
			if (ShapeLibrary.IsValid())
			{
				const FString NameSpace = bUseNameSpace ? ShapeLibrary->GetName() + TEXT(".") : FString();
				ShapeNameList.Add(MakeShared<FString>(NameSpace + ShapeLibrary->DefaultShape.ShapeName.ToString()));
				for (const FControlRigShapeDefinition& Shape : ShapeLibrary->Shapes)
				{
					ShapeNameList.Add(MakeShared<FString>(NameSpace + Shape.ShapeName.ToString()));
				}
			}
		}
	}

	IDetailCategoryBuilder& ShapeCategory = DetailBuilder.EditCategory(TEXT("Shape"), LOCTEXT("Shape", "Shape"));

	const TSharedPtr<IPropertyHandle> SettingsHandle = DetailBuilder.GetProperty(TEXT("Settings"));

	if(!IsAnyControlNotOfAnimationType(ERigControlAnimationType::ProxyControl))
	{
		ShapeCategory.AddProperty(SettingsHandle->GetChildHandle(TEXT("ShapeVisibility")).ToSharedRef())
		.IsEnabled(bIsEnabled)
		.DisplayName(FText::FromString(TEXT("Visibility Mode")));
	}

	ShapeCategory.AddProperty(SettingsHandle->GetChildHandle(TEXT("bShapeVisible")).ToSharedRef())
	.IsEnabled(bIsEnabled)
	.DisplayName(FText::FromString(TEXT("Visible")));

	IDetailGroup& ShapePropertiesGroup = ShapeCategory.AddGroup(TEXT("Shape Properties"), LOCTEXT("ShapeProperties", "Shape Properties"));
	ShapePropertiesGroup.HeaderRow()
	.IsEnabled(bIsEnabled)
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("ShapeProperties", "Shape Properties"))
		.ToolTipText(LOCTEXT("ShapePropertiesTooltip", "Customize the properties of the shape"))
	]
	.CopyAction(FUIAction(
		FExecuteAction::CreateSP(this, &FRigControlElementDetails::OnCopyShapeProperties)))
	.PasteAction(FUIAction(
		FExecuteAction::CreateSP(this, &FRigControlElementDetails::OnPasteShapeProperties),
		FCanExecuteAction::CreateLambda([bIsEnabled]() { return bIsEnabled; })));
	
	// setup shape transform
	SAdvancedTransformInputBox<FEulerTransform>::FArguments TransformWidgetArgs = SAdvancedTransformInputBox<FEulerTransform>::FArguments()
	.IsEnabled(bIsEnabled)
	.DisplayToggle(false)
	.DisplayRelativeWorld(false)
	.Font(IDetailLayoutBuilder::GetDetailFont());

	TArray<FRigElementKey> Keys = GetElementKeys();
	Keys = PerElementInfos[0].GetHierarchy()->SortKeys(Keys);

	auto GetShapeTransform = [this](
		const FRigElementKey& Key
		) -> FEulerTransform
	{
		if(const FPerElementInfo& Info = FindElement(Key))
		{
			if(FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
			{
				return FEulerTransform(Info.GetHierarchy()->GetControlShapeTransform(ControlElement, ERigTransformType::InitialLocal));
			}
		}
		return FEulerTransform::Identity;
	};

	auto SetShapeTransform = [this](
		const FRigElementKey& Key,
		const FEulerTransform& InTransform,
		bool bSetupUndo
		)
	{
		if(const FPerElementInfo& Info = FindElement(Key))
		{
			if(const FRigControlElement* ControlElement = Info.GetDefaultElement<FRigControlElement>())
			{
				Info.GetDefaultHierarchy()->SetControlShapeTransform((FRigControlElement*)ControlElement, InTransform.ToFTransform(), ERigTransformType::InitialLocal, bSetupUndo, true, bSetupUndo);
			}
		}
	};

	TransformWidgetArgs.OnGetNumericValue_Lambda([Keys, GetShapeTransform](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent) -> TOptional<FVector::FReal>
	{
		TOptional<FVector::FReal> FirstValue;

		for(int32 Index = 0; Index < Keys.Num(); Index++)
		{
			const FRigElementKey& Key = Keys[Index];
			FEulerTransform Xfo = GetShapeTransform(Key);

			TOptional<FVector::FReal> CurrentValue = SAdvancedTransformInputBox<FEulerTransform>::GetNumericValueFromTransform(Xfo, Component, Representation, SubComponent);
			if(!CurrentValue.IsSet())
			{
				return CurrentValue;
			}

			if(Index == 0)
			{
				FirstValue = CurrentValue;
			}
			else
			{
				if(!FMath::IsNearlyEqual(FirstValue.GetValue(), CurrentValue.GetValue()))
				{
					return TOptional<FVector::FReal>();
				}
			}
		}
		
		return FirstValue;
	});

	TransformWidgetArgs.OnNumericValueChanged_Lambda(
	[
		Keys,
		this,
		GetShapeTransform,
		SetShapeTransform
	](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FVector::FReal InNumericValue)
	{
		for(const FRigElementKey& Key : Keys)
		{
			FEulerTransform Transform = GetShapeTransform(Key);
			FEulerTransform PreviousTransform = Transform;
			SAdvancedTransformInputBox<FEulerTransform>::ApplyNumericValueChange(Transform, InNumericValue, Component, Representation, SubComponent);

			if(!FRigControlElementDetails::Equals(Transform, PreviousTransform))
			{
				if(!SliderTransaction.IsValid())
				{
					SliderTransaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("ControlRigElementDetails", "ChangeNumericValue", "Change Numeric Value")));
					PerElementInfos[0].GetDefaultHierarchy()->Modify();
				}
				SetShapeTransform(Key, Transform, false);
			}
		}
	});

	TransformWidgetArgs.OnNumericValueCommitted_Lambda(
	[
		Keys,
		this,
		GetShapeTransform,
		SetShapeTransform
	](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FVector::FReal InNumericValue,
		ETextCommit::Type InCommitType)
	{
		{
			FScopedTransaction Transaction(LOCTEXT("ChangeNumericValue", "Change Numeric Value"));
			PerElementInfos[0].GetDefaultHierarchy()->Modify();

			for(const FRigElementKey& Key : Keys)
			{
				FEulerTransform Transform = GetShapeTransform(Key);
				FEulerTransform PreviousTransform = Transform;
				SAdvancedTransformInputBox<FEulerTransform>::ApplyNumericValueChange(Transform, InNumericValue, Component, Representation, SubComponent);
				if(!FRigControlElementDetails::Equals(Transform, PreviousTransform))
				{
					SetShapeTransform(Key, Transform, true);
				}
			}
		}
		SliderTransaction.Reset();
	});

	TransformWidgetArgs.OnCopyToClipboard_Lambda([Keys, GetShapeTransform](
		ESlateTransformComponent::Type InComponent
		)
	{
		if(Keys.Num() == 0)
		{
			return;
		}

		const FRigElementKey& FirstKey = Keys[0];
		FEulerTransform Xfo = GetShapeTransform(FirstKey);

		FString Content;
		switch(InComponent)
		{
			case ESlateTransformComponent::Location:
			{
				const FVector Data = Xfo.GetLocation();
				TBaseStructure<FVector>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
				break;
			}
			case ESlateTransformComponent::Rotation:
			{
				const FRotator Data = Xfo.Rotator();
				TBaseStructure<FRotator>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
				break;
			}
			case ESlateTransformComponent::Scale:
			{
				const FVector Data = Xfo.GetScale3D();
				TBaseStructure<FVector>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
				break;
			}
			case ESlateTransformComponent::Max:
			default:
			{
				TBaseStructure<FEulerTransform>::Get()->ExportText(Content, &Xfo, &Xfo, nullptr, PPF_None, nullptr);
				break;
			}
		}

		if(!Content.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Content);
		}
	});

	TransformWidgetArgs.OnPasteFromClipboard_Lambda([Keys, GetShapeTransform, SetShapeTransform, this](
		ESlateTransformComponent::Type InComponent
		)
	{
		if(Keys.Num() == 0)
		{
			return;
		}

		FString Content;
		FPlatformApplicationMisc::ClipboardPaste(Content);

		if(Content.IsEmpty())
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("PasteTransform", "Paste Transform"));
		PerElementInfos[0].GetDefaultHierarchy()->Modify();

		for(const FRigElementKey& Key : Keys)
		{
			FEulerTransform Xfo = GetShapeTransform(Key);
			{
				class FRigPasteTransformWidgetErrorPipe : public FOutputDevice
				{
				public:

					int32 NumErrors;

					FRigPasteTransformWidgetErrorPipe()
						: FOutputDevice()
						, NumErrors(0)
					{
					}

					virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
					{
						UE_LOG(LogControlRig, Error, TEXT("Error Pasting to Widget: %s"), V);
						NumErrors++;
					}
				};

				FRigPasteTransformWidgetErrorPipe ErrorPipe;
				
				switch(InComponent)
				{
					case ESlateTransformComponent::Location:
					{
						FVector Data = Xfo.GetLocation();
						TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
						Xfo.SetLocation(Data);
						break;
					}
					case ESlateTransformComponent::Rotation:
					{
						FRotator Data = Xfo.Rotator();
						TBaseStructure<FRotator>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FRotator>::Get()->GetName(), true);
						Xfo.SetRotator(Data);
						break;
					}
					case ESlateTransformComponent::Scale:
					{
						FVector Data = Xfo.GetScale3D();
						TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
						Xfo.SetScale3D(Data);
						break;
					}
					case ESlateTransformComponent::Max:
					default:
					{
						TBaseStructure<FEulerTransform>::Get()->ImportText(*Content, &Xfo, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FEulerTransform>::Get()->GetName(), true);
						break;
					}
				}

				if(ErrorPipe.NumErrors == 0)
				{
					SetShapeTransform(Key, Xfo, true);
				}
			}
		}
	});

	TransformWidgetArgs.DiffersFromDefault_Lambda([
		Keys,
		GetShapeTransform
	](
		ESlateTransformComponent::Type InComponent) -> bool
	{
		for(const FRigElementKey& Key : Keys)
		{
			const FEulerTransform CurrentTransform = GetShapeTransform(Key);
			static const FEulerTransform DefaultTransform = FEulerTransform::Identity;

			switch(InComponent)
			{
				case ESlateTransformComponent::Location:
				{
					if(!DefaultTransform.GetLocation().Equals(CurrentTransform.GetLocation()))
					{
						return true;
					}
					break;
				}
				case ESlateTransformComponent::Rotation:
				{
					if(!DefaultTransform.Rotator().Equals(CurrentTransform.Rotator()))
					{
						return true;
					}
					break;
				}
				case ESlateTransformComponent::Scale:
				{
					if(!DefaultTransform.GetScale3D().Equals(CurrentTransform.GetScale3D()))
					{
						return true;
					}
					break;
				}
				default: // also no component whole transform
				{
					if(!DefaultTransform.GetLocation().Equals(CurrentTransform.GetLocation()) ||
						!DefaultTransform.Rotator().Equals(CurrentTransform.Rotator()) ||
						!DefaultTransform.GetScale3D().Equals(CurrentTransform.GetScale3D()))
					{
						return true;
					}
					break;
				}
			}
		}
		return false;
	});

	TransformWidgetArgs.OnResetToDefault_Lambda([Keys, GetShapeTransform, SetShapeTransform, this](
		ESlateTransformComponent::Type InComponent)
	{
		FScopedTransaction Transaction(LOCTEXT("ResetTransformToDefault", "Reset Transform to Default"));
		PerElementInfos[0].GetDefaultHierarchy()->Modify();

		for(const FRigElementKey& Key : Keys)
		{
			FEulerTransform CurrentTransform = GetShapeTransform(Key);
			static const FEulerTransform DefaultTransform = FEulerTransform::Identity; 

			switch(InComponent)
			{
				case ESlateTransformComponent::Location:
				{
					CurrentTransform.SetLocation(DefaultTransform.GetLocation());
					break;
				}
				case ESlateTransformComponent::Rotation:
				{
					CurrentTransform.SetRotator(DefaultTransform.Rotator());
					break;
				}
				case ESlateTransformComponent::Scale:
				{
					CurrentTransform.SetScale3D(DefaultTransform.GetScale3D());
					break;
				}
				default: // whole transform / max component
				{
					CurrentTransform = DefaultTransform;
					break;
				}
			}

			SetShapeTransform(Key, CurrentTransform, true);
		}
	});

	TArray<FRigControlElement*> ControlElements;
	Algo::Transform(PerElementInfos, ControlElements, [](const FPerElementInfo& Info)
	{
		return Info.GetElement<FRigControlElement>();
	});

	SAdvancedTransformInputBox<FEulerTransform>::ConstructGroupedTransformRows(
		ShapeCategory, 
		LOCTEXT("ShapeTransform", "Shape Transform"), 
		LOCTEXT("ShapeTransformTooltip", "The relative transform of the shape under the control"),
		TransformWidgetArgs);

	ShapeNameHandle = SettingsHandle->GetChildHandle(TEXT("ShapeName"));
	ShapePropertiesGroup.AddPropertyRow(ShapeNameHandle.ToSharedRef()).CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.IsEnabled(bIsEnabled)
		.Text(FText::FromString(TEXT("Shape")))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.IsEnabled(this, &FRigControlElementDetails::IsShapeEnabled)
	]
	.ValueContent()
	[
		SAssignNew(ShapeNameListWidget, SControlRigShapeNameList, ControlElements, PerElementInfos[0].GetBlueprint())
		.OnGetNameListContent(this, &FRigControlElementDetails::GetShapeNameList)
		.IsEnabled(this, &FRigControlElementDetails::IsShapeEnabled)
	];

	ShapeColorHandle = SettingsHandle->GetChildHandle(TEXT("ShapeColor"));
	ShapePropertiesGroup.AddPropertyRow(ShapeColorHandle.ToSharedRef())
	.IsEnabled(bIsEnabled)
	.DisplayName(FText::FromString(TEXT("Color")));
}

void FRigControlElementDetails::BeginDestroy()
{
	FRigTransformElementDetails::BeginDestroy();

	if(ShapeNameListWidget.IsValid())
	{
		ShapeNameListWidget->BeginDestroy();
	}
}

void FRigControlElementDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass)
{
	FRigTransformElementDetails::RegisterSectionMappings(PropertyEditorModule, InClass);

	TSharedRef<FPropertySection> ControlSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Control", LOCTEXT("Control", "Control"));
	ControlSection->AddCategory("General");
	ControlSection->AddCategory("Control");
	ControlSection->AddCategory("Value");
	ControlSection->AddCategory("AnimationChannels");

	TSharedRef<FPropertySection> ShapeSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Shape", LOCTEXT("Shape", "Shape"));
	ShapeSection->AddCategory("General");
	ShapeSection->AddCategory("Shape");

	TSharedRef<FPropertySection> ChannelsSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Channels", LOCTEXT("Channels", "Channels"));
	ChannelsSection->AddCategory("AnimationChannels");
}

bool FRigControlElementDetails::IsShapeEnabled() const
{
	if(IsAnyElementProcedural())
	{
		 return false;
	}
	
	return ContainsElementByPredicate([](const FPerElementInfo& Info)
	{
		if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
		{
			return ControlElement->Settings.SupportsShape();
		}
		return false;
	});
}

const TArray<TSharedPtr<FString>>& FRigControlElementDetails::GetShapeNameList() const
{
	return ShapeNameList;
}

FText FRigControlElementDetails::GetDisplayName() const
{
	FName DisplayName(NAME_None);

	for(int32 ObjectIndex = 0; ObjectIndex < PerElementInfos.Num(); ObjectIndex++)
	{
		const FPerElementInfo& Info = PerElementInfos[ObjectIndex];
		if(const FRigControlElement* ControlElement = Info.GetDefaultElement<FRigControlElement>())
		{
 			const FName ThisDisplayName =
 				(ControlElement->IsAnimationChannel()) ?
 				ControlElement->GetDisplayName() :
				ControlElement->Settings.DisplayName;

			if(ObjectIndex == 0)
			{
				DisplayName = ThisDisplayName;
			}
			else if(DisplayName != ThisDisplayName)
			{
				return ControlRigDetailsMultipleValues;
			}
		}
	}

	if(!DisplayName.IsNone())
	{
		return FText::FromName(DisplayName);
	}
	return FText();
}

void FRigControlElementDetails::SetDisplayName(const FText& InNewText, ETextCommit::Type InCommitType)
{
	for(int32 ObjectIndex = 0; ObjectIndex < PerElementInfos.Num(); ObjectIndex++)
	{
		const FPerElementInfo& Info = PerElementInfos[ObjectIndex];
		if(const FRigControlElement* ControlElement = Info.GetDefaultElement<FRigControlElement>())
		{
			SetDisplayNameForElement(InNewText, InCommitType, ControlElement->GetKey());
		}
	}
}

void FRigControlElementDetails::SetDisplayNameForElement(const FText& InNewText, ETextCommit::Type InCommitType, const FRigElementKey& InKeyToRename)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	const FPerElementInfo& Info = FindElement(InKeyToRename);
	if(!Info.IsValid())
	{
		return;
	}

	if(Info.IsProcedural())
	{
		return;
	}

	const FName DisplayName = InNewText.IsEmpty() ? FName(NAME_None) : FName(*InNewText.ToString());
	const bool bRename = IsAnyControlOfAnimationType(ERigControlAnimationType::AnimationChannel);
	Info.GetDefaultHierarchy()->GetController(true)->SetDisplayName(InKeyToRename, DisplayName, bRename, true, true);
}

bool FRigControlElementDetails::OnVerifyDisplayNameChanged(const FText& InText, FText& OutErrorMessage, const FRigElementKey& InKeyToRename)
{
	const FString NewName = InText.ToString();
	if (NewName.IsEmpty())
	{
		OutErrorMessage = FText::FromString(TEXT("Name is empty."));
		return false;
	}

	const FPerElementInfo& Info = FindElement(InKeyToRename);
	if(!Info.IsValid())
	{
		return false;
	}

	if(Info.IsProcedural())
	{
		return false;
	}

	// make sure there is no duplicate
	if(const URigHierarchy* Hierarchy = Info.GetDefaultHierarchy())
	{
		if(const FRigControlElement* ControlElement = Info.GetDefaultElement<FRigControlElement>())
		{
			if(const FRigBaseElement* ParentElement = Hierarchy->GetFirstParent(ControlElement))
			{
				FString OutErrorString;
				if (!Hierarchy->IsDisplayNameAvailable(ParentElement->GetKey(), NewName, &OutErrorString))
				{
					OutErrorMessage = FText::FromString(OutErrorString);
					return false;
				}
			}
		}
	}
	return true;
}

void FRigControlElementDetails::OnCopyShapeProperties()
{
	FString Value;

	if (!PerElementInfos.IsEmpty())
	{
		if(const FRigControlElement* ControlElement = PerElementInfos[0].GetElement<FRigControlElement>())
		{
			Value = FString::Printf(TEXT("(ShapeName=\"%s\",ShapeColor=%s,Transform=%s)"),
				*ControlElement->Settings.ShapeName.ToString(),
				*ControlElement->Settings.ShapeColor.ToString(),
				*ControlElement->Shape.Initial.Local.Transform.ToString());
		}
	}
		
	if (!Value.IsEmpty())
	{
		// Copy.
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}
}

void FRigControlElementDetails::OnPasteShapeProperties()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);
	
	FString TrimmedText = PastedText.LeftChop(1).RightChop(1);
	FString ShapeName;
	FString ShapeColorStr;
	FString TransformStr;
	bool bSuccessful = FParse::Value(*TrimmedText, TEXT("ShapeName="), ShapeName) &&
					   FParse::Value(*TrimmedText, TEXT("ShapeColor="), ShapeColorStr, false) &&
					   FParse::Value(*TrimmedText, TEXT("Transform="), TransformStr, false);

	if (bSuccessful)
	{
		FScopedTransaction Transaction(LOCTEXT("PasteShape", "Paste Shape"));
		
		// Name
		{
			ShapeNameHandle->NotifyPreChange();
			ShapeNameHandle->SetValue(ShapeName);
			ShapeNameHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
		
		// Color
		{
			ShapeColorHandle->NotifyPreChange();
			TArray<void*> RawDataPtrs;
			ShapeColorHandle->AccessRawData(RawDataPtrs);
			for (void* RawPtr: RawDataPtrs)
			{
				bSuccessful &= static_cast<FLinearColor*>(RawPtr)->InitFromString(ShapeColorStr);
				if (!bSuccessful)
				{
					Transaction.Cancel();
					return;
				}
			}		
			ShapeColorHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}

		// Transform
		{
			ShapeTransformHandle->NotifyPreChange();
			TArray<void*> RawDataPtrs;
			ShapeTransformHandle->AccessRawData(RawDataPtrs);
			for (void* RawPtr: RawDataPtrs)
			{
				bSuccessful &= static_cast<FTransform*>(RawPtr)->InitFromString(TransformStr);
				if (!bSuccessful)
				{
					Transaction.Cancel();
					return;
				}
			}		
			ShapeTransformHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}		
	}
}

FDetailWidgetRow& FRigControlElementDetails::CreateBoolValueWidgetRow(
	const TArray<FRigElementKey>& Keys,
	IDetailCategoryBuilder& CategoryBuilder,
	const FText& Label,
	const FText& Tooltip,
	ERigControlValueType ValueType,
	TAttribute<EVisibility> Visibility,
	TSharedPtr<SWidget> NameContent)
{
	const static TCHAR* TrueText = TEXT("True");
	const static TCHAR* FalseText = TEXT("False");
	
	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bIsEnabled = !bIsProcedural || ValueType == ERigControlValueType::Current;

	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();
	if(ValueType == ERigControlValueType::Current)
	{
		HierarchyToChange = Hierarchy;
	}

	if(!NameContent.IsValid())
	{
		SAssignNew(NameContent, STextBlock)
		.Text(Label)
		.ToolTipText(Tooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.IsEnabled(bIsEnabled);
	}

	FDetailWidgetRow& WidgetRow = CategoryBuilder.AddCustomRow(Label)
	.Visibility(Visibility)
	.NameContent()
	.MinDesiredWidth(200.f)
	.MaxDesiredWidth(800.f)
	[
		NameContent.ToSharedRef()
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked_Lambda([ValueType, Keys, Hierarchy]() -> ECheckBoxState
		{
			const bool FirstValue = Hierarchy->GetControlValue<bool>(Keys[0], ValueType);
			for(int32 Index = 1; Index < Keys.Num(); Index++)
			{
				const bool SecondValue = Hierarchy->GetControlValue<bool>(Keys[Index], ValueType);
				if(FirstValue != SecondValue)
				{
					return ECheckBoxState::Undetermined;
				}
			}
			return FirstValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([ValueType, Keys, HierarchyToChange](ECheckBoxState NewState)
		{
			if(NewState == ECheckBoxState::Undetermined)
			{
				return;
			}

			const bool Value = NewState == ECheckBoxState::Checked;
			FScopedTransaction Transaction(LOCTEXT("ChangeValue", "Change Value"));
			HierarchyToChange->Modify();
			for(const FRigElementKey& Key : Keys)
			{
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<bool>(Value), ValueType, true, true); 
			}
		})
		.IsEnabled(bIsEnabled)
	]
	.CopyAction(FUIAction(
	FExecuteAction::CreateLambda([ValueType, Keys, Hierarchy]()
		{
			const bool FirstValue = Hierarchy->GetControlValue<bool>(Keys[0], ValueType);
			FPlatformApplicationMisc::ClipboardCopy(FirstValue ? TrueText : FalseText);
		}),
		FCanExecuteAction())
	)
	.PasteAction(FUIAction(
		FExecuteAction::CreateLambda([ValueType, Keys, HierarchyToChange]()
		{
			FString Content;
			FPlatformApplicationMisc::ClipboardPaste(Content);

			const bool Value = FToBoolHelper::FromCStringWide(*Content);
			FScopedTransaction Transaction(LOCTEXT("ChangeValue", "Change Value"));
			HierarchyToChange->Modify();
			for(const FRigElementKey& Key : Keys)
			{
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<bool>(Value), ValueType, true, true); 
			}
		}),
		FCanExecuteAction::CreateLambda([bIsEnabled]() { return bIsEnabled; }))
	)
	.OverrideResetToDefault(FResetToDefaultOverride::Create(
		TAttribute<bool>::CreateLambda([ValueType, Keys, Hierarchy, bIsEnabled]() -> bool
		{
			if(!bIsEnabled)
			{
				return false;
			}
			
			const bool FirstValue = Hierarchy->GetControlValue<bool>(Keys[0], ValueType);
			const bool ReferenceValue = ValueType == ERigControlValueType::Initial ? false :
				Hierarchy->GetControlValue<bool>(Keys[0], ERigControlValueType::Initial);

			return FirstValue != ReferenceValue;
		}),
		FSimpleDelegate::CreateLambda([ValueType, Keys, HierarchyToChange]()
		{
			FScopedTransaction Transaction(LOCTEXT("ResetValueToDefault", "Reset Value To Default"));
			HierarchyToChange->Modify();
			for(const FRigElementKey& Key : Keys)
			{
				const bool ReferenceValue = ValueType == ERigControlValueType::Initial ? false :
					HierarchyToChange->GetControlValue<bool>(Keys[0], ERigControlValueType::Initial);
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<bool>(ReferenceValue), ValueType, true, true); 
			}
		})
	));

	return WidgetRow;
}

FDetailWidgetRow& FRigControlElementDetails::CreateFloatValueWidgetRow(
	const TArray<FRigElementKey>& Keys,
	IDetailCategoryBuilder& CategoryBuilder,
	const FText& Label,
	const FText& Tooltip,
	ERigControlValueType ValueType,
	TAttribute<EVisibility> Visibility,
	TSharedPtr<SWidget> NameContent)
{
	return CreateNumericValueWidgetRow<float>(Keys, CategoryBuilder, Label, Tooltip, ValueType, Visibility, NameContent);
}

FDetailWidgetRow& FRigControlElementDetails::CreateIntegerValueWidgetRow(
	const TArray<FRigElementKey>& Keys,
	IDetailCategoryBuilder& CategoryBuilder,
	const FText& Label,
	const FText& Tooltip,
	ERigControlValueType ValueType,
	TAttribute<EVisibility> Visibility,
	TSharedPtr<SWidget> NameContent)
{
	return CreateNumericValueWidgetRow<int32>(Keys, CategoryBuilder, Label, Tooltip, ValueType, Visibility, NameContent);
}

FDetailWidgetRow& FRigControlElementDetails::CreateEnumValueWidgetRow(
	const TArray<FRigElementKey>& Keys,
	IDetailCategoryBuilder& CategoryBuilder,
	const FText& Label,
	const FText& Tooltip,
	ERigControlValueType ValueType,
	TAttribute<EVisibility> Visibility,
	TSharedPtr<SWidget> NameContent)
{
	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bIsEnabled = !bIsProcedural || ValueType == ERigControlValueType::Current;

	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();
	if(ValueType == ERigControlValueType::Current)
	{
		HierarchyToChange = Hierarchy;
	}

	UEnum* Enum = nullptr;
	for(const FRigElementKey& Key : Keys)
	{
		if(const FPerElementInfo& Info = FindElement(Key))
		{
			if(const FRigControlElement* ControlElement = Info.GetElement<FRigControlElement>())
			{
				Enum = ControlElement->Settings.ControlEnum.Get();
				if(Enum)
				{
					break;
				}
			}
		}
	}

	check(Enum != nullptr);

	if(!NameContent.IsValid())
	{
		SAssignNew(NameContent, STextBlock)
		.Text(Label)
		.ToolTipText(Tooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.IsEnabled(bIsEnabled);
	}

	FDetailWidgetRow& WidgetRow = CategoryBuilder.AddCustomRow(Label)
	.Visibility(Visibility)
	.NameContent()
	.MinDesiredWidth(200.f)
	.MaxDesiredWidth(800.f)
	[
		NameContent.ToSharedRef()
	]
	.ValueContent()
	[
		SNew(SEnumComboBox, Enum)
		.CurrentValue_Lambda([ValueType, Keys, Hierarchy]() -> int32
		{
			const int32 FirstValue = Hierarchy->GetControlValue<int32>(Keys[0], ValueType);
			for(int32 Index = 1; Index < Keys.Num(); Index++)
			{
				const int32 SecondValue = Hierarchy->GetControlValue<int32>(Keys[Index], ValueType);
				if(FirstValue != SecondValue)
				{
					return INDEX_NONE;
				}
			}
			return FirstValue;
		})
		.OnEnumSelectionChanged_Lambda([ValueType, Keys, HierarchyToChange](int32 NewSelection, ESelectInfo::Type)
		{
			if(NewSelection == INDEX_NONE)
			{
				return;
			}

			FScopedTransaction Transaction(LOCTEXT("ChangeValue", "Change Value"));
			HierarchyToChange->Modify();
			for(const FRigElementKey& Key : Keys)
			{
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<int32>(NewSelection), ValueType, true, true); 
			}
		})
		.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
		.IsEnabled(bIsEnabled)
	]
	.CopyAction(FUIAction(
	FExecuteAction::CreateLambda([ValueType, Keys, Hierarchy]()
		{
			const int32 FirstValue = Hierarchy->GetControlValue<int32>(Keys[0], ValueType);
			FPlatformApplicationMisc::ClipboardCopy(*FString::FromInt(FirstValue));
		}),
		FCanExecuteAction())
	)
	.PasteAction(FUIAction(
		FExecuteAction::CreateLambda([ValueType, Keys, HierarchyToChange]()
		{
			FString Content;
			FPlatformApplicationMisc::ClipboardPaste(Content);
			if(!Content.IsNumeric())
			{
				return;
			}

			const int32 Value = FCString::Atoi(*Content);
			FScopedTransaction Transaction(LOCTEXT("ChangeValue", "Change Value"));
			HierarchyToChange->Modify();

			for(const FRigElementKey& Key : Keys)
			{
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<int32>(Value), ValueType, true, true); 
			}
		}),
		FCanExecuteAction::CreateLambda([bIsEnabled]() { return bIsEnabled; }))
	)
	.OverrideResetToDefault(FResetToDefaultOverride::Create(
		TAttribute<bool>::CreateLambda([ValueType, Keys, Hierarchy, bIsEnabled]() -> bool
		{
			if(!bIsEnabled)
			{
				return false;
			}
			
			const int32 FirstValue = Hierarchy->GetControlValue<int32>(Keys[0], ValueType);
			const int32 ReferenceValue = ValueType == ERigControlValueType::Initial ? 0 :
				Hierarchy->GetControlValue<int32>(Keys[0], ERigControlValueType::Initial);

			return FirstValue != ReferenceValue;
		}),
		FSimpleDelegate::CreateLambda([ValueType, Keys, HierarchyToChange]()
		{
			FScopedTransaction Transaction(LOCTEXT("ResetValueToDefault", "Reset Value To Default"));
			HierarchyToChange->Modify();
			for(const FRigElementKey& Key : Keys)
			{
				const int32 ReferenceValue = ValueType == ERigControlValueType::Initial ? 0 :
					HierarchyToChange->GetControlValue<int32>(Keys[0], ERigControlValueType::Initial);
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<int32>(ReferenceValue), ValueType, true, true); 
			}
		})
	));

	return WidgetRow;
}

FDetailWidgetRow& FRigControlElementDetails::CreateVector2DValueWidgetRow(
	const TArray<FRigElementKey>& Keys,
	IDetailCategoryBuilder& CategoryBuilder, 
	const FText& Label, 
	const FText& Tooltip, 
	ERigControlValueType ValueType,
	TAttribute<EVisibility> Visibility,
	TSharedPtr<SWidget> NameContent)
{
	const bool bIsProcedural = IsAnyElementProcedural();
	const bool bIsEnabled = !bIsProcedural || ValueType == ERigControlValueType::Current;
	const bool bShowToggle = (ValueType == ERigControlValueType::Minimum) || (ValueType == ERigControlValueType::Maximum);
	
	URigHierarchy* Hierarchy = PerElementInfos[0].GetHierarchy();
	URigHierarchy* HierarchyToChange = PerElementInfos[0].GetDefaultHierarchy();
	if(ValueType == ERigControlValueType::Current)
	{
		HierarchyToChange = Hierarchy;
	}

	using SNumericVector2DInputBox = SNumericVectorInputBox<float, FVector2f, 2>;
	TSharedPtr<SNumericVector2DInputBox> VectorInputBox;
	
	FDetailWidgetRow& WidgetRow = CategoryBuilder.AddCustomRow(Label);
	TAttribute<ECheckBoxState> ToggleXChecked, ToggleYChecked;
	FOnCheckStateChanged OnToggleXChanged, OnToggleYChanged;

	if(bShowToggle)
	{
		auto ToggleChecked = [ValueType, Keys, Hierarchy](int32 Index) -> ECheckBoxState
		{
			TOptional<bool> FirstValue;

			for(const FRigElementKey& Key : Keys)
			{
				if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
				{
					if(ControlElement->Settings.LimitEnabled.Num() == 2)
					{
						const bool Value = ControlElement->Settings.LimitEnabled[Index].GetForValueType(ValueType);
						if(FirstValue.IsSet())
						{
							if(FirstValue.GetValue() != Value)
							{
								return ECheckBoxState::Undetermined;
							}
						}
						else
						{
							FirstValue = Value;
						}
					}
				}
			}

			if(!ensure(FirstValue.IsSet()))
			{
				return ECheckBoxState::Undetermined;
			}

			return FirstValue.GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};
		
		ToggleXChecked = TAttribute<ECheckBoxState>::CreateLambda([ToggleChecked]() -> ECheckBoxState
		{
			return ToggleChecked(0);
		});

		ToggleYChecked = TAttribute<ECheckBoxState>::CreateLambda([ToggleChecked]() -> ECheckBoxState
		{
			return ToggleChecked(1);
		});

		auto OnToggleChanged = [ValueType, Keys, HierarchyToChange](ECheckBoxState InValue, int32 Index)
		{
			if(InValue == ECheckBoxState::Undetermined)
			{
				return;
			}
					
			FScopedTransaction Transaction(LOCTEXT("ChangeLimitToggle", "Change Limit Toggle"));
			HierarchyToChange->Modify();

			for(const FRigElementKey& Key : Keys)
			{
				if(FRigControlElement* ControlElement = HierarchyToChange->Find<FRigControlElement>(Key))
				{
					if(ControlElement->Settings.LimitEnabled.Num() == 2)
					{
						ControlElement->Settings.LimitEnabled[Index].SetForValueType(ValueType, InValue == ECheckBoxState::Checked);
						HierarchyToChange->SetControlSettings(ControlElement, ControlElement->Settings, true, true, true);
					}
				}
			}
		};

		OnToggleXChanged = FOnCheckStateChanged::CreateLambda([OnToggleChanged](ECheckBoxState InValue)
		{
			OnToggleChanged(InValue, 0);
		});

		OnToggleYChanged = FOnCheckStateChanged::CreateLambda([OnToggleChanged](ECheckBoxState InValue)
		{
			OnToggleChanged(InValue, 1);
		});
	}

	auto GetValue = [ValueType, Keys, Hierarchy](int32 Component) -> TOptional<float>
	{
		const float FirstValue = Hierarchy->GetControlValue<FVector3f>(Keys[0], ValueType).Component(Component);
		for(int32 Index = 1; Index < Keys.Num(); Index++)
		{
			const float SecondValue = Hierarchy->GetControlValue<FVector3f>(Keys[Index], ValueType).Component(Component);
			if(FirstValue != SecondValue)
			{
				return TOptional<float>();
			}
		}
		return FirstValue;
	};

	auto OnValueChanged = [ValueType, Keys, Hierarchy, HierarchyToChange, this]
		(TOptional<float> InValue, ETextCommit::Type InCommitType, bool bSetupUndo, int32 Component)
		{
			if(!InValue.IsSet())
			{
				return;
			}

			const float Value = InValue.GetValue();
		
			for(const FRigElementKey& Key : Keys)
			{
				FVector3f Vector = Hierarchy->GetControlValue<FVector3f>(Key, ValueType);
				if(!FMath::IsNearlyEqual(Vector.Component(Component), Value))
				{
					if(!SliderTransaction.IsValid())
					{
						SliderTransaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("ControlRigElementDetails", "ChangeValue", "Change Value")));
						HierarchyToChange->Modify();
					}
					Vector.Component(Component) = Value;
					HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<FVector3f>(Vector), ValueType, bSetupUndo, bSetupUndo);
				};
			}

			if(bSetupUndo)
			{
				SliderTransaction.Reset();
			}
	};

	if(!NameContent.IsValid())
	{
		SAssignNew(NameContent, STextBlock)
		.Text(Label)
		.ToolTipText(Tooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.IsEnabled(bIsEnabled);
	}

	WidgetRow
	.Visibility(Visibility)
	.NameContent()
	.MinDesiredWidth(200.f)
	.MaxDesiredWidth(800.f)
	[
		NameContent.ToSharedRef()
	]
	.ValueContent()
	[
		SAssignNew(VectorInputBox, SNumericVector2DInputBox)
        .Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
        .AllowSpin(ValueType == ERigControlValueType::Current || ValueType == ERigControlValueType::Initial)
		.SpinDelta(0.01f)
		.X_Lambda([GetValue]() -> TOptional<float>
        {
			return GetValue(0);
        })
        .Y_Lambda([GetValue]() -> TOptional<float>
		{
			return GetValue(1);
		})
		.OnXChanged_Lambda([OnValueChanged](TOptional<float> InValue)
		{
			OnValueChanged(InValue, ETextCommit::Default, false, 0);
		})
		.OnYChanged_Lambda([OnValueChanged](TOptional<float> InValue)
		{
			OnValueChanged(InValue, ETextCommit::Default, false, 1);
		})
		.OnXCommitted_Lambda([OnValueChanged](TOptional<float> InValue, ETextCommit::Type InCommitType)
		{
			OnValueChanged(InValue, InCommitType, true, 0);
		})
		.OnYCommitted_Lambda([OnValueChanged](TOptional<float> InValue, ETextCommit::Type InCommitType)
		{
			OnValueChanged(InValue, InCommitType, true, 1);
		})
		 .DisplayToggle(bShowToggle)
		 .ToggleXChecked(ToggleXChecked)
		 .ToggleYChecked(ToggleYChecked)
		 .OnToggleXChanged(OnToggleXChanged)
		 .OnToggleYChanged(OnToggleYChanged)
		 .IsEnabled(bIsEnabled)
	]
	.CopyAction(FUIAction(
	FExecuteAction::CreateLambda([ValueType, Keys, Hierarchy]()
		{
			const FVector3f Data3 = Hierarchy->GetControlValue<FVector3f>(Keys[0], ValueType);
			const FVector2f Data(Data3.X, Data3.Y);
			FString Content = Data.ToString();
			FPlatformApplicationMisc::ClipboardCopy(*Content);
		}),
		FCanExecuteAction())
	)
	.PasteAction(FUIAction(
		FExecuteAction::CreateLambda([ValueType, Keys, HierarchyToChange]()
		{
			FString Content;
			FPlatformApplicationMisc::ClipboardPaste(Content);
			if(Content.IsEmpty())
			{
				return;
			}

			FVector2f Data = FVector2f::ZeroVector;
			Data.InitFromString(Content);

			FVector3f Data3(Data.X, Data.Y, 0);

			FScopedTransaction Transaction(NSLOCTEXT("ControlRigElementDetails", "ChangeValue", "Change Value"));
			HierarchyToChange->Modify();
			
			for(const FRigElementKey& Key : Keys)
			{
				HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<FVector3f>(Data3), ValueType, true, true); 
			}
		}),
		FCanExecuteAction::CreateLambda([bIsEnabled]() { return bIsEnabled; }))
	);

	if((ValueType == ERigControlValueType::Current || ValueType == ERigControlValueType::Initial) && bIsEnabled)
	{
		WidgetRow.OverrideResetToDefault(FResetToDefaultOverride::Create(
			TAttribute<bool>::CreateLambda([ValueType, Keys, Hierarchy]() -> bool
			{
				const FVector3f FirstValue = Hierarchy->GetControlValue<FVector3f>(Keys[0], ValueType);
				const FVector3f ReferenceValue = ValueType == ERigControlValueType::Initial ? FVector3f::ZeroVector :
					Hierarchy->GetControlValue<FVector3f>(Keys[0], ERigControlValueType::Initial);

				return !(FirstValue - ReferenceValue).IsNearlyZero();
			}),
			FSimpleDelegate::CreateLambda([ValueType, Keys, HierarchyToChange]()
			{
				FScopedTransaction Transaction(LOCTEXT("ResetValueToDefault", "Reset Value To Default"));
				HierarchyToChange->Modify();
				
				for(const FRigElementKey& Key : Keys)
				{
					const FVector3f ReferenceValue = ValueType == ERigControlValueType::Initial ? FVector3f::ZeroVector :
						HierarchyToChange->GetControlValue<FVector3f>(Keys[0], ERigControlValueType::Initial);
					HierarchyToChange->SetControlValue(Key, FRigControlValue::Make<FVector3f>(ReferenceValue), ValueType, true, true); 
				}
			})
		));
	}

	return WidgetRow;
}

void FRigNullElementDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FRigTransformElementDetails::CustomizeDetails(DetailBuilder);
	CustomizeTransform(DetailBuilder);
	CustomizeMetadata(DetailBuilder);
}

#undef LOCTEXT_NAMESPACE
