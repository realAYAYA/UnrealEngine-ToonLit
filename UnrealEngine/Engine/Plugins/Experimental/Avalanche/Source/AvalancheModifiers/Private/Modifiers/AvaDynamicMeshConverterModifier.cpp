// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaDynamicMeshConverterModifier.h"

#include "Async/Async.h"
#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"
#include "Engine/StaticMesh.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "ProceduralMeshComponent.h"
#include "Engine/CollisionProfile.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dialogs/DlgPickAssetPath.h"
#endif

#define LOCTEXT_NAMESPACE "AvaDynamicMeshConverterModifier"

FAvaDynamicMeshConverterModifierComponentState::FAvaDynamicMeshConverterModifierComponentState(UPrimitiveComponent* InPrimitiveComponent)
	: Component(InPrimitiveComponent)
{
	if (Component.IsValid())
	{
		if (const AActor* ComponentOwner = Component->GetOwner())
		{
#if WITH_EDITOR
			bActorHiddenInGame = ComponentOwner->IsHidden();
			bActorHiddenInEditor = ComponentOwner->IsTemporarilyHiddenInEditor();
#endif
			if (const USceneComponent* RootComponent = ComponentOwner->GetRootComponent())
			{
				bComponentVisible = RootComponent->IsVisible();
				bComponentHiddenInGame = RootComponent->bHiddenInGame;
			}
		}
	}
}

void UAvaDynamicMeshConverterModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("DynamicMeshConverter"));
	InMetadata.SetCategory(TEXT("Conversion"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Converts a non dynamic mesh actor into a dynamic mesh"));
#endif
	InMetadata.SetCompatibilityRule([](const AActor* InActor)->bool
	{
		return InActor && !InActor->FindComponentByClass<UDynamicMeshComponent>();
	});
}

void UAvaDynamicMeshConverterModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddDynamicMeshComponent();
	bConvertMesh = true;
}

void UAvaDynamicMeshConverterModifier::OnModifierEnabled(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierEnabled(InReason);

	// On load update state by converting source again
	if (InReason == EActorModifierCoreEnableReason::Load)
	{
		bConvertMesh = true;
		MarkModifierDirty();
	}
}

void UAvaDynamicMeshConverterModifier::RestorePreState()
{
	UAvaGeometryBaseModifier::RestorePreState();
	
	for (FAvaDynamicMeshConverterModifierComponentState& ConvertedComponent : ConvertedComponents)
	{
		if (ConvertedComponent.Component.IsValid())
		{
			AActor* ComponentActor = ConvertedComponent.Component->GetOwner();

			if (ComponentActor != GetModifiedActor())
			{
#if WITH_EDITOR
				// Hide actor but do not hide ourselves
				ComponentActor->SetActorHiddenInGame(ConvertedComponent.bActorHiddenInGame);
				ComponentActor->SetIsTemporarilyHiddenInEditor(ConvertedComponent.bActorHiddenInEditor);
#endif
			}
			else if (USceneComponent* RootComponent = ComponentActor->GetRootComponent())
			{
				// In the meantime hide root component but later hide component itself
				RootComponent->SetHiddenInGame(ConvertedComponent.bComponentHiddenInGame);
				RootComponent->SetVisibility(ConvertedComponent.bComponentVisible);
			}
		}
	}
}

void UAvaDynamicMeshConverterModifier::OnModifierRemoved(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierRemoved(InReason);

	if (InReason != EActorModifierCoreDisableReason::Destroyed)
	{
		RemoveDynamicMeshComponent();
	}
}

