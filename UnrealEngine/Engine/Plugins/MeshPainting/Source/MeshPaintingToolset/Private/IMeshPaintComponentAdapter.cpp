// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMeshPaintComponentAdapter.h"

#include "Components/MeshComponent.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialInstance.h"
#include "MeshPaintingToolsetTypes.h"
#include "TexturePaintToolset.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

//////////////////////////////////////////////////////////////////////////
// IMeshPaintGeometryAdapter

void IMeshPaintComponentAdapter::DefaultApplyOrRemoveTextureOverride(UMeshComponent* InMeshComponent, UTexture* SourceTexture, UTexture* OverrideTexture)
{
	// Legacy implementation for the deprecation duration. For a valid implementation look at deprecation comment

	const ERHIFeatureLevel::Type FeatureLevel = InMeshComponent->GetWorld()->GetFeatureLevel();

	// Check all the materials on the mesh to see if the user texture is there
	int32 MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	while (MaterialToCheck != nullptr)
	{
		if (!OverrideTexture || DoesMaterialUseTexture(MaterialToCheck, SourceTexture))
		{
			MaterialToCheck->OverrideTexture(SourceTexture, OverrideTexture, FeatureLevel);
		}

		++MaterialIndex;
		MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	}
}

static bool IsTextureSuitableForTexturePainting(const TWeakObjectPtr<UTexture> TexturePtr)
{
	return (TexturePtr.Get() != nullptr &&
		!TexturePtr->IsNormalMap() &&
		!TexturePtr->VirtualTextureStreaming &&
		!TexturePtr->HasHDRSource() && // Currently HDR textures are not supported to paint on.
		TexturePtr->Source.IsValid() &&
		TexturePtr->Source.GetBytesPerPixel() > 0 && // Textures' sources must have a known count of bytes per pixel
		(TexturePtr->Source.GetBytesPerPixel() <= UTexturePaintToolset::GetMaxSupportedBytesPerPixelForPainting())); // Textures' sources must fit in FColor struct to be supported.
}

void IMeshPaintComponentAdapter::DefaultQueryPaintableTextures(int32 MaterialIndex, const UMeshComponent* MeshComponent, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList)
{
	OutDefaultIndex = INDEX_NONE;

	// We already know the material we are painting on, take it off the static mesh component
	UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);

	while (Material != nullptr)
	{
		if (Material != NULL)
		{
			FPaintableTexture PaintableTexture;
			// Find all the unique textures used in the top material level of the selected actor materials

			// Only grab the textures from the top level of samples
			for (UMaterialExpression* Expression : Material->GetMaterial()->GetExpressions())
			{
				UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(Expression);
				if (TextureBase != NULL &&
					IsTextureSuitableForTexturePainting(TextureBase->Texture))
				{
					// Default UV channel to index 0. 
					PaintableTexture = FPaintableTexture(TextureBase->Texture, 0);

					// Texture Samples can have UV's specified, check the first node for whether it has a custom UV channel set. 
					// We only check the first as the Mesh paint mode does not support painting with UV's modified in the shader.
					UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);
					if (TextureSample != NULL)
					{
						UMaterialExpressionTextureCoordinate* TextureCoords = Cast<UMaterialExpressionTextureCoordinate>(TextureSample->Coordinates.Expression);
						if (TextureCoords != NULL)
						{
							// Store the uv channel, this is set when the texture is selected. 
							PaintableTexture.UVChannelIndex = TextureCoords->CoordinateIndex;
						}

						// Handle texture parameter expressions
						UMaterialExpressionTextureSampleParameter* TextureSampleParameter = Cast<UMaterialExpressionTextureSampleParameter>(TextureSample);
						if (TextureSampleParameter != NULL)
						{
							// Grab the overridden texture if it exists.  
							Material->GetTextureParameterValue(TextureSampleParameter->ParameterName, PaintableTexture.Texture);
						}
					}

					// note that the same texture will be added again if its UV channel differs. 
					int32 TextureIndex = InOutTextureList.AddUnique(PaintableTexture);

					// cache the first default index, if there is no previous info this will be used as the selected texture
					if ((OutDefaultIndex == INDEX_NONE) && TextureBase->IsDefaultMeshpaintTexture)
					{
						OutDefaultIndex = TextureIndex;
					}
				}
			}
		}
		// Make sure to include all texture parameters, this will include all of the texture parameters from internal material functions
		TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ParameterValues;
		Material->GetAllParametersOfType(EMaterialParameterType::Texture, ParameterValues);

		for (auto& ParameterElem : ParameterValues)
		{
			const TWeakObjectPtr<UTexture> TexturePtr = ParameterElem.Value.Value.Texture;

			if (IsTextureSuitableForTexturePainting(TexturePtr))
			{
				FPaintableTexture PaintableTexture;

				// Default UV channel to index 0.
				PaintableTexture = FPaintableTexture(TexturePtr.Get(), 0);
				InOutTextureList.AddUnique(PaintableTexture);
			}
		}

		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance)
		{
			Material = MaterialInstance->Parent ? Cast<UMaterialInstance>(MaterialInstance->Parent) : nullptr;
		}
		else
		{
			// This prevents an infinite loop when `Material` isn't a material instance.
			break;
		}
	}
}

