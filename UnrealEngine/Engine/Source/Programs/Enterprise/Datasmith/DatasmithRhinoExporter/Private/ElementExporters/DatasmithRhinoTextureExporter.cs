// Copyright Epic Games, Inc. All Rights Reserved.


using DatasmithRhino.ExportContext;
using Rhino.DocObjects;
using System.Collections.Generic;

namespace DatasmithRhino.ElementExporters
{
	public class DatasmithRhinoTextureExporter : IDatasmithRhinoElementExporter<DatasmithRhinoTextureExporter, DatasmithTextureInfo>
	{
		///// BEGIN IDatasmithRhinoElementExporter Interface /////

		protected override int GetElementsToSynchronizeCount()
		{
			return ExportContext.TextureHashToTextureInfo.Count;
		}

		protected override IEnumerable<DatasmithTextureInfo> GetElementsToSynchronize()
		{
			return ExportContext.TextureHashToTextureInfo.Values;
		}

		protected override FDatasmithFacadeElement CreateElement(DatasmithTextureInfo ElementInfo)
		{
			FDatasmithFacadeTexture TextureElement = new FDatasmithFacadeTexture(ElementInfo.Name);

			if (ParseTexture(ElementInfo, TextureElement))
			{
				return TextureElement;
			}

			return null;
		}

		protected override void AddElement(DatasmithTextureInfo ElementInfo)
		{
			DatasmithScene.AddTexture(ElementInfo.ExportedTexture);
		}

		protected override void ModifyElement(DatasmithTextureInfo ElementInfo)
		{
			if (!ParseTexture(ElementInfo, ElementInfo.ExportedTexture))
			{
				DatasmithScene.RemoveTexture(ElementInfo.ExportedTexture);
			}
		}

		protected override void DeleteElement(DatasmithTextureInfo ElementInfo)
		{
			DatasmithScene.RemoveTexture(ElementInfo.ExportedTexture);
		}

		///// END IDatasmithRhinoElementExporter Interface /////

		private static bool ParseTexture(DatasmithTextureInfo TextureInfo, FDatasmithFacadeTexture TextureElement)
		{
			Texture RhinoTexture = TextureInfo.RhinoTexture;
			if (!TextureInfo.IsSupported())
			{
				return false;
			}

			TextureElement.SetLabel(TextureInfo.UniqueLabel);
			TextureElement.SetFile(TextureInfo.FilePath);
			TextureElement.SetTextureFilter(FDatasmithFacadeTexture.ETextureFilter.Default);
			TextureElement.SetRGBCurve(1);
			TextureElement.SetSRGB(RhinoTexture.TextureType == TextureType.Bitmap ? FDatasmithFacadeTexture.EColorSpace.sRGB : FDatasmithFacadeTexture.EColorSpace.Linear);
			TextureElement.SetTextureAddressX(RhinoTexture.WrapU == TextureUvwWrapping.Clamp ? FDatasmithFacadeTexture.ETextureAddress.Clamp : FDatasmithFacadeTexture.ETextureAddress.Wrap);
			TextureElement.SetTextureAddressY(RhinoTexture.WrapV == TextureUvwWrapping.Clamp ? FDatasmithFacadeTexture.ETextureAddress.Clamp : FDatasmithFacadeTexture.ETextureAddress.Wrap);

			FDatasmithFacadeTexture.ETextureMode TextureMode = FDatasmithFacadeTexture.ETextureMode.Diffuse;
			if (RhinoTexture.TextureType == TextureType.Bump)
			{
				TextureMode = FDatasmithFacadeTexture.ETextureMode.Bump;
			}

			TextureElement.SetTextureMode(TextureMode);

			return true;
		}
	}
}