void UAvaDynamicMeshConverterModifier::Apply()
{
	if (!IsMeshValid())
	{
		Fail(LOCTEXT("InvalidDynamicMeshComponent", "Invalid dynamic mesh component on modified actor"));
		return;
	}

	UDynamicMeshComponent* DynMeshComponent = GetMeshComponent();

	if (!IsValid(DynMeshComponent))
	{
		Fail(LOCTEXT("InvalidDynamicMeshComponent", "Invalid dynamic mesh component on modified actor"));
		return;
	}
	
	using namespace UE::Geometry;
	
	if (bConvertMesh)
	{
		DynMeshComponent->SetNumMaterials(0);
		DynMeshComponent->EditMesh([this, DynMeshComponent](FDynamicMesh3& AppendToMesh)
		{
			AppendToMesh.Clear();
			FMeshIndexMappings TmpMappings;
			FDynamicMeshEditor Editor(&AppendToMesh);
			FGeometryScriptAppendMeshOptions AppendOptions;
			AppendOptions.CombineMode = EGeometryScriptCombineAttributesMode::EnableAllMatching;
			int32 MaterialCount = 0;
			// Convert meshes
			ConvertedComponents.Empty();
			ConvertComponents(ConvertedComponents);
			for (const FAvaDynamicMeshConverterModifierComponentState& OutConvert : ConvertedComponents)
			{
				// Enable matching attributes & append mesh
				AppendOptions.UpdateAttributesForCombineMode(AppendToMesh, OutConvert.Mesh);
				Editor.AppendMesh(&OutConvert.Mesh, TmpMappings);
				
				if (OutConvert.Mesh.HasAttributes() && OutConvert.Mesh.Attributes()->HasMaterialID())
				{
					// Fix triangles materials linking
					const FDynamicMeshMaterialAttribute* FromMaterialIDAttrib = OutConvert.Mesh.Attributes()->GetMaterialID();
					FDynamicMeshMaterialAttribute* ToMaterialIDAttrib = AppendToMesh.Attributes()->GetMaterialID();
					TMap<int32, int32> MaterialMap;
					for (const TPair<int32, int32>& FromToTId : TmpMappings.GetTriangleMap().GetForwardMap())
					{
						const int32 FromMatId = FromMaterialIDAttrib->GetValue(FromToTId.Key);
						const int32 ToMatId = FromMatId + MaterialCount;
						MaterialMap.Add(FromMatId, ToMatId);
						ToMaterialIDAttrib->SetNewValue(FromToTId.Value, ToMatId);
					}
					MaterialCount += MaterialMap.Num();
					
					// Reapply original materials
					if (OutConvert.Component.IsValid())
					{
						for (const TPair<int32, int32>& MatPair : MaterialMap)
						{
							DynMeshComponent->SetMaterial(MatPair.Value, OutConvert.Component->GetMaterial(MatPair.Key));
						}
					}
				}
				
				if (OutConvert.Component.IsValid())
				{
					AActor* ComponentActor = OutConvert.Component->GetOwner();
					
					// Hide converted component
					if (bHideConvertedMesh)
					{
						if (ComponentActor != GetModifiedActor())
						{
							ComponentActor->SetActorHiddenInGame(true);
#if WITH_EDITOR
							ComponentActor->SetIsTemporarilyHiddenInEditor(true);
#endif
						}
						else if (USceneComponent* RootComponent = ComponentActor->GetRootComponent())
						{
							RootComponent->SetHiddenInGame(true);
							RootComponent->SetVisibility(false);
						}
					}
				}
				
				TmpMappings.Reset();
			}
			
			ConvertedMesh = AppendToMesh;
		});
	}
	else if (ConvertedMesh.IsSet())
	{
		DynMeshComponent->EditMesh([this](FDynamicMesh3& AppendToMesh)
		{
			AppendToMesh = ConvertedMesh.GetValue();
		});
	}

	Next();
}

#if WITH_EDITOR
void UAvaDynamicMeshConverterModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	
	static const FName SourceActorName = GET_MEMBER_NAME_CHECKED(UAvaDynamicMeshConverterModifier, SourceActorWeak);

	if (MemberName == SourceActorName)
	{
		OnSourceActorChanged();
	}
}

