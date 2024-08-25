// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectBindingExtension.h"
#include "EdGraphSchema_K2.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"
#include "InstancedStruct.h"
#include "PropertyBindingPath.h"
#include "Styling/AppStyle.h"
#include "UObject/EnumProperty.h"
#include "Widgets/Layout/SBox.h"
#include "SmartObjectDefinition.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

namespace UE::SmartObject::PropertyBinding
{

const FName DataIDName(TEXT("DataID"));

USmartObjectDefinition* GetOuterSmartObjectDefinition(const TSharedPtr<const IPropertyHandle>& InPropertyHandle)
{
	USmartObjectDefinition* Definition = nullptr;
	
	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);
	for (UObject* OuterObject : OuterObjects)
	{
		if (OuterObject)
		{
			if (USmartObjectDefinition* CurrentDefinition = Cast<USmartObjectDefinition>(OuterObject))
			{
				Definition = CurrentDefinition;
				break;
			}
			else if (USmartObjectDefinition* OuterDefinition = OuterObject->GetTypedOuter<USmartObjectDefinition>())
			{
				Definition = OuterDefinition;
				break;
			}
		}
	}

	return Definition;
}

UStruct* ResolveLeafValueStructType(FPropertyBindingDataView ValueView, const TArray<FBindingChainElement>& InBindingChain)
{
	if (ValueView.GetMemory() == nullptr)
	{
		return nullptr;
	}
	
	FPropertyBindingPath Path;

	for (const FBindingChainElement& Element : InBindingChain)
	{
		if (const FProperty* Property = Element.Field.Get<FProperty>())
		{
			Path.AddPathSegment(Property->GetFName(), Element.ArrayIndex);
		}
		else if (const UFunction* Function = Element.Field.Get<UFunction>())
		{
			// Cannot handle function calls
			return nullptr;
		}
	}

	TArray<FPropertyBindingPathIndirection> Indirections;
	if (!Path.ResolveIndirectionsWithValue(ValueView, Indirections)
		|| Indirections.IsEmpty())
	{
		return nullptr;
	}

	// Last indirection points to the value of the leaf property, check the type.
	const FPropertyBindingPathIndirection& LastIndirection = Indirections.Last();

	UStruct* Result = nullptr;

	if (LastIndirection.GetContainerAddress())
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(LastIndirection.GetProperty()))
		{
			// Get the type of the instanced struct's value.
			if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
			{
				const FInstancedStruct& InstancedStruct = *reinterpret_cast<const FInstancedStruct*>(LastIndirection.GetPropertyAddress());
				Result = const_cast<UScriptStruct*>(InstancedStruct.GetScriptStruct());
			}
		}
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(LastIndirection.GetProperty()))
		{
			// Get type of the instanced object.
			if (const UObject* Object = *reinterpret_cast<UObject* const*>(LastIndirection.GetPropertyAddress()))
			{
				Result = Object->GetClass();
			}
		}
	}

	return Result;
}

void MakeStructPropertyPathFromBindingChain(const FGuid StructID, const TArray<FBindingChainElement>& InBindingChain, FPropertyBindingDataView DataView, FPropertyBindingPath& OutPath)
{
	OutPath.Reset();
	OutPath.SetStructID(StructID);
	
	for (const FBindingChainElement& Element : InBindingChain)
	{
		if (const FProperty* Property = Element.Field.Get<FProperty>())
		{
			OutPath.AddPathSegment(Property->GetFName(), Element.ArrayIndex);
		}
		else if (const UFunction* Function = Element.Field.Get<UFunction>())
		{
			OutPath.AddPathSegment(Function->GetFName());
		}
	}

	OutPath.UpdateSegmentsFromValue(DataView);
}

