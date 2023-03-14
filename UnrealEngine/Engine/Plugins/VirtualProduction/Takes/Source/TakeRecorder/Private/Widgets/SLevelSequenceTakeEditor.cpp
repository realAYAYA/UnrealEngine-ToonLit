// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SLevelSequenceTakeEditor.h"
#include "Widgets/TakeRecorderWidgetConstants.h"
#include "TakePreset.h"
#include "TakeMetaData.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSourceProperty.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderStyle.h"
#include "Widgets/STakeRecorderSources.h"
#include "TakeRecorderModule.h"

#include "LevelSequence.h"

// Core includes
#include "Modules/ModuleManager.h"
#include "Algo/Sort.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectIterator.h"
#include "Templates/SubclassOf.h"
#include "ClassIconFinder.h"

// AssetRegistry includes
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"

// ContentBrowser includes
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

// AssetTools includes
#include "AssetToolsModule.h"

// Slate includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateIconFinder.h"
#include "SPositiveActionButton.h"

// Style includes
#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"

// UnrealEd includes
#include "ScopedTransaction.h"

// PropertyEditor includes
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailRootObjectCustomization.h"
#include "DetailWidgetRow.h"

// Widget Includes
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SLevelSequenceTakeEditor"


TArray<UClass*> FindRecordingSourceClasses()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> ClassList;

	FARFilter Filter;
	Filter.ClassPaths.Add(UTakeRecorderSource::StaticClass()->GetClassPathName());

	// Include any Blueprint based objects as well, this includes things like Blutilities, UMG, and GameplayAbility objects
	Filter.bRecursiveClasses = true;
	AssetRegistryModule.Get().GetAssets(Filter, ClassList);

	TArray<UClass*> Classes;

	for (const FAssetData& Data : ClassList)
	{
		UClass* Class = Data.GetClass();
		if (Class)
		{
			Classes.Add(Class);
		}
	}

	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		if (ClassIterator->IsChildOf(UTakeRecorderSource::StaticClass()) && !ClassIterator->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			Classes.Add(*ClassIterator);
		}
	}

	return Classes;
}

PRAGMA_DISABLE_OPTIMIZATION
void SLevelSequenceTakeEditor::Construct(const FArguments& InArgs)
{
	bRequestDetailsRefresh = true;
	LevelSequenceAttribute = InArgs._LevelSequence;

	DetailsBox = SNew(SScrollBox);
	DetailsBox->SetScrollBarRightClickDragAllowed(true);

	SourcesWidget = SNew(STakeRecorderSources)
	.OnSelectionChanged(this, &SLevelSequenceTakeEditor::OnSourcesSelectionChanged);

	CheckForNewLevelSequence();

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)

		+ SSplitter::Slot()
		.Value(.5f)
		[
			SNew(SBorder)
			.Padding(4)
			.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				SourcesWidget.ToSharedRef()
			]
		]

		+ SSplitter::Slot()
		.Value(.5f)
		[
			DetailsBox.ToSharedRef()
		]
	];
}

TSharedRef<SWidget> SLevelSequenceTakeEditor::MakeAddSourceButton()
{
	return SNew(SPositiveActionButton)
		.OnGetMenuContent(this, &SLevelSequenceTakeEditor::OnGenerateSourcesMenu)
		.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
		.Text(LOCTEXT("AddNewSource_Text", "Source"));
}
PRAGMA_ENABLE_OPTIMIZATION

void SLevelSequenceTakeEditor::AddExternalSettingsObject(UObject* InObject)
{
	check(InObject);

	ExternalSettingsObjects.AddUnique(InObject);
	bRequestDetailsRefresh = true;
}

bool SLevelSequenceTakeEditor::RemoveExternalSettingsObject(UObject* InObject)
{
	check(InObject);

	int32 NumRemoved = ExternalSettingsObjects.Remove(InObject);
	if (NumRemoved > 0)
	{
		bRequestDetailsRefresh = true;
		return true;
	}

	return false;
}