void UAvaDynamicMeshConverterModifier::ConvertToStaticMeshAsset()
{
	using namespace UE::Geometry;

	UDynamicMeshComponent* DynMeshComponent = GetMeshComponent();
	const AActor* OwningActor = GetModifiedActor();

	if (!OwningActor || !DynMeshComponent)
	{
		return;
	}
	
	// generate name for asset
	const FString NewNameSuggestion = TEXT("SM_MotionDesign_") + OwningActor->GetActorNameOrLabel();
	FString PackageName = FString(TEXT("/Game/Meshes/")) + NewNameSuggestion;
	FString AssetName;

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, AssetName);

	const TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
		SNew(SDlgPickAssetPath)
		.Title(LOCTEXT("ConvertToStaticMeshPickName", "Choose New StaticMesh Location"))
		.DefaultAssetPath(FText::FromString(PackageName));

	if (PickAssetPathWidget->ShowModal() != EAppReturnType::Ok)
	{
		return;
	}

	// get input name provided by user
	FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
	FName MeshName(*FPackageName::GetLongPackageAssetName(UserPackageName));

	// is input name valid ?
	if (MeshName == NAME_None)
	{
		// Use default if invalid
		UserPackageName = PackageName;
		MeshName = *AssetName;
	}

	const FDynamicMesh3* MeshIn = DynMeshComponent->GetMesh();

	// empty mesh do not export
	if (!MeshIn || MeshIn->TriangleCount() == 0)
	{
		return;
	}
	
	// find/create package
	UPackage* Package = CreatePackage(*UserPackageName);
	check(Package);

	// Create StaticMesh object
	UStaticMesh* DestinationMesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone);
	UDynamicMesh* SourceMesh = DynMeshComponent->GetDynamicMesh();
	
	// export options
	FGeometryScriptCopyMeshToAssetOptions AssetOptions;
	AssetOptions.bReplaceMaterials = false;
	AssetOptions.bEnableRecomputeNormals = false;
	AssetOptions.bEnableRecomputeTangents = false;
	AssetOptions.bEnableRemoveDegenerates = true;
	
	// LOD options
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins OutResult;

	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(SourceMesh, DestinationMesh, AssetOptions, TargetLOD, OutResult);
	DestinationMesh->GetBodySetup()->AggGeom = DynMeshComponent->GetBodySetup()->AggGeom;
	
	if (OutResult == EGeometryScriptOutcomePins::Success)
	{
		// Notify asset registry of new asset
		FAssetRegistryModule::AssetCreated(DestinationMesh);
	}
}
#endif

void UAvaDynamicMeshConverterModifier::SetSourceActorWeak(const TWeakObjectPtr<AActor>& InActor)
{
	if (InActor.Get() == SourceActorWeak.Get())
	{
		return;
	}

	SourceActorWeak = InActor;
	OnSourceActorChanged();
}

void UAvaDynamicMeshConverterModifier::SetComponentType(int32 InComponentType)
{
	if (InComponentType == ComponentType)
	{
		return;
	}

	ComponentType = InComponentType;
}

void UAvaDynamicMeshConverterModifier::SetFilterActorMode(EAvaDynamicMeshConverterModifierFilter InFilter)
{
	FilterActorMode = InFilter;
}

void UAvaDynamicMeshConverterModifier::SetFilterActorClasses(const TSet<TSubclassOf<AActor>>& InClasses)
{
	FilterActorClasses = InClasses;
}

void UAvaDynamicMeshConverterModifier::SetIncludeAttachedActors(bool bInInclude)
{
	bIncludeAttachedActors = bInInclude;
}

void UAvaDynamicMeshConverterModifier::SetHideConvertedMesh(bool bInHide)
{
	bHideConvertedMesh = bInHide;
}

void UAvaDynamicMeshConverterModifier::OnSourceActorChanged()
{
	const AActor* SourceActor = SourceActorWeak.Get();
	const AActor* ActorModified = GetModifiedActor();
	if (!SourceActor || !ActorModified)
	{
		return;
	}
	bHideConvertedMesh = SourceActor == ActorModified || SourceActor->IsAttachedTo(ActorModified);
}