void MakeStructPropertyPathFromPropertyHandle(TSharedPtr<const IPropertyHandle> InPropertyHandle, FPropertyBindingPath& OutPath)
{
	OutPath.Reset();

	FGuid StructID;
	TArray<FPropertyBindingPathSegment> PathSegments;

	TSharedPtr<const IPropertyHandle> CurrentPropertyHandle = InPropertyHandle;
	while (CurrentPropertyHandle.IsValid())
	{
		const FProperty* Property = CurrentPropertyHandle->GetProperty();
		if (Property)
		{
			FPropertyBindingPathSegment& Segment = PathSegments.InsertDefaulted_GetRef(0); // Traversing from leaf to root, insert in reverse.

			// Store path up to the property which has ID.
			Segment.SetName(Property->GetFName());
			Segment.SetArrayIndex(CurrentPropertyHandle->GetIndexInArray());

			// Store type of the object (e.g. for instanced objects or instanced structs).
			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				if (ObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference))
				{
					const UObject* Object = nullptr;
					if (CurrentPropertyHandle->GetValue(Object) == FPropertyAccess::Success)
					{
						if (Object)
						{
							Segment.SetInstanceStruct(Object->GetClass());
						}
					}
				}
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
				{
					void* Address = nullptr;
					if (CurrentPropertyHandle->GetValueData(Address) == FPropertyAccess::Success)
					{
						if (Address)
						{
							FInstancedStruct& Struct = *static_cast<FInstancedStruct*>(Address);
							Segment.SetInstanceStruct(Struct.GetScriptStruct());
						}
					}
				}
			}

			// Array access is represented as: "Array, PropertyInArray[Index]", we're traversing from leaf to root, skip the node without index.
			// Advancing the node before ID test, since the array is on the instance data, the ID will be on the Array node.
			if (Segment.GetArrayIndex() != INDEX_NONE)
			{
				TSharedPtr<const IPropertyHandle> ParentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
				if (ParentPropertyHandle.IsValid())
				{
					const FProperty* ParentProperty = ParentPropertyHandle->GetProperty();
					if (ParentProperty
						&& ParentProperty->IsA<FArrayProperty>()
						&& Property->GetFName() == ParentProperty->GetFName())
					{
						CurrentPropertyHandle = ParentPropertyHandle;
					}
				}
			}

			// Bindable property must have node ID
			if (const FString* IDString = CurrentPropertyHandle->GetInstanceMetaData(UE::SmartObject::PropertyBinding::DataIDName))
			{
				LexFromString(StructID, **IDString);
				break;
			}
		}
		
		CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
	}

	if (!StructID.IsValid())
	{
		if (USmartObjectDefinition* Definition = GetOuterSmartObjectDefinition(InPropertyHandle))
		{
			StructID = Definition->GetDataRootID();
		}
	}

	if (StructID.IsValid())
	{
		OutPath = FPropertyBindingPath(StructID, PathSegments);
	}
}
	
// @todo: there's a similar function in StateTreeNodeDetails.cpp, merge.
FText GetPropertyTypeText(const FProperty* Property)
{
	FEdGraphPinType PinType;
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Schema->ConvertPropertyToPinType(Property, PinType);
				
	const FName PinSubCategory = PinType.PinSubCategory;
	const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
	if (PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask && PinSubCategoryObject)
	{
		if (const UField* Field = Cast<const UField>(PinSubCategoryObject))
		{
			return Field->GetDisplayNameText();
		}
		return FText::FromString(PinSubCategoryObject->GetName());
	}

	return UEdGraphSchema_K2::GetCategoryText(PinType.PinCategory, NAME_None, true);
}

struct FCachedBindingData : public TSharedFromThis<FCachedBindingData>
{
	FCachedBindingData(USmartObjectDefinition* InDefinition, const FPropertyBindingPath& InTargetPath, const TSharedPtr<const IPropertyHandle>& InPropertyHandle, TArrayView<FBindableStructDesc> InAccessibleStructs)
		: WeakDefinition(InDefinition)
		, TargetPath(InTargetPath)
		, PropertyHandle(InPropertyHandle)
		, AccessibleStructs(InAccessibleStructs)
	{
	}

	void AddBinding(const TArray<FBindingChainElement>& InBindingChain)
	{
		if (InBindingChain.IsEmpty())
		{
			return;
		}
		
		if (!TargetPath.GetStructID().IsValid())
		{
			return;
		}

		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("SmartObject_AddBingin", "Add Binding"));

		// First item in the binding chain is the index in AccessibleStructs.
		const int32 SourceStructIndex = InBindingChain[0].ArrayIndex;
		check(SourceStructIndex >= 0 && SourceStructIndex < AccessibleStructs.Num());
				
		TArray<FBindingChainElement> SourceBindingChain = InBindingChain;
		SourceBindingChain.RemoveAt(0); // remove struct index.

