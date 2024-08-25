// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDConversionUtils.h"

#include "USDAssetImportData.h"
#include "USDAssetUserData.h"
#include "USDClassesModule.h"
#include "USDDrawModeComponent.h"
#include "USDDuplicateType.h"
#include "USDErrorUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDProjectSettings.h"
#include "USDSkeletalDataConversion.h"
#include "USDTypesConversion.h"
#include "USDUnrealAssetInfo.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "AnalyticsEventAttribute.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GeometryCache.h"
#include "GroomAsset.h"
#include "GroomCache.h"
#include "InstancedFoliageActor.h"
#include "LandscapeProxy.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "Widgets/Notifications/SNotificationList.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/usd/kind/registry.h"
#include "pxr/usd/sdf/copyUtils.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/editContext.h"
#include "pxr/usd/usd/modelAPI.h"
#include "pxr/usd/usd/payloads.h"
#include "pxr/usd/usd/primCompositionQuery.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/variantSets.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/usd/usdGeom/gprim.h"
#include "pxr/usd/usdGeom/imageable.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/modelAPI.h"
#include "pxr/usd/usdGeom/primvar.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/shapingAPI.h"
#include "pxr/usd/usdLux/sphereLight.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/cache.h"
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/skeletonQuery.h"
#include "pxr/usd/usdVol/openVDBAsset.h"
#include "pxr/usd/usdVol/volume.h"
#include "USDIncludesEnd.h"

#include <string>

#define LOCTEXT_NAMESPACE "USDConversionUtils"

static bool GParseUVSetsFromFloat2Primvars = true;
static FAutoConsoleVariableRef CVarParseUVSetsFromFloat2Primvars(
	TEXT("USD.ParseUVSetsFromFloat2Primvars"),
	GParseUVSetsFromFloat2Primvars,
	TEXT("Primvars with the 'texCoord2f' role will always be parsed when handling potential UV sets. If this cvar is enabled, we'll also handle "
		 "primvars declared as just 'float2' however. You could disable this cvar if your pipeline emits many 'float2' primvars that you do not wish "
		 "to be parsed as UV sets.")
);

namespace USDConversionUtilsImpl
{
	/** Show some warnings if the UVSet primvars show some unsupported/problematic behavior */
	void CheckUVSetPrimvars(
		TMap<int32, TArray<pxr::UsdGeomPrimvar>> UsablePrimvars,
		TMap<int32, TArray<pxr::UsdGeomPrimvar>> UsedPrimvars,
		const FString& MeshPath
	)
	{
		// Show a warning if the mesh has a primvar that could be used as a UV set but will actually be ignored because it targets a UV set with index
		// larger than MAX_STATIC_TEXCOORDS - 1
		TArray<FString> IgnoredPrimvarNames;
		for (const TPair<int32, TArray<pxr::UsdGeomPrimvar>>& UsedPrimvar : UsedPrimvars)
		{
			if (UsedPrimvar.Key > MAX_STATIC_TEXCOORDS - 1)
			{
				for (const pxr::UsdGeomPrimvar& Primvar : UsedPrimvar.Value)
				{
					IgnoredPrimvarNames.AddUnique(UsdToUnreal::ConvertToken(Primvar.GetBaseName()));
				}
			}
		}
		for (const TPair<int32, TArray<pxr::UsdGeomPrimvar>>& UsablePrimvar : UsablePrimvars)
		{
			if (UsablePrimvar.Key > MAX_STATIC_TEXCOORDS - 1)
			{
				for (const pxr::UsdGeomPrimvar& Primvar : UsablePrimvar.Value)
				{
					// Only consider texcoord2f here because the user may have some other float2[] for some other reason
					if (Primvar.GetTypeName().GetRole() == pxr::SdfValueTypeNames->TexCoord2f.GetRole())
					{
						IgnoredPrimvarNames.AddUnique(UsdToUnreal::ConvertToken(Primvar.GetBaseName()));
					}
				}
			}
		}
		if (IgnoredPrimvarNames.Num() > 0)
		{
			FString PrimvarNames = FString::Join(IgnoredPrimvarNames, TEXT(", "));
			FUsdLogManager::LogMessage(
				EMessageSeverity::Warning,
				FText::Format(
					LOCTEXT(
						"TooHighUVIndex",
						"Mesh '{0}' has some valid UV set primvars ({1}) that will be ignored because they target an UV index larger than the "
						"highest supported ({2})"
					),
					FText::FromString(MeshPath),
					FText::FromString(PrimvarNames),
					MAX_STATIC_TEXCOORDS - 1
				)
			);
		}

		// Show a warning if the mesh does not contain the exact primvars the material wants
		for (const TPair<int32, TArray<pxr::UsdGeomPrimvar>>& UVAndPrimvars : UsedPrimvars)
		{
			const int32 UVIndex = UVAndPrimvars.Key;
			const TArray<pxr::UsdGeomPrimvar>& UsedPrimvarsForIndex = UVAndPrimvars.Value;
			if (UsedPrimvarsForIndex.Num() < 1)
			{
				continue;
			}

			// If we have multiple, we'll pick the first one and show a warning about this later
			const pxr::UsdGeomPrimvar& UsedPrimvar = UsedPrimvarsForIndex[0];

			bool bFoundUsablePrimvar = false;
			if (const TArray<pxr::UsdGeomPrimvar>* FoundUsablePrimvars = UsablePrimvars.Find(UVIndex))
			{
				// We will only ever use the first one, but will show more warnings in case there are multiple
				if (FoundUsablePrimvars->Contains(UsedPrimvar))
				{
					bFoundUsablePrimvar = true;
				}
			}

			if (!bFoundUsablePrimvar)
			{
				FUsdLogManager::LogMessage(
					EMessageSeverity::Warning,
					FText::Format(
						LOCTEXT("DidNotFindPrimvar", "Could not find primvar '{0}' on mesh '{1}', used by its bound material"),
						FText::FromString(UsdToUnreal::ConvertString(UsedPrimvar.GetBaseName())),
						FText::FromString(MeshPath)
					)
				);
			}
		}

		// Show a warning if the mesh has multiple primvars that want to write to the same UV set (e.g. 'st', 'st_0' and 'st0' at the same time)
		for (const TPair<int32, TArray<pxr::UsdGeomPrimvar>>& UVAndPrimvars : UsablePrimvars)
		{
			const int32 UVIndex = UVAndPrimvars.Key;
			const TArray<pxr::UsdGeomPrimvar>& Primvars = UVAndPrimvars.Value;
			if (Primvars.Num() > 1)
			{
				// Find out what primvar we'll actually end up using, as UsedPrimvars will take precedence. Note that in the best case scenario,
				// UsablePrimvars will *contain* UsedPrimvars, so that really we're just picking which of the UsedPrimvars we'll choose. If we're not
				// in that scenario, then we will show another warning about it
				const pxr::UsdGeomPrimvar* UsedPrimvar = nullptr;
				bool bUsedByMaterial = false;
				if (const TArray<pxr::UsdGeomPrimvar>* FoundUsedPrimvars = UsedPrimvars.Find(UVIndex))
				{
					if (FoundUsedPrimvars->Num() > 0)
					{
						UsedPrimvar = &(*FoundUsedPrimvars)[0];
						bUsedByMaterial = true;
					}
				}
				else
				{
					UsedPrimvar = &Primvars[0];
				}

				FUsdLogManager::LogMessage(
					EMessageSeverity::Warning,
					FText::Format(
						LOCTEXT(
							"MoreThanOnePrimvarForIndex",
							"Mesh '{0}' has more than one primvar used as UV set with index '{1}'. The UV set will use the values from primvar "
							"'{2}'{3}"
						),
						FText::FromString(MeshPath),
						UVAndPrimvars.Key,
						FText::FromString(UsdToUnreal::ConvertString(UsedPrimvar->GetBaseName())),
						bUsedByMaterial ? FText::FromString(TEXT(", as its used by its bound material")) : FText::GetEmpty()
					)
				);
			}
		}
	}

	// Shows a notification saying that some specs of the provided prims won't be duplicated due to being on external
	// layers
	void NotifySpecsWontBeDuplicated(const TArray<UE::FUsdPrim>& Prims)
	{
		if (Prims.Num() == 0)
		{
			return;
		}

		const FText Text = LOCTEXT("IncompleteDuplicationText", "USD: Incomplete duplication");

		FString PrimNamesString;
		const static FString Delimiter = TEXT(", ");
		for (const UE::FUsdPrim& Prim : Prims)
		{
			PrimNamesString += Prim.GetName().ToString() + Delimiter;
		}
		PrimNamesString.RemoveFromEnd(Delimiter);

		const int32 NumPrims = Prims.Num();

		const FText SubText = FText::Format(
			LOCTEXT(
				"IncompleteDuplicationSubText",
				"{0}|plural(one=This,other=These) duplicated {0}|plural(one=prim,other=prims):\n\n{1}\n\n{0}|plural(one=Has,other=Have) some specs "
				"within layers that are outside of the stage's local layer stack, and so will not be duplicated.\n\nIf you wish to modify referenced "
				"or payload layers, please open those layers as USD stages directly."
			),
			NumPrims,
			FText::FromString(PrimNamesString)
		);

		UE_LOG(LogUsd, Warning, TEXT("%s"), *SubText.ToString().Replace(TEXT("\n\n"), TEXT(" ")));

		const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
		if (Settings && Settings->bShowWarningOnIncompleteDuplication)
		{
			static TWeakPtr<SNotificationItem> Notification;

			FNotificationInfo Toast(Text);
			Toast.SubText = SubText;
			Toast.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
			Toast.CheckBoxText = LOCTEXT("DontAskAgain", "Don't prompt again");
			Toast.bUseLargeFont = false;
			Toast.bFireAndForget = false;
			Toast.FadeOutDuration = 0.0f;
			Toast.ExpireDuration = 0.0f;
			Toast.bUseThrobber = false;
			Toast.bUseSuccessFailIcons = false;
			Toast.ButtonDetails.Emplace(
				LOCTEXT("OverridenOpinionMessageOk", "Ok"),
				FText::GetEmpty(),
				FSimpleDelegate::CreateLambda(
					[]()
					{
						if (TSharedPtr<SNotificationItem> PinnedNotification = Notification.Pin())
						{
							PinnedNotification->SetCompletionState(SNotificationItem::CS_Success);
							PinnedNotification->ExpireAndFadeout();
						}
					}
				)
			);
			// This is flipped because the default checkbox message is "Don't prompt again"
			Toast.CheckBoxState = Settings->bShowWarningOnIncompleteDuplication ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
			Toast.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic(
				[](ECheckBoxState NewState)
				{
					if (UUsdProjectSettings* Settings = GetMutableDefault<UUsdProjectSettings>())
					{
						// This is flipped because the default checkbox message is "Don't prompt again"
						Settings->bShowWarningOnIncompleteDuplication = NewState == ECheckBoxState::Unchecked;
						Settings->SaveConfig();
					}
				}
			);

			// Only show one at a time
			if (!Notification.IsValid())
			{
				Notification = FSlateNotificationManager::Get().AddNotification(Toast);
			}

			if (TSharedPtr<SNotificationItem> PinnedNotification = Notification.Pin())
			{
				PinnedNotification->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
	}
}	 // namespace USDConversionUtilsImpl

template<typename ValueType>
ValueType UsdUtils::GetUsdValue(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode)
{
	ValueType Value{};
	if (Attribute)
	{
		Attribute.Get(&Value, TimeCode);
	}

	return Value;
}

// Explicit template instantiation
template USDUTILITIES_API bool UsdUtils::GetUsdValue<bool>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API float UsdUtils::GetUsdValue<float>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API double UsdUtils::GetUsdValue<double>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API pxr::TfToken UsdUtils::GetUsdValue<pxr::TfToken>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API pxr::GfVec3f UsdUtils::GetUsdValue<pxr::GfVec3f>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API pxr::GfMatrix4d UsdUtils::GetUsdValue<pxr::GfMatrix4d>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API pxr::SdfAssetPath UsdUtils::GetUsdValue<pxr::SdfAssetPath>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);
template USDUTILITIES_API pxr::VtArray<pxr::GfVec3f> UsdUtils::GetUsdValue<pxr::VtArray<pxr::GfVec3f>>(
	const pxr::UsdAttribute& Attribute,
	pxr::UsdTimeCode TimeCode
);
template USDUTILITIES_API pxr::VtArray<float> UsdUtils::GetUsdValue<pxr::VtArray<float>>(
	const pxr::UsdAttribute& Attribute,
	pxr::UsdTimeCode TimeCode
);
template USDUTILITIES_API pxr::VtArray<int> UsdUtils::GetUsdValue<pxr::VtArray<int>>(const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode);

pxr::TfToken UsdUtils::GetUsdStageUpAxis(const pxr::UsdStageRefPtr& Stage)
{
	return pxr::UsdGeomGetStageUpAxis(Stage);
}

EUsdUpAxis UsdUtils::GetUsdStageUpAxisAsEnum(const pxr::UsdStageRefPtr& Stage)
{
	pxr::TfToken UpAxisToken = pxr::UsdGeomGetStageUpAxis(Stage);
	return UpAxisToken == pxr::UsdGeomTokens->z ? EUsdUpAxis::ZAxis : EUsdUpAxis::YAxis;
}

void UsdUtils::SetUsdStageUpAxis(const pxr::UsdStageRefPtr& Stage, pxr::TfToken Axis)
{
	pxr::UsdGeomSetStageUpAxis(Stage, Axis);
}

void UsdUtils::SetUsdStageUpAxis(const pxr::UsdStageRefPtr& Stage, EUsdUpAxis Axis)
{
	pxr::TfToken UpAxisToken = Axis == EUsdUpAxis::ZAxis ? pxr::UsdGeomTokens->z : pxr::UsdGeomTokens->y;
	SetUsdStageUpAxis(Stage, UpAxisToken);
}

float UsdUtils::GetUsdStageMetersPerUnit(const pxr::UsdStageRefPtr& Stage)
{
	return (float)pxr::UsdGeomGetStageMetersPerUnit(Stage);
}

void UsdUtils::SetUsdStageMetersPerUnit(const pxr::UsdStageRefPtr& Stage, float MetersPerUnit)
{
	if (!Stage || !Stage->GetRootLayer())
	{
		return;
	}

	pxr::UsdEditContext(Stage, Stage->GetRootLayer());
	pxr::UsdGeomSetStageMetersPerUnit(Stage, MetersPerUnit);
}

int32 UsdUtils::GetUsdStageNumFrames(const pxr::UsdStageRefPtr& Stage)
{
	// USD time code range is inclusive on both ends
	return Stage ? FMath::Abs(FMath::CeilToInt32(Stage->GetEndTimeCode()) - FMath::FloorToInt32(Stage->GetStartTimeCode()) + 1) : 0;
}

bool UsdUtils::HasCompositionArcs(const pxr::UsdPrim& Prim)
{
	if (!Prim || !Prim.IsActive())
	{
		return false;
	}

	return Prim.HasAuthoredReferences() || Prim.HasAuthoredPayloads() || Prim.HasAuthoredInherits() || Prim.HasAuthoredSpecializes()
		   || Prim.HasVariantSets();
}

bool UsdUtils::HasCompositionArcs(const pxr::SdfPrimSpecHandle& PrimSpec)
{
	if (!PrimSpec || !PrimSpec->GetActive())
	{
		return false;
	}

	return PrimSpec->HasReferences() || PrimSpec->HasPayloads() || PrimSpec->HasInheritPaths() || PrimSpec->HasSpecializes()
		   || PrimSpec->HasVariantSetNames();
}

UClass* UsdUtils::GetActorTypeForPrim(const pxr::UsdPrim& Prim)
{
	// If we have this attribute and a valid child camera prim then we'll assume
	// we correspond to the root scene component of an exported cine camera actor. Let's assume
	// then that we have an actual ACineCameraActor class so that the schema translators can
	// reuse the main UCineCameraComponent for the actual child camera prim
	bool bIsCineCameraActorRootComponent = false;
	if (pxr::UsdAttribute Attr = Prim.GetAttribute(UnrealToUsd::ConvertToken(TEXT("unrealCameraPrimName")).Get()))
	{
		pxr::TfToken CameraComponentPrim;
		if (Attr.Get<pxr::TfToken>(&CameraComponentPrim))
		{
			pxr::UsdPrim ChildCameraPrim = Prim.GetChild(CameraComponentPrim);
			if (ChildCameraPrim && ChildCameraPrim.IsA<pxr::UsdGeomCamera>())
			{
				bIsCineCameraActorRootComponent = true;
			}
		}
	}

	if (Prim.IsA<pxr::UsdGeomCamera>() || bIsCineCameraActorRootComponent)
	{
		return ACineCameraActor::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdLuxDistantLight>())
	{
		return ADirectionalLight::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdLuxRectLight>() || Prim.IsA<pxr::UsdLuxDiskLight>())
	{
		return ARectLight::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdLuxSphereLight>())
	{
		if (Prim.HasAPI<pxr::UsdLuxShapingAPI>())
		{
			return ASpotLight::StaticClass();
		}
		else
		{
			return APointLight::StaticClass();
		}
	}
	else if (Prim.IsA<pxr::UsdLuxDomeLight>())
	{
		return ASkyLight::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdVolVolume>())
	{
		return AHeterogeneousVolume::StaticClass();
	}
	else
	{
		return AActor::StaticClass();
	}
}

UClass* UsdUtils::GetComponentTypeForPrim(const pxr::UsdPrim& Prim)
{
	if (Prim.IsA<pxr::UsdSkelSkeleton>())
	{
		return USkeletalMeshComponent::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdGeomGprim>())
	{
		return UStaticMeshComponent::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdGeomCamera>())
	{
		return UCineCameraComponent::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdLuxDistantLight>())
	{
		return UDirectionalLightComponent::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdLuxRectLight>() || Prim.IsA<pxr::UsdLuxDiskLight>())
	{
		return URectLightComponent::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdLuxSphereLight>())
	{
		if (Prim.HasAPI<pxr::UsdLuxShapingAPI>())
		{
			return USpotLightComponent::StaticClass();
		}
		else
		{
			return UPointLightComponent::StaticClass();
		}
	}
	else if (Prim.IsA<pxr::UsdLuxDomeLight>())
	{
		return USkyLightComponent::StaticClass();
	}
	else if (Prim.IsA<pxr::UsdGeomXformable>())
	{
		return USceneComponent::StaticClass();
	}
	else
	{
		return nullptr;
	}
}

FString UsdUtils::GetSchemaNameForComponent(const USceneComponent& Component)
{
	AActor* OwnerActor = Component.GetOwner();
	if (OwnerActor->IsA<AInstancedFoliageActor>())
	{
		return TEXT("PointInstancer");
	}
	else if (OwnerActor->IsA<ALandscapeProxy>())
	{
		return TEXT("Mesh");
	}

	if (Component.IsA<USkinnedMeshComponent>())
	{
		return TEXT("SkelRoot");
	}
	else if (Component.IsA<UHierarchicalInstancedStaticMeshComponent>())
	{
		// The original HISM component becomes just a regular Xform prim, so that we can handle
		// its children correctly. We'll manually create a new child PointInstancer prim to it
		// however, and convert the HISM data onto that prim.
		return TEXT("Xform");
	}
	else if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(&Component))
	{
		UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
		if (Mesh && Mesh->GetNumLODs() > 1)
		{
			// Don't export 'Mesh' if we're going to export LODs, as those will also be Mesh prims.
			// We need at least an Xform schema though as this component may still have a transform of its own
			return TEXT("Xform");
		}
		return TEXT("Mesh");
	}
	else if (Component.IsA<UCineCameraComponent>())
	{
		return TEXT("Camera");
	}
	else if (Component.IsA<UDirectionalLightComponent>())
	{
		return TEXT("DistantLight");
	}
	else if (Component.IsA<URectLightComponent>())
	{
		return TEXT("RectLight");
	}
	else if (Component.IsA<UPointLightComponent>())
	{
		return TEXT("SphereLight");
	}
	else if (Component.IsA<USkyLightComponent>())
	{
		return TEXT("DomeLight");
	}

