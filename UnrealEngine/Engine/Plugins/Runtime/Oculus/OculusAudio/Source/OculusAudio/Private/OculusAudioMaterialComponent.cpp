// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusAudioMaterialComponent.h"
#include "OculusAudioDllManager.h"

UOculusAudioMaterialComponent::UOculusAudioMaterialComponent()
	: MaterialPreset(EOculusAudioMaterial::NOMATERIAL) // default
{
	ResetAcousticMaterialPreset();
}

void UOculusAudioMaterialComponent::ConstructMaterial(ovrAudioMaterial ovrMaterial)
{
	ovrResult Result = ovrSuccess;
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 63.5f,   Absorption63Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 125.0f,  Absorption125Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 250.0f,  Absorption250Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 500.0f,  Absorption500Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 1000.0f, Absorption1000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 2000.0f, Absorption2000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 4000.0f, Absorption4000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Absorption, 8000.0f, Absorption8000Hz);
	check(Result == ovrSuccess);

	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 63.5f,   Transmission63Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 125.0f,  Transmission125Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 250.0f,  Transmission250Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 500.0f,  Transmission500Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 1000.0f, Transmission1000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 2000.0f, Transmission2000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 4000.0f, Transmission4000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Transmission, 8000.0f, Transmission8000Hz);
	check(Result == ovrSuccess);

	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 63.5f,   Scattering63Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 125.0f,  Scattering125Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 250.0f,  Scattering250Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 500.0f,  Scattering500Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 1000.0f, Scattering1000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 2000.0f, Scattering2000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 4000.0f, Scattering4000Hz);
	check(Result == ovrSuccess);
	Result = OVRA_CALL(ovrAudio_AudioMaterialSetFrequency)(ovrMaterial, ovrAudioMaterialProperty_Scattering, 8000.0f, Scattering8000Hz);
	check(Result == ovrSuccess);
}

void UOculusAudioMaterialComponent::ResetAcousticMaterialPreset()
{
	Absorption63Hz = .0f;
	Absorption125Hz = .0f;
	Absorption250Hz = .0f;
	Absorption500Hz = .0f;
	Absorption1000Hz = .0f;
	Absorption2000Hz = .0f;
	Absorption4000Hz = .0f;
	Absorption8000Hz = .0f;

	Transmission63Hz = .0f;
	Transmission125Hz = .0f;
	Transmission250Hz = .0f;
	Transmission500Hz = .0f;
	Transmission1000Hz = .0f;
	Transmission2000Hz = .0f;
	Transmission4000Hz = .0f;
	Transmission8000Hz = .0f;

	Scattering63Hz = .0f;
	Scattering125Hz = .0f;
	Scattering250Hz = .0f;
	Scattering500Hz = .0f;
	Scattering1000Hz = .0f;
	Scattering2000Hz = .0f;
	Scattering4000Hz = .0f;
	Scattering8000Hz = .0f;
}

