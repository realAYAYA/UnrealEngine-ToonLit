// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DebugRenderSceneProxy.h: Useful scene proxy for rendering non performance-critical information.


=============================================================================*/

#include "DebugRenderSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Debug/DebugDrawService.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"

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

	FMaterialCache DefaultMaterialCache(Collector);
	FMaterialCache SolidMeshMaterialCache(Collector, /**bUseLight*/ true, SolidMeshMaterial.Get());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			GetDynamicMeshElementsForView(View, ViewIndex, ViewFamily, VisibilityMap, Collector, DefaultMaterialCache, SolidMeshMaterialCache);
		}
	}
}

void FDebugRenderSceneProxy::GetDynamicMeshElementsForView(const FSceneView* View, const int32 ViewIndex, const FSceneViewFamily& ViewFamily, const uint32 VisibilityMap, FMeshElementCollector& Collector, FMaterialCache& DefaultMaterialCache, FMaterialCache& SolidMeshMaterialCache) const
{
	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

	// Draw Lines
	const int32 LinesNum = Lines.Num();
	PDI->AddReserveLines(SDPG_World, LinesNum, false, false);
	for (const FDebugLine& Line : Lines)
	{
		Line.Draw(PDI);
	}

	// Draw Dashed Lines
	for (const FDashedLine& Dash : DashedLines)
	{
		Dash.Draw(PDI);
	}

	// Draw Arrows
	const uint32 ArrowsNum = ArrowLines.Num();
	PDI->AddReserveLines(SDPG_World, 5 * ArrowsNum, false, false);
	for (const FArrowLine& ArrowLine : ArrowLines)
	{
		ArrowLine.Draw(PDI, 8.f);
	}

	// Draw Stars
	for (const FWireStar& Star : Stars)
	{
		Star.Draw(PDI);
	}

	// Draw Cylinders
	for (const FWireCylinder& Cylinder : Cylinders)
	{
		Cylinder.Draw(PDI, (Cylinder.DrawTypeOverride != EDrawType::Invalid) ? Cylinder.DrawTypeOverride : DrawType, DrawAlpha, DefaultMaterialCache, ViewIndex, Collector);
	}

	// Draw Boxes
	for (const FDebugBox& Box : Boxes)
	{
		Box.Draw(PDI, (Box.DrawTypeOverride != EDrawType::Invalid) ? Box.DrawTypeOverride : DrawType, DrawAlpha, DefaultMaterialCache, ViewIndex, Collector);
	}

	// Draw Cones
	TArray<FVector> Verts;
	for (const FCone& Cone : Cones)
	{
		Cone.Draw(PDI, (Cone.DrawTypeOverride != EDrawType::Invalid) ? Cone.DrawTypeOverride : DrawType, DrawAlpha, DefaultMaterialCache, ViewIndex, Collector, &Verts);
	}

	// Draw spheres
	for (const FSphere& Sphere : Spheres)
	{
		if (PointInView(Sphere.Location, View))
		{
			Sphere.Draw(PDI, (Sphere.DrawTypeOverride != EDrawType::Invalid) ? Sphere.DrawTypeOverride : DrawType, DrawAlpha, DefaultMaterialCache, ViewIndex, Collector);
		}
	}

	// Draw Capsules
	for (const FCapsule& Capsule : Capsules)
	{
		if (PointInView(Capsule.Base, View))
		{
			Capsule.Draw(PDI, (Capsule.DrawTypeOverride != EDrawType::Invalid) ? Capsule.DrawTypeOverride : DrawType, DrawAlpha, DefaultMaterialCache, ViewIndex, Collector);
		}
	}

	// Draw Meshes
	for (const FMesh& Mesh : Meshes)
	{
		FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
		MeshBuilder.AddVertices(Mesh.Vertices);
		MeshBuilder.AddTriangles(Mesh.Indices);

		FMaterialCache& MeshMaterialCache = Mesh.Color.A == 255 ? SolidMeshMaterialCache : DefaultMaterialCache; 
		MeshBuilder.GetMesh(FMatrix::Identity, MeshMaterialCache[Mesh.Color], SDPG_World, false, false, ViewIndex, Collector);
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
	const FArrowLine ArrowLine = {Start, End, Color};
	ArrowLine.Draw(PDI, Mag);
}

FDebugRenderSceneProxy::FMaterialCache::FMaterialCache(FMeshElementCollector& InCollector, bool bUseLight, UMaterial* InMaterial)
	: Collector(InCollector)
	, SolidMeshMaterial(InMaterial)
	, bUseFakeLight(bUseLight)
{
}

FMaterialRenderProxy* FDebugRenderSceneProxy::FMaterialCache::operator[](FLinearColor Color)
{
	FMaterialRenderProxy* MeshColor = nullptr;
	const uint32 HashKey = GetTypeHashHelper(Color);
	if (MeshColorInstances.Contains(HashKey))
	{
		MeshColor = *MeshColorInstances.Find(HashKey);
	}
	else
	{
		if (bUseFakeLight && SolidMeshMaterial.IsValid())
		{
			MeshColor = &Collector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(
				SolidMeshMaterial->GetRenderProxy(),
				Color,
				"GizmoColor"
				);
		}
		else
		{
			MeshColor = &Collector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(GEngine->DebugMeshMaterial->GetRenderProxy(), Color);
		}

		MeshColorInstances.Add(HashKey, MeshColor);
	}

	return MeshColor;
}

void FDebugRenderSceneProxy::FDebugLine::Draw(FPrimitiveDrawInterface* PDI) const
{
	PDI->DrawLine(Start, End, Color, SDPG_World, Thickness, 0, Thickness > 0);
}

void FDebugRenderSceneProxy::FWireStar::Draw(FPrimitiveDrawInterface* PDI) const
{
	DrawWireStar(PDI, Position, Size, Color, SDPG_World);
}

void FDebugRenderSceneProxy::FArrowLine::Draw(FPrimitiveDrawInterface* PDI, const float Mag) const
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

void FDebugRenderSceneProxy::FDashedLine::Draw(FPrimitiveDrawInterface* PDI) const
{
	DrawDashedLine(PDI, Start, End, Color, DashSize, SDPG_World);
}

void FDebugRenderSceneProxy::FDebugBox::Draw(FPrimitiveDrawInterface* PDI, EDrawType InDrawType, uint32 InDrawAlpha, FMaterialCache& MaterialCache, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	if (InDrawType == SolidAndWireMeshes || InDrawType == WireMesh)
	{
		DrawWireBox(PDI, Transform.ToMatrixWithScale(), Box, Color, SDPG_World, InDrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
	}
	if (InDrawType == SolidAndWireMeshes || InDrawType == SolidMesh)
	{
		GetBoxMesh(FTransform(Box.GetCenter()).ToMatrixNoScale() * Transform.ToMatrixWithScale(), Box.GetExtent(), MaterialCache[Color.WithAlpha(InDrawAlpha * Color.A)], SDPG_World, ViewIndex, Collector);
	}
}

void FDebugRenderSceneProxy::FWireCylinder::Draw(FPrimitiveDrawInterface* PDI, EDrawType InDrawType, uint32 InDrawAlpha, FMaterialCache& MaterialCache, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	FVector XAxis, YAxis;
	Direction.FindBestAxisVectors(XAxis, YAxis);
	if (InDrawType == SolidAndWireMeshes || InDrawType == WireMesh)
	{
		DrawWireCylinder(PDI, Base, XAxis, YAxis, Direction, Color, Radius, HalfHeight, (InDrawType == SolidAndWireMeshes) ? 9 : 16, SDPG_World, InDrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
	}

	if (InDrawType == SolidAndWireMeshes || InDrawType == SolidMesh)
	{
		GetCylinderMesh(Base, XAxis, YAxis, Direction, Radius, HalfHeight, 16, MaterialCache[Color.WithAlpha(InDrawAlpha * Color.A)], SDPG_World, ViewIndex, Collector);
	}
}

void FDebugRenderSceneProxy::FCone::Draw(FPrimitiveDrawInterface* PDI, EDrawType InDrawType, uint32 InDrawAlpha, FMaterialCache& MaterialCache, int32 ViewIndex, FMeshElementCollector& Collector, TArray<FVector>* VertsCache) const
{
	if (InDrawType == SolidAndWireMeshes || InDrawType == WireMesh)
	{
		TArray<FVector> LocalVertsCache;
		TArray<FVector>& Verts = VertsCache != nullptr ? *VertsCache : LocalVertsCache;
		DrawWireCone(PDI, Verts, ConeToWorld, 1, Angle2, (InDrawType == SolidAndWireMeshes) ? 9 : 16, Color, SDPG_World, InDrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
	}
	if (InDrawType == SolidAndWireMeshes || InDrawType == SolidMesh)
	{
		GetConeMesh(ConeToWorld, Angle1, Angle2, 16, MaterialCache[Color.WithAlpha(InDrawAlpha * Color.A)], SDPG_World, ViewIndex, Collector);
	}
}


void FDebugRenderSceneProxy::FSphere::Draw(FPrimitiveDrawInterface* PDI, EDrawType InDrawType, uint32 InDrawAlpha, FMaterialCache& MaterialCache, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	if (InDrawType == SolidAndWireMeshes || InDrawType == WireMesh)
	{
		DrawWireSphere(PDI, Location, Color.WithAlpha(255), Radius, 20, SDPG_World, InDrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
	}
	if (InDrawType == SolidAndWireMeshes || InDrawType == SolidMesh)
	{
		GetSphereMesh(Location, FVector(Radius), 20, 7, MaterialCache[Color.WithAlpha(InDrawAlpha * Color.A)], SDPG_World, false, ViewIndex, Collector);
	}
}

void FDebugRenderSceneProxy::FCapsule::Draw(FPrimitiveDrawInterface* PDI, EDrawType InDrawType, uint32 InDrawAlpha, FMaterialCache& MaterialCache, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	if (InDrawType == SolidAndWireMeshes || InDrawType == WireMesh)
	{
		const float HalfAxis = FMath::Max<float>(HalfHeight - Radius, 1.f);
		const FVector BottomEnd = Base + Radius * Z;
		const FVector TopEnd = BottomEnd + (2 * HalfAxis) * Z;
		const float CylinderHalfHeight = (TopEnd - BottomEnd).Size() * 0.5;
		const FVector CylinderLocation = BottomEnd + CylinderHalfHeight * Z;
		DrawWireCapsule(PDI, CylinderLocation, X, Y, Z, Color, Radius, HalfHeight, (InDrawType == SolidAndWireMeshes) ? 9 : 16, SDPG_World, InDrawType == SolidAndWireMeshes ? 2 : 0, 0, true);
	}
	if (InDrawType == SolidAndWireMeshes || InDrawType == SolidMesh)
	{
		GetCapsuleMesh(Base, X, Y, Z, Color, Radius, HalfHeight, 16, MaterialCache[Color.WithAlpha(InDrawAlpha * Color.A)], SDPG_World, false, ViewIndex, Collector);
	}
}