	return TEXT("Xform");
}

FString UsdUtils::GetPrimPathForObject(const UObject* ActorOrComponent, const FString& ParentPrimPath, bool bUseActorFolders)
{
	if (!ActorOrComponent)
	{
		return {};
	}

	// Get component and its owner actor
	const USceneComponent* Component = Cast<const USceneComponent>(ActorOrComponent);
	const AActor* Owner = nullptr;
	if (Component)
	{
		Owner = Component->GetOwner();
	}
	else
	{
		Owner = Cast<AActor>(ActorOrComponent);
		if (Owner)
		{
			Component = Owner->GetRootComponent();
		}
	}
	if (!Component || !Owner)
	{
		return {};
	}

	// Get component name. Use actor label if the component is its root component
	FString Path;
#if WITH_EDITOR
	if (Component == Owner->GetRootComponent())
	{
		Path = Owner->GetActorLabel();
	}
	else
#endif	  // WITH_EDITOR
	{
		Path = Component->GetName();
	}
	Path = UsdUtils::SanitizeUsdIdentifier(*Path);

	// Get a clean folder path string if we have and need one
	FString FolderPathString;
#if WITH_EDITOR
	if (bUseActorFolders && Component == Owner->GetRootComponent())
	{
		const FName& FolderPath = Owner->GetFolderPath();
		if (!FolderPath.IsNone())
		{
			FolderPathString = FolderPath.ToString();

			TArray<FString> FolderSegments;
			FolderPathString.ParseIntoArray(FolderSegments, TEXT("/"));

			for (FString& Segment : FolderSegments)
			{
				Segment = UsdUtils::SanitizeUsdIdentifier(*Segment);
			}

			FolderPathString = FString::Join(FolderSegments, TEXT("/"));

			if (!FolderPathString.IsEmpty())
			{
				Path = FolderPathString / Path;
			}
		}
	}
#endif	  // WITH_EDITOR

	// Get parent prim path if we need to
	if (!ParentPrimPath.IsEmpty())
	{
		Path = ParentPrimPath / Path;
	}
	else
	{
		FString FoundParentPath;

		if (USceneComponent* ParentComp = Component->GetAttachParent())
		{
			FoundParentPath = GetPrimPathForObject(ParentComp, TEXT(""), bUseActorFolders);
		}
		else
		{
			FoundParentPath = TEXT("/Root");
		}

		Path = FoundParentPath / Path;
	}

	return Path;
}

TUsdStore<pxr::TfToken> UsdUtils::GetUVSetName(int32 UVChannelIndex)
{
	FScopedUnrealAllocs UnrealAllocs;

	FString UVSetName = TEXT("primvars:st");

	if (UVChannelIndex > 0)
	{
		UVSetName += LexToString(UVChannelIndex);
	}

	TUsdStore<pxr::TfToken> UVSetNameToken = MakeUsdStore<pxr::TfToken>(UnrealToUsd::ConvertString(*UVSetName).Get());

	return UVSetNameToken;
}

int32 UsdUtils::GetPrimvarUVIndex(FString PrimvarName)
{
	int32 Index = PrimvarName.Len();
	while (Index > 0 && PrimvarName[Index - 1] >= '0' && PrimvarName[Index - 1] <= '9')
	{
		--Index;
	}

	if (Index < PrimvarName.Len())
	{
		return FCString::Atoi(*PrimvarName.RightChop(Index));
	}

	return 0;
}

TArray<TUsdStore<pxr::UsdGeomPrimvar>> UsdUtils::GetUVSetPrimvars(const pxr::UsdPrim& UsdPrim, int32 MaxNumPrimvars)
{
	if (!UsdPrim)
	{
		return {};
	}

	FScopedUsdAllocs Allocs;

	TArray<TUsdStore<pxr::UsdGeomPrimvar>> TexCoord2fPrimvars;
	TArray<TUsdStore<pxr::UsdGeomPrimvar>> Float2Primvars;

	// Collect all primvars that could be used as UV sets
	pxr::UsdGeomPrimvarsAPI PrimvarsAPI{UsdPrim};
	for (const pxr::UsdGeomPrimvar& Primvar : PrimvarsAPI.GetPrimvars())
	{
		if (!Primvar || !Primvar.HasValue())
		{
			continue;
		}

		// We only care about primvars that can be used as float2[]. TexCoord2f is included
		const pxr::SdfValueTypeName& TypeName = Primvar.GetTypeName();
		if (!TypeName.GetType().IsA(pxr::SdfValueTypeNames->Float2Array.GetType()))
		{
			continue;
		}

		if (Primvar.GetTypeName().GetRole() == pxr::SdfValueTypeNames->TexCoord2f.GetRole())
		{
			TexCoord2fPrimvars.Add(Primvar);
		}
		else if (GParseUVSetsFromFloat2Primvars)
		{
			Float2Primvars.Add(Primvar);
		}
	}

	TexCoord2fPrimvars.Sort(
		[](const TUsdStore<pxr::UsdGeomPrimvar>& A, const TUsdStore<pxr::UsdGeomPrimvar>& B)
		{
			return A.Get().GetName() < B.Get().GetName();
		}
	);
	Float2Primvars.Sort(
		[](const TUsdStore<pxr::UsdGeomPrimvar>& A, const TUsdStore<pxr::UsdGeomPrimvar>& B)
		{
			return A.Get().GetName() < B.Get().GetName();
		}
	);

	TArray<TUsdStore<pxr::UsdGeomPrimvar>> Result;
	Result.Reserve(FMath::Min(TexCoord2fPrimvars.Num() + Float2Primvars.Num(), MaxNumPrimvars));

	int32 TexCoordPrimvarIndex = 0;
	while (Result.Num() < MaxNumPrimvars && TexCoord2fPrimvars.IsValidIndex(TexCoordPrimvarIndex))
	{
		Result.Add(TexCoord2fPrimvars[TexCoordPrimvarIndex++]);
	}

	int32 Float2PrimvarIndex = 0;
	while (Result.Num() < MaxNumPrimvars && Float2Primvars.IsValidIndex(Float2PrimvarIndex))
	{
		Result.Add(Float2Primvars[Float2PrimvarIndex++]);
	}

	return Result;
}

TArray<TUsdStore<pxr::UsdGeomPrimvar>> UsdUtils::GetUVSetPrimvars(
	const pxr::UsdGeomMesh& UsdMesh,
	const TMap<FString, TMap<FString, int32>>& MaterialToPrimvarsUVSetNames,
	const pxr::TfToken& RenderContext,
	const pxr::TfToken& MaterialPurpose
)
{
	return UsdUtils::GetUVSetPrimvars(UsdMesh.GetPrim());
}

TArray<TUsdStore<pxr::UsdGeomPrimvar>> UsdUtils::GetUVSetPrimvars(
	const pxr::UsdGeomMesh& UsdMesh,
	const TMap<FString, TMap<FString, int32>>& MaterialToPrimvarsUVSetNames,
	const UsdUtils::FUsdPrimMaterialAssignmentInfo& UsdMeshMaterialAssignmentInfo
)
{
	return UsdUtils::GetUVSetPrimvars(UsdMesh.GetPrim());
}

TArray<TUsdStore<pxr::UsdGeomPrimvar>> UsdUtils::AssemblePrimvarsIntoUVSets(
	const TArray<TUsdStore<pxr::UsdGeomPrimvar>>& AllMeshUVPrimvars,
	const TMap<FString, int32>& AllowedPrimvarsToUVIndex
)
{
	TArray<TUsdStore<pxr::UsdGeomPrimvar>> PrimvarsByUVIndex;

	if (AllowedPrimvarsToUVIndex.Num() > 0)
	{
		for (const TUsdStore<pxr::UsdGeomPrimvar>& MeshUVPrimvar : AllMeshUVPrimvars)
		{
			FString PrimvarName = UsdToUnreal::ConvertToken(MeshUVPrimvar.Get().GetName());
			PrimvarName.RemoveFromStart(TEXT("primvars:"));

			if (const int32* FoundTargetUVIndex = AllowedPrimvarsToUVIndex.Find(PrimvarName))
			{
				int32 TargetUVIndex = *FoundTargetUVIndex;
				if (TargetUVIndex < 0)
				{
					continue;
				}

				if (!PrimvarsByUVIndex.IsValidIndex(TargetUVIndex))
				{
					if (TargetUVIndex < USD_PREVIEW_SURFACE_MAX_UV_SETS)
					{
						PrimvarsByUVIndex.SetNum(TargetUVIndex + 1);
					}
					else
					{
						continue;
					}
				}

				TUsdStore<pxr::UsdGeomPrimvar>& ExistingPrimvar = PrimvarsByUVIndex[TargetUVIndex];
				if (!ExistingPrimvar.Get())
				{
					PrimvarsByUVIndex[TargetUVIndex] = MeshUVPrimvar;
				}
			}
		}
	}

	return PrimvarsByUVIndex;
}

TMap<FString, int32> UsdUtils::AssemblePrimvarsIntoPrimvarToUVIndexMap(const TArray<TUsdStore<pxr::UsdGeomPrimvar>>& AllMeshUVPrimvars)
{
	TMap<FString, int32> Result;
	Result.Reserve(AllMeshUVPrimvars.Num());

	for (int32 UVIndex = 0; UVIndex < AllMeshUVPrimvars.Num(); ++UVIndex)
	{
		const TUsdStore<pxr::UsdGeomPrimvar>& Primvar = AllMeshUVPrimvars[UVIndex];
		FString PrimvarName = UsdToUnreal::ConvertToken(Primvar.Get().GetName());
		PrimvarName.RemoveFromStart(TEXT("primvars:"));

		Result.Add(PrimvarName, UVIndex);
	}

	return Result;
}

TMap<FString, int32> UsdUtils::CombinePrimvarsIntoUVSets(const TSet<FString>& AllPrimvars, const TSet<FString>& PreferredPrimvars)
{
	TArray<FString> SortedPrimvars = AllPrimvars.Array();

	// Promote a deterministic primvar-to-UV-index assignment preferring texCoord2f primvars
	SortedPrimvars.Sort(
		[&PreferredPrimvars](const FString& LHS, const FString& RHS)
		{
			const bool bLHSPreferred = PreferredPrimvars.Contains(LHS);
			const bool bRHSPreferred = PreferredPrimvars.Contains(RHS);
			if (bLHSPreferred == bRHSPreferred)
			{
				return LHS < RHS;
			}
			else
			{
				return bLHSPreferred < bRHSPreferred;
			}
		}
	);

	// We can only have up to USD_PREVIEW_SURFACE_MAX_UV_SETS UV sets
	SortedPrimvars.SetNum(FMath::Min(SortedPrimvars.Num(), (int32)USD_PREVIEW_SURFACE_MAX_UV_SETS));

	TMap<FString, int32> PrimvarToUVIndex;
	PrimvarToUVIndex.Reserve(SortedPrimvars.Num());
	int32 UVIndex = 0;
	for (const FString& Primvar : SortedPrimvars)
	{
		PrimvarToUVIndex.Add(Primvar, UVIndex++);
	}

	return PrimvarToUVIndex;
}