		FPropertyBindingDataView DataView;
		if (Definition->GetDataViewByID(AccessibleStructs[SourceStructIndex].ID, DataView))
		{
			// If SourceBindingChain is empty at this stage, it means that the binding points to the source struct itself.
			FPropertyBindingPath SourcePath;
			UE::SmartObject::PropertyBinding::MakeStructPropertyPathFromBindingChain(AccessibleStructs[SourceStructIndex].ID, SourceBindingChain, DataView, SourcePath);
					
			Definition->Modify();
			Definition->AddPropertyBinding(SourcePath, TargetPath);

			UpdateData();
		}
	}

	bool CanRemoveBinding() const
	{
		return HasBinding();
	}

	bool HasBinding() const
	{
		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return false;
		}

		const FPropertyBindingPath* SourcePath = Definition->GetPropertyBindingSource(TargetPath);

		return SourcePath != nullptr;
	}

	void RemoveBinding()
	{
		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("SmartObject_RemoveBInding", "Remove Binding"));

		Definition->Modify();
		Definition->RemovePropertyBindings(TargetPath);

		UpdateData();
	}


	void UpdateData()
	{
		static FName PropertyIcon(TEXT("Kismet.Tabs.Variables"));

		Text = FText::GetEmpty();
		TooltipText = FText::GetEmpty();
		Color = FLinearColor::White;
		Image = nullptr;

		if (!PropertyHandle.IsValid())
		{
			return;
		}

		const FProperty* Property = PropertyHandle->GetProperty();
		if (!Property)
		{
			return;
		}

		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		check(Schema);

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(Property, PinType);

		if (const FPropertyBindingPath* SourcePath = Definition->GetPropertyBindingSource(TargetPath))
		{
			const FBindableStructDesc* SourceDesc = AccessibleStructs.FindByPredicate([StructID = SourcePath->GetStructID()](const FBindableStructDesc& Desc) { return Desc.ID == StructID; });

			if (SourceDesc)
			{
				bool bIsValidBinding = false;

				// Check that the binding is valid.
				FPropertyBindingDataView SourceDataView;
				FPropertyBindingDataView TargetDataView;
				const FProperty* SourceLeafProperty = nullptr;
				const UStruct* SourceStruct = nullptr;
				if (Definition->GetDataViewByID(SourcePath->GetStructID(), SourceDataView)
					&& Definition->GetDataViewByID(TargetPath.GetStructID(), TargetDataView))
				{
					TArray<FPropertyBindingPathIndirection> SourceIndirections;
					TArray<FPropertyBindingPathIndirection> TargetIndirections;

					// Resolve source and target properties.
					// Source path can be empty, when the binding binds directly to a context struct/class.
					// Target path must always point to a valid property (at least one indirection).
					if (SourcePath->ResolveIndirectionsWithValue(SourceDataView, SourceIndirections)
						&& TargetPath.ResolveIndirectionsWithValue(TargetDataView, TargetIndirections)
						&& !TargetIndirections.IsEmpty())
					{
						const FPropertyBindingPathIndirection LastTargetIndirection = TargetIndirections.Last();
						if (SourceIndirections.Num() > 0)
						{
							// Binding to a source property.
							SourceLeafProperty = SourceIndirections.Last().GetProperty();
							bIsValidBinding = USmartObjectDefinition::ArePropertiesCompatible(SourceLeafProperty, LastTargetIndirection.GetProperty());
						}
						else
						{
							// Binding to a source context struct.
							SourceStruct = SourceDataView.GetStruct();
							bIsValidBinding = ArePropertyAndContextStructCompatible(SourceStruct, LastTargetIndirection.GetProperty());
						}
					}
				}

				FString SourcePropertyName;
				SourcePropertyName += SourceDesc->Name.ToString();
				if (!SourcePath->IsPathEmpty())
				{
					SourcePropertyName += TEXT(" ") + SourcePath->ToString();
				}

				if (bIsValidBinding)
				{
					Text = FText::FromString(SourcePropertyName);

					if (SourcePath->IsPathEmpty())
					{
						TooltipText = FText::Format(LOCTEXT("ExistingBindingTooltip", "Property is bound to {0}."), FText::FromString(SourceDesc->ToString()));
					}
					else
					{
						TooltipText = FText::Format(LOCTEXT("ExistingBindingWithPropertyTooltip", "Property is bound to {0} property {1}."), FText::FromString(SourceDesc->ToString()), FText::FromString(SourcePath->ToString()));
					}
					
					Image = FAppStyle::GetBrush(PropertyIcon);
					Color = Schema->GetPinTypeColor(PinType);
				}
				else
				{
					FText SourceType;
					if (SourceLeafProperty)
					{
						SourceType = UE::SmartObject::PropertyBinding::GetPropertyTypeText(SourceLeafProperty);
					}
					else if (SourceStruct)
					{
						SourceType = SourceStruct->GetDisplayNameText();
					}
					FText TargetType = UE::SmartObject::PropertyBinding::GetPropertyTypeText(Property);
					
					Text = FText::FromString(SourcePropertyName);

					if (SourcePath->IsPathEmpty())
					{
						TooltipText = FText::Format(LOCTEXT("MismatchingBindingTooltip", "Property is bound to {0}, but binding source type '{1}' does not match property type '{2}'."),
							FText::FromString(SourceDesc->ToString()), SourceType, TargetType);
					}
					else
					{
						TooltipText = FText::Format(LOCTEXT("MismatchingBindingTooltipWithProperty", "Property is bound to {0} property {1}, but binding source type '{2}' does not match property type '{3}'."),
							FText::FromString(SourceDesc->ToString()), FText::FromString(SourcePath->ToString()), SourceType, TargetType);
					}

					Image = FCoreStyle::Get().GetBrush("Icons.ErrorWithColor");
					Color = FLinearColor::White;
				}
			}
			else
			{
				// Missing source
				Text = FText::Format(LOCTEXT("MissingSource", "???.{0}"), FText::FromString(SourcePath->ToString()));
				TooltipText = FText::Format(LOCTEXT("MissingBindingTooltip", "Missing binding source for property path '{0}'."), FText::FromString(SourcePath->ToString()));
				Image = FCoreStyle::Get().GetBrush("Icons.ErrorWithColor");
				Color = FLinearColor::White;
			}

			CachedSourcePath = *SourcePath;
		}
		else
		{
			// No bindings
			Text = FText::GetEmpty();
			TooltipText = FText::Format(LOCTEXT("BindTooltip", "Bind {0} to value from another property."), UE::SmartObject::PropertyBinding::GetPropertyTypeText(Property));
			Image = FAppStyle::GetBrush(PropertyIcon);
			Color = Schema->GetPinTypeColor(PinType);

			CachedSourcePath.Reset();
		}

		bIsDataCached = true;
	}

	bool CanBindToContextStruct(const UStruct* InStruct)
	{
		ConditionallyUpdateData();

		return ArePropertyAndContextStructCompatible(InStruct, PropertyHandle->GetProperty());
	}
			
	bool CanBindToProperty(const FProperty* SourceProperty)
	{
		ConditionallyUpdateData();

		// Special case for binding widget calling OnCanBindProperty with Args.Property (i.e. self).
		if (PropertyHandle->GetProperty() == SourceProperty)
		{
			return true;
		}

		void* TargetValueAddress = nullptr;
		if (PropertyHandle->GetValueData(TargetValueAddress) == FPropertyAccess::Success)
		{
			return USmartObjectDefinition::ArePropertiesCompatible(SourceProperty, PropertyHandle->GetProperty());
		}
		
		return false;
	}

	bool CanAcceptPropertyOrChildren(const FProperty* SourceProperty, TConstArrayView<FBindingChainElement> InBindingChain)
	{
		ConditionallyUpdateData();

		return SourceProperty->HasAnyPropertyFlags(CPF_Edit);
	}

	void PromoteToParameter()
	{
		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("SmartObject_PromoteToParameter", "Promote to Parameter"));

		Definition->Modify();
		Definition->AddParameterAndBindingFromPropertyPath(TargetPath);

		UpdateData();
	}
	
	bool CanCreateParameter() const
	{
		return !HasBinding();
	}

	static bool ArePropertyAndContextStructCompatible(const UStruct* SourceStruct, const FProperty* TargetProperty)
	{
		if (const FStructProperty* TargetStructProperty = CastField<FStructProperty>(TargetProperty))
		{
			return TargetStructProperty->Struct == SourceStruct;
		}
		if (const FObjectProperty* TargetObjectProperty = CastField<FObjectProperty>(TargetProperty))
		{
			return SourceStruct != nullptr && SourceStruct->IsChildOf(TargetObjectProperty->PropertyClass);
		}
		
		return false;
	}

	UStruct* ResolveIndirection(TArray<FBindingChainElement> InBindingChain)
	{
		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return nullptr;
		}

		const int32 SourceStructIndex = InBindingChain[0].ArrayIndex;
		check(SourceStructIndex >= 0 && SourceStructIndex < AccessibleStructs.Num());
		
		TArray<FBindingChainElement> SourceBindingChain = InBindingChain;
		SourceBindingChain.RemoveAt(0);

		FPropertyBindingDataView DataView;
		if (Definition->GetDataViewByID(AccessibleStructs[SourceStructIndex].ID, DataView))
		{
			return UE::SmartObject::PropertyBinding::ResolveLeafValueStructType(DataView, InBindingChain);
		}

		return nullptr;
	}

	FText GetText()
	{
		ConditionallyUpdateData();
		return Text;
	}
	
	FText GetTooltipText()
	{
		ConditionallyUpdateData();
		return TooltipText;
	}
	
	FLinearColor GetColor()
	{
		ConditionallyUpdateData();
		return Color;
	}
	
	const FSlateBrush* GetImage()
	{
		ConditionallyUpdateData();
		return Image;
	}
	