void UAvaDynamicMeshConverterModifier::ConvertComponents(TArray<FAvaDynamicMeshConverterModifierComponentState>& OutResults) const
{
	if (!IsMeshValid() || !SourceActorWeak.IsValid())
	{
		return;
	}
	const FTransform SourceTransform = SourceActorWeak->GetActorTransform();
	UDynamicMeshComponent* DynMeshComponent = GetMeshComponent();
	UDynamicMesh* OutputDynamicMesh = DynMeshComponent->GetDynamicMesh();
	static const FGeometryScriptCopyMeshFromAssetOptions FromMeshOptions;
	static FGeometryScriptMeshReadLOD FromMeshLOD;
	FromMeshLOD.LODType = EGeometryScriptLODType::SourceModel;
	TArray<AActor*> FilteredActors;
	GetFilteredActors(FilteredActors);
	if (HasFlag(EAvaDynamicMeshConverterModifierType::StaticMeshComponent))
	{
		TArray<UStaticMeshComponent*> Components;
		GetStaticMeshComponents(FilteredActors, Components);
		for (UStaticMeshComponent* Component : Components)
		{
			UStaticMesh* StaticMesh = Component->GetStaticMesh();

			// convert to dynamic mesh
			EGeometryScriptOutcomePins OutResult;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(StaticMesh, OutputDynamicMesh, FromMeshOptions, FromMeshLOD, OutResult);
			if (OutResult == EGeometryScriptOutcomePins::Success)
			{
				// Transform the new mesh relative to the component
				const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
				OutputDynamicMesh->EditMesh([&OutResults, Component, RelativeTransform](FDynamicMesh3& EditMesh)
				{
					MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
					FAvaDynamicMeshConverterModifierComponentState NewResult(Component);
					NewResult.Mesh = MoveTemp(EditMesh);
					OutResults.Add(NewResult);
					// replace by empty mesh
					FDynamicMesh3 EmptyMesh;
					EditMesh = MoveTemp(EmptyMesh);
				});
			}
		}
	}
	if (HasFlag(EAvaDynamicMeshConverterModifierType::DynamicMeshComponent))
	{
		TArray<UDynamicMeshComponent*> Components;
		GetDynamicMeshComponents(FilteredActors, Components);
		for (UDynamicMeshComponent* Component : Components)
		{
			const UDynamicMesh* DynamicMesh = Component->GetDynamicMesh();
			// Transform the new mesh relative to the component
			const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
			// Create a copy
			DynamicMesh->ProcessMesh([&OutResults, Component, RelativeTransform](const FDynamicMesh3& EditMesh)
			{
				FDynamicMesh3 CopyMesh = EditMesh;
				MeshTransforms::ApplyTransform(CopyMesh, RelativeTransform);
				FAvaDynamicMeshConverterModifierComponentState NewResult(Component);
				NewResult.Mesh = MoveTemp(CopyMesh);
				OutResults.Add(NewResult);
			});
		}
	}
	if (HasFlag(EAvaDynamicMeshConverterModifierType::SkeletalMeshComponent))
	{
		TArray<USkeletalMeshComponent*> Components;
		GetSkeletalMeshComponents(FilteredActors, Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			USkeletalMesh* SkeletalMesh = Component->GetSkeletalMeshAsset();

			// convert to dynamic mesh
			EGeometryScriptOutcomePins OutResult;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(SkeletalMesh, OutputDynamicMesh, FromMeshOptions, FromMeshLOD, OutResult);
			if (OutResult == EGeometryScriptOutcomePins::Success)
			{
				// Transform the new mesh relative to the component
				const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
				OutputDynamicMesh->EditMesh([&OutResults, Component, RelativeTransform](FDynamicMesh3& EditMesh)
				{
					MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
					FAvaDynamicMeshConverterModifierComponentState NewResult(Component);
					NewResult.Mesh = MoveTemp(EditMesh);
					OutResults.Add(NewResult);
					// replace by empty mesh
					FDynamicMesh3 EmptyMesh;
					EditMesh = MoveTemp(EmptyMesh);
				});
			}
		}
	}
	if (HasFlag(EAvaDynamicMeshConverterModifierType::BrushComponent))
	{
		static FGeometryScriptCopyMeshFromComponentOptions Options;
		Options.RequestedLOD = FromMeshLOD;
		FTransform Transform;
		TArray<UBrushComponent*> Components;
		GetBrushComponents(FilteredActors, Components);
		for (UBrushComponent* Component : Components)
		{
			// convert to dynamic mesh
			EGeometryScriptOutcomePins OutResult;

			UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(Component, OutputDynamicMesh, Options, false, Transform, OutResult);
			if (OutResult == EGeometryScriptOutcomePins::Success)
			{
				// Transform the new mesh relative to the component
				const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
				OutputDynamicMesh->EditMesh([&OutResults, Component, RelativeTransform](FDynamicMesh3& EditMesh)
				{
					MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
					FAvaDynamicMeshConverterModifierComponentState NewResult(Component);
					NewResult.Mesh = MoveTemp(EditMesh);
					OutResults.Add(NewResult);
					// replace by empty mesh
					FDynamicMesh3 EmptyMesh;
					EditMesh = MoveTemp(EmptyMesh);
				});
			}
		}
	}
	if (HasFlag(EAvaDynamicMeshConverterModifierType::ProceduralMeshComponent))
	{
		using namespace UE::Geometry;
		TArray<UProceduralMeshComponent*> Components;
		GetProceduralMeshComponents(FilteredActors, Components);
		for (UProceduralMeshComponent* Component : Components)
		{
			const int32 SectionCount = Component->GetNumSections();
			if (SectionCount == 0)
			{
				continue;
			}

			// Transform the new mesh relative to the component
			const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
			
			FAvaDynamicMeshConverterModifierComponentState NewResult(Component);
			NewResult.Mesh.EnableAttributes();
			NewResult.Mesh.Attributes()->EnablePrimaryColors();
			NewResult.Mesh.Attributes()->EnableMaterialID();
			NewResult.Mesh.Attributes()->SetNumNormalLayers(1);
			NewResult.Mesh.Attributes()->SetNumUVLayers(1);
			NewResult.Mesh.Attributes()->SetNumPolygroupLayers(1);
			NewResult.Mesh.Attributes()->EnableTangents();
			
			FDynamicMeshColorOverlay* ColorOverlay = NewResult.Mesh.Attributes()->PrimaryColors();
			FDynamicMeshNormalOverlay* NormalOverlay = NewResult.Mesh.Attributes()->PrimaryNormals();
			FDynamicMeshUVOverlay* UVOverlay = NewResult.Mesh.Attributes()->PrimaryUV();
			FDynamicMeshMaterialAttribute* MaterialAttr = NewResult.Mesh.Attributes()->GetMaterialID();
			FDynamicMeshPolygroupAttribute* GroupAttr = NewResult.Mesh.Attributes()->GetPolygroupLayer(0);
			FDynamicMeshNormalOverlay* TangentOverlay = NewResult.Mesh.Attributes()->PrimaryTangents();
			
			for (int32 SectionIdx = 0; SectionIdx < SectionCount; SectionIdx++)
			{
				if (FProcMeshSection* Section = Component->GetProcMeshSection(SectionIdx))
				{
					if (Section->bSectionVisible)
					{
						TArray<int32> VtxIds;
						TArray<int32> NormalIds;
						TArray<int32> ColorIds;
						TArray<int32> UVIds;
						TArray<int32> TaIds;
						
						// copy vertices data (position, normal, color, UV, tangent)
						for (FProcMeshVertex& SectionVertex : Section->ProcVertexBuffer)
						{
							int32 VId = NewResult.Mesh.AppendVertex(SectionVertex.Position);
							VtxIds.Add(VId);
							
							int32 NId = NormalOverlay->AppendElement(static_cast<FVector3f>(SectionVertex.Normal));
							NormalIds.Add(NId);
							
							int32 CId = ColorOverlay->AppendElement(static_cast<FVector4f>(SectionVertex.Color));
							ColorIds.Add(CId);
							
							int32 UVId = UVOverlay->AppendElement(static_cast<FVector2f>(SectionVertex.UV0));
							UVIds.Add(UVId);

							int32 TaId = TangentOverlay->AppendElement(static_cast<FVector3f>(SectionVertex.Tangent.TangentX));
							TaIds.Add(TaId);
						}
						
						// copy tris data
						if (Section->ProcIndexBuffer.Num() % 3 != 0)
						{
							continue;
						}
						for (int32 Idx = 0; Idx < Section->ProcIndexBuffer.Num(); Idx+=3)
						{
							int32 VIdx1 = Section->ProcIndexBuffer[Idx];
							int32 VIdx2 = Section->ProcIndexBuffer[Idx + 1];
							int32 VIdx3 = Section->ProcIndexBuffer[Idx + 2];
							
							int32 VId1 = VtxIds[VIdx1];
							int32 VId2 = VtxIds[VIdx2];
							int32 VId3 = VtxIds[VIdx3];

							int32 TId = NewResult.Mesh.AppendTriangle(VId1, VId2, VId3, SectionIdx);
							if (TId < 0)
							{
								continue;
							}

							NormalOverlay->SetTriangle(TId, FIndex3i(NormalIds[VIdx1], NormalIds[VIdx2], NormalIds[VIdx3]), true);
							ColorOverlay->SetTriangle(TId, FIndex3i(ColorIds[VIdx1], ColorIds[VIdx2], ColorIds[VIdx3]), true);
							UVOverlay->SetTriangle(TId, FIndex3i(UVIds[VIdx1], UVIds[VIdx2], UVIds[VIdx3]), true);
							TangentOverlay->SetTriangle(TId, FIndex3i(TaIds[VIdx1], TaIds[VIdx2], TaIds[VIdx3]), true);
							
							MaterialAttr->SetValue(TId, SectionIdx);
							GroupAttr->SetValue(TId, SectionIdx);
						}
					}
				}
			}

			if (NewResult.Mesh.TriangleCount() > 0)
			{
				MeshTransforms::ApplyTransform(NewResult.Mesh, RelativeTransform);
				OutResults.Add(NewResult);
			}
		}
	}
}