bool UsdUtils::IsAnimated(const pxr::UsdPrim& Prim)
{
	if (!Prim || !Prim.IsActive())
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdGeomXformable Xformable(Prim);
	if (Xformable)
	{
		std::vector<double> TimeSamples;
		Xformable.GetTimeSamples(&TimeSamples);

		if (TimeSamples.size() > 0)
		{
			return true;
		}

		// If this xformable has an op to reset the xform stack and one of its ancestors is animated, then we need to pretend
		// its transform is also animated. This because that op effectively means "discard the parent transform and treat this
		// as a direct world transform", but when reading we'll manually recompute the relative transform to its parent anyway
		// (for simplicity's sake). If that parent (or any of its ancestors) is being animated, we'll need to recompute this
		// for every animation keyframe, which basically means we're animated too
		if (Xformable.GetResetXformStack())
		{
			pxr::UsdPrim AncestorPrim = Prim.GetParent();
			while (AncestorPrim && !AncestorPrim.IsPseudoRoot())
			{
				if (pxr::UsdGeomXformable AncestorXformable{AncestorPrim})
				{
					std::vector<double> AncestorTimeSamples;
					if (AncestorXformable.GetTimeSamples(&AncestorTimeSamples) && AncestorTimeSamples.size() > 0)
					{
						return true;
					}

					// The exception is if our ancestor also wants to reset its xform stack (i.e. its transform is meant to be
					// used as the world transform). In this case we don't need to care about higher up ancestors anymore, as
					// their transforms wouldn't affect below this prim anyway
					if (AncestorXformable.GetResetXformStack())
					{
						break;
					}
				}

				AncestorPrim = AncestorPrim.GetParent();
			}
		}
	}

	const std::vector<pxr::UsdAttribute>& Attributes = Prim.GetAttributes();
	for (const pxr::UsdAttribute& Attribute : Attributes)
	{
		std::vector<double> TimeSamples;
		if (Attribute.GetTimeSamples(&TimeSamples) && TimeSamples.size() > 0)
		{
			return true;
		}
	}

	if (pxr::UsdSkelSkeleton Skeleton{Prim})
	{
		if (pxr::UsdSkelRoot ClosestParentSkelRoot = pxr::UsdSkelRoot{UsdUtils::GetClosestParentSkelRoot(Prim)})
		{
			pxr::UsdSkelCache SkeletonCache;
			SkeletonCache.Populate(ClosestParentSkelRoot, pxr::UsdTraverseInstanceProxies());

			pxr::UsdSkelSkeletonQuery SkelQuery = SkeletonCache.GetSkelQuery(Skeleton);
			if (pxr::UsdSkelAnimQuery AnimQuery = SkelQuery.GetAnimQuery())
			{
				std::vector<double> JointTimeSamples;
				std::vector<double> BlendShapeTimeSamples;
				if ((AnimQuery.GetJointTransformTimeSamples(&JointTimeSamples) && JointTimeSamples.size() > 0)
					|| (AnimQuery.GetBlendShapeWeightTimeSamples(&BlendShapeTimeSamples) && BlendShapeTimeSamples.size() > 0))
				{
					return true;
				}
			}
		}
	}
	else if (pxr::UsdVolVolume Volume{Prim})
	{
		pxr::UsdStageRefPtr Stage = Prim.GetStage();

		const std::map<pxr::TfToken, pxr::SdfPath>& FieldMap = Volume.GetFieldPaths();
		for (std::map<pxr::TfToken, pxr::SdfPath>::const_iterator Iter = FieldMap.cbegin(); Iter != FieldMap.cend(); ++Iter)
		{
			const pxr::SdfPath& AssetPrimPath = Iter->second;

			if (pxr::UsdVolOpenVDBAsset OpenVDBAsset{Stage->GetPrimAtPath(AssetPrimPath)})
			{
				std::vector<double> TimeSamples;
				pxr::UsdAttribute FilePathAttr = OpenVDBAsset.GetFilePathAttr();
				if (FilePathAttr && FilePathAttr.GetTimeSamples(&TimeSamples) && TimeSamples.size() > 1)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool UsdUtils::HasAnimatedVisibility(const pxr::UsdPrim& Prim)
{
	if (!Prim || !Prim.IsActive())
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdGeomImageable Imageable(Prim);
	if (Imageable)
	{
		if (pxr::UsdAttribute Attr = Imageable.GetVisibilityAttr())
		{
			std::vector<double> TimeSamples;
			if (Attr.GetTimeSamples(&TimeSamples) && TimeSamples.size() > 0)
			{
				return true;
			}
		}
	}

	return false;
}

namespace UE::ConversionUtilsImpl::Private
{
	// Convenience function so we don't have to spell this out every time
	inline void CollectTimeSamplesIfNeeded(bool bCollectTimeSamples, const pxr::UsdAttribute& Attr, TArray<double>* OutTimeSamples)
	{
		std::vector<double> TempTimeSamples;
		if (bCollectTimeSamples && Attr.GetTimeSamples(&TempTimeSamples))
		{
			OutTimeSamples->Append(TempTimeSamples.data(), TempTimeSamples.size());
		}
	}

	bool GetOrCollectAnimatedBounds(
		const pxr::UsdPrim& Prim,
		TArray<double>* OutTimeSamples,
		bool bCollectTimeSamples,
		bool bIsParentPrim,
		EUsdPurpose IncludedPurposes,
		bool bUseExtentsHint,
		bool bIgnoreVisibility
	)
	{
		if (!Prim)
		{
			return false;
		}

		// If we want to collect timeSamples we must have some place to put them in
		if (!ensure(!bCollectTimeSamples || OutTimeSamples))
		{
			return false;
		}

		FScopedUsdAllocs UsdAllocs;

		// If the prim is fully invisible due to visibility or purpose then we shouldn't even check it
		bool bHasAnimatedVisibility = false;
		if (pxr::UsdGeomImageable Imageable{Prim}; !bIgnoreVisibility && Imageable)
		{
			if (pxr::UsdAttribute Visibility = Imageable.GetVisibilityAttr())
			{
				// Keep track of this for later
				bHasAnimatedVisibility = Visibility.ValueMightBeTimeVarying();

				if (bHasAnimatedVisibility)
				{
					CollectTimeSamplesIfNeeded(bCollectTimeSamples, Visibility, OutTimeSamples);
				}
				else
				{
					pxr::TfToken VisibilityToken;
					if (!bIsParentPrim && Visibility.Get(&VisibilityToken) && VisibilityToken == pxr::UsdGeomTokens->invisible)
					{
						// We don't "propagate the (in)visibility token", we just flat out stop recursing and abandon the subtree
						return false;
					}
				}
			}
		}
		if (!bIsParentPrim && !EnumHasAllFlags(IncludedPurposes, IUsdPrim::GetPurpose(Prim)))
		{
			return false;
		}

		// If the prim has authored animated extents we know we're fully done, because our computed bounds
		// will also need to be animated and will read *exclusively* from these anyway.
		// We don't even need to collect any further timeSamples from child prims after this, as we will be ignoring individual
		// animations on random prims in the subtree and instead just using the authored extent animation.
		// Also, extentsHint is preferred over extent, so check for that first.
		if (pxr::UsdGeomModelAPI GeomModelAPI{Prim}; bUseExtentsHint && GeomModelAPI)
		{
			if (pxr::UsdAttribute ExtentsHint = GeomModelAPI.GetExtentsHintAttr())
			{
				if (ExtentsHint.HasAuthoredValue())
				{
					CollectTimeSamplesIfNeeded(bCollectTimeSamples, ExtentsHint, OutTimeSamples);
					return ExtentsHint.ValueMightBeTimeVarying();
				}
			}
		}
		if (pxr::UsdGeomBoundable Boundable{Prim})
		{
			if (pxr::UsdAttribute Extent = Boundable.GetExtentAttr())
			{
				// If we have authored extent or extentsHint (even if not animated, i.e. just default opinions), the
				// BBoxCache will refuse to compute bounds at any timeCode and just fallback to using the authored stuff
				if (Extent.HasAuthoredValue())
				{
					CollectTimeSamplesIfNeeded(bCollectTimeSamples, Extent, OutTimeSamples);
					return Extent.ValueMightBeTimeVarying();
				}
			}
		}

		// It's visible at the default timeCode, but has animated visibility. This means
		// it could affect the bounds as it becomes visible or invisible, so just return now.
		if (!bCollectTimeSamples && bHasAnimatedVisibility)
		{
			return true;
		}

		bool bHasAnimatedBounds = bHasAnimatedVisibility;

		// Otherwise the prim may have some animated attributes that would make our parent extents animated.
		// For this function we mostly care about whether the *bounds themselves* are animated.
		// The parent prim having animated transform means we'll just put this transform on the component itself,
		// but the bounds could remain un-animated
		if (pxr::UsdGeomXformable Xformable{Prim}; !bIsParentPrim && Xformable)
		{
			if (Xformable.TransformMightBeTimeVarying())
			{
				bHasAnimatedBounds = true;

				std::vector<double> TempTimeSamples;
				if (bCollectTimeSamples && Xformable.GetTimeSamples(&TempTimeSamples))
				{
					OutTimeSamples->Append(TempTimeSamples.data(), TempTimeSamples.size());
				}
			}
		}
		else if (pxr::UsdGeomPointBased PointBased{Prim})
		{
			if (pxr::UsdAttribute Points = PointBased.GetPointsAttr())
			{
				if (Points.ValueMightBeTimeVarying())
				{
					CollectTimeSamplesIfNeeded(bCollectTimeSamples, Points, OutTimeSamples);
					bHasAnimatedBounds = true;
				}
			}
		}
		else if (pxr::UsdGeomPointInstancer PointInstancer{Prim})
		{
			if (pxr::UsdAttribute Positions = PointInstancer.GetPositionsAttr())
			{
				if (Positions.ValueMightBeTimeVarying())
				{
					CollectTimeSamplesIfNeeded(bCollectTimeSamples, Positions, OutTimeSamples);
					bHasAnimatedBounds = true;
				}
			}
		}
		// Check for a SkelRoot with SkelAnimation
		else if (UE::FUsdPrim SkelAnimationPrim = UsdUtils::FindFirstAnimationSource(UE::FUsdPrim{Prim}))
		{
			pxr::UsdSkelAnimation SkelAnim{SkelAnimationPrim};
			if (ensure(SkelAnim))
			{
				const bool bIncludeInherited = false;
				for (const pxr::TfToken& SkelAnimAttrName : SkelAnim.GetSchemaAttributeNames(bIncludeInherited))
				{
					if (pxr::UsdAttribute Attr = SkelAnim.GetPrim().GetAttribute(SkelAnimAttrName))
					{
						if (Attr.ValueMightBeTimeVarying())
						{
							bHasAnimatedBounds = true;

							if (!bCollectTimeSamples)
							{
								break;
							}
							CollectTimeSamplesIfNeeded(bCollectTimeSamples, Attr, OutTimeSamples);
						}
					}
				}
			}
		}

		// If we're not collecting timeSamples and we run into a prim with animated bounds then we know that
		// we're done, and can return then. If we're collecting timeSamples however then we want instead to remember
		// that we found those animated bounds, but still try to step into children in case they also had animated
		// bounds and additional timeSamples
		if (!bCollectTimeSamples && bHasAnimatedBounds)
		{
			return true;
		}

		for (pxr::UsdPrim Child : Prim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate)))
		{
			const bool bChildIsParentPrim = false;
			bHasAnimatedBounds |= GetOrCollectAnimatedBounds(
				Child,
				OutTimeSamples,
				bCollectTimeSamples,
				bChildIsParentPrim,
				IncludedPurposes,
				bUseExtentsHint,
				bIgnoreVisibility
			);

			// Don't need to visit any other children, we're done here
			if (!bCollectTimeSamples && bHasAnimatedBounds)
			{
				return true;
			}
		}

		return bHasAnimatedBounds;
	}
}	 // namespace UE::ConversionUtilsImpl::Private

bool UsdUtils::HasAnimatedBounds(const pxr::UsdPrim& Prim, EUsdPurpose IncludedPurposes, bool bUseExtentsHint, bool bIgnoreVisibility)
{
	// "ParentPrim" here because there are slight differences in behavior between handling the actual provided
	// prim and another random prim in its subtree (for which bIsParentPrim will be 'false')
	const bool bIsParentPrim = true;
	const bool bCollectTimeSamples = false;
	TArray<double>* OutTimeSamples = nullptr;
	return UE::ConversionUtilsImpl::Private::GetOrCollectAnimatedBounds(
		Prim,
		OutTimeSamples,
		bCollectTimeSamples,
		bIsParentPrim,
		IncludedPurposes,
		bUseExtentsHint,
		bIgnoreVisibility
	);
}

bool UsdUtils::GetAnimatedBoundsTimeSamples(
	const pxr::UsdPrim& InPrim,
	TArray<double>& OutTimeSamples,
	EUsdPurpose InIncludedPurposes,
	bool bInUseExtentsHint,
	bool bInIgnoreVisibility
)
{
	OutTimeSamples.Reset();

	// "ParentPrim" here because there are slight differences in behavior between handling the actual provided
	// prim and another random prim in its subtree (for which bIsParentPrim will be 'false')
	const bool bIsParentPrim = true;
	const bool bCollectTimeSamples = true;
	const bool bHasAnimatedBounds = UE::ConversionUtilsImpl::Private::GetOrCollectAnimatedBounds(
		InPrim,
		&OutTimeSamples,
		bCollectTimeSamples,
		bIsParentPrim,
		InIncludedPurposes,
		bInUseExtentsHint,
		bInIgnoreVisibility
	);

	OutTimeSamples.Sort();

	return bHasAnimatedBounds;
}

EUsdDefaultKind UsdUtils::GetDefaultKind(const pxr::UsdPrim& Prim)
{
	FScopedUsdAllocs Allocs;

	pxr::UsdModelAPI Model{pxr::UsdTyped(Prim)};

	EUsdDefaultKind Result = EUsdDefaultKind::None;

	if (!Model)
	{
		return Result;
	}

	// We need KindValidationNone here or else we get inconsistent results when a prim references another prim that is a component.
	// For example, when referencing a component prim in another file, this returns 'true' if the referencer is a root prim,
	// but false if the referencer is within another Xform prim, for whatever reason.
	if (Model.IsKind(pxr::KindTokens->model, pxr::UsdModelAPI::KindValidationNone))
	{
		Result |= EUsdDefaultKind::Model;
	}

	if (Model.IsKind(pxr::KindTokens->component, pxr::UsdModelAPI::KindValidationNone))
	{
		Result |= EUsdDefaultKind::Component;
	}

	if (Model.IsKind(pxr::KindTokens->group, pxr::UsdModelAPI::KindValidationNone))
	{
		Result |= EUsdDefaultKind::Group;
	}

	if (Model.IsKind(pxr::KindTokens->assembly, pxr::UsdModelAPI::KindValidationNone))
	{
		Result |= EUsdDefaultKind::Assembly;
	}

	if (Model.IsKind(pxr::KindTokens->subcomponent, pxr::UsdModelAPI::KindValidationNone))
	{
		Result |= EUsdDefaultKind::Subcomponent;
	}

	return Result;
}

bool UsdUtils::SetDefaultKind(pxr::UsdPrim& Prim, EUsdDefaultKind NewKind)
{
	if (!Prim)
	{
		return false;
	}

	FScopedUsdAllocs Allocs;

	const int32 NewKindInt = static_cast<int32>(NewKind);
	const bool bSingleFlagSet = NewKindInt != 0 && (NewKindInt & (NewKindInt - 1)) == 0;
	if (!bSingleFlagSet)
	{
		return false;
	}

	pxr::TfToken NewKindToken;
	switch (NewKind)
	{
		default:
		case EUsdDefaultKind::Model:
		{
			NewKindToken = pxr::KindTokens->model;
			break;
		}
		case EUsdDefaultKind::Component:
		{
			NewKindToken = pxr::KindTokens->component;
			break;
		}
		case EUsdDefaultKind::Group:
		{
			NewKindToken = pxr::KindTokens->group;
			break;
		}
		case EUsdDefaultKind::Assembly:
		{
			NewKindToken = pxr::KindTokens->assembly;
			break;
		}
		case EUsdDefaultKind::Subcomponent:
		{
			NewKindToken = pxr::KindTokens->subcomponent;
			break;
		}
	}
	if (NewKindToken.IsEmpty())
	{
		return false;
	}

	return IUsdPrim::SetKind(Prim, NewKindToken);
}

EUsdDrawMode UsdUtils::GetAppliedDrawMode(const pxr::UsdPrim& Prim)
{
	// Reference: https://openusd.org/release/api/class_usd_geom_model_a_p_i.html#UsdGeomModelAPI_drawMode

	if (!Prim)
	{
		return EUsdDrawMode::Default;
	}

	FScopedUsdAllocs Allocs;

	// Only "models" should have these (i.e. uninterrupted chain of authored "kind"s back to the root prim)
	if (!Prim.IsModel())
	{
		return EUsdDrawMode::Default;
	}

	pxr::UsdGeomModelAPI GeomModelAPI{Prim};
	if (!GeomModelAPI)
	{
		return EUsdDrawMode::Default;
	}

	bool bHasAuthoredApply = false;
	bool bShouldApplyFromAttr = false;
	pxr::UsdAttribute Attr = GeomModelAPI.GetModelApplyDrawModeAttr();
	if (Attr && Attr.HasAuthoredValue() && Attr.Get(&bShouldApplyFromAttr))
	{
		if (!bShouldApplyFromAttr)
		{
			return EUsdDrawMode::Default;
		}

		bHasAuthoredApply = true;
	}

	// "Models of kind component are treated as if model:applyDrawMode were true"
	// According to UsdImagingDelegate::_IsDrawModeApplied this only works as a "fallback" though:
	// if the prim has authored whether to apply or not we always use that directly
	pxr::UsdModelAPI Model{Prim};
	const bool bIsComponentKind = Model && Model.IsKind(pxr::KindTokens->component, pxr::UsdModelAPI::KindValidationNone);
	if (!bHasAuthoredApply && !bIsComponentKind)
	{
		return EUsdDrawMode::Default;
	}

	// Note: We can provide the parent draw mode to optimize the ComputeModelDrawMode call if it becomes an issue
	pxr::TfToken DesiredDrawMode = GeomModelAPI.ComputeModelDrawMode();
	if (DesiredDrawMode == pxr::UsdGeomTokens->default_)
	{
		return EUsdDrawMode::Default;
	}
	else if (DesiredDrawMode == pxr::UsdGeomTokens->origin)
	{
		return EUsdDrawMode::Origin;
	}
	else if (DesiredDrawMode == pxr::UsdGeomTokens->bounds)
	{
		return EUsdDrawMode::Bounds;
	}
	else if (DesiredDrawMode == pxr::UsdGeomTokens->cards)
	{
		return EUsdDrawMode::Cards;
	}
	else if (DesiredDrawMode == pxr::UsdGeomTokens->inherited)
	{
		// If we're using ComputeModelDrawMode we shouldn't get inherited or anything else here
		ensure(false);
		return EUsdDrawMode::Inherited;
	}
	else
	{
		ensure(false);
		return EUsdDrawMode::Default;
	}
}

TArray<TUsdStore<pxr::UsdPrim>> UsdUtils::GetAllPrimsOfType(
	const pxr::UsdPrim& StartPrim,
	const pxr::TfType& SchemaType,
	const TArray<TUsdStore<pxr::TfType>>& ExcludeSchemaTypes
)
{
	return GetAllPrimsOfType(
		StartPrim,
		SchemaType,
		[](const pxr::UsdPrim&)
		{
			return false;
		},
		ExcludeSchemaTypes
	);
}

TArray<TUsdStore<pxr::UsdPrim>> UsdUtils::GetAllPrimsOfType(
	const pxr::UsdPrim& StartPrim,
	const pxr::TfType& SchemaType,
	TFunction<bool(const pxr::UsdPrim&)> PruneChildren,
	const TArray<TUsdStore<pxr::TfType>>& ExcludeSchemaTypes
)
{
	TArray<TUsdStore<pxr::UsdPrim>> Result;

	pxr::UsdPrimRange PrimRange(StartPrim, pxr::UsdTraverseInstanceProxies());

	for (pxr::UsdPrimRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt)
	{
		bool bIsExcluded = false;

		for (const TUsdStore<pxr::TfType>& SchemaToExclude : ExcludeSchemaTypes)
		{
			if (PrimRangeIt->IsA(SchemaToExclude.Get()))
			{
				bIsExcluded = true;
				break;
			}
		}

		if (!bIsExcluded && PrimRangeIt->IsA(SchemaType))
		{
			Result.Add(*PrimRangeIt);
		}

		if (bIsExcluded || PruneChildren(*PrimRangeIt))
		{
			PrimRangeIt.PruneChildren();
		}
	}

	return Result;
}

FString UsdUtils::GetAssetPathFromPrimPath(const FString& RootContentPath, const pxr::UsdPrim& Prim)
{
	FString FinalPath;

	auto GetEnclosingModelPrim = [](const pxr::UsdPrim& Prim) -> pxr::UsdPrim
	{
		pxr::UsdPrim ModelPrim = Prim.GetParent();

		while (ModelPrim)
		{
			if (IUsdPrim::IsKindChildOf(ModelPrim, "model"))
			{
				break;
			}
			else
			{
				ModelPrim = ModelPrim.GetParent();
			}
		}

		return ModelPrim.IsValid() ? ModelPrim : Prim;
	};

	const pxr::UsdPrim& ModelPrim = GetEnclosingModelPrim(Prim);

	const FString RawPrimName = UsdToUnreal::ConvertString(Prim.GetName());

	pxr::UsdModelAPI ModelApi = pxr::UsdModelAPI(ModelPrim);

	std::string RawAssetName;
	ModelApi.GetAssetName(&RawAssetName);

	FString AssetName = UsdToUnreal::ConvertString(RawAssetName);
	FString MeshName = IUsdClassesModule::SanitizeObjectName(RawPrimName);

	FString USDPath = UsdToUnreal::ConvertString(Prim.GetPrimPath().GetString().c_str());

	pxr::SdfAssetPath AssetPath;
	if (ModelApi.GetAssetIdentifier(&AssetPath))
	{
		std::string AssetIdentifier = AssetPath.GetAssetPath();
		USDPath = UsdToUnreal::ConvertString(AssetIdentifier.c_str());

		USDPath = FPaths::ConvertRelativePathToFull(RootContentPath, USDPath);

		FPackageName::TryConvertFilenameToLongPackageName(USDPath, USDPath);
		USDPath.RemoveFromEnd(AssetName);
	}

	FString VariantName;

	if (ModelPrim.HasVariantSets())
	{
		pxr::UsdVariantSet ModelVariantSet = ModelPrim.GetVariantSet("modelingVariant");
		if (ModelVariantSet.IsValid())
		{
			std::string VariantSelection = ModelVariantSet.GetVariantSelection();

			if (VariantSelection.length() > 0)
			{
				VariantName = UsdToUnreal::ConvertString(VariantSelection.c_str());
			}
		}
	}

	if (!VariantName.IsEmpty())
	{
		USDPath = USDPath / VariantName;
	}

	USDPath.RemoveFromStart(TEXT("/"));
	USDPath.RemoveFromEnd(RawPrimName);
	FinalPath /= (USDPath / MeshName);

	return FinalPath;
}
#endif	  // #if USE_USD_SDK

TArray<UE::FUsdPrim> UsdUtils::GetAllPrimsOfType(const UE::FUsdPrim& StartPrim, const TCHAR* SchemaName)
{
	return GetAllPrimsOfType(
		StartPrim,
		SchemaName,
		[](const UE::FUsdPrim&)
		{
			return false;
		}
	);
}

TArray<UE::FUsdPrim> UsdUtils::GetAllPrimsOfType(
	const UE::FUsdPrim& StartPrim,
	const TCHAR* SchemaName,
	TFunction<bool(const UE::FUsdPrim&)> PruneChildren,
	const TArray<const TCHAR*>& ExcludeSchemaNames
)
{
	TArray<UE::FUsdPrim> Result;

#if USE_USD_SDK
	const pxr::TfType SchemaType = pxr::TfType::FindByName(TCHAR_TO_ANSI(SchemaName));

	TArray<TUsdStore<pxr::TfType>> ExcludeSchemaTypes;
	ExcludeSchemaTypes.Reserve(ExcludeSchemaNames.Num());
	for (const TCHAR* ExcludeSchemaName : ExcludeSchemaNames)
	{
		ExcludeSchemaTypes.Add(pxr::TfType(pxr::TfType::FindByName(TCHAR_TO_ANSI(ExcludeSchemaName))));
	}

	auto UsdPruneChildren = [&PruneChildren](const pxr::UsdPrim& ChildPrim) -> bool
	{
		return PruneChildren(UE::FUsdPrim(ChildPrim));
	};

	TArray<TUsdStore<pxr::UsdPrim>> UsdResult = GetAllPrimsOfType(StartPrim, SchemaType, UsdPruneChildren, ExcludeSchemaTypes);

	for (const TUsdStore<pxr::UsdPrim>& Prim : UsdResult)
	{
		Result.Emplace(Prim.Get());
	}
#endif	  // #if USE_USD_SDK

	return Result;
}

double UsdUtils::GetDefaultTimeCode()
{
#if USE_USD_SDK
	return pxr::UsdTimeCode::Default().GetValue();
#else
	return 0.0;
#endif
}

double UsdUtils::GetEarliestTimeCode()
{
#if USE_USD_SDK
	return pxr::UsdTimeCode::EarliestTime().GetValue();
#else
	return 0.0;
#endif
}

UUsdAssetImportData* UsdUtils::GetAssetImportData(UObject* Asset)
{
	UUsdAssetImportData* ImportData = nullptr;
#if WITH_EDITORONLY_DATA
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(Mesh->AssetImportData);
	}
	else if (USkeleton* Skeleton = Cast<USkeleton>(Asset))
	{
		if (USkeletalMesh* SkMesh = Skeleton->GetPreviewMesh())
		{
			ImportData = Cast<UUsdAssetImportData>(SkMesh->GetAssetImportData());
		}
	}
	else if (UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(Asset))
	{
		if (USkeletalMesh* SkMesh = PhysicsAsset->GetPreviewMesh())
		{
			ImportData = Cast<UUsdAssetImportData>(SkMesh->GetAssetImportData());
		}
	}
	else if (UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset))
	{
		// We will always have a skeleton, but not necessarily we will have a preview mesh directly
		// on the UAnimBlueprint
		if (USkeleton* AnimBPSkeleton = AnimBP->TargetSkeleton.Get())
		{
			if (USkeletalMesh* SkMesh = AnimBPSkeleton->GetPreviewMesh())
			{
				ImportData = Cast<UUsdAssetImportData>(SkMesh->GetAssetImportData());
			}
		}
	}
	else if (USkeletalMesh* SkMesh = Cast<USkeletalMesh>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(SkMesh->GetAssetImportData());
	}
	else if (UAnimSequence* SkelAnim = Cast<UAnimSequence>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(SkelAnim->AssetImportData);
	}
	else if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(Material->AssetImportData);
	}
	else if (UTexture* Texture = Cast<UTexture>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(Texture->AssetImportData);
	}
	else if (UGeometryCache* GeometryCache = Cast<UGeometryCache>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(GeometryCache->AssetImportData);
	}
	else if (UGroomAsset* Groom = Cast<UGroomAsset>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(Groom->AssetImportData);
	}
	else if (UGroomCache* GroomCache = Cast<UGroomCache>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(GroomCache->AssetImportData);
	}
	else if (UStreamableSparseVolumeTexture* SparseVolumeTexture = Cast<UStreamableSparseVolumeTexture>(Asset))
	{
		ImportData = Cast<UUsdAssetImportData>(SparseVolumeTexture->AssetImportData);
	}