namespace UE::MeshPaintingToolset
{
	namespace Private
	{
		struct FOverrideData
		{
			TWeakObjectPtr<UTexture> OverrideTexture;
			TSet<ERHIFeatureLevel::Type, DefaultKeyFuncs<ERHIFeatureLevel::Type>, TInlineSetAllocator<1>> OverridenFeatureLevels;
			uint32 Count = 0;
		};

		struct FGlobalTextureOverrideState
		{
			using FOverrideKey = TPair<TWeakObjectPtr<UMaterialInterface>, TWeakObjectPtr<const UTexture>>;

			static void DuplicateOverrideOwnership(const FDefaultTextureOverride* Current, const FDefaultTextureOverride* To)
			{
				TSet<FOverrideKey, DefaultKeyFuncs<FOverrideKey>, TInlineSetAllocator<2>>* Overrides = DefaultTextureOverrideToOverrides.Find(Current);
				if (Overrides)
				{
					for (const FOverrideKey& Pair : *Overrides)
					{
						OverridesData.Find(Pair)->Count += 1;
					}

					// Duplicate the override in case an allocation change the addresses
					DefaultTextureOverrideToOverrides.Add(To, TSet<FOverrideKey>(*Overrides));
				}
			}

			static void TransferOverrideOwnership(const FDefaultTextureOverride* Current, const FDefaultTextureOverride* To)
			{
				TSet<FOverrideKey, DefaultKeyFuncs<FOverrideKey>, TInlineSetAllocator<2>> Overrides;
				if (DefaultTextureOverrideToOverrides.RemoveAndCopyValue(Current, Overrides))
				{
					DefaultTextureOverrideToOverrides.Add(To, MoveTemp(Overrides));
				}
			}

			static void RegisterMaterialOverride(const FDefaultTextureOverride* Requester, UMaterialInterface* Material, const UTexture* SourceTexture, UTexture* OverrrideTexture, const ERHIFeatureLevel::Type FeatureLevel)
			{
				TSet<FOverrideKey, DefaultKeyFuncs<FOverrideKey>, TInlineSetAllocator<2>>& Overrides = DefaultTextureOverrideToOverrides.FindOrAdd(Requester);
				
				FOverrideKey Pair = FOverrideKey(TWeakObjectPtr<UMaterialInterface>(Material), TWeakObjectPtr<const UTexture>(SourceTexture));
				uint32 PairHash = GetTypeHash(Pair);

				Overrides.AddByHash(PairHash, Pair);
				
				FOverrideData& OverrideData = OverridesData.FindOrAddByHash(PairHash, Pair);
				
				bool bFeatureLevelAlreadyOverriden = false;
				uint32 FeatureLevelHash = GetTypeHash(FeatureLevel);
				OverrideData.OverridenFeatureLevels.AddByHash(FeatureLevelHash, FeatureLevel, &bFeatureLevelAlreadyOverriden);

				if (OverrideData.Count == 0 || OverrideData.OverrideTexture != OverrrideTexture)
				{
					OverrideData.OverrideTexture = OverrrideTexture;

					for (const ERHIFeatureLevel::Type LevelToUpdate : OverrideData.OverridenFeatureLevels)
					{
						Material->OverrideTexture(SourceTexture, OverrrideTexture, LevelToUpdate);
					}

					AddMaterialTracking(Pair);
				}
				else if (!bFeatureLevelAlreadyOverriden)
				{
					OverrideData.OverridenFeatureLevels.AddByHash(FeatureLevelHash, FeatureLevel);
					Material->OverrideTexture(SourceTexture, OverrrideTexture, FeatureLevel);
				}
	
				OverrideData.Count += 1;
			}

