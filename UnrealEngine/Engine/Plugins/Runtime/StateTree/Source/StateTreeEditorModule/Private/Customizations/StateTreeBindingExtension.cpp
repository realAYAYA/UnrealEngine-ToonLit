// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeBindingExtension.h"
#include "EdGraphSchema_K2.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"
#include "StateTreeAnyEnum.h"
#include "StateTreeCompiler.h"
#include "StateTreeEditorPropertyBindings.h"
#include "StateTreeNodeBase.h"
#include "Styling/AppStyle.h"
#include "UObject/EnumProperty.h"
#include "Widgets/Layout/SBox.h"
#include "StateTreePropertyRef.h"
#include "StateTreePropertyRefHelpers.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::PropertyBinding
{
	
const FName StateTreeNodeIDName(TEXT("StateTreeNodeID"));
const FName AllowAnyBindingName(TEXT("AllowAnyBinding"));

UObject* FindEditorBindingsOwner(UObject* InObject)
{
	UObject* Result = nullptr;

	for (UObject* Outer = InObject; Outer; Outer = Outer->GetOuter())
	{
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(Outer);
		if (BindingOwner)
		{
			Result = Outer;
			break;
		}
	}
	return Result;
}

UStruct* ResolveLeafValueStructType(FStateTreeDataView ValueView, const TArray<FBindingChainElement>& InBindingChain)
{
	if (ValueView.GetMemory() == nullptr)
	{
		return nullptr;
	}
	
	FStateTreePropertyPath Path;

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

	TArray<FStateTreePropertyPathIndirection> Indirections;
	if (!Path.ResolveIndirectionsWithValue(ValueView, Indirections)
		|| Indirections.IsEmpty())
	{
		return nullptr;
	}

	// Last indirection points to the value of the leaf property, check the type.
	const FStateTreePropertyPathIndirection& LastIndirection = Indirections.Last();

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

void MakeStructPropertyPathFromBindingChain(const FGuid StructID, const TArray<FBindingChainElement>& InBindingChain, FStateTreeDataView DataView, FStateTreePropertyPath& OutPath)
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

EStateTreePropertyUsage MakeStructPropertyPathFromPropertyHandle(TSharedPtr<const IPropertyHandle> InPropertyHandle, FStateTreePropertyPath& OutPath)
{
	OutPath.Reset();

	FGuid StructID;
	TArray<FStateTreePropertyPathSegment> PathSegments;
	EStateTreePropertyUsage ResultUsage = EStateTreePropertyUsage::Invalid; 

	TSharedPtr<const IPropertyHandle> CurrentPropertyHandle = InPropertyHandle;
	while (CurrentPropertyHandle.IsValid())
	{
		const FProperty* Property = CurrentPropertyHandle->GetProperty();
		if (Property)
		{
			FStateTreePropertyPathSegment& Segment = PathSegments.InsertDefaulted_GetRef(0); // Traversing from leaf to root, insert in reverse.

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
			if (const FString* IDString = CurrentPropertyHandle->GetInstanceMetaData(UE::StateTree::PropertyBinding::StateTreeNodeIDName))
			{
				LexFromString(StructID, **IDString);
				ResultUsage = UE::StateTree::GetUsageFromMetaData(Property);
				break;
			}
		}
		
		CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
	}

	if (!StructID.IsValid())
	{
		ResultUsage = EStateTreePropertyUsage::Invalid;
	}
	else
	{
		OutPath = FStateTreePropertyPath(StructID, PathSegments);
	}

	return ResultUsage;
}

const FStateTreeBindableStructDesc* FindStruct(TConstArrayView<FStateTreeBindableStructDesc> AccessibleStructs, const FGuid StructID)
{
	return AccessibleStructs.FindByPredicate([StructID](const FStateTreeBindableStructDesc& Desc) { return Desc.ID == StructID; });
}


FText GetSectionNameFromDataSource(const EStateTreeBindableStructSource Source)
{
	switch (Source)
	{
	case EStateTreeBindableStructSource::Context:
		return LOCTEXT("Context", "Context");
	case EStateTreeBindableStructSource::Parameter:
		return LOCTEXT("Parameters", "Parameters");
	case EStateTreeBindableStructSource::Evaluator:
		return LOCTEXT("Evaluators", "Evaluators");
	case EStateTreeBindableStructSource::GlobalTask:
		return LOCTEXT("StateGlobalTasks", "Global Tasks");
	case EStateTreeBindableStructSource::State:
		return LOCTEXT("StateParameters", "State");
	case EStateTreeBindableStructSource::Task:
		return LOCTEXT("Tasks", "Tasks");
	default:
		return FText::GetEmpty();
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

FOnStateTreePropertyBindingChanged STATETREEEDITORMODULE_API OnStateTreePropertyBindingChanged;

struct FCachedBindingData : public TSharedFromThis<FCachedBindingData>
{
	FCachedBindingData(UObject* InOwnerObject, const FStateTreePropertyPath& InTargetPath, const TSharedPtr<const IPropertyHandle>& InPropertyHandle, TArrayView<FStateTreeBindableStructDesc> InAccessibleStructs)
		: WeakOwnerObject(InOwnerObject)
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

		UObject* OwnerObject = WeakOwnerObject.Get();
		if (!OwnerObject)
		{
			return;
		}
		
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(OwnerObject);
		if (!BindingOwner)
		{
			return;
		}

		FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
		if (!EditorBindings)
		{
			return;
		}

		// First item in the binding chain is the index in AccessibleStructs.
		const int32 SourceStructIndex = InBindingChain[0].ArrayIndex;
		check(SourceStructIndex >= 0 && SourceStructIndex < AccessibleStructs.Num());
				
		TArray<FBindingChainElement> SourceBindingChain = InBindingChain;
		SourceBindingChain.RemoveAt(0); // remove struct index.

		FStateTreeDataView DataView;
		BindingOwner->GetDataViewByID(AccessibleStructs[SourceStructIndex].ID, DataView);

		// If SourceBindingChain is empty at this stage, it means that the binding points to the source struct itself.
		FStateTreePropertyPath SourcePath;
		UE::StateTree::PropertyBinding::MakeStructPropertyPathFromBindingChain(AccessibleStructs[SourceStructIndex].ID, SourceBindingChain, DataView, SourcePath);
				
		OwnerObject->Modify();
		EditorBindings->AddPropertyBinding(SourcePath, TargetPath);

		UpdateData();

		UE::StateTree::PropertyBinding::OnStateTreePropertyBindingChanged.Broadcast(SourcePath, TargetPath);
	}

	void RemoveBinding()
	{
		UObject* OwnerObject = WeakOwnerObject.Get();
		if (!OwnerObject)
		{
			return;
		}
		
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(OwnerObject);
		if (!BindingOwner)
		{
			return;
		}

		FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
		if (!EditorBindings)
		{
			return;
		}

		OwnerObject->Modify();
		EditorBindings->RemovePropertyBindings(TargetPath);

		UpdateData();
		
		const FStateTreePropertyPath SourcePath; // Null path
		UE::StateTree::PropertyBinding::OnStateTreePropertyBindingChanged.Broadcast(SourcePath, TargetPath);
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

		UObject* OwnerObject = WeakOwnerObject.Get();
		if (!OwnerObject)
		{
			return;
		}
		
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(OwnerObject);
		if (!BindingOwner)
		{
			return;
		}

		FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
		if (!EditorBindings)
		{
			return;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		check(Schema);

		FEdGraphPinType PinType;

		if (UE::StateTree::PropertyRefHelpers::IsPropertyRef(*Property))
		{
			// Use internal type to construct PinType if it's property of PropertyRef type.
			PinType = UE::StateTree::PropertyRefHelpers::GetPropertyRefInternalTypeAsPin(*Property);
		}
		else
		{
			Schema->ConvertPropertyToPinType(Property, PinType);
		}

		if (const FStateTreePropertyPath* SourcePath = EditorBindings->GetPropertyBindingSource(TargetPath))
		{
			const FStateTreeBindableStructDesc* SourceDesc = UE::StateTree::PropertyBinding::FindStruct(AccessibleStructs, SourcePath->GetStructID());
			if (SourceDesc)
			{
				bool bIsValidBinding = false;

				// Check that the binding is valid.
				FStateTreeDataView SourceDataView;
				FStateTreeDataView TargetDataView;
				const FProperty* SourceLeafProperty = nullptr;
				const UStruct* SourceStruct = nullptr;
				if (BindingOwner->GetDataViewByID(SourcePath->GetStructID(), SourceDataView)
					&& BindingOwner->GetDataViewByID(TargetPath.GetStructID(), TargetDataView))
				{
					TArray<FStateTreePropertyPathIndirection> SourceIndirections;
					TArray<FStateTreePropertyPathIndirection> TargetIndirections;

					// Resolve source and target properties.
					// Source path can be empty, when the binding binds directly to a context struct/class.
					// Target path must always point to a valid property (at least one indirection).
					if (SourcePath->ResolveIndirectionsWithValue(SourceDataView, SourceIndirections)
						&& TargetPath.ResolveIndirectionsWithValue(TargetDataView, TargetIndirections)
						&& !TargetIndirections.IsEmpty())
					{
						const FStateTreePropertyPathIndirection LastTargetIndirection = TargetIndirections.Last();
						if (SourceIndirections.Num() > 0)
						{
							// Binding to a source property.
							SourceLeafProperty = SourceIndirections.Last().GetProperty();
							bIsValidBinding = ArePropertiesCompatible(SourceLeafProperty, LastTargetIndirection.GetProperty(), LastTargetIndirection.GetPropertyAddress());
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
						SourceType = UE::StateTree::PropertyBinding::GetPropertyTypeText(SourceLeafProperty);
					}
					else if (SourceStruct)
					{
						SourceType = SourceStruct->GetDisplayNameText();
					}
					FText TargetType = UE::StateTree::PropertyBinding::GetPropertyTypeText(Property);
					
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
			TooltipText = FText::Format(LOCTEXT("BindTooltip", "Bind {0} to value from another property."), UE::StateTree::PropertyBinding::GetPropertyTypeText(Property));
			Image = FAppStyle::GetBrush(PropertyIcon);
			Color = Schema->GetPinTypeColor(PinType);

			CachedSourcePath.Reset();
		}

		bIsDataCached = true;
	}

	bool CanBindToContextStruct(const UStruct* InStruct)
	{
		ConditionallyUpdateData();

		// Do not allow to bind directly StateTree nodes
		// @todo: find a way to more specifically call out the context structs, e.g. pass the property path to the callback.
		if (InStruct != nullptr)
		{
			const bool bIsStateTreeNode = AccessibleStructs.ContainsByPredicate([InStruct](const FStateTreeBindableStructDesc& AccessibleStruct)
			{
				return (AccessibleStruct.DataSource != EStateTreeBindableStructSource::Context && AccessibleStruct.DataSource != EStateTreeBindableStructSource::Parameter)
					&& AccessibleStruct.Struct == InStruct;
			});

			if (bIsStateTreeNode)
			{
				return false;
			}
		}

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
			return ArePropertiesCompatible(SourceProperty, PropertyHandle->GetProperty(), TargetValueAddress);
		}
		
		return false;
	}

	bool CanAcceptPropertyOrChildren(const FProperty* SourceProperty, TConstArrayView<FBindingChainElement> InBindingChain)
	{
		ConditionallyUpdateData();

		if (UE::StateTree::PropertyRefHelpers::IsPropertyRef(*PropertyHandle->GetProperty()))
		{
			const int32 SourceStructIndex = InBindingChain[0].ArrayIndex;
			check(AccessibleStructs.IsValidIndex(SourceStructIndex));

			if (!UE::StateTree::PropertyRefHelpers::IsPropertyAccessibleForPropertyRef(*SourceProperty, InBindingChain, AccessibleStructs[SourceStructIndex]))
			{
				return false;
			}
		}

		return SourceProperty->HasAnyPropertyFlags(CPF_Edit);
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

	static bool ArePropertiesCompatible(const FProperty* SourceProperty, const FProperty* TargetProperty, const void* TargetPropertyValue)
	{
		// @TODO: Refactor FStateTreePropertyBindings::ResolveCopyType() so that we can use it directly here.
		
		bool bCanBind = false;

		const FStructProperty* TargetStructProperty = CastField<FStructProperty>(TargetProperty);
		
		// AnyEnums need special handling.
		// It is a struct property but we want to treat it as an enum. We need to do this here, instead of 
		// FStateTreePropertyBindingCompiler::GetPropertyCompatibility() because the treatment depends on the value too.
		// Note: AnyEnums will need special handling before they can be used for binding.
		if (TargetStructProperty && TargetStructProperty->Struct == FStateTreeAnyEnum::StaticStruct())
		{
			// If the AnyEnum has AllowAnyBinding, allow to bind to any enum.
			const bool bAllowAnyBinding = TargetProperty->HasMetaData(UE::StateTree::PropertyBinding::AllowAnyBindingName);

			check(TargetPropertyValue);
			const FStateTreeAnyEnum* TargetAnyEnum = static_cast<const FStateTreeAnyEnum*>(TargetPropertyValue);

			// If the enum class is not specified, allow to bind to any enum, if the class is specified allow only that enum.
			if (const FByteProperty* SourceByteProperty = CastField<FByteProperty>(SourceProperty))
			{
				if (UEnum* Enum = SourceByteProperty->GetIntPropertyEnum())
				{
					bCanBind = bAllowAnyBinding || TargetAnyEnum->Enum == Enum;
				}
			}
			else if (const FEnumProperty* SourceEnumProperty = CastField<FEnumProperty>(SourceProperty))
			{
				bCanBind = bAllowAnyBinding || TargetAnyEnum->Enum == SourceEnumProperty->GetEnum();
			}
		}
		else if (TargetStructProperty && TargetStructProperty->Struct == FStateTreeStructRef::StaticStruct())
		{
			FString BaseStructName;
			const UScriptStruct* TargetStructRefBaseStruct = UE::StateTree::Compiler::GetBaseStructFromMetaData(TargetProperty, BaseStructName);

			if (const FStructProperty* SourceStructProperty = CastField<FStructProperty>(SourceProperty))
			{
				if (SourceStructProperty->Struct == TBaseStructure<FStateTreeStructRef>::Get())
				{
					FString SourceBaseStructName;
					const UScriptStruct* SourceStructRefBaseStruct = UE::StateTree::Compiler::GetBaseStructFromMetaData(SourceStructProperty, SourceBaseStructName);
					bCanBind = SourceStructRefBaseStruct && SourceStructRefBaseStruct->IsChildOf(TargetStructRefBaseStruct);
				}
				else
				{
					bCanBind = SourceStructProperty->Struct && SourceStructProperty->Struct->IsChildOf(TargetStructRefBaseStruct);
				}
			}
		}
		else if (TargetStructProperty && TargetStructProperty->Struct == FStateTreePropertyRef::StaticStruct())
		{
			check(TargetPropertyValue);
			bCanBind = UE::StateTree::PropertyRefHelpers::IsPropertyRefCompatibleWithProperty(*TargetStructProperty, *SourceProperty);
		}
		else
		{
			// Note: We support type promotion here
			bCanBind = FStateTreePropertyBindings::GetPropertyCompatibility(SourceProperty, TargetProperty) != EStateTreePropertyAccessCompatibility::Incompatible;
		}

		return bCanBind;
	}

	UStruct* ResolveIndirection(TArray<FBindingChainElement> InBindingChain)
	{
		UObject* OwnerObject = WeakOwnerObject.Get();
		if (!OwnerObject)
		{
			return nullptr;
		}
		
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(OwnerObject);
		if (!BindingOwner)
		{
			return nullptr;
		}

		const int32 SourceStructIndex = InBindingChain[0].ArrayIndex;
		check(SourceStructIndex >= 0 && SourceStructIndex < AccessibleStructs.Num());
		
		TArray<FBindingChainElement> SourceBindingChain = InBindingChain;
		SourceBindingChain.RemoveAt(0);

		FStateTreeDataView DataView;
		if (BindingOwner->GetDataViewByID(AccessibleStructs[SourceStructIndex].ID, DataView))
		{
			return UE::StateTree::PropertyBinding::ResolveLeafValueStructType(DataView, InBindingChain);
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
		UObject* OwnerObject = WeakOwnerObject.Get();
		if (!OwnerObject)
		{
			return;
		}
		
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(OwnerObject);
		if (!BindingOwner)
		{
			return;
		}

		FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
		if (!EditorBindings)
		{
			return;
		}

		const FStateTreePropertyPath* CurrentSourcePath = EditorBindings->GetPropertyBindingSource(TargetPath);
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
	
	TWeakObjectPtr<UObject> WeakOwnerObject = nullptr;
	FStateTreePropertyPath CachedSourcePath;
	FStateTreePropertyPath TargetPath;
	TSharedPtr<const IPropertyHandle> PropertyHandle;
	TArray<FStateTreeBindableStructDesc> AccessibleStructs;

	FText Text;
	FText TooltipText;
	FLinearColor Color = FLinearColor::White;
	const FSlateBrush* Image = nullptr;
	
	bool bIsDataCached = false;
};

} // UE::StateTree::PropertyBinding

bool FStateTreeBindingExtension::IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const
{
	const FProperty* Property = PropertyHandle.GetProperty();
	if (Property->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_EditorOnly | CPF_Config))
	{
		return false;
	}
	
	FStateTreePropertyPath TargetPath;
	// Figure out the structs we're editing, and property path relative to current property.
	const EStateTreePropertyUsage Usage = UE::StateTree::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(PropertyHandle.AsShared(), TargetPath);

	if (Usage == EStateTreePropertyUsage::Input || Usage == EStateTreePropertyUsage::Context)
	{
		// Allow to bind only to the main level on input and context properties.
		return TargetPath.GetSegments().Num() == 1;
	}
	if (Usage == EStateTreePropertyUsage::Parameter)
	{
		return true;
	}
	
	return false;
}


void FStateTreeBindingExtension::ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	if (!IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
	{
		return;
	}

	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	UObject* OwnerObject = nullptr;
	FStateTreeEditorPropertyBindings* EditorBindings = nullptr;

	// Array of structs we can bind to.
	TArray<FBindingContextStruct> BindingContextStructs;
	TArray<FStateTreeBindableStructDesc> AccessibleStructs;

	// The struct and property where we're binding.
	FStateTreePropertyPath TargetPath;

	IStateTreeEditorPropertyBindingsOwner* BindingOwner = nullptr;

	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		// Only allow to binding when one object is selected.
		OwnerObject = UE::StateTree::PropertyBinding::FindEditorBindingsOwner(OuterObjects[0]);

		// Figure out the structs we're editing, and property path relative to current property.
		UE::StateTree::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(InPropertyHandle, TargetPath);

		BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(OwnerObject);
		if (BindingOwner)
		{
			EditorBindings = BindingOwner->GetPropertyEditorBindings();
			BindingOwner->GetAccessibleStructs(TargetPath.GetStructID(), AccessibleStructs);

			for (FStateTreeBindableStructDesc& StructDesc : AccessibleStructs)
			{
				const UStruct* Struct = StructDesc.Struct;

				FBindingContextStruct& ContextStruct = BindingContextStructs.AddDefaulted_GetRef();
				ContextStruct.DisplayText = FText::FromString(StructDesc.Name.ToString());
				ContextStruct.Struct = const_cast<UStruct*>(Struct);
				ContextStruct.Section = UE::StateTree::PropertyBinding::GetSectionNameFromDataSource(StructDesc.DataSource);
			}
		}

		// Wrap value widget 
		if (EditorBindings)
		{
			auto IsValueVisible = TAttribute<EVisibility>::Create([TargetPath, EditorBindings]() -> EVisibility
			{
				return EditorBindings->HasPropertyBinding(TargetPath) ? EVisibility::Collapsed : EVisibility::Visible;
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
		}
		
	}

	TSharedPtr<UE::StateTree::PropertyBinding::FCachedBindingData> CachedBindingData = MakeShared<UE::StateTree::PropertyBinding::FCachedBindingData>(OwnerObject, TargetPath, InPropertyHandle, AccessibleStructs);
	
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

	Args.OnCanRemoveBinding = FOnCanRemoveBinding::CreateLambda([EditorBindings, TargetPath](FName InPropertyName)
		{
			return EditorBindings && EditorBindings->HasPropertyBinding(TargetPath);
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

	if (BindingOwner)
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

	InWidgetRow.ExtensionContent()
	[
		PropertyAccessEditor.MakePropertyBindingWidget(BindingContextStructs, Args)
	];
}

#undef LOCTEXT_NAMESPACE