#endif
	return ImportData;
}

void UsdUtils::SetAssetImportData(UObject* Asset, UAssetImportData* ImportData)
{
	if (!Asset)
	{
		return;
	}

#if WITH_EDITOR
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		Mesh->AssetImportData = ImportData;
	}
	else if (USkeletalMesh* SkMesh = Cast<USkeletalMesh>(Asset))
	{
		SkMesh->SetAssetImportData(ImportData);
	}
	else if (UAnimSequence* SkelAnim = Cast<UAnimSequence>(Asset))
	{
		SkelAnim->AssetImportData = ImportData;
	}
	else if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset))
	{
		Material->AssetImportData = ImportData;
	}
	else if (UTexture* Texture = Cast<UTexture>(Asset))
	{
		Texture->AssetImportData = ImportData;
	}
	else if (UGeometryCache* GeometryCache = Cast<UGeometryCache>(Asset))
	{
		GeometryCache->AssetImportData = ImportData;
	}
	else if (UGroomAsset* Groom = Cast<UGroomAsset>(Asset))
	{
		Groom->AssetImportData = ImportData;
	}
	else if (UGroomCache* GroomCache = Cast<UGroomCache>(Asset))
	{
		GroomCache->AssetImportData = ImportData;
	}
	else if (UStreamableSparseVolumeTexture* SparseVolumeTexture = Cast<UStreamableSparseVolumeTexture>(Asset))
	{
		SparseVolumeTexture->AssetImportData = ImportData;
	}
#endif	  // WITH_EDITOR
}