bool UAvaDynamicMeshConverterModifier::HasFlag(EAvaDynamicMeshConverterModifierType InFlag) const
{
	return EnumHasAnyFlags(static_cast<EAvaDynamicMeshConverterModifierType>(ComponentType), InFlag);
}

void UAvaDynamicMeshConverterModifier::AddDynamicMeshComponent()
{
	UDynamicMeshComponent* DynMeshComponent = GetMeshComponent();
	
	if (DynMeshComponent)
	{
		return;
	}

	AActor* ActorModified = GetModifiedActor();

	if (!IsValid(ActorModified))
	{
		return;
	}
	
#if WITH_EDITOR
	ActorModified->Modify();
	Modify();
#endif

	const UClass* const NewComponentClass = UDynamicMeshComponent::StaticClass();

	// Construct the new component and attach as needed
	DynMeshComponent = NewObject<UDynamicMeshComponent>(ActorModified
		, NewComponentClass
		, MakeUniqueObjectName(ActorModified, NewComponentClass, TEXT("DynamicMeshComponent"))
		, RF_Transactional);

	// Add to SerializedComponents array so it gets saved
	ActorModified->AddInstanceComponent(DynMeshComponent);
	DynMeshComponent->OnComponentCreated();
	DynMeshComponent->RegisterComponent();

	if (USceneComponent* RootComponent = ActorModified->GetRootComponent())
	{
		static const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, false);
		DynMeshComponent->AttachToComponent(RootComponent, AttachRules);
	}
	else
	{
		ActorModified->SetRootComponent(DynMeshComponent);
	}

	DynMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	DynMeshComponent->SetGenerateOverlapEvents(true);

