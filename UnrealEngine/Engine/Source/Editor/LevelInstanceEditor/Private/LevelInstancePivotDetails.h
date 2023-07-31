// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "IDetailCustomization.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "Input/Reply.h"

class AActor;

class FLevelInstancePivotDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	/** End IDetailCustomization interface */

private:
	/** Use MakeInstance to create an instance of this class */
	FLevelInstancePivotDetails();

	bool IsApplyButtonEnabled(TWeakObjectPtr<AActor> LevelInstancePivot) const;
	FReply OnApplyButtonClicked(TWeakObjectPtr<AActor> LevelInstancePivot);

	int32 GetSelectedPivotType() const;
	void OnSelectedPivotTypeChanged(int32 NewValue, ESelectInfo::Type SelectionType);

	// struct to hold weak ptr
	struct FActorInfo
	{
		TWeakObjectPtr<AActor> Actor;
	};

	TSharedRef<SWidget> OnGeneratePivotActorWidget(TSharedPtr<FActorInfo> Actor) const;
	FText GetSelectedPivotActorText() const;
	void OnSelectedPivotActorChanged(TSharedPtr<FActorInfo> NewValue, ESelectInfo::Type SelectionType);
	bool IsPivotActorSelectionEnabled() const;

	// Settings for Apply button
	ELevelInstancePivotType PivotType;
	TWeakObjectPtr<AActor> PivotActor;
	TArray<TSharedPtr<FActorInfo>> PivotActors;
};