UUsdAssetUserData* UsdUtils::GetAssetUserData(const UObject* Object, TSubclassOf<UUsdAssetUserData> Class)
{
	if (!Object)
	{
		return nullptr;
	}

	if (!Class)
	{
		Class = UUsdAssetUserData::StaticClass();
	}

	const IInterface_AssetUserData* AssetUserDataInterface = Cast<const IInterface_AssetUserData>(Object);
	if (!AssetUserDataInterface)
	{
		UE_LOG(
			LogUsd,
			Warning,
			TEXT("Tried getting AssetUserData from object '%s', but the class '%s' doesn't implement the AssetUserData interface!"),
			*Object->GetPathName(),
			*Object->GetClass()->GetName()
		);
		return nullptr;
	}

	// Const cast because there is no const access of asset user data on the interface
	return Cast<UUsdAssetUserData>(const_cast<IInterface_AssetUserData*>(AssetUserDataInterface)->GetAssetUserDataOfClass(Class));
}

UUsdAssetUserData* UsdUtils::GetOrCreateAssetUserData(UObject* Object, TSubclassOf<UUsdAssetUserData> Class)
{
	if (!Object)
	{
		return nullptr;
	}

	if (!Class)
	{
		Class = UUsdAssetUserData::StaticClass();
	}

	IInterface_AssetUserData* AssetUserDataInterface = Cast<IInterface_AssetUserData>(Object);
	if (!AssetUserDataInterface)
	{
		UE_LOG(
			LogUsd,
			Warning,
			TEXT("Tried adding AssetUserData to object '%s', but it doesn't implement the AssetUserData interface!"),
			*Object->GetPathName()
		);
		return nullptr;
	}

	UUsdAssetUserData* AssetUserData = Cast<UUsdAssetUserData>(AssetUserDataInterface->GetAssetUserDataOfClass(Class));
	if (!AssetUserData)
	{
		// For now we're expecting objects to only have one instance of UUsdAssetUserData
		ensure(!AssetUserDataInterface->HasAssetUserDataOfClass(UUsdAssetUserData::StaticClass()));

		AssetUserData = NewObject<UUsdAssetUserData>(Object, Class, TEXT("UsdAssetUserData"));
		AssetUserDataInterface->AddAssetUserData(AssetUserData);
	}

	return AssetUserData;
}

bool UsdUtils::SetAssetUserData(UObject* Object, UUsdAssetUserData* AssetUserData)
{
	if (!Object)
	{
		return false;
	}

	IInterface_AssetUserData* AssetUserDataInterface = Cast<IInterface_AssetUserData>(Object);
	if (!AssetUserDataInterface)
	{
		UE_LOG(
			LogUsd,
			Warning,
			TEXT("Tried adding AssetUserData to object '%s', but it doesn't implement the AssetUserData interface!"),
			*Object->GetPathName()
		);
		return false;
	}

	while (AssetUserDataInterface->HasAssetUserDataOfClass(UUsdAssetUserData::StaticClass()))
	{
		UE_LOG(LogUsd, Verbose, TEXT("Removing old AssetUserData from object '%s' before adding a new one"), *Object->GetPathName());
		AssetUserDataInterface->RemoveUserDataOfClass(UUsdAssetUserData::StaticClass());
	}

	AssetUserDataInterface->AddAssetUserData(AssetUserData);
	return true;
}

#if USE_USD_SDK
FString UsdUtils::GetAssetHashPrefix(const pxr::UsdPrim& PrimForAsset, bool bReuseIdenticalAssets)
{
	if (!PrimForAsset || bReuseIdenticalAssets)
	{
		return FString{};
	}

	FString PrimPath = *UsdToUnreal::ConvertPath(PrimForAsset.GetPrimPath());
	FString StageIdentifier = *UsdToUnreal::ConvertString(PrimForAsset.GetStage()->GetRootLayer()->GetIdentifier());

	FSHA1 SHA1;
	SHA1.UpdateWithString(*PrimPath, PrimPath.Len());
	SHA1.UpdateWithString(*StageIdentifier, StageIdentifier.Len());

	FSHAHash Hash;
	SHA1.Final();
	SHA1.GetHash(&Hash.Hash[0]);
	return Hash.ToString() + TEXT("_");
}
#endif	  // WITH_EDITOR

namespace UE::UsdConversionUtils::Private
{
#if USE_USD_SDK
	bool PrepareToAddReferenceOrPayload(
		const UE::FUsdPrim& Prim,
		const TCHAR* AbsoluteFilePath,
		const UE::FSdfPath& TargetPrimPath,
		std::string& OutRelativeFilePath,
		pxr::SdfPath& OutPrimPath
	)
	{
		if (!Prim || !AbsoluteFilePath)
		{
			return false;
		}

		FScopedUsdAllocs UsdAllocs;

		pxr::UsdPrim UsdPrim(Prim);

		pxr::UsdStageRefPtr UsdStage = UsdPrim.GetStage();
		if (!UsdStage)
		{
			return false;
		}

		// Turn our layer path into a relative one
		FString RelativePath = AbsoluteFilePath;
		if (!RelativePath.IsEmpty())
		{
			pxr::SdfLayerHandle EditLayer = UsdPrim.GetStage()->GetEditTarget().GetLayer();

			std::string RepositoryPath = EditLayer->GetRepositoryPath().empty() ? EditLayer->GetRealPath() : EditLayer->GetRepositoryPath();

			// If we're editing an in-memory stage our root layer may not have a path yet
			// Giving an empty InRelativeTo to MakePathRelativeTo causes it to use the engine binary
			if (!RepositoryPath.empty())
			{
				FString LayerAbsolutePath = UsdToUnreal::ConvertString(RepositoryPath);
				FPaths::MakePathRelativeTo(RelativePath, *LayerAbsolutePath);
			}
		}

		// Get the target layer
		pxr::SdfLayerRefPtr TargetLayer;
		bool bIsInternalReference = false;
		if (RelativePath.IsEmpty())
		{
			TargetLayer = UsdStage->GetRootLayer();
			bIsInternalReference = true;
		}
		else
		{
			TargetLayer = pxr::SdfLayer::FindOrOpen(UnrealToUsd::ConvertString(AbsoluteFilePath).Get());
		}
		if (!TargetLayer)
		{
			return false;
		}

		// Get the target prim spec we want to reference
		pxr::SdfPrimSpecHandle TargetPrimSpec = TargetLayer->GetPrimAtPath(TargetPrimPath);
		if ((TargetPrimPath.IsEmpty() || !TargetPrimSpec) && TargetLayer->HasDefaultPrim())
		{
			TargetPrimSpec = TargetLayer->GetPrimAtPath(pxr::SdfPath::AbsoluteRootPath().AppendChild(TargetLayer->GetDefaultPrim()));
		}

		// Update UsdPrim's type so that it can handle the reference and be parsed by the proper translator
		if (!UsdPrim.GetTypeName().IsEmpty() && TargetPrimSpec)
		{
			pxr::TfToken TargetTypeName = TargetPrimSpec->GetTypeName();
			pxr::TfType TargetPrimType = pxr::UsdSchemaRegistry::GetTypeFromName(TargetTypeName);

			if (TargetPrimType.IsUnknown())
			{
				UsdPrim.ClearTypeName();
			}
			else if (!UsdPrim.IsA(TargetPrimType))
			{
				UsdPrim.SetTypeName(TargetTypeName);
			}
		}

		// We want to output no path for the prim if we received it as such, even if we already know what the path to the
		// default prim is, so that the authored reference doesn't actually specify any prim name and just refers to the
		// default prim by default. Otherwise if the default prim of the layer changed, we wouldn't update to the new prim
		OutPrimPath = TargetPrimSpec && !TargetPrimPath.IsEmpty() ? TargetPrimSpec->GetPath() : pxr::SdfPath{};
		OutRelativeFilePath = bIsInternalReference ? std::string() : UnrealToUsd::ConvertString(*RelativePath).Get();
		return true;
	}
#endif	  // #if USE_USD_SDK
}	 // namespace UE::UsdConversionUtils::Private

void UsdUtils::AddReference(
	UE::FUsdPrim& Prim,
	const TCHAR* AbsoluteFilePath,
	const UE::FSdfPath& TargetPrimPath,
	double TimeCodeOffset,
	double TimeCodeScale
)
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	// Group updates or else the SetTypeName (inside PrepareToAddReferenceOrPayload) and AddReference calls below will
	// both trigger separate resyncs of the same prim path
	pxr::SdfChangeBlock ChangeBlock;

	std::string RelativeLayerPath;
	pxr::SdfPath FinalPrimPath;
	bool bProceed = UE::UsdConversionUtils::Private::PrepareToAddReferenceOrPayload(
		Prim,
		AbsoluteFilePath,
		TargetPrimPath,
		RelativeLayerPath,
		FinalPrimPath
	);

	if (!bProceed)
	{
		return;
	}

	pxr::UsdReferences References = pxr::UsdPrim{Prim}.GetReferences();
	References.AddReference(RelativeLayerPath, FinalPrimPath, pxr::SdfLayerOffset{TimeCodeOffset, TimeCodeScale});
#endif	  // #if USE_USD_SDK
}

bool UsdUtils::GetReferenceFilePath(const UE::FUsdPrim& Prim, const FString& FileExtension, FString& OutReferenceFilePath)
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrimCompositionQuery PrimCompositionQuery = pxr::UsdPrimCompositionQuery::GetDirectReferences(Prim);
	for (const pxr::UsdPrimCompositionQueryArc& CompositionArc : PrimCompositionQuery.GetCompositionArcs())
	{
		if (CompositionArc.GetArcType() == pxr::PcpArcTypeReference)
		{
			pxr::SdfReferenceEditorProxy ReferenceEditor;
			pxr::SdfReference UsdReference;

			if (CompositionArc.GetIntroducingListEditor(&ReferenceEditor, &UsdReference))
			{
				FString AbsoluteFilePath = UsdToUnreal::ConvertString(UsdReference.GetAssetPath());

				FString Extension = FPaths::GetExtension(AbsoluteFilePath);
				if (Extension == FileExtension && FPaths::FileExists(AbsoluteFilePath))
				{
					OutReferenceFilePath = AbsoluteFilePath;
					return true;
				}
			}
		}
	}
#endif	  // #if USE_USD_SDK

	return false;
}

void UsdUtils::AddPayload(
	UE::FUsdPrim& Prim,
	const TCHAR* AbsoluteFilePath,
	const UE::FSdfPath& TargetPrimPath,
	double TimeCodeOffset,
	double TimeCodeScale
)
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	// Group updates or else the SetTypeName (inside PrepareToAddReferenceOrPayload) and AddReference calls below will
	// both trigger separate resyncs of the same prim path
	pxr::SdfChangeBlock ChangeBlock;

	std::string RelativeLayerPath;
	pxr::SdfPath FinalPrimPath;
	bool bProceed = UE::UsdConversionUtils::Private::PrepareToAddReferenceOrPayload(
		Prim,
		AbsoluteFilePath,
		TargetPrimPath,
		RelativeLayerPath,
		FinalPrimPath
	);

	if (!bProceed)
	{
		return;
	}

	pxr::UsdPayloads Payloads = pxr::UsdPrim{Prim}.GetPayloads();
	Payloads.AddPayload(RelativeLayerPath, FinalPrimPath, pxr::SdfLayerOffset{TimeCodeOffset, TimeCodeScale});
#endif	  // #if USE_USD_SDK
}

bool UsdUtils::RenamePrim(UE::FUsdPrim& Prim, const TCHAR* NewPrimName)
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	if (!Prim || !NewPrimName)
	{
		return false;
	}

	if (Prim.GetName() == FName(NewPrimName))
	{
		return false;
	}

	pxr::UsdPrim PxrUsdPrim{Prim};
	pxr::UsdStageRefPtr PxrUsdStage{Prim.GetStage()};
	if (!PxrUsdStage)
	{
		return false;
	}

	pxr::TfToken NewNameToken = UnrealToUsd::ConvertToken(NewPrimName).Get();
	pxr::SdfPath TargetPath = PxrUsdPrim.GetPrimPath().ReplaceName(NewNameToken);

	std::unordered_set<std::string> LocalLayerIdentifiers;
	const bool bIncludeSessionLayers = true;
	for (const pxr::SdfLayerHandle& Handle : PxrUsdStage->GetLayerStack(bIncludeSessionLayers))
	{
		LocalLayerIdentifiers.insert(Handle->GetIdentifier());
	}

	std::vector<pxr::SdfPrimSpecHandle> SpecStack = PxrUsdPrim.GetPrimStack();
	TArray<TPair<pxr::SdfLayerRefPtr, pxr::SdfBatchNamespaceEdit>> Edits;

	// Check if we can apply this rename, and collect error messages if we can't
	// We will only rename if we can change all specs, or else we'd split the prim
	TArray<FString> ErrorMessages;
	pxr::SdfNamespaceEditDetailVector Details;
	int32 LastDetailsSize = 0;
	bool bCanApply = true;
	for (const pxr::SdfPrimSpecHandle& Spec : SpecStack)
	{
		if (!Spec)
		{
			continue;
		}

		pxr::SdfPath SpecPath = Spec->GetPath();
		if (!SpecPath.IsPrimPath())
		{
			// Especially when it comes to variants, we can have many different specs for a prim.
			// e.g. we can simultaneously have "/Prim{Varset=}", "/Prim{Varset=Var}" and "/Prim" in there, and
			// we will fail to do anything if these paths are not prim paths
			continue;
		}

		pxr::SdfLayerRefPtr SpecLayer = Spec->GetLayer();

		// We should only rename specs on layers that are in the stage's *local* layer stack (which will include root, sublayers and
		// session layers). We shouldn't rename any spec that is created due to references/payloads to other layers, because if we do
		// we'll end up renaming the prims within those layers too, which is not what we want: For reference/payloads it's as if
		// we're just consuming the *contents* of the referenced prim, but we don't want to affect it. Another more drastic example:
		// if we were to remove the referencer prim, we don't really want to delete the referenced prim within its layer
		if (LocalLayerIdentifiers.count(SpecLayer->GetIdentifier()) == 0)
		{
			continue;
		}

		pxr::SdfBatchNamespaceEdit BatchEdit;
		BatchEdit.Add(pxr::SdfNamespaceEdit::Rename(SpecPath, NewNameToken));

		int32 CurrentNumDetails = Details.size();
		if (SpecLayer->CanApply(BatchEdit, &Details) != pxr::SdfNamespaceEditDetail::Result::Okay)
		{
			FString LayerIdentifier = UsdToUnreal::ConvertString(SpecLayer->GetIdentifier());

			// This error pushed something new into the Details vector. Get it as an error message
			FString ErrorMessage;
			if (CurrentNumDetails != LastDetailsSize)
			{
				ErrorMessage = UsdToUnreal::ConvertString(Details[CurrentNumDetails - 1].reason);
			}

			ErrorMessages.Add(FString::Printf(TEXT("\t%s: %s"), *LayerIdentifier, *ErrorMessage));
			bCanApply = false;
			// Don't break so we can collect all error messages
		}

		LastDetailsSize = CurrentNumDetails;
		Edits.Add(TPair<pxr::SdfLayerRefPtr, pxr::SdfBatchNamespaceEdit>{SpecLayer, BatchEdit});
	}

	if (!bCanApply)
	{
		UE_LOG(
			LogUsd,
			Error,
			TEXT("Failed to rename prim with path '%s' to name '%s'. Errors:\n%s"),
			*Prim.GetPrimPath().GetString(),
			NewPrimName,
			*FString::Join(ErrorMessages, TEXT("\n"))
		);

		return false;
	}

	// Actually apply the renames
	{
		pxr::SdfChangeBlock Block;

		for (const TPair<pxr::SdfLayerRefPtr, pxr::SdfBatchNamespaceEdit>& Pair : Edits)
		{
			const pxr::SdfLayerRefPtr& Layer = Pair.Key;
			const pxr::SdfBatchNamespaceEdit& Edit = Pair.Value;

			// Make sure that if the renamed prim is the layer's default prim, we also update that to match the
			// prim's new name
			pxr::UsdPrim ParentPrim = PxrUsdPrim.GetParent();
			const bool bNeedToRenameDefaultPrim = ParentPrim && ParentPrim.IsPseudoRoot() && (PxrUsdPrim.GetName() == Layer->GetDefaultPrim());

			if (!Layer->Apply(Edit))
			{
				// This should not be happening since CanApply was true, so stop doing whatever it is we're doing
				UE_LOG(
					LogUsd,
					Error,
					TEXT("Failed to rename prim with path '%s' to name '%s' in layer '%s'"),
					*Prim.GetPrimPath().GetString(),
					NewPrimName,
					*UsdToUnreal::ConvertString(Layer->GetIdentifier())
				);

				return false;
			}

			if (bNeedToRenameDefaultPrim)
			{
				Layer->SetDefaultPrim(NewNameToken);
			}
		}
	}

	// For whatever reason, if the renamed prim is within a variant set it will be left inactive (i.e. effectively deleted) post-rename by USD.
	// Here we override that with a SetActive opinion on the session layer, which will also trigger a new resync of that prim.
	//
	// We must send a separate notice for this (which is why this function can't be inside a change block) for two reasons:
	// - In order to let the transactor know that this edit is done on the session layer (so that we can have our active=true opinion there and not
	// save it to disk);
	// - Because after we apply the rename, usd *needs* to responds to notices in order to make the target path valid again. Until
	//   it does so, we can't Get/Override/Define a prim at the target path at all, and so can't set it to active.
	//
	// We can't do this *before* we rename because if we already have a prim defined/overriden on "/Root/Target", then we
	// can't apply a rename from a prim onto "/Root/Target": Meaning we'd lose all extra data we have on the prim on the session layer.
	{
		pxr::UsdEditContext EditContext{PxrUsdStage, PxrUsdStage->GetSessionLayer()};

		if (pxr::UsdPrim PostRenamePrim = PxrUsdStage->OverridePrim(TargetPath))
		{
			// We need to toggle it back and forth because whenever we undo a rename we'll rename our spec on the session layer
			// back to the original path, and that spec *already* has an active=true opinion that we set during the first rename.
			// This means that just setting it to active here wouldn't send any notice (because it already is). We need a new notice
			// to update to the fact that the child prim is now active again (the rename notice is a resync, but it already comes with the prim set to
			// inactive)
			pxr::SdfChangeBlock Block;
			const bool bActive = true;
			PostRenamePrim.SetActive(!bActive);
			PostRenamePrim.SetActive(bActive);
		}
	}

	return true;
