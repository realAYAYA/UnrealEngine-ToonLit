// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/MediaTextureCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Engine/Texture.h"
#include "Widgets/SNullWidget.h"
#include "PropertyHandle.h"


/* IDetailCustomization interface
 *****************************************************************************/

void FMediaTextureCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'Texture' category
	IDetailCategoryBuilder& MediaTextureCategory = DetailBuilder.EditCategory("MediaTexture");
	{
		MediaTextureCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UTexture, Filter), UTexture::StaticClass());
	}

	DetailBuilder.HideCategory("Compression");
	DetailBuilder.HideCategory("Texture");
}
