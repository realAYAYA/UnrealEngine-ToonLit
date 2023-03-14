// Copyright Epic Games, Inc. All Rights Reserved.

#include "PointLightGizmo.h"
#include "Components/PointLightComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PointLightGizmo)

#define LOCTEXT_NAMESPACE "UScalableSphereGizmo"

UInteractiveGizmo* UPointLightGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	UPointLightGizmo* NewGizmo = NewObject<UPointLightGizmo>(SceneState.GizmoManager);
	NewGizmo->SetWorld(SceneState.World);
	return NewGizmo;
}

UPointLightGizmo::UPointLightGizmo()
{
	LightActor = nullptr;
	AttenuationGizmo = nullptr;
	TransformProxy = nullptr;
}

void UPointLightGizmo::Setup()
{
	
}

void UPointLightGizmo::Tick(float DeltaTime)
{
	 // Make sure that radius of the sphere is correct every frame
	AttenuationGizmo->SetRadius(LightActor->PointLightComponent->AttenuationRadius);
}

void UPointLightGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{

}

void UPointLightGizmo::Shutdown()
{
	if (AttenuationGizmo)
	{
		GetGizmoManager()->DestroyGizmo(AttenuationGizmo);
		AttenuationGizmo = nullptr;
	}
}

void UPointLightGizmo::SetSelectedObject(APointLight* InLight)
{
	LightActor = InLight;

	// Create a transform proxy and add the light to it
	// TODO: Cannot change selected object right now as there is no way to remove a component from the transform proxy
	if (!TransformProxy)
	{
		TransformProxy = NewObject<USubTransformProxy>(this);
	}

	USceneComponent* SceneComponent = LightActor->GetRootComponent();
	TransformProxy->AddComponent(SceneComponent);
}

USubTransformProxy* UPointLightGizmo::GetTransformProxy()
{
	return TransformProxy;
}


void UPointLightGizmo::CreateLightGizmo()
{
	if (!LightActor)
	{
		return;
	}

	AttenuationGizmo = GetGizmoManager()->CreateGizmo<UScalableSphereGizmo>(UInteractiveGizmoManager::DefaultScalableSphereBuilderIdentifier);

	AttenuationGizmo->SetTarget(TransformProxy);

	AttenuationGizmo->UpdateRadiusFunc = [this](float NewRadius) { this->OnAttenuationUpdate(NewRadius); };

	AttenuationGizmo->TransactionDescription = LOCTEXT("PointLightGizmo", "Point Light Attenuation");
}

void UPointLightGizmo::SetWorld(UWorld* InWorld)
{
	World = InWorld;
}

void UPointLightGizmo::OnAttenuationUpdate(float NewRadius)
{
	// Update the attenuation radius
	// TODO: Is this the right way to update radius?

	LightActor->PointLightComponent->Modify();
	LightActor->PointLightComponent->AttenuationRadius = NewRadius;

}

#undef LOCTEXT_NAMESPACE