#else
	return false;
#endif	  // #if USE_USD_SDK
}

bool UsdUtils::RemoveNumberedSuffix(FString& Prefix)
{
	if (Prefix.IsNumeric())
	{
		return false;
	}

	bool bRemoved = false;

	FString LastChar = Prefix.Right(1);
	while ((LastChar.IsNumeric() || LastChar == TEXT("_")) && Prefix.Len() > 1)
	{
		Prefix.LeftChopInline(1, EAllowShrinking::No);
		LastChar = Prefix.Right(1);

		bRemoved = true;
	}
	Prefix.Shrink();

	return bRemoved;
}

FString UsdUtils::GetUniqueName(FString Name, const TSet<FString>& UsedNames)
{
	if (!UsedNames.Contains(Name))
	{
		return Name;
	}

	const bool bRemoved = RemoveNumberedSuffix(Name);

	// Its possible that removing the suffix made it into a unique name already
	if (bRemoved && !UsedNames.Contains(Name))
	{
		return Name;
	}

	int32 Suffix = 0;
	FString Result;
	do
	{
		Result = FString::Printf(TEXT("%s_%d"), *Name, Suffix++);
	} while (UsedNames.Contains(Result));

	return Result;
}

#if USE_USD_SDK
FString UsdUtils::GetValidChildName(FString InName, const pxr::UsdPrim& ParentPrim)
{
	if (!ParentPrim)
	{
		return {};
	}

	FScopedUsdAllocs Allocs;

	TSet<FString> UsedNames;
	for (const pxr::UsdPrim& Child : ParentPrim.GetChildren())
	{
		UsedNames.Add(UsdToUnreal::ConvertToken(Child.GetName()));
	}

	return GetUniqueName(SanitizeUsdIdentifier(*InName), UsedNames);
}
#endif	  // USE_USD_SDK

FString UsdUtils::SanitizeUsdIdentifier(const TCHAR* InIdentifier)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	std::string UsdInName = UnrealToUsd::ConvertString(InIdentifier).Get();
	std::string UsdValidName = pxr::TfMakeValidIdentifier(UsdInName);

	return UsdToUnreal::ConvertString(UsdValidName);
#endif	  // USE_USD_SDK

	return InIdentifier;
}

void UsdUtils::MakeVisible(UE::FUsdPrim& Prim, double TimeCode)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim PxrUsdPrim{Prim};
	if (pxr::UsdGeomImageable Imageable{PxrUsdPrim})
	{
		Imageable.MakeVisible(TimeCode);
	}
#endif	  // USE_USD_SDK
}

void UsdUtils::MakeInvisible(UE::FUsdPrim& Prim, double TimeCode)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim PxrUsdPrim{Prim};
	if (pxr::UsdGeomImageable Imageable{PxrUsdPrim})
	{
		Imageable.MakeInvisible(TimeCode);
	}
#endif	  // USE_USD_SDK
}

bool UsdUtils::IsVisible(const UE::FUsdPrim& Prim, double TimeCode)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim PxrUsdPrim{Prim};
	if (pxr::UsdGeomImageable Imageable{PxrUsdPrim})
	{
		return Imageable.ComputeVisibility(TimeCode) == pxr::UsdGeomTokens->inherited;
	}

	return true;
#else
	return false;
#endif	  // USE_USD_SDK
}

bool UsdUtils::HasInheritedVisibility(const UE::FUsdPrim& Prim, double TimeCode)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim PxrUsdPrim{Prim};
	if (pxr::UsdGeomImageable Imageable{PxrUsdPrim})
	{
		if (pxr::UsdAttribute VisibilityAttr = Imageable.GetVisibilityAttr())
		{
			pxr::TfToken Visibility;
			if (!VisibilityAttr.Get<pxr::TfToken>(&Visibility, TimeCode))
			{
				return true;
			}

			return Visibility == pxr::UsdGeomTokens->inherited;
		}
	}

	// If it doesn't have the attribute the default is for it to be 'inherited'
	return true;
#else
	return false;
#endif	  // USE_USD_SDK
}

bool UsdUtils::HasInvisibleParent(const UE::FUsdPrim& Prim, const UE::FUsdPrim& RootPrim, double TimeCode)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim PxrUsdPrim{Prim};
	pxr::UsdPrim Parent = PxrUsdPrim.GetParent();

	while (Parent && Parent != RootPrim)
	{
		if (pxr::UsdGeomImageable Imageable{Parent})
		{
			if (pxr::UsdAttribute VisibilityAttr = Imageable.GetVisibilityAttr())
			{
				pxr::TfToken Visibility;
				if (VisibilityAttr.Get<pxr::TfToken>(&Visibility, TimeCode))
				{
					if (Visibility == pxr::UsdGeomTokens->invisible)
					{
						return true;
					}
				}
			}
		}

		Parent = Parent.GetParent();
	}
#endif	  // USE_USD_SDK

	return false;
}

TArray<UE::FUsdPrim> UsdUtils::GetVisibleChildren(const UE::FUsdPrim& Prim, EUsdPurpose AllowedPurposes)
{
	TArray<UE::FUsdPrim> VisiblePrims;

#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	TFunction<void(const pxr::UsdPrim& Prim)> RecursivelyCollectVisibleMeshes;
	RecursivelyCollectVisibleMeshes = [&RecursivelyCollectVisibleMeshes, &VisiblePrims, AllowedPurposes](const pxr::UsdPrim& Prim)
	{
		if (!Prim || !EnumHasAllFlags(AllowedPurposes, IUsdPrim::GetPurpose(Prim)))
		{
			return;
		}

		if (pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable(Prim))
		{
			if (pxr::UsdAttribute VisibilityAttr = UsdGeomImageable.GetVisibilityAttr())
			{
				pxr::TfToken VisibilityToken;
				if (VisibilityAttr.Get(&VisibilityToken) && VisibilityToken == pxr::UsdGeomTokens->invisible)
				{
					// We don't propagate the (in)visibility token, we just flat out stop recursing instead
					return;
				}
			}
		}

		VisiblePrims.Add(UE::FUsdPrim{Prim});

		for (const pxr::UsdPrim& ChildPrim : Prim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies()))
		{
			RecursivelyCollectVisibleMeshes(ChildPrim);
		}
	};
	RecursivelyCollectVisibleMeshes(Prim);
#endif	  // USE_USD_SDK

	return VisiblePrims;
}

UE::FSdfPath UsdUtils::GetPrimSpecPathForLayer(const UE::FUsdPrim& Prim, const UE::FSdfLayer& Layer)
{
	UE::FSdfPath Result;
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{Prim};
	pxr::SdfLayerRefPtr UsdLayer{Layer};
	if (!UsdPrim || !UsdLayer)
	{
		return Result;
	}

	// We may have multiple specs in the same layer if we're within a variant set (e.g "/Root/Parent/Child" and
	// "/Root{Varset=Var}Parent/Child{ChildSet=ChildVar}" and "/Root{Varset=Var}Parent/Child").
	// This function needs to return a prim path with all of its variant selections (i.e. the last example above)
	std::size_t LargestPathLength = 0;
	for (const pxr::SdfPrimSpecHandle& Spec : UsdPrim.GetPrimStack())
	{
		if (!Spec)
		{
			continue;
		}

		pxr::SdfPath SpecPath = Spec->GetPath();
		if (!SpecPath.IsPrimPath())
		{
			continue;
		}

		if (Spec->GetLayer() == UsdLayer)
		{
			const std::size_t NewPathLength = Spec->GetPath().GetString().length();
			if (NewPathLength > LargestPathLength)
			{
				Result = UE::FSdfPath{SpecPath};
			}
		}
	}

#endif	  // USE_USD_SDK
	return Result;
}

USDUTILITIES_API void UsdUtils::RemoveAllLocalPrimSpecs(const UE::FUsdPrim& Prim, const UE::FSdfLayer& Layer)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{Prim};
	if (!UsdPrim)
	{
		return;
	}

	pxr::SdfLayerRefPtr UsdLayer{Layer};
	pxr::UsdStageRefPtr UsdStage = UsdPrim.GetStage();

	std::unordered_set<std::string> LocalLayerIdentifiers;

	// We'll want to remove specs from the entire stage. We need to be careful though to only remove specs from the
	// local layer stack. If a prim within the stage has a reference/payload to another layer and we remove the
	// referencer prim, we don't want to end up removing the referenced/payload prim within its own layer too.
	if (!UsdLayer)
	{
		const bool bIncludeSessionLayers = true;
		for (const pxr::SdfLayerHandle& Handle : UsdStage->GetLayerStack(bIncludeSessionLayers))
		{
			LocalLayerIdentifiers.insert(Handle->GetIdentifier());
		}
	}

	const pxr::SdfPath TargetPath = UsdPrim.GetPrimPath();

	for (const pxr::SdfPrimSpecHandle& Spec : UsdPrim.GetPrimStack())
	{
		// For whatever reason sometimes there are invalid specs in the layer stack, so we need to be careful
		if (!Spec)
		{
			continue;
		}

		pxr::SdfPath SpecPath = Spec->GetPath();

		// Filtering by the target path is important because if X references Y, we'll actually get Y's specs within
		// X.GetPrimStack(), and we don't want to remove the referenced specs when removing the referencer.
		// We strip variant selections here because when removing something inside the variant, SpecPath will contain
		// the variant selection and look like '/PrimWithVarSet{VarSet=SomeVar}ChildPrim', but our TargetPath will
		// just look like '/PrimWithVarSet/ChildPrim' instead. These do refer to the exact same prim on the stage
		// though (when SomeVar is active at least), so we do want to remove both
		if (!SpecPath.IsPrimPath() || SpecPath.StripAllVariantSelections() != TargetPath)
		{
			continue;
		}

		pxr::SdfLayerRefPtr SpecLayer = Spec->GetLayer();
		if (UsdLayer && SpecLayer != UsdLayer)
		{
			continue;
		}

		if (!UsdLayer && LocalLayerIdentifiers.count(SpecLayer->GetIdentifier()) == 0)
		{
			continue;
		}

		UE_LOG(
			LogUsd,
			Log,
			TEXT("Removing prim spec '%s' from layer '%s'"),
			*UsdToUnreal::ConvertPath(SpecPath),
			*UsdToUnreal::ConvertString(SpecLayer->GetIdentifier())
		);
		pxr::UsdEditContext Context(UsdStage, SpecLayer);
		UsdStage->RemovePrim(SpecPath);
	}

#endif	  // USE_USD_SDK
}

bool UsdUtils::CutPrims(const TArray<UE::FUsdPrim>& Prims)
{
	bool bCopied = UsdUtils::CopyPrims(Prims);
	if (!bCopied)
	{
		return false;
	}

	for (const UE::FUsdPrim& Prim : Prims)
	{
		UsdUtils::RemoveAllLocalPrimSpecs(Prim);
	}

	return true;
}

