// Copyright Epic Games, Inc. All Rights Reserved.

using Rhino.DocObjects;
using Rhino.Runtime;

namespace DatasmithRhino.Utils
{
	public class DatasmithRhinoUniqueNameGenerator
	{
		/// <summary>
		/// Gives a human readable name based on the type of the given common object. 
		/// </summary>
		/// <param name="InCommonObject"></param>
		/// <returns></returns>
		public static string GetDefaultTypeName(CommonObject InCommonObject)
		{
			string DefaultName;

			if (InCommonObject is RhinoObject InRhinoObject)
			{
				switch (InRhinoObject.ObjectType)
				{
					case ObjectType.InstanceDefinition:
						DefaultName = "block definition";
						break;
					case ObjectType.InstanceReference:
						// The default name for instances should be the name of the definition.
						InstanceDefinition Definition = (InRhinoObject as InstanceObject).InstanceDefinition;
						if (Definition != null && !string.IsNullOrEmpty(Definition.Name))
						{
							DefaultName = Definition.Name;
						}
						else
						{
							DefaultName = "block instance";
						}
						break;
					case ObjectType.Point:
						DefaultName = "point";
						break;
					case ObjectType.Curve:
						DefaultName = "curve";
						break;
					case ObjectType.Surface:
						DefaultName = "surface";
						break;
					case ObjectType.Brep:
						DefaultName = "brep";
						break;
					case ObjectType.Mesh:
						DefaultName = "mesh";
						break;
					case ObjectType.TextDot:
						DefaultName = "textdot";
						break;
					case ObjectType.SubD:
						DefaultName = "subd";
						break;
					case ObjectType.BrepLoop:
						DefaultName = "loop";
						break;
					case ObjectType.Cage:
						DefaultName = "cage";
						break;
					case ObjectType.ClipPlane:
						DefaultName = "clip plane";
						break;
					case ObjectType.Extrusion:
						DefaultName = "extrusion";
						break;
					case ObjectType.Light:
						DefaultName = "light";
						break;
					default:
						DefaultName = "unknown";
						break;
				}
			}
			else if (InCommonObject is Layer)
			{
				DefaultName = "layer";
			}
			else if (InCommonObject is Material)
			{
				DefaultName = "material";
			}
			else if (InCommonObject is ViewportInfo)
			{
				DefaultName = "camera";
			}
			else
			{
				DefaultName = "unknown";
			}

			return DefaultName;
		}

		/// <summary>
		/// Gives the non-unique "base" name for the given model component.
		/// </summary>
		/// <param name="InModelComponent"></param>
		/// <returns></returns>
		public static string GetTargetName(ModelComponent InModelComponent)
		{
			string TargetName = !string.IsNullOrEmpty(InModelComponent.Name)
				? InModelComponent.Name
				: GetDefaultTypeName(InModelComponent);

			return TargetName;
		}

		/// <summary>
		/// Gives the non-unique "base" name for the given model component.
		/// </summary>
		/// <param name="InModelComponent"></param>
		/// <returns></returns>
		public static string GetTargetName(ViewInfo InNamedView)
		{
			string TargetName = !string.IsNullOrEmpty(InNamedView.Name)
				? InNamedView.Name
				: GetDefaultTypeName(InNamedView.Viewport);

			return TargetName;
		}

		/// <summary>
		/// Generate a unique name from the given component, the returned name won't be generated again.
		/// </summary>
		/// <param name="InModelComponent"></param>
		/// <returns></returns>
		public string GenerateUniqueName(ModelComponent InModelComponent)
		{
			string TargetName = GetTargetName(InModelComponent);
			return GenerateUniqueNameFromBaseName(TargetName);
		}

		/// <summary>
		/// Generate a unique name from the given string BaseName, the returned name won't be generated again.
		/// </summary>
		/// <param name="BaseName"></param>
		/// <returns></returns>
		public string GenerateUniqueNameFromBaseName(string BaseName)
		{
			string UniqueName = FacadeNameProvider.GenerateUniqueName(BaseName);
			FacadeNameProvider.AddExistingName(UniqueName);

			return UniqueName;
		}

		private FDatasmithFacadeUniqueNameProvider FacadeNameProvider = new FDatasmithFacadeUniqueNameProvider();
	}
}