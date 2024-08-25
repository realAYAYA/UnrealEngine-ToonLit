// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SkyAtmosphereComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/Level.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#include "Misc/MapErrors.h"
#include "Misc/UObjectToken.h"
#include "SceneInterface.h"
#include "UObject/UObjectIterator.h"
#include "SceneManagement.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/DirectionalLightComponent.h"
#include "UObject/UE5MainStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkyAtmosphereComponent)

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#include "Rendering/StaticLightingSystemInterface.h"
#endif

#define LOCTEXT_NAMESPACE "SkyAtmosphereComponent"



/*=============================================================================
	USkyAtmosphereComponent implementation.
=============================================================================*/

USkyAtmosphereComponent::USkyAtmosphereComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SkyAtmosphereSceneProxy(nullptr)
{
	// All distance here are in kilometer and scattering/absorptions coefficient in 1/kilometers.
	const float EarthBottomRadius = 6360.0f;
	const float EarthTopRadius = 6420.0f;
	const float EarthRayleighScaleHeight = 8.0f;
	const float EarthMieScaleHeight = 1.2f;
	
	// Default: Earth like atmosphere
	TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtAbsoluteWorldOrigin;
	BottomRadius = EarthBottomRadius;
	AtmosphereHeight = EarthTopRadius - EarthBottomRadius;
	GroundAlbedo = FColor(170, 170, 170); // 170 => 0.4f linear

	// Float to a u8 rgb + float length can lose some precision but it is better UI wise.
	const FLinearColor RayleightScatteringRaw = FLinearColor(0.005802f, 0.013558f, 0.033100f);
	RayleighScattering = RayleightScatteringRaw * (1.0f / RayleightScatteringRaw.B);
	RayleighScatteringScale = RayleightScatteringRaw.B;
	RayleighExponentialDistribution = EarthRayleighScaleHeight;

	MieScattering = FColor::White;
	MieScatteringScale = 0.003996f;
	MieAbsorption = FColor::White;
	MieAbsorptionScale = 0.000444f;
	MieAnisotropy = 0.8f;
	MieExponentialDistribution = EarthMieScaleHeight;

	// Absorption tent distribution representing ozone distribution in Earth atmosphere.
	const FLinearColor OtherAbsorptionRaw = FLinearColor(0.000650f, 0.001881f, 0.000085f);
	OtherAbsorptionScale = OtherAbsorptionRaw.G;
	OtherAbsorption = OtherAbsorptionRaw * (1.0f / OtherAbsorptionRaw.G);
	OtherTentDistribution.TipAltitude = 25.0f;
	OtherTentDistribution.TipValue    =  1.0f;
	OtherTentDistribution.Width       = 15.0f;

	SkyLuminanceFactor = FLinearColor(FLinearColor::White);
	MultiScatteringFactor = 1.0f;
	AerialPespectiveViewDistanceScale = 1.0f;
	HeightFogContribution = 1.0f;
	TransmittanceMinLightElevationAngle = -90.0f;
	AerialPerspectiveStartDepth = 0.1f;

	TraceSampleCountScale = 1.0f;

	bHoldout = false;
	bRenderInMainPass = true;

	memset(OverrideAtmosphericLight, 0, sizeof(OverrideAtmosphericLight));

	ValidateStaticLightingGUIDs();
}

USkyAtmosphereComponent::~USkyAtmosphereComponent()
{
}

static bool SkyAtmosphereComponentStaticLightingBuilt(const USkyAtmosphereComponent* Component)
{
	AActor* Owner = Component->GetOwner();
	UMapBuildDataRegistry* Registry = nullptr;
	if (Owner)
	{
		ULevel* OwnerLevel = Owner->GetLevel();
		if (OwnerLevel && OwnerLevel->OwningWorld)
		{
			ULevel* ActiveLightingScenario = OwnerLevel->OwningWorld->GetActiveLightingScenario();
			if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
			{
				Registry = ActiveLightingScenario->MapBuildData;
			}
			else if (OwnerLevel->MapBuildData)
			{
				Registry = OwnerLevel->MapBuildData;
			}
		}
	}

	const FSkyAtmosphereMapBuildData* SkyAtmosphereFogBuildData = Registry ? Registry->GetSkyAtmosphereBuildData(Component->GetStaticLightingBuiltGuid()) : nullptr;
	UWorld* World = Component->GetWorld();
	if (World)
	{
		class FSceneInterface* Scene = Component->GetWorld()->Scene;

		// Only require building if there is a Sky or Sun light requiring lighting builds, i.e. non movable.
		const bool StaticLightingDependsOnAtmosphere = Scene->HasSkyLightRequiringLightingBuild() || Scene->HasAtmosphereLightRequiringLightingBuild();
		// Built data is available or static lighting does not depend any sun/sky components.
		return (SkyAtmosphereFogBuildData != nullptr && StaticLightingDependsOnAtmosphere) || !StaticLightingDependsOnAtmosphere;
	}

	return true;	// The component has not been spawned in any world yet so let's mark it as built for now.
}

