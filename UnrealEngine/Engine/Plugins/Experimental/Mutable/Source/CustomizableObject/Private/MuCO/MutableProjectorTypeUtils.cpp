// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/MutableProjectorTypeUtils.h"

#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuR/Parameters.h"

ECustomizableObjectProjectorType ProjectorUtils::GetEquivalentProjectorType (mu::PROJECTOR_TYPE ProjectorType)
{
	// Translate projector type from Mutable Core enum type to CO enum type
	switch (ProjectorType)
	{
	case mu::PROJECTOR_TYPE::PLANAR:
		return ECustomizableObjectProjectorType::Planar;
		
	case mu::PROJECTOR_TYPE::CYLINDRICAL:
		return ECustomizableObjectProjectorType::Cylindrical;
		
	case mu::PROJECTOR_TYPE::WRAPPING:
		return ECustomizableObjectProjectorType::Wrapping;
		
	case mu::PROJECTOR_TYPE::COUNT:
	default:
		checkNoEntry();
		return ECustomizableObjectProjectorType::Planar;
	}
}


mu::PROJECTOR_TYPE ProjectorUtils::GetEquivalentProjectorType (ECustomizableObjectProjectorType ProjectorType)
{
	if (GetEquivalentProjectorType(mu::PROJECTOR_TYPE::PLANAR) == ProjectorType)
	{
		return mu::PROJECTOR_TYPE::PLANAR;
	}
	if (GetEquivalentProjectorType(mu::PROJECTOR_TYPE::WRAPPING) == ProjectorType)
	{
		return mu::PROJECTOR_TYPE::WRAPPING;
	}
	if (GetEquivalentProjectorType(mu::PROJECTOR_TYPE::CYLINDRICAL) == ProjectorType)
	{
		return mu::PROJECTOR_TYPE::CYLINDRICAL;
	}

	checkNoEntry();
	return mu::PROJECTOR_TYPE::COUNT;		// Invalid 
}
