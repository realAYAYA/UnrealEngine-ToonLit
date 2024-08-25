// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialHub/AvaTextMaterialHub.h"
#include "AvaTextDefs.h"

UAvaTextMaterialHub* UAvaTextMaterialHub::Get()
{
	UAvaTextMaterialHub* MaterialHub = GetMutableDefault<UAvaTextMaterialHub>();

	static bool bInitialized = false;
	
	if (!bInitialized)
	{
		// Make sure to load config, in case custom values are specified by the user
		MaterialHub->LoadConfig();

		// Initializes the internal maps used to access/lookup materials
		MaterialHub->SetupInternalMaps();

		bInitialized = true;
	}

	return MaterialHub;
}

UAvaTextMaterialHub::UAvaTextMaterialHub()
{
	SolidMaterial                    = TEXT("/Avalanche/Text3DResources/Materials/Solid/M_DefaultText3DMaterial.M_DefaultText3DMaterial");
	SolidTranslucentMaterial         = TEXT("/Avalanche/Text3DResources/Materials/Solid/M_DefaultText3DMaterial_Translucent.M_DefaultText3DMaterial_Translucent");
	SolidTranslucentUnlitMaterial    = TEXT("/Avalanche/Text3DResources/Materials/Solid/M_DefaultText3DMaterial_Translucent_Unlit.M_DefaultText3DMaterial_Translucent_Unlit");
	SolidMaskedMaterial              = TEXT("/Avalanche/Text3DResources/Materials/Solid/M_DefaultText3DMaterial_Masked.M_DefaultText3DMaterial_Masked");
	SolidUnlitMaterial               = TEXT("/Avalanche/Text3DResources/Materials/Solid/M_DefaultText3DMaterial_Unlit.M_DefaultText3DMaterial_Unlit");
	SolidUnlitMaskedTextMaterial     = TEXT("/Avalanche/Text3DResources/Materials/Solid/M_DefaultText3DMaterial_Unlit_Masked.M_DefaultText3DMaterial_Unlit_Masked");
	
	GradientMaterial                 = TEXT("/Avalanche/Text3DResources/Materials/Gradient/M_GradientText3DMaterial.M_GradientText3DMaterial");
	GradientTranslucentMaterial      = TEXT("/Avalanche/Text3DResources/Materials/Gradient/M_GradientText3DMaterial_Translucent.M_GradientText3DMaterial_Translucent");
	GradientTranslucentUnlitMaterial = TEXT("/Avalanche/Text3DResources/Materials/Gradient/M_GradientText3DMaterial_Translucent_Unlit.M_GradientText3DMaterial_Translucent_Unlit");
	GradientMaskedMaterial           = TEXT("/Avalanche/Text3DResources/Materials/Gradient/M_GradientText3DMaterial_Masked.M_GradientText3DMaterial_Masked");
	GradientUnlitMaterial            = TEXT("/Avalanche/Text3DResources/Materials/Gradient/M_GradientText3DMaterial_Unlit.M_GradientText3DMaterial_Unlit");
	GradientUnlitMaskedMaterial      = TEXT("/Avalanche/Text3DResources/Materials/Gradient/M_GradientText3DMaterial_Unlit_Masked.M_GradientText3DMaterial_Unlit_Masked");

	TexturedMaterial                 = TEXT("/Avalanche/Text3DResources/Materials/Textured/M_TexturedText3DMaterial.M_TexturedText3DMaterial");
	TexturedTranslucentMaterial      = TEXT("/Avalanche/Text3DResources/Materials/Textured/M_TexturedText3DMaterial_Translucent.M_TexturedText3DMaterial_Translucent");
	TexturedTranslucentUnlitMaterial = TEXT("/Avalanche/Text3DResources/Materials/Textured/M_TexturedText3DMaterial_Translucent_Unlit.M_TexturedText3DMaterial_Translucent_Unlit");
	TexturedMaskedMaterial           = TEXT("/Avalanche/Text3DResources/Materials/Textured/M_TexturedText3DMaterial_Masked.M_TexturedText3DMaterial_Masked");
	TexturedUnlitMaterial            = TEXT("/Avalanche/Text3DResources/Materials/Textured/M_TexturedText3DMaterial_Unlit.M_TexturedText3DMaterial_Unlit");
	TexturedUnlitMaskedMaterial      = TEXT("/Avalanche/Text3DResources/Materials/Textured/M_TexturedText3DMaterial_Unlit_Masked.M_TexturedText3DMaterial_Unlit_Masked");
}

