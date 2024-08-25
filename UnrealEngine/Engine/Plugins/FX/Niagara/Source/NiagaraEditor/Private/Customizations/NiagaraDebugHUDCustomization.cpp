// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDebugHUDCustomization.h"

#include "Modules/ModuleManager.h"

//Customization
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailChildrenBuilder.h"
//Widgets
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNiagaraDebugger.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSuggestionTextBox.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
///Niagara
#include "NiagaraEditorModule.h"
#include "NiagaraComponent.h"

#if WITH_NIAGARA_DEBUGGER

#define LOCTEXT_NAMESPACE "NiagaraDebugHUDCustomization"

//////////////////////////////////////////////////////////////////////////

namespace NiagaraDebugHUDSettingsDetailsCustomizationInternal
{

class SDebuggerSuggestionTextBox : public SSuggestionTextBox
{
public:
	SLATE_BEGIN_ARGS(SDebuggerSuggestionTextBox) {}
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PropertyHandle)
		SLATE_ARGUMENT(UClass*, ObjectClass)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		Debugger = NiagaraEditorModule.GetDebugger();
		if ( Debugger )
		{
			Debugger->GetOnSimpleClientInfoChanged().AddSP(this, &SDebuggerSuggestionTextBox::OnSimpleClientInfoChanged);
		}

		PropertyHandle = InArgs._PropertyHandle;
		ObjectClass = InArgs._ObjectClass;

		FText CurrentValue;
		PropertyHandle->GetValueAsFormattedText(CurrentValue);

		SSuggestionTextBox::Construct(
			SSuggestionTextBox::FArguments()
			.ForegroundColor(FSlateColor::UseForeground())
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(CurrentValue)
			.OnTextCommitted(this, &SDebuggerSuggestionTextBox::OnDebuggerTextCommitted)
			.OnShowingSuggestions(this, &SDebuggerSuggestionTextBox::GetDebuggerSuggestions)
		);
	}

	void OnDebuggerTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		if (PropertyHandle)
		{
			PropertyHandle->SetValueFromFormattedString(NewText.ToString());
		}
	}

	void GetDebuggerSuggestions(const FString& CurrText, TArray<FString>& OutSuggestions)
	{
		if (!Debugger.IsValid() || !ObjectClass)
		{
			return;
		}

		const FNiagaraSimpleClientInfo& SimpleClientInfo = Debugger->GetSimpleClientInfo();
		if (ObjectClass == UNiagaraSystem::StaticClass())
		{
			OutSuggestions.Append(SimpleClientInfo.Systems);
		}
		else if (ObjectClass == UNiagaraEmitter::StaticClass())
		{
			OutSuggestions.Append(SimpleClientInfo.Emitters);
		}
		else if (ObjectClass == AActor::StaticClass())
		{
			OutSuggestions.Append(SimpleClientInfo.Actors);
		}
		else if (ObjectClass == UNiagaraComponent::StaticClass())
		{
			OutSuggestions.Append(SimpleClientInfo.Components);
		}
	}

	void OnSimpleClientInfoChanged(const FNiagaraSimpleClientInfo& ClientInfo)
	{
		bWaitingUpdate = false;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SSuggestionTextBox::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		if (Debugger)
		{
			const bool bHasFocus = FSlateApplication::Get().HasFocusedDescendants(SharedThis(this));
			if (bHasFocus && !bWaitingUpdate)
			{
				bWaitingUpdate = true;
				Debugger->RequestUpdatedClientInfo();
			}
		}
	}

	TSharedPtr<FNiagaraDebugger>	Debugger;
	TSharedPtr<IPropertyHandle>		PropertyHandle;
	UClass*							ObjectClass = nullptr;
	bool							bWaitingUpdate = false;
};

} // namespace

//////////////////////////////////////////////////////////////////////////

void FNiagaraDebugHUDVariableCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	EnabledPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraDebugHUDVariable, bEnabled));
	NamePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraDebugHUDVariable, Name));
	check(EnabledPropertyHandle.IsValid() && NamePropertyHandle.IsValid())

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FNiagaraDebugHUDVariableCustomization::IsEnabled)
				.OnCheckStateChanged(this, &FNiagaraDebugHUDVariableCustomization::SetEnabled)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SEditableTextBox)
				.IsEnabled(this, &FNiagaraDebugHUDVariableCustomization::IsTextEditable)
				.Text(this, &FNiagaraDebugHUDVariableCustomization::GetText)
				.OnTextCommitted(this, &FNiagaraDebugHUDVariableCustomization::SetText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

ECheckBoxState FNiagaraDebugHUDVariableCustomization::IsEnabled() const
{
	bool bEnabled = false;
	EnabledPropertyHandle->GetValue(bEnabled);
	return bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FNiagaraDebugHUDVariableCustomization::SetEnabled(ECheckBoxState NewState)
{
	bool bEnabled = NewState == ECheckBoxState::Checked;
	EnabledPropertyHandle->SetValue(bEnabled);
}

FText FNiagaraDebugHUDVariableCustomization::GetText() const
{
	FString Text;
	NamePropertyHandle->GetValue(Text);
	return FText::FromString(Text);
}

void FNiagaraDebugHUDVariableCustomization::SetText(const FText& NewText, ETextCommit::Type CommitInfo)
{
	NamePropertyHandle->SetValue(NewText.ToString());
}

bool FNiagaraDebugHUDVariableCustomization::IsTextEditable() const
{
	bool bEnabled = false;
	EnabledPropertyHandle->GetValue(bEnabled);
	return bEnabled;
}

FNiagaraDebugHUDSettingsDetailsCustomization::FNiagaraDebugHUDSettingsDetailsCustomization(UNiagaraDebugHUDSettings* InSettings)
	: WeakSettings(InSettings)
{
}

void FNiagaraDebugHUDSettingsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Customize General
	{
		IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("Debug General");
	}

	// Customize Overview
	{
		IDetailCategoryBuilder& OverviewCategory = DetailBuilder.EditCategory("Debug Overview");
	}

	// Customize Filters
	{
		IDetailCategoryBuilder& FilterCategory = DetailBuilder.EditCategory("Debug Filter");
		MakeCustomAssetSearch(DetailBuilder, FilterCategory, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FNiagaraDebugHUDSettingsData, SystemFilter), FNiagaraDebugHUDSettingsData::StaticStruct()), UNiagaraSystem::StaticClass(), [&]() -> bool& { return WeakSettings.Get()->Data.bSystemFilterEnabled; });
		MakeCustomAssetSearch(DetailBuilder, FilterCategory, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FNiagaraDebugHUDSettingsData, EmitterFilter), FNiagaraDebugHUDSettingsData::StaticStruct()), UNiagaraEmitter::StaticClass(), [&]() -> bool& { return WeakSettings.Get()->Data.bEmitterFilterEnabled; });
		MakeCustomAssetSearch(DetailBuilder, FilterCategory, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FNiagaraDebugHUDSettingsData, ActorFilter), FNiagaraDebugHUDSettingsData::StaticStruct()), AActor::StaticClass(), [&]() -> bool& { return WeakSettings.Get()->Data.bActorFilterEnabled; });
		MakeCustomAssetSearch(DetailBuilder, FilterCategory, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FNiagaraDebugHUDSettingsData, ComponentFilter), FNiagaraDebugHUDSettingsData::StaticStruct()), UNiagaraComponent::StaticClass(), [&]() -> bool& { return WeakSettings.Get()->Data.bComponentFilterEnabled; });
	}
}

void FNiagaraDebugHUDSettingsDetailsCustomization::MakeCustomAssetSearch(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& DetailCategory, TSharedRef<IPropertyHandle> PropertyHandle, UClass* ObjRefClass, TFunction<bool&()> GetEditBool)
{
	if ( !PropertyHandle->IsValidHandle() || (ObjRefClass == nullptr) )
	{
		return;
	}

	FText CurrentValue;
	PropertyHandle->GetValueAsFormattedText(CurrentValue);

	DetailBuilder.HideProperty(PropertyHandle);

	DetailCategory.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([GetEditBool]() -> ECheckBoxState { return GetEditBool() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
			.OnCheckStateChanged_Lambda([this, GetEditBool](ECheckBoxState NewState) { GetEditBool() = NewState == ECheckBoxState::Checked; WeakSettings.Get()->NotifyPropertyChanged(); })
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			.IsEnabled_Lambda([=]() { return GetEditBool(); })
			+ SHorizontalBox::Slot()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		.IsEnabled_Lambda([=]() { return GetEditBool(); })
		+ SHorizontalBox::Slot()
		[
			SNew(NiagaraDebugHUDSettingsDetailsCustomizationInternal::SDebuggerSuggestionTextBox)
			.PropertyHandle(PropertyHandle)
			.ObjectClass(ObjRefClass)
		]
	];
}

#undef LOCTEXT_NAMESPACE

#endif