#if WITH_EDITOR

	void UOculusAudioMaterialComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
	{
		FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, MaterialPreset))
		{
			if (IsValidMaterialPreset()) // a valid existing material until we allow custom ones
			{
				switch (MaterialPreset)
				{
				case EOculusAudioMaterial::ACOUSTICTILE:
					AssignCoefficients(
						{ .50f, .50f, .70f, .60f, .70f, .70f, .50f, .50f },
						{ .05f, .05f, .04f, .03f, .02f, .005f, .002f, .001f },
						{ .10f, .10f, .15f, .20f, .20f, .25f, .30f, .35f });
					break;
				case EOculusAudioMaterial::BRICK:
					AssignCoefficients(
						{ .02f, .02f, .02f, .03f, .04f, .05f, .07f, .07f },
						{ .025f, .025f, .019f, .01f, .0045f, .0018f, .00089f, .00089f },
						{ .10f, .20f, .25f, .30f, .35f, .40f, .45f, .50f });
					break;
				case EOculusAudioMaterial::BRICKPAINTED:
					AssignCoefficients(
						{ .01f, .01f, .01f, .02f, .02f, .02f, .03f, .03f },
						{ .025f, .025f, .019f, .01f, .0045f, .0018f, .00089f, .00089f },
						{ .10f, .15f, .15f, .20f, .20f, .20f, .25f, .30f });
					break;
				case EOculusAudioMaterial::CARPET:
					AssignCoefficients(
						{ .01f, .01f, .05f, .10f, .20f, .45f, .65f, .70f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .10f, .15f, .20f, .30f, .45f, .50f });
					break;
				case EOculusAudioMaterial::CARPETHEAVY:
					AssignCoefficients(
						{ .02f, .02f, .06f, .14f, .37f, .48f, .63f, .63f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .10f, .15f, .20f, .30f, .45f, .50f });
					break;
				case EOculusAudioMaterial::CARPETHEAVYPADDED:
					AssignCoefficients(
						{ .08f, .08f, .24f, .57f, .69f, .71f, .73f, .73f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .10f, .15f, .20f, .30f, .45f, .50f });
					break;
				case EOculusAudioMaterial::CERAMICTILE:
					AssignCoefficients(
						{ .01f, .01f, .01f, .01f, .01f, .02f, .02f, .02f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .12f, .14f, .16f, .18f, .20f, .22f });
					break;
				case EOculusAudioMaterial::CONCRETE:
					AssignCoefficients(
						{ .01f, .01f, .01f, .02f, .02f, .02f, .02f, .02f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .11f, .12f, .13f, .14f, .15f, .20f });
					break;
				case EOculusAudioMaterial::CONCRETEROUGH:
					AssignCoefficients(
						{ .01f, .01f, .02f, .04f, .06f, .08f, .10f, .10f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .12f, .15f, .20f, .25f, .30f, .35f });
					break;
				case EOculusAudioMaterial::CONCRETEBLOCK:
					AssignCoefficients(
						{ .36f, .36f, .44f, .31f, .29f, .39f, .21f, .21f },
						{ .02f, .02f, .01f, .0063f, .0035f, .00011f, .00063f, .0005f },
						{ .10f, .10f, .12f, .15f, .20f, .30f, .40f, .45f });
					break;
				case EOculusAudioMaterial::CONCRETEBLOCKPAINTED:
					AssignCoefficients(
						{ .10f, .10f, .05f, .06f, .07f, .09f, .08f, .08f },
						{ .02f, .02f, .01f, .0063f, .0035f, .00011f, .00063f, .00063f },
						{ .10f, .10f, .11f, .13f, .15f, .16f, .20f, .25f });
					break;
				case EOculusAudioMaterial::CURTAIN:
					AssignCoefficients(
						{ .05f, .07f, .31f, .49f, .75f, .70f, .60f, .60f },
						{ .50f, .42f, .39f, .21f, .14f, .079f, .045f, .04f },
						{ .10f, .10f, .15f, .2f, .3f, .4f, .5f, .5f });
					break;
				case EOculusAudioMaterial::FOLIAGE:
					AssignCoefficients(
						{ .03f, .03f, .06f, .11f, .17f, .27f, .31f, .31f },
						{ .95f, .9f, .9f, .9f, .8f, .5f, .3f, .25f },
						{ .20f, .20f, .3f, .4f, .5f, .7f, .8f, .8f });
					break;
				case EOculusAudioMaterial::GLASS:
					AssignCoefficients(
						{ .35f, .35f, .25f, .18f, .12f, .07f, .05f, .05f },
						{ .200f, .125f, .089f, .05f, .028f, .022f, .079f, .05f },
						{ .05f, .05f, .05f, .05f, .05f, .05f, .05f, .05f });
					break;
				case EOculusAudioMaterial::GLASSHEAVY:
					AssignCoefficients(
						{ .18f, .18f, .06f, .04f, .03f, .02f, .02f, .02f },
						{ .06f, .056f, .039f, .028f, .02f, .032f, .014f, .010f },
						{ .05f, .05f, .05f, .05f, .05f, .05f, .05f, .05f });
					break;
				case EOculusAudioMaterial::GRASS:
					AssignCoefficients(
						{ .11f, .11f, .26f, .60f, .69f, .92f, .99f, .99f },
						{ .0f, .0f, .0f, .0f, .0f, .0f, .0f, .0f },
						{ .20f, .25f, .30f, .40f, .50f, .60f, .70f, .80f });
					break;
				case EOculusAudioMaterial::GRAVEL:
					AssignCoefficients(
						{ .25f, .25f, .60f, .65f, .70f, .75f, .80f, .80f },
						{ .0f, .0f, .0f, .0f, .0f, .0f, .0f, .0f },
						{ .20f, .25f, .30f, .40f, .50f, .60f, .70f, .80f });
					break;
				case EOculusAudioMaterial::GYPSUMBOARD:
					AssignCoefficients(
						{ .29f, .29f, .10f, .05f, .04f, .07f, .09f, .09f },
						{ .05f, .035f, .0125f, .0056f, .0025f, .0013f, .0032f, .002f },
						{ .10f, .10f, .11f, .12f, .13f, .14f, .15f, .20f });
					break;
				case EOculusAudioMaterial::PLASTERONBRICK:
					AssignCoefficients(
						{ .01f, .01f, .02f, .02f, .03f, .04f, .05f, .05f },
						{ .025f, .025f, .019f, .01f, .0045f, .0018f, .00089f, .0006f },
						{ .10f, .10f, .11f, .12f, .13f, .14f, .15f, .20f });
					break;
				case EOculusAudioMaterial::PLASTERONCONCRETEBLOCK:
					AssignCoefficients(
						{ .12f, .12f, .09f, .07f, .05f, .05f, .04f, .04f },
						{ .02f, .02f, .01f, .0063f, .0035f, .00011f, .00063f, .0005f },
						{ .10f, .10f, .11f, .12f, .13f, .14f, .15f, .20f });
					break;
				case EOculusAudioMaterial::SOIL:
					AssignCoefficients(
						{ .15f, .15f, .25f, .40f, .55f, .60f, .60f, .60f },
						{ .0f, .0f, .0f, .0f, .0f, .0f, .0f, .0f },
						{ .10f, .15f, .20f, .25f, .40f, .55f, .70f, .80f });
					break;
				case EOculusAudioMaterial::SOUNDPROOF:
					AssignCoefficients(
						{ 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f },
						{ .0f, .0f, .0f, .0f, .0f, .0f, .0f, .0f },
						{ .0f, .0f, .0f, .0f, .0f, .0f, .0f, .0f });
					break;
				case EOculusAudioMaterial::SNOW:
					AssignCoefficients(
						{ .45f, .45f, .75f, .90f, .95f, .95f, .95f, .95f },
						{ .0f, .0f, .0f, .0f, .0f, .0f, .0f, .0f },
						{ .15f, .20f, .30f, .40f, .50f, .60f, .75f, .80f });
					break;
				case EOculusAudioMaterial::STEEL:
					AssignCoefficients(
						{ .05f, .05f, .10f, .10f, .10f, .07f, .02f, .02f },
						{ .3f, .25f, .2f, .17f, .089f, .089f, .0056f, .003f },
						{ .10f, .10f, .10f, .10f, .10f, .10f, .10f, .10f });
					break;
				case EOculusAudioMaterial::WATER:
					AssignCoefficients(
						{ .01f, .01f, .01f, .01f, .02f, .02f, .03f, .03f },
						{ .03f, .03f, .03f, .03f, .02f, .015f, .01f, .01f },
						{ .10f, .10f, .10f, .10f, .07f, .05f, .05f, .05f });
					break;
				case EOculusAudioMaterial::WOODTHIN:
					AssignCoefficients(
						{ .42f, .42f, .21f, .10f, .08f, .06f, .06f, .06f },
						{ .22f, .2f, .125f, .079f, .1f, .089f, .05f, .03f },
						{ .10f, .10f, .10f, .10f, .10f, .10f, .15f, .15f });
					break;
				case EOculusAudioMaterial::WOODTHICK:
					AssignCoefficients(
						{ .19f, .19f, .14f, .09f, .06f, .06f, .05f, .05f },
						{ .04f, .035f, .028f, .028f, .028f, .011f, .0071f, .005f },
						{ .10f, .10f, .10f, .10f, .10f, .10f, .15f, .15f });
					break;
				case EOculusAudioMaterial::WOODFLOOR:
					AssignCoefficients(
						{ .15f, .15f, .11f, .10f, .07f, .06f, .07f, .07f },
						{ .08f, .071f, .025f, .0158f, .0056f, .0035f, .0016f, .001f },
						{ .10f, .10f, .10f, .10f, .10f, .10f, .15f, .15f });
					break;
				case EOculusAudioMaterial::WOODONCONCRETE:
					AssignCoefficients(
						{ .04f, .04f, .04f, .07f, .06f, .06f, .07f, .07f },
						{ .004f, .004f, .0079f, .0056f, .0016f, .0014f, .0005f, .0003f },
						{ .10f, .10f, .10f, .10f, .10f, .10f, .15f, .15f });
					break;
				}
			} 
			else
			{
				ResetAcousticMaterialPreset();
			}
		}
	}

	bool UOculusAudioMaterialComponent::CanEditChange(const FProperty* InProperty) const
	{
		const bool ParentVal = Super::CanEditChange(InProperty);
		FName PropertyName = InProperty->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption63Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption125Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption250Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption500Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption1000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption2000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption4000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Absorption8000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission63Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission125Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission250Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission500Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission1000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission2000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission4000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Transmission8000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering63Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering125Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering250Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering500Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering1000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering2000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering4000Hz) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UOculusAudioMaterialComponent, Scattering8000Hz))
		{
			return ParentVal  &&  IsValidMaterialPreset();
		}
		else
		{
			return ParentVal;
		}
	}

	void UOculusAudioMaterialComponent::AssignCoefficients(TArray<float> Absorption, TArray<float> Transmission, TArray<float> Scattering)
	{
		Absorption63Hz   = Absorption[0];
		Absorption125Hz  = Absorption[1];
		Absorption250Hz  = Absorption[2];
		Absorption500Hz  = Absorption[3];
		Absorption1000Hz = Absorption[4];
		Absorption2000Hz = Absorption[5];
		Absorption4000Hz = Absorption[6];
		Absorption8000Hz = Absorption[7];
		Transmission63Hz   = Transmission[0];
		Transmission125Hz  = Transmission[1];
		Transmission250Hz  = Transmission[2];
		Transmission500Hz  = Transmission[3];
		Transmission1000Hz = Transmission[4];
		Transmission2000Hz = Transmission[5];
		Transmission4000Hz = Transmission[6];
		Transmission8000Hz = Transmission[7];
		Scattering63Hz   = Scattering[0];
		Scattering125Hz  = Scattering[1];
		Scattering250Hz  = Scattering[2];
		Scattering500Hz  = Scattering[3];
		Scattering1000Hz = Scattering[4];
		Scattering2000Hz = Scattering[5];
		Scattering4000Hz = Scattering[6];
		Scattering8000Hz = Scattering[7];
	}

#endif // WITH_EDITOR