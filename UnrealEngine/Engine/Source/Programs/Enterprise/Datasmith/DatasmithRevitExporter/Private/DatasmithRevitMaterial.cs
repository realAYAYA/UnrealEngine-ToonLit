// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;

using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Visual;


namespace DatasmithRevitExporter
{
	public class FMaterialData
	{
		private Material                         CurrentMaterial   = null;
		private string                           MaterialLabel     = null;	
		public  FDatasmithFacadeMaterialInstance MaterialInstance    = null;
		public  List<FDatasmithFacadeTexture>    CollectedTextures = new List<FDatasmithFacadeTexture>();
		private IList<string>                    ExtraTexturePaths = null;
		public  IList<string>                    MessageList       = new List<string>();

		// Multi-line debug log.
		// private FDatasmithFacadeLog DebugLog = null;

		public static string GetMaterialName(
			MaterialNode InMaterialNode,
			Material     InMaterial
		)
		{
			string Name;

			if (InMaterial == null)
			{
				Color MaterialColor        = InMaterialNode.Color.IsValid ? InMaterialNode.Color : new Color(255, 255, 255);
				int   MaterialTransparency = (int)(InMaterialNode.Transparency * 100.0);
				int   MaterialSmoothness   = InMaterialNode.Smoothness;

				// Generate a unique name for the fallback material.
				Name = $"{MaterialColor.Red:x2}{MaterialColor.Green:x2}{MaterialColor.Blue:x2}{MaterialTransparency:x2}{MaterialSmoothness:x2}";
			}
			else
			{
				Name = $"{InMaterial.UniqueId}";
			}

			return FDatasmithFacadeElement.GetStringHash(Name);
		}

		// Calculate lightness from color value. 
		// Result is mapped to range 0..1.
		static private float LightnessFromColor(Color inColor)
		{
			float R = inColor.Red / 255.0f;
			float G = inColor.Green / 255.0f;
			float B = inColor.Blue / 255.0f;
			float Cmax = Math.Max(R, Math.Max(G, B));
			float Cmin = Math.Min(R, Math.Min(G, B));
			float Lightness = ((Cmax + Cmin) * 0.5f);
			return Lightness;
		}

		public FMaterialData(
			MaterialNode  InMaterialNode,
			Material      InMaterial,
			IList<string> InExtraTexturePaths
		)
		{
			CurrentMaterial   = InMaterial;
			MaterialLabel     = GetMaterialLabel(InMaterialNode, InMaterial);
			ExtraTexturePaths = InExtraTexturePaths;

			// Create a new Datasmith master material.
			// Hash the Datasmith master material name to shorten it.
			string HashedMaterialName = GetMaterialName(InMaterialNode, CurrentMaterial);
			MaterialInstance = new FDatasmithFacadeMaterialInstance(HashedMaterialName);
			MaterialInstance.SetLabel(GetMaterialLabel(InMaterialNode, CurrentMaterial));

			// Set the properties of the Datasmith master material.
			if (!SetMaterialInstance(CurrentMaterial, MaterialInstance, CollectedTextures))
			{
				SetFallbackMaterial(InMaterialNode.Color, (float) InMaterialNode.Transparency, InMaterialNode.Smoothness / 100.0F, MaterialInstance);
			}
		}

		public FMaterialData(
			string InHashedMaterialName,
			string InMaterialLabel,
			Color  InMaterialColor
		)
		{
			MaterialLabel = InMaterialLabel;

			// Create a new Datasmith master material.
			MaterialInstance = new FDatasmithFacadeMaterialInstance(InHashedMaterialName);
			MaterialInstance.SetLabel(MaterialLabel);

			// Set the properties of the Datasmith master material.
			SetFallbackMaterial(InMaterialColor, 0.0F, 0.5F, MaterialInstance);
		}

		public void Log(
			MaterialNode        InMaterialNode,
			FDatasmithFacadeLog InDebugLog,
			string              InLinePrefix
		)
		{
			if (InDebugLog != null)
			{
				int MaterialId = (CurrentMaterial == null) ? 0 : CurrentMaterial.Id.IntegerValue;

				InDebugLog.AddLine($"{InLinePrefix} {MaterialId}: '{GetMaterialLabel(InMaterialNode, CurrentMaterial)}'");
			}
		}

		private static string GetMaterialLabel(
			MaterialNode InMaterialNode,
			Material     InMaterial
		)
		{
			if (InMaterial != null)
			{
				Asset RenderingAsset = (InMaterial.Document.GetElement(InMaterial.AppearanceAssetId) as AppearanceAssetElement)?.GetRenderingAsset();

				if (RenderingAsset != null)
				{
					string RenderingAssetName = RenderingAsset.Name.Replace("Schema", "");
					if (RenderingAssetName.Contains("Prism"))
					{
						RenderingAssetName = RenderingAssetName.Replace("Prism", "Advanced");
					}
					Type RenderingAssetType = Type.GetType($"Autodesk.Revit.DB.Visual.{RenderingAssetName},RevitAPI");

					if (RenderingAssetType != null)
					{
						switch (RenderingAssetType.Name)
						{
							case "Ceramic":
							case "Concrete":
							case "Glazing":
							case "Hardwood":
							case "MasonryCMU":
							case "Metal":
							case "MetallicPaint":
							case "Mirror":
							case "PlasticVinyl":
							case "SolidGlass":
							case "Stone":
							case "WallPaint":
							case "Generic":
							// PBR Schemas
							case "AdvancedGlazing":
							case "AdvancedLayered":
							case "AdvancedMetal":
							case "AdvancedOpaque":
							case "AdvancedTransparent":
							return InMaterial.Name;
							default:
							break;
						}
					}

					return InMaterial.Name;
				}

				return InMaterial.Name;
			}

			return InMaterialNode.NodeName;
		}