			static void RemoveMaterialOverride(const FDefaultTextureOverride* Requester, UMaterialInterface* Material, const UTexture* SourceTexture)
			{
				if (TSet<FOverrideKey, DefaultKeyFuncs<FOverrideKey>, TInlineSetAllocator<2>>* Override = DefaultTextureOverrideToOverrides.Find(Requester))
				{
					FOverrideKey Pair = FOverrideKey(TWeakObjectPtr<UMaterialInterface>(Material), TWeakObjectPtr<const UTexture>(SourceTexture));
					uint32 PairHash = GetTypeHash(Pair);

					if (Override->RemoveByHash(PairHash, Pair) > 0)
					{
						if (FOverrideData* OverrideData = OverridesData.FindByHash(PairHash, Pair))
						{
							OverrideData->Count -= 1;
							if (OverrideData->Count == 0)
							{
								for (const ERHIFeatureLevel::Type FeatureLevel : OverrideData->OverridenFeatureLevels)
								{
									Material->OverrideTexture(SourceTexture, nullptr, FeatureLevel);
								}

								OverridesData.RemoveByHash(PairHash, Pair);
								RemoveMaterialTracking(Pair);
							}
						}
					}
				}
			}

			static void FreeOverrideOwnership(const FDefaultTextureOverride* Requester)
			{
				TSet<FOverrideKey, DefaultKeyFuncs<FOverrideKey>, TInlineSetAllocator<2>> Overrides;
				if (DefaultTextureOverrideToOverrides.RemoveAndCopyValue(Requester, Overrides))
				{
					for (const FOverrideKey& Override : Overrides)
					{
						uint32 OverrideHash = GetTypeHash(Override);
						if (FOverrideData* OverrideData = OverridesData.FindByHash(OverrideHash, Override))
						{
							OverrideData->Count -= 1;
							if (OverrideData->Count == 0)
							{
								UMaterialInterface* Material = Override.Key.Get();
								const UTexture* SourceTexture = Override.Value.Get();
								for (const ERHIFeatureLevel::Type LevelToUpdate : OverrideData->OverridenFeatureLevels)
								{
									Material->OverrideTexture(SourceTexture, nullptr, LevelToUpdate);
								}

								OverridesData.RemoveByHash(OverrideHash, Override);
								RemoveMaterialTracking(Override);
							}
						}
					}
				}
			}

			static void AddMaterialTracking(const FOverrideKey& Override)
			{
				if (MaterialsAndTexturesOverriden.IsEmpty())
				{
					OnObjectModifiedDelegateHandle = FCoreUObjectDelegates::OnObjectModified.AddStatic(&FGlobalTextureOverrideState::OnObjectModified);
					PostEditDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddStatic(&FGlobalTextureOverrideState::OnObjectPropertyChanged);
				}

				TSet<TWeakObjectPtr<const UTexture>, DefaultKeyFuncs<TWeakObjectPtr<const UTexture>>, TInlineSetAllocator<2>>& Textures = MaterialsAndTexturesOverriden.FindOrAdd(Override.Key);
				Textures.Add(Override.Value);
			}

			static void RemoveMaterialTracking(const FOverrideKey& Override)
			{
				uint32 MaterialHash = GetTypeHash(Override.Key);
				if (TSet<TWeakObjectPtr<const UTexture>, DefaultKeyFuncs<TWeakObjectPtr<const UTexture>>, TInlineSetAllocator<2>>* Textures = MaterialsAndTexturesOverriden.FindByHash(MaterialHash, Override.Key))
				{
					Textures->Remove(Override.Value);
					if (Textures->IsEmpty())
					{
						MaterialsAndTexturesOverriden.RemoveByHash(MaterialHash, Override.Key);
						if (MaterialsAndTexturesOverriden.IsEmpty())
						{
							FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedDelegateHandle);
							FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PostEditDelegateHandle);
						}
					}
				}
			}

