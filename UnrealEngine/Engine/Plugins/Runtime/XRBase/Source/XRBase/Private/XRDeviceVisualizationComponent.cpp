// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "XRDeviceVisualizationComponent.h"
#include "IXRSystemAssets.h"
#include "Features/IModularFeatures.h"
#include "XRMotionControllerBase.h"
#include "IXRTrackingSystem.h"
#include "Misc/Attribute.h"
#include "MotionControllerComponent.h"
#include "XRTrackingSystemBase.h"
#if WITH_EDITOR
#include "IVREditorModule.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(XRDeviceVisualizationComponent)

DEFINE_LOG_CATEGORY_STATIC(LogXRDeviceVisualizationComponent, Log, All);

FName UXRDeviceVisualizationComponent::CustomModelSourceId(TEXT("Custom"));

//=============================================================================
UXRDeviceVisualizationComponent::UXRDeviceVisualizationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	MotionSource = FXRMotionControllerBase::LeftHandSourceId;
	bAutoActivate = true;

	// ensure InitializeComponent() gets called
	bWantsInitializeComponent = true;
}

#if WITH_EDITOR
//=============================================================================
void UXRDeviceVisualizationComponent::OnCloseVREditor()
{
	SetStaticMesh(nullptr);
}
#endif

//=============================================================================
void UXRDeviceVisualizationComponent::OnRegister() {
	Super::OnRegister();

	UMotionControllerComponent::OnActivateVisualizationComponent.RemoveAll(this);
	FXRTrackingSystemDelegates::OnXRInteractionProfileChanged.RemoveAll(this);
#if WITH_EDITOR
	IVREditorModule::Get().OnVREditingModeExit().RemoveAll(this);
#endif

	UMotionControllerComponent* ParentController = FindParentMotionController();
	if (ParentController != nullptr) 
	{
		MotionSource = ParentController->GetTrackingMotionSource();
		// Wait for controller to activate the rendering for this component.
		bIsRenderingActive = false;
		FXRTrackingSystemDelegates::OnXRInteractionProfileChanged.AddUObject(this, &UXRDeviceVisualizationComponent::OnInteractionProfileChanged);
		UMotionControllerComponent::OnActivateVisualizationComponent.AddUObject(this, &UXRDeviceVisualizationComponent::SetIsRenderingActive);
#if WITH_EDITOR
		IVREditorModule::Get().OnVREditingModeExit().AddUObject(this, &UXRDeviceVisualizationComponent::OnCloseVREditor);
#endif
	}
	else 
	{
		UE_LOG(LogXRDeviceVisualizationComponent, Error, TEXT("No parent UMotionControllerComponent for %s was found in the hierarchy. This component will not render anything."), *GetName());
	}
}

//=============================================================================
void UXRDeviceVisualizationComponent::OnInteractionProfileChanged()
{
	bInteractionProfilePresent = true;
	RefreshMesh();
}

//=============================================================================
void UXRDeviceVisualizationComponent::OnUnregister()
{
	Super::OnUnregister();
	FXRTrackingSystemDelegates::OnXRInteractionProfileChanged.RemoveAll(this);
	UMotionControllerComponent::OnActivateVisualizationComponent.RemoveAll(this);
#if WITH_EDITOR
	IVREditorModule::Get().OnVREditingModeExit().RemoveAll(this);
#endif
}

//=============================================================================
UMotionControllerComponent* UXRDeviceVisualizationComponent::FindParentMotionController()
{	
	USceneComponent* Parent = GetAttachParent();
	UMotionControllerComponent* ParentController;
	while (Parent != nullptr) {
		ParentController = Cast<UMotionControllerComponent>(Parent);
		if (ParentController != nullptr) {	
			return ParentController;
		}
		Parent = Parent->GetAttachParent();
	}
	return nullptr;
}

//=============================================================================
void UXRDeviceVisualizationComponent::SetIsVisualizationActive(bool bNewVisualizationState)
{
	if (bNewVisualizationState != bIsVisualizationActive) 
	{
		bIsVisualizationActive = bNewVisualizationState;
	}
	RefreshMesh();
}

//=============================================================================
bool UXRDeviceVisualizationComponent::CanDeviceBeDisplayed() 
{
	return bIsRenderingActive && bIsVisualizationActive;
}

//=============================================================================
void UXRDeviceVisualizationComponent::SetDisplayModelSource(const FName NewDisplayModelSource)
{
	if (NewDisplayModelSource != DisplayModelSource)
	{
		DisplayModelSource = NewDisplayModelSource;
		RefreshMesh();
	}
}

//=============================================================================
void UXRDeviceVisualizationComponent::SetCustomDisplayMesh(UStaticMesh* NewDisplayMesh)
{
	if (NewDisplayMesh != CustomDisplayMesh)
	{
		CustomDisplayMesh = NewDisplayMesh;
		if (DisplayModelSource == UXRDeviceVisualizationComponent::CustomModelSourceId && CustomDisplayMesh != nullptr)
		{
			SetStaticMesh(CustomDisplayMesh);
		}
	}
}

