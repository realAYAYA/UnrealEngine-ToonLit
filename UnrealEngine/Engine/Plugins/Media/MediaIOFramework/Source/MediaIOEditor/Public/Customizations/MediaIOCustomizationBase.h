// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Misc/Attribute.h"
#include "PropertyHandle.h"
#include "Widgets/SWidget.h"

/**
 * Base implementation of different MediaIO details view customization
 */
class MEDIAIOEDITOR_API FMediaIOCustomizationBase : public IPropertyTypeCustomization
{
public:
	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

protected:
	template<class T>
	T* GetPropertyValueFromPropertyHandle()
	{
			FProperty* Property = MediaProperty->GetProperty();
			check(Property);
			check(CastField<FStructProperty>(Property));
			check(CastField<FStructProperty>(Property)->Struct);
			check(CastField<FStructProperty>(Property)->Struct->IsChildOf(T::StaticStruct()));

			TArray<void*> RawData;
			MediaProperty->AccessRawData(RawData);

			check(RawData.Num() == 1);
			T* MediaValue = reinterpret_cast<T*>(RawData[0]);
			check(MediaValue);
			return MediaValue;
	}
	
	template<class T>
	void AssignValue(const T& NewValue) const
	{
		AssignValueImpl(reinterpret_cast<const void*>(&NewValue));
	}

	virtual TAttribute<FText> GetContentText() = 0;
	virtual TSharedRef<SWidget> HandleSourceComboButtonMenuContent() = 0;
	TSharedPtr<IPropertyHandle> GetMediaProperty() const { return MediaProperty; };
	TArray<UObject*> GetCustomizedObjects() const;

	FName DeviceProviderName;

private:
	void AssignValueImpl(const void* NewValue) const;

	/** Pointer to the property handle. */
	TSharedPtr<IPropertyHandle> MediaProperty;
};