void SLevelSequenceTakeEditor::CheckForNewLevelSequence()
{
	ULevelSequence* NewLevelSequence = LevelSequenceAttribute.Get();
	if (CachedLevelSequence != NewLevelSequence)
	{
		CachedLevelSequence = NewLevelSequence;

		UTakeRecorderSources* Sources = NewLevelSequence ? NewLevelSequence->FindOrAddMetaData<UTakeRecorderSources>() : nullptr;

		SourcesWidget->SetSourceObject(Sources);
		bRequestDetailsRefresh = true;
	}
}

void SLevelSequenceTakeEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CheckForNewLevelSequence();
	if (bRequestDetailsRefresh)
	{
		UpdateDetails();
		bRequestDetailsRefresh = false;
	}
}

TSharedRef<SWidget> SLevelSequenceTakeEditor::OnGenerateSourcesMenu()
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	{
		ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
		UTakeRecorderSources* Sources       = LevelSequence ? LevelSequence->FindOrAddMetaData<UTakeRecorderSources>() : nullptr;

		if (Sources)
		{
			FTakeRecorderModule& TakeRecorderModule = FModuleManager::GetModuleChecked<FTakeRecorderModule>("TakeRecorder");
			TakeRecorderModule.PopulateSourcesMenu(Extender, Sources);
		}
	}

	FMenuBuilder MenuBuilder(true, nullptr, Extender);

	MenuBuilder.BeginSection("Sources", LOCTEXT("SourcesMenuSection", "Available Sources"));
	{
		TArray<UClass*> SourceClasses = FindRecordingSourceClasses();
		Algo::SortBy(SourceClasses, &UClass::GetDisplayNameText, FText::FSortPredicate());

		for (UClass* Class : SourceClasses)
		{
			TSubclassOf<UTakeRecorderSource> SubclassOf = Class;

			MenuBuilder.AddMenuEntry(
				FText::FromString(Class->GetMetaData(TEXT("TakeRecorderDisplayName"))),
				Class->GetToolTipText(true),
				FSlateIconFinder::FindIconForClass(Class),
				FUIAction(
					FExecuteAction::CreateSP(this, &SLevelSequenceTakeEditor::AddSourceFromClass, SubclassOf),
					FCanExecuteAction::CreateSP(this, &SLevelSequenceTakeEditor::CanAddSourceFromClass, SubclassOf)
				)
			);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SLevelSequenceTakeEditor::AddSourceFromClass(TSubclassOf<UTakeRecorderSource> SourceClass)
{
	ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
	UTakeRecorderSources* Sources       = LevelSequence ? LevelSequence->FindOrAddMetaData<UTakeRecorderSources>() : nullptr;

	if (*SourceClass && Sources)
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("AddNewSource", "Add New {0} Source"), SourceClass->GetDisplayNameText()));
		Sources->Modify();

		Sources->AddSource(SourceClass);
	}
}

bool SLevelSequenceTakeEditor::CanAddSourceFromClass(TSubclassOf<UTakeRecorderSource> SourceClass)
{
	ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
	UTakeRecorderSources* Sources = LevelSequence ? LevelSequence->FindOrAddMetaData<UTakeRecorderSources>() : nullptr;

	if (*SourceClass && Sources)
	{
		UTakeRecorderSource* Default = SourceClass->GetDefaultObject<UTakeRecorderSource>();
		return Default->CanAddSource(Sources);
	}

	return false;
}

void SLevelSequenceTakeEditor::OnSourcesSelectionChanged(TSharedPtr<ITakeRecorderSourceTreeItem>, ESelectInfo::Type)
{
	bRequestDetailsRefresh = true;
}

