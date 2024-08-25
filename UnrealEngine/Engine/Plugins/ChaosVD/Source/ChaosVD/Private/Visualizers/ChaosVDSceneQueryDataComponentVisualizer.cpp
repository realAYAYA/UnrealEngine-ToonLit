// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDSceneQueryDataComponentVisualizer.h"

#include "Actors/ChaosVDSceneQueryDataContainer.h"
#include "Components/ChaosVDSceneQueryDataComponent.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDScene.h"
#include "ChaosVDSceneQueryDataInspectorTab.h"
#include "ChaosVDTabsIDs.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "SceneView.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"
#include "Visualizers/IChaosVDParticleVisualizationDataProvider.h"
#include "Widgets/SChaosVDMainTab.h"

class UChaosVDEditorSettings;

IMPLEMENT_HIT_PROXY(HChaosVDSceneQueryProxy, HComponentVisProxy)

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void FChaosVDSceneQueryDataComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UChaosVDSceneQueryDataComponent* SceneQueryDataComponent = Cast<UChaosVDSceneQueryDataComponent>(Component);
	if (!SceneQueryDataComponent)
	{
		return;
	}

	const AChaosVDSceneQueryDataContainer* SceneQueryDataContainer = Cast<AChaosVDSceneQueryDataContainer>(SceneQueryDataComponent->GetOwner());
	if (!SceneQueryDataContainer)
	{
		return;
	}

	const TSharedPtr<FChaosVDScene> CVDScene = SceneQueryDataContainer->GetScene().Pin();
	if (!CVDScene)
	{
		return;
	}

	const TSharedPtr<FChaosVDGeometryBuilder> GeometryGenerator = CVDScene->GetGeometryGenerator();
	if (!GeometryGenerator)
	{
		return;
	}

	const TSharedPtr<FChaosVDRecording> CVDRecording = CVDScene->LoadedRecording;
	if (!CVDRecording)
	{
		return;
	}

	FChaosVDSceneQueryVisualizationDataContext VisualizationContext;
	VisualizationContext.CVDScene = SceneQueryDataContainer->GetScene();
	VisualizationContext.SpaceTransform = FTransform::Identity;
	VisualizationContext.GeometryGenerator = GeometryGenerator;

	if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
	{
		VisualizationContext.VisualizationFlags = EditorSettings->GlobalSceneQueriesVisualizationFlags;
	}

	if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::EnableDraw, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
	{
		// If Draw only selected Query is enabled, but no query is selected, just draw all queries

		const TSharedPtr<FChaosVDQueryDataWrapper> SelectedQuery = SceneQueryDataComponent->GetSelectedQueryHandle().GetQueryData().Pin();
		const bool bHasSelectedQuery = SelectedQuery.IsValid();
		if (bHasSelectedQuery && EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::OnlyDrawSelectedQuery, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
		{
			DrawSceneQuery(Component, View, PDI, CVDScene, CVDRecording, VisualizationContext, SelectedQuery);
		}
		else
		{
			for (const TSharedPtr<FChaosVDQueryDataWrapper>& Query : SceneQueryDataComponent->GetAllQueries())
			{
				DrawSceneQuery(Component, View, PDI, CVDScene, CVDRecording, VisualizationContext, Query);
			}
		}
	}
}

bool FChaosVDSceneQueryDataComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	const HChaosVDSceneQueryProxy* SceneQueryDataProxy = HitProxyCast<HChaosVDSceneQueryProxy>(VisProxy);
	if (SceneQueryDataProxy == nullptr)
	{
		return false;
	}
	
	if (const UChaosVDSceneQueryDataComponent* SQDataComponent = Cast<UChaosVDSceneQueryDataComponent>(SceneQueryDataProxy->Component.Get()))
	{
		// Bring the SQ Inspector into focus if available
		const TSharedPtr<SChaosVDMainTab> MainTabToolkitHost = InViewportClient->GetModeTools() ? StaticCastSharedPtr<SChaosVDMainTab>(InViewportClient->GetModeTools()->GetToolkitHost()) : nullptr;
		if (const TSharedPtr<FTabManager> TabManager = MainTabToolkitHost ? MainTabToolkitHost->GetTabManager() : nullptr)
		{
			TabManager->TryInvokeTab(FChaosVDTabID::SceneQueryDataDetails);
		}

		const_cast<UChaosVDSceneQueryDataComponent*>(SQDataComponent)->SelectQuery(SceneQueryDataProxy->DataSelectionHandle);

		return true;
	}

	return false;
}

