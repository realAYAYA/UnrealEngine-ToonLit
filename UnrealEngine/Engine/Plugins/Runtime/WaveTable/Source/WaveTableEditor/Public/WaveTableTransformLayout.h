// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Curves/SimpleCurve.h"
#include "IPropertyTypeCustomization.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SCurveEditor.h"
#include "WaveTableSettings.h"
#include "WaveTableTransform.h"


namespace WaveTable
{
	namespace Editor
	{
		class WAVETABLEEDITOR_API FTransformLayoutCustomizationBase : public IPropertyTypeCustomization
		{
		public:
			//~ Begin IPropertyTypeCustomization
			virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
			virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
			//~ End IPropertyTypeCustomization

		protected:
			virtual TSet<EWaveTableCurve> GetSupportedCurves() const;
			virtual FWaveTableTransform* GetTransform() const = 0;
			virtual bool IsBipolar() const = 0;

			void CachePCMFromFile();
			EWaveTableCurve GetCurve() const;
			int32 GetOwningArrayIndex() const;
			bool IsScaleableCurve() const;

			TSharedPtr<IPropertyHandle> CurveHandle;
			TSharedPtr<IPropertyHandle> ChannelIndexHandle;
			TSharedPtr<IPropertyHandle> FilePathHandle;
			TSharedPtr<IPropertyHandle> WaveTableOptionsHandle;

		private:
			void CustomizeCurveSelector(IDetailChildrenBuilder& ChildBuilder);

			TMap<FString, FName> CurveDisplayStringToNameMap;
		};

		class WAVETABLEEDITOR_API FTransformLayoutCustomization : public FTransformLayoutCustomizationBase
		{
		public:
			static TSharedRef<IPropertyTypeCustomization> MakeInstance()
			{
				return MakeShared<FTransformLayoutCustomization>();
			}

			virtual bool IsBipolar() const override;
			virtual FWaveTableTransform* GetTransform() const override;
		};
	} // namespace Editor
} // namespace WaveTable