class FRecordedPropertyCustomization : public IPropertyTypeCustomization
{
	static const FString PropertyPathDelimiter;

   	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils )
   	{
	   	if (PropertyHandle->IsValidHandle())
	   	{
	        TSharedPtr<IPropertyHandle> PropertyNameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, PropertyName)); 
	        TSharedPtr<IPropertyHandle> bEnabledHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, bEnabled)); 
	        TSharedPtr<IPropertyHandle> RecorderNameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, RecorderName));

	        FString PropertyNameValue;
	        PropertyNameHandle->GetValueAsDisplayString(PropertyNameValue);

		    FString ParentGroups; 
		    FString PropertyName;
		    FText DisplayString = FText::FromString(PropertyNameValue);
		    if (PropertyNameValue.Split(PropertyPathDelimiter, &ParentGroups, &PropertyName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		    {
		    	DisplayString = FText::FromString(PropertyName);
			}

	        HeaderRow 
	        [

	        	SNew(SHorizontalBox)

	        	+SHorizontalBox::Slot()
	        	.AutoWidth()
	        	[
	        		bEnabledHandle->CreatePropertyValueWidget(false)
	        	]

	        	+SHorizontalBox::Slot()
	        	.Padding(8, 0, 0, 0)
	        	[
	        		PropertyNameHandle->CreatePropertyNameWidget(DisplayString)
	        	]

	        ];
	   	}
   		return;
   	}

	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils )
	{
		// Intentionally Left Blank, Child Customization was handled in the Header Row
	}
};

const FString FRecordedPropertyCustomization::PropertyPathDelimiter = FString(TEXT("."));

