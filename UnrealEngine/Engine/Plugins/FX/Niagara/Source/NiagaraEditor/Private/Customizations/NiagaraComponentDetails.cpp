// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponentDetails.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterface.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"
#include "ViewModels/NiagaraParameterViewModel.h"
#include "NiagaraEditorUtilities.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraTypes.h"
#include "ScopedTransaction.h"
#include "NiagaraEditorModule.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeInput.h"
#include "GameDelegates.h"
#include "NiagaraEditorStyle.h"
#include "IDetailChildrenBuilder.h"
#include "NiagaraConstants.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SNiagaraDebugger.h"
#include "Widgets/SNiagaraParameterName.h"
#include "ViewModels/HierarchyEditor/NiagaraUserParametersHierarchyViewModel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "SNiagaraSystemUserParameters.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "NiagaraComponentDetails"

static FNiagaraVariant GetParameterValueFromSystem(const FNiagaraVariableBase& Parameter, const UNiagaraSystem* System)
{
	const FNiagaraUserRedirectionParameterStore& UserParameterStore = System->GetExposedParameters();
	if (Parameter.IsDataInterface())
	{
		int32 Index = UserParameterStore.IndexOf(Parameter);
		if (Index != INDEX_NONE)
		{
			return FNiagaraVariant(UserParameterStore.GetDataInterfaces()[Index]);
		}
	}
	
	if (Parameter.IsUObject())
	{
		int32 Index = UserParameterStore.IndexOf(Parameter);
		if (Index != INDEX_NONE)
		{
			return FNiagaraVariant(UserParameterStore.GetUObjects()[Index]);
		}
	}

	if (Parameter.GetType() == FNiagaraTypeDefinition::GetPositionDef())
	{
		const FVector* Value = UserParameterStore.GetPositionParameterValue(Parameter.GetName());
		return Value == nullptr ? FNiagaraVariant() : FNiagaraVariant(Value, sizeof(FVector));
	}

	TArray<uint8> DataValue;
	DataValue.AddUninitialized(Parameter.GetSizeInBytes());
	if (UserParameterStore.CopyParameterData(Parameter, DataValue.GetData()))
	{
		return FNiagaraVariant(DataValue);
	}
	return FNiagaraVariant();
}

static FNiagaraVariant GetCurrentParameterValue(const FNiagaraVariableBase& Parameter, const UObject* Owner)
{
	if(const UNiagaraComponent* Component = Cast<UNiagaraComponent>(Owner))
	{
		FNiagaraVariant CurrentValue = Component->GetCurrentParameterValue(Parameter);
		if (CurrentValue.IsValid())
		{
			return CurrentValue;
		}
		
		return GetParameterValueFromSystem(Parameter, Component->GetAsset());
	}
	else if(const UNiagaraSystem* System = Cast<UNiagaraSystem>(Owner))
	{
		return GetParameterValueFromSystem(Parameter, System);
	}

	ensure(false);
	return FNiagaraVariant();
}

// Proxy class to allow us to override values on the component that are not yet overridden.
class FNiagaraParameterProxy
{
public:
	FNiagaraParameterProxy(TWeakObjectPtr<UObject> InOwner, const FNiagaraVariableBase& InKey, const FNiagaraVariant& InValue, const FSimpleDelegate& InOnRebuild, TArray<TSharedPtr<IPropertyHandle>> InPropertyHandles)
	{
		bResettingToDefault = false;
		Owner = InOwner;
		ParameterKey = InKey;
		ParameterValue = InValue;
		OnRebuild = InOnRebuild;
		PropertyHandles = InPropertyHandles;
	}

	FReply OnResetToDefaultClicked()
	{
		OnResetToDefault();
		return FReply::Handled();
	}

	void OnResetToDefault()
	{
		if(Owner->IsA<UNiagaraComponent>())
		{
			TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();
			if (RawComponent != nullptr)
			{
				FScopedTransaction ScopedTransaction(NSLOCTEXT("UnrealEd", "PropertyWindowResetToDefault", "Reset to Default"));
				RawComponent->Modify();

				bResettingToDefault = true;

				for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
				{
					PropertyHandle->NotifyPreChange();
				}

				RawComponent->RemoveParameterOverride(ParameterKey);

				for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
				{
					PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				}

				OnRebuild.ExecuteIfBound();
				bResettingToDefault = false;
			}
		}
	}

	EVisibility GetResetToDefaultVisibility() const
	{
		TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();
		if (RawComponent.IsValid())
		{
			return RawComponent->HasParameterOverride(ParameterKey) ? EVisibility::Visible : EVisibility::Hidden;
		}
		return EVisibility::Hidden;
	}

	FNiagaraVariant FindExistingOverride() const
	{
		TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();
		if (RawComponent.IsValid())
		{
			return RawComponent->FindParameterOverride(ParameterKey);
		}
		return FNiagaraVariant();
	}

	void OnParameterPreChange()
	{
		if (Owner->IsA<UNiagaraComponent>())
		{
			TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();
			if(RawComponent.IsValid())
			{
				RawComponent->Modify();

				for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
				{
					PropertyHandle->NotifyPreChange();
				}
			}
		}
		else if(Owner->IsA<UNiagaraSystem>())
		{
			TWeakObjectPtr<UNiagaraSystem> System = GetSystem();
			if(System.IsValid())
			{
				System->Modify();
			}
		}
	}

	void OnParameterChanged(const FPropertyChangedEvent& PropertyChangedEvent)
	{
		if(Owner->IsA<UNiagaraComponent>())
		{
			if (bResettingToDefault)
			{
				return;
			}

			TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();
			if(RawComponent.IsValid())
			{
				RawComponent->SetParameterOverride(ParameterKey, ParameterValue);

				for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
				{
					PropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}
			}
		}
		else if(Owner->IsA<UNiagaraSystem>())
		{
			TWeakObjectPtr<UNiagaraSystem> System = GetSystem();
			if(System.IsValid())
			{
				if(ParameterValue.GetMode() == ENiagaraVariantMode::Bytes)
				{
					// we have to special case the position definition because, while SetParameterData does handle Position variables, it expects 12 Bytes of data
					if(ParameterKey.GetType() == FNiagaraTypeDefinition::GetPositionDef())
					{
						FVector Value = *reinterpret_cast<const FVector*>(ParameterValue.GetBytes());
						System->GetExposedParameters().SetPositionParameterValue(Value, ParameterKey.GetName());
					}
					else
					{
						System->GetExposedParameters().SetParameterData(ParameterValue.GetBytes(), ParameterKey);
					}
				}
				else if(ParameterValue.GetMode() == ENiagaraVariantMode::DataInterface)
				{
					// we are operating on the DI objects in the parameter store directly, so no need to call SetDataInterface
					System->GetExposedParameters().OnInterfaceChange();
				}
				else if(ParameterValue.GetMode() == ENiagaraVariantMode::Object)
				{
					System->GetExposedParameters().SetUObject(ParameterValue.GetUObject(), ParameterKey);
				}

				for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
				{
					PropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}			
			}
		}
	}