			static void OnObjectModified(UObject* Object)
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object))
				{
					FOverrideKey Override;
					Override.Key = Material;
					if (const TSet<TWeakObjectPtr<const UTexture>, DefaultKeyFuncs<TWeakObjectPtr<const UTexture>>, TInlineSetAllocator<2>>* Textures = MaterialsAndTexturesOverriden.Find(Override.Key))
					{
						for (const TWeakObjectPtr<const UTexture>& Texture : *Textures)
						{
							if (const UTexture* RawTexturePtr = Texture.Get())
							{
								Override.Value = Texture;
								if (FOverrideData* OverrideData = OverridesData.Find(Override))
								{
									for (const ERHIFeatureLevel::Type FeatureLevel : OverrideData->OverridenFeatureLevels)
									{
										// The material resource might change because of the modifications. To avoid leaking some temp texture overrides, this just remove the temporary overrides during the modification.
										Material->OverrideTexture(RawTexturePtr, nullptr, FeatureLevel);
									}
								}
							}
						}
					}
				}
			}

			static void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
			{
				if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
				{
					if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object))
					{
						FOverrideKey Override;
						Override.Key = Material;
						if (const TSet<TWeakObjectPtr<const UTexture>, DefaultKeyFuncs<TWeakObjectPtr<const UTexture>>, TInlineSetAllocator<2>>* Textures = MaterialsAndTexturesOverriden.Find(Override.Key))
						{
							for (const TWeakObjectPtr<const UTexture>& Texture : *Textures)
							{
								if (const UTexture* RawTexturePtr = Texture.Get())
								{
									Override.Value = Texture;
									if (FOverrideData* OverrideData = OverridesData.Find(Override))
									{
										if (UTexture* OverrideTexture = OverrideData->OverrideTexture.Get())
										{ 
											for (const ERHIFeatureLevel::Type FeatureLevel : OverrideData->OverridenFeatureLevels)
											{
												// Reapply the temporary overrides after the modification.
												Material->OverrideTexture(RawTexturePtr, OverrideTexture, FeatureLevel);
											}
										}
									}
								}
							}
						}
					}
				}
			}


		private:
			static TMap<FOverrideKey, FOverrideData> OverridesData;
			static TMap<const FDefaultTextureOverride*, TSet<FOverrideKey, DefaultKeyFuncs<FOverrideKey>, TInlineSetAllocator<2>>> DefaultTextureOverrideToOverrides;
			static TMap<TWeakObjectPtr<UMaterialInterface>, TSet<TWeakObjectPtr<const UTexture>, DefaultKeyFuncs<TWeakObjectPtr<const UTexture>>, TInlineSetAllocator<2>>> MaterialsAndTexturesOverriden;

			static FDelegateHandle OnObjectModifiedDelegateHandle;
			static FDelegateHandle PostEditDelegateHandle;
		};

		TMap<FGlobalTextureOverrideState::FOverrideKey, FOverrideData> FGlobalTextureOverrideState::OverridesData;
		TMap<const FDefaultTextureOverride*, TSet<FGlobalTextureOverrideState::FOverrideKey, DefaultKeyFuncs<FGlobalTextureOverrideState::FOverrideKey>, TInlineSetAllocator<2>>> FGlobalTextureOverrideState::DefaultTextureOverrideToOverrides;
		TMap<TWeakObjectPtr<UMaterialInterface>, TSet<TWeakObjectPtr<const UTexture>, DefaultKeyFuncs<TWeakObjectPtr<const UTexture>>, TInlineSetAllocator<2>>> FGlobalTextureOverrideState::MaterialsAndTexturesOverriden;
		FDelegateHandle FGlobalTextureOverrideState::OnObjectModifiedDelegateHandle;
		FDelegateHandle FGlobalTextureOverrideState::PostEditDelegateHandle;
	}


	FDefaultTextureOverride::FDefaultTextureOverride(const FDefaultTextureOverride& InOther)
	{
		operator=(InOther);
	}

	FDefaultTextureOverride::FDefaultTextureOverride(FDefaultTextureOverride&& InOther)
	{
		operator=(MoveTemp(InOther));
	}

	FDefaultTextureOverride& FDefaultTextureOverride::operator=(const FDefaultTextureOverride& InOther)
	{
		Private::FGlobalTextureOverrideState::DuplicateOverrideOwnership(this, &InOther);
		return *this;
	}

	FDefaultTextureOverride& FDefaultTextureOverride::operator=(FDefaultTextureOverride&& InOther)
	{
		Private::FGlobalTextureOverrideState::TransferOverrideOwnership(this, &InOther);
		return *this;
	}

	void FDefaultTextureOverride::ApplyOrRemoveTextureOverride(const UMeshComponent* InMeshComponent, const UTexture* SourceTexture, UTexture* OverrideTexture) const
	{
		check(IsInGameThread());

		const ERHIFeatureLevel::Type FeatureLevel = InMeshComponent->GetWorld()->GetFeatureLevel();

		// Check all the materials on the mesh to see if the user texture is there
		int32 MaterialIndex = 0;
		UMaterialInterface* MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
		while (MaterialToCheck != nullptr)
		{
			if (!OverrideTexture)
			{
				// Unregister for all materials. This will no affect the material that weren't overridden by this instance
				Private::FGlobalTextureOverrideState::RemoveMaterialOverride(this, MaterialToCheck, SourceTexture);
			}
			else if (DoesMaterialUseTexture(MaterialToCheck, SourceTexture))
			{
				// Keep track of the material overridden
				Private::FGlobalTextureOverrideState::RegisterMaterialOverride(this, MaterialToCheck, SourceTexture, OverrideTexture, FeatureLevel);

			}

			++MaterialIndex;
			MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
		}
	}

	FDefaultTextureOverride::~FDefaultTextureOverride()
	{
		check(IsInGameThread());

		Private::FGlobalTextureOverrideState::FreeOverrideOwnership(this);
	}

}