void FChaosVDSceneQueryDataComponentVisualizer::DrawLineTraceQuery(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	PDI->SetHitProxy(new HChaosVDSceneQueryProxy(Component, VisualizationContext.DataSelectionHandle));

	const FText DebugText = FText::FormatOrdered(LOCTEXT("LineTraceDebugDrawText", "Type: Line Trace \n Tag {1} \n Owner Tag {2}"), FText::AsCultureInvariant(SceneQueryData.CollisionQueryParams.TraceTag.ToString()), FText::AsCultureInvariant(SceneQueryData.CollisionQueryParams.OwnerTag.ToString()));

	FVector EndLocationToDraw;
	if (SceneQueryData.SQVisitData.IsValidIndex(SceneQueryData.CurrentVisitIndex))
	{
		const FChaosVDQueryVisitStep& SQVisitData = SceneQueryData.SQVisitData[SceneQueryData.CurrentVisitIndex];
		EndLocationToDraw = SceneQueryData.StartLocation + SQVisitData.QueryFastData.Dir * SQVisitData.QueryFastData.CurrentLength;
	}
	else
	{
		// Fallback to draw the end position
		EndLocationToDraw = SceneQueryData.EndLocation;
	}

	FChaosVDDebugDrawUtils::DrawArrowVector(PDI, SceneQueryData.StartLocation, EndLocationToDraw, DebugText, VisualizationContext.DebugDrawColor, SDPG_Foreground);

	PDI->SetHitProxy(nullptr);

	if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::DrawHits, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
	{
		DrawHits(Component, SceneQueryData, PDI, VisualizationContext.HitColor, VisualizationContext);
	}
}

void FChaosVDSceneQueryDataComponentVisualizer::DrawOverlapQuery(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	PDI->SetHitProxy(new HChaosVDSceneQueryProxy(Component, VisualizationContext.DataSelectionHandle));

	const Chaos::FConstImplicitObjectPtr InputShapePtr = VisualizationContext.InputGeometry;
	if (ensure(InputShapePtr))
	{
		const FText DebugText = FText::FormatOrdered(LOCTEXT("OverlapDebugDrawText", "Type: Overlap \n Tag {1} \n Owner Tag {2}"), FText::AsCultureInvariant(SceneQueryData.CollisionQueryParams.TraceTag.ToString()), FText::AsCultureInvariant(SceneQueryData.CollisionQueryParams.OwnerTag.ToString()));
		FChaosVDDebugDrawUtils::DrawImplicitObject(PDI, VisualizationContext.GeometryGenerator.Pin(), InputShapePtr, FTransform(SceneQueryData.GeometryOrientation, SceneQueryData.StartLocation), VisualizationContext.DebugDrawColor, DebugText, ESceneDepthPriorityGroup::SDPG_Foreground);
	}

	PDI->SetHitProxy(nullptr);

	if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::DrawHits, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
	{
		DrawHits(Component, SceneQueryData, PDI, VisualizationContext.HitColor, VisualizationContext);
	}
}