void USkyAtmosphereComponent::SendRenderTransformCommand()
{
	if (SkyAtmosphereSceneProxy)
	{
		FTransform ComponentTransform = GetComponentTransform();
		uint8 TrsfMode = uint8(TransformMode);
		FSkyAtmosphereSceneProxy* SceneProxy = SkyAtmosphereSceneProxy;
		ENQUEUE_RENDER_COMMAND(FUpdateSkyAtmosphereSceneProxyTransformCommand)(
			[SceneProxy, ComponentTransform, TrsfMode](FRHICommandList& RHICmdList)
		{
			SceneProxy->UpdateTransform(ComponentTransform, TrsfMode);
		});
	}
}

void USkyAtmosphereComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);
	// If one day we need to look up lightmass built data, lookup it up here using the guid from the correct MapBuildData.

	bool bHidden = false;
#if WITH_EDITORONLY_DATA
	bHidden = GetOwner() ? GetOwner()->bHiddenEdLevel : false;
#endif // WITH_EDITORONLY_DATA
	if (!ShouldComponentAddToScene())
	{
		bHidden = true;
	}

	if (GetVisibleFlag() && !bHidden &&
		ShouldComponentAddToScene() && ShouldRender() && IsRegistered() && (GetOuter() == NULL || !GetOuter()->HasAnyFlags(RF_ClassDefaultObject)))
	{
		// Create the scene proxy.
		SkyAtmosphereSceneProxy = new FSkyAtmosphereSceneProxy(this);
		GetWorld()->Scene->AddSkyAtmosphere(SkyAtmosphereSceneProxy, SkyAtmosphereComponentStaticLightingBuilt(this));
	}

}

void USkyAtmosphereComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();
	SendRenderTransformCommand();
}

void USkyAtmosphereComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (SkyAtmosphereSceneProxy)
	{
		GetWorld()->Scene->RemoveSkyAtmosphere(SkyAtmosphereSceneProxy);

		FSkyAtmosphereSceneProxy* SceneProxy = SkyAtmosphereSceneProxy;
		ENQUEUE_RENDER_COMMAND(FDestroySkyAtmosphereSceneProxyCommand)(
			[SceneProxy](FRHICommandList& RHICmdList)
		{
			delete SceneProxy;
		});

		SkyAtmosphereSceneProxy = nullptr;
	}
}

void USkyAtmosphereComponent::ValidateStaticLightingGUIDs()
{
	// Validate light guids.
	if (!bStaticLightingBuiltGUID.IsValid())
	{
		UpdateStaticLightingGUIDs();
	}
}

void USkyAtmosphereComponent::UpdateStaticLightingGUIDs()
{
	bStaticLightingBuiltGUID = FGuid::NewGuid();
}

#if WITH_EDITOR

void USkyAtmosphereComponent::CheckForErrors()
{
	AActor* Owner = GetOwner();
	if (Owner && GetVisibleFlag())
	{
		UWorld* ThisWorld = Owner->GetWorld();
		bool bMultipleFound = false;

		if (ThisWorld)
		{
			for (TObjectIterator<USkyAtmosphereComponent> ComponentIt; ComponentIt; ++ComponentIt)
			{
				USkyAtmosphereComponent* Component = *ComponentIt;

				if (Component != this
					&& IsValid(Component)
					&& Component->GetVisibleFlag()
					&& Component->GetOwner()
					&& ThisWorld->ContainsActor(Component->GetOwner())
					&& IsValid(Component->GetOwner()))
				{
					bMultipleFound = true;
					break;
				}
			}
		}

		if (bMultipleFound)
		{
			FMessageLog("MapCheck").Error()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_MultipleSkyAtmosphere", "Multiple sky atmosphere are active, only one can be enabled per world.")))
				->AddToken(FMapErrorToken::Create(FMapErrors::MultipleSkyAtmospheres));
		}
	}
}

void USkyAtmosphereComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If any properties have been changed in the atmosphere category, it means the sky look will change and lighting needs to be rebuild.
	const FName CategoryName = FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property);
	if (CategoryName == FName(TEXT("Planet")) ||
		CategoryName == FName(TEXT("Atmosphere")) ||
		CategoryName == FName(TEXT("Atmosphere - Rayleigh")) ||
		CategoryName == FName(TEXT("Atmosphere - Mie")) ||
		CategoryName == FName(TEXT("Atmosphere - Absorption")) ||
		CategoryName == FName(TEXT("Art direction")))
	{
		if (SkyAtmosphereComponentStaticLightingBuilt(this))
		{
			// If we have changed an atmosphere property and the lighyting has already been built, we need to ask for a rebuild by updating the static lighting GUIDs.
			UpdateStaticLightingGUIDs();
		}

		if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USkyAtmosphereComponent, TransformMode))
		{
			SendRenderTransformCommand();
		}

