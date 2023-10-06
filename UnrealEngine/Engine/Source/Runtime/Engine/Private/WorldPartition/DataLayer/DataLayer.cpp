// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataLayer.cpp: UDEPRECATED_DataLayer class implementation
=============================================================================*/

#include "WorldPartition/DataLayer/DataLayer.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayer)

#define LOCTEXT_NAMESPACE "DataLayer"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_DataLayer::UDEPRECATED_DataLayer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
, bIsInitiallyActive_DEPRECATED(false)
, bIsVisible(true)
, bIsInitiallyVisible(true)
, bIsInitiallyLoadedInEditor(true)
, bIsLoadedInEditor(true)
, bIsLocked(false)
#endif
, DataLayerLabel(GetFName())
, bIsRuntime(false)
, InitialRuntimeState(EDataLayerRuntimeState::Unloaded)
, DebugColor(FColor::Black)
{}

void UDEPRECATED_DataLayer::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (bIsInitiallyActive_DEPRECATED)
	{
		InitialRuntimeState = EDataLayerRuntimeState::Activated;
	}

	// Initialize bIsVisible with persistent flag bIsInitiallyVisible
	bIsVisible = bIsInitiallyVisible;

	// Sanitize Label
	DataLayerLabel = *FDataLayerUtils::GetSanitizedDataLayerShortName(DataLayerLabel.ToString());

	if (DebugColor == FColor::Black)
	{
		DebugColor = FColor::MakeRandomSeededColor(GetTypeHash(GetName()));
	}
#endif

	if (Parent_DEPRECATED)
	{
		Parent_DEPRECATED->AddChild(this);
	}
}

bool UDEPRECATED_DataLayer::IsInitiallyVisible() const
{
#if WITH_EDITOR
	return bIsInitiallyVisible;
#else
	return false;
#endif
}

bool UDEPRECATED_DataLayer::IsVisible() const
{
#if WITH_EDITOR
	return bIsVisible;
#else
	return false;
#endif
}

bool UDEPRECATED_DataLayer::IsEffectiveVisible() const
{
#if WITH_EDITOR
	bool bResult = IsVisible();
	const UDEPRECATED_DataLayer* ParentDataLayer = GetParent();
	while (ParentDataLayer && bResult)
	{
		bResult = bResult && ParentDataLayer->IsVisible();
		ParentDataLayer = ParentDataLayer->GetParent();
	}
	return bResult && IsEffectiveLoadedInEditor();
#else
	return false;
#endif
}

void UDEPRECATED_DataLayer::AddChild(UDEPRECATED_DataLayer* InDataLayer)
{
	Modify();
	checkSlow(!Children_DEPRECATED.Contains(InDataLayer));
	Children_DEPRECATED.Add(InDataLayer);
#if WITH_EDITOR
	if (IsRuntime())
	{
		InDataLayer->SetIsRuntime(true);
	}
#endif
}

#if WITH_EDITOR

bool UDEPRECATED_DataLayer::IsEffectiveLoadedInEditor() const
{
	bool bResult = IsLoadedInEditor();
	const UDEPRECATED_DataLayer* ParentDataLayer = GetParent();
	while (ParentDataLayer && bResult)
	{
		bResult = bResult && ParentDataLayer->IsLoadedInEditor();
		ParentDataLayer = ParentDataLayer->GetParent();
	}
	return bResult;
}

bool UDEPRECATED_DataLayer::IsLocked() const
{
	if (bIsLocked)
	{
		return true;
	}

	return IsRuntime() && !GetOuterAWorldDataLayers()->GetAllowRuntimeDataLayerEditing();
}

bool UDEPRECATED_DataLayer::CanEditChange(const FProperty* InProperty) const
{
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDEPRECATED_DataLayer, bIsRuntime)) ||
		(InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDEPRECATED_DataLayer, InitialRuntimeState)) ||
		(InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDEPRECATED_DataLayer, DebugColor)))
	{
		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDEPRECATED_DataLayer, bIsRuntime))
		{
			// If DataLayer is Runtime because of its parent being Runtime, we don't allow modifying 
			if (Parent_DEPRECATED && Parent_DEPRECATED->IsRuntime())
			{
				check(IsRuntime());
				return false;
			}
		}
		return GetOuterAWorldDataLayers()->GetAllowRuntimeDataLayerEditing();
	}

	return Super::CanEditChange(InProperty);
}

void UDEPRECATED_DataLayer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_IsRuntime = GET_MEMBER_NAME_CHECKED(UDEPRECATED_DataLayer, bIsRuntime);
	FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FName MemberPropertyName = MemberPropertyThatChanged != NULL ? MemberPropertyThatChanged->GetFName() : NAME_None;
	if (MemberPropertyName == NAME_IsRuntime)
	{
		PropagateIsRuntime();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UDEPRECATED_DataLayer::CanParent(const UDEPRECATED_DataLayer* InParent) const
{
	return (this != InParent) && (Parent_DEPRECATED != InParent);
}

void UDEPRECATED_DataLayer::SetParent(UDEPRECATED_DataLayer* InParent)
{
	if (!CanParent(InParent))
	{
		return;
	}

	Modify();
	if (Parent_DEPRECATED)
	{
		Parent_DEPRECATED->RemoveChild(this);
	}
	Parent_DEPRECATED = InParent;
	if (Parent_DEPRECATED)
	{
		Parent_DEPRECATED->AddChild(this);
	}
}

void UDEPRECATED_DataLayer::SetChildParent(UDEPRECATED_DataLayer* InParent)
{
	if (this == InParent)
	{
		return;
	}

	Modify();
	while (Children_DEPRECATED.Num())
	{
		Children_DEPRECATED[0]->SetParent(InParent);
	};
}

void UDEPRECATED_DataLayer::RemoveChild(UDEPRECATED_DataLayer* InDataLayer)
{
	Modify();
	check(Children_DEPRECATED.Contains(InDataLayer));
	Children_DEPRECATED.RemoveSingle(InDataLayer);
}

const TCHAR* UDEPRECATED_DataLayer::GetDataLayerIconName() const
{
	return IsRuntime() ? TEXT("DataLayer.Runtime") : TEXT("DataLayer.Editor");
}

void UDEPRECATED_DataLayer::SetVisible(bool bInIsVisible)
{
	if (bIsVisible != bInIsVisible)
	{
		Modify(/*bAlwaysMarkDirty*/false);
		bIsVisible = bInIsVisible;
	}
}

void UDEPRECATED_DataLayer::SetIsInitiallyVisible(bool bInIsInitiallyVisible)
{
	if (bIsInitiallyVisible != bInIsInitiallyVisible)
	{
		Modify();
		bIsInitiallyVisible = bInIsInitiallyVisible;
	}
}

void UDEPRECATED_DataLayer::SetIsRuntime(bool bInIsRuntime)
{
	if (bIsRuntime != bInIsRuntime)
	{
		Modify();
		bIsRuntime = bInIsRuntime;

		PropagateIsRuntime();
	}
}

void UDEPRECATED_DataLayer::PropagateIsRuntime()
{
	if (IsRuntime())
	{
		for (UDEPRECATED_DataLayer* Child : Children_DEPRECATED)
		{
			Child->SetIsRuntime(true);
		}
	}
}

void UDEPRECATED_DataLayer::SetIsLoadedInEditor(bool bInIsLoadedInEditor, bool bInFromUserChange)
{
	if (bIsLoadedInEditor != bInIsLoadedInEditor)
	{
		Modify(false);
		bIsLoadedInEditor = bInIsLoadedInEditor;
		bIsLoadedInEditorChangedByUserOperation |= bInFromUserChange;
	}
}

FText UDEPRECATED_DataLayer::GetDataLayerText(const UDEPRECATED_DataLayer* InDataLayer)
{
	return InDataLayer ? FText::FromName(InDataLayer->GetDataLayerLabel()) : LOCTEXT("InvalidDataLayerLabel", "<None>");
}

#endif

void UDEPRECATED_DataLayer::ForEachChild(TFunctionRef<bool(const UDEPRECATED_DataLayer*)> Operation) const
{
	for (UDEPRECATED_DataLayer* Child : Children_DEPRECATED)
	{
		if (!Operation(Child))
		{
			break;
		}
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
