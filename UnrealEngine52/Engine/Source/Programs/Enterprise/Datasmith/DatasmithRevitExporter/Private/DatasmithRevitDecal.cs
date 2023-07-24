// Copyright Epic Games, Inc. All Rights Reserved.

using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Visual;
using System.Collections.Generic;

namespace DatasmithRevitExporter
{
	public class FDecalMaterial
	{
		public string MaterialName { get; private set; }
		public string DiffuseTexturePath { get; private set; }
		public string BumpTexturePath { get; private set; }
		public string CutoutTexturePath { get; private set; }
		public double BumpAmount { get; private set; }
		public double Transparency { get; private set; }
		public double Luminance { get; private set; }

		private void SetMaterialProperties(Asset InMaterialAsset)
		{
			const string DiffuseMapPropName = "decApp_diffuse";
			const string BumpMapPropName = "decApp_bump_map";
			const string TexturePathName = "unifiedbitmap_Bitmap";
			const string BumpAmountPropName = "decApp_bump_amount";
			const string CutoutMapPropName = "decApp_cutout_opacity";
			const string LuminancePropName = "decApp_self_illum_luminance";
			const string TransparencyPropName = "decApp_transparency";

			string GetConnectedImagePath(AssetProperty InProp)
			{
				IList<AssetProperty> ConnectedProps = InProp.GetAllConnectedProperties();

				foreach (AssetProperty InnerProp in ConnectedProps)
				{
					if (InnerProp.Type == AssetPropertyType.Asset)
					{
						Asset InnerPropAsset = InnerProp as Asset;

						for (int Index = 0; Index < InnerPropAsset.Size; ++Index)
						{
#if REVIT_API_2018
							AssetProperty AssetProp = InnerPropAsset[Index];
#else
							AssetProperty AssetProp = InnerPropAsset.Get(Index);
#endif

							if (AssetProp.Name == TexturePathName)
							{
								return (AssetProp as AssetPropertyString).Value;
							}
						}
					}
				}

				return null;
			}

			for (int PropIndex = 0; PropIndex < InMaterialAsset.Size; ++PropIndex)
			{
#if REVIT_API_2018
				AssetProperty Prop = InMaterialAsset[PropIndex];
#else
				AssetProperty Prop = InMaterialAsset.Get(PropIndex);
#endif
				if (Prop.Name == DiffuseMapPropName)
				{
					DiffuseTexturePath = GetConnectedImagePath(Prop);
				}
				else if (Prop.Name == BumpMapPropName)
				{
					BumpTexturePath = GetConnectedImagePath(Prop);
				}
				else if (Prop.Name == BumpAmountPropName)
				{
					BumpAmount = (Prop as AssetPropertyDouble)?.Value ?? 0;
				}
				else if (Prop.Name == CutoutMapPropName)
				{
					CutoutTexturePath = GetConnectedImagePath(Prop);
				}
				else if (Prop.Name == LuminancePropName)
				{
					Luminance = (Prop as AssetPropertyDouble)?.Value ?? 0;
				}
				else if (Prop.Name == TransparencyPropName)
				{
					Transparency = (Prop as AssetPropertyDouble)?.Value ?? 0;
				}
			}
		}

		private static void FillDecalElementIdAndAppearancePairList(Asset Decal, ref IList<KeyValuePair<ElementId,Asset>> DecalElementIds)
		{
			const string PropertyName_Decal = "Decal";
			const string PropertyName_DecalElemId = "decalelementId";
			const string PropertyName_DecalAppearance = "DecalAppearance";
			const string PropertyName_SurfaceMaterial = "surface_material";

			int DecalElementId = -1;
			Asset DecalAppearance = null;
			for (int PropIndex = 0; PropIndex < Decal.Size; ++PropIndex)
			{
#if REVIT_API_2018
				AssetProperty Prop = Decal[PropIndex];
#else
				AssetProperty Prop = Decal.Get(PropIndex);
#endif

				if (Prop.Type == AssetPropertyType.Reference)
				{
					AssetPropertyReference RefProp = (Prop as AssetPropertyReference);
					foreach (AssetProperty InnerProp in RefProp.GetAllConnectedProperties())
					{
						if (Prop.Name == PropertyName_SurfaceMaterial && InnerProp.Name == PropertyName_Decal && InnerProp.Type == AssetPropertyType.Asset)
						{
							Asset InnerDecal = InnerProp as Asset;
							FillDecalElementIdAndAppearancePairList(InnerDecal, ref DecalElementIds);
						}

						if (InnerProp.Name == PropertyName_DecalAppearance && InnerProp.Type == AssetPropertyType.Asset)
						{
							DecalAppearance = InnerProp as Asset;
						}
					}
				}

				if (Prop.Name == PropertyName_DecalElemId && Prop.Type == AssetPropertyType.Integer)
				{
					DecalElementId = (Prop as AssetPropertyInteger).Value;
				}
			}

			if (DecalAppearance != null && DecalElementId != -1)
			{
				DecalElementIds.Add(new KeyValuePair<ElementId, Asset>(new ElementId(DecalElementId), DecalAppearance));
			}
		}

		public static IList<KeyValuePair<ElementId, Asset>> GetDecalElementIdAndAppearancePairList(MaterialNode InMaterialNode)
		{
			IList<KeyValuePair<ElementId, Asset>> DecalElementIdAndAppearancePairList = new List<KeyValuePair<ElementId, Asset>>();

			if (!InMaterialNode.HasOverriddenAppearance)
			{
				return DecalElementIdAndAppearancePairList;
			}

			Asset Decal = InMaterialNode.GetAppearanceOverride();

			if (Decal == null || Decal.Name != "Decal")
			{
				return DecalElementIdAndAppearancePairList;
			}

			FillDecalElementIdAndAppearancePairList(Decal, ref DecalElementIdAndAppearancePairList);

			return DecalElementIdAndAppearancePairList;
		}

		public static FDecalMaterial Create(MaterialNode InMaterialNode, Material InMaterial, int DecalAppearanceIndex, Asset DecalAppearanceAsset)
		{
			FDecalMaterial DecalMaterial = new FDecalMaterial();

			DecalMaterial.MaterialName = FDatasmithFacadeElement.GetStringHash($"Decal_{FMaterialData.GetMaterialName(InMaterialNode, InMaterial)}_{DecalAppearanceIndex}");

			DecalMaterial.SetMaterialProperties(DecalAppearanceAsset);

			return DecalMaterial;
		}

		//Ignores MaterialName, only checks Render value equality
		public bool CheckRenderValueEquiality(FDecalMaterial Input)
		{
			if (DiffuseTexturePath == Input.DiffuseTexturePath
				&& BumpTexturePath == Input.BumpTexturePath
				&& CutoutTexturePath == Input.CutoutTexturePath
				&& BumpAmount == Input.BumpAmount
				&& Transparency == Input.Transparency
				&& Luminance == Input.Luminance)
			{
				return true;
			}
			return false;
		}
	}
}
