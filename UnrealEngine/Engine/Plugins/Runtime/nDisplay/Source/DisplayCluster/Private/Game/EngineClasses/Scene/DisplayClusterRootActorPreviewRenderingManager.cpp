// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRootActorPreviewRenderingManager.h"
#include "DisplayClusterRootActor.h"

#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"

///////////////////////////////////////////////////////////////////
int32 GDisplayClusterRootActorPreviewRenderingEnable = 1;
static FAutoConsoleVariableRef CVarDisplayClusterRootActorPreviewRenderingEnable(
	TEXT("nDisplay.preview.render.enable"),
	GDisplayClusterRootActorPreviewRenderingEnable,
	TEXT("Globally enable DCRA preview rendering\n")
	TEXT("0 : Disable preview render \n"),
	ECVF_Default
);

int32 GDisplayClusterRootActorPreviewRenderingOptimization = 1;
static FAutoConsoleVariableRef CVarDisplayClusterRootActorPreviewRenderingOptimization(
	TEXT("nDisplay.preview.render.optimize"),
	GDisplayClusterRootActorPreviewRenderingOptimization,
	TEXT("Optimize DCRA preview rendering only 1 DCRA per frame\n")
	TEXT("0 : Disable optimization\n"),
	ECVF_Default
);

///////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::DCRAPreview
{
	/**
	 * We can skip the preview rendering for invisible DCRA objects:
	 * minimized or inactive window, invisible tab, etc.
	 */
	static bool IsRootActorVisible(ADisplayClusterRootActor* InRootActor)
	{
		check(InRootActor);

		// Todo: Add logic to get DCRA visibility.

		return true;
	}

	static FDisplayClusterRootActorPreviewRenderingManager& GetPreviewRenderingManager()
	{
		static FDisplayClusterRootActorPreviewRenderingManager DCRAPreviewManagerSingletonInstance;

		return DCRAPreviewManagerSingletonInstance;
	}

};
using namespace UE::DisplayCluster::DCRAPreview;

//------------------------------------------------------------------
// FDisplayClusterRootActorPreviewObject
//------------------------------------------------------------------
void FDisplayClusterRootActorPreviewObject::HandleRenderRequest()
{
	if (!GDisplayClusterRootActorPreviewRenderingEnable)
	{
		return;
	}

	if (GDisplayClusterRootActorPreviewRenderingOptimization)
	{
		// Request for preview render
		bRenderPreviewRequested = true;
	}
	else
	{
		RenderPreview();
	}
}

void FDisplayClusterRootActorPreviewObject::RenderPreview()
{
#if WITH_EDITOR
	if (ADisplayClusterRootActor* RootActor = GetRootActor())
	{
		RootActor->RenderPreview_Editor();
	}
#endif
}

//------------------------------------------------------------------
// FDisplayClusterRootActorPreviewRenderingManager
//------------------------------------------------------------------
void FDisplayClusterRootActorPreviewRenderingManager::HandleEvent(const EDisplayClusterRootActorPreviewEvent InDCRAEvent, ADisplayClusterRootActor* InRootActor)
{
	GetPreviewRenderingManager().HandleEventImpl(InDCRAEvent, InRootActor);
}

void FDisplayClusterRootActorPreviewRenderingManager::HandleEventImpl(const EDisplayClusterRootActorPreviewEvent InDCRAEvent, ADisplayClusterRootActor* InRootActor)
{
	const int32 PreviewObjectIndex = PreviewObjects.IndexOfByPredicate([InRootActor](const FDisplayClusterRootActorPreviewObject& PreviewObjectIt)
		{
			return PreviewObjectIt.IsEqual(InRootActor);
		});

	switch (InDCRAEvent)
	{
	case EDisplayClusterRootActorPreviewEvent::Create:
		if (PreviewObjectIndex == INDEX_NONE)
		{
			PreviewObjects.Add(FDisplayClusterRootActorPreviewObject(InRootActor));
		}
		break;

	case EDisplayClusterRootActorPreviewEvent::Remove:
		if (PreviewObjectIndex != INDEX_NONE)
		{
			PreviewObjects.RemoveAt(PreviewObjectIndex);
		}
		break;

	case EDisplayClusterRootActorPreviewEvent::Render:
		if (PreviewObjectIndex != INDEX_NONE)
		{
			PreviewObjects[PreviewObjectIndex].HandleRenderRequest();
		}
		else
		{
			// Create new ref by render request
			const int32 NewIndex = PreviewObjects.Add(FDisplayClusterRootActorPreviewObject(InRootActor));
			PreviewObjects[NewIndex].HandleRenderRequest();
		}
		break;

	default:
		break;
	}

	UpdateTickableGameObject();
}

