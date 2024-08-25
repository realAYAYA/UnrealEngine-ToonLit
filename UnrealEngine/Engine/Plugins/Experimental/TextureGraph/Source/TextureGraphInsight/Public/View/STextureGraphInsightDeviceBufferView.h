// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

//#include "SlateBasics.h"

#include "Model/TextureGraphInsightRecord.h"
#include <Widgets/Layout/SBorder.h>
#include <UObject/GCObject.h>

class UMaterialInstanceDynamic;

class TEXTUREGRAPHINSIGHT_API STextureGraphInsightDeviceBufferView : public SBorder, public FGCObject /// Need FGCObject to control garbage collection of objects
{
public:
	SLATE_BEGIN_ARGS(STextureGraphInsightDeviceBufferView) :
		_withToolTip(false),
		_withDescription(false) {}
		SLATE_ARGUMENT(RecordID, recordID)
		SLATE_ARGUMENT(RecordID, blobID)
		SLATE_ARGUMENT(bool, withToolTip)
		SLATE_ARGUMENT(bool, withDescription)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	RecordID _recordID;
	RecordID _blobID;
	uint32 _imageWidth = 1;
	uint32 _imageHeight = 1;
	FString _imageName;

	// Needs our own brush to display the blob content
	TSharedPtr<FSlateBrush> _brush;
	TObjectPtr<UMaterialInstanceDynamic> _brushMaterial = nullptr;

	void CreateWidgetBrush(UTexture* blobTexture);
	void MakeWidgetMaterial();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(_brushMaterial);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("STextureGraphInsightDeviceBufferView");
	}
};