#if WITH_EDITOR
		FStaticLightingSystemInterface::OnSkyAtmosphereModified.Broadcast();
#endif

	}
}

#endif // WITH_EDITOR

void USkyAtmosphereComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	// Only load the lighting GUID if
	if( (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::RemovedAtmosphericFog && Ar.IsLoading() && bIsAtmosphericFog) //Loading an AtmosphereFog component into a SkyAtmosphere component
		|| (Ar.IsSaving() && bIsAtmosphericFog)	// Saving an AtmosphereFog component as a SkyAtmosphere component
		|| !bIsAtmosphericFog) // Saving / Loading a regular SkyAtmosphere
	{
		// Only load that for SkyAtmosphere or AtmosphericFog component that have already been converted
		Ar << bStaticLightingBuiltGUID;
	}
}

void USkyAtmosphereComponent::OverrideAtmosphereLightDirection(int32 AtmosphereLightIndex, const FVector& LightDirection)
{
	check(AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS);
	if (AreDynamicDataChangesAllowed() && SkyAtmosphereSceneProxy &&
		(!OverrideAtmosphericLight[AtmosphereLightIndex] || OverrideAtmosphericLightDirection[AtmosphereLightIndex]!=LightDirection))
	{
		FSceneInterface* Scene = GetWorld()->Scene;
		OverrideAtmosphericLight[AtmosphereLightIndex] = true;
		OverrideAtmosphericLightDirection[AtmosphereLightIndex] = LightDirection;
		MarkRenderStateDirty();
	}
}

bool USkyAtmosphereComponent::IsAtmosphereLightDirectionOverriden(int32 AtmosphereLightIndex)
{
	check(AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS);
	if (AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS)
	{
		return OverrideAtmosphericLight[AtmosphereLightIndex];
	}
	return false;
}

FVector USkyAtmosphereComponent::GetOverridenAtmosphereLightDirection(int32 AtmosphereLightIndex)
{
	check(AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS);
	if (AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS)
	{
		return OverrideAtmosphericLightDirection[AtmosphereLightIndex];
	}
	return FVector::ZeroVector;
}

void USkyAtmosphereComponent::ResetAtmosphereLightDirectionOverride(int32 AtmosphereLightIndex)
{
	check(AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS);
	if (AtmosphereLightIndex >= 0 && AtmosphereLightIndex < NUM_ATMOSPHERE_LIGHTS)
	{
		OverrideAtmosphericLight[AtmosphereLightIndex] = false;
		OverrideAtmosphericLightDirection[AtmosphereLightIndex] = FVector::ZeroVector;
	}
}

void USkyAtmosphereComponent::GetOverrideLightStatus(bool* OutOverrideAtmosphericLight, FVector* OutOverrideAtmosphericLightDirection) const
{
	memcpy(OutOverrideAtmosphericLight, OverrideAtmosphericLight, sizeof(OverrideAtmosphericLight));
	memcpy(OutOverrideAtmosphericLightDirection, OverrideAtmosphericLightDirection, sizeof(OverrideAtmosphericLightDirection));
}

void USkyAtmosphereComponent::SetPositionToMatchDeprecatedAtmosphericFog()
{
	TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtComponentTransform;
	SetWorldLocation(FVector(0.0f, 0.0f, -100000.0f));
}

#define SKY_DECLARE_BLUEPRINT_SETFUNCTION(MemberType, MemberName) void USkyAtmosphereComponent::Set##MemberName(MemberType NewValue)\
{\
	if (AreDynamicDataChangesAllowed() && MemberName != NewValue)\
	{\
		MemberName = NewValue;\
		MarkRenderStateDirty();\
	}\
}\

#define SKY_DECLARE_BLUEPRINT_SETFUNCTION_LINEARCOEFFICIENT(MemberName) void USkyAtmosphereComponent::Set##MemberName(FLinearColor NewValue)\
{\
	if (AreDynamicDataChangesAllowed() && MemberName != NewValue)\
	{\
		MemberName = NewValue.GetClamped(0.0f, 1e38f); \
		MarkRenderStateDirty();\
	}\
}\

SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, BottomRadius);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(const FColor&, GroundAlbedo);

SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, AtmosphereHeight);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, MultiScatteringFactor);

SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, RayleighScatteringScale);
SKY_DECLARE_BLUEPRINT_SETFUNCTION_LINEARCOEFFICIENT(RayleighScattering);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, RayleighExponentialDistribution);

SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, MieScatteringScale);
SKY_DECLARE_BLUEPRINT_SETFUNCTION_LINEARCOEFFICIENT(MieScattering);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, MieAbsorptionScale);
SKY_DECLARE_BLUEPRINT_SETFUNCTION_LINEARCOEFFICIENT(MieAbsorption);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, MieAnisotropy);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, MieExponentialDistribution);

SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, OtherAbsorptionScale);
SKY_DECLARE_BLUEPRINT_SETFUNCTION_LINEARCOEFFICIENT(OtherAbsorption);

SKY_DECLARE_BLUEPRINT_SETFUNCTION_LINEARCOEFFICIENT(SkyLuminanceFactor);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, AerialPespectiveViewDistanceScale);
SKY_DECLARE_BLUEPRINT_SETFUNCTION(float, HeightFogContribution);

void USkyAtmosphereComponent::SetHoldout(bool bNewHoldout)
{
	if (bHoldout != bNewHoldout)
	{
		bHoldout = bNewHoldout;
		MarkRenderStateDirty();
	}
}

void USkyAtmosphereComponent::SetRenderInMainPass(bool bValue)
{
	if (bRenderInMainPass != bValue)
	{
		bRenderInMainPass = bValue;
		MarkRenderStateDirty();
	}
}

FLinearColor USkyAtmosphereComponent::GetAtmosphereTransmitanceOnGroundAtPlanetTop(UDirectionalLightComponent* DirectionalLight)
{
	if(DirectionalLight != nullptr)
	{
		FAtmosphereSetup AtmosphereSetup(*this);
		const FLinearColor TransmittanceAtDirLight = AtmosphereSetup.GetTransmittanceAtGroundLevel(-DirectionalLight->GetDirection());
		return TransmittanceAtDirLight;
	}
	return FLinearColor::White;
}

/*=============================================================================
	ASkyAtmosphere implementation.
=============================================================================*/

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif

ASkyAtmosphere::ASkyAtmosphere(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkyAtmosphereComponent = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphereComponent"));
	RootComponent = SkyAtmosphereComponent;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SkyAtmosphereTextureObject;
			FName ID_SkyAtmosphere;
			FText NAME_SkyAtmosphere;
			FConstructorStatics()
				: SkyAtmosphereTextureObject(TEXT("/Engine/EditorResources/S_SkyAtmosphere"))
				, ID_SkyAtmosphere(TEXT("Fog"))
				, NAME_SkyAtmosphere(NSLOCTEXT("SpriteCategory", "Fog", "Fog"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetSpriteComponent())
		{
			GetSpriteComponent()->Sprite = ConstructorStatics.SkyAtmosphereTextureObject.Get();
			GetSpriteComponent()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_SkyAtmosphere;
			GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_SkyAtmosphere;
			GetSpriteComponent()->SetupAttachment(SkyAtmosphereComponent);
		}

		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(150, 200, 255);

			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_SkyAtmosphere;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_SkyAtmosphere;
			ArrowComponent->SetupAttachment(SkyAtmosphereComponent);
			ArrowComponent->bLightAttachment = true;
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = true;
	SetHidden(false);
}



/*=============================================================================
	FSkyAtmosphereSceneProxy implementation.
=============================================================================*/



FSkyAtmosphereSceneProxy::FSkyAtmosphereSceneProxy(const USkyAtmosphereComponent* InComponent)
	: bStaticLightingBuilt(false)
	, AtmosphereSetup(*InComponent)
	, bHoldout(InComponent->bHoldout > 0)
	, bRenderInMainPass(InComponent->bRenderInMainPass > 0)
{
	SkyLuminanceFactor = InComponent->SkyLuminanceFactor;
	AerialPespectiveViewDistanceScale = InComponent->AerialPespectiveViewDistanceScale;
	HeightFogContribution = InComponent->HeightFogContribution;
	AerialPerspectiveStartDepthKm = InComponent->AerialPerspectiveStartDepth;
	TraceSampleCountScale = InComponent->TraceSampleCountScale;

	InComponent->GetOverrideLightStatus(OverrideAtmosphericLight, OverrideAtmosphericLightDirection);
}

FSkyAtmosphereSceneProxy::~FSkyAtmosphereSceneProxy()
{
}

FVector FSkyAtmosphereSceneProxy::GetAtmosphereLightDirection(int32 AtmosphereLightIndex, const FVector& DefaultDirection) const
{
	if (OverrideAtmosphericLight[AtmosphereLightIndex])
	{
		return OverrideAtmosphericLightDirection[AtmosphereLightIndex];
	}
	return DefaultDirection;
}


#undef LOCTEXT_NAMESPACE