class FRecorderPropertyMapCustomization : public IPropertyTypeCustomization
{
	static const FString PropertyPathDelimiter; 

	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils )
	{
	    TSharedPtr<IPropertyHandle> RecordedObjectHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, RecordedObject)); 

	    UObject* Object = nullptr;
	    FText ActorOrComponentName = LOCTEXT("MissingActorOrComponentName", "MissingActorOrComponentName");

	    const FSlateBrush* Icon = nullptr;

	    bool bSuccess = false;
		if ( RecordedObjectHandle->IsValidHandle() && RecordedObjectHandle->GetValue(Object) == FPropertyAccess::Success && Object != nullptr )
		{
			bSuccess = true;

			if (AActor* Actor = Cast<AActor>(Object))
			{
				ActorOrComponentName = FText::AsCultureInvariant(Actor->GetActorLabel());
				Icon = FClassIconFinder::FindIconForActor(Actor);
			}
			else
			{
				ActorOrComponentName = FText::AsCultureInvariant(Object->GetName());
				Icon = FSlateIconFinder::FindIconBrushForClass(Object->GetClass());
			}
		}

	    HeaderRow
	    [
	    	SNew(SHorizontalBox)

	    	+SHorizontalBox::Slot()
	    	.AutoWidth()
	    	.VAlign(VAlign_Center)
	    	[
	    		SNew( SCheckBox )
	    		.OnCheckStateChanged( this, &FRecorderPropertyMapCustomization::OnCheckStateChanged, PropertyHandle )	
	    		.IsChecked( this, &FRecorderPropertyMapCustomization::OnGetCheckState, PropertyHandle )	
	    		.Padding(0.0)
	    	]

	    	+SHorizontalBox::Slot()
	    	.AutoWidth()
	    	.VAlign(VAlign_Center)
	    	.Padding(8, 0, 0, 0)
	    	[
	    		SNew(SImage)
				.Image(Icon)
	    	]

	    	+SHorizontalBox::Slot()
	    	.VAlign(VAlign_Center)
	    	.AutoWidth()
	    	.Padding(2.0, 0.0)
	    	[
	    		SNew(STextBlock)
	    		.Text(ActorOrComponentName)
	    		.Font( FAppStyle::GetFontStyle( "PropertyWindow.BoldFont" ))
	    	]

	    	+SHorizontalBox::Slot()
	    	.FillWidth(1.0f)
	    	.VAlign(VAlign_Center)
	    	.Padding(2.0, 0.0)
	    	[
	    		SNew(STextBlock)
	    		.Text(LOCTEXT("TakeRecorderRecordedPropertiesTitle", "Recorded Properties"))
	    		.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ))
	    	]
	    ];
	}

	virtual IDetailGroup& GetOrCreateDetailGroup(IDetailChildrenBuilder& ChildBuilder,  TMap<FString, IDetailGroup*>& GroupMap, TSharedPtr<IPropertyHandleArray> PropertiesArray, FString& GroupName)
	{
		if (GroupMap.Contains(GroupName))
		{
			return *GroupMap[GroupName];
		}

		FString ParentGroups; 
		FString PropertyName;
		FText DisplayName;

		if (GroupName.Split(PropertyPathDelimiter, &ParentGroups, &PropertyName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			IDetailGroup& ParentGroup = GetOrCreateDetailGroup(ChildBuilder, GroupMap, PropertiesArray, ParentGroups);
			DisplayName = FText::FromString(PropertyName);
			GroupMap.Add(GroupName, &ParentGroup.AddGroup(FName(*PropertyName), DisplayName));
		}
		else 
		{
			DisplayName = FText::FromString(GroupName);
			GroupMap.Add(GroupName, &ChildBuilder.AddGroup(FName(*GroupName), DisplayName));
		}

		GroupMap[GroupName]->HeaderRow()
		[
			SNew(SHorizontalBox)

	    	+SHorizontalBox::Slot()
	    	.AutoWidth()
	    	.VAlign(VAlign_Center)
	    	[
	    		SNew( SCheckBox )
	    		.OnCheckStateChanged( this, &FRecorderPropertyMapCustomization::OnGroupCheckStateChanged, PropertiesArray, GroupName )	
	    		.IsChecked( this, &FRecorderPropertyMapCustomization::OnGroupGetCheckState, PropertiesArray, GroupName )	
	    	]

	    	+SHorizontalBox::Slot()
	    	.VAlign(VAlign_Center)
	    	.AutoWidth()
	    	.Padding(6.0, 0.0)
	    	[
	    		SNew(STextBlock)
	    		.Text(DisplayName)
	    		.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ))
	    	]
		];

		return *GroupMap[GroupName];
	}


	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils )
	{
		TMap<FString, IDetailGroup*> DetailGroupMap;

		TSharedPtr<IPropertyHandleArray> RecordedPropertiesArrayHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, Properties))->AsArray();

		uint32 NumProperties;
		RecordedPropertiesArrayHandle->GetNumElements(NumProperties);


		for(uint32 i = 0; i < NumProperties; ++i)
		{
			TSharedRef<IPropertyHandle> RecordedPropertyTemp = RecordedPropertiesArrayHandle->GetElement(i);
			if (RecordedPropertyTemp->IsValidHandle())
			{

	        	TSharedPtr<IPropertyHandle> PropertyNameHandle = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, PropertyName)); 
	        	if (PropertyNameHandle->IsValidHandle())
	        	{
		        	FString PropertyNameValue;
		        	PropertyNameHandle->GetValueAsDisplayString(PropertyNameValue);

		        	FString ParentGroups; 
		        	FString PropertyName;
		        	if (PropertyNameValue.Split(PropertyPathDelimiter, &ParentGroups, &PropertyName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		        	{
		        		IDetailGroup& ParentGroup = GetOrCreateDetailGroup(ChildBuilder, DetailGroupMap, RecordedPropertiesArrayHandle, ParentGroups);
		        		ParentGroup.AddPropertyRow(RecordedPropertyTemp);
		        	}
		        	else 
		        	{
						ChildBuilder.AddProperty(RecordedPropertyTemp);
		        	}
	        	}
			}
		}

		TSharedPtr<IPropertyHandleArray> RecordedComponentsArrayHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, Children))->AsArray();

		uint32 NumCompProperties;
		RecordedComponentsArrayHandle->GetNumElements(NumCompProperties);

		for(uint32 i = 0; i < NumCompProperties; ++i)
		{
			TSharedRef<IPropertyHandle> RecordedCompTemp = RecordedComponentsArrayHandle->GetElement(i);
			if (RecordedCompTemp->IsValidHandle())
			{
				ChildBuilder.AddProperty(RecordedCompTemp);
			}
		}
	}

	void OnGroupCheckStateChanged( ECheckBoxState InNewState, TSharedPtr<IPropertyHandleArray> RecordedPropertiesArrayHandle, FString GroupName) const  
	{
		uint32 NumProperties;
		RecordedPropertiesArrayHandle->GetNumElements(NumProperties);

		for(uint32 i = 0; i < NumProperties; ++i)
		{
			TSharedRef<IPropertyHandle> RecordedPropertyTemp = RecordedPropertiesArrayHandle->GetElement(i);
			if (RecordedPropertyTemp->IsValidHandle())
			{
	        	TSharedPtr<IPropertyHandle> PropertyNameHandle = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, PropertyName)); 
	        	if (PropertyNameHandle->IsValidHandle())
	        	{
		        	FString PropertyNameValue;
		        	PropertyNameHandle->GetValueAsDisplayString(PropertyNameValue);

		        	if (PropertyNameValue.StartsWith(GroupName))
		        	{
						TSharedPtr<IPropertyHandle> EnabledRecordedPropertyTemp = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, bEnabled));
						if (EnabledRecordedPropertyTemp->IsValidHandle())
						{
							EnabledRecordedPropertyTemp->SetValue( InNewState == ECheckBoxState::Checked ? true : false );
						}
					}
				}
			}
		}
	}

	ECheckBoxState OnGroupGetCheckState( TSharedPtr<IPropertyHandleArray> RecordedPropertiesArrayHandle, FString GroupName) const 
	{
		bool SetFirst = false;
		bool FinalCheckedValue = false;

		uint32 NumProperties;
		RecordedPropertiesArrayHandle->GetNumElements(NumProperties);

		for(uint32 i = 0; i < NumProperties; ++i)
		{
			TSharedRef<IPropertyHandle> RecordedPropertyTemp = RecordedPropertiesArrayHandle->GetElement(i);
			if (RecordedPropertyTemp->IsValidHandle())
			{
	        	TSharedPtr<IPropertyHandle> PropertyNameHandle = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, PropertyName)); 
	        	if (PropertyNameHandle->IsValidHandle())
	        	{
		        	FString PropertyNameValue;
		        	PropertyNameHandle->GetValueAsDisplayString(PropertyNameValue);

		        	if (PropertyNameValue.StartsWith(GroupName))
		        	{
						TSharedPtr<IPropertyHandle> EnabledRecordedPropertyTemp = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, bEnabled));
						if (EnabledRecordedPropertyTemp->IsValidHandle())
						{
							bool EnabledValue;
							if ( EnabledRecordedPropertyTemp->GetValue(EnabledValue) == FPropertyAccess::Success )
							{
								if (!SetFirst)
								{
									FinalCheckedValue = EnabledValue;
									SetFirst = true;
								}
								else if ( EnabledValue != FinalCheckedValue )
								{
									return ECheckBoxState::Undetermined;
								}
							}
						}
					}
				}
			}
		}

		return FinalCheckedValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnCheckStateChanged( ECheckBoxState InNewState, TSharedRef<IPropertyHandle> PropertyHandle )
	{
		TSharedPtr<IPropertyHandleArray> RecordedPropertiesArrayHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, Properties))->AsArray();

		uint32 NumProperties;
		RecordedPropertiesArrayHandle->GetNumElements(NumProperties);

		for(uint32 i = 0; i < NumProperties; ++i)
		{
			TSharedRef<IPropertyHandle> RecordedPropertyTemp = RecordedPropertiesArrayHandle->GetElement(i);
			if (RecordedPropertyTemp->IsValidHandle())
			{
				TSharedPtr<IPropertyHandle> EnabledRecordedPropertyTemp = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, bEnabled));
				if (EnabledRecordedPropertyTemp->IsValidHandle())
				{
					EnabledRecordedPropertyTemp->SetValue( InNewState == ECheckBoxState::Checked );
				}
			}
		}

		TSharedPtr<IPropertyHandleArray> RecordedComponentsArrayHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, Children))->AsArray();

		uint32 NumCompProperties;
		RecordedComponentsArrayHandle->GetNumElements(NumCompProperties);

		for(uint32 i = 0; i < NumCompProperties; ++i)
		{
			TSharedPtr<IPropertyHandle> RecordedCompTemp = RecordedComponentsArrayHandle->GetElement(i);
			if (RecordedCompTemp->IsValidHandle())
			{
				OnCheckStateChanged( InNewState, RecordedCompTemp.ToSharedRef());
			}
		}
	}

	ECheckBoxState OnGetCheckState( TSharedRef<IPropertyHandle> PropertyHandle ) const 
	{
		bool SetFirst = false;
		bool FinalCheckedValue = false;

		TSharedPtr<IPropertyHandleArray> RecordedPropertiesArrayHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, Properties))->AsArray();

		uint32 NumProperties;
		RecordedPropertiesArrayHandle->GetNumElements(NumProperties);

		for(uint32 i = 0; i < NumProperties; ++i)
		{
			TSharedRef<IPropertyHandle> RecordedPropertyTemp = RecordedPropertiesArrayHandle->GetElement(i);
			if (RecordedPropertyTemp->IsValidHandle())
			{

				TSharedPtr<IPropertyHandle> EnabledRecordedPropertyTemp = RecordedPropertyTemp->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActorRecordedProperty, bEnabled));
				if (EnabledRecordedPropertyTemp->IsValidHandle())
				{
					bool EnabledValue;
					if ( EnabledRecordedPropertyTemp->GetValue(EnabledValue) == FPropertyAccess::Success )
					{
						if (!SetFirst)
						{
							FinalCheckedValue = EnabledValue;
							SetFirst = true;
						}
						else if ( EnabledValue != FinalCheckedValue )
						{
							return ECheckBoxState::Undetermined;
						}
					}
				}
			}
		}

		TSharedPtr<IPropertyHandleArray> RecordedComponentsArrayHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActorRecorderPropertyMap, Children))->AsArray();

		uint32 NumCompProperties;
		RecordedComponentsArrayHandle->GetNumElements(NumCompProperties);

		for(uint32 i = 0; i < NumCompProperties; ++i)
		{
			TSharedPtr<IPropertyHandle> RecordedCompTemp = RecordedComponentsArrayHandle->GetElement(i);
			if (RecordedCompTemp->IsValidHandle())
			{
				ECheckBoxState CompEnabledState = OnGetCheckState( RecordedCompTemp.ToSharedRef() );
				if (CompEnabledState == ECheckBoxState::Undetermined)
				{
					return ECheckBoxState::Undetermined;
				}
				else 
				{
					bool IsCompChecked = CompEnabledState == ECheckBoxState::Checked ? true : false;

					if ( !SetFirst )
					{
						FinalCheckedValue = IsCompChecked;	
						SetFirst = true;
					}
					else if ( IsCompChecked != FinalCheckedValue )
					{
						return ECheckBoxState::Undetermined;
					}
				}
			}
		}

		return FinalCheckedValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

};
const FString FRecorderPropertyMapCustomization::PropertyPathDelimiter = FString(TEXT("."));