	void OnAssetSelectedFromPicker(const FAssetData& InAssetData)
	{
		if(Owner->IsA<UNiagaraComponent>())
		{
			TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();

			if(RawComponent.IsValid())
			{
				UObject* Asset = InAssetData.GetAsset();
				if (Asset == nullptr || Asset->GetClass()->IsChildOf(ParameterKey.GetType().GetClass()))
				{
					FScopedTransaction ScopedTransaction(LOCTEXT("ChangeAsset", "Change asset"));
					RawComponent->Modify();
					RawComponent->SetParameterOverride(ParameterKey, FNiagaraVariant(Asset));

					for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
					{
						PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
					}
				}
			}
		}
		else if(Owner->IsA<UNiagaraSystem>())
		{
			TWeakObjectPtr<UNiagaraSystem> System = GetSystem();

			if(System.IsValid())
			{
				UObject* Asset = InAssetData.GetAsset();
				if (Asset == nullptr || Asset->GetClass()->IsChildOf(ParameterKey.GetType().GetClass()))
				{
					FScopedTransaction ScopedTransaction(LOCTEXT("ChangeAsset", "Change asset"));
					System->Modify();
					System->GetExposedParameters().SetUObject(Asset, ParameterKey);

					for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
					{
						PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
					}
				}
			}
		}
	}

	FString GetCurrentAssetPath() const
	{
		UObject* CurrentObject = nullptr;

		if(Owner->IsA<UNiagaraComponent>())
		{
			TWeakObjectPtr<UNiagaraComponent> RawComponent = GetComponent();

			if(RawComponent.IsValid())
			{
				FNiagaraVariant CurrentValue = FindExistingOverride();
				if (CurrentValue.IsValid())
				{
					CurrentObject = CurrentValue.GetUObject();
				}
				else
				{
					// fetch from asset
					UNiagaraSystem* System = RawComponent->GetAsset();
					if (System != nullptr)
					{
						FNiagaraUserRedirectionParameterStore& AssetParamStore = System->GetExposedParameters();
						CurrentObject = AssetParamStore.GetUObject(ParameterKey);
					}
				}
			}
		}
		else if(Owner->IsA<UNiagaraSystem>())
		{			
			TWeakObjectPtr<UNiagaraSystem> System = GetSystem();

			if(System.IsValid())
			{
				FNiagaraUserRedirectionParameterStore& AssetParamStore = System->GetExposedParameters();
				CurrentObject = AssetParamStore.GetUObject(ParameterKey);			
			}
		}

		return CurrentObject != nullptr ? CurrentObject->GetPathName() : FString();
	}
	
	const FNiagaraVariableBase& Key() const { return ParameterKey; }
	FNiagaraVariant& Value() { return ParameterValue; }

	TWeakObjectPtr<UNiagaraComponent> GetComponent() const { return Cast<UNiagaraComponent>(Owner); }
	TWeakObjectPtr<UNiagaraSystem> GetSystem() const { return Cast<UNiagaraSystem>(Owner); }

private:
	TWeakObjectPtr<UObject> Owner;
	TArray<TSharedPtr<IPropertyHandle>> PropertyHandles;
	FNiagaraVariableBase ParameterKey;
	FNiagaraVariant ParameterValue;
	FSimpleDelegate OnRebuild;
	bool bResettingToDefault;
};
#undef LOCTEXT_NAMESPACE

#define LOCTEXT_NAMESPACE "NiagaraUserParameterDetails"