#if WITH_EDITOR
	// Rerun construction scripts
	ActorModified->RerunConstructionScripts();
#endif
	
	bComponentCreated = true;
}

void UAvaDynamicMeshConverterModifier::RemoveDynamicMeshComponent()
{
	UDynamicMeshComponent* DynMeshComponent = GetMeshComponent();
	
	if (!DynMeshComponent)
	{
		return;
	}

	// Did we create the component or was it already there
	if (!bComponentCreated)
	{
		return;
	}
	
	AActor* ActorModified = GetModifiedActor();

	if (!IsValid(ActorModified))
	{
		return;
	}
	
#if WITH_EDITOR
	ActorModified->Modify();
	Modify();
#endif

	const FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, false);
	DynMeshComponent->DetachFromComponent(DetachRules);
	
	ActorModified->RemoveInstanceComponent(DynMeshComponent);
	DynMeshComponent->DestroyComponent(false);
	
	bComponentCreated = false;
}

void UAvaDynamicMeshConverterModifier::GetFilteredActors(TArray<AActor*>& OutActors) const
{
	if (AActor* OriginActor = SourceActorWeak.Get())
	{
		OutActors.Add(OriginActor);
		if (bIncludeAttachedActors)
		{
			OriginActor->GetAttachedActors(OutActors, false, true);
		}
		// Filter actor class
		if (FilterActorMode != EAvaDynamicMeshConverterModifierFilter::None)
		{
			for (int32 Idx = OutActors.Num() - 1; Idx >= 0; Idx--)
			{
				const AActor* CurrentActor = OutActors[Idx];
				if (!IsValid(CurrentActor))
				{
					continue;
				}
				// Include this actor if it's in the filter class
				if (FilterActorMode == EAvaDynamicMeshConverterModifierFilter::Include)
				{
					if (!FilterActorClasses.Contains(CurrentActor->GetClass()))
					{
						OutActors.RemoveAt(Idx);
					}
				}
				else // Exclude this actor if it's in the filter class
				{
					if (FilterActorClasses.Contains(CurrentActor->GetClass()))
					{
						OutActors.RemoveAt(Idx);
					}
				}
			}
		}
	}
}

