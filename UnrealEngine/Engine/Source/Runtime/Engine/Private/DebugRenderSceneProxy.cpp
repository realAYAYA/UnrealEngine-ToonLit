// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DebugRenderSceneProxy.h: Useful scene proxy for rendering non performance-critical information.


=============================================================================*/

#include "DebugRenderSceneProxy.h"
#include "SceneManagement.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Debug/DebugDrawService.h"

// FPrimitiveSceneProxy interface.

FDebugRenderSceneProxy::FDebugRenderSceneProxy(const UPrimitiveComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, ViewFlagIndex(uint32(FEngineShowFlags::FindIndexByName(TEXT("Game"))))
	, TextWithoutShadowDistance(1500)
	, ViewFlagName(TEXT("Game"))
	, DrawType(WireMesh)
	, DrawAlpha(100)
{
}

void FDebugDrawDelegateHelper::RegisterDebugDrawDelegateInternal()
{
	// note that it's possible at this point for State == RegisteredState since RegisterDebugDrawDelegateInternal can get 
	// called in multiple scenarios, most notably blueprint recompilation or changing level visibility in the editor.
	if (State == InitializedState)
	{
		DebugTextDrawingDelegate = FDebugDrawDelegate::CreateRaw(this, &FDebugDrawDelegateHelper::DrawDebugLabels);
		DebugTextDrawingDelegateHandle = UDebugDrawService::Register(*ViewFlagName, DebugTextDrawingDelegate);
		State = RegisteredState;
	}
}

void FDebugDrawDelegateHelper::RequestRegisterDebugDrawDelegate(FRegisterComponentContext* Context)
{
	bDeferredRegister = Context != nullptr;

	if (!bDeferredRegister)
	{
		RegisterDebugDrawDelegateInternal();
	}
}

void FDebugDrawDelegateHelper::ProcessDeferredRegister()
{
	if (bDeferredRegister)
	{
		RegisterDebugDrawDelegateInternal();
		bDeferredRegister = false;
	}
}

void FDebugDrawDelegateHelper::UnregisterDebugDrawDelegate()
{
	// note that it's possible at this point for State == InitializedState since UnregisterDebugDrawDelegate can get 
	// called in multiple scenarios, most notably blueprint recompilation or changing level visibility in the editor.
	if (State == RegisteredState)
	{
		check(DebugTextDrawingDelegate.IsBound());
		UDebugDrawService::Unregister(DebugTextDrawingDelegateHandle);
		State = InitializedState;
	}
}

void  FDebugDrawDelegateHelper::ReregisterDebugDrawDelegate()
{
	ensureMsgf(State != UndefinedState, TEXT("DrawDelegate is in an invalid State: %i !"), State);
	if (State == RegisteredState)
	{
		UnregisterDebugDrawDelegate();
		RegisterDebugDrawDelegateInternal();
	}
}

uint32 FDebugRenderSceneProxy::GetAllocatedSize(void) const 
{ 
	return	FPrimitiveSceneProxy::GetAllocatedSize() + 
		Cylinders.GetAllocatedSize() + 
		ArrowLines.GetAllocatedSize() + 
		Stars.GetAllocatedSize() + 
		DashedLines.GetAllocatedSize() + 
		Lines.GetAllocatedSize() + 
		Boxes.GetAllocatedSize() + 
		Spheres.GetAllocatedSize() +
		Texts.GetAllocatedSize();
}


void FDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController*)
{
	const FColor OldDrawColor = Canvas->DrawColor;
	const FFontRenderInfo FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
	const FFontRenderInfo FontRenderInfoWithShadow = Canvas->CreateFontRenderInfo(true, true);

	Canvas->SetDrawColor(FColor::White);

	UFont* RenderFont = GEngine->GetSmallFont();

	const FSceneView* View = Canvas->SceneView;
	for (auto It = Texts.CreateConstIterator(); It; ++It)
	{
		if (FDebugRenderSceneProxy::PointInView(It->Location, View))
		{
			const FVector ScreenLoc = Canvas->Project(It->Location);
			const FFontRenderInfo& FontInfo = TextWithoutShadowDistance >= 0 ? (FDebugRenderSceneProxy::PointInRange(It->Location, View, TextWithoutShadowDistance) ? FontRenderInfoWithShadow : FontRenderInfo) : FontRenderInfo;
			Canvas->SetDrawColor(It->Color);
			Canvas->DrawText(RenderFont, It->Text, ScreenLoc.X, ScreenLoc.Y, 1, 1, FontInfo);
		}
	}

	Canvas->SetDrawColor(OldDrawColor);
}