void FNiagaraUserParameterNodeBuilder::GenerateRowForUserParameter(IDetailChildrenBuilder& ChildrenBuilder, FNiagaraVariable UserParameter, UObject* Owner)
{
	FNiagaraVariable ChoppedUserParameter(UserParameter);

	if(UserParameter.IsInNameSpace(FNiagaraConstants::UserNamespaceString))
	{
		ChoppedUserParameter.SetName(FName(UserParameter.GetName().ToString().RightChop(5)));
	}
	
	FNiagaraVariant ParameterValue = GetCurrentParameterValue(ChoppedUserParameter, Owner);
	if (!ParameterValue.IsValid())
	{
		return;
	}
		
	if (UserParameter.IsDataInterface())
	{
		if(Owner->IsA<UNiagaraComponent>())
		{
			ParameterValue = FNiagaraVariant(DuplicateObject(ParameterValue.GetDataInterface(), Owner));
		}
		else if(Owner->IsA<UNiagaraSystem>())
		{
			ParameterValue = FNiagaraVariant(ParameterValue.GetDataInterface());
		}
	}

	TSharedPtr<FNiagaraParameterProxy> ParameterProxy = ParameterProxies.Add_GetRef(MakeShared<FNiagaraParameterProxy>(Owner, UserParameter, ParameterValue, OnRebuildChildren, OverridePropertyHandles));

	IDetailPropertyRow* Row = nullptr;

	TSharedPtr<SWidget> CustomNameWidget = GenerateCustomNameWidget(UserParameter);
 	TSharedPtr<SWidget> CustomValueWidget;
	
	if (UserParameter.IsDataInterface())
	{
		// if no changes are made, then it'll just be the same as the asset
		TArray<UObject*> Objects { ParameterProxy->Value().GetDataInterface() };
	
		FAddPropertyParams Params = FAddPropertyParams()
			.UniqueId(ChoppedUserParameter.GetName())
			.AllowChildren(true)
			.CreateCategoryNodes(false);
	
		Row = ChildrenBuilder.AddExternalObjectProperty(Objects, NAME_None, Params);

		CustomValueWidget =
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
			.Text(FText::FromString(FName::NameToDisplayString(UserParameter.GetType().GetClass()->GetName(), false)));
	}
	else if(UserParameter.IsUObject())
	{
		TArray<UObject*> Objects { ParameterProxy->Value().GetUObject() };

		FAddPropertyParams Params = FAddPropertyParams()
			.UniqueId(ChoppedUserParameter.GetName())
			.AllowChildren(false) // Don't show the material's properties
			.CreateCategoryNodes(false);

		Row = ChildrenBuilder.AddExternalObjectProperty(Objects, NAME_None, Params);
		
		CustomValueWidget = SNew(SObjectPropertyEntryBox)
			.ObjectPath(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::GetCurrentAssetPath)
			.AllowedClass(UserParameter.GetType().GetClass())
			.OnObjectChanged(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnAssetSelectedFromPicker)
			.AllowClear(false)
			.DisplayUseSelected(true)
			.DisplayBrowse(true)
			.DisplayThumbnail(true)
			.NewAssetFactories(TArray<UFactory*>());
	}
	else
	{
		FNiagaraTypeDefinition Type = UserParameter.GetType();
		UStruct* Struct = Type.GetStruct();
			
		if (Type == FNiagaraTypeDefinition::GetPositionDef())
		{
			static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
			static UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector"));
			Struct = VectorStruct;
		}

		// the details panel uses byte properties with enums assigned to display enums
		if (Type.IsEnum())
		{
			static UPackage* NiagaraEditorPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/NiagaraEditor"));
			static UScriptStruct* ByteStruct = FindObjectChecked<UScriptStruct>(NiagaraEditorPkg, TEXT("NiagaraEnumToByteHelper"));
			Struct = ByteStruct;
		}
			
		TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Struct, ParameterProxy->Value().GetBytes()));

		FAddPropertyParams Params = FAddPropertyParams()
			.UniqueId(ChoppedUserParameter.GetName());

		Row = ChildrenBuilder.AddExternalStructureProperty(StructOnScope.ToSharedRef(), NAME_None, Params);

		// we set the enum of the contained byte property to the enum carried with the type
		if(Type.IsEnum())
		{
			FProperty* Property = Row->GetPropertyHandle()->GetProperty();
			TSharedPtr<IPropertyHandle> ValuePropertyHandle = Row->GetPropertyHandle()->GetChildHandle(0);
			FProperty* ValueProperty = ValuePropertyHandle->GetProperty();
			if(ValueProperty->IsA<FByteProperty>())
			{
				if(FByteProperty* ByteProperty = CastField<FByteProperty>(ValueProperty))
				{
					ByteProperty->Enum = Type.GetEnum();
				}
			}
		}
	
		ParameterToDisplayStruct.Add(UserParameter, TWeakPtr<FStructOnScope>(StructOnScope));
	}

	TSharedPtr<SWidget> DefaultNameWidget;
	TSharedPtr<SWidget> DefaultValueWidget;
	
	Row->DisplayName(FText::FromName(ChoppedUserParameter.GetName()));
	
	FDetailWidgetRow& CustomWidget = Row->CustomWidget(true);
	
	AddCustomMenuActionsForParameter(CustomWidget, UserParameter);
	
	UNiagaraSystem* System = nullptr;
	if(ParameterProxy->GetSystem().IsValid())
	{
		System = ParameterProxy->GetSystem().Get();
	}
	else if(ParameterProxy->GetComponent().IsValid())
	{
		System = ParameterProxy->GetComponent()->GetAsset();
	}

	if(System)
	{
		UNiagaraScriptVariable* ScriptVariable = FNiagaraEditorUtilities::GetScriptVariableForUserParameter(UserParameter, *System);
			
		Row->GetDefaultWidgets(DefaultNameWidget, DefaultValueWidget, CustomWidget);

		Row->GetPropertyHandle()->MarkResetToDefaultCustomized(true);
		
		Row->GetPropertyHandle()->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnParameterPreChange));
		Row->GetPropertyHandle()->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateSP(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnParameterPreChange));
		Row->GetPropertyHandle()->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnParameterChanged));
		Row->GetPropertyHandle()->SetOnChildPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnParameterChanged));
		Row->GetPropertyHandle()->SetOnPropertyResetToDefault(FSimpleDelegate::CreateSP(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnResetToDefault));

		TSharedPtr<SWidget> NameWidget = SNew(SBox)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(FText::FromName(ChoppedUserParameter.GetName()))
			];
		
		if (CustomNameWidget.IsValid())
		{
			NameWidget = CustomNameWidget;
		}

		// regardless of which widget we use for the name, we want to use the script variable's description as tooltip
		TAttribute<FText> Tooltip = FText::GetEmpty();
		if(ScriptVariable)
		{
			Tooltip = TAttribute<FText>::CreateLambda([ScriptVariable]
			{
				return ScriptVariable->Metadata.Description;
			});
		}
		NameWidget->SetToolTipText(Tooltip);
		
		TSharedPtr<SWidget> ValueWidget = DefaultValueWidget;
		if (CustomValueWidget.IsValid())
		{
			ValueWidget = CustomValueWidget;
		}
		
		CustomWidget
		.FilterString(FText::FromName(ChoppedUserParameter.GetName()))
		.NameContent()
		.HAlign(HAlign_Left)
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(4.0f)
			[
				// Add in the parameter editor factoried above.
				ValueWidget.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				// Add in the "reset to default" buttons
				SNew(SButton)
				.OnClicked(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::OnResetToDefaultClicked)
				.Visibility(ParameterProxy.ToSharedRef(), &FNiagaraParameterProxy::GetResetToDefaultVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
	}
}

TSharedRef<IDetailCustomization> FNiagaraSystemUserParameterDetails::MakeInstance()
{
	return MakeShared<FNiagaraSystemUserParameterDetails>();
}

void FNiagaraSystemUserParameterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
	
	TArray<FName> Categories;
	DetailBuilder.GetCategoryNames(Categories);

	for(FName& CategoryName : Categories)
	{
		DetailBuilder.HideCategory(CategoryName);
	}
	
	static const FName ParamCategoryName = TEXT("NiagaraSystem_UserParameters");

	if(CustomizedObjects.Num() == 1 && CustomizedObjects[0]->IsA<UNiagaraSystem>())
	{
		System = Cast<UNiagaraSystem>(CustomizedObjects[0]);
		IDetailCategoryBuilder& InputParamCategory = DetailBuilder.EditCategory(ParamCategoryName, LOCTEXT("ParamCategoryName", "User Parameters"), ECategoryPriority::Important);
		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::GetExistingViewModelForObject(System.Get());
		ensure(SystemViewModel.IsValid());
		TSharedRef<FNiagaraSystemUserParameterBuilder> SystemUserParameterBuilder = MakeShared<FNiagaraSystemUserParameterBuilder>(SystemViewModel, ParamCategoryName);
		InputParamCategory.AddCustomBuilder(SystemUserParameterBuilder);
		TSharedRef<SWidget> AddParameterButton = SystemUserParameterBuilder->GetAddParameterButton();
		InputParamCategory.HeaderContent(AddParameterButton);
	}
}

