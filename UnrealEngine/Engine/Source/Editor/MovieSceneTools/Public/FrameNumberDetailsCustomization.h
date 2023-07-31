// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/NumericTypeInterface.h"

class IDetailLayoutBuilder;

/**
 *  Customize the FFrameNumber to support conversion from seconds/frames/timecode formats.
 */
class MOVIESCENETOOLS_API FFrameNumberDetailsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(TSharedPtr<INumericTypeInterface<double>> InNumericTypeInterface)
	{
		return MakeShared<FFrameNumberDetailsCustomization>(InNumericTypeInterface);
	}

	FFrameNumberDetailsCustomization(TSharedPtr<INumericTypeInterface<double>> InNumericTypeInterface)
	{
		NumericTypeInterface = InNumericTypeInterface;
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FText OnGetTimeText() const;
	FText OnGetTimeToolTipText() const;
	void OnTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	/** The Numeric Type interface used to convert between display formats and internal tick resolution. */
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface;

	/** Store the property handle to the FrameNumber field so we can get/set the value on the object via text box callbacks. */
	TSharedPtr<IPropertyHandle> FrameNumberProperty;

	/** If they've used the UIMin metadata on the FFrameNumber property, we store that for use via text box callbacks. */
	int32 UIClampMin;
	/** If they've used the UIMax metadata on the FFrameNumber property, we store that for use via text box callbacks. */
	int32 UIClampMax;
};