class FRecorderSourceObjectCustomization : public IDetailCustomization
{
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		FText NewTitle = ComputeTitle(DetailBuilder.GetDetailsView());
		if (!NewTitle.IsEmpty())
		{
			// Edit the category and add *all* properties for the object to it
			IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("CustomCategory", NewTitle);

			UClass* BaseClass = DetailBuilder.GetBaseClass();
			while (BaseClass)
			{
				for (FProperty* Property : TFieldRange<FProperty>(BaseClass, EFieldIteratorFlags::ExcludeSuper))
				{
					CategoryBuilder.AddProperty(Property->GetFName(), BaseClass);
				}

				BaseClass = BaseClass->GetSuperClass();
			}
		}
	}

	FText ComputeTitle(const IDetailsView* DetailsView) const
	{
		// Compute the title for all the sources that this details panel is editing
		static const FName CategoryName = "Category";

		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailsView->GetSelectedObjects();
		UObject* FirstObject = SelectedObjects.Num() > 0 ? SelectedObjects[0].Get() : nullptr;

		if (FirstObject)
		{
			if (SelectedObjects.Num() == 1)
			{
				UTakeRecorderSource* Source = Cast<UTakeRecorderSource>(FirstObject);
				return Source ? Source->GetDisplayText() : FText::FromString(FirstObject->GetName());
			}
			else if (SelectedObjects.Num() > 1)
			{
				const FString& Category = FirstObject->GetClass()->GetMetaData(CategoryName);
				return FText::Format(LOCTEXT("CategoryFormatString", "{0} ({1})"), FText::FromString(Category), SelectedObjects.Num());
			}
		}

		return FText();
	}
};


