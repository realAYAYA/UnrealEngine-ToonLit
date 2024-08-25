// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <DetailsNameWidgetOverrideCustomization.h>

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "Containers/Set.h"
#include "PropertyPath.h"
#include "Widgets/SWidget.h"

class IDetailCategoryBuilder;
class FInstanceDataObjectFixupPanel;

/**
 * 
 */
class INSTANCEDATAOBJECTFIXUPTOOL_API FInstanceDataObjectFixupDetailCustomization : public IDetailCustomization, public TSharedFromThis<FInstanceDataObjectFixupDetailCustomization>
{
public:
	FInstanceDataObjectFixupDetailCustomization(const TSharedRef<FInstanceDataObjectFixupPanel>& DiffPanel);
	virtual ~FInstanceDataObjectFixupDetailCustomization() override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	bool IsHidden(const TSharedPtr<IPropertyHandle>& PropertyHandle) const;
	void CustomizeHandle(const TSharedRef<IPropertyHandle>& Handle, IDetailLayoutBuilder& DetailBuilder);
	TWeakPtr<FInstanceDataObjectFixupPanel> DiffPanel;
};

class INSTANCEDATAOBJECTFIXUPTOOL_API FInstanceDataObjectNameWidgetOverride : public FDetailsNameWidgetOverrideCustomization
{
public:
	FInstanceDataObjectNameWidgetOverride(const TSharedRef<FInstanceDataObjectFixupPanel>& DiffPanel);
	virtual ~FInstanceDataObjectNameWidgetOverride() override = default;
	virtual TSharedRef<SWidget> CustomizeName(TSharedRef<SWidget> InnerNameContent, FPropertyPath& Path) override;
private:
	
	int32 GetNameWidgetIndex(FPropertyPath Path) const;
	TSharedRef<SWidget> GeneratePropertyRedirectMenu(FPropertyPath Path) const;
	EVisibility DeletionSymbolVisibility(FPropertyPath Path) const;
	EVisibility ValueContentVisibility(FPropertyPath Path) const;
	
	TSet<FPropertyPath> GetRedirectOptions(const UStruct* Struct, void* Value) const;
	void GetRedirectOptions(const UStruct* Struct, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const;
	void GetRedirectOptions(const FProperty* Property, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const;
	
	TWeakPtr<FInstanceDataObjectFixupPanel> DiffPanel;

	enum ENameWidgetIndex : uint8
	{
		DisplayRegularName = 0,
		DisplayRedirectMenu = 1
	};
};

class INSTANCEDATAOBJECTFIXUPTOOL_API FHideLoosePropertiesCustomization : public IDetailCustomization, public TSharedFromThis<FInstanceDataObjectFixupDetailCustomization>
{
public:
	FHideLoosePropertiesCustomization() = default;
	virtual ~FHideLoosePropertiesCustomization() override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	static void CustomizeHandle(const TSharedRef<IPropertyHandle>& Handle, IDetailLayoutBuilder& DetailBuilder);

private:
	TWeakPtr<FInstanceDataObjectFixupPanel> DiffPanel;
};
