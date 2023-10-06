// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"


// Forward Declarations
enum class EWaveTableCurve : uint8;
enum class EWaveTableSamplingMode : uint8;
struct FWaveTableTransform;


namespace WaveTable::Editor
{
	class WAVETABLEEDITOR_API FWaveTableDataLayoutCustomization : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance()
		{
			return MakeShared<FWaveTableDataLayoutCustomization>();
		}

		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	};

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
		virtual EWaveTableSamplingMode GetSamplingMode() const;

		void CachePCMFromFile();
		EWaveTableCurve GetCurve() const;
		int32 GetOwningArrayIndex() const;
		bool IsScaleableCurve() const;

		TSharedPtr<IPropertyHandle> CurveHandle;
		TSharedPtr<IPropertyHandle> ChannelIndexHandle;
		TSharedPtr<IPropertyHandle> FilePathHandle;
		TSharedPtr<IPropertyHandle> SourceDataHandle;
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
} // namespace WaveTable::Editor