//=============================================================================
void UXRDeviceVisualizationComponent::RefreshMesh()
{
	if (CanDeviceBeDisplayed())
	{
		UStaticMeshComponent* StaticMeshComponent;

		if (DisplayModelSource == UXRDeviceVisualizationComponent::CustomModelSourceId)
		{
			if (CustomDisplayMesh)
			{
				SetStaticMesh(CustomDisplayMesh);
				int32 MatCount = FMath::Min(GetNumMaterials(), DisplayMeshMaterialOverrides.Num());
				SetMaterials(MatCount);
			}
			else
			{
				SetStaticMesh(nullptr);
				UE_LOG(LogXRDeviceVisualizationComponent, Error, TEXT("Failed to set a custom mesh for %s because no custom mesh was specified."), *GetName());
			}
		}
		else
		{
			TArray<IXRSystemAssets*> XRAssetSystems = IModularFeatures::Get().GetModularFeatureImplementations<IXRSystemAssets>(IXRSystemAssets::GetModularFeatureName());
			for (IXRSystemAssets* AssetSys : XRAssetSystems)
			{
				if (!DisplayModelSource.IsNone() && AssetSys->GetSystemName() != DisplayModelSource)
				{
					continue;
				}

				int32 DeviceId = INDEX_NONE;
				if (MotionSource == FXRMotionControllerBase::HMDSourceId)
				{
					DeviceId = IXRTrackingSystem::HMDDeviceId;
				}
				else
				{
					EControllerHand ControllerHandIndex;
					if (!FXRMotionControllerBase::GetHandEnumForSourceName(MotionSource, ControllerHandIndex))
					{
						break;
					}
					DeviceId = AssetSys->GetDeviceId(ControllerHandIndex);
				}

				if (GetStaticMesh() && DisplayDeviceId.IsOwnedBy(AssetSys) && DisplayDeviceId.DeviceId == DeviceId)
				{
					// Assume that the current StaticMesh is the same one we'd get back, so don't recreate it
					break;
				}

				// Needs to be set before CreateRenderComponent() since the LoadComplete callback may be triggered before it returns (for syncrounous loads)
				DisplayModelLoadState = EModelLoadStatus::Pending;
				FXRComponentLoadComplete LoadCompleteDelegate = FXRComponentLoadComplete::CreateUObject(this, &UXRDeviceVisualizationComponent::OnDisplayModelLoaded);

				const EObjectFlags SubObjFlags = RF_Transactional | RF_TextExportTransient;
				UPrimitiveComponent* RenderComponent = AssetSys->CreateRenderComponent(DeviceId, GetOwner(), SubObjFlags, /*bForceSynchronous=*/false, LoadCompleteDelegate);
				if (RenderComponent != nullptr) 
				{
					StaticMeshComponent = Cast<UStaticMeshComponent>(RenderComponent);
					if (StaticMeshComponent != nullptr) 
					{
						if (DisplayModelLoadState != EModelLoadStatus::Complete)
						{
							DisplayModelLoadState = EModelLoadStatus::InProgress;
						}
						DisplayDeviceId = FXRDeviceId(AssetSys, DeviceId);
						break;
					}
					else 
					{
						DisplayModelLoadState = EModelLoadStatus::Unloaded;
						SetStaticMesh(nullptr);
						UE_LOG(LogXRDeviceVisualizationComponent, Error, TEXT("CreateRenderComponent must return a UStaticMeshComponent. Setting a NULL StaticMesh."));
					}
				}
				else 
				{
					DisplayModelLoadState = EModelLoadStatus::Unloaded;
					SetStaticMesh(nullptr);
					// xrGetCurrentInteractionProfile returns 0 for the first bunch of frames.
					// There is a race condition between the MotionController starting the rendering on this component and the interaction profile becoming available.
					// If the MotionController starts the rendering before an interaction profile has been found, the RenderComponent will be null and we don't want
					// to fire an error for this.
					if (bInteractionProfilePresent)
					{
						UE_LOG(LogXRDeviceVisualizationComponent, Error, TEXT("CreateRenderComponent returned a NULL UPrimitiveComponent*. Setting a NULL StaticMesh."));
					}
				}
			}
		}
	}
	else
	{
		SetStaticMesh(nullptr);
	}
}

void UXRDeviceVisualizationComponent::SetMaterials(int32 MatCount) {
	for (int32 MatIndex = 0; MatIndex < MatCount; ++MatIndex)
	{
		this->SetMaterial(MatIndex, DisplayMeshMaterialOverrides[MatIndex]);
	}
}

void UXRDeviceVisualizationComponent::OnDisplayModelLoaded(UPrimitiveComponent* InDisplayComponent)
{
	if (DisplayModelLoadState == EModelLoadStatus::Pending || DisplayModelLoadState == EModelLoadStatus::InProgress)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(InDisplayComponent);
		if (InDisplayComponent != nullptr && StaticMeshComponent != nullptr && StaticMeshComponent->GetStaticMesh())
		{
			SetStaticMesh(StaticMeshComponent->GetStaticMesh());
			const int32 MatCount = FMath::Min(InDisplayComponent->GetNumMaterials(), DisplayMeshMaterialOverrides.Num());
			SetMaterials(MatCount);
			DisplayModelLoadState = EModelLoadStatus::Complete;
		}
		else 
		{
			SetStaticMesh(nullptr);
			UE_LOG(LogXRDeviceVisualizationComponent, Warning, TEXT("OnDisplayModelLoaded expects to received a UStaticMeshComponent with a valid StaticMesh. Setting a NULL StaticMesh."));
		}
	}

	if (InDisplayComponent != nullptr)
	{
		InDisplayComponent->DestroyComponent();
	}
}

void UXRDeviceVisualizationComponent::SetIsRenderingActive(bool bRenderingIsActive) 
{
	if (bIsRenderingActive != bRenderingIsActive) 
	{
		bIsRenderingActive = bRenderingIsActive;
		RefreshMesh();
	}
}