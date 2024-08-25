// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class IPropertyHandle;
class IDetailLayoutBuilder;
class FName;

/** Helper Class for CVD Custom Details view */
class FChaosVDDetailsCustomizationUtils
{
public:
	/**
	 * Hides all categories of this view, except the ones provided in the Allowed Categories set
	 * @param DetailBuilder Layout builder of the class we are customizing
	 * @param AllowedCategories Set of category names we do not want to hide 
	 */
	static void HideAllCategories(IDetailLayoutBuilder& DetailBuilder, const TSet<FName>& AllowedCategories = TSet<FName>());

	/**
	 * Marks any property of the provided handles array as hidden if they are not valid CVD properties (meaning they don't have serialized data loaded from a CVD recording)
	 * @param InPropertyHandles Handles of properties to evaluate and hide if needed
	 */
	static void HideInvalidCVDDataWrapperProperties(TConstArrayView<TSharedPtr<IPropertyHandle>> InPropertyHandles);

	/**
	 * Marks any property of the provided handles array as hidden if they are not valid CVD properties (meaning they don't have serialized data loaded from a CVD recording), using the provided details builder
	 * @param InPropertyHandles Handles of properties to evaluate and hide if needed
	 * @param DetailBuilder Details builder that will hide the property
	 */
	static void HideInvalidCVDDataWrapperProperties(TConstArrayView<TSharedRef<IPropertyHandle>> InPropertyHandles, IDetailLayoutBuilder& DetailBuilder);

	/**
	 * Marks any property of the provided handles array as hidden if they are not valid CVD properties (meaning they don't have serialized data loaded from a CVD recording), using the provided details builder
	 * @param InPropertyHandle Property Handle to evaluate if it is valid
	 * @param bOutIsCVDBaseDataStruct Set to true if the property handle provided is from a CVD Wrapper Data Base. All other types will be deemed valid
	 */
	static bool HasValidCVDWrapperData(const TSharedPtr<IPropertyHandle>& InPropertyHandle, bool& bOutIsCVDBaseDataStruct);
};
