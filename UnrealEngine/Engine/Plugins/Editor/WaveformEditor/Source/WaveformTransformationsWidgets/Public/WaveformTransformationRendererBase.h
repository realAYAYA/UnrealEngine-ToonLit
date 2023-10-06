// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IWaveformTransformationRenderer.h"
#include "PropertyHandle.h"

class WAVEFORMTRANSFORMATIONSWIDGETS_API FWaveformTransformationRendererBase : public IWaveformTransformationRenderer
{
public:
	FWaveformTransformationRendererBase() = default;
	
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override {};
	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(SWidget& OwnerWidget, const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override {};
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override {};
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	virtual void SetTransformationWaveInfo(const FWaveformTransformationRenderInfo& InWaveInfo) override;
	virtual void SetPropertyHandles(const TArray<TSharedRef<IPropertyHandle>>& InPropertyHandles) override;

protected:
	TSharedPtr<IPropertyHandle> GetPropertyHandle(const FName& PropertyName) const;

	template<typename T>
	T GetPropertyValue(const TSharedPtr<IPropertyHandle>& PropertyHandle)
	{
		check(PropertyHandle.IsValid());
		T OutValue;
		PropertyHandle->GetValue(OutValue);
		return OutValue;
	}

	template<typename T>
	void SetPropertyValue(const FName& PropertyName, const T Value, const EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags)
	{
		TSharedPtr<IPropertyHandle> Handle = GetPropertyHandle(PropertyName);
		Handle->SetValue(Value, Flags);
	}


	int32 BeginTransaction(const TCHAR* TransactionContext, const FText& Description, UObject* PrimaryObject);
	int32 EndTransaction();

	FWaveformTransformationRenderInfo TransformationWaveInfo;

private:
	TArray<TSharedRef<IPropertyHandle>> CachedPropertyHandles;	
};