/** The category builder will display all sub-categories & parameters contained within a given category in the details panel. */
class FNiagaraUserParameterCategoryBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FNiagaraUserParameterCategoryBuilder>
{
public:
	FNiagaraUserParameterCategoryBuilder(UObject* InOwner, const UNiagaraHierarchyCategory* InNiagaraHierarchyCategory, FName InCustomBuilderRowName, FNiagaraUserParameterNodeBuilder& InNodeBuilder) : NodeBuilder(InNodeBuilder) 
	{
		HierarchyCategory = InNiagaraHierarchyCategory;
		CustomBuilderRowName = InCustomBuilderRowName;

		Owner = InOwner;
	}
	
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
	{
		NodeRow.NameContent()
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
			.Text_UObject(HierarchyCategory.Get(), &UNiagaraHierarchyCategory::GetCategoryAsText)
			.ToolTipText_UObject(HierarchyCategory.Get(), &UNiagaraHierarchyCategory::GetTooltip)
		];

		NodeRow.FilterString(FText::FromName(CustomBuilderRowName));
	}
	
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }

	virtual FName GetName() const  override
	{
		return CustomBuilderRowName;
	}
	
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		TArray<const UNiagaraHierarchyUserParameter*> ContainedParameters;
		TArray<const UNiagaraHierarchyCategory*> ContainedCategories;

		for(const UNiagaraHierarchyItemBase* Child : HierarchyCategory->GetChildren())
		{
			if(const UNiagaraHierarchyUserParameter* UserParameter = Cast<UNiagaraHierarchyUserParameter>(Child))
			{
				ContainedParameters.Add(UserParameter);
			}
		}

		for(UNiagaraHierarchyItemBase* Child : HierarchyCategory->GetChildren())
		{
			if(UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(Child))
			{
				ContainedCategories.Add(Category);
			}
		}

		for(const UNiagaraHierarchyCategory* Category : ContainedCategories)
		{
			if(Category->DoesOneChildExist<UNiagaraHierarchyUserParameter>(true))
			{
				ChildrenBuilder.AddCustomBuilder(MakeShared<FNiagaraUserParameterCategoryBuilder>(
					Owner.Get(),Category, Category->GetCategoryName(), NodeBuilder));
			}
		}
		
		for(const UNiagaraHierarchyUserParameter* HierarchyUserParameter : ContainedParameters)
		{
			NodeBuilder.GenerateRowForUserParameter(ChildrenBuilder, HierarchyUserParameter->GetUserParameter(), Owner.Get());
		}
	}

	TWeakObjectPtr<UNiagaraComponent> GetComponent() const { return Cast<UNiagaraComponent>(Owner); }
	TWeakObjectPtr<UNiagaraSystem> GetSystem() const { return Cast<UNiagaraSystem>(Owner); }

private:
	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<const UNiagaraHierarchyCategory> HierarchyCategory;
	FName CustomBuilderRowName;
	FNiagaraUserParameterNodeBuilder& NodeBuilder;
};

