// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcessSettingsCustomization.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/BlendableInterface.h"
#include "Factories/Factory.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "ObjectEditorUtils.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "SlotBase.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Tuple.h"
#include "Textures/SlateIcon.h"
#include "Types/SlateStructs.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class IDetailPropertyRow;
class SWidget;
class UPackage;

#define LOCTEXT_NAMESPACE "PostProcessSettingsCustomization"

const FName ShowPostProcessCategoriesName("ShowPostProcessCategories");
const FName ShowOnlyInnerPropertiesName("ShowOnlyInnerProperties");

struct FCategoryOrGroup
{
	IDetailCategoryBuilder* Category;
	IDetailGroup* Group;

	FCategoryOrGroup(IDetailCategoryBuilder& NewCategory)
		: Category(&NewCategory)
		, Group(nullptr)
	{}

	FCategoryOrGroup(IDetailGroup& NewGroup)
		: Category(nullptr)
		, Group(&NewGroup)
	{}

	FCategoryOrGroup()
		: Category(nullptr)
		, Group(nullptr)
	{}

	IDetailPropertyRow& AddProperty(TSharedRef<IPropertyHandle> PropertyHandle)
	{
		if (Category)
		{
			return Category->AddProperty(PropertyHandle);
		}
		else
		{
			return Group->AddPropertyRow(PropertyHandle);
		}
	}

	IDetailGroup& AddGroup(FName GroupName, const FText& DisplayName)
	{
		if (Category)
		{
			return Category->AddGroup(GroupName, DisplayName);
		}
		else
		{
			return Group->AddGroup(GroupName, DisplayName);
		}
	}

	bool IsValid() const
	{
		return Group || Category;
	}
};


struct FPostProcessGroup
{
	FString RawGroupName;
	FString DisplayName;
	FCategoryOrGroup RootCategory;
	TArray<TSharedPtr<IPropertyHandle>> SimplePropertyHandles;
	TArray<TSharedPtr<IPropertyHandle>> AdvancedPropertyHandles;

	bool IsValid() const
	{
		return !RawGroupName.IsEmpty() && !DisplayName.IsEmpty() && RootCategory.IsValid();
	}

	FPostProcessGroup()
		: RootCategory()
	{}
};

void FPostProcessSettingsCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren = 0;
	FPropertyAccess::Result Result = StructPropertyHandle->GetNumChildren(NumChildren);

	FProperty* Prop = StructPropertyHandle->GetProperty();
	FStructProperty* StructProp = CastField<FStructProperty>(Prop);

	// a category with this name should be one level higher, should be "PostProcessSettings"
	FName ClassName = StructProp->Struct->GetFName();

	// Create new categories in the parent layout rather than adding all post process settings to one category
	IDetailLayoutBuilder& LayoutBuilder = StructBuilder.GetParentCategory().GetParentLayout();

	TMap<FString, FCategoryOrGroup> NameToCategoryBuilderMap;
	TMap<FString, FPostProcessGroup> NameToGroupMap;

	static const auto VarDefaultAutoExposureExtendDefaultLuminanceRange = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	const bool bExtendedLuminanceRange = VarDefaultAutoExposureExtendDefaultLuminanceRange->GetValueOnGameThread() == 1;
	static const FName ExposureCategory("Lens|Exposure");



	bool bShowPostProcessCategories = StructPropertyHandle->HasMetaData(ShowPostProcessCategoriesName);

	if(Result == FPropertyAccess::Success && NumChildren > 0)
	{
		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex );

			if( ChildHandle.IsValid() && ChildHandle->GetProperty() )
			{
				FProperty* Property = ChildHandle->GetProperty();

				FName CategoryFName = FObjectEditorUtils::GetCategoryFName(Property);
					
				if (CategoryFName == ExposureCategory && bExtendedLuminanceRange)
				{
					if (Property->GetName() == TEXT("AutoExposureMinBrightness"))
					{
						Property->SetMetaData(TEXT("DisplayName"), TEXT("Min EV100"));
					}
					else if (Property->GetName() == TEXT("AutoExposureMaxBrightness"))
					{
						Property->SetMetaData(TEXT("DisplayName"), TEXT("Max EV100"));
					}
					else if (Property->GetName() == TEXT("HistogramLogMin"))
					{
						Property->SetMetaData(TEXT("DisplayName"), TEXT("Histogram Min EV100"));
					}
					else if (Property->GetName() == TEXT("HistogramLogMax"))
					{
						Property->SetMetaData(TEXT("DisplayName"), TEXT("Histogram Max EV100"));
					}
				}
				
				
				FString RawCategoryName = CategoryFName.ToString();

				TArray<FString> CategoryAndGroups;
				RawCategoryName.ParseIntoArray(CategoryAndGroups, TEXT("|"), 1);

				FString RootCategoryName = CategoryAndGroups.Num() > 0 ? CategoryAndGroups[0] : RawCategoryName;


				FCategoryOrGroup* Category = NameToCategoryBuilderMap.Find(RootCategoryName);
				if(!Category)
				{
					if(bShowPostProcessCategories)
					{
					IDetailCategoryBuilder& NewCategory = LayoutBuilder.EditCategory(*RootCategoryName, FText::GetEmpty(), ECategoryPriority::TypeSpecific);
						Category = &NameToCategoryBuilderMap.Emplace(RootCategoryName, NewCategory);
					}
					else
					{
						IDetailGroup& NewGroup = StructBuilder.AddGroup(*RootCategoryName, FText::FromString(RootCategoryName));
						Category = &NameToCategoryBuilderMap.Emplace(RootCategoryName, NewGroup);
					}
				}

				if(CategoryAndGroups.Num() > 1)
				{
					// Only handling one group for now
					// There are sub groups so add them now
					FPostProcessGroup& PPGroup = NameToGroupMap.FindOrAdd(RawCategoryName);
					
					// Is this a new group? It wont be valid if it is
					if(!PPGroup.IsValid())
					{
						PPGroup.RootCategory = *Category;
						PPGroup.RawGroupName = RawCategoryName;
						PPGroup.DisplayName = CategoryAndGroups[1].TrimStartAndEnd();
					}
	
					bool bIsSimple = !ChildHandle->GetProperty()->HasAnyPropertyFlags(CPF_AdvancedDisplay);
					if(bIsSimple)
					{
						PPGroup.SimplePropertyHandles.Add(ChildHandle);
					}
					else
					{
						PPGroup.AdvancedPropertyHandles.Add(ChildHandle);
					}
				}
				else
				{
					Category->AddProperty(ChildHandle.ToSharedRef());
				}

			}
		}

		for(auto& NameAndGroup : NameToGroupMap)
		{
			FPostProcessGroup& PPGroup = NameAndGroup.Value;

			if(PPGroup.SimplePropertyHandles.Num() > 0 || PPGroup.AdvancedPropertyHandles.Num() > 0 )
			{
				IDetailGroup& SimpleGroup = PPGroup.RootCategory.AddGroup(*PPGroup.RawGroupName, FText::FromString(PPGroup.DisplayName));

				static const FString ColorGradingName = TEXT("Color Grading");

				// Only enable group reset on color grading category groups
				if (PPGroup.RawGroupName.Contains(ColorGradingName))
				{
					SimpleGroup.EnableReset(true);
				}

				for(auto& SimpleProperty : PPGroup.SimplePropertyHandles)
				{
					SimpleGroup.AddPropertyRow(SimpleProperty.ToSharedRef());
				}

				if(PPGroup.AdvancedPropertyHandles.Num() > 0)
				{
					IDetailGroup& AdvancedGroup = SimpleGroup.AddGroup(*(PPGroup.RawGroupName+TEXT("Advanced")), LOCTEXT("PostProcessAdvancedGroup", "Advanced"));
					
					for(auto& AdvancedProperty : PPGroup.AdvancedPropertyHandles)
					{
						AdvancedGroup.AddPropertyRow(AdvancedProperty.ToSharedRef());
					}
				}
			}
		}
	}
}

void FPostProcessSettingsCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	bool bShowHeader = !StructPropertyHandle->HasMetaData(ShowPostProcessCategoriesName) && !StructPropertyHandle->HasMetaData(ShowOnlyInnerPropertiesName);
	if(bShowHeader)
	{
		HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];

		HeaderRow.ValueContent()
		[
			StructPropertyHandle->CreatePropertyValueWidget()
		];
	}
}

void FWeightedBlendableCustomization::AddDirectAsset(TSharedRef<IPropertyHandle> StructPropertyHandle, TSharedPtr<IPropertyHandle> Weight, TSharedPtr<IPropertyHandle> Value, UClass* Class)
{
	Weight->SetValue(1.0f);

	{
		TArray<UObject*> Objects;
		StructPropertyHandle->GetOuterObjects(Objects);

		TArray<FString> Values;

		for(TArray<UObject*>::TConstIterator It = Objects.CreateConstIterator(); It; It++)
		{
			UObject* Obj = *It;

			const UObject* NewObj = NewObject<UObject>(Obj, Class);

			FString Str = NewObj->GetPathName();

			Values.Add(Str);
		}

		Value->SetPerObjectValues(Values);
	}
}

void FWeightedBlendableCustomization::AddIndirectAsset(TSharedPtr<IPropertyHandle> Weight)
{
	Weight->SetValue(1.0f);
}

EVisibility FWeightedBlendableCustomization::IsWeightVisible(TSharedPtr<IPropertyHandle> Weight) const
{
	float WeightValue = 1.0f;
	
	Weight->GetValue(WeightValue);

	return (WeightValue >= 0) ? EVisibility::Visible : EVisibility::Hidden;
}

FText FWeightedBlendableCustomization::GetDirectAssetName(TSharedPtr<IPropertyHandle> Value) const
{
	UObject* RefObject = 0;
	
	Value->GetValue(RefObject);

	check(RefObject);

	return FText::FromString(RefObject->GetFullName());
}

FReply FWeightedBlendableCustomization::JumpToDirectAsset(TSharedPtr<IPropertyHandle> Value)
{
	UObject* RefObject = 0;
	
	Value->GetValue(RefObject);

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(RefObject);

	return FReply::Handled();
}

TSharedRef<SWidget> FWeightedBlendableCustomization::GenerateContentWidget(TSharedRef<IPropertyHandle> StructPropertyHandle, UPackage* Package, TSharedPtr<IPropertyHandle> Weight, TSharedPtr<IPropertyHandle> Value)
{
	bool bSeparatorIsNeeded = false; 

	FMenuBuilder MenuBuilder(true, NULL);
	{
		for(TObjectIterator<UClass> It; It; ++It)
		{
			if( It->IsChildOf(UFactory::StaticClass()))
			{
				UFactory* Factory = It->GetDefaultObject<UFactory>();

				check(Factory);

				UClass* SupportedClass = Factory->GetSupportedClass();

				if(SupportedClass)
				{
					if(SupportedClass->ImplementsInterface(UBlendableInterface::StaticClass()))
					{
						// At the moment we know about 3 Blendables: Material, UMaterialInstanceConstant, LightPropagationVolumeBlendable
						// The materials are not that useful to have here (hard to reference) so we suppress them here
						if(!(
							SupportedClass == UMaterial::StaticClass() ||
							SupportedClass == UMaterialInstanceConstant::StaticClass()
							))
						{
							FUIAction Direct2(FExecuteAction::CreateSP(this, &FWeightedBlendableCustomization::AddDirectAsset, StructPropertyHandle, Weight, Value, SupportedClass));

							FName ClassName = SupportedClass->GetFName();
						
							MenuBuilder.AddMenuEntry(FText::FromString(ClassName.GetPlainNameString()),
								LOCTEXT("Blendable_DirectAsset2h", "Creates an asset that is owned by the containing object"), FSlateIcon(), Direct2);

							bSeparatorIsNeeded = true;
						}
					}
				}
			}
		}

		if(bSeparatorIsNeeded)
		{
			MenuBuilder.AddMenuSeparator();
		}

		FUIAction Indirect(FExecuteAction::CreateSP(this, &FWeightedBlendableCustomization::AddIndirectAsset, Weight));
		MenuBuilder.AddMenuEntry(LOCTEXT("Blendable_IndirectAsset", "Asset reference"), 
			LOCTEXT("Blendable_IndirectAsseth", "reference a Blendable asset (owned by a content package), e.g. material with Post Process domain"), FSlateIcon(), Indirect);
	}
	

	TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher)
			.WidgetIndex(this, &FWeightedBlendableCustomization::ComputeSwitcherIndex, StructPropertyHandle, Package, Weight, Value);

	Switcher->AddSlot()
		[
			SNew(SComboButton)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Blendable_ChooseElement", "Choose"))
			]
			.ContentPadding(FMargin(6.0, 2.0))
			.MenuContent()
			[
				MenuBuilder.MakeWidget()
			]
		];

	Switcher->AddSlot()
		[
			SNew(SButton)
			.ContentPadding(FMargin(0,0))
			.Text(this, &FWeightedBlendableCustomization::GetDirectAssetName, Value)
			.OnClicked(this, &FWeightedBlendableCustomization::JumpToDirectAsset, Value)
		];

	Switcher->AddSlot()
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(Value)
		];

	return Switcher;
}


int32 FWeightedBlendableCustomization::ComputeSwitcherIndex(TSharedRef<IPropertyHandle> StructPropertyHandle, UPackage* Package, TSharedPtr<IPropertyHandle> Weight, TSharedPtr<IPropertyHandle> Value) const
{
	float WeightValue = 1.0f;
	UObject* RefObject = 0;
	
	Weight->GetValue(WeightValue);
	Value->GetValue(RefObject);

	if(RefObject)
	{
		UPackage* PropPackage = RefObject->GetOutermost();

		return (PropPackage == Package) ? 1 : 2;
	}
	else
	{
		return (WeightValue < 0.0f) ? 0 : 2;
	}
}

void FWeightedBlendableCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// we don't have children but this is a pure virtual so we need to override
}

void FWeightedBlendableCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> SharedWeightProp;
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(FName(TEXT("Weight")));
		if (ChildHandle.IsValid() && ChildHandle->GetProperty())
		{
			SharedWeightProp = ChildHandle;
		}
	}
		
	TSharedPtr<IPropertyHandle> SharedValueProp;
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(FName(TEXT("Object")));
		if (ChildHandle.IsValid() && ChildHandle->GetProperty())
		{
			SharedValueProp = ChildHandle;
		}
	}

	float WeightValue = 1.0f;
	UObject* RefObject = 0;
	
	SharedWeightProp->GetValue(WeightValue);
	SharedValueProp->GetValue(RefObject);

	UPackage* StructPackage = 0;
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
		TArray<UObject*> Objects;
		StructPropertyHandle->GetOuterObjects(Objects);

		for (TArray<UObject*>::TConstIterator It = Objects.CreateConstIterator(); It; It++)
		{
			UObject* ref = *It;

			if (StructPackage)
			{
				// Differing outermost package values indicate that the current RefObject refers to post-process 
				// volumes selected within different levels, e.g. persistent and a sub-level. 
				// In this case, do not store a package name. It is only used by ComputeSwitcherIndex() to determine direct
				// vs. indirect assets in the post process materials/blendables array. When more than one volume is selected, the direct
				// asset entries will simple read 'Multiple values' since each belongs to separate post-process volumes.
				if (StructPackage != ref->GetOutermost())
				{
					StructPackage = NULL;
					break;
				}
			}
			else
			{
				StructPackage = ref->GetOutermost();
			}
		}
	}

	HeaderRow.NameContent()
	[
		SNew(SHorizontalBox)
		.Visibility(this, &FWeightedBlendableCustomization::IsWeightVisible, SharedWeightProp)
		+SHorizontalBox::Slot()
		[
			SNew(SBox)
			.MinDesiredWidth(60.0f)
			.MaxDesiredWidth(60.0f)		
			[
				SharedWeightProp->CreatePropertyValueWidget()
			]
		]
	];

	HeaderRow.ValueContent()
	.MaxDesiredWidth(0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			GenerateContentWidget(StructPropertyHandle, StructPackage, SharedWeightProp, SharedValueProp)
		]
	];
}





#undef LOCTEXT_NAMESPACE