void FChaosVDSceneQueryDataComponentVisualizer::DrawSweepQuery(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	PDI->SetHitProxy(new HChaosVDSceneQueryProxy(Component, VisualizationContext.DataSelectionHandle));

	const Chaos::FConstImplicitObjectPtr InputShapePtr = VisualizationContext.InputGeometry;
	if (ensure(InputShapePtr))
	{
		FChaosVDDebugDrawUtils::DrawImplicitObject(PDI, VisualizationContext.GeometryGenerator.Pin(), InputShapePtr, FTransform(SceneQueryData.GeometryOrientation, SceneQueryData.StartLocation), VisualizationContext.DebugDrawColor, FText::GetEmpty(), ESceneDepthPriorityGroup::SDPG_Foreground);

		FVector EndLocationToDraw;
		if (SceneQueryData.SQVisitData.IsValidIndex(SceneQueryData.CurrentVisitIndex))
		{
			const FChaosVDQueryVisitStep& SQVisitData = SceneQueryData.SQVisitData[SceneQueryData.CurrentVisitIndex];
			EndLocationToDraw = SceneQueryData.StartLocation + SQVisitData.QueryFastData.Dir * SQVisitData.QueryFastData.CurrentLength;
		}
		else
		{
			// Fallback to draw the end position
			EndLocationToDraw = SceneQueryData.EndLocation;
		}

		FChaosVDDebugDrawUtils::DrawImplicitObject(PDI, VisualizationContext.GeometryGenerator.Pin(), InputShapePtr, FTransform(SceneQueryData.GeometryOrientation, EndLocationToDraw), VisualizationContext.DebugDrawColor, FText::GetEmpty(), ESceneDepthPriorityGroup::SDPG_Foreground);
	}

	PDI->SetHitProxy(nullptr);

	const FText DebugText = FText::FormatOrdered(LOCTEXT("SweepDebugDrawText", "Type: Sweep \n Tag {1} \n Owner Tag {2}"), FText::AsCultureInvariant(SceneQueryData.CollisionQueryParams.TraceTag.ToString()), FText::AsCultureInvariant(SceneQueryData.CollisionQueryParams.OwnerTag.ToString()));

	FChaosVDDebugDrawUtils::DrawArrowVector(PDI, SceneQueryData.StartLocation, SceneQueryData.EndLocation, DebugText, VisualizationContext.DebugDrawColor, SDPG_Foreground);

	if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::DrawHits, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
	{
		DrawHits(Component, SceneQueryData, PDI, VisualizationContext.HitColor, VisualizationContext);
	}
}

void FChaosVDSceneQueryDataComponentVisualizer::DrawHits(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FPrimitiveDrawInterface* PDI, const FColor& InColor, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext)
{
	for (int32 SQVisitIndex = 0; SQVisitIndex < SceneQueryData.SQVisitData.Num(); ++SQVisitIndex)
	{
		const FChaosVDQueryVisitStep& SQVisitData = SceneQueryData.SQVisitData[SQVisitIndex];
		if (!SQVisitData.HitData.HasValidData())
		{
			continue;
		}

		VisualizationContext.DataSelectionHandle = FChaosVDSceneQuerySelectionHandle(VisualizationContext.DataSelectionHandle.GetQueryData(), SQVisitIndex);

		PDI->SetHitProxy(new HChaosVDSceneQueryProxy(Component, VisualizationContext.DataSelectionHandle));

		const FText HitPointDebugText = FText::FormatOrdered(LOCTEXT("SceneQueryHitDebugText", "Distance {0} \n Face Index {1} \n "), SQVisitData.HitData.Distance, SQVisitData.HitData.FaceIdx);
		static FText HitFaceNormalDebugText = FText::AsCultureInvariant(TEXT("Hit Face Normal"));
		static FText HitWorldNormalDebugText = FText::AsCultureInvariant(TEXT("Hit World Normal"));

		const FMatrix Axes = FRotationMatrix::MakeFromX(SQVisitData.HitData.WorldNormal);
		constexpr float Thickness = 5.0f;
		constexpr float CircleRadius = 5.0f;
		constexpr int32 CircleSegments = 12;
		constexpr float NormalScale = 10.5f;
		FChaosVDDebugDrawUtils::DrawCircle(PDI, SQVisitData.HitData.WorldPosition, CircleRadius, CircleSegments, InColor, Thickness, Axes.GetUnitAxis(EAxis::Y), Axes.GetUnitAxis(EAxis::Z), HitPointDebugText,  ESceneDepthPriorityGroup::SDPG_Foreground);
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, SQVisitData.HitData.WorldPosition, SQVisitData.HitData.WorldPosition + SQVisitData.HitData.FaceNormal * NormalScale, HitFaceNormalDebugText, (FLinearColor(InColor) * 0.65f).ToFColorSRGB(), ESceneDepthPriorityGroup::SDPG_Foreground);

		// Hit Face Normal is not used in line traces
		if (SceneQueryData.Type != EChaosVDSceneQueryType::RayCast)
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, SQVisitData.HitData.WorldPosition, SQVisitData.HitData.WorldPosition + SQVisitData.HitData.WorldNormal * NormalScale, HitWorldNormalDebugText, InColor, ESceneDepthPriorityGroup::SDPG_Foreground);
		}

		if (SQVisitData.bIsSelectedInEditor)
		{
			// We don't have an easy way to show something is selected with debug draw
			// but 3D box surrounding the hit is better than nothing
			FTransform SelectionBoxTransform;
			SelectionBoxTransform.SetRotation(FRotationMatrix::MakeFromZ(SQVisitData.HitData.WorldNormal).ToQuat());
			SelectionBoxTransform.SetLocation(SQVisitData.HitData.WorldPosition);

			// The Selection box should be a bit bigger than the configured circle radius for the debug draw hit
			constexpr float HitSelectionBoxSize = CircleRadius * 1.2f;

			FVector SelectionBoxExtents(HitSelectionBoxSize,HitSelectionBoxSize,HitSelectionBoxSize);
			FChaosVDDebugDrawUtils::DrawBox(PDI, SelectionBoxExtents, FColor::Yellow, SelectionBoxTransform, FText::GetEmpty(), ESceneDepthPriorityGroup::SDPG_Foreground);
		}

		PDI->SetHitProxy(nullptr);
	}
}