void UAvaDynamicMeshConverterModifier::GetStaticMeshComponents(const TArray<AActor*>& InActors, TArray<UStaticMeshComponent*>& OutComponents) const
{
	for (const AActor* Actor : InActors)
	{
		TArray<UStaticMeshComponent*> OutMeshComponents;
		Actor->GetComponents(OutMeshComponents, false);
		OutComponents.Append(OutMeshComponents);
	}
	// remove all invalid components
	OutComponents.RemoveAll([](const UStaticMeshComponent* InComponent)->bool
	{
#if WITH_EDITOR
		return !IsValid(InComponent) || InComponent->IsVisualizationComponent();
#else
		return !IsValid(InComponent);
#endif
	});
}

void UAvaDynamicMeshConverterModifier::GetDynamicMeshComponents(const TArray<AActor*>& InActors, TArray<UDynamicMeshComponent*>& OutComponents) const
{
	for (const AActor* Actor : InActors)
	{
		TArray<UDynamicMeshComponent*> OutMeshComponents;
		Actor->GetComponents(OutMeshComponents, false);
		OutComponents.Append(OutMeshComponents);
	}
	// remove all invalid components & and the modifier created component
	OutComponents.RemoveAll([this](const UDynamicMeshComponent* InComponent)->bool
	{
#if WITH_EDITOR
		return !IsValid(InComponent) || InComponent == GetMeshComponent() || InComponent->IsVisualizationComponent();
#else
		return !IsValid(InComponent) || InComponent == GetMeshComponent();
#endif
	});
}

void UAvaDynamicMeshConverterModifier::GetSkeletalMeshComponents(const TArray<AActor*>& InActors, TArray<USkeletalMeshComponent*>& OutComponents) const
{
	for (const AActor* Actor : InActors)
	{
		TArray<USkeletalMeshComponent*> OutMeshComponents;
		Actor->GetComponents(OutMeshComponents, false);
		OutComponents.Append(OutMeshComponents);
	}
	// remove all invalid components
	OutComponents.RemoveAll([this](const USkeletalMeshComponent* InComponent)->bool
	{
#if WITH_EDITOR
		return !IsValid(InComponent) || InComponent->IsVisualizationComponent();
#else
		return !IsValid(InComponent);
#endif
	});
}

void UAvaDynamicMeshConverterModifier::GetBrushComponents(const TArray<AActor*>& InActors, TArray<UBrushComponent*>& OutComponents) const
{
	for (const AActor* Actor : InActors)
	{
		TArray<UBrushComponent*> OutMeshComponents;
		Actor->GetComponents(OutMeshComponents, false);
		OutComponents.Append(OutMeshComponents);
	}
	// remove all invalid components
	OutComponents.RemoveAll([this](const UBrushComponent* InComponent)->bool
	{
#if WITH_EDITOR
		return !IsValid(InComponent) || InComponent->IsVisualizationComponent();
#else
		return !IsValid(InComponent);
#endif
	});
}

void UAvaDynamicMeshConverterModifier::GetProceduralMeshComponents(const TArray<AActor*>& InActors, TArray<UProceduralMeshComponent*>& OutComponents) const
{
	for (const AActor* Actor : InActors)
	{
		TArray<UProceduralMeshComponent*> OutMeshComponents;
		Actor->GetComponents(OutMeshComponents, false);
		OutComponents.Append(OutMeshComponents);
	}
	// remove all invalid components
	OutComponents.RemoveAll([this](const UProceduralMeshComponent* InComponent)->bool
	{
#if WITH_EDITOR
		return !IsValid(InComponent) || InComponent->IsVisualizationComponent();
#else
		return !IsValid(InComponent);
#endif
	});
}

void UAvaDynamicMeshConverterModifier::ConvertMesh()
{
	bConvertMesh = true;
	MarkModifierDirty();
}

#undef LOCTEXT_NAMESPACE
