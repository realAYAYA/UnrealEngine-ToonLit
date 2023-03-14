// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBagDetails.h"
#include "InstancedStructDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SPinTypeSelector.h"
#include "PropertyBag.h"
#include "Styling/AppStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PropertyBagDetails)

#define LOCTEXT_NAMESPACE "StructUtilsEditor"

////////////////////////////////////

namespace UE::StructUtils::Private
{

/** @return true property handle holds struct property of type T.  */ 
template<typename T>
bool IsScriptStruct(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	if (!PropertyHandle)
	{
		return false;
	}

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(PropertyHandle->GetProperty());
	return StructProperty && StructProperty->Struct == TBaseStructure<T>::Get();
}

/** @return true if the property is one of the known missing types. */
bool HasMissingType(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	if (!PropertyHandle)
	{
		return false;
	}

	// Handles Struct
	if (FStructProperty* StructProperty = CastField<FStructProperty>(PropertyHandle->GetProperty()))
	{
		return StructProperty->Struct == FPropertyBagMissingStruct::StaticStruct();
	}
	// Handles Object, SoftObject, Class, SoftClass.
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropertyHandle->GetProperty()))
	{
		return ObjectProperty->PropertyClass == UPropertyBagMissingObject::StaticClass();
	}
	// Handles Enum
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(PropertyHandle->GetProperty()))
	{
		return EnumProperty->GetEnum() == StaticEnum<EPropertyBagMissingEnum>();
	}

	return false;
}

/** @return property bag struct common to all edited properties. */
const UPropertyBag* GetCommonBagStruct(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UPropertyBag* CommonBagStruct = nullptr;

	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		StructProperty->EnumerateConstRawData([&CommonBagStruct](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (RawData)
			{
				const FInstancedPropertyBag* Bag = static_cast<const FInstancedPropertyBag*>(RawData);

				const UPropertyBag* BagStruct = Bag->GetPropertyBagStruct();
				if (CommonBagStruct && CommonBagStruct != BagStruct)
				{
					// Multiple struct types on the sources - show nothing set
					CommonBagStruct = nullptr;
					return false;
				}
				CommonBagStruct = BagStruct;
			}

			return true;
		});
	}

	return CommonBagStruct;
}

/** @return property descriptors of the property bag struct common to all edited properties. */
TArray<FPropertyBagPropertyDesc> GetCommonPropertyDescs(const TSharedPtr<IPropertyHandle>& StructProperty)
{
	TArray<FPropertyBagPropertyDesc> PropertyDescs;
	
	if (const UPropertyBag* BagStruct = GetCommonBagStruct(StructProperty))
	{
		PropertyDescs = BagStruct->GetPropertyDescs();
	}
	
	return PropertyDescs;
}

/** Creates new property bag struct and sets all properties to use it, migrating over old values. */
void SetPropertyDescs(const TSharedPtr<IPropertyHandle>& StructProperty, const TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs)
{
	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		// Create new bag struct
		const UPropertyBag* NewBagStruct = UPropertyBag::GetOrCreateFromDescs(PropertyDescs);

		// Migrate structs to the new type, copying values over.
		StructProperty->EnumerateRawData([&NewBagStruct](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (RawData)
			{
				if (FInstancedPropertyBag* Bag = static_cast<FInstancedPropertyBag*>(RawData))
				{
					Bag->MigrateToNewBagStruct(NewBagStruct);
				}
			}

			return true;
		});
	}
}

/** @return sanitized property name based on the input string. */
FName GetValidPropertyName(const FString& Name)
{
	FName Result;
	if (!Name.IsEmpty())
	{
		if (!FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS))
		{
			Result = MakeObjectNameFromDisplayLabel(Name, NAME_None);
		}
		else
		{
			Result = FName(Name);
		}
	}
	else
	{
		Result = FName(TEXT("Property"));
	}

	return Result;
}