bool UsdUtils::CopyPrims(const TArray<UE::FUsdPrim>& Prims)
{
	bool bCopiedSomething = false;

#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr UsdStage;
	for (const UE::FUsdPrim& Prim : Prims)
	{
		if (Prim)
		{
			UsdStage = pxr::UsdStageRefPtr{Prim.GetStage()};
			if (UsdStage)
			{
				break;
			}
		}
	}
	if (!UsdStage)
	{
		return false;
	}

	pxr::UsdStageRefPtr ClipboardStage = pxr::UsdStageRefPtr{UnrealUSDWrapper::GetClipboardStage()};
	if (!ClipboardStage)
	{
		return false;
	}

	pxr::SdfLayerHandle ClipboardRoot = ClipboardStage->GetRootLayer();
	if (!ClipboardRoot)
	{
		return false;
	}

	pxr::UsdStagePopulationMask Mask;
	for (const UE::FUsdPrim& Prim : Prims)
	{
		if (Prim)
		{
			Mask.Add(pxr::SdfPath{Prim.GetPrimPath()});
		}
	}
	if (Mask.IsEmpty())
	{
		return false;
	}

	pxr::UsdStageRefPtr TempStage = pxr::UsdStage::OpenMasked(UsdStage->GetRootLayer(), Mask);
	if (!TempStage)
	{
		return false;
	}

	// USD will retain instances and prototypes even when flattening, which is not what we want
	// so let's disable instancing on our temp stage before we ask it to flatten.
	// Note how we traverse the entire masked stage here, because we also need to handle the case
	// where the prim we're duplicating is not instanceable, but has instanceable children
	TArray<pxr::SdfPath> OldInstanceablePrims;
	if (TempStage->GetPrototypes().size() > 0)
	{
		pxr::UsdEditContext Context{TempStage, TempStage->GetSessionLayer()};

		pxr::UsdPrimRange PrimRange(TempStage->GetPseudoRoot());
		for (pxr::UsdPrimRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt)
		{
			if (PrimRangeIt->IsPseudoRoot())
			{
				continue;
			}

			if (PrimRangeIt->HasAuthoredInstanceable())
			{
				PrimRangeIt->SetInstanceable(false);
				OldInstanceablePrims.Add(PrimRangeIt->GetPrimPath());
			}
		}
	}

	const bool bAddSourceFileComment = false;
	pxr::SdfLayerRefPtr FlattenedLayer = TempStage->Flatten(bAddSourceFileComment);
	if (!FlattenedLayer)
	{
		return false;
	}

	// We may had to force instanceable=false on the prims we duplicated in order to get our session layer
	// opinion to disable instancing. We don't want those prims to come out with "instanceable=false" on the
	// flattened copy though, so here we clear that opinion
	for (const pxr::SdfPath& Path : OldInstanceablePrims)
	{
		if (pxr::SdfPrimSpecHandle Spec = FlattenedLayer->GetPrimAtPath(Path))
		{
			Spec->ClearInstanceable();
		}
	}

	ClipboardRoot->Clear();

	TSet<FString> UsedNames;

	for (const UE::FUsdPrim& Prim : Prims)
	{
		pxr::SdfPrimSpecHandle FlattenedPrim = FlattenedLayer->GetPrimAtPath(pxr::SdfPath{Prim.GetPrimPath()});
		if (!FlattenedPrim)
		{
			continue;
		}

		// Have to ensure the selected prims can coexist as siblings on the clipboard until being pasted.
		// Note how we don't use GetValidChildName here: That should work too, but it could fail if somebody ever
		// calls this function within a SdfChangeBlock, given that GetValidChildName relies on USD's GetChildren,
		// which could potentially yield stale results until USD actually emits the notices about these prims being
		// added.
		FString PrimName = Prim.GetName().ToString();
		FString UniqueName = GetUniqueName(SanitizeUsdIdentifier(*PrimName), UsedNames);
		UsedNames.Add(UniqueName);

		const bool bSuccess = pxr::SdfCopySpec(
			FlattenedLayer,
			FlattenedPrim->GetPath(),
			ClipboardRoot,
			pxr::SdfPath::AbsoluteRootPath().AppendChild(UnrealToUsd::ConvertToken(*UniqueName).Get())
		);
		if (!bSuccess)
		{
			continue;
		}

		bCopiedSomething = true;
		UE_LOG(LogUsd, Log, TEXT("Copied prim '%s' into the clipboard"), *Prim.GetPrimPath().GetString());
	}
#endif	  // USE_USD_SDK

	return bCopiedSomething;
}

TArray<UE::FSdfPath> UsdUtils::PastePrims(const UE::FUsdPrim& ParentPrim)
{
	TArray<UE::FSdfPath> Result;

#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdParentPrim{ParentPrim};
	if (!UsdParentPrim)
	{
		return Result;
	}

	pxr::UsdStageRefPtr UsdStage = UsdParentPrim.GetStage();
	if (!UsdStage)
	{
		return Result;
	}

	pxr::UsdStageRefPtr ClipboardStage = pxr::UsdStageRefPtr{UnrealUSDWrapper::GetClipboardStage()};
	if (!ClipboardStage)
	{
		return Result;
	}

	pxr::SdfLayerHandle ClipboardRoot = ClipboardStage->GetRootLayer();
	if (!ClipboardRoot)
	{
		return Result;
	}

	pxr::UsdPrimSiblingRange PrimChildren = ClipboardStage->GetPseudoRoot().GetChildren();
	int32 NumPrimsToPaste = std::distance(PrimChildren.begin(), PrimChildren.end());

	TArray<pxr::UsdPrim> PrimsToPaste;
	PrimsToPaste.Reserve(NumPrimsToPaste);
	for (const pxr::UsdPrim& ClipboardPrim : ClipboardStage->GetPseudoRoot().GetChildren())
	{
		PrimsToPaste.Add(ClipboardPrim);
	}

	pxr::SdfLayerHandle EditTarget = UsdStage->GetEditTarget().GetLayer();
	if (!EditTarget)
	{
		return Result;
	}

	TSet<FString> UsedNames;
	for (const pxr::UsdPrim& Child : ParentPrim.GetChildren())
	{
		UsedNames.Add(UsdToUnreal::ConvertToken(Child.GetName()));
	}

	Result.SetNum(NumPrimsToPaste);
	for (int32 Index = 0; Index < NumPrimsToPaste; ++Index)
	{
		const pxr::UsdPrim& ClipboardPrim = PrimsToPaste[Index];
		if (!ClipboardPrim)
		{
			continue;
		}

		const FString OriginalName = UsdToUnreal::ConvertToken(ClipboardPrim.GetName());
		FString ValidName = GetUniqueName(SanitizeUsdIdentifier(*OriginalName), UsedNames);
		UsedNames.Add(ValidName);

		pxr::SdfPath TargetSpecPath = UsdParentPrim.GetPath().AppendChild(UnrealToUsd::ConvertToken(*ValidName).Get());

		// Ensure our parent prim spec exists, otherwise pxr::SdfCopySpec will fail
		if (!pxr::SdfCreatePrimInLayer(EditTarget, TargetSpecPath))
		{
			continue;
		}

		if (!pxr::SdfCopySpec(ClipboardRoot, ClipboardPrim.GetPath(), EditTarget, TargetSpecPath))
		{
			continue;
		}

		UE_LOG(
			LogUsd,
			Log,
			TEXT("Pasted prim '%s' as a child of prim '%s' within the edit target '%s'"),
			*OriginalName,
			*UsdToUnreal::ConvertPath(UsdParentPrim.GetPath()),
			*UsdToUnreal::ConvertString(EditTarget->GetIdentifier())
		);
		Result[Index] = UE::FSdfPath{TargetSpecPath};
	}
#endif	  // USE_USD_SDK

	return Result;
}

bool UsdUtils::CanPastePrims()
{
#if USE_USD_SDK
	pxr::UsdStageRefPtr ClipboardStage = pxr::UsdStageRefPtr{UnrealUSDWrapper::GetClipboardStage()};
	if (!ClipboardStage)
	{
		return false;
	}

	for (const pxr::UsdPrim& ClipboardPrim : ClipboardStage->GetPseudoRoot().GetChildren())
	{
		if (ClipboardPrim)
		{
			return true;
		}
	}
#endif	  // USE_USD_SDK

	return false;
}

void UsdUtils::ClearPrimClipboard()
{
#if USE_USD_SDK
	pxr::UsdStageRefPtr ClipboardStage = pxr::UsdStageRefPtr{UnrealUSDWrapper::GetClipboardStage()};
	if (!ClipboardStage)
	{
		return;
	}

	pxr::SdfLayerHandle ClipboardRoot = ClipboardStage->GetRootLayer();
	if (!ClipboardRoot)
	{
		return;
	}

	ClipboardRoot->Clear();
#endif	  // USE_USD_SDK
}

TArray<UE::FSdfPath> UsdUtils::DuplicatePrims(const TArray<UE::FUsdPrim>& Prims, EUsdDuplicateType DuplicateType, const UE::FSdfLayer& TargetLayer)
{
	TArray<UE::FSdfPath> Result;
	Result.SetNum(Prims.Num());

#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr UsdStage;
	for (const UE::FUsdPrim& Prim : Prims)
	{
		if (Prim)
		{
			UsdStage = pxr::UsdStageRefPtr{Prim.GetStage()};
			if (UsdStage)
			{
				break;
			}
		}
	}
	if (!UsdStage)
	{
		return Result;
	}

	pxr::SdfLayerRefPtr UsdLayer{TargetLayer};

	// Figure out which layers we'll modify
	std::unordered_set<pxr::SdfLayerHandle, pxr::TfHash> LayersThatCanBeAffected;
	switch (DuplicateType)
	{
		case EUsdDuplicateType::FlattenComposedPrim:
		case EUsdDuplicateType::SingleLayerSpecs:
		{
			if (!UsdLayer)
			{
				return Result;
			}

			LayersThatCanBeAffected.insert(UsdLayer);
			break;
		}
		case EUsdDuplicateType::AllLocalLayerSpecs:
		{
			const bool bIncludeSessionLayers = true;
			for (const pxr::SdfLayerHandle& Handle : UsdStage->GetLayerStack(bIncludeSessionLayers))
			{
				LayersThatCanBeAffected.insert(Handle);
			}

			// If any of our prims has specs on layers that are used by the stage but are not within the local layer
			// stack, then warn the user that some of these specs will not be duplicated
			{
				TArray<UE::FUsdPrim> PrimsWithExternalSpecs;
				for (const UE::FUsdPrim& Prim : Prims)
				{
					pxr::UsdPrim UsdPrim{Prim};
					if (!UsdPrim)
					{
						continue;
					}

					for (const pxr::SdfPrimSpecHandle& Spec : UsdPrim.GetPrimStack())
					{
						if (Spec && LayersThatCanBeAffected.count(Spec->GetLayer()) == 0)
						{
							PrimsWithExternalSpecs.Add(Prim);
							break;
						}
					}
				}
				USDConversionUtilsImpl::NotifySpecsWontBeDuplicated(PrimsWithExternalSpecs);
			}
			break;
		}
	}

	// If we're going to need to flatten, just flatten the stage once for all prims we'll duplicate
	pxr::SdfLayerRefPtr FlattenedLayer = nullptr;
	if (DuplicateType == EUsdDuplicateType::FlattenComposedPrim)
	{
		pxr::UsdStagePopulationMask Mask;
		for (int32 Index = 0; Index < Prims.Num(); ++Index)
		{
			pxr::UsdPrim UsdPrim{Prims[Index]};
			if (UsdPrim)
			{
				Mask.Add(UsdPrim.GetPath());
			}
		}

		pxr::UsdStageRefPtr TempStage = pxr::UsdStage::OpenMasked(UsdStage->GetRootLayer(), Mask);
		if (!TempStage)
		{
			return Result;
		}

		// USD will retain instances and prototypes even when flattening, which is not what we want
		// so let's disable instancing on our temp stage before we ask it to flatten.
		// Note how we travere the entire masked stage here, because we also need to handle the case
		// where the prim we're duplicating is not instanceable, but has instanceable children
		TArray<pxr::SdfPath> OldInstanceablePrims;
		if (TempStage->GetPrototypes().size() > 0)
		{
			pxr::UsdEditContext Context{TempStage, TempStage->GetSessionLayer()};

			pxr::UsdPrimRange PrimRange(TempStage->GetPseudoRoot());
			for (pxr::UsdPrimRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt)
			{
				if (PrimRangeIt->IsPseudoRoot())
				{
					continue;
				}

				if (PrimRangeIt->HasAuthoredInstanceable())
				{
					PrimRangeIt->SetInstanceable(false);
					OldInstanceablePrims.Add(PrimRangeIt->GetPrimPath());
				}
			}
		}

		const bool bAddSourceFileComment = false;
		FlattenedLayer = TempStage->Flatten(bAddSourceFileComment);
		if (!FlattenedLayer)
		{
			return Result;
		}

		// We may had to force instanceable=false on the prims we duplicated in order to get our session layer
		// opinion to disable instancing. We don't want those prims to come out with "instanceable=false" on the
		// flattened copy though, so here we clear that opinion
		for (const pxr::SdfPath& Path : OldInstanceablePrims)
		{
			if (pxr::SdfPrimSpecHandle Spec = FlattenedLayer->GetPrimAtPath(Path))
			{
				Spec->ClearInstanceable();
			}
		}
	}

	for (int32 Index = 0; Index < Prims.Num(); ++Index)
	{
		pxr::UsdPrim UsdPrim{Prims[Index]};
		if (!UsdPrim)
		{
			continue;
		}

		std::vector<pxr::SdfPrimSpecHandle> PrimSpecs = UsdPrim.GetPrimStack();

		// Note: We won't actually use these in case we're flattening, but it makes the code a bit simpler to also
		// do this while we're collecting LayersThatWillBeAffected below
		std::vector<pxr::SdfPrimSpecHandle> SpecsToDuplicate;
		SpecsToDuplicate.reserve(PrimSpecs.size());

		std::unordered_set<pxr::SdfLayerHandle, pxr::TfHash> LayersThatWillBeAffected;
		LayersThatWillBeAffected.reserve(PrimSpecs.size());

		pxr::SdfPath TargetPath = UsdPrim.GetPrimPath();
		for (int32 SpecIndex = PrimSpecs.size() - 1; SpecIndex >= 0; --SpecIndex)
		{
			const pxr::SdfPrimSpecHandle& Spec = PrimSpecs[SpecIndex];

			// For whatever reason sometimes there are invalid specs in the layer stack, so we need to be careful
			if (!Spec)
			{
				continue;
			}

			pxr::SdfPath SpecPath = Spec->GetPath();

			// Skip specs that have a different path than the actual prim path. The only way this could happen
			// is if the prim is referencing this particular path, and if we were to duplicate this spec
			// we'd essentially end up flattening the referenced prim over the new duplicate prim, which
			// is not what we want. We'll already get the fact that "prim references this other prim" by copying
			// the spec at the actual TargetPath however
			if (!SpecPath.IsPrimPath() || SpecPath.StripAllVariantSelections() != TargetPath)
			{
				continue;
			}

			pxr::SdfLayerHandle SpecLayerHandle = Spec->GetLayer();
			if (!SpecLayerHandle || LayersThatCanBeAffected.count(SpecLayerHandle) == 0)
			{
				continue;
			}

			SpecsToDuplicate.push_back(Spec);
			LayersThatWillBeAffected.insert(SpecLayerHandle);
		}

		// Find a usable name for the new duplicate prim
		pxr::SdfPath NewSpecPath;
		{
			const std::string SourcePrimName = UsdPrim.GetName().GetString();
			const pxr::SdfPath ParentPath = UsdPrim.GetPath().GetParentPath();

			int32 Suffix = -1;

			bool bFoundName = false;
			while (!bFoundName)
			{
				NewSpecPath = ParentPath.AppendElementString(SourcePrimName + "_" + std::to_string(++Suffix));

				bFoundName = true;
				for (const pxr::SdfLayerHandle& Layer : LayersThatWillBeAffected)
				{
					if (Layer->HasSpec(NewSpecPath))
					{
						bFoundName = false;
						break;
					}
				}
			}
		}

		// Actually do the duplication operation we chose
		if (DuplicateType == EUsdDuplicateType::FlattenComposedPrim && FlattenedLayer)
		{
			pxr::SdfPrimSpecHandle FlattenedPrim = FlattenedLayer->GetPrimAtPath(UsdPrim.GetPath());
			if (!FlattenedPrim)
			{
				return Result;
			}

			if (!pxr::SdfJustCreatePrimInLayer(UsdLayer, NewSpecPath))
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Failed to create prim and parent specs for path '%s' within layer '%s'"),
					*UsdToUnreal::ConvertPath(NewSpecPath),
					*UsdToUnreal::ConvertString(UsdLayer->GetIdentifier())
				);
				return Result;
			}

			if (!pxr::SdfCopySpec(FlattenedLayer, FlattenedPrim->GetPath(), UsdLayer, NewSpecPath))
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Failed to copy flattened prim spec from '%s' onto path '%s' within layer '%s'"),
					*UsdToUnreal::ConvertPath(UsdPrim.GetPath()),
					*UsdToUnreal::ConvertPath(NewSpecPath),
					*UsdToUnreal::ConvertString(UsdLayer->GetIdentifier())
				);
				return Result;
			}

			UE_LOG(
				LogUsd,
				Log,
				TEXT("Flattened prim '%s' onto spec '%s' at layer '%s'"),
				*UsdToUnreal::ConvertPath(UsdPrim.GetPath()),
				*UsdToUnreal::ConvertPath(NewSpecPath),
				*UsdToUnreal::ConvertString(UsdLayer->GetIdentifier())
			);
		}
		else
		{
			for (const pxr::SdfPrimSpecHandle& Spec : SpecsToDuplicate)
			{
				pxr::SdfPath SpecPath = Spec->GetPath();
				pxr::SdfLayerHandle SpecLayerHandle = Spec->GetLayer();

				UE_LOG(
					LogUsd,
					Log,
					TEXT("Duplicating prim spec '%s' within layer '%s'"),
					*UsdToUnreal::ConvertPath(SpecPath),
					*UsdToUnreal::ConvertString(SpecLayerHandle->GetIdentifier())
				);

				// Technically we shouldn't need to do this since we'll already do our changes on the Sdf level, however the
				// USDTransactor will record these notices as belonging to the current edit target, and if that is not in sync
				// with the layer that is actually changing, we won't be able to undo/redo the duplicate operation
				pxr::UsdEditContext Context{UsdStage, SpecLayerHandle};

				// Since we're duplicating a prim essentially as a sibling, parent specs should always exist.
				// Let's ensure that though, just in case
				if (!pxr::SdfJustCreatePrimInLayer(SpecLayerHandle, NewSpecPath))
				{
					UE_LOG(
						LogUsd,
						Warning,
						TEXT("Failed to create prim and parent specs for path '%s' within layer '%s'"),
						*UsdToUnreal::ConvertPath(NewSpecPath),
						*UsdToUnreal::ConvertString(SpecLayerHandle->GetIdentifier())
					);
					continue;
				}

				pxr::SdfShouldCopyValueFn ShouldCopyValue = [](pxr::SdfSpecType SpecType,
															   const pxr::TfToken& Field,
															   const pxr::SdfLayerHandle& SrcLayer,
															   const pxr::SdfPath& SrcPath,
															   bool FieldInSrc,
															   const pxr::SdfLayerHandle& DstLayer,
															   const pxr::SdfPath& DstPath,
															   bool FieldInDst,
															   std::optional<pxr::VtValue>* ValueToCopy) -> bool
				{
					// Only copy a field over if it has a value. Otherwise it seems to clear the destination spec
					// for nothing
					return FieldInSrc;
				};

				pxr::SdfShouldCopyChildrenFn ShouldCopyChildren = [](const pxr::TfToken& ChildrenField,
																	 const pxr::SdfLayerHandle& SrcLayer,
																	 const pxr::SdfPath& SrcPath,
																	 bool FieldInSrc,
																	 const pxr::SdfLayerHandle& DstLayer,
																	 const pxr::SdfPath& DstPath,
																	 bool FieldInDst,
																	 std::optional<pxr::VtValue>* SrcChildren,
																	 std::optional<pxr::VtValue>* DstChildren) -> bool
				{
					return true;
				};

				// We use the advanced version of SdfCopySpec here as otherwise the default behavior is to fully clear
				// the destination spec before copying stuff, and we may want to copy multiple specs overwriting each other
				if (!pxr::SdfCopySpec(SpecLayerHandle, SpecPath, SpecLayerHandle, NewSpecPath, ShouldCopyValue, ShouldCopyChildren))
				{
					UE_LOG(
						LogUsd,
						Warning,
						TEXT("Failed to copy spec from path '%s' onto path '%s' within layer '%s'"),
						*UsdToUnreal::ConvertPath(SpecPath),
						*UsdToUnreal::ConvertPath(NewSpecPath),
						*UsdToUnreal::ConvertString(SpecLayerHandle->GetIdentifier())
					);
				}
			}
		}

		Result[Index] = UE::FSdfPath{NewSpecPath};
	}
