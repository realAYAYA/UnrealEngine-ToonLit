// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "Input/Reply.h"
#include "NiagaraComponent.h"
#include "ViewModels/TNiagaraViewModelManager.h"
#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "ViewModels/HierarchyEditor/NiagaraUserParametersHierarchyViewModel.h"
#include "NiagaraComponentDetails.generated.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class FNiagaraParameterViewModelCustomDetails;
class INiagaraParameterViewModel;
class SNiagaraParameterEditor;
class FNiagaraParameterViewModelCustomDetails;
class UNiagaraSystem;

USTRUCT()
struct FNiagaraEnumToByteHelper
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	uint8 Value = 0;
};

class FNiagaraComponentDetails : public IDetailCustomization
{
public:
	virtual ~FNiagaraComponentDetails() override;

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

protected:
	void OnWorldDestroyed(class UWorld* InWorld);
	void OnPiEEnd();

	FReply OnResetSelectedSystem();
	FReply OnDebugSelectedSystem();
private:
	TWeakObjectPtr<UNiagaraComponent> Component;
	IDetailLayoutBuilder* Builder = nullptr;
};

class FNiagaraSystemUserParameterDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

protected:
	TWeakObjectPtr<UNiagaraSystem> System;
};

/** A base class for generating rows based on a given hierarchy root that contains sections, categories and user parameters. */
class FNiagaraUserParameterNodeBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FNiagaraUserParameterNodeBuilder>, FSelfRegisteringEditorUndoClient
{
public:
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override {}
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const  override
	{
		return CustomBuilderRowName;
	}
	
	void Rebuild() const
	{
		OnRebuildChildren.ExecuteIfBound();
	}
	
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override
	{
		OnRebuildChildren = InOnRegenerateChildren;
	}

	virtual void PostUndo(bool bSuccess) override { Rebuild(); }
	virtual void PostRedo(bool bSuccess) override { Rebuild(); }

	/** Allows the addition of right-click context actions for a specific user parameter */
	virtual void AddCustomMenuActionsForParameter(FDetailWidgetRow& WidgetRow, FNiagaraVariable UserParameter) {}
	/** Allows the creation of a custom name widget for a user parameter. */
	virtual TSharedPtr<SWidget> GenerateCustomNameWidget(FNiagaraVariable UserParameter) { return nullptr; }

	/** Generates all rows for the node builder. Includes setting up sections, categories and user parameters */
	void GenerateUserParameterRows(IDetailChildrenBuilder& ChildrenBuilder, UNiagaraHierarchyRoot& UserParameterHierarchyRoot, UObject* Owner);
	/** Generates a single row for a specific user parameter. */
	void GenerateRowForUserParameter(IDetailChildrenBuilder& ChildrenBuilder, FNiagaraVariable UserParameter, UObject* Owner);
	
	void OnActiveSectionChanged(ECheckBoxState State, const UNiagaraHierarchySection* NewSelection)
	{
		ActiveSection = NewSelection;
		OnRebuildChildren.Execute();
	}

	ECheckBoxState IsSectionActive(const UNiagaraHierarchySection* Section) const
	{
		return ActiveSection == Section ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	
protected:
	bool bDelegatesInitialized = false;
	TArray<TSharedPtr<IPropertyHandle>> OverridePropertyHandles;
	FSimpleDelegate OnRebuildChildren;
	TArray<TSharedPtr<class FNiagaraParameterProxy>> ParameterProxies;
	TMap<FNiagaraVariableBase, TWeakPtr<FStructOnScope>> ParameterToDisplayStruct;
	FName CustomBuilderRowName;
	const UNiagaraHierarchySection* ActiveSection = nullptr;
	TArray<UNiagaraHierarchySection*> AvailableSections;
};

/** A specialized class to generate user parameters for a component. */
class FNiagaraComponentUserParametersNodeBuilder : public FNiagaraUserParameterNodeBuilder
{
public:
	FNiagaraComponentUserParametersNodeBuilder(UNiagaraComponent* InComponent, TArray<TSharedPtr<IPropertyHandle>> InOverridePropertyHandles, FName InCustomBuilderRowName) 
	{
		OverridePropertyHandles = InOverridePropertyHandles;
		CustomBuilderRowName = InCustomBuilderRowName;

		Component = InComponent;
		bDelegatesInitialized = false;
	}

	virtual ~FNiagaraComponentUserParametersNodeBuilder() override
	{
		if (Component.IsValid() && bDelegatesInitialized)
		{
			Component->OnSynchronizedWithAssetParameters().RemoveAll(this);
			Component->GetOverrideParameters().RemoveAllOnChangedHandlers(this);

			if(UNiagaraSystem* System = Component->GetAsset())
			{
				TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::GetExistingViewModelForObject(System);
				if(SystemViewModel.IsValid())
				{
					SystemViewModel->GetUserParametersHierarchyViewModel()->OnHierarchyChanged().RemoveAll(this);
				}
			}
		}
	}
	
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;

private:
	void RegisterRebuildOnHierarchyChanged();

	void ParameterValueChanged();

private:
	TWeakObjectPtr<UNiagaraComponent> Component;
};

/** A specialized class to generate user parameters for a system asset's editor. Due to reliance on the system view model this should only be used within the asset editor. */
class FNiagaraSystemUserParameterBuilder : public FNiagaraUserParameterNodeBuilder
{
public:
	FNiagaraSystemUserParameterBuilder(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, FName InCustomBuilderRowName);

	virtual ~FNiagaraSystemUserParameterBuilder() override
	{
		if (System.IsValid() && bDelegatesInitialized)
		{
			System->GetExposedParameters().OnStructureChanged().RemoveAll(this);
			
			if(SystemViewModel.IsValid())
			{
				if (UNiagaraUserParametersHierarchyViewModel* HierarchyViewModel = SystemViewModel.Pin()->GetUserParametersHierarchyViewModel())
				{
					HierarchyViewModel->OnHierarchyChanged().RemoveAll(this);
				}
			}
		}
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;

	/** Instead of pure text, we use the parameter widgets for display in a system asset. */
	virtual TSharedPtr<SWidget> GenerateCustomNameWidget(FNiagaraVariable UserParameter) override;
	
	/** We want to add rename & delete actions within a system asset */
	virtual void AddCustomMenuActionsForParameter(FDetailWidgetRow& WidgetRow, FNiagaraVariable UserParameter) override;

	TSharedRef<SWidget> GetAddParameterButton();
private:
	TSharedRef<SWidget> GetAddParameterMenu();
	void AddParameter(FNiagaraVariable NewParameter) const;
	bool CanMakeNewParameterOfType(const FNiagaraTypeDefinition& InType) const;

	void ParameterValueChanged();

	void DeleteParameter(FNiagaraVariable UserParameter) const;
	void RequestRename(FNiagaraVariable UserParameter);
	void RenameParameter(FNiagaraVariable UserParameter, FName NewName);

	FReply GenerateParameterDragDropOp(const FGeometry& Geometry, const FPointerEvent& MouseEvent, FNiagaraVariable UserParameter) const;
private:
	TWeakObjectPtr<UNiagaraSystem> System;
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModel;
	TOptional<FNiagaraVariable> SelectedParameter;
	TMap<FNiagaraVariable, TSharedPtr<class SNiagaraParameterNameTextBlock>> UserParamToWidgetMap;
	TSharedPtr<SWidget> AddParameterButtonContainer;
	TSharedPtr<class SComboButton> AddParameterButton;
	TSharedPtr<class SNiagaraAddParameterFromPanelMenu> AddParameterMenu;
};