void FNiagaraUserParameterNodeBuilder::GenerateUserParameterRows(IDetailChildrenBuilder& ChildrenBuilder, UNiagaraHierarchyRoot& UserParameterHierarchyRoot, UObject* Owner)
{
	UNiagaraSystem* SystemAsset = nullptr;

	if(UNiagaraSystem* System = Cast<UNiagaraSystem>(Owner))
	{
		SystemAsset = System;
	}
	else if(UNiagaraComponent* Component = Cast<UNiagaraComponent>(Owner))
	{
		SystemAsset = Component->GetAsset();
	}

	if(SystemAsset == nullptr)
	{
		return;
	}

	TArray<const UNiagaraHierarchySection*> OrderedSections;
	TMap<const UNiagaraHierarchySection*, TArray<const UNiagaraHierarchyCategory*>> SectionCategoryMap;

	UNiagaraHierarchyRoot* Root = &UserParameterHierarchyRoot;
	for(const UNiagaraHierarchyItemBase* Child : Root->GetChildren())
	{
		if(const UNiagaraHierarchyCategory* HierarchyCategory = Cast<UNiagaraHierarchyCategory>(Child))
		{
			if(HierarchyCategory->GetSection() != nullptr)
			{
				SectionCategoryMap.FindOrAdd(HierarchyCategory->GetSection()).Add(HierarchyCategory);	
			}
		}
	}

	// we create a custom row for user param sections here, but only if we have at least one section specified
	if(SectionCategoryMap.Num() > 0)
	{
		TSharedRef<SHorizontalBox> SectionsBox = SNew(SHorizontalBox);

		// maps don't guarantee order, so we iterate over the original section data instead
		for(auto& Section : Root->GetSectionData())
		{
			// if the map doesn't contain at the entry, it means there are 0 categories for that section, so we skip it
			if(!SectionCategoryMap.Contains(Section))
			{
				continue;
			}
			
			TArray<const UNiagaraHierarchyCategory*>& HierarchyCategories = SectionCategoryMap[Section];
			
			if(HierarchyCategories.Num() > 0)
			{
				// we only want to add a section if any of its categories contain at least one user parameter
				bool bDoesUserParamExist = false;
				for(const UNiagaraHierarchyCategory* Category : HierarchyCategories)
				{
					bDoesUserParamExist |= Category->DoesOneChildExist<UNiagaraHierarchyUserParameter>(true);

					if(bDoesUserParamExist)
					{
						break;
					}
				}

				if(bDoesUserParamExist)
				{
					SectionsBox->AddSlot()
					.AutoWidth()
					.Padding(FMargin(2.f))
					[
						SNew(SBox)
						.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
						[
							SNew(SCheckBox)
							.Style(FAppStyle::Get(), "DetailsView.SectionButton")
							.OnCheckStateChanged_Lambda([this, Section](ECheckBoxState NewState)
							{
								ActiveSection = Section;
								OnRebuildChildren.ExecuteIfBound();
							})
							.IsChecked_Lambda([=]()
							{
								return ActiveSection == Section ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							})
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "SmallText")
								.Text_UObject(Section, &UNiagaraHierarchySection::GetSectionNameAsText)
								.ToolTipText_UObject(Section, &UNiagaraHierarchySection::GetTooltip)
							]
						]
					];
				}
			}
		}

		// if we have at least one custom section, we add a default "All" section
		if(SectionsBox->GetChildren()->Num() > 0)
		{
			SectionsBox->AddSlot()
			.AutoWidth()
			.Padding(FMargin(2.f))
			[
				SNew(SBox)
				.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "DetailsView.SectionButton")
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
					{
						ActiveSection = nullptr;
						OnRebuildChildren.ExecuteIfBound();
					})
					.IsChecked_Lambda([=]()
					{
						return ActiveSection == nullptr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallText")
						.Text(FText::FromString("All"))
					]
				]
			];
			
			FDetailWidgetRow& Row = ChildrenBuilder.AddCustomRow(FText::FromString("Parameter Sections"));
			Row.WholeRowContent()
			[
				SectionsBox
			];
		}
	}
	
	// we add the categories now if the active section is set to all (nullptr) or if the section it belongs to is active
	for(const UNiagaraHierarchyItemBase* Child : Root->GetChildren())
	{		
		if(const UNiagaraHierarchyCategory* HierarchyCategory = Cast<UNiagaraHierarchyCategory>(Child))
		{
			if((ActiveSection == nullptr || HierarchyCategory->GetSection() == ActiveSection) && HierarchyCategory->DoesOneChildExist<UNiagaraHierarchyUserParameter>(true))
			{
				ChildrenBuilder.AddCustomBuilder(MakeShared<FNiagaraUserParameterCategoryBuilder>(
					Owner, HierarchyCategory, HierarchyCategory->GetCategoryName(), *this));
			
			}
		}
		else if(const UNiagaraHierarchyUserParameter* UserParameter = Cast<UNiagaraHierarchyUserParameter>(Child))
		{
			// we only want to display parameters within the root in the all section
			if(ActiveSection == nullptr)
			{
				GenerateRowForUserParameter(ChildrenBuilder, UserParameter->GetUserParameter(), Owner);
			}
		}
	}

	// at last we add all parameters that haven't been setup at all, if the active section is set to "All"
	if(ActiveSection == nullptr)
	{
		TArray<FNiagaraVariable> LeftoverUserParameters;
		SystemAsset->GetExposedParameters().GetUserParameters(LeftoverUserParameters);

		TArray<UNiagaraHierarchyUserParameter*> HierarchyUserParameters;
		Root->GetChildrenOfType<>(HierarchyUserParameters, true);

		TArray<FNiagaraVariable> RedirectedUserParameters;
		for(FNiagaraVariable& Parameter : LeftoverUserParameters)
		{
			SystemAsset->GetExposedParameters().RedirectUserVariable(Parameter);
		}
		
		for(UNiagaraHierarchyUserParameter* HierarchyUserParameter : HierarchyUserParameters)
		{
			if(LeftoverUserParameters.Contains(HierarchyUserParameter->GetUserParameter()))
			{
				LeftoverUserParameters.Remove(HierarchyUserParameter->GetUserParameter());
			}
		}

		for(FNiagaraVariable& LeftoverUserParameter : LeftoverUserParameters)
		{			
			GenerateRowForUserParameter(ChildrenBuilder, LeftoverUserParameter, Owner);
		}
	}
}

void FNiagaraComponentUserParametersNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	if(Component.IsValid())
	{
		if (bDelegatesInitialized == false)
		{
			Component->OnSynchronizedWithAssetParameters().AddSP(this, &FNiagaraComponentUserParametersNodeBuilder::Rebuild);
			Component->GetOverrideParameters().AddOnChangedHandler(
				FNiagaraParameterStore::FOnChanged::FDelegate::CreateSP(this, &FNiagaraComponentUserParametersNodeBuilder::ParameterValueChanged));

			RegisterRebuildOnHierarchyChanged();
			bDelegatesInitialized = true;
		}

		ParameterProxies.Reset();

		UNiagaraSystem* SystemAsset = Component->GetAsset();
		if (SystemAsset == nullptr)
		{
			return;
		}
			
		TArray<FNiagaraVariable> UserParameters;
		SystemAsset->GetExposedParameters().GetUserParameters(UserParameters);
			
		ParameterProxies.Reserve(UserParameters.Num());

		ParameterToDisplayStruct.Empty();

		GenerateUserParameterRows(ChildrenBuilder, *Cast<UNiagaraSystemEditorData>(SystemAsset->GetEditorData())->UserParameterHierarchy.Get(),Component.Get());
	}
}

void FNiagaraComponentUserParametersNodeBuilder::RegisterRebuildOnHierarchyChanged()
{
	if(Component.IsValid())
	{
		if(UNiagaraSystem* Asset = Component->GetAsset())
		{
			TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::GetExistingViewModelForObject(Asset);
			if(SystemViewModel.IsValid())
			{
				SystemViewModel->GetUserParametersHierarchyViewModel()->OnHierarchyChanged().RemoveAll(this);
				SystemViewModel->GetUserParametersHierarchyViewModel()->OnHierarchyChanged().Add(UNiagaraHierarchyViewModelBase::FOnHierarchyChanged::FDelegate::CreateSP(this, &FNiagaraComponentUserParametersNodeBuilder::Rebuild));
			}
		}
	}
}

void FNiagaraComponentUserParametersNodeBuilder::ParameterValueChanged()
{
	if (Component.IsValid())
	{
		const FNiagaraParameterStore& OverrideParameters = Component->GetOverrideParameters();
		TArray<FNiagaraVariable> UserParameters;
		OverrideParameters.GetParameters(UserParameters);
		for (const FNiagaraVariable& UserParameter : UserParameters)
		{
			if (UserParameter.IsUObject() == false)
			{
				TWeakPtr<FStructOnScope>* DisplayStructPtr = ParameterToDisplayStruct.Find(UserParameter);
				if (DisplayStructPtr != nullptr && DisplayStructPtr->IsValid())
				{
					TSharedPtr<FStructOnScope> DisplayStruct = DisplayStructPtr->Pin();
					if (UserParameter.GetType() == FNiagaraTypeDefinition::GetPositionDef())
					{
						const FVector* PositionValue = OverrideParameters.GetPositionParameterValue(UserParameter.GetName());
						if (ensureMsgf(PositionValue != nullptr, TEXT("Position user parameter %s was missing it's position data.  Path: %s"),
							*UserParameter.GetName().ToString(), *Component->GetPathName()))
						{
							FMemory::Memcpy(DisplayStruct->GetStructMemory(), PositionValue, DisplayStruct->GetStruct()->GetStructureSize());
						}
					}
					else
					{
						OverrideParameters.CopyParameterData(UserParameter, DisplayStruct->GetStructMemory());
					}
				}
			}
		}
	}
}