private:

	void ConditionallyUpdateData()
	{
		USmartObjectDefinition* Definition = WeakDefinition.Get();
		if (!Definition)
		{
			return;
		}

		const FPropertyBindingPath* CurrentSourcePath = Definition->GetPropertyBindingSource(TargetPath);
		bool bPathsIdentical = false;
		if (CurrentSourcePath)
		{
			bPathsIdentical = CachedSourcePath == *CurrentSourcePath;
		}
		else
		{
			bPathsIdentical = CachedSourcePath.IsPathEmpty();
		}

		if (!bIsDataCached || !bPathsIdentical)
		{
			UpdateData();
		}
	}
	
	TWeakObjectPtr<USmartObjectDefinition> WeakDefinition = nullptr;
	FPropertyBindingPath CachedSourcePath;
	FPropertyBindingPath TargetPath;
	TSharedPtr<const IPropertyHandle> PropertyHandle;
	
	TArray<FBindableStructDesc> AccessibleStructs;

	FText Text;
	FText TooltipText;
	FLinearColor Color = FLinearColor::White;
	const FSlateBrush* Image = nullptr;
	
	bool bIsDataCached = false;
};

} // UE::SmartObject::PropertyBinding

bool FSmartObjectDefinitionBindingExtension::IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const
{
	const FProperty* Property = PropertyHandle.GetProperty();
	if (Property->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_EditorOnly | CPF_Config))
	{
		return false;
	}

	bool bCanBind = true;

	static const FName NoBindingName(TEXT("NoBinding"));
	TSharedPtr<const IPropertyHandle> CurrentPropertyHandle = PropertyHandle.AsShared();
	while (CurrentPropertyHandle.IsValid())
	{
		if (CurrentPropertyHandle->HasMetaData(NoBindingName))
		{
			bCanBind = false;
			break;
		}
		CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
	}

	FPropertyBindingPath TargetPath;
	UE::SmartObject::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(PropertyHandle.AsShared(), TargetPath);
	if (!TargetPath.GetStructID().IsValid())
	{
		bCanBind = false;
	}
	
	return bCanBind;
}


