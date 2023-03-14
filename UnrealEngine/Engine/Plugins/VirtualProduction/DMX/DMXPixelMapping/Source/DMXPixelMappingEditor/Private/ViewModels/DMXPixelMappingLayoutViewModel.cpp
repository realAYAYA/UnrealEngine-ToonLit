// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/DMXPixelMappingLayoutViewModel.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingEditorCommon.h"
#include "DMXPixelMappingLayoutSettings.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "LayoutScripts/DMXPixelMappingLayoutScript.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "Editor.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingLayoutViewModel"

UDMXPixelMappingLayoutViewModel::UDMXPixelMappingLayoutViewModel()
{
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

UDMXPixelMappingLayoutViewModel::~UDMXPixelMappingLayoutViewModel()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void UDMXPixelMappingLayoutViewModel::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingLayoutViewModel, LayoutScriptClass))
	{
		PreEditChangeLayoutScriptClass = LayoutScriptClass;
	}
}

void UDMXPixelMappingLayoutViewModel::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingLayoutViewModel, LayoutScriptClass))
	{
		OnLayoutScriptClassChanged();
	}
}

void UDMXPixelMappingLayoutViewModel::SetToolkit(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit)
{
	WeakToolkit = InToolkit;

	RefreshComponents();
	RefreshLayoutScriptClass();

	const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
	if (LayoutSettings && LayoutSettings->bApplyLayoutScriptWhenLoaded)
	{
		FScopedTransaction ApplyLayoutScriptTransactionDirect(LOCTEXT("ApplyLayoutScriptTransactionDirect", "Apply Layout Script"));
		ApplyLayoutScripts();
	}

	InToolkit->GetOnSelectedComponentsChangedDelegate().AddUObject(this, &UDMXPixelMappingLayoutViewModel::OnSelectedComponentsChanged);
}

EDMXPixelMappingLayoutViewModelMode UDMXPixelMappingLayoutViewModel::GetMode() const
{
	// If a Fixture Group is selected, layout its children.
	// If a Matrix is selected, layout its children.
	// If more than one Fixture Group or a Screen Component is selected, layout those.
	if (FixtureGroupComponents.Num() == 1 && ScreenComponents.IsEmpty())
	{
		return EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren;
	}
	else if (MatrixComponents.Num() == 1 && FixtureGroupComponents.IsEmpty())
	{
		return EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren;
	}
	else if (RendererComponent.IsValid() && ScreenComponents.IsEmpty() && FixtureGroupComponents.IsEmpty() && MatrixComponents.IsEmpty())
	{
		return EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren;
	}

	return EDMXPixelMappingLayoutViewModelMode::LayoutNone;
}

TArray<UObject*> UDMXPixelMappingLayoutViewModel::GetLayoutScriptsObjectsSlow() const
{
	TArray<UObject*> Result;

	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren)
	{
		if (RendererComponent.IsValid())
		{
			Result.Add(RendererComponent->LayoutScript);
		}
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren)
	{
		for (TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent> FixtureGroup : FixtureGroupComponents)
		{
			if (FixtureGroup.IsValid() && FixtureGroup->LayoutScript)
			{
				Result.Add(FixtureGroup->LayoutScript);
			}
		}
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren)
	{
		for (TWeakObjectPtr<UDMXPixelMappingMatrixComponent> Matrix : MatrixComponents)
		{
			if (Matrix.IsValid() && Matrix->LayoutScript)
			{
				Result.Add(Matrix->LayoutScript);
			}
		}
	}

	return Result;
}

void UDMXPixelMappingLayoutViewModel::RequestApplyLayoutScripts()
{
	if (!ApplyLayoutScriptTimerHandle.IsValid())
	{
		ApplyLayoutScriptTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UDMXPixelMappingLayoutViewModel::ApplyLayoutScripts));
	}
}

void UDMXPixelMappingLayoutViewModel::PostUndo(bool bSuccess)
{
	RefreshComponents();
	RefreshLayoutScriptClass();
}

void UDMXPixelMappingLayoutViewModel::PostRedo(bool bSuccess)
{
	RefreshComponents();
	RefreshLayoutScriptClass();
}

void UDMXPixelMappingLayoutViewModel::ApplyLayoutScripts()
{
	ApplyLayoutScriptTimerHandle.Invalidate();

	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren)
	{
		LayoutRendererComponentChildren();
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren)
	{
		LayoutFixtureGroupComponentChildren();
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren)
	{
		LayoutMatrixComponentChildren();
	}
	else
	{
		ensureAlwaysMsgf(LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutNone, TEXT("Unhandled Layout Type, layout cannot be applied"));
	}
}

