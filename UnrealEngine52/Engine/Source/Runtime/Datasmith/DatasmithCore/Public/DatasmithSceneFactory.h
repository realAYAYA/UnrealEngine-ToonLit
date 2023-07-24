// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithAnimationElements.h"
#include "DatasmithDefinitions.h"
#include "DatasmithMaterialElements.h"
#include "DatasmithVariantElements.h"
#include "HAL/Platform.h"
#include "IDatasmithSceneElements.h"
#include "Templates/SharedPointer.h"

/**
 * Factory to create the scene elements used for the export and import process.
 * The shared pointer returned is the only one existing at that time.
 * Make sure to hang onto it until the scene element isn't needed anymore.
 */
class DATASMITHCORE_API FDatasmithSceneFactory
{
public:
	static TSharedPtr< IDatasmithElement > CreateElement( EDatasmithElementType InType, const TCHAR* InName );

	static TSharedPtr< IDatasmithElement > CreateElement( EDatasmithElementType InType, uint64 InSubType, const TCHAR* InName );

	static TSharedRef< IDatasmithActorElement > CreateActor( const TCHAR* InName );

	static TSharedRef< IDatasmithCameraActorElement > CreateCameraActor( const TCHAR* InName );

	static TSharedRef< IDatasmithCompositeTexture > CreateCompositeTexture();

	static TSharedRef< IDatasmithCustomActorElement > CreateCustomActor( const TCHAR* InName );

	static TSharedRef< IDatasmithLandscapeElement > CreateLandscape( const TCHAR* InName );

	static TSharedRef< IDatasmithPostProcessVolumeElement > CreatePostProcessVolume( const TCHAR* InName );

	static TSharedRef< IDatasmithEnvironmentElement > CreateEnvironment( const TCHAR* InName );

	static TSharedRef< IDatasmithPointLightElement > CreatePointLight( const TCHAR* InName );
	static TSharedRef< IDatasmithSpotLightElement > CreateSpotLight( const TCHAR* InName );
	static TSharedRef< IDatasmithDirectionalLightElement > CreateDirectionalLight( const TCHAR* InName );
	static TSharedRef< IDatasmithAreaLightElement > CreateAreaLight( const TCHAR* InName );
	static TSharedRef< IDatasmithLightmassPortalElement > CreateLightmassPortal( const TCHAR* InName );

	static TSharedRef< IDatasmithKeyValueProperty > CreateKeyValueProperty( const TCHAR* InName );

	static TSharedRef< IDatasmithMeshElement > CreateMesh( const TCHAR* InName );

	static TSharedRef< IDatasmithMeshActorElement > CreateMeshActor(const TCHAR* InName);

	static TSharedRef< IDatasmithClothElement > CreateCloth( const TCHAR* InName );

	static TSharedRef< IDatasmithClothActorElement > CreateClothActor(const TCHAR* InName);

	static TSharedRef< IDatasmithHierarchicalInstancedStaticMeshActorElement > CreateHierarchicalInstanceStaticMeshActor( const TCHAR* InName );

	static TSharedRef< IDatasmithMaterialElement > CreateMaterial( const TCHAR* InName );

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.1, "FDatasmithSceneFactory::CreateMasterMaterial will not be supported in 5.2. Please use FDatasmithSceneFactory::CreateMaterialInstance instead.")
	static TSharedRef< IDatasmithMasterMaterialElement > CreateMasterMaterial(const TCHAR* InName)
	{
		return CreateMaterialInstance(InName);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static TSharedRef< IDatasmithMaterialInstanceElement > CreateMaterialInstance(const TCHAR* InName);

	static TSharedRef< IDatasmithUEPbrMaterialElement > CreateUEPbrMaterial( const TCHAR* InName );

	/**
	 * Creates a MaterialExpression from the given type.
	 * Warning: This function's main purpose is to allow the creation of serialized material expressions.
	 *          Creating and adding new expressions to a UEPbrMaterial should be done with the IDatasmithUEPbrMaterialElement::AddMaterialExpression() function.
	 */
	static TSharedPtr< IDatasmithMaterialExpression > CreateMaterialExpression( EDatasmithMaterialExpressionType MaterialExpression );

	/**
	 * Creates an ExpressionInput from the given type.
	 * Warning: This function's main purpose is to allow the creation of serialized expression inputs. It should not be used to add inputs to a material expression.
	 */
	static TSharedRef< IDatasmithExpressionInput > CreateExpressionInput( const TCHAR* InName );

	/**
	 * Creates an ExpressionOutput from the given type.
	 * Warning: This function's main purpose is to allow the creation of serialized expression outputs. It should not be used to add outputs to a material expression.
	 */
	static TSharedRef< IDatasmithExpressionOutput > CreateExpressionOutput( const TCHAR* InName );

	static TSharedRef< IDatasmithMetaDataElement > CreateMetaData( const TCHAR* InName );

	static TSharedRef< IDatasmithMaterialIDElement > CreateMaterialId( const TCHAR* InName );

	static TSharedRef< IDatasmithPostProcessElement > CreatePostProcess();

	static TSharedRef< IDatasmithShaderElement > CreateShader( const TCHAR* InName );

	static TSharedRef< IDatasmithTextureElement > CreateTexture( const TCHAR* InName );

	static TSharedRef< IDatasmithLevelSequenceElement > CreateLevelSequence( const TCHAR* InName );
	static TSharedRef< IDatasmithTransformAnimationElement > CreateTransformAnimation( const TCHAR* InName );
	static TSharedRef< IDatasmithVisibilityAnimationElement > CreateVisibilityAnimation( const TCHAR* InName );
	static TSharedRef< IDatasmithSubsequenceAnimationElement > CreateSubsequenceAnimation( const TCHAR* InName );

	static TSharedRef< IDatasmithLevelVariantSetsElement > CreateLevelVariantSets( const TCHAR* InName );
	static TSharedRef< IDatasmithVariantSetElement > CreateVariantSet( const TCHAR* InName );
	static TSharedRef< IDatasmithVariantElement > CreateVariant( const TCHAR* InName );
	static TSharedRef< IDatasmithActorBindingElement > CreateActorBinding();
	static TSharedRef< IDatasmithPropertyCaptureElement > CreatePropertyCapture();
	static TSharedRef< IDatasmithObjectPropertyCaptureElement > CreateObjectPropertyCapture();

	static TSharedRef< IDatasmithDecalActorElement > CreateDecalActor( const TCHAR* InName );
	static TSharedRef< IDatasmithDecalMaterialElement > CreateDecalMaterial( const TCHAR* InName );

	static TSharedRef< IDatasmithScene > CreateScene( const TCHAR* InName );
	static TSharedRef< IDatasmithScene > DuplicateScene( const TSharedRef< IDatasmithScene >& InScene );
};
