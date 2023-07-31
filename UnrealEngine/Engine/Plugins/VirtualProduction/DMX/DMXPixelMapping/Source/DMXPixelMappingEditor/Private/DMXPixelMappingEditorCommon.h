// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDMXPixelMappingEditor, All, All);

class UDMXPixelMappingBaseComponent;

class FDMXPixelMappingToolkit;
class FDMXPixelMappingHierarchyItemWidgetModel;
class FDMXPixelMappingPaletteWidgetViewModel;
class FDMXPixelMappingComponentTemplate;

using FDMXPixelMappingToolkitPtr = TSharedPtr<FDMXPixelMappingToolkit>;
using FDMXPixelMappingToolkitWeakPtr = TWeakPtr<FDMXPixelMappingToolkit>;
using FDMXPixelMappingHierarchyItemWidgetModelPtr = TSharedPtr<FDMXPixelMappingHierarchyItemWidgetModel>;
using FDMXPixelMappingHierarchyWidgetModelWeakPtr = TWeakPtr<FDMXPixelMappingHierarchyItemWidgetModel>;
using FDMXPixelMappingHierarchyItemWidgetModelArr = TArray<FDMXPixelMappingHierarchyItemWidgetModelPtr>;
using FDMXPixelMappingPreviewWidgetViewModelPtr = TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>;
using FDMXPixelMappingPreviewWidgetViewModelArray = TArray<FDMXPixelMappingPreviewWidgetViewModelPtr>;
using FDMXPixelMappingComponentTemplatePtr = TSharedPtr<FDMXPixelMappingComponentTemplate>;
using FDMXPixelMappingComponentTemplateArray = TArray<FDMXPixelMappingComponentTemplatePtr>;