void UDMXPixelMappingLayoutViewModel::LayoutRendererComponentChildren()
{
	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (!ensureMsgf(LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren, TEXT("Trying to layout children of Renderer Components, but the current selection does not support this.")))
	{
		return;
	}

	if (!RendererComponent.IsValid())
	{
		return;
	}

	InitializeLayoutScript(RendererComponent.Get(), RendererComponent->LayoutScript);

	TArray<FDMXPixelMappingLayoutToken> InLayoutTokens;
	for (UDMXPixelMappingBaseComponent* ChildComponentBase : RendererComponent->GetChildren())
	{
		if (UDMXPixelMappingOutputComponent* ChildComponent = Cast<UDMXPixelMappingOutputComponent>(ChildComponentBase))
		{
			InLayoutTokens.Add(FDMXPixelMappingLayoutToken(ChildComponent));
		}
	}

	TArray<FDMXPixelMappingLayoutToken> OutLayoutTokens;
	RendererComponent->LayoutScript->Layout(InLayoutTokens, OutLayoutTokens);

	RendererComponent->PreEditChange(UDMXPixelMappingRendererComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingRendererComponent, LayoutScript)));
	ApplyLayoutTokens(OutLayoutTokens);
	RendererComponent->PostEditChange();
}

void UDMXPixelMappingLayoutViewModel::LayoutFixtureGroupComponentChildren()
{
	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (!ensureMsgf(!FixtureGroupComponents.IsEmpty() && LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren, TEXT("Trying to layout children of Fixture Group Components, but the current selection does not support this.")))
	{
		return;
	}

	for (TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent> WeakFixtureGroupComponent : FixtureGroupComponents)
	{
		if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = WeakFixtureGroupComponent.Get())
		{
			if (!ensureMsgf(FixtureGroupComponent->LayoutScript, TEXT("Missing layout script when trying to apply layout script to Fixture Group Component.")))
			{
				continue;
			}

			InitializeLayoutScript(FixtureGroupComponent, FixtureGroupComponent->LayoutScript);

			TArray<FDMXPixelMappingLayoutToken> InLayoutTokens;
			for (UDMXPixelMappingBaseComponent* ChildComponent : FixtureGroupComponent->GetChildren())
			{
				if (UDMXPixelMappingOutputComponent* ChildOutputComponent = Cast<UDMXPixelMappingOutputComponent>(ChildComponent))
				{
					InLayoutTokens.Add(FDMXPixelMappingLayoutToken(ChildOutputComponent));
				}
			}

			TArray<FDMXPixelMappingLayoutToken> OutLayoutTokens;
			FixtureGroupComponent->LayoutScript->Layout(InLayoutTokens, OutLayoutTokens);

			FixtureGroupComponent->PreEditChange(UDMXPixelMappingFixtureGroupComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupComponent, LayoutScript)));
			ApplyLayoutTokens(OutLayoutTokens);
			FixtureGroupComponent->PostEditChange();
		}
	}
}

void UDMXPixelMappingLayoutViewModel::LayoutMatrixComponentChildren()
{
	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (!ensureMsgf(!MatrixComponents.IsEmpty() && LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren, TEXT("Trying to layout children of Fixture Group Components, but the current selection does not support this.")))
	{
		return;
	}

	for (TWeakObjectPtr<UDMXPixelMappingMatrixComponent> WeakMatrixComponent : MatrixComponents)
	{
		if (UDMXPixelMappingMatrixComponent* MatrixComponent = WeakMatrixComponent.Get())
		{
			if (!ensureMsgf(MatrixComponent->LayoutScript, TEXT("Missing layout script when trying to apply layout script to Matrix Component.")))
			{
				continue;
			}

			InitializeLayoutScript(MatrixComponent, MatrixComponent->LayoutScript);

			TArray<FDMXPixelMappingLayoutToken> InLayoutTokens;
			for (UDMXPixelMappingBaseComponent* ChildComponent : MatrixComponent->GetChildren())
			{
				if (UDMXPixelMappingOutputComponent* ChildOutputComponent = Cast<UDMXPixelMappingOutputComponent>(ChildComponent))
				{
					InLayoutTokens.Add(FDMXPixelMappingLayoutToken(ChildOutputComponent));
				}
			}

			TArray<FDMXPixelMappingLayoutToken> OutLayoutTokens;
			MatrixComponent->LayoutScript->Layout(InLayoutTokens, OutLayoutTokens);

			MatrixComponent->PreEditChange(UDMXPixelMappingMatrixComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, LayoutScript)));
			ApplyLayoutTokens(OutLayoutTokens);
			MatrixComponent->PostEditChange();
		}
	}
}