void FSmartObjectDefinitionBindingExtension::ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	if (!IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
	{
		return;
	}
	
	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	// Array of structs we can bind to.
	TArray<FBindingContextStruct> BindingContextStructs;
	TArray<FBindableStructDesc> AccessibleStructs;

	// The struct and property where we're binding.
	FPropertyBindingPath TargetPath;

	// Find the definition we're editing.
	USmartObjectDefinition* Definition = UE::SmartObject::PropertyBinding::GetOuterSmartObjectDefinition(InPropertyHandle);

	if (Definition)
	{
		// Figure out the structs we're editing, and property path relative to current property.
		UE::SmartObject::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(InPropertyHandle, TargetPath);

		Definition->GetAccessibleStructs(TargetPath.GetStructID(), AccessibleStructs);

		for (FBindableStructDesc& StructDesc : AccessibleStructs)
		{
			const UStruct* Struct = StructDesc.Struct;

			FBindingContextStruct& ContextStruct = BindingContextStructs.AddDefaulted_GetRef();
			ContextStruct.DisplayText = FText::FromString(StructDesc.Name.ToString());
			ContextStruct.Struct = const_cast<UStruct*>(Struct);
		}
	}

	TSharedPtr<UE::SmartObject::PropertyBinding::FCachedBindingData> CachedBindingData = MakeShared<UE::SmartObject::PropertyBinding::FCachedBindingData>(Definition, TargetPath, InPropertyHandle, AccessibleStructs);

	// Wrap value widget 
	auto IsValueVisible = TAttribute<EVisibility>::Create([TargetPath, CachedBindingData]() -> EVisibility
	{
		return CachedBindingData->HasBinding() ? EVisibility::Collapsed : EVisibility::Visible;
	});

	TSharedPtr<SWidget> ValueWidget = InWidgetRow.ValueContent().Widget;
	InWidgetRow.ValueContent()
	[
		SNew(SBox)
		.Visibility(IsValueVisible)
		[
			ValueWidget.ToSharedRef()
		]
	];

	FPropertyBindingWidgetArgs Args;
	Args.Property = InPropertyHandle->GetProperty();

	Args.OnCanBindProperty = FOnCanBindProperty::CreateLambda([CachedBindingData](FProperty* InProperty)
		{
			return CachedBindingData->CanBindToProperty(InProperty);
		});

	Args.OnCanBindToContextStruct = FOnCanBindToContextStruct::CreateLambda([CachedBindingData](const UStruct* InStruct)
		{
			return CachedBindingData->CanBindToContextStruct(InStruct);
		});

	Args.OnCanAcceptPropertyOrChildrenWithBindingChain = FOnCanAcceptPropertyOrChildrenWithBindingChain::CreateLambda([CachedBindingData](FProperty* InProperty, TConstArrayView<FBindingChainElement> InBindingChain)
		{
			return CachedBindingData->CanAcceptPropertyOrChildren(InProperty, InBindingChain);
		});

	Args.OnCanBindToClass = FOnCanBindToClass::CreateLambda([](UClass* InClass)
		{
			return true;
		});

	Args.OnAddBinding = FOnAddBinding::CreateLambda([CachedBindingData](FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
		{
			CachedBindingData->AddBinding(InBindingChain);
		});

	Args.OnRemoveBinding = FOnRemoveBinding::CreateLambda([CachedBindingData](FName InPropertyName)
		{
			CachedBindingData->RemoveBinding();
		});

	Args.OnCanRemoveBinding = FOnCanRemoveBinding::CreateLambda([CachedBindingData](FName InPropertyName)
		{
			return CachedBindingData->CanRemoveBinding();
		});

	Args.CurrentBindingText = MakeAttributeLambda([CachedBindingData]()
		{
			return CachedBindingData->GetText();
		});

	Args.CurrentBindingToolTipText = MakeAttributeLambda([CachedBindingData]()
		{
			return CachedBindingData->GetTooltipText();
		});

	Args.CurrentBindingImage = MakeAttributeLambda([CachedBindingData]() -> const FSlateBrush*
		{
			return CachedBindingData->GetImage();
		});

	Args.CurrentBindingColor = MakeAttributeLambda([CachedBindingData]() -> FLinearColor
		{
			return CachedBindingData->GetColor();
		});

	if (Definition)
	{
		Args.OnResolveIndirection = FOnResolveIndirection::CreateLambda([CachedBindingData](TArray<FBindingChainElement> InBindingChain)
		{
			return CachedBindingData->ResolveIndirection(InBindingChain);
		});
	}

	Args.BindButtonStyle = &FAppStyle::Get().GetWidgetStyle<FButtonStyle>("HoverHintOnly");
	Args.bAllowNewBindings = false;
	Args.bAllowArrayElementBindings = false;
	Args.bAllowUObjectFunctions = false;

	Args.MenuExtender = MakeShareable(new FExtender);
	Args.MenuExtender->AddMenuExtension(
		"BindingActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([CachedBindingData](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("PromoteToParameter", "Promote to Parameter"),
				LOCTEXT("PromoteToParameterTooltip", "Create a new parameter of the same type as the property, copy value over, and bind the property to the new parameter."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
				FUIAction(
					FExecuteAction::CreateLambda([CachedBindingData]()
					{
						if (CachedBindingData)
						{
							CachedBindingData->PromoteToParameter();
						}
					}),
					FCanExecuteAction::CreateLambda([CachedBindingData]()
					{
						return CachedBindingData ? CachedBindingData->CanCreateParameter() : false;
					})
				)
			);
		})
	);

	
	InWidgetRow.ExtensionContent()
	[
		PropertyAccessEditor.MakePropertyBindingWidget(BindingContextStructs, Args)
	];
}

#undef LOCTEXT_NAMESPACE