SIZE_T FDebugRenderSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FDebugRenderSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const 
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_DebugRenderSceneProxy_GetDynamicMeshElements );

	// Draw solid spheres
	struct FMaterialCache
	{
		FMaterialCache()
			: bUseFakeLight(false)
		{}

		FMaterialRenderProxy* operator[](FLinearColor Color)
		{
			FMaterialRenderProxy* MeshColor = NULL;
			const uint32 HashKey = ::GetTypeHash(Color);
			if (MeshColorInstances.Contains(HashKey))
			{
				MeshColor = *MeshColorInstances.Find(HashKey);
			}
			else
			{
				if (bUseFakeLight && SolidMeshMaterial.IsValid())
				{
					
					MeshColor = &Collector->AllocateOneFrameResource<FColoredMaterialRenderProxy>(
						SolidMeshMaterial->GetRenderProxy(),
						Color,
						"GizmoColor"
						);
				}
				else
				{
					MeshColor = &Collector->AllocateOneFrameResource<FColoredMaterialRenderProxy>(GEngine->DebugMeshMaterial->GetRenderProxy(), Color);
				}

				MeshColorInstances.Add(HashKey, MeshColor);
			}

			return MeshColor;
		}

		void Init(FMeshElementCollector& InCollector, bool UseLight = false, class UMaterial* InMaterial = nullptr)
		{
			Collector = &InCollector;
			bUseFakeLight = UseLight;
			SolidMeshMaterial = InMaterial;
		}

		FMeshElementCollector* Collector = nullptr;
		TMap<uint32, FMaterialRenderProxy*> MeshColorInstances;
		TWeakObjectPtr<class UMaterial> SolidMeshMaterial;
		bool bUseFakeLight;
	};

	FMaterialCache MaterialCache[2];
	MaterialCache[0].Init(Collector);
	MaterialCache[1].Init(Collector, true, SolidMeshMaterial.Get());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			// Draw Lines
			const int32 LinesNum = Lines.Num();
			PDI->AddReserveLines(SDPG_World, LinesNum, false, false);
			for (const auto& CurrentLine : Lines)
			{
				PDI->DrawLine(CurrentLine.Start, CurrentLine.End, CurrentLine.Color, SDPG_World, CurrentLine.Thickness, 0, CurrentLine.Thickness > 0);
			}

			// Draw Dashed Lines
			for(int32 DashIdx=0; DashIdx<DashedLines.Num(); DashIdx++)
			{
				const FDashedLine& Dash = DashedLines[DashIdx];

				DrawDashedLine(PDI, Dash.Start, Dash.End, Dash.Color, Dash.DashSize, SDPG_World);
			}

			// Draw Arrows
			const uint32 ArrowsNum = ArrowLines.Num();
			PDI->AddReserveLines(SDPG_World, 5 * ArrowsNum, false, false);
			for (const auto& CurrentArrow : ArrowLines)
			{
				DrawLineArrow(PDI, CurrentArrow.Start, CurrentArrow.End, CurrentArrow.Color, 8.0f);
			}

			// Draw Stars
			for(int32 StarIdx=0; StarIdx<Stars.Num(); StarIdx++)
			{
				const FWireStar& Star = Stars[StarIdx];

				DrawWireStar(PDI, Star.Position, Star.Size, Star.Color, SDPG_World);
			}

			// Draw Cylinders
			for(const auto& Cylinder : Cylinders)
			{
				if (DrawType == SolidAndWireMeshes || DrawType == WireMesh)
				{
					DrawWireCylinder(PDI, Cylinder.Base, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Cylinder.Color, Cylinder.Radius, Cylinder.HalfHeight, (DrawType == SolidAndWireMeshes) ? 9 : 16, SDPG_World, DrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
				}

				if (DrawType == SolidAndWireMeshes || DrawType == SolidMesh)
				{
					GetCylinderMesh(Cylinder.Base, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Cylinder.Radius, Cylinder.HalfHeight, 16, MaterialCache[0][Cylinder.Color.WithAlpha(DrawAlpha)], SDPG_World, ViewIndex, Collector);
				}
			}

			// Draw Boxes
			for(const auto& Box :  Boxes)
			{
				if (DrawType == SolidAndWireMeshes || DrawType == WireMesh)
				{
					DrawWireBox(PDI, Box.Transform.ToMatrixWithScale(), Box.Box, Box.Color, SDPG_World, DrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
				}
				if (DrawType == SolidAndWireMeshes || DrawType == SolidMesh)
				{
					GetBoxMesh(FTransform(Box.Box.GetCenter()).ToMatrixNoScale() * Box.Transform.ToMatrixWithScale(), Box.Box.GetExtent(), MaterialCache[0][Box.Color.WithAlpha(DrawAlpha)], SDPG_World, ViewIndex, Collector);
				}
			}

			// Draw Boxes
			TArray<FVector> Verts;
			for (auto& CurrentCone : Cones)
			{
				if (DrawType == SolidAndWireMeshes || DrawType == WireMesh)
				{
					DrawWireCone(PDI, Verts, CurrentCone.ConeToWorld, 1, CurrentCone.Angle2, (DrawType == SolidAndWireMeshes) ? 9 : 16, CurrentCone.Color, SDPG_World, DrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
				}
				if (DrawType == SolidAndWireMeshes || DrawType == SolidMesh)
				{
					GetConeMesh(CurrentCone.ConeToWorld, CurrentCone.Angle1, CurrentCone.Angle2, 16, MaterialCache[0][CurrentCone.Color.WithAlpha(DrawAlpha)], SDPG_World, ViewIndex, Collector);
				}
			}

			for (auto It = Spheres.CreateConstIterator(); It; ++It)
			{
				if (PointInView(It->Location, View))
				{
					if (DrawType == SolidAndWireMeshes || DrawType == WireMesh)
					{
						DrawWireSphere(PDI, It->Location, It->Color.WithAlpha(255), It->Radius, 20, SDPG_World, DrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
					}
					if (DrawType == SolidAndWireMeshes || DrawType == SolidMesh)
					{
						GetSphereMesh(It->Location, FVector(It->Radius), 20, 7, MaterialCache[0][It->Color.WithAlpha(DrawAlpha)], SDPG_World, false, ViewIndex, Collector);
					}
				}
			}

			for (auto It = Capsules.CreateConstIterator(); It; ++It)
			{
				if (PointInView(It->Base, View))
				{
					if (DrawType == SolidAndWireMeshes || DrawType == WireMesh)
					{
						const float HalfAxis = FMath::Max<float>(It->HalfHeight - It->Radius, 1.f);
						const FVector BottomEnd = It->Base + It->Radius * It->Z;
						const FVector TopEnd = BottomEnd + (2 * HalfAxis) * It->Z;
						const float CylinderHalfHeight = (TopEnd - BottomEnd).Size() * 0.5;
						const FVector CylinderLocation = BottomEnd + CylinderHalfHeight * It->Z;
						DrawWireCapsule(PDI, CylinderLocation, It->X, It->Y, It->Z, It->Color, It->Radius, It->HalfHeight, (DrawType == SolidAndWireMeshes) ? 9 : 16, SDPG_World, DrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
					}
					if (DrawType == SolidAndWireMeshes || DrawType == SolidMesh)
					{
						GetCapsuleMesh(It->Base, It->X, It->Y, It->Z, It->Color, It->Radius, It->HalfHeight, 16, MaterialCache[0][It->Color.WithAlpha(DrawAlpha)], SDPG_World, false, ViewIndex, Collector);
					}
				}
			}

			for (const auto& Mesh : Meshes)
			{
				FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
				MeshBuilder.AddVertices(Mesh.Vertices);
				MeshBuilder.AddTriangles(Mesh.Indices);

				MeshBuilder.GetMesh(FMatrix::Identity, MaterialCache[Mesh.Color.A == 255 ? 1 : 0][Mesh.Color], SDPG_World, false, false, ViewIndex, Collector);
			}

		}
	}
}

/**
* Draws a line with an arrow at the end.
*
* @param Start		Starting point of the line.
* @param End		Ending point of the line.
* @param Color		Color of the line.
* @param Mag		Size of the arrow.
*/
void FDebugRenderSceneProxy::DrawLineArrow(FPrimitiveDrawInterface* PDI,const FVector &Start,const FVector &End,const FColor &Color,float Mag) const
{
	// draw a pretty arrow
	FVector Dir = End - Start;
	const float DirMag = Dir.Size();
	Dir /= DirMag;
	FVector YAxis, ZAxis;
	Dir.FindBestAxisVectors(YAxis,ZAxis);
	FMatrix ArrowTM(Dir,YAxis,ZAxis,Start);
	DrawDirectionalArrow(PDI,ArrowTM,Color,DirMag,Mag,SDPG_World);
}