void UDMXPixelMappingLayoutViewModel::ApplyLayoutTokens(const TArray<FDMXPixelMappingLayoutToken>& LayoutTokens) const
{

	for (const FDMXPixelMappingLayoutToken& LayoutToken : LayoutTokens)
	{
		if (UDMXPixelMappingOutputComponent* OutputComponent = LayoutToken.Component.Get())
		{
			if (LayoutToken.SizeX < 1.f || LayoutToken.SizeY < 1.f)
			{
				UE_LOG(LogDMXPixelMappingEditor, Warning, TEXT("Trying to apply layout script. But layout defines a component size < 1. This is not supported."));
			}

			if (OutputComponent->GetParent())
			{
				OutputComponent->GetParent()->Modify();
			}

			OutputComponent->Modify();

			OutputComponent->SetPosition(FVector2D(LayoutToken.PositionX, LayoutToken.PositionY));
			OutputComponent->SetSize(FVector2D(LayoutToken.SizeX, LayoutToken.SizeY));
		}
	}
}

void UDMXPixelMappingLayoutViewModel::OnLayoutScriptClassChanged()
{
	if (PreEditChangeLayoutScriptClass != LayoutScriptClass)
	{
		InstantiateLayoutScripts();

		// Give others time to update before applying the scripts
		OnModelChanged.Broadcast();
	}

	const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
	if (LayoutScriptClass && LayoutSettings && LayoutSettings->bApplyLayoutScriptWhenLoaded)
	{
		ApplyLayoutScripts();
	}
}

void UDMXPixelMappingLayoutViewModel::OnSelectedComponentsChanged()
{
	RefreshComponents();
	RefreshLayoutScriptClass();
	InstantiateLayoutScripts();

	OnModelChanged.Broadcast();
}

void UDMXPixelMappingLayoutViewModel::InstantiateLayoutScripts()
{
	if (!RendererComponent.IsValid())
	{
		return;
	}

	UClass* StrongLayoutScriptClass = LayoutScriptClass.Get();
	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren)
	{
		if (RendererComponent.IsValid() && 
		   (!RendererComponent->LayoutScript ||	RendererComponent->LayoutScript->GetClass() != StrongLayoutScriptClass))
		{
			if (StrongLayoutScriptClass)
			{
				const FName UniqueName = MakeUniqueObjectName(RendererComponent.Get(), UDMXPixelMappingLayoutScript::StaticClass());
				RendererComponent->LayoutScript = NewObject<UDMXPixelMappingLayoutScript>(RendererComponent.Get(), StrongLayoutScriptClass, UniqueName, RF_Transactional | RF_Public);
				InitializeLayoutScript(RendererComponent.Get(), RendererComponent->LayoutScript);
			}
			else
			{
				RendererComponent->LayoutScript = nullptr;
			}
		}
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren)
	{
		for (TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent> FixtureGroupComponent : FixtureGroupComponents)
		{
			if (FixtureGroupComponent.IsValid() && 
				(!FixtureGroupComponent->LayoutScript || FixtureGroupComponent->LayoutScript->GetClass() != StrongLayoutScriptClass))
			{
				if (StrongLayoutScriptClass)
				{
					const FName UniqueName = MakeUniqueObjectName(FixtureGroupComponent.Get(), UDMXPixelMappingLayoutScript::StaticClass());
					FixtureGroupComponent->LayoutScript = NewObject<UDMXPixelMappingLayoutScript>(FixtureGroupComponent.Get(), StrongLayoutScriptClass, UniqueName, RF_Transactional | RF_Public);
					InitializeLayoutScript(FixtureGroupComponent.Get(), FixtureGroupComponent->LayoutScript);
				}
				else
				{
					FixtureGroupComponent->LayoutScript = nullptr;
				}
			}
		}
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren)
	{
		for (TWeakObjectPtr<UDMXPixelMappingMatrixComponent> MatrixComponent : MatrixComponents)
		{
			if (MatrixComponent.IsValid() &&
			   (!MatrixComponent->LayoutScript || MatrixComponent->LayoutScript->GetClass() != StrongLayoutScriptClass))
			{
				if (StrongLayoutScriptClass)
				{
					const FName UniqueName = MakeUniqueObjectName(MatrixComponent.Get(), UDMXPixelMappingLayoutScript::StaticClass());
					MatrixComponent->LayoutScript = NewObject<UDMXPixelMappingLayoutScript>(MatrixComponent.Get(), StrongLayoutScriptClass, UniqueName, RF_Transactional | RF_Public);
					InitializeLayoutScript(MatrixComponent.Get(), MatrixComponent->LayoutScript);
				}
				else
				{
					MatrixComponent->LayoutScript = nullptr;
				}
			}
		}
	}
	else
	{
		ensureMsgf(LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutNone, TEXT("Unhandled Layout Type, layout cannot be applied"));
	}
}