		private bool SetMaterialInstance(
			Material                       InMaterial,
			FDatasmithFacadeMaterialInstance IOMaterialInstance,
			List<FDatasmithFacadeTexture>  IOCollectedTextures
		)
		{
			if (InMaterial == null)
			{
				// The properties of the Datasmith master material cannot be set.
				return false;
			}

			Asset RenderingAsset = (InMaterial.Document.GetElement(InMaterial.AppearanceAssetId) as AppearanceAssetElement)?.GetRenderingAsset();

			if (RenderingAsset == null)
			{
				// The properties of the Datasmith master material cannot be set.
				return false;
			}

			string RenderingAssetName = RenderingAsset.Name.Replace("Schema", "");
			if (RenderingAssetName.Contains("Prism"))
			{
				RenderingAssetName = RenderingAssetName.Replace("Prism", "Advanced");
			}
			Type RenderingAssetType = Type.GetType($"Autodesk.Revit.DB.Visual.{RenderingAssetName},RevitAPI");

			if (RenderingAssetType == null)
			{
				// The properties of the Datasmith master material cannot be set.
				return false;
			}

			Color sourceMaterialColor = InMaterial.Color.IsValid ? InMaterial.Color : new Color(255, 255, 255);

			// TODO: Some master material setup code below should be put in reusable methods.

			switch (RenderingAssetType.Name)
			{
				case "Ceramic":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					Color color = GetColorPropertyValue(RenderingAsset, Ceramic.CeramicColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, Ceramic.CeramicColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Ceramic.CeramicColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Ceramic.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Ceramic.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", false);

					float glossiness = 0.6F; // CeramicApplicationType.Satin
					switch ((CeramicApplicationType) GetIntegerPropertyValue(RenderingAsset, Ceramic.CeramicApplication, (int) CeramicApplicationType.Satin))
					{
						case CeramicApplicationType.HighGlossy:
							glossiness = 0.9F;
							break;
						case CeramicApplicationType.Satin:
							glossiness = 0.6F;
							break;
						case CeramicApplicationType.Matte:
							glossiness = 0.35F;
							break;
					}                        

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", glossiness);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					if ((CeramicPatternType) GetIntegerPropertyValue(RenderingAsset, Ceramic.CeramicPattern, (int) CeramicPatternType.None) == CeramicPatternType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, Ceramic.CeramicPatternMap);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, Ceramic.CeramicPatternAmount, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicPatternMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicPatternMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicPatternMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Ceramic.CeramicPatternMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Ceramic.CeramicPatternMap, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}
				}
				break;

				case "Concrete":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					Color color = GetColorPropertyValue(RenderingAsset, Concrete.ConcreteColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, Concrete.ConcreteColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Concrete.ConcreteColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Concrete.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Concrete.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", false);

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", 0.5F);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					if ((ConcreteFinishType) GetIntegerPropertyValue(RenderingAsset, Concrete.ConcreteFinish, (int) ConcreteFinishType.Straight) == ConcreteFinishType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, Concrete.ConcreteBumpMap);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount  = GetFloatPropertyValue(RenderingAsset, Concrete.ConcreteBumpAmount, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteBumpMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteBumpMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteBumpMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Concrete.ConcreteBumpMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Concrete.ConcreteBumpMap, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}
				}
				break;

				case "Glazing":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

					// TODO: Should use the Glazing.GlazingTransmittanceColor to select a predefined color value.
					Color color           = sourceMaterialColor;
					string diffuseMapPath = null;
					if ((GlazingTransmittanceColorType) GetIntegerPropertyValue(RenderingAsset, Glazing.GlazingTransmittanceColor, (int) GlazingTransmittanceColorType.Clear) == GlazingTransmittanceColorType.Custom)
					{
						color          = GetColorPropertyValue(RenderingAsset, Glazing.GlazingTransmittanceMap, sourceMaterialColor);
						diffuseMapPath = GetTexturePropertyPath(RenderingAsset, Glazing.GlazingTransmittanceMap);
					}

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Glazing.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Glazing.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					float transparency = 1.0F - GetFloatPropertyValue(RenderingAsset, Glazing.GlazingReflectance, 0.0F);

					// Control the Unreal material Opacity.
					IOMaterialInstance.AddFloat("Transparency", transparency);
					IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);

					// Control the Unreal material Normal.
					IOMaterialInstance.AddFloat("BumpAmount", 0.0F);

					// Control the Unreal material Refraction.
					IOMaterialInstance.AddFloat("RefractionIndex", 1.01F);
				}
				break;

				case "Hardwood":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", sourceMaterialColor.Red / 255.0F, sourceMaterialColor.Green / 255.0F, sourceMaterialColor.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, Hardwood.HardwoodColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Hardwood.HardwoodColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Hardwood.HardwoodColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Hardwood.HardwoodColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Hardwood.HardwoodColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Hardwood.HardwoodColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Hardwood.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Hardwood.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", false);