/** @return true of the property name is not used yet by the property bag structure common to all edited properties. */
bool IsUniqueName(const FName NewName, const FName OldName, const TSharedPtr<IPropertyHandle>& StructProperty)
{
	if (NewName == OldName)
	{
		return false;
	}
	
	bool bFound = false;

	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		StructProperty->EnumerateConstRawData([&bFound, NewName](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (const FInstancedPropertyBag* Bag = static_cast<const FInstancedPropertyBag*>(RawData))
			{
				if (const UPropertyBag* BagStruct = Bag->GetPropertyBagStruct())
				{
					const bool bContains = BagStruct->GetPropertyDescs().ContainsByPredicate([NewName](const FPropertyBagPropertyDesc& Desc)
					{
						return Desc.Name == NewName;
					});
					if (bContains)
					{
						bFound = true;
						return false; // Stop iterating
					}
				}
			}

			return true;
		});
	}
	
	return !bFound;
}

/** @return Blueprint pin type from property descriptor. */
FEdGraphPinType GetPropertyDescAsPin(const FPropertyBagPropertyDesc& Desc)
{
	UEnum* PropertyTypeEnum = StaticEnum<EPropertyBagPropertyType>();
	check(PropertyTypeEnum);
	const UPropertyBagSchema* Schema = GetDefault<UPropertyBagSchema>();
	check(Schema);

	FEdGraphPinType PinType;
	PinType.PinSubCategory = NAME_None;

	// Container type
	switch (Desc.ContainerType)
	{
	case EPropertyBagContainerType::Array:
		PinType.ContainerType = EPinContainerType::Array;
		break;
	default:
		PinType.ContainerType = EPinContainerType::None;
	}

	// Value type
	switch (Desc.ValueType)
	{
	case EPropertyBagPropertyType::Bool:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		break;
	case EPropertyBagPropertyType::Byte:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		break;
	case EPropertyBagPropertyType::Int32:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		break;
	case EPropertyBagPropertyType::Int64:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		break;
	case EPropertyBagPropertyType::Float:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		break;
	case EPropertyBagPropertyType::Double:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		break;
	case EPropertyBagPropertyType::Name:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		break;
	case EPropertyBagPropertyType::String:
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		break;
	case EPropertyBagPropertyType::Text:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		break;
	case EPropertyBagPropertyType::Enum:
		// @todo: some pin coloring is not correct due to this (byte-as-enum vs enum). 
		PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Struct:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Object:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Class:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftClass:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled value type %s"), *UEnum::GetValueAsString(Desc.ValueType));
		break;
	}

	return PinType;
}

/** Sets property descriptor based on a Blueprint pin type. */
void SetPropertyDescFromPin(FPropertyBagPropertyDesc& Desc, const FEdGraphPinType& PinType)
{
	const UPropertyBagSchema* Schema = GetDefault<UPropertyBagSchema>();
	check(Schema);

	// Container type
	switch (PinType.ContainerType)
	{
	case EPinContainerType::Array:
		Desc.ContainerType = EPropertyBagContainerType::Array;
		break;
	default:
		Desc.ContainerType = EPropertyBagContainerType::None;
	}
	
	// Value type
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		Desc.ValueType = EPropertyBagPropertyType::Bool;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject))
		{
			Desc.ValueType = EPropertyBagPropertyType::Enum;
			Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else
		{
			Desc.ValueType = EPropertyBagPropertyType::Byte;
			Desc.ValueTypeObject = nullptr;
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		Desc.ValueType = EPropertyBagPropertyType::Int32;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		Desc.ValueType = EPropertyBagPropertyType::Int64;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			Desc.ValueType = EPropertyBagPropertyType::Float;
			Desc.ValueTypeObject = nullptr;
		}
		else if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			Desc.ValueType = EPropertyBagPropertyType::Double;
			Desc.ValueTypeObject = nullptr;
		}		
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		Desc.ValueType = EPropertyBagPropertyType::Name;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		Desc.ValueType = EPropertyBagPropertyType::String;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		Desc.ValueType = EPropertyBagPropertyType::Text;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		Desc.ValueType = EPropertyBagPropertyType::Enum;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		Desc.ValueType = EPropertyBagPropertyType::Struct;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		Desc.ValueType = EPropertyBagPropertyType::Object;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		Desc.ValueType = EPropertyBagPropertyType::SoftObject;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		Desc.ValueType = EPropertyBagPropertyType::Class;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		Desc.ValueType = EPropertyBagPropertyType::SoftClass;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled pin category %s"), *PinType.PinCategory.ToString());
	}
}