void SLevelSequenceTakeEditor::AddDetails(const TPair<const UClass*, TArray<UObject*> >& Pair, TArray<FObjectKey>& PreviousClasses)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bShowScrollBar = false;

	PreviousClasses.Remove(Pair.Key);

	TSharedPtr<IDetailsView> ExistingDetails = ClassToDetailsView.FindRef(Pair.Key);
	if (ExistingDetails.IsValid())
	{
		ExistingDetails->SetObjects(Pair.Value);
	}
	else
	{
		TSharedRef<IDetailsView> Details = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

		// Register the custom property layout for all object types to rename the category to the object type
		// @note: this is registered as a base for all objects on the details panel that
		// overrides the category name for *all* properties in the object. This makes property categories irrelevant for recorder sources,
		// And may also interfere with any other detail customizations for sources as a whole if any are added in future (property type customizations will still work fine)
		// We may want to change this in future but it seems like the neatest way to make top level categories have helpful names.
		Details->RegisterInstancedCustomPropertyLayout(UTakeRecorderSource::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda(&MakeShared<FRecorderSourceObjectCustomization>));

		Details->RegisterInstancedCustomPropertyTypeLayout(FName(TEXT("ActorRecorderPropertyMap")), FOnGetPropertyTypeCustomizationInstance::CreateLambda(&MakeShared<FRecorderPropertyMapCustomization >));
		Details->RegisterInstancedCustomPropertyTypeLayout(FName(TEXT("ActorRecordedProperty")), FOnGetPropertyTypeCustomizationInstance::CreateLambda(&MakeShared<FRecordedPropertyCustomization >));
		Details->SetObjects(Pair.Value);

		Details->SetEnabled(LevelSequenceAttribute.IsSet() && LevelSequenceAttribute.Get()->FindMetaData<UTakeMetaData>() ? !LevelSequenceAttribute.Get()->FindMetaData<UTakeMetaData>()->Recorded() : true);

		DetailsBox->AddSlot()
			[
				Details
			];

		ClassToDetailsView.Add(Pair.Key, Details);
	}
}