FNiagaraSystemUserParameterBuilder::FNiagaraSystemUserParameterBuilder(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, FName InCustomBuilderRowName)
{
	CustomBuilderRowName = InCustomBuilderRowName;
	System = &InSystemViewModel->GetSystem();
	SystemViewModel = InSystemViewModel;
	bDelegatesInitialized = false;
}

TSharedPtr<SWidget> FNiagaraSystemUserParameterBuilder::GenerateCustomNameWidget(FNiagaraVariable UserParameter)
{	
	TSharedRef<SNiagaraParameterNameTextBlock> ParameterName = SNew(SNiagaraParameterNameTextBlock)
		// if this is specified, we avoid the default behavior, which is "when clicked when the widget had keyboard focus already, enter editing mode"
		// we don't want to be able to rename by clicking, only by context menu
		.IsSelected_Lambda([]()
		{
			return false;
		})
		.ParameterText(FText::FromName(UserParameter.GetName()))
		.OnDragDetected(this, &FNiagaraSystemUserParameterBuilder::GenerateParameterDragDropOp, UserParameter)
		.OnTextCommitted_Lambda([this, UserParameter](const FText& InText, ETextCommit::Type Type)
		{
			RenameParameter(UserParameter, FName(InText.ToString()));
		});
	
	UserParamToWidgetMap.Add(UserParameter, ParameterName);
	
	return ParameterName;
}

void FNiagaraSystemUserParameterBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	check(System.IsValid());

	if (System.IsValid())
	{
		if(bDelegatesInitialized == false)
		{
			System->GetExposedParameters().OnStructureChanged().Add(FNiagaraParameterStore::FOnStructureChanged::FDelegate::CreateSP(this, &FNiagaraSystemUserParameterBuilder::Rebuild));

			SystemViewModel.Pin()->GetUserParametersHierarchyViewModel()->OnHierarchyChanged().RemoveAll(this);
			SystemViewModel.Pin()->GetUserParametersHierarchyViewModel()->OnHierarchyChanged().Add(UNiagaraHierarchyViewModelBase::FOnHierarchyChanged::FDelegate::CreateSP(this, &FNiagaraSystemUserParameterBuilder::Rebuild));

			TSharedPtr<FNiagaraUserParameterPanelViewModel> UserParameterPanelViewModel = SystemViewModel.Pin()->GetUserParameterPanelViewModel();
			if(ensure(UserParameterPanelViewModel.IsValid()))
			{
				UserParameterPanelViewModel->OnParameterAdded().BindSP(this, &FNiagaraSystemUserParameterBuilder::RequestRename);
			}
			
			bDelegatesInitialized = true;
		}
			
		ParameterProxies.Reset();
			
		TArray<FNiagaraVariable> UserParameters;
		System->GetExposedParameters().GetUserParameters(UserParameters);
		ParameterProxies.Reserve(UserParameters.Num());

		ParameterToDisplayStruct.Empty();

		UNiagaraHierarchyRoot* Root = Cast<UNiagaraSystemEditorData>(System->GetEditorData())->UserParameterHierarchy;
		GenerateUserParameterRows(ChildrenBuilder, *Root, System.Get());
	}
}

void FNiagaraSystemUserParameterBuilder::AddCustomMenuActionsForParameter(FDetailWidgetRow& WidgetRow, FNiagaraVariable UserParameter)
{
	FNiagaraUserParameterNodeBuilder::AddCustomMenuActionsForParameter(WidgetRow, UserParameter);
	
	FUIAction RenameAction(FExecuteAction::CreateSP(this, &FNiagaraSystemUserParameterBuilder::RequestRename, UserParameter));
	WidgetRow.AddCustomContextMenuAction(RenameAction, LOCTEXT("RenameParameterAction", "Rename"), LOCTEXT("RenameParameterActionTooltip", "Rename this user parameter"));

	FUIAction DeleteAction(FExecuteAction::CreateSP(this, &FNiagaraSystemUserParameterBuilder::DeleteParameter, UserParameter));
	WidgetRow.AddCustomContextMenuAction(DeleteAction, LOCTEXT("DeleteParameterAction", "Delete"), LOCTEXT("DeleteParameterActionTooltip", "Delete this user parameter"));
}

TSharedRef<SWidget> FNiagaraSystemUserParameterBuilder::GetAddParameterButton()
{
	if(!AddParameterButtonContainer.IsValid())
	{
		AddParameterButtonContainer = SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SAssignNew(AddParameterButton, SComboButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "RoundButton")
			.ForegroundColor(FAppStyle::GetSlateColor("DefaultForeground"))
			.ContentPadding(FMargin(2, 0))
			.OnGetMenuContent(this, &FNiagaraSystemUserParameterBuilder::GetAddParameterMenu)
			.OnComboBoxOpened_Lambda([this]
			{
				AddParameterButton->SetMenuContentWidgetToFocus(AddParameterMenu->GetSearchBox());
			})
			.HasDownArrow(false)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Plus"))
			]
		];
	}
	
	return AddParameterButtonContainer.ToSharedRef(); 
}

TSharedRef<SWidget> FNiagaraSystemUserParameterBuilder::GetAddParameterMenu()
{
	FNiagaraParameterPanelCategory UserCategory = GetDefault<UNiagaraEditorSettings>()->GetMetaDataForNamespaces({FNiagaraConstants::UserNamespace});
	AddParameterMenu = SNew(SNiagaraAddParameterFromPanelMenu)
		.Graphs(SystemViewModel.Pin()->GetParameterPanelViewModel()->GetEditableGraphsConst())
		.OnNewParameterRequested(this, &FNiagaraSystemUserParameterBuilder::AddParameter)
		.OnAllowMakeType(this, &FNiagaraSystemUserParameterBuilder::CanMakeNewParameterOfType)
		.NamespaceId(UserCategory.NamespaceMetaData.GetGuid())
		.ShowNamespaceCategory(false)
		.ShowGraphParameters(false)
		.AutoExpandMenu(false);

	return AddParameterMenu.ToSharedRef();
}

void FNiagaraSystemUserParameterBuilder::AddParameter(FNiagaraVariable NewParameter) const
{
	FNiagaraParameterPanelCategory UserCategory = GetDefault<UNiagaraEditorSettings>()->GetMetaDataForNamespaces({FNiagaraConstants::UserNamespace});
	FNiagaraEditorUtilities::AddParameter(NewParameter, SystemViewModel.Pin()->GetSystem().GetExposedParameters(), SystemViewModel.Pin()->GetSystem(), nullptr);
	SystemViewModel.Pin()->GetUserParameterPanelViewModel()->OnParameterAdded().Execute(NewParameter);
}

bool FNiagaraSystemUserParameterBuilder::CanMakeNewParameterOfType(const FNiagaraTypeDefinition& InType) const
{
	return SystemViewModel.Pin()->GetParameterPanelViewModel()->CanMakeNewParameterOfType(InType);
}

void FNiagaraSystemUserParameterBuilder::ParameterValueChanged()
{
	if (System.IsValid())
	{
		const FNiagaraParameterStore& ExposedParameters = System->GetExposedParameters();
		TArray<FNiagaraVariable> UserParameters;
		ExposedParameters.GetParameters(UserParameters);
		for (const FNiagaraVariable& UserParameter : UserParameters)
		{
			if (UserParameter.IsUObject() == false)
			{
				TWeakPtr<FStructOnScope>* DisplayStructPtr = ParameterToDisplayStruct.Find(UserParameter);
				if (DisplayStructPtr != nullptr && DisplayStructPtr->IsValid())
				{
					TSharedPtr<FStructOnScope> DisplayStruct = DisplayStructPtr->Pin();
					if (UserParameter.GetType() == FNiagaraTypeDefinition::GetPositionDef())
					{
						FMemory::Memcpy(DisplayStruct->GetStructMemory(), ExposedParameters.GetPositionParameterValue(UserParameter.GetName()), UserParameter.GetSizeInBytes());
					}
					else
					{
						ExposedParameters.CopyParameterData(UserParameter, DisplayStruct->GetStructMemory());
					}
				}
			}
		}
	}
}

void FNiagaraSystemUserParameterBuilder::DeleteParameter(FNiagaraVariable UserParameter) const
{
	FScopedTransaction Transaction(LOCTEXT("DeleteParameterTransaction", "Deleted user parameter"));
	System->Modify();
	
	if(System->GetExposedParameters().IndexOf(UserParameter) != INDEX_NONE)
	{
		System->GetExposedParameters().RemoveParameter(UserParameter);		
	}

	Cast<UNiagaraSystemEditorData>(System->GetEditorData())->UserParameterHierarchy->Modify();
	System->HandleVariableRemoved(UserParameter, true);
}

void FNiagaraSystemUserParameterBuilder::RequestRename(FNiagaraVariable UserParameter)
{
	SelectedParameter = UserParameter;

	// we add a timer for next frame, as requesting a rename on a parameter that has just been created will not preselect the entire text. Waiting a frame fixes that.
	UserParamToWidgetMap[UserParameter]->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this, UserParameter](double CurrentTime, float DeltaTime)
	{
		UserParamToWidgetMap[UserParameter]->EnterEditingMode();
		return EActiveTimerReturnType::Stop;
	}));
}

void FNiagaraSystemUserParameterBuilder::RenameParameter(FNiagaraVariable UserParameter, FName NewName)
{
	if(UserParameter.GetName() == NewName)
	{
		return;
	}
	
	if(System->GetExposedParameters().IndexOf(UserParameter) != INDEX_NONE)
	{		
		TArray<FNiagaraVariable> Parameters;
		System->GetExposedParameters().GetParameters(Parameters);

		TSet<FName> Names;
		for(FNiagaraVariable& Parameter : Parameters)
		{
			Names.Add(Parameter.GetName());
		}
		
		FName UniqueName = FNiagaraUtilities::GetUniqueName(NewName, Names);

		FNiagaraVariable VariableWithNewName(UserParameter);
		VariableWithNewName.SetName(UniqueName);
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(VariableWithNewName);
		
		SystemViewModel.Pin()->RenameParameter(UserParameter, NewName, ENiagaraGetGraphParameterReferencesMode::AllGraphs);
	}
}

FReply FNiagaraSystemUserParameterBuilder::GenerateParameterDragDropOp(const FGeometry& Geometry, const FPointerEvent& MouseEvent, FNiagaraVariable UserParameter) const
{
	TSharedPtr<FEdGraphSchemaAction> ParameterDragAction = MakeShared<FNiagaraParameterAction>(UserParameter, FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), 0, FText(), nullptr, 0/*SectionID*/);
	TSharedRef<FNiagaraParameterDragOperation> ParameterDragOperation = MakeShared<FNiagaraParameterDragOperation>(ParameterDragAction);
	ParameterDragOperation->SetupDefaults();
	ParameterDragOperation->Construct();
	return FReply::Handled().BeginDragDrop(ParameterDragOperation);
}

TSharedRef<IDetailCustomization> FNiagaraComponentDetails::MakeInstance()
{
	return MakeShareable(new FNiagaraComponentDetails);
}

FNiagaraComponentDetails::~FNiagaraComponentDetails()
{
	if (GEngine)
	{
		GEngine->OnWorldDestroyed().RemoveAll(this);
	}

	FGameDelegates::Get().GetEndPlayMapDelegate().RemoveAll(this);
}

void FNiagaraComponentDetails::OnPiEEnd()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("onPieEnd"));
	if (Component.IsValid())
	{
		if (Component->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("onPieEnd - has package flags"));
			UWorld* TheWorld = UWorld::FindWorldInPackage(Component->GetOutermost());
			if (TheWorld)
			{
				OnWorldDestroyed(TheWorld);
			}
		}
	}
}

void FNiagaraComponentDetails::OnWorldDestroyed(class UWorld* InWorld)
{
	// We have to clear out any temp data interfaces that were bound to the component's package when the world goes away or otherwise
	// we'll report GC leaks..
	if (Component.IsValid())
	{
		if (Component->GetWorld() == InWorld)
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("OnWorldDestroyed - matched up"));
			Builder = nullptr;
		}
	}
}

void FNiagaraComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	Builder = &DetailBuilder;

	static const FName ParamCategoryName = TEXT("NiagaraComponent_Parameters");
	static const FName ParamUtilitiesName = TEXT("NiagaraComponent_Utilities");
	static const FName ScriptCategoryName = TEXT("Parameters");

	static bool bFirstTime = true;
	if (bFirstTime)
	{
		const FText DisplayName = LOCTEXT("EffectsSectionName", "Effects");
		TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection("NiagaraComponent", "Effects", DisplayName);
		Section->AddCategory(TEXT("Niagara"));
		Section->AddCategory(ParamCategoryName);
		Section->AddCategory(ParamUtilitiesName); 
		Section->AddCategory(TEXT("Activation"));
		Section->AddCategory(ScriptCategoryName);
		Section->AddCategory(TEXT("Randomness"));
		bFirstTime = false;
	}

	TSharedPtr<IPropertyHandle> LocalOverridesPropertyHandle = DetailBuilder.GetProperty("OverrideParameters");
	if (LocalOverridesPropertyHandle.IsValid())
	{
		LocalOverridesPropertyHandle->MarkHiddenByCustomization();
	}

	TSharedPtr<IPropertyHandle> TemplateParameterOverridesPropertyHandle = DetailBuilder.GetProperty("TemplateParameterOverrides");
	TemplateParameterOverridesPropertyHandle->MarkHiddenByCustomization();

	TSharedPtr<IPropertyHandle> InstanceParameterOverridesPropertyHandle = DetailBuilder.GetProperty("InstanceParameterOverrides");
	InstanceParameterOverridesPropertyHandle->MarkHiddenByCustomization();

	TArray<TSharedPtr<IPropertyHandle>> PropertyHandles { TemplateParameterOverridesPropertyHandle, InstanceParameterOverridesPropertyHandle };

	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	// we override the sort order by specifying the category priority. For same-category, the order of editing decides.
	DetailBuilder.EditCategory("Niagara", FText::GetEmpty(), ECategoryPriority::Important);
	//DetailBuilder.EditCategory(ParamCategoryName, FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Activation", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Lighting", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Attachment", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Randomness", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Parameters", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Materials", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	
	if (ObjectsCustomized.Num() == 1 && ObjectsCustomized[0]->IsA<UNiagaraComponent>())
	{
		Component = CastChecked<UNiagaraComponent>(ObjectsCustomized[0].Get());
		if (GEngine)
		{
			GEngine->OnWorldDestroyed().AddRaw(this, &FNiagaraComponentDetails::OnWorldDestroyed);
		}

		FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FNiagaraComponentDetails::OnPiEEnd);
			
		IDetailCategoryBuilder& InputParamCategory = DetailBuilder.EditCategory(ParamCategoryName, LOCTEXT("ParamCategoryName", "User Parameters"), ECategoryPriority::Important);
		InputParamCategory.AddCustomBuilder(MakeShared<FNiagaraComponentUserParametersNodeBuilder>(Component.Get(), PropertyHandles, ParamCategoryName));
	}
	else if (ObjectsCustomized.Num() > 1)
	{
		IDetailCategoryBuilder& InputParamCategory = DetailBuilder.EditCategory(ParamCategoryName, LOCTEXT("ParamCategoryName", "User Parameters"));
		InputParamCategory.AddCustomRow(LOCTEXT("ParamCategoryName", "User Parameters"))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallText")
				.Text(LOCTEXT("OverrideParameterMultiselectionUnsupported", "Multiple override parameter sets cannot be edited simultaneously."))
			];
	}
	
	IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory(ParamUtilitiesName, LOCTEXT("ParamUtilsCategoryName", "Niagara Utilities"), ECategoryPriority::Important);

	CustomCategory.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.MaxDesiredWidth(300.f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2.0f)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.OnClicked(this, &FNiagaraComponentDetails::OnDebugSelectedSystem)
					.ToolTipText(LOCTEXT("DebugButtonTooltip", "Open Niagara Debugger and point to the first selected particle system"))
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DebugButton", "Debug"))
					]
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.OnClicked(this, &FNiagaraComponentDetails::OnResetSelectedSystem)
					.ToolTipText(LOCTEXT("ResetEmitterButtonTooltip", "Resets the selected particle systems."))
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ResetEmitterButton", "Reset"))
					]
				]
			]
		];
}

FReply FNiagaraComponentDetails::OnResetSelectedSystem()
{
	if (!Builder)
		return FReply::Handled();

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = Builder->GetSelectedObjects();

	for (int32 Idx = 0; Idx < SelectedObjects.Num(); ++Idx)
	{
		if (SelectedObjects[Idx].IsValid())
		{
			if (AActor* Actor = Cast<AActor>(SelectedObjects[Idx].Get()))
			{
				for (UActorComponent* AC : Actor->GetComponents())
				{
					UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(AC);
					if (NiagaraComponent)
					{
						NiagaraComponent->Activate(true);
						NiagaraComponent->ReregisterComponent();
					}
				}
			}
			else if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(SelectedObjects[Idx].Get()))
			{
				NiagaraComponent->Activate(true);
				NiagaraComponent->ReregisterComponent();
			}
			
		}
	}

	return FReply::Handled();
}

FReply FNiagaraComponentDetails::OnDebugSelectedSystem()
{
	if (!Builder)
		return FReply::Handled();

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = Builder->GetSelectedObjects();

	UNiagaraComponent* NiagaraComponentToUse = nullptr;
	for (int32 Idx = 0; Idx < SelectedObjects.Num(); ++Idx)
	{
		if (SelectedObjects[Idx].IsValid())
		{
			if (AActor* Actor = Cast<AActor>(SelectedObjects[Idx].Get()))
			{
				for (UActorComponent* AC : Actor->GetComponents())
				{
					UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(AC);
					if (NiagaraComponent)
					{
						NiagaraComponentToUse = NiagaraComponent;
						break;
					}
				}
			}
			else if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(SelectedObjects[Idx].Get()))
			{
				NiagaraComponentToUse = NiagaraComponent;
				break;
			}
		}
	}

	if (NiagaraComponentToUse)
	{

#if WITH_NIAGARA_DEBUGGER
		SNiagaraDebugger::InvokeDebugger(NiagaraComponentToUse);
#endif
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