template<typename TFunc>
void ApplyChangesToPropertyDescs(const FText& SessionName, const TSharedPtr<IPropertyHandle>& StructProperty, IPropertyUtilities* PropUtils, TFunc&& Function)
{
	FScopedTransaction Transaction(SessionName);
	TArray<FPropertyBagPropertyDesc> PropertyDescs = GetCommonPropertyDescs(StructProperty);
	StructProperty->NotifyPreChange();

	Function(PropertyDescs);

	SetPropertyDescs(StructProperty, PropertyDescs);
	
	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();
	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

bool CanHaveMemberVariableOfType(const FEdGraphPinType& PinType)
{
	if ((PinType.PinCategory == UEdGraphSchema_K2::PC_Struct))
	{
		if (const UObject* TypeObject = PinType.PinSubCategoryObject.Get())
		{
			if (TypeObject->IsA<UUserDefinedStruct>())
			{
				return false;
			}
		}
	}
	else if ((PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) 
		|| (PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		|| (PinType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		|| (PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate)
		|| (PinType.PinCategory == UEdGraphSchema_K2::PC_Interface))
	{
		return false;
	}
	
	return true;
}

} // UE::StructUtils::Private


//----------------------------------------------------------------//
//  FPropertyBagInstanceDataDetails
//  - StructProperty is FInstancedPropertyBag
//  - ChildPropertyHandle a child property of the FInstancedPropertyBag::Value (FInstancedStruct)  
//----------------------------------------------------------------//

FPropertyBagInstanceDataDetails::FPropertyBagInstanceDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, IPropertyUtilities* InPropUtils, const bool bInFixedLayout)
	: FInstancedStructDataDetails(InStructProperty.IsValid() ? InStructProperty->GetChildHandle(TEXT("Value")) : nullptr)
	, BagStructProperty(InStructProperty)
	, PropUtils(InPropUtils)
	, bFixedLayout(bInFixedLayout)
{
	ensure(UE::StructUtils::Private::IsScriptStruct<FInstancedPropertyBag>(BagStructProperty));
	ensure(PropUtils != nullptr);
}

void FPropertyBagInstanceDataDetails::OnChildRowAdded(IDetailPropertyRow& ChildRow)
{
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;
	ChildRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

	TSharedPtr<IPropertyHandle> ChildPropertyHandle = ChildRow.GetPropertyHandle();
	
	if (!bFixedLayout)
	{
		// Inline editable name
		TSharedPtr<SInlineEditableTextBlock> InlineWidget = SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.MultiLine(false)
			.Text_Lambda([ChildPropertyHandle]()
			{
				return FText::FromName(ChildPropertyHandle->GetProperty()->GetFName());
			})
			.OnVerifyTextChanged_Lambda([this, ChildPropertyHandle](const FText& InText, FText& OutErrorMessage)
			{
				const FName NewName = UE::StructUtils::Private::GetValidPropertyName(InText.ToString());
				bool bResult = UE::StructUtils::Private::IsUniqueName(NewName, ChildPropertyHandle->GetProperty()->GetFName(), BagStructProperty);
				if (!bResult)
				{
					OutErrorMessage = LOCTEXT("MustBeUniqueName", "Property must have unique name");
				}
				return bResult;
			})
			.OnTextCommitted_Lambda([this, ChildPropertyHandle](const FText& InNewText, ETextCommit::Type InCommitType)
			{
				if (InCommitType == ETextCommit::OnCleared)
				{
					return;
				}

				const FName NewName = UE::StructUtils::Private::GetValidPropertyName(InNewText.ToString());
				if (!UE::StructUtils::Private::IsUniqueName(NewName, ChildPropertyHandle->GetProperty()->GetFName(), BagStructProperty))
				{
					return;
				}

				UE::StructUtils::Private::ApplyChangesToPropertyDescs(
					LOCTEXT("OnPropertyNameChanged", "Change Property Name"), BagStructProperty, PropUtils,
					[&NewName, &ChildPropertyHandle](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
					{
						const FProperty* Property = ChildPropertyHandle->GetProperty();
						if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
						{
							Desc->Name = NewName;
						}
					});
			});

		NameWidget = SNew(SComboButton)
			.OnGetMenuContent(this, &FPropertyBagInstanceDataDetails::OnPropertyNameContent, ChildRow.GetPropertyHandle(), InlineWidget)
			.ContentPadding(2)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ButtonContent()
			[
				InlineWidget.ToSharedRef()
			];
	}

	ChildRow
		.CustomWidget(/*bShowChildren*/true)
		.NameContent()
		[
			SNew(SHorizontalBox)
			// Error icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0,0,2,0)
			[
				SNew(SBox)
				.WidthOverride(12)
				.HeightOverride(12)
				[
					SNew(SImage)
					.ToolTipText(LOCTEXT("MissingType", "The property is missing type. The Struct, Enum, or Object may have been removed."))
					.Visibility_Lambda([this, ChildPropertyHandle]() { return UE::StructUtils::Private::HasMissingType(ChildPropertyHandle) ? EVisibility::Visible : EVisibility::Collapsed; })
					.Image(FAppStyle::GetBrush("Icons.Error"))
				]
			]
			// Name
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				NameWidget.ToSharedRef()
			]
		]
		.ValueContent()
		[
			ValueWidget.ToSharedRef()
		];
}

TSharedRef<SWidget> FPropertyBagInstanceDataDetails::OnPropertyNameContent(TSharedPtr<IPropertyHandle> ChildPropertyHandle, TSharedPtr<SInlineEditableTextBlock> InlineWidget) const
{
	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr);

	auto GetFilteredVariableTypeTree = [](TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
	{
		check(GetDefault<UEdGraphSchema_K2>());
		GetDefault<UPropertyBagSchema>()->GetVariableTypeTree(TypeTree, TypeTreeFilter);

		// Filter
		for (TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinType : TypeTree)
		{
			if (!PinType.IsValid())
			{
				return;
			}

			for (int32 ChildIndex = 0; ChildIndex < PinType->Children.Num(); )
			{
				TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> Child = PinType->Children[ChildIndex];
				if (Child.IsValid())
				{
					if (!UE::StructUtils::Private::CanHaveMemberVariableOfType(Child->GetPinType(/*bForceLoadSubCategoryObject*/false)))
					{
						PinType->Children.RemoveAt(ChildIndex);
						continue;
					}
				}
				++ChildIndex;
			}
		}			
	};

	auto GetPinInfo = [this, ChildPropertyHandle]()
	{
		TArray<FPropertyBagPropertyDesc> PropertyDescs = UE::StructUtils::Private::GetCommonPropertyDescs(BagStructProperty);

		const FProperty* Property = ChildPropertyHandle->GetProperty();
		if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
		{
			return UE::StructUtils::Private::GetPropertyDescAsPin(*Desc);
		}
	
		return FEdGraphPinType();
	};

	auto PinInfoChanged = [this, ChildPropertyHandle](const FEdGraphPinType& PinType)
	{
		UE::StructUtils::Private::ApplyChangesToPropertyDescs(
			LOCTEXT("OnPropertyTypeChanged", "Change Property Type"), BagStructProperty, PropUtils,
			[&PinType, &ChildPropertyHandle](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
			{
				// Find and change struct type
				const FProperty* Property = ChildPropertyHandle->GetProperty();
				if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
				{
					UE::StructUtils::Private::SetPropertyDescFromPin(*Desc, PinType);
				}
			});
	};

	auto RemoveProperty = [this, ChildPropertyHandle]()
	{
		UE::StructUtils::Private::ApplyChangesToPropertyDescs(
			LOCTEXT("OnPropertyRemoved", "Remove Property"), BagStructProperty, PropUtils,
			[&ChildPropertyHandle](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
			{
				const FProperty* Property = ChildPropertyHandle->GetProperty();
				PropertyDescs.RemoveAll([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; });
			});
	};

	auto MoveProperty = [this, ChildPropertyHandle](const int32 Delta)
	{
		UE::StructUtils::Private::ApplyChangesToPropertyDescs(
		LOCTEXT("OnPropertyMoved", "Move Property"), BagStructProperty, PropUtils,
		[&ChildPropertyHandle, &Delta](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
		{
			// Move
			if (PropertyDescs.Num() > 1)
			{
				const FProperty* Property = ChildPropertyHandle->GetProperty();
				const int32 PropertyIndex = PropertyDescs.IndexOfByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; });
				if (PropertyIndex != INDEX_NONE)
				{
					const int32 NewPropertyIndex = FMath::Clamp(PropertyIndex + Delta, 0, PropertyDescs.Num() - 1);
					PropertyDescs.Swap(PropertyIndex, NewPropertyIndex);
				}
			}
		});
	};


	MenuBuilder.AddWidget(
		SNew(SBox)
		.HAlign(HAlign_Right)
		.Padding(FMargin(12, 0, 12, 0))
		[
			SNew(SPinTypeSelector, FGetPinTypeTree::CreateLambda(GetFilteredVariableTypeTree))
				.TargetPinType_Lambda(GetPinInfo)
				.OnPinTypeChanged_Lambda(PinInfoChanged)
				.Schema(GetDefault<UPropertyBagSchema>())
				.bAllowArrays(true)
				.TypeTreeFilter(ETypeTreeFilter::None)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
		],
		FText::GetEmpty());
	
	MenuBuilder.AddSeparator();
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Rename", "Rename"),
		LOCTEXT("Rename_ToolTip", "Rename property"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateLambda([InlineWidget]()  { InlineWidget->EnterEditingMode(); }))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Remove", "Remove"),
		LOCTEXT("Remove_ToolTip", "Remove property"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateLambda(RemoveProperty))
	);
	
	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MoveUp", "Move Up"),
		LOCTEXT("MoveUp_ToolTip", "Move property up in the list"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowUp"),
		FUIAction(FExecuteAction::CreateLambda(MoveProperty, -1))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MoveDown", "Move Down"),
		LOCTEXT("MoveDown_ToolTip", "Move property down in the list"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowDown"),
		FUIAction(FExecuteAction::CreateLambda(MoveProperty, +1))
	);

	return MenuBuilder.MakeWidget();
}


////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FPropertyBagDetails::MakeInstance()
{
	return MakeShared<FPropertyBagDetails>();
}

void FPropertyBagDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();
	
	StructProperty = StructPropertyHandle;
	check(StructProperty);
	
	static const FName NAME_FixedLayout = "FixedLayout";
	if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
	{
		bFixedLayout = MetaDataProperty->HasMetaData(NAME_FixedLayout);
	}

	TSharedPtr<SWidget> ValueWidget = SNullWidget::NullWidget;
	if (!bFixedLayout)
	{
		ValueWidget = MakeAddPropertyWidget(StructProperty, PropUtils);
	}
	
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			ValueWidget.ToSharedRef()
		]
		.ShouldAutoExpand(true);
}

void FPropertyBagDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Show the Value (FInstancedStruct) as child rows.
	TSharedRef<FPropertyBagInstanceDataDetails> InstanceDetails = MakeShareable(new FPropertyBagInstanceDataDetails(StructProperty, PropUtils, bFixedLayout));
	StructBuilder.AddCustomBuilder(InstanceDetails);
}

TSharedPtr<SWidget> FPropertyBagDetails::MakeAddPropertyWidget(TSharedPtr<IPropertyHandle> InStructProperty, class IPropertyUtilities* InPropUtils)
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("AddProperty_Tooltip", "Add new property"))
			.OnClicked_Lambda([InStructProperty, InPropUtils]()
			{
				constexpr int32 MaxIterations = 100;
				FName NewName(TEXT("NewProperty"));
				int32 Number = 1;
				while (!UE::StructUtils::Private::IsUniqueName(NewName, FName(), InStructProperty) && Number < MaxIterations)
				{
					Number++;
					NewName.SetNumber(Number);
				}
				if (Number == MaxIterations)
				{
					return FReply::Handled();
				}

				UE::StructUtils::Private::ApplyChangesToPropertyDescs(
					LOCTEXT("OnPropertyAdded", "Add Property"), InStructProperty, InPropUtils,
					[&NewName](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
					{
						PropertyDescs.Emplace(NewName, EPropertyBagPropertyType::Bool);
					});
					
				return FReply::Handled();

			})
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

}

////////////////////////////////////

bool UPropertyBagSchema::SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction,
		const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const
{
	return ContainerType == EPinContainerType::None || ContainerType == EPinContainerType::Array;
}


#undef LOCTEXT_NAMESPACE