UMaterialInterface* UAvaTextMaterialHub::GetMaterial(FAvaTextMaterialSettings InMaterialSettings)
{
	return Get()->GetMaterialInternal(InMaterialSettings);
}

FAvaTextMaterialSettings* UAvaTextMaterialHub::GetSettingsFromMaterial(const UMaterialInterface* InMaterial)
{
	return Get()->GetSettingsFromMaterialInternal(InMaterial);
}

void UAvaTextMaterialHub::SetupInternalMaps()
{
	AddMaterial(SolidMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::Solid, EAvaTextMaterialFeatures::None));

	AddMaterial(SolidTranslucentMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::Solid, EAvaTextMaterialFeatures::Translucent));

	AddMaterial(SolidTranslucentUnlitMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::Solid, EAvaTextMaterialFeatures::Translucent | EAvaTextMaterialFeatures::Unlit));
		
	AddMaterial(SolidMaskedMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Solid, EAvaTextMaterialFeatures::GradientMask));

	AddMaterial(SolidUnlitMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Solid, EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(SolidUnlitMaskedTextMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Solid, EAvaTextMaterialFeatures::GradientMask | EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(GradientMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Gradient, EAvaTextMaterialFeatures::None));

	AddMaterial(GradientTranslucentMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::Gradient, EAvaTextMaterialFeatures::Translucent));

	AddMaterial(GradientTranslucentUnlitMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::Gradient, EAvaTextMaterialFeatures::Translucent | EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(GradientMaskedMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Gradient, EAvaTextMaterialFeatures::GradientMask));
	
	AddMaterial(GradientUnlitMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Gradient, EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(GradientUnlitMaskedMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::Gradient, EAvaTextMaterialFeatures::GradientMask | EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(TexturedMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::FromTexture, EAvaTextMaterialFeatures::None));

	AddMaterial(TexturedTranslucentMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::FromTexture, EAvaTextMaterialFeatures::Translucent));

	AddMaterial(TexturedTranslucentUnlitMaterial,
			FAvaTextMaterialSettings(EAvaTextColoringStyle::FromTexture, EAvaTextMaterialFeatures::Translucent | EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(TexturedMaskedMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::FromTexture, EAvaTextMaterialFeatures::GradientMask));
	
	AddMaterial(TexturedUnlitMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::FromTexture, EAvaTextMaterialFeatures::Unlit));
	
	AddMaterial(TexturedUnlitMaskedMaterial,
		FAvaTextMaterialSettings(EAvaTextColoringStyle::FromTexture, EAvaTextMaterialFeatures::GradientMask | EAvaTextMaterialFeatures::Unlit));
}

void UAvaTextMaterialHub::AddMaterial(
	const FSoftObjectPath& InMaterialSoftObjectPath
	, FAvaTextMaterialSettings InMaterialSettings)
{
	MaterialsMap.Add(InMaterialSettings, InMaterialSoftObjectPath);

	FAvaTextMaterialId MaterialId(InMaterialSoftObjectPath);
	FAvaTextMaterialData MaterialData(InMaterialSoftObjectPath, InMaterialSettings);

	MaterialDataMap.Add(MoveTemp(MaterialId), MoveTemp(MaterialData));
}

UMaterialInterface* UAvaTextMaterialHub::GetMaterialInternal(FAvaTextMaterialSettings InMaterialSettings) const
{
	if (MaterialsMap.Contains(InMaterialSettings))
	{
		if (const FSoftObjectPath* MaterialMatch = MaterialsMap.Find(InMaterialSettings))
		{
			return Cast<UMaterialInterface>(MaterialMatch->TryLoad());
		}
	}
	
	return nullptr;
}

FAvaTextMaterialSettings* UAvaTextMaterialHub::GetSettingsFromMaterialInternal(const UMaterialInterface* InMaterial)
{
	if (!InMaterial)
	{
		return nullptr;
	}
	
	const FAvaTextMaterialId CurrentMaterialId(InMaterial);

	// look for current material in static material data map, to see if it's a known one
	if (FAvaTextMaterialData* MaterialDataMatch = MaterialDataMap.Find(CurrentMaterialId))
	{
		// retrieve material info
		return &MaterialDataMatch->MaterialSettings;
	}
	
	return nullptr;
}