					float glossiness = 0.45F; // HardwoodFinishType.Satin
					switch ((HardwoodFinishType) GetIntegerPropertyValue(RenderingAsset, Hardwood.HardwoodFinish, (int) HardwoodFinishType.Satin))
					{
						case HardwoodFinishType.Gloss:
							glossiness = 0.8F;
							break;
						case HardwoodFinishType.Semigloss:
							glossiness = 0.65F;
							break;
						case HardwoodFinishType.Satin:
							glossiness = 0.45F;
							break;
						case HardwoodFinishType.Unfinished:
							glossiness = 0.05F;
							break;
					}                        

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", glossiness);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					HardwoodImperfectionsType hardwoodImperfectionsType = (HardwoodImperfectionsType) GetIntegerPropertyValue(RenderingAsset, Hardwood.HardwoodImperfections, (int) HardwoodImperfectionsType.None);
					if (hardwoodImperfectionsType != HardwoodImperfectionsType.None)
					{
						string texturePropertyName = (hardwoodImperfectionsType == HardwoodImperfectionsType.Automatic) ? Hardwood.HardwoodColor : Hardwood.HardwoodImperfectionsShader;

						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, texturePropertyName);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, Hardwood.HardwoodImperfectionsAmount, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, texturePropertyName, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, texturePropertyName, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, texturePropertyName, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, texturePropertyName, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, texturePropertyName, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}
				}
				break;

				case "MasonryCMU":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					Color color = GetColorPropertyValue(RenderingAsset, MasonryCMU.MasonryCMUColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, MasonryCMU.MasonryCMUColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, MasonryCMU.MasonryCMUColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, MasonryCMU.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, MasonryCMU.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", false);

					float glossiness = 0.25F; // MasonryCMUApplicationType.Matte
					switch ((MasonryCMUApplicationType) GetIntegerPropertyValue(RenderingAsset, MasonryCMU.MasonryCMUApplication, (int) MasonryCMUApplicationType.Matte))
					{
						case MasonryCMUApplicationType.Glossy:
							glossiness = 0.9F;
							break;
						case MasonryCMUApplicationType.Matte:
						case MasonryCMUApplicationType.Unfinished:
							glossiness = 0.25F;
							break;
					}                        

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", glossiness);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					if ((MasonryCMUPatternType) GetIntegerPropertyValue(RenderingAsset, MasonryCMU.MasonryCMUPattern, (int) MasonryCMUPatternType.None) == MasonryCMUPatternType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, MasonryCMU.MasonryCMUPatternMap);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, MasonryCMU.MasonryCMUPatternHeight, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUPatternMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUPatternMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUPatternMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, MasonryCMU.MasonryCMUPatternMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, MasonryCMU.MasonryCMUPatternMap, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}
				}
				break;

				case "Metal":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					// TODO: Should use the Metal.MetalColor to select a predefined color value.

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", sourceMaterialColor.Red / 255.0F, sourceMaterialColor.Green / 255.0F, sourceMaterialColor.Blue / 255.0F, 1.0F);
					IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Metal.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Metal.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", true);

					float glossiness = 0.5F; // MetalFinishType.SemiPolished
					switch ((MetalFinishType) GetIntegerPropertyValue(RenderingAsset, Metal.MetalFinish, (int) MetalFinishType.SemiPolished))
					{
						case MetalFinishType.Polished:
							glossiness = 1.0F;
							break;
						case MetalFinishType.SemiPolished:
							glossiness = 0.5F;
							break;
						case MetalFinishType.Satin:
						case MetalFinishType.Brushed:
							glossiness = 0.25F;
							break;
					}

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", glossiness);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					if ((MetalPerforationsType) GetIntegerPropertyValue(RenderingAsset, Metal.MetalPerforations, (int) MetalPerforationsType.None) == MetalPerforationsType.Custom)
					{
						string cutoutMapPath = GetTexturePropertyPath(RenderingAsset, Metal.MetalPerforationsShader);

						if (!string.IsNullOrEmpty(cutoutMapPath))
						{
							float cutoutMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Metal.MetalPerforationsShader, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float cutoutMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Metal.MetalPerforationsShader, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float cutoutMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Metal.MetalPerforationsShader, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float cutoutMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Metal.MetalPerforationsShader, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float cutoutMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Metal.MetalPerforationsShader, UnifiedBitmap.TextureWAngle);

							IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.CutOut);

							// Control the Unreal material Opacity Mask.
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(cutoutMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("CutoutOpacityMap", TextureElement);
							IOMaterialInstance.AddFloat("CutoutMap_UVOffsetX", cutoutMapUVOffsetX);
							IOMaterialInstance.AddFloat("CutoutMap_UVOffsetY", cutoutMapUVOffsetY);
							IOMaterialInstance.AddFloat("CutoutMap_UVScaleX",  cutoutMapUVScaleX);
							IOMaterialInstance.AddFloat("CutoutMap_UVScaleY",  cutoutMapUVScaleY);
							IOMaterialInstance.AddFloat("CutoutMap_UVWAngle",  cutoutMapUVWAngle);
						}
					}

					if ((MetalPatternType) GetIntegerPropertyValue(RenderingAsset, Metal.MetalPattern, (int) MetalPatternType.None) == MetalPatternType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, Metal.MetalPatternShader);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, Metal.MetalPatternHeight, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Metal.MetalPatternShader, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Metal.MetalPatternShader, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Metal.MetalPatternShader, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Metal.MetalPatternShader, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Metal.MetalPatternShader, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}
				}
				break;

				case "MetallicPaint":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					Color color = GetColorPropertyValue(RenderingAsset, MetallicPaint.MetallicpaintBaseColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, MetallicPaint.MetallicpaintBaseColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, MetallicPaint.MetallicpaintBaseColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, MetallicPaint.MetallicpaintBaseColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, MetallicPaint.MetallicpaintBaseColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, MetallicPaint.MetallicpaintBaseColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, MetallicPaint.MetallicpaintBaseColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, MetallicPaint.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, MetallicPaint.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", true);

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", 0.9F);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					// Control the Unreal material Normal.
					IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
				}
				break;

				case "Mirror":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					Color color = GetColorPropertyValue(RenderingAsset, Mirror.MirrorTintcolor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);
					IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Mirror.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Mirror.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", true);

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", 1.0F);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					// Control the Unreal material Normal.
					IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
				}
				break;

				case "PlasticVinyl":
				{
					Color color = GetColorPropertyValue(RenderingAsset, PlasticVinyl.PlasticvinylColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, PlasticVinyl.PlasticvinylColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, PlasticVinyl.PlasticvinylColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, PlasticVinyl.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, PlasticVinyl.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					if ((PlasticvinylBumpType) GetIntegerPropertyValue(RenderingAsset, PlasticVinyl.PlasticvinylBump, (int) PlasticvinylBumpType.None) == PlasticvinylBumpType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, PlasticVinyl.PlasticvinylBumpMap);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, PlasticVinyl.PlasticvinylBumpAmount, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylBumpMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylBumpMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylBumpMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, PlasticVinyl.PlasticvinylBumpMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, PlasticVinyl.PlasticvinylBumpMap, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}

					if ((PlasticvinylType) GetIntegerPropertyValue(RenderingAsset, PlasticVinyl.PlasticvinylType, (int) PlasticvinylType.Plasticsolid) == PlasticvinylType.Plastictransparent)
					{
						IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

						// Control the Unreal material Opacity.
						IOMaterialInstance.AddFloat("Transparency", 0.5F);
						IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);

						// Control the Unreal material Refraction.
						IOMaterialInstance.AddFloat("RefractionIndex", 1.0F);
					}
					else
					{
						IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

						// Control the Unreal material Metallic.
						IOMaterialInstance.AddBoolean("IsMetal", false);

						float glossiness = 0.9F; // PlasticvinylApplicationType.Polished
						switch ((PlasticvinylApplicationType) GetIntegerPropertyValue(RenderingAsset, PlasticVinyl.PlasticvinylApplication, (int) PlasticvinylApplicationType.Polished))
						{
							case PlasticvinylApplicationType.Glossy:
								glossiness = 1.0F;
								break;
							case PlasticvinylApplicationType.Polished:
								glossiness = 0.9F;
								break;
							case PlasticvinylApplicationType.Matte:
								glossiness = 0.75F;
								break;
						}                        

						// Control the Unreal material Roughness.
						IOMaterialInstance.AddFloat("Glossiness", glossiness);
					}
				}
				break;

				case "SolidGlass":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

					// TODO: Should use the SolidGlass.SolidglassTransmittance to select a predefined color value.
					Color color           = sourceMaterialColor;
					string diffuseMapPath = null;
					if ((SolidglassTransmittanceType) GetIntegerPropertyValue(RenderingAsset, SolidGlass.SolidglassTransmittance, (int) SolidglassTransmittanceType.Clear) == SolidglassTransmittanceType.CustomColor)
					{
						color          = GetColorPropertyValue(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, sourceMaterialColor);
						diffuseMapPath = GetTexturePropertyPath(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor);
					}

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, SolidGlass.SolidglassTransmittanceCustomColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, SolidGlass.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, SolidGlass.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					float transparency = 1.0F - GetFloatPropertyValue(RenderingAsset, SolidGlass.SolidglassReflectance, 0.0F);

					// Control the Unreal material Opacity.
					IOMaterialInstance.AddFloat("Transparency", transparency);
					IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);

					if ((SolidglassBumpEnableType) GetIntegerPropertyValue(RenderingAsset, SolidGlass.SolidglassBumpEnable, (int) SolidglassBumpEnableType.None) == SolidglassBumpEnableType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, SolidGlass.SolidglassBumpMap);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, SolidGlass.SolidglassBumpAmount, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassBumpMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassBumpMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassBumpMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, SolidGlass.SolidglassBumpMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, SolidGlass.SolidglassBumpMap, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}

					// Control the Unreal material Refraction.
					IOMaterialInstance.AddFloat("RefractionIndex", 1.0F);
				}
				break;
				
				case "Stone":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", sourceMaterialColor.Red / 255.0F, sourceMaterialColor.Green / 255.0F, sourceMaterialColor.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, Stone.StoneColor);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Stone.StoneColor, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Stone.StoneColor, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Stone.StoneColor, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Stone.StoneColor, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Stone.StoneColor, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Stone.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Stone.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", false);

					float glossiness = 0.35F; // StoneApplicationType.Matte
					switch ((StoneApplicationType) GetIntegerPropertyValue(RenderingAsset, Stone.StoneApplication, (int) StoneApplicationType.Matte))
					{
						case StoneApplicationType.Polished:
							glossiness = 1.0F;
							break;
						case StoneApplicationType.Glossy:
							glossiness = 0.8F;
							break;
						case StoneApplicationType.Matte:
							glossiness = 0.35F;
							break;
						case StoneApplicationType.Unfinished:
							glossiness = 0.25F;
							break;
					}                        

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", glossiness);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					if ((StonePatternType) GetIntegerPropertyValue(RenderingAsset, Stone.StonePattern, (int) StonePatternType.None) == StonePatternType.Custom)
					{
						string bumpMapPath = GetTexturePropertyPath(RenderingAsset, Stone.StonePatternMap);

						if (!string.IsNullOrEmpty(bumpMapPath))
						{
							float bumpAmount       = GetFloatPropertyValue(RenderingAsset, Stone.StonePatternAmount, 0.0F);
							float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Stone.StonePatternMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Stone.StonePatternMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Stone.StonePatternMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Stone.StonePatternMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Stone.StonePatternMap, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Normal.
							IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("BumpMap", TextureElement);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
							IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
							IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
							IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
						}
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}
				}
				break;

				case "WallPaint":
				{
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					Color color = GetColorPropertyValue(RenderingAsset, WallPaint.WallpaintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);
					IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, WallPaint.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, WallPaint.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					// Control the Unreal material Metallic.
					IOMaterialInstance.AddBoolean("IsMetal", false);

					float glossiness = 0.6F; // WallpaintFinishType.Pearl
					switch ((WallpaintFinishType) GetIntegerPropertyValue(RenderingAsset, WallPaint.WallpaintFinish, (int) WallpaintFinishType.Pearl))
					{
						case WallpaintFinishType.Gloss:
							glossiness = 0.75F;
							break;
						case WallpaintFinishType.Semigloss:
							glossiness = 0.7F;
							break;
						case WallpaintFinishType.Pearl:
							glossiness = 0.6F;
							break;
						case WallpaintFinishType.Platinum:
							glossiness = 0.55F;
							break;
						case WallpaintFinishType.Eggshell:
							glossiness = 0.5F;
							break;
						case WallpaintFinishType.Flat:
							glossiness = 0.4F;
							break;
					}                        

					// Control the Unreal material Roughness.
					IOMaterialInstance.AddFloat("Glossiness", glossiness);

					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

					// Control the Unreal material Normal.
					IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
				}
				break;

				case "Generic":
				{
					Color color = GetColorPropertyValue(RenderingAsset, Generic.GenericDiffuse, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddColor("DiffuseColor", color.Red / 255.0F, color.Green / 255.0F, color.Blue / 255.0F, 1.0F);

					string diffuseMapPath = GetTexturePropertyPath(RenderingAsset, Generic.GenericDiffuse);

					if (!string.IsNullOrEmpty(diffuseMapPath))
					{
						float diffuseImageFade    = GetFloatPropertyValue(RenderingAsset, Generic.GenericDiffuseImageFade, 0.0F);
						float diffuseMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Generic.GenericDiffuse, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float diffuseMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Generic.GenericDiffuse, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float diffuseMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericDiffuse, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float diffuseMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericDiffuse, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float diffuseMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Generic.GenericDiffuse, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Base Color.
						IOMaterialInstance.AddFloat("DiffuseMapFading", diffuseImageFade);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(diffuseMapPath);
						TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("DiffuseMap", TextureElement);                    
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetX", diffuseMapUVOffsetX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVOffsetY", diffuseMapUVOffsetY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleX",  diffuseMapUVScaleX);
						IOMaterialInstance.AddFloat("DiffuseMap_UVScaleY",  diffuseMapUVScaleY);
						IOMaterialInstance.AddFloat("DiffuseMap_UVWAngle",  diffuseMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);                          
					}

					bool  tintEnabled = GetBooleanPropertyValue(RenderingAsset, Generic.CommonTintToggle, false);
					Color tintColor   = GetColorPropertyValue(RenderingAsset, Generic.CommonTintColor, sourceMaterialColor);

					// Control the Unreal material Base Color.
					IOMaterialInstance.AddBoolean("TintEnabled", tintEnabled);
					IOMaterialInstance.AddColor("TintColor", tintColor.Red / 255.0F, tintColor.Green / 255.0F, tintColor.Blue / 255.0F, 1.0F);

					float selfIlluminationLuminance			= GetFloatPropertyValue(RenderingAsset, Generic.GenericSelfIllumLuminance, 0.0F);
					float selfIlluminationColorTemperature	= GetFloatPropertyValue(RenderingAsset, Generic.GenericSelfIllumColorTemperature, 0.0f);
					Color selfIlluminationFilterColor		= GetColorPropertyValue(RenderingAsset, Generic.GenericSelfIllumFilterMap, new Color(255, 255, 255));
				
					// Control the Unreal material Emissive Color.
					IOMaterialInstance.AddFloat("SelfIlluminationLuminance", selfIlluminationLuminance);
					IOMaterialInstance.AddFloat("SelfIlluminationColorTemperature", selfIlluminationColorTemperature);
					IOMaterialInstance.AddColor("SelfIlluminationFilter", selfIlluminationFilterColor.Red / 255.0F, selfIlluminationFilterColor.Green / 255.0F, selfIlluminationFilterColor.Blue / 255.0F, 1.0F);

					string selfIlluminationMapPath = GetTexturePropertyPath(RenderingAsset, Generic.GenericSelfIllumFilterMap);

					if (!string.IsNullOrEmpty(selfIlluminationMapPath))
					{
						float selfIlluminationMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Generic.GenericSelfIllumFilterMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float selfIlluminationMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Generic.GenericSelfIllumFilterMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float selfIlluminationMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericSelfIllumFilterMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float selfIlluminationMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericSelfIllumFilterMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float selfIlluminationMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Generic.GenericSelfIllumFilterMap, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Emissive Color.
						IOMaterialInstance.AddBoolean("SelfIlluminationMapEnable", true);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(selfIlluminationMapPath);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("SelfIlluminationMap", TextureElement);
						IOMaterialInstance.AddFloat("SelfIlluminationMap_UVOffsetX", selfIlluminationMapUVOffsetX);
						IOMaterialInstance.AddFloat("SelfIlluminationMap_UVOffsetY", selfIlluminationMapUVOffsetY);
						IOMaterialInstance.AddFloat("SelfIlluminationMap_UVScaleX",  selfIlluminationMapUVScaleX);
						IOMaterialInstance.AddFloat("SelfIlluminationMap_UVScaleY",  selfIlluminationMapUVScaleY);
						IOMaterialInstance.AddFloat("SelfIlluminationMap_UVWAngle",  selfIlluminationMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddBoolean("SelfIlluminationMapEnable", false);
					}

					string bumpMapPath = GetTexturePropertyPath(RenderingAsset, Generic.GenericBumpMap);

					if (!string.IsNullOrEmpty(bumpMapPath))
					{
						float bumpAmount       = GetFloatPropertyValue(RenderingAsset, Generic.GenericBumpAmount, 0.0F);
						float bumpMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Generic.GenericBumpMap, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
						float bumpMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Generic.GenericBumpMap, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
						float bumpMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericBumpMap, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
						float bumpMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericBumpMap, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
						float bumpMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Generic.GenericBumpMap, UnifiedBitmap.TextureWAngle);

						// Control the Unreal material Normal.
						IOMaterialInstance.AddFloat("BumpAmount", bumpAmount);
						FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(bumpMapPath);
						TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
						IOCollectedTextures.Add(TextureElement);
						IOMaterialInstance.AddTexture("BumpMap", TextureElement);
						IOMaterialInstance.AddFloat("BumpMap_UVOffsetX", bumpMapUVOffsetX);
						IOMaterialInstance.AddFloat("BumpMap_UVOffsetY", bumpMapUVOffsetY);
						IOMaterialInstance.AddFloat("BumpMap_UVScaleX",  bumpMapUVScaleX);
						IOMaterialInstance.AddFloat("BumpMap_UVScaleY",  bumpMapUVScaleY);
						IOMaterialInstance.AddFloat("BumpMap_UVWAngle",  bumpMapUVWAngle);
					}
					else
					{
						IOMaterialInstance.AddFloat("BumpAmount", 0.0F);
					}

					float transparency = GetFloatPropertyValue(RenderingAsset, Generic.GenericTransparency, 0.0F);

					if (transparency > 0.0F)
					{
						IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

						// Control the Unreal material Opacity.
						IOMaterialInstance.AddFloat("Transparency", transparency);

						string transparencyMapPath = GetTexturePropertyPath(RenderingAsset, Generic.GenericTransparency);

						if (!string.IsNullOrEmpty(transparencyMapPath))
						{
							float transparencyImageFade    = GetFloatPropertyValue(RenderingAsset, Generic.GenericTransparencyImageFade, 0.0F);
							float transparencyMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Generic.GenericTransparency, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float transparencyMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Generic.GenericTransparency, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float transparencyMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericTransparency, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float transparencyMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericTransparency, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float transparencyMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Generic.GenericTransparency, UnifiedBitmap.TextureWAngle);

							// Control the Unreal material Opacity.
							IOMaterialInstance.AddFloat("TransparencyMapFading", transparencyImageFade);
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(transparencyMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("TransparencyMap", TextureElement);
							IOMaterialInstance.AddFloat("TransparencyMap_UVOffsetX", transparencyMapUVOffsetX);
							IOMaterialInstance.AddFloat("TransparencyMap_UVOffsetY", transparencyMapUVOffsetY);
							IOMaterialInstance.AddFloat("TransparencyMap_UVScaleX",  transparencyMapUVScaleX);
							IOMaterialInstance.AddFloat("TransparencyMap_UVScaleY",  transparencyMapUVScaleY);
							IOMaterialInstance.AddFloat("TransparencyMap_UVWAngle",  transparencyMapUVWAngle);
						}
						else
						{
							IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);
						}

						float refractionIndex = GetFloatPropertyValue(RenderingAsset, Generic.GenericRefractionIndex, 1.0F);

						// Control the Unreal material Refraction.
						IOMaterialInstance.AddFloat("RefractionIndex", refractionIndex);
					}
					else
					{
						IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

						bool isMetal = GetBooleanPropertyValue(RenderingAsset, Generic.GenericIsMetal, false);

						// Control the Unreal material Metallic.
						IOMaterialInstance.AddBoolean("IsMetal", isMetal);

						float glossiness = GetFloatPropertyValue(RenderingAsset, Generic.GenericGlossiness, InMaterial.Smoothness / 100.0F);

						// Control the Unreal material Roughness.
						IOMaterialInstance.AddFloat("Glossiness", glossiness);

						string cutoutMapPath = GetTexturePropertyPath(RenderingAsset, Generic.GenericCutoutOpacity);

						if (!string.IsNullOrEmpty(cutoutMapPath))
						{
							float cutoutMapUVOffsetX = GetTexturePropertyDistance(RenderingAsset, Generic.GenericCutoutOpacity, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
							float cutoutMapUVOffsetY = GetTexturePropertyDistance(RenderingAsset, Generic.GenericCutoutOpacity, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
							float cutoutMapUVScaleX  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericCutoutOpacity, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
							float cutoutMapUVScaleY  = 1.0F / GetTexturePropertyDistance(RenderingAsset, Generic.GenericCutoutOpacity, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
							float cutoutMapUVWAngle  = GetTexturePropertyAngle(RenderingAsset, Generic.GenericCutoutOpacity, UnifiedBitmap.TextureWAngle);

							IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.CutOut);

							// Control the Unreal material Opacity Mask.
							FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(cutoutMapPath);
							TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
							IOCollectedTextures.Add(TextureElement);
							IOMaterialInstance.AddTexture("CutoutOpacityMap", TextureElement);
							IOMaterialInstance.AddFloat("CutoutMap_UVOffsetX", cutoutMapUVOffsetX);
							IOMaterialInstance.AddFloat("CutoutMap_UVOffsetY", cutoutMapUVOffsetY);
							IOMaterialInstance.AddFloat("CutoutMap_UVScaleX",  cutoutMapUVScaleX);
							IOMaterialInstance.AddFloat("CutoutMap_UVScaleY",  cutoutMapUVScaleY);
							IOMaterialInstance.AddFloat("CutoutMap_UVWAngle",  cutoutMapUVWAngle);
						}
					}
				}
				break;

				// PBR materials

#if REVIT_API_2020
				case "AdvancedGlazing":
				{
					IOMaterialInstance.AddBoolean("IsPBR", true);
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

					Color TranspColor = GetColorPropertyValue(RenderingAsset, AdvancedGlazing.GlazingTransmissionColor, sourceMaterialColor);
					float Transparency = LightnessFromColor(TranspColor);
					IOMaterialInstance.AddFloat("Transparency", Transparency);
					IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);

					IOMaterialInstance.AddBoolean("TintEnabled", true);
					IOMaterialInstance.AddColor("TintColor", TranspColor.Red / 255.0F, TranspColor.Green / 255.0F, TranspColor.Blue / 255.0F, 1.0F);

					ExportRougness(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedGlazing.GlazingTransmissionRoughness);
					ExportCutout(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedGlazing.SurfaceCutout);
					ExportNormalMap(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedGlazing.SurfaceNormal);
				}
				break;
#endif

				case "AdvancedLayered":
				{
					IOMaterialInstance.AddBoolean("IsPBR", true);
					ExportDiffuse(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedLayered.LayeredBottomF0, sourceMaterialColor);
					ExportRougness(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedLayered.LayeredRoughness);
					ExportCutout(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedLayered.SurfaceCutout);
					ExportNormalMap(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedLayered.LayeredNormal);
				}
				break;

				case "AdvancedTransparent":
				{
					IOMaterialInstance.AddBoolean("IsPBR", true);
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

					Color TranspColor = GetColorPropertyValue(RenderingAsset, AdvancedTransparent.TransparentColor, sourceMaterialColor);

					IOMaterialInstance.AddBoolean("TintEnabled", true);
					IOMaterialInstance.AddColor("TintColor", TranspColor.Red / 255.0F, TranspColor.Green / 255.0F, TranspColor.Blue / 255.0F, 1.0F);

					float Transparency = LightnessFromColor(TranspColor);
					IOMaterialInstance.AddFloat("Transparency", Transparency);
					IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);

					// Control the Unreal material Refraction.
					float RefractionIndex = GetFloatPropertyValue(RenderingAsset, AdvancedTransparent.TransparentIor, 1.0f);
					IOMaterialInstance.AddFloat("RefractionIndex", RefractionIndex);

					ExportRougness(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedTransparent.SurfaceRoughness);
					ExportCutout(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedTransparent.SurfaceCutout);
					ExportNormalMap(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedTransparent.SurfaceNormal);
				}
				break;

				case "AdvancedMetal":
				{
					IOMaterialInstance.AddBoolean("IsPBR", true);
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);
					IOMaterialInstance.AddBoolean("IsMetal", true);
					ExportDiffuse(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedMetal.MetalF0, sourceMaterialColor);
					ExportRougness(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedMetal.SurfaceRoughness);
					ExportCutout(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedMetal.SurfaceCutout);
					ExportNormalMap(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedMetal.SurfaceNormal);
				}
				break;

				case "AdvancedOpaque":
				{
					IOMaterialInstance.AddBoolean("IsPBR", true);
					IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

					ExportDiffuse(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedOpaque.OpaqueAlbedo, sourceMaterialColor);
					ExportCutout(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedOpaque.SurfaceCutout);
					ExportRougness(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedOpaque.SurfaceRoughness);
					ExportNormalMap(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedOpaque.SurfaceNormal);

					bool emission = GetBooleanPropertyValue(RenderingAsset, AdvancedOpaque.OpaqueEmission, false);
					if (emission)
					{
						ExportEmission(IOMaterialInstance, IOCollectedTextures, RenderingAsset, AdvancedOpaque.OpaqueLuminance, AdvancedOpaque.OpaqueLuminanceModifier);
					}
					else
					{
						IOMaterialInstance.AddBoolean("SelfIlluminationMapEnable", false);
					}
				}
				break;

				default:
				{
					// The properties of the Datasmith master material cannot be set.
					return false;
				}
			}

			// The properties of the Datasmith master material are set.
			return true;
		}

		private FDatasmithFacadeTexture ExportTexture(
			FDatasmithFacadeMaterialInstance IOMaterialInstance, Asset RenderingAsset, string AssetPropertyName, 
			string MapParamName, string TextureParamPrefix)
		{
			string MapPath = GetTexturePropertyPath(RenderingAsset, AssetPropertyName);

			if (!string.IsNullOrEmpty(MapPath))
			{
				float UVOffsetX = GetTexturePropertyDistance(RenderingAsset, AssetPropertyName, UnifiedBitmap.TextureRealWorldOffsetX, 0.0F);
				float UVOffsetY = GetTexturePropertyDistance(RenderingAsset, AssetPropertyName, UnifiedBitmap.TextureRealWorldOffsetY, 0.0F);
				float UVScaleX = 1.0F / GetTexturePropertyDistance(RenderingAsset, AssetPropertyName, UnifiedBitmap.TextureRealWorldScaleX, 1.0F);
				float UVScaleY = 1.0F / GetTexturePropertyDistance(RenderingAsset, AssetPropertyName, UnifiedBitmap.TextureRealWorldScaleY, 1.0F);
				float UVWAngle = GetTexturePropertyAngle(RenderingAsset, AssetPropertyName, UnifiedBitmap.TextureWAngle);

				// Control the Unreal material Normal.
				FDatasmithFacadeTexture TextureElement = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(MapPath);
				IOMaterialInstance.AddTexture(MapParamName, TextureElement);
				IOMaterialInstance.AddFloat($"{TextureParamPrefix}_UVOffsetX", UVOffsetX);
				IOMaterialInstance.AddFloat($"{TextureParamPrefix}_UVOffsetY", UVOffsetY);
				IOMaterialInstance.AddFloat($"{TextureParamPrefix}_UVScaleX", UVScaleX);
				IOMaterialInstance.AddFloat($"{TextureParamPrefix}_UVScaleY", UVScaleY);
				IOMaterialInstance.AddFloat($"{TextureParamPrefix}_UVWAngle", UVWAngle);

				return TextureElement;
			}

			return null;
		}

		private void ExportNormalMap(FDatasmithFacadeMaterialInstance IOMaterialInstance, List<FDatasmithFacadeTexture> IOCollectedTextures, Asset RenderingAsset, string AssetPropertyName)
		{
			FDatasmithFacadeTexture TextureElement = ExportTexture(IOMaterialInstance, RenderingAsset, AssetPropertyName, "NormalMap", "NormalMap");
			if (TextureElement != null)
			{
				IOCollectedTextures.Add(TextureElement);
				TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Normal);
			}
		}

		private void ExportDiffuse(FDatasmithFacadeMaterialInstance IOMaterialInstance, List<FDatasmithFacadeTexture> IOCollectedTextures, Asset RenderingAsset, string AssetProperty, Color DefaultColor)
		{
			Color DiffuseColor = GetColorPropertyValue(RenderingAsset, AssetProperty, DefaultColor);

			// Control the Unreal material Base Color.
			IOMaterialInstance.AddColor("DiffuseColor", DiffuseColor.Red / 255.0F, DiffuseColor.Green / 255.0F, DiffuseColor.Blue / 255.0F, 1.0F);

			FDatasmithFacadeTexture TextureElement = ExportTexture(IOMaterialInstance, RenderingAsset, AssetProperty, "DiffuseMap", "DiffuseMap");
			if (TextureElement != null)
			{
				IOCollectedTextures.Add(TextureElement);
				TextureElement.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
				TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
				IOMaterialInstance.AddFloat("DiffuseMapFading", 1.0F);
			}
			else
			{
				IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);
			}
		}

		private void ExportEmission(FDatasmithFacadeMaterialInstance IOMaterialInstance, List<FDatasmithFacadeTexture> IOCollectedTextures, Asset RenderingAsset, string LuminanceParam, string ColorParam)
		{
			float EmissionLuminance = GetFloatPropertyValue(RenderingAsset, LuminanceParam, 1.0f);
			Color EmissionFilterColor = GetColorPropertyValue(RenderingAsset, ColorParam, new Color(255, 255, 255));

			// Control the Unreal material Emissive Color.
			IOMaterialInstance.AddFloat("SelfIlluminationLuminance", EmissionLuminance);
			IOMaterialInstance.AddColor("SelfIlluminationFilter", EmissionFilterColor.Red / 255.0F, EmissionFilterColor.Green / 255.0F, EmissionFilterColor.Blue / 255.0F, 1.0F);

			FDatasmithFacadeTexture TextureElement = ExportTexture(IOMaterialInstance, RenderingAsset, ColorParam, "SelfIlluminationMap", "SelfIlluminationMap");
			if (TextureElement != null)
			{
				IOCollectedTextures.Add(TextureElement);
				TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
				IOMaterialInstance.AddBoolean("SelfIlluminationMapEnable", true);
			}
			else
			{
				IOMaterialInstance.AddBoolean("SelfIlluminationMapEnable", false);
			}
		}

		private void ExportRougness(FDatasmithFacadeMaterialInstance IOMaterialInstance, List<FDatasmithFacadeTexture> IOCollectedTextures, Asset RenderingAsset, string AssetProperty)
		{
			FDatasmithFacadeTexture TextureElement = ExportTexture(IOMaterialInstance, RenderingAsset, AssetProperty, "RoughnessMap", "RoughnessMap");
			if (TextureElement != null)
			{
				IOCollectedTextures.Add(TextureElement);
				TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
				IOMaterialInstance.AddBoolean("RoughnessMapEnable", true);
			}
			else
			{
				IOMaterialInstance.AddBoolean("RoughnessMapEnable", false);
				float Rougness = GetFloatPropertyValue(RenderingAsset, AssetProperty, 0f);
				IOMaterialInstance.AddFloat("Rougness", Rougness);
			}
		}

		private void ExportCutout(FDatasmithFacadeMaterialInstance IOMaterialInstance, List<FDatasmithFacadeTexture> IOCollectedTextures, Asset RenderingAsset, string AssetProperty)
		{
			FDatasmithFacadeTexture TextureElement = ExportTexture(IOMaterialInstance, RenderingAsset, AssetProperty, "CutoutOpacityMap", "CutoutMap");
			if (TextureElement != null)
			{
				IOCollectedTextures.Add(TextureElement);
				TextureElement.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
				IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.CutOut);
			}
		}

		private void SetFallbackMaterial(
			Color                          InMaterialColor,
			float                          InMaterialTransparency, // in range 0.0-1.0
			float                          InMaterialGlossiness,   // in range 0.0-1.0
			FDatasmithFacadeMaterialInstance IOMaterialInstance
		)
		{
			// Control the Unreal material Base Color.
			Color MaterialColor = InMaterialColor.IsValid ? InMaterialColor : new Color(255, 255, 255);
			IOMaterialInstance.AddColor("DiffuseColor", MaterialColor.Red / 255.0F, MaterialColor.Green / 255.0F, MaterialColor.Blue / 255.0F, 1.0F);
			IOMaterialInstance.AddFloat("DiffuseMapFading", 0.0F);
			IOMaterialInstance.AddBoolean("TintEnabled", false);

			// Control the Unreal material Emissive Color.
			IOMaterialInstance.AddFloat("SelfIlluminationLuminance", 0.0F);

			// Control the Unreal material Normal.
			IOMaterialInstance.AddFloat("BumpAmount", 0.0F);

			if (InMaterialTransparency > 0.0F)
			{
				IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);

				// Control the Unreal material Opacity.
				IOMaterialInstance.AddFloat("Transparency", InMaterialTransparency);
				IOMaterialInstance.AddFloat("TransparencyMapFading", 0.0F);

				// Control the Unreal material Refraction.
				IOMaterialInstance.AddFloat("RefractionIndex", 1.0F);
			}
			else
			{
				IOMaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);

				// Control the Unreal material Metallic.
				IOMaterialInstance.AddBoolean("IsMetal", false);

				// Control the Unreal material Roughness.
				IOMaterialInstance.AddFloat("Glossiness", InMaterialGlossiness);
			}
		}

		private bool GetBooleanPropertyValue(
			Asset  in_asset,
			string in_propertyName,
			bool   in_defaultValue
		)
        {
			// DebugLog.AddLine($"Boolean Property {in_propertyName}");

			AssetProperty booleanProperty = in_asset.FindByName(in_propertyName);

			return (booleanProperty != null) ? (booleanProperty as AssetPropertyBoolean).Value : in_defaultValue;
        }

		private int GetIntegerPropertyValue(
			Asset  in_asset,
			string in_propertyName,
			int    in_defaultValue
		)
        {
			// DebugLog.AddLine($"Integer Property {in_propertyName}");

			AssetProperty integerProperty = in_asset.FindByName(in_propertyName);

			if (integerProperty != null)
			{
				if (integerProperty.Type == AssetPropertyType.Enumeration)
				{
					// Handle the spurious case of an enumerated type value not stored in an integer asset property.
					return (integerProperty as AssetPropertyEnum).Value;
				}
				else
				{
					return (integerProperty as AssetPropertyInteger).Value;
				}
			}

			return in_defaultValue;
        }

		private float GetFloatPropertyValue(
			Asset  in_asset,
			string in_propertyName,
			float  in_defaultValue
		)
        {
			// DebugLog.AddLine($"Float Property {in_propertyName}");

			AssetProperty doubleProperty = in_asset.FindByName(in_propertyName);

			if (doubleProperty != null)
			{
				if (doubleProperty.Type == AssetPropertyType.Float)
				{
					// Handle the spurious case of a value not stored in a double asset property.
					return (doubleProperty as AssetPropertyFloat).Value;
				}
				else
				{
					return (float) (doubleProperty as AssetPropertyDouble).Value;
				}
			}

			return in_defaultValue;
        }

		private Color GetColorPropertyValue(
			Asset  in_asset,
			string in_propertyName,
			Color  in_defaultValue
		)
        {
			// DebugLog.AddLine($"Color Property {in_propertyName}");

			AssetProperty colorProperty = in_asset.FindByName(in_propertyName);

			if (colorProperty != null)
			{
				Color color = (colorProperty as AssetPropertyDoubleArray4d).GetValueAsColor();

				if (color.IsValid)
				{
					return color;
				}
			}

			return in_defaultValue;
		}

		private string GetTexturePropertyPath(
			Asset  in_asset,
			string in_propertyName
		)
        {
			AssetProperty textureProperty = in_asset.FindByName(in_propertyName);

			if (textureProperty != null)
			{
				Asset unifiedBitmapAsset = null;

				try
				{
					unifiedBitmapAsset = textureProperty.GetSingleConnectedAsset();
				}
				catch {}

				if (unifiedBitmapAsset != null)
				{
					AssetProperty sourceProperty = unifiedBitmapAsset.FindByName(UnifiedBitmap.UnifiedbitmapBitmap);

					if (sourceProperty != null)
					{
						string sourcePath = (sourceProperty as AssetPropertyString).Value;

						if (!string.IsNullOrEmpty(sourcePath))
						{
							if (sourcePath.Contains("|"))
							{
								sourcePath = sourcePath.Split('|')[0];
							}

							if (!string.IsNullOrEmpty(sourcePath))
							{
								// TODO: Better handle relative paths.
								const string RevitTextureFolder1 = "C:\\Program Files (x86)\\Common Files\\Autodesk Shared\\Materials\\Textures\\";
								const string RevitTextureFolder2 = "C:\\Program Files (x86)\\Common Files\\Autodesk Shared\\Materials\\Textures\\1\\Mats\\";

								if (sourcePath[0] == '1')
								{
									sourcePath =  $"{RevitTextureFolder1}{sourcePath}";
								}

								if (sourcePath.Contains("Materials\\Generic\\Presets\\"))
								{
									sourcePath = sourcePath.Replace("Materials\\Generic\\Presets\\", RevitTextureFolder2).Replace(".jpg", ".png");
								}

								if (!Path.IsPathRooted(sourcePath))
								{
									string rootedSourcePath = Path.Combine(RevitTextureFolder2, sourcePath);

									if (File.Exists(rootedSourcePath))
									{
										sourcePath = rootedSourcePath;
									}
								}
								else if (!File.Exists(sourcePath))
								{
									// Path is absolute but file is not found there.
									// Remove the path component, we'll try searching the extra texture paths.
									sourcePath = Path.GetFileName(sourcePath);
								}

								// Also search a relative path in the extra texture paths.
								if (!Path.IsPathRooted(sourcePath))
								{
									foreach (string extraTexturePath in ExtraTexturePaths)
									{
										string extraSourcePath = Path.Combine(extraTexturePath, sourcePath);

										if (Path.IsPathRooted(extraSourcePath) && File.Exists(extraSourcePath))
										{
											sourcePath = extraSourcePath;
											break;
										}
									}
								}
								
								if (!File.Exists(sourcePath))
								{
									MessageList.Add($"Warning - Material \"{MaterialLabel}\": Cannot find texture file {sourcePath}");
								}

								return sourcePath;
							}
						}
					}
				}
			}

            return "";
        }

		private float GetTexturePropertyDistance(
			Asset  in_asset,
			string in_propertyName,
			string in_distanceName,
			float  in_defaultValue
		)
        {
			// DebugLog.AddLine($"Texture Distance Property {in_propertyName}");

			AssetProperty textureProperty = in_asset.FindByName(in_propertyName);

			if (textureProperty != null)
			{
				Asset unifiedBitmapAsset = textureProperty.GetSingleConnectedAsset();

				if (unifiedBitmapAsset != null)
				{
					AssetPropertyDistance distanceProperty = unifiedBitmapAsset.FindByName(in_distanceName) as AssetPropertyDistance;

					if (distanceProperty != null && distanceProperty.Value != 0.0)
					{
#if REVIT_API_2021 || REVIT_API_2022 || REVIT_API_2023
						return (float) UnitUtils.Convert(distanceProperty.Value, distanceProperty.GetUnitTypeId(), UnitTypeId.Feet);
#else
						return (float) UnitUtils.Convert(distanceProperty.Value, distanceProperty.DisplayUnitType, DisplayUnitType.DUT_DECIMAL_FEET);
#endif
					}
				}
			}

            return in_defaultValue;
        }

		private float GetTexturePropertyAngle(
			Asset  in_asset,
			string in_propertyName,
			string in_angleName
		)
        {
			// DebugLog.AddLine($"Texture Angle Property {in_propertyName}");

			AssetProperty textureProperty = in_asset.FindByName(in_propertyName);

			if (textureProperty != null)
			{
				Asset unifiedBitmapAsset = textureProperty.GetSingleConnectedAsset();

				if (unifiedBitmapAsset != null)
				{
					AssetProperty angleProperty = unifiedBitmapAsset.FindByName(in_angleName);

					if (angleProperty != null)
					{
						return (float) (angleProperty as AssetPropertyDouble).Value / 360.0F;
					}
				}
			}

            return 0.0F;
        }
	}
}