#endif	  // USE_USD_SDK

	return Result;
}

void UsdUtils::SetPrimAssetInfo(UE::FUsdPrim& Prim, const FUsdUnrealAssetInfo& Info)
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{Prim};
	if (!UsdPrim)
	{
		return;
	}

	// Just fetch the dictionary already since we'll add custom keys anyway
	pxr::VtDictionary AssetInfoDict = UsdPrim.GetAssetInfo();

	if (!Info.Name.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(pxr::UsdModelAPIAssetInfoKeys->name, pxr::VtValue{UnrealToUsd::ConvertString(*Info.Name).Get()});
	}

	if (!Info.Identifier.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(
			pxr::UsdModelAPIAssetInfoKeys->identifier,
			pxr::VtValue{pxr::SdfAssetPath{UnrealToUsd::ConvertString(*Info.Identifier).Get()}}
		);
	}

	if (!Info.Version.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(pxr::UsdModelAPIAssetInfoKeys->version, pxr::VtValue{UnrealToUsd::ConvertString(*Info.Version).Get()});
	}

	if (!Info.UnrealContentPath.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(UnrealIdentifiers::UnrealContentPath, pxr::VtValue{UnrealToUsd::ConvertString(*Info.UnrealContentPath).Get()});
	}

	if (!Info.UnrealAssetType.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(UnrealIdentifiers::UnrealAssetType, pxr::VtValue{UnrealToUsd::ConvertString(*Info.UnrealAssetType).Get()});
	}

	if (!Info.UnrealExportTime.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(UnrealIdentifiers::UnrealExportTime, pxr::VtValue{UnrealToUsd::ConvertString(*Info.UnrealExportTime).Get()});
	}

	if (!Info.UnrealEngineVersion.IsEmpty())
	{
		AssetInfoDict.SetValueAtPath(
			UnrealIdentifiers::UnrealEngineVersion,
			pxr::VtValue{UnrealToUsd::ConvertString(*Info.UnrealEngineVersion).Get()}
		);
	}

	UsdPrim.SetAssetInfo(AssetInfoDict);
#endif	  // USE_USD_SDK
}

FUsdUnrealAssetInfo UsdUtils::GetPrimAssetInfo(const UE::FUsdPrim& Prim)
{
	FUsdUnrealAssetInfo Result;

#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{Prim};
	if (!UsdPrim)
	{
		return Result;
	}

	// Just fetch the dictionary already since we'll fetch custom keys anyway
	pxr::VtDictionary AssetInfoDict = UsdPrim.GetAssetInfo();

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(pxr::UsdModelAPIAssetInfoKeys->name))
	{
		if (Value->IsHolding<std::string>())
		{
			Result.Name = UsdToUnreal::ConvertString(Value->Get<std::string>());
		}
	}

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(pxr::UsdModelAPIAssetInfoKeys->identifier))
	{
		if (Value->IsHolding<pxr::SdfAssetPath>())
		{
			Result.Identifier = UsdToUnreal::ConvertString(Value->Get<pxr::SdfAssetPath>().GetAssetPath());
		}
	}

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(pxr::UsdModelAPIAssetInfoKeys->version))
	{
		if (Value->IsHolding<std::string>())
		{
			Result.Version = UsdToUnreal::ConvertString(Value->Get<std::string>());
		}
	}

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(UnrealIdentifiers::UnrealContentPath))
	{
		if (Value->IsHolding<std::string>())
		{
			Result.UnrealContentPath = UsdToUnreal::ConvertString(Value->Get<std::string>());
		}
	}

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(UnrealIdentifiers::UnrealAssetType))
	{
		if (Value->IsHolding<std::string>())
		{
			Result.UnrealAssetType = UsdToUnreal::ConvertString(Value->Get<std::string>());
		}
	}

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(UnrealIdentifiers::UnrealExportTime))
	{
		if (Value->IsHolding<std::string>())
		{
			Result.UnrealExportTime = UsdToUnreal::ConvertString(Value->Get<std::string>());
		}
	}

	if (const pxr::VtValue* Value = AssetInfoDict.GetValueAtPath(UnrealIdentifiers::UnrealEngineVersion))
	{
		if (Value->IsHolding<std::string>())
		{
			Result.UnrealEngineVersion = UsdToUnreal::ConvertString(Value->Get<std::string>());
		}
	}
#endif	  // USE_USD_SDK

	return Result;
}

#if USE_USD_SDK
bool UsdUtils::ClearNonEssentialPrimMetadata(const pxr::UsdPrim& Prim)
{
	FScopedUsdAllocs Allocs;

	pxr::SdfChangeBlock ChangeBlock;

	// Note: This only returns top-level fields, and won't have a separate entry for values inside VtDictionaries
	// or anything like that. This means this likely won't be that expensive, and we don't have to care about order
	std::map<pxr::TfToken, pxr::VtValue, pxr::TfDictionaryLessThan> MetadataMap = Prim.GetAllAuthoredMetadata();

	for (std::map<pxr::TfToken, pxr::VtValue, pxr::TfDictionaryLessThan>::const_iterator MetadataIter = MetadataMap.begin();
		 MetadataIter != MetadataMap.end();
		 ++MetadataIter)
	{
		const pxr::TfToken& FieldName = MetadataIter->first;

		// We consider those "essential metadata", as removing them will mess with the prim definition
		const static std::unordered_set<pxr::TfToken, pxr::TfHash> FieldsToSkip = {pxr::SdfFieldKeys->Specifier, pxr::SdfFieldKeys->TypeName};
		if (FieldsToSkip.count(FieldName) > 0)
		{
			continue;
		}

		const bool bSuccess = Prim.ClearMetadata(FieldName);

		if (!bSuccess)
		{
			UE_LOG(
				LogUsd,
				Warning,
				TEXT("Failed to clear metadata field '%s' from prim '%s'"),
				*UsdToUnreal::ConvertToken(FieldName),
				*UsdToUnreal::ConvertPath(Prim.GetPrimPath())
			);
			return false;
		}
	}

	return true;
}
#endif	  // USE_USD_SDK

void UsdUtils::CollectSchemaAnalytics(const UE::FUsdStage& Stage, const FString& EventName)
{
#if USE_USD_SDK
	if (!Stage)
	{
		return;
	}

	TSet<FString> SeenSchemas;

	{
		FScopedUsdAllocs Allocs;

		pxr::UsdPrimRange PrimRange{Stage.GetPseudoRoot(), pxr::UsdTraverseInstanceProxies()};
		for (pxr::UsdPrimRange::iterator PrimRangeIt = ++PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt)
		{
			// It's perfectly fine to have a typeless prim (e.g. "def 'Cube'")
			if (PrimRangeIt->HasAuthoredTypeName())
			{
				const pxr::TfToken& TypeName = PrimRangeIt->GetTypeName();
				const FString TypeNameStr = UsdToUnreal::ConvertToken(TypeName);

				if (!TypeNameStr.IsEmpty())
				{
					SeenSchemas.Add(TypeNameStr);
				}
			}

			const pxr::UsdPrimTypeInfo& PrimTypeInfo = PrimRangeIt->GetPrimTypeInfo();
			for (const pxr::TfToken& AppliedSchema : PrimTypeInfo.GetAppliedAPISchemas())
			{
				std::pair<pxr::TfToken, pxr::TfToken> Pair = pxr::UsdSchemaRegistry::GetTypeNameAndInstance(AppliedSchema);
				FString TypeName = UsdToUnreal::ConvertToken(Pair.first);

				// These applied schema names shouldn't ever end up as the empty string... but we don't really want to pop
				// an ensure or show a warning when analytics fails
				if (!TypeName.IsEmpty())
				{
					SeenSchemas.Add(TypeName);
				}
			}
		}
	}

	const static TSet<FString> NativeSchemaNames{
		"AssetPreviewsAPI",
		"Backdrop",
		"BasisCurves",
		"BlendShape",
		"Camera",
		"Capsule",
		"ClipsAPI",
		"CollectionAPI",
		"Cone",
		"ConnectableAPI",
		"ControlRigAPI",
		"CoordSysAPI",
		"Cube",
		"Cylinder",
		"CylinderLight",
		"DiskLight",
		"DistantLight",
		"DomeLight",
		"Field3DAsset",
		"GenerativeProcedural",
		"GeomModelAPI",
		"GeomSubset",
		"GeometryLight",
		"GroomAPI",
		"GroomBindingAPI",
		"HermiteCurves",
		"HydraGenerativeProceduralAPI",
		"LightAPI",
		"LightFilter",
		"LightListAPI",
		"ListAPI",
		"LiveLinkAPI",
		"Material",
		"MaterialBindingAPI",
		"Mesh",
		"MeshLightAPI",
		"ModelAPI",
		"MotionAPI",
		"NodeDefAPI",
		"NodeGraph",
		"NodeGraphNodeAPI",
		"NurbsCurves",
		"NurbsPatch",
		"OpenVDBAsset",
		"PackedJointAnimation",
		"PhysicsArticulationRootAPI",
		"PhysicsCollisionAPI",
		"PhysicsCollisionGroup",
		"PhysicsDistanceJoint",
		"PhysicsDriveAPI",
		"PhysicsFilteredPairsAPI",
		"PhysicsFixedJoint",
		"PhysicsJoint",
		"PhysicsLimitAPI",
		"PhysicsMassAPI",
		"PhysicsMaterialAPI",
		"PhysicsMeshCollisionAPI",
		"PhysicsPrismaticJoint",
		"PhysicsRevoluteJoint",
		"PhysicsRigidBodyAPI",
		"PhysicsScene",
		"PhysicsSphericalJoint",
		"Plane",
		"PluginLight",
		"PluginLightFilter",
		"PointInstancer",
		"Points",
		"PortalLight",
		"PrimvarsAPI",
		"RectLight",
		"RenderDenoisePass",
		"RenderPass",
		"RenderProduct",
		"RenderSettings",
		"RenderVar",
		"RiMaterialAPI",
		"RiSplineAPI",
		"SceneGraphPrimAPI",
		"Scope",
		"Shader",
		"ShadowAPI",
		"ShapingAPI",
		"SkelAnimation",
		"SkelBindingAPI",
		"SkelRoot",
		"Skeleton",
		"SpatialAudio",
		"Sphere",
		"SphereLight",
		"StatementsAPI",
		"VisibilityAPI",
		"Volume",
		"VolumeLightAPI",
		"Xform",
		"XformCommonAPI"};

	int32 CustomSchemaCount = 0;
	for (const FString& SchemaName : SeenSchemas)
	{
		// We only care about non-native schemas
		if (!NativeSchemaNames.Contains(SchemaName))
		{
			++CustomSchemaCount;
		}
	}

	if (CustomSchemaCount > 0)
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Emplace(TEXT("CustomSchemas"), CustomSchemaCount);
		IUsdClassesModule::SendAnalytics(MoveTemp(EventAttributes), FString::Printf(TEXT("%s.CustomSchemaCount"), *EventName));
	}
#endif	  // USE_USD_SDK
}

#if USE_USD_SDK
#undef LOCTEXT_NAMESPACE
#endif