bool FChaosVDSceneQueryDataComponentVisualizer::HasEndLocation(const FChaosVDQueryDataWrapper& SceneQueryData) const
{
	return SceneQueryData.Type != EChaosVDSceneQueryType::Sweep;
}

void FChaosVDSceneQueryDataComponentVisualizer::DrawSceneQuery(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI, const TSharedPtr<FChaosVDScene>& CVDScene, const TSharedPtr<FChaosVDRecording>& CVDRecording, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const TSharedPtr<FChaosVDQueryDataWrapper>& Query)
{
	// Reset query Specify context values
	VisualizationContext.InputGeometry = nullptr;
	VisualizationContext.DataSelectionHandle = FChaosVDSceneQuerySelectionHandle(nullptr, INDEX_NONE);

	if (!Query)
	{
		return;
	}

	AChaosVDSolverInfoActor* SolverInfoActor = CVDScene->GetSolverInfoActor(Query->WorldSolverID);
	if (!SolverInfoActor)
	{
		return;
	}
	
	if (!SolverInfoActor->IsVisible())
	{
		return;
	}

	const bool bHideEmptyQueries = EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::HideEmptyQueries, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags));
	if (bHideEmptyQueries && Query->SQVisitData.IsEmpty())
	{
		return;
	}

	const bool bHideSubQueries = EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::HideSubQueries, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags));
	if (bHideSubQueries && Query->ParentQueryID != INDEX_NONE)
	{
		return;
	}

	//TODO: Should we try to calculate actual bounds?
	constexpr float MinVisibleRadius = 100.0f;
	const float QueryHalfDistance = (Query->EndLocation - Query->StartLocation).Size() * 0.5;
	const float VisibleRadius = FMath::Max(QueryHalfDistance, MinVisibleRadius);
	if (!View->ViewFrustum.IntersectSphere(Query->StartLocation, VisibleRadius))
	{
		// If this query location is not even visible, just ignore it.
		return;
	}

	if (const Chaos::FConstImplicitObjectPtr* InputShapePtrPtr = CVDRecording->GetGeometryMap().Find(Query->InputGeometryKey))
	{
		VisualizationContext.InputGeometry = *InputShapePtrPtr;
	}

	VisualizationContext.DataSelectionHandle = FChaosVDSceneQuerySelectionHandle(Query, INDEX_NONE);
	VisualizationContext.GenerateColor(Query->ID, Query->bIsSelectedInEditor);

	switch (Query->Type)
	{
	case EChaosVDSceneQueryType::RayCast:
		{
			if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::DrawLineTraceQueries, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
			{
				DrawLineTraceQuery(Component, *Query, VisualizationContext, View, PDI);
			}
			break;
		}
	case EChaosVDSceneQueryType::Overlap:
		{
			if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::DrawOverlapQueries, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
			{
				DrawOverlapQuery(Component, *Query, VisualizationContext, View, PDI);
			}
			break;
		}
	case EChaosVDSceneQueryType::Sweep:
		{
			if (EnumHasAnyFlags(EChaosVDSceneQueryVisualizationFlags::DrawSweepQueries, static_cast<EChaosVDSceneQueryVisualizationFlags>(VisualizationContext.VisualizationFlags)))
			{
				DrawSweepQuery(Component, *Query, VisualizationContext, View, PDI);
			}
			break;
		}
	default:
		break;
	}
}

#undef LOCTEXT_NAMESPACE
