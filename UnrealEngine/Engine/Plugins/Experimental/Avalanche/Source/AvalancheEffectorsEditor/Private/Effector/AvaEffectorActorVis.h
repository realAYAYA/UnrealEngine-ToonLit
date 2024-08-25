// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Effector/CEEffectorActor.h"
#include "AvaVisBase.h"

class UCEEffectorComponent;

struct HAvaEffectorActorZoneHitProxy : HAvaHitProxy
{
	DECLARE_HIT_PROXY();

	HAvaEffectorActorZoneHitProxy(const UActorComponent* InComponent, bool bInInnerZone)
		: HAvaHitProxy(InComponent)
		, bInnerZone(bInInnerZone)
	{}

	bool bInnerZone = false;
};

/** Custom visualization for effector actor to handle weight zones */
class FAvaEffectorActorVisualizer : public FAvaVisualizerBase
{
public:
	using Super = FAvaVisualizerBase;

	FAvaEffectorActorVisualizer();

	//~ Begin FAvaVisualizerBase
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TMap<UObject*, TArray<FProperty*>> GatherEditableProperties(UObject* InObject) const override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* InVisProxy, const FViewportClick& InClick) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* InViewportClient, FVector& OutLocation) const override;
	virtual bool GetWidgetMode(const FEditorViewportClient* InViewportClient, UE::Widget::EWidgetMode& OutMode) const override;
	virtual bool GetWidgetAxisList(const FEditorViewportClient* InViewportClient, UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const override;
	virtual bool GetWidgetAxisListDragOverride(const FEditorViewportClient* InViewportClient, UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const override;
	virtual bool ResetValue(FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy) override;
	virtual bool IsEditing() const override;
	virtual void EndEditing() override;
	virtual void StoreInitialValues() override;
	virtual FBox GetComponentBounds(const UActorComponent* InComponent) const override;
	virtual bool HandleInputDeltaInternal(FEditorViewportClient* InViewportClient, FViewport* InViewport, const FVector& InAccumulatedTranslation, const FRotator& InAccumulatedRotation, const FVector& InAccumulatedScale) override;
	virtual void DrawVisualizationEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex) override;
	virtual void DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex) override;
	//~ End FAvaVisualizerBase

	ACEEffectorActor* GetEffectorActor() const { return EffectorActorWeak.Get(); };
protected:

	FVector GetHandleZoneLocation(const ACEEffectorActor* InEffectorActor, bool bInInnerSize) const;
	void DrawZoneButton(const ACEEffectorActor* InEffectorActor, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32 InIconIndex, bool bInInnerZone, FLinearColor InColor) const;

	FProperty* InnerRadiusProperty;
	FProperty* OuterRadiusProperty;
	FProperty* InnerExtentProperty;
	FProperty* OuterExtentProperty;
	FProperty* PlaneSpacingProperty;

	TWeakObjectPtr<ACEEffectorActor> EffectorActorWeak = nullptr;
	float InitialInnerRadius = 0.f;
	float InitialOuterRadius = 0.f;
	FVector InitialInnerExtent = FVector(0.f);
	FVector InitialOuterExtent = FVector(0.f);
	float InitialPlaneSpacing = 0.f;
	bool bEditingInnerZone = false;
	bool bEditingOuterZone = false;
};