void FDisplayClusterRootActorPreviewRenderingManager::Tick(float DeltaTime)
{
	if (!GDisplayClusterRootActorPreviewRenderingOptimization || !GDisplayClusterRootActorPreviewRenderingEnable)
	{
		return;
	}

	// Render one DCRA per frame:
	int32 NextPreviewObjectIndex = GetNextPreviewObject();
	if (NextPreviewObjectIndex == INDEX_NONE)
	{
		// Reset render cycle
		BeginNewRenderCycle();

		// start new cycle
		NextPreviewObjectIndex = GetNextPreviewObject();
	}

	if (NextPreviewObjectIndex != INDEX_NONE)
	{
		// Render this DCRA
		PreviewObjects[NextPreviewObjectIndex].RenderPreview();
	}
}

int32 FDisplayClusterRootActorPreviewRenderingManager::GetNextPreviewObject()
{
	int32 OutIndex = INDEX_NONE;

	// rebuild list - remove invalid DCRA's
	TArray<FDisplayClusterRootActorPreviewObject> NewPreviewObjects;
	NewPreviewObjects.Reserve(PreviewObjects.Num());

	for (FDisplayClusterRootActorPreviewObject& PreviewObjectIt : PreviewObjects)
	{
		if (PreviewObjectIt.IsValid())
		{
			if (OutIndex == INDEX_NONE && PreviewObjectIt.ShouldRenderPreview())
			{
				// Mark this object as rendered
				PreviewObjectIt.MarkAsRendered();

				// Render only visible DCRA objects
				if (IsRootActorVisible(PreviewObjectIt.GetRootActor()))
				{
					OutIndex = NewPreviewObjects.Num();
				}
			}

			NewPreviewObjects.Add(PreviewObjectIt);
		}
	}

	PreviewObjects = NewPreviewObjects;

	return OutIndex;
}

void FDisplayClusterRootActorPreviewRenderingManager::BeginNewRenderCycle()
{
	for (FDisplayClusterRootActorPreviewObject& PreviewObject : PreviewObjects)
	{
		if (PreviewObject.ShouldRenderPreview())
		{
			// This object was not render in the current loop
			return;
		}
	}

	for (FDisplayClusterRootActorPreviewObject& PreviewObject : PreviewObjects)
	{
		// Allow to render again
		PreviewObject.MarkAsNotRendered();
	}
}

//---------------------------------------------------------------------------------
// FDisplayClusterPreviewTickableGameObject
//---------------------------------------------------------------------------------
class FDisplayClusterPreviewTickableGameObject : public FTickableGameObject
{
public:
	virtual bool IsTickableInEditor() const override { return true; }

	virtual void Tick(float DeltaTime) override
	{
		GetPreviewRenderingManager().Tick(DeltaTime);
	}

	virtual ETickableTickType GetTickableTickType() const override
	{ 
		return ETickableTickType::Always;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDisplayClusterPreviewTickableGameObject, STATGROUP_Tickables);
	}
};

void FDisplayClusterRootActorPreviewRenderingManager::UpdateTickableGameObject()
{
	if (PreviewObjects.IsEmpty())
	{
		// delete when no DCRAs
		TickableGameObject.Reset();
	}
	else
	{
		// When we have at least one DCRA, a tickable object is created
		if (!TickableGameObject.IsValid())
		{
			TickableGameObject = MakeUnique<FDisplayClusterPreviewTickableGameObject>();
		}
	}
}