void UDMXPixelMappingLayoutViewModel::InitializeLayoutScript(UDMXPixelMappingOutputComponent* OutputComponent, UDMXPixelMappingLayoutScript* LayoutScript)
{
	if (!ensureMsgf(OutputComponent && LayoutScript && OutputComponent->GetRendererComponent(), TEXT("Invalid component, layout script or renderer component when trying to initialize layut script. layout cannot be applied")))
	{
		return;
	}

	LayoutScript->SetNumTokens(OutputComponent->GetChildren().Num());
	LayoutScript->SetParentComponentPosition(OutputComponent->GetPosition());
	LayoutScript->SetParentComponentSize(OutputComponent->GetSize());
	LayoutScript->SetTextureSize(OutputComponent->GetRendererComponent()->GetSize());
}

void UDMXPixelMappingLayoutViewModel::RefreshComponents()
{
	// Reset
	RendererComponent = nullptr;
	ScreenComponents.Reset();
	FixtureGroupComponents.Reset();
	MatrixComponents.Reset();

	// Find relevant components
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin();
	if (!Toolkit.IsValid())
	{
		return;
	}

	RendererComponent = Toolkit->GetActiveRendererComponent();
	if (!RendererComponent.IsValid())
	{
		return;
	}

	const TSet<FDMXPixelMappingComponentReference> ComponentsToLayout = Toolkit->GetSelectedComponents();

	for (const FDMXPixelMappingComponentReference& ComponentReference : ComponentsToLayout)
	{
		if (UDMXPixelMappingBaseComponent* Component = ComponentReference.GetComponent())
		{
			if (UDMXPixelMappingScreenComponent* ScreenComponent = Cast<UDMXPixelMappingScreenComponent>(Component))
			{
				ScreenComponents.Add(ScreenComponent);
			}
			else if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(Component))
			{
				FixtureGroupComponents.Add(FixtureGroupComponent);
			}
			else if (UDMXPixelMappingFixtureGroupItemComponent* FixtureGroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(Component))
			{
				// If Fixture Group Items are selected, use their parent Group instead.
				if (UDMXPixelMappingFixtureGroupComponent* ParentFixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(FixtureGroupItemComponent->GetParent()))
				{
					FixtureGroupComponents.AddUnique(ParentFixtureGroupComponent);
				}
			}
			else if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(Component))
			{
				MatrixComponents.Add(MatrixComponent);
			}
			else if (UDMXPixelMappingMatrixCellComponent* MatrixCellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(Component))
			{					
				// If Matrix Cells are selected, use their parent Matrix instead.
				if (UDMXPixelMappingMatrixComponent* ParentMatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(MatrixCellComponent->GetParent()))
				{
					MatrixComponents.AddUnique(ParentMatrixComponent);
				}
			}
		}
	}
}

void UDMXPixelMappingLayoutViewModel::RefreshLayoutScriptClass()
{
	LayoutScriptClass = nullptr;

	const EDMXPixelMappingLayoutViewModelMode LayoutType = GetMode();
	if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren)
	{
		if (RendererComponent.IsValid() && RendererComponent->LayoutScript)
		{
			LayoutScriptClass =RendererComponent->LayoutScript->GetClass();
		}
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren)
	{
		for (TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent> FixtureGroup : FixtureGroupComponents)
		{
			if (!FixtureGroup.IsValid())
			{
				continue;
			}

			if (FixtureGroup->LayoutScript && LayoutScriptClass && FixtureGroup->LayoutScript->GetClass() != LayoutScriptClass)
			{
				// Multiple values
				LayoutScriptClass = nullptr;
				return;
			}
			else if (FixtureGroup->LayoutScript)
			{
				LayoutScriptClass = FixtureGroup->LayoutScript->GetClass();
			}
		}
	}
	else if (LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren)
	{
		for (TWeakObjectPtr<UDMXPixelMappingMatrixComponent> Matrix : MatrixComponents)
		{
			if (!Matrix.IsValid())
			{
				continue;
			}

			if (Matrix->LayoutScript && LayoutScriptClass && Matrix->LayoutScript->GetClass() != LayoutScriptClass)
			{
				// Multiple values
				LayoutScriptClass = nullptr;
				return;
			}
			else if (Matrix->LayoutScript)
			{
				LayoutScriptClass = Matrix->LayoutScript->GetClass();
			}
		}
	}
	else
	{
		ensureMsgf(LayoutType == EDMXPixelMappingLayoutViewModelMode::LayoutNone, TEXT("Unhandled Layout Type, layout cannot be applied"));
	}
}

#undef LOCTEXT_NAMESPACE
