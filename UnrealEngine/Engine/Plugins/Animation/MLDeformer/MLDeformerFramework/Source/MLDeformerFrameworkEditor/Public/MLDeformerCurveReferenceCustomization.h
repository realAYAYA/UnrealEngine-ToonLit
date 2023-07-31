// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"

class USkeleton;

namespace UE::MLDeformer
{
	/**
	 * The curve reference property detail customization.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerCurveReferenceCustomization
		: public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		// IPropertyTypeCustomization overrides.
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
		// ~END IPropertyTypeCustomization overrides.

	protected:
		void SetSkeleton(TSharedRef<IPropertyHandle> StructPropertyHandle);
		virtual void SetPropertyHandle(TSharedRef<IPropertyHandle> StructPropertyHandle);
		TSharedPtr<IPropertyHandle> FindStructMemberProperty(TSharedRef<IPropertyHandle> PropertyHandle, const FName& PropertyName);

		/** Property to change after curve has been picked. */
		TSharedPtr<IPropertyHandle> CurveNameProperty = nullptr;

		/** The Skeleton we get the curves from. */
		TObjectPtr<USkeleton> Skeleton = nullptr;

	private:
		virtual void OnCurveSelectionChanged(const FString& Name);
		virtual FString OnGetSelectedCurve() const;
		virtual USkeleton* OnGetSkeleton() const;
	};
}	// namespace UE::MLDeformer
