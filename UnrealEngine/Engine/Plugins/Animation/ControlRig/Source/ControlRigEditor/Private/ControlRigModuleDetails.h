// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "ControlRig.h"
#include "ModularRig.h"
#include "ControlRigBlueprint.h"
#include "ControlRigElementDetails.h"
#include "Editor/ControlRigWrapperObject.h"
#include "Styling/SlateTypes.h"
#include "IPropertyUtilities.h"
#include "SSearchableComboBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Internationalization/FastDecimalFormat.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Algo/Transform.h"
#include "IPropertyUtilities.h"
#include "Editor/SRigHierarchyTreeView.h"

class IPropertyHandle;

class FRigModuleInstanceDetails : public IDetailCustomization
{
public:

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRigModuleInstanceDetails);
	}

	FText GetName() const;
	void SetName(const FText& InValue, ETextCommit::Type InCommitType, const TSharedRef<IPropertyUtilities> PropertyUtilities);
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);
	FText GetShortName() const;
	void SetShortName(const FText& InValue, ETextCommit::Type InCommitType, const TSharedRef<IPropertyUtilities> PropertyUtilities);
	bool OnVerifyShortNameChanged(const FText& InText, FText& OutErrorMessage);
	FText GetLongName() const;
	FText GetRigClassPath() const;
	TArray<FRigModuleConnector> GetConnectors() const;
	FRigElementKeyRedirector GetConnections() const;
	void PopulateConnectorTargetList(const FString InConnectorKey);
	void PopulateConnectorCurrentTarget(
		TSharedPtr<SVerticalBox> InListBox,
		const FSlateBrush* InBrush,
		const FSlateColor& InColor,
		const FText& InTitle);

	void OnConfigValueChanged(const FName InVariableName);
	void OnConnectorTargetChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo, const FRigModuleConnector InConnector);
	
	struct FPerModuleInfo
	{
		FPerModuleInfo()
			: Path()
			, Module()
			, DefaultModule()
		{}

		bool IsValid() const { return Module.IsValid(); }
		operator bool() const { return IsValid(); }

		const FString& GetPath() const
		{
			return Path;
		}

		UModularRig* GetModularRig() const
		{
			return (UModularRig*)Module.GetModularRig();
		}
		
		UModularRig* GetDefaultRig() const
		{
			if(DefaultModule.IsValid())
			{
				return (UModularRig*)DefaultModule.GetModularRig();
			}
			return GetModularRig();
		}

		UControlRigBlueprint* GetBlueprint() const
		{
			if(const UModularRig* ControlRig = GetModularRig())
			{
				return Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy);
			}
			return nullptr;
		}

		FRigModuleInstance* GetModule() const
		{
			return (FRigModuleInstance*)Module.Get();
		}

		FRigModuleInstance* GetDefaultModule() const
		{
			if(DefaultModule)
			{
				return (FRigModuleInstance*)DefaultModule.Get();
			}
			return GetModule();
		}

		const FRigModuleReference* GetReference() const
		{
			if(const UControlRigBlueprint* Blueprint = GetBlueprint())
			{
				return Blueprint->ModularRigModel.FindModule(Path);
			}
			return nullptr;
		}

		FString Path;
		FModuleInstanceHandle Module;
		FModuleInstanceHandle DefaultModule;
	};

	const FPerModuleInfo& FindModule(const FString& InKey) const;
	const FPerModuleInfo* FindModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const;
	bool ContainsModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const;

	virtual void RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass);

protected:

	FText GetBindingText(const FProperty* InProperty) const;
	const FSlateBrush* GetBindingImage(const FProperty* InProperty) const;
	FLinearColor GetBindingColor(const FProperty* InProperty) const;
	void FillBindingMenu(FMenuBuilder& MenuBuilder, const FProperty* InProperty) const;
	bool CanRemoveBinding(FName InPropertyName) const;
	void HandleRemoveBinding(FName InPropertyName) const;
	void HandleChangeBinding(const FProperty* InProperty, const FString& InNewVariablePath) const;

	TArray<FPerModuleInfo> PerModuleInfos;
	TMap<FString, TSharedPtr<SSearchableRigHierarchyTreeView>> ConnectionListBox;

	/** Helper buttons. */
	TMap<FString, TSharedPtr<SButton>> UseSelectedButton;
	TMap<FString, TSharedPtr<SButton>> SelectElementButton;
	TMap<FString, TSharedPtr<SButton>> ResetConnectorButton;
};
