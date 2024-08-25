// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FReply;
class UAvaShapeDynamicMeshBase;
class UDynamicMaterialModel;

/* Used to create the details materials meshes widget and export to StaticMesh */
class FAvaMeshesDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FAvaMeshesDetailCustomization>();
	}

	FAvaMeshesDetailCustomization()
	{
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	// Handler when the convert button is clicked
	FReply OnConvertToStaticMeshClicked();

	// Enable or disable the button when selected object is not compatible
	bool CanConvertToStaticMesh() const;

	TArray<TWeakObjectPtr<UAvaShapeDynamicMeshBase>> SelectedDynamicMeshes;
};