void SLevelSequenceTakeEditor::UpdateDetails()
{
	TMap<const UClass*, TArray<UObject*>> ExternalClassToSources;

	for (TWeakObjectPtr<> WeakExternalObj : ExternalSettingsObjects)
	{
		UObject* Object = WeakExternalObj.Get();
		if (Object)
		{
			ExternalClassToSources.FindOrAdd(Object->GetClass()).Add(Object);
		}
	}

	TMap<const UClass*, TArray<UObject*>> ClassToSources;

	TArray<UTakeRecorderSource*> SelectedSources;
	SourcesWidget->GetSelectedSources(SelectedSources);

	// Create 1 details panel per source class type
	for (UTakeRecorderSource* Source : SelectedSources)
	{
		ClassToSources.FindOrAdd(Source->GetClass()).Add(Source);

		// Each source can provide an array of additional settings objects. This allows sources to dynamically
		// spawn settings that aren't part of the base class but still have them presented in the UI in a way that
		// gets hidden automatically.
		for (UObject* SettingsObject : Source->GetAdditionalSettingsObjects())
		{
			ClassToSources.FindOrAdd(SettingsObject->GetClass()).Add(SettingsObject);
		}
	}

	TArray<FObjectKey> PreviousClasses;
	ClassToDetailsView.GenerateKeyArray(PreviousClasses);

	// Clear all existing details views if there are external settings objects, so that they can be displayed last
	if (ExternalSettingsObjects.Num())
	{
		for (FObjectKey StaleClass : PreviousClasses)
		{
			TSharedPtr<IDetailsView> Details = ClassToDetailsView.FindRef(StaleClass);
			DetailsBox->RemoveSlot(Details.ToSharedRef());
			ClassToDetailsView.Remove(StaleClass);
		}

		PreviousClasses.Empty();
	}

	for (auto& Pair : ClassToSources)
	{
		AddDetails(Pair, PreviousClasses);
	}

	for (auto& Pair : ExternalClassToSources)
	{
		AddDetails(Pair, PreviousClasses);
	}
	
	for (FObjectKey StaleClass : PreviousClasses)
	{
		TSharedPtr<IDetailsView> Details = ClassToDetailsView.FindRef(StaleClass);
		DetailsBox->RemoveSlot(Details.ToSharedRef());
		ClassToDetailsView.Remove(StaleClass);
	}
}


#undef LOCTEXT_NAMESPACE