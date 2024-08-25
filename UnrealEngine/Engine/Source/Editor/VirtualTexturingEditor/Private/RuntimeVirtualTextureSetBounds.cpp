// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureSetBounds.h"

#include "Components/PrimitiveComponent.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "UObject/UObjectIterator.h"
#include "VT/RuntimeVirtualTexture.h"

namespace RuntimeVirtualTexture
{
	void SetBounds(URuntimeVirtualTextureComponent* InComponent)
	{
		URuntimeVirtualTexture const* VirtualTexture = InComponent->GetVirtualTexture();
		check(VirtualTexture != nullptr);

		// Calculate bounds in our desired local space.
		AActor* Owner = InComponent->GetOwner();
		const FVector TargetPosition = Owner->ActorToWorld().GetTranslation();
		
		// Local space will take rotation from a BoundsAlignActor if set.
		TSoftObjectPtr<AActor>& BoundsAlignActor = InComponent->GetBoundsAlignActor();
		const FQuat TargetRotation = BoundsAlignActor.IsValid() ? BoundsAlignActor->GetTransform().GetRotation() : Owner->ActorToWorld().GetRotation();

		FTransform LocalTransform;
		LocalTransform.SetComponents(TargetRotation, TargetPosition, FVector::OneVector);
		FTransform WorldToLocal = LocalTransform.Inverse();

		// Special case where if the bounds align actor is a landscape we want to automatically include all associated landscape components.
		FGuid BoundsAlignLandscapeGuid;
		if (BoundsAlignActor.IsValid())
		{
			if (ALandscape const* LandscapeProxy = Cast<ALandscape>(BoundsAlignActor.Get()))
			{
				BoundsAlignLandscapeGuid = LandscapeProxy->GetLandscapeGuid();
			}
		}

		// Expand bounds for the BoundsAlignActor and all primitive components that write to this virtual texture.
		FBox Bounds(ForceInit);
		for (TObjectIterator<UPrimitiveComponent> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			bool bUseBounds = BoundsAlignActor.IsValid() && It->GetOwner() == BoundsAlignActor.Get();

			if (BoundsAlignLandscapeGuid.IsValid())
			{
				if (ALandscapeProxy const* LandscapeProxy = Cast<ALandscapeProxy>(It->GetOwner()))
				{
					bUseBounds |= LandscapeProxy->GetLandscapeGuid() == BoundsAlignLandscapeGuid;
				}
			}

			TArray<URuntimeVirtualTexture*> const& VirtualTextures = It->GetRuntimeVirtualTextures();
			for (int32 Index = 0; !bUseBounds && Index < VirtualTextures.Num(); ++Index) 
			{
				if (VirtualTextures[Index] == InComponent->GetVirtualTexture())
				{
					bUseBounds = true;
				}
			}

			if (bUseBounds)
			{
				FBoxSphereBounds LocalSpaceBounds = It->CalcBounds(It->GetComponentTransform() * WorldToLocal);
				if (LocalSpaceBounds.GetBox().GetVolume() > 0.f)
				{
					Bounds += LocalSpaceBounds.GetBox();
				}
			}
		}

		// Expand bounds.
		const float ExpandBounds = InComponent->GetExpandBounds();
		if (Bounds.IsValid && ExpandBounds > 0)
		{
			Bounds = Bounds.ExpandBy(ExpandBounds);
		}

		// Calculate the transform to fit the bounds.
		FTransform Transform;
		const FVector LocalPosition = Bounds.Min;
		const FVector WorldPosition = LocalTransform.TransformPosition(LocalPosition);
		const FVector WorldSize = Bounds.GetSize();
		Transform.SetComponents(TargetRotation, WorldPosition, WorldSize);

		// Adjust and snap to landscape if requested.
		// This places the texels on the landscape vertex positions which is desirable for virtual textures that hold height or position information.
		// Warning: This shifts the virtual texture volume so that it might be larger then the landscape (or smaller if insufficient resolution has been set).
		if (InComponent->GetSnapBoundsToLandscape() && BoundsAlignActor.IsValid())
		{
			ALandscape const* Landscape = Cast<ALandscape>(BoundsAlignActor.Get());
			if (Landscape != nullptr)
			{
				const FTransform LandscapeTransform = Landscape->GetTransform();
				const FVector LandscapePosition = LandscapeTransform.GetTranslation();
				const FVector LandscapeScale = LandscapeTransform.GetScale3D();

				ULandscapeInfo const* LandscapeInfo = Landscape->GetLandscapeInfo();
				int32 MinX, MinY, MaxX, MaxY;
				LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
				const FIntPoint LandscapeSize(MaxX - MinX + 1, MaxY - MinY + 1);
				const int32 LandscapeSizeLog2 = FMath::Max(FMath::CeilLogTwo(LandscapeSize.X), FMath::CeilLogTwo(LandscapeSize.Y));

				const int32 VirtualTextureSize = VirtualTexture->GetSize();
				const int32 VirtualTextureSizeLog2 = FMath::FloorLog2(VirtualTextureSize);

				// Adjust scale.
				// Note that we need virtual texture resolution to be greater or equal to the landscape resolution for the virtual texture to cover the entire landscape.
				const int32 VirtualTexelsPerLandscapeVertexLog2 = FMath::Max(VirtualTextureSizeLog2 - LandscapeSizeLog2, 0);
				const int32 VirtualTexelsPerLandscapeVertex = 1 << VirtualTexelsPerLandscapeVertexLog2;
				const FVector VirtualTexelWorldSize = LandscapeScale / (float)VirtualTexelsPerLandscapeVertex;
				const FVector VirtualTextureScale = VirtualTexelWorldSize * (float)VirtualTextureSize;
				Transform.SetScale3D(FVector(VirtualTextureScale.X, VirtualTextureScale.Y, Transform.GetScale3D().Z));

				// Adjust position to snap at a half texel offset from landscape.
				const FVector BaseVirtualTexturePosition = Transform.GetTranslation();
				const FVector LandscapeSnapPosition = LandscapePosition - 0.5f * VirtualTexelWorldSize;
				const float SnapOffsetX = FMath::Frac((BaseVirtualTexturePosition.X - LandscapeSnapPosition.X) / VirtualTexelWorldSize.X) * VirtualTexelWorldSize.X;
				const float SnapOffsetY = FMath::Frac((BaseVirtualTexturePosition.Y - LandscapeSnapPosition.Y) / VirtualTexelWorldSize.Y) * VirtualTexelWorldSize.Y;
				const FVector VirtualTexturePosition = BaseVirtualTexturePosition - FVector(SnapOffsetX, SnapOffsetY, 0);
				Transform.SetTranslation(FVector(BaseVirtualTexturePosition.X - SnapOffsetX, BaseVirtualTexturePosition.Y - SnapOffsetY, BaseVirtualTexturePosition.Z));
			}
		}

		// Apply final result and notify the parent actor
		Owner->Modify();
		Owner->SetActorTransform(Transform);
		Owner->PostEditMove(true);
	}
}
