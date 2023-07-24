﻿// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.FConfigurationTree;


namespace DatasmithSolidworks
{
	// Wrapping Component Name, Actor Name and Mesh Name to distinguish one from the other in the code. This helps to
	//  - make explicit which name where
	//  - avoid using sanitized(Datasmith) name to check component's name and vice versa
	namespace Names
	{
		public struct FActorName : IEquatable<FActorName>
		{
			private string Value;

			public FActorName(FComponentName ComponentName)
			{
				Value = FDatasmithExporter.SanitizeName(ComponentName.GetString());
			}
			private FActorName(string InValue) => Value = InValue;

			public static FActorName FromString(string Value) => new FActorName(Value);

			// Following is common between 'Names' but unfortunately C# has no way to generalize this for structs
			public bool IsValid() => !string.IsNullOrEmpty(Value);
			public string GetString() => Value;

			public override string ToString() => Value;

			public override bool Equals(object Obj) => Obj is FActorName Other && this == Other;
			public bool Equals(FActorName Other) => Value == Other.Value;
			public static bool operator ==(FActorName A, FActorName B) => A.Equals(B);
			public static bool operator !=(FActorName A, FActorName B) => !(A == B);

			public override int GetHashCode() => Value.GetHashCode();
		}

		public struct FComponentName: IEquatable<FComponentName>
		{
			private string Value;

			public FComponentName(Component2 Component)
			{
				Value = Component.Name2;
			}

			public FComponentName(IComponent2 Component)
			{
				Value = Component.Name2;
			}

			private FComponentName(string Name)
			{
				Value = Name;
			}

			/// Used to convert string to component name ONLY when received from Solidworks API
			public static FComponentName FromApiString(string Name)
			{
				return new FComponentName(Name);
			}

			/// Used to convert from a custom string to represent something 'like' a Solidworks component
			/// todo: may want to avoid trying to mix components and non-components into the same 'entity'?
			public static FComponentName FromCustomString(string Value)
			{
				return new FComponentName(Value);
			}

			public FActorName GetActorName()
			{
				return new FActorName(this);
			}

			public string GetLabel()
			{
				return Value.Split('/').Last();
			}

			// Following is common between 'Names' but unfortunately C# has no way to generalize this for structs
			public bool IsValid() => !string.IsNullOrEmpty(Value);
			public string GetString() => Value;

			public override string ToString() => Value;
			public override bool Equals(object Obj) => Obj is FComponentName Other && this == Other;
			public bool Equals(FComponentName Other) => Value == Other.Value;
			public static bool operator ==(FComponentName A, FComponentName B) => A.Equals(B);
			public static bool operator !=(FComponentName A, FComponentName B) => !(A == B);

			public override int GetHashCode() => Value.GetHashCode();
		}

		public struct FMeshName : IEquatable<FMeshName>
		{
			private string Value;

			public FMeshName(FComponentName ComponentName) => Value = $"{ComponentName}_Mesh";
			public FMeshName(FComponentName ComponentName, string CfgName) => Value = $"{ComponentName}_{CfgName}_Mesh";

			private FMeshName(string InValue) => Value = InValue;

			public static FMeshName FromString(string Value) => new FMeshName(Value);

			// Following is common between 'Names' but unfortunately C# has no way to generalize this for structs
			public bool IsValid() => !string.IsNullOrEmpty(Value);
			public string GetString() => Value;

			public override string ToString() => Value;

			public override bool Equals(object Obj) => Obj is FMeshName Other && this == Other;
			public bool Equals(FMeshName Other) => Value == Other.Value;
			public static bool operator ==(FMeshName A, FMeshName B) => A.Equals(B);
			public static bool operator !=(FMeshName A, FMeshName B) => !(A == B);

			public override int GetHashCode() => Value.GetHashCode();
		}

	}
}

namespace DatasmithSolidworks
{

	public enum EActorType
	{
		SimpleActor,
		MeshActor,
		PointLightActor,
		SpotLightActor,
		DirLightActor
	};


	public class FDatasmithActorExportInfo
	{
		public EActorType Type;
		public string Label;
		public FActorName Name;
		public FActorName ParentName;
		public FMeshName MeshName;
		public float[] Transform;
		public bool bVisible;
	};

	public class FDatasmithExporter
	{
		private Dictionary<FActorName, Tuple<EActorType, FDatasmithFacadeActor>> ExportedActorsMap = new Dictionary<FActorName, Tuple<EActorType, FDatasmithFacadeActor>>();
		private ConcurrentDictionary<FMeshName, FDatasmithFacadeMeshElement> ExportedMeshesMap = new ConcurrentDictionary<FMeshName, FDatasmithFacadeMeshElement>();
		private ConcurrentDictionary<int, FDatasmithFacadeMaterialInstance> ExportedMaterialsMap = new ConcurrentDictionary<int, FDatasmithFacadeMaterialInstance>();
		private ConcurrentDictionary<string, FDatasmithFacadeTexture> ExportedTexturesMap = new ConcurrentDictionary<string, FDatasmithFacadeTexture>();
		private Dictionary<string, FDatasmithFacadeActorBinding> ExportedActorBindingsMap = new Dictionary<string, FDatasmithFacadeActorBinding>();
		private Dictionary<string, FDatasmithFacadeVariant> ExportedVariantsMap = new Dictionary<string, FDatasmithFacadeVariant>();

		private FDatasmithFacadeScene DatasmithScene = null;

		public struct FMeshExportInfo
		{
			public FComponentName ComponentName;
			public FMeshName MeshName;
			public FActorName ActorName;
			public FMeshData MeshData;

			public FDatasmithFacadeMeshElement MeshElement;

			public override string ToString()
			{
				return $"FMeshExportInfo(ComponentName={ComponentName}, MeshName={MeshName}, ActorName={ActorName}), MeshElement={(MeshElement==null?"<null>":MeshElement.GetName())}";
			}
		}

		public FDatasmithExporter(FDatasmithFacadeScene InScene)
		{
			DatasmithScene = InScene;
		}

		public EActorType? GetExportedActorType(FActorName InActorName)
		{
			Tuple<EActorType, FDatasmithFacadeActor> ActorInfo = null;
			if (ExportedActorsMap.TryGetValue(InActorName, out ActorInfo))
			{
				return ActorInfo.Item1;
			}
			return null;
		}

		public FDatasmithFacadeActor ExportOrUpdateActor(FDatasmithActorExportInfo InExportInfo)
		{
			FDatasmithFacadeActor Actor = null;

			if (ExportedActorsMap.ContainsKey(InExportInfo.Name))
			{
				Tuple<EActorType, FDatasmithFacadeActor> ActorInfo = ExportedActorsMap[InExportInfo.Name];

				if (ActorInfo.Item1 != InExportInfo.Type)
				{
					// Actor was exported but is different type -- delete old one
					DatasmithScene.RemoveActor(ActorInfo.Item2);
					ExportedActorsMap.Remove(InExportInfo.Name);
				}
				else
				{
					Actor = ActorInfo.Item2;
				}
			}

			if (Actor == null)
			{
				switch (InExportInfo.Type)
				{
					case EActorType.SimpleActor: Actor = new FDatasmithFacadeActor(InExportInfo.Name.GetString()); break;
					case EActorType.MeshActor: Actor = new FDatasmithFacadeActorMesh(InExportInfo.Name.GetString()); break;
					case EActorType.PointLightActor: Actor = new FDatasmithFacadePointLight(InExportInfo.Name.GetString()); break;
					case EActorType.SpotLightActor: Actor = new FDatasmithFacadeSpotLight(InExportInfo.Name.GetString()); break;
					case EActorType.DirLightActor: Actor = new FDatasmithFacadeDirectionalLight(InExportInfo.Name.GetString()); break;
				}

				Actor.AddTag(InExportInfo.Name.GetString());

				ExportedActorsMap[InExportInfo.Name] = new Tuple<EActorType, FDatasmithFacadeActor>(InExportInfo.Type, Actor);

				Tuple<EActorType, FDatasmithFacadeActor> ParentExportInfo = null;

				if (InExportInfo.ParentName.IsValid() && ExportedActorsMap.TryGetValue(InExportInfo.ParentName, out ParentExportInfo))
				{
					FDatasmithFacadeActor ParentActor = ParentExportInfo.Item2;
					ParentActor.AddChild(Actor);
				}
				else
				{
					DatasmithScene.AddActor(Actor);
				}
			}

			// ImportBinding uses Tag[0] ('original name') to group parts used in variants
			Actor.SetLabel(InExportInfo.Label);
			Actor.SetVisibility(InExportInfo.bVisible);
			Actor.SetWorldTransform(AdjustTransformForDatasmith(InExportInfo.Transform));

			if (InExportInfo.Type == EActorType.MeshActor)
			{
				FDatasmithFacadeActorMesh MeshActor = Actor as FDatasmithFacadeActorMesh;
				if (InExportInfo.MeshName.IsValid())
				{
					MeshActor.SetMesh(GetDatasmithMeshName(InExportInfo.MeshName));
				}
			}

			return Actor;
		}

		public void AddMesh(FMeshName MeshName, FDatasmithFacadeMeshElement NewMesh)
		{
			Debug.Assert(MeshName.IsValid());
			Debug.Assert(NewMesh != null);
			RemoveMesh(MeshName);
			ExportedMeshesMap.TryAdd(MeshName, NewMesh);
			DatasmithScene.AddMesh(NewMesh);
		}

		public void RemoveMesh(FMeshName MeshName)
		{
            if (!MeshName.IsValid())
            {
                return;
            }
			if (ExportedMeshesMap.TryRemove(MeshName, out FDatasmithFacadeMeshElement OldMesh))
			{
				DatasmithScene.RemoveMesh(OldMesh);
			}
		}

		public void RemoveActor(FActorName InActorName)
		{
			if (ExportedActorsMap.ContainsKey(InActorName))
			{
				Tuple<EActorType, FDatasmithFacadeActor> ActorInfo = ExportedActorsMap[InActorName];
				FDatasmithFacadeActor Actor = ActorInfo.Item2;
				DatasmithScene.RemoveActor(Actor);
				ExportedActorsMap.Remove(InActorName);
			}
		}

		public void ExportLight(FLight InLight)
		{
			FDatasmithActorExportInfo ExportInfo = new FDatasmithActorExportInfo();
			ExportInfo.Label = InLight.LightLabel;
			ExportInfo.Name = FActorName.FromString(InLight.LightName);
			ExportInfo.bVisible = true;

			FVec3 LightPosition = null;
			FVec3 LightDirection = null;

			switch (InLight.LightType)
			{
				case FLight.EType.Directional:
				{
					LightDirection = - InLight.DirLightDirection; // Direction FROM the light source, SW gives away direction TOWARDS light source
					ExportInfo.Type = EActorType.DirLightActor;
				}
				break;
				case FLight.EType.Point:
				{
					LightPosition = InLight.PointLightPosition;
					ExportInfo.Type = EActorType.PointLightActor;
				}
				break;
				case FLight.EType.Spot:
				{
					LightPosition = InLight.SpotLightPosition;
					LightDirection = (InLight.SpotLightTarget - InLight.SpotLightPosition).Normalized();  // Direction FROM the light source
					ExportInfo.Type = EActorType.SpotLightActor;
				}
				break;

				default: return; // Unsupported light type
			}

			if (LightDirection != null)
			{
				ExportInfo.Transform = MathUtils.LookAt(LightDirection, LightPosition, 100f);
			}
			else if (LightPosition != null)
			{
				ExportInfo.Transform = MathUtils.Translation(LightPosition, 100f);
			}

			FDatasmithFacadeActorLight LightActor = ExportOrUpdateActor(ExportInfo) as FDatasmithFacadeActorLight;

			const float MaxIntensity = 500f; // Map from SW (normlized) to Datasmith intensity

			LightActor.SetIntensity(InLight.Intensity * MaxIntensity);
			LightActor.SetColor(InLight.Color.X, InLight.Color.Y, InLight.Color.Z, 1f);
			LightActor.SetEnabled(InLight.bIsEnabled);

			if (LightActor is FDatasmithFacadeSpotLight SpotLight)
			{
				// Solidworks spot light has only one cone angle
				SpotLight.SetInnerConeAngle(InLight.SpotLightConeAngle);
				SpotLight.SetOuterConeAngle(InLight.SpotLightConeAngle);
			}
		}

		public bool ExportMesh(FMeshName InMeshName, FMeshData InData, out FDatasmithFacadeMeshElement OutMeshElement)
		{
			string MeshNameInitial = InMeshName.GetString(); // Make Datasmith Mesh string to build Mesh/MeshElement from, Datasmith will modify it with FDatasmithUtils::SanitizeObjectName

			OutMeshElement = null;

			if (InData.Vertices == null || InData.Normals == null || InData.TexCoords == null || InData.Triangles == null)
			{
				Addin.Instance.LogDebugThread($"  skipping(some attributes are null) - Vertices:{InData.Vertices == null}, Normals:{InData.Normals == null}, TexCoords: {InData.TexCoords == null}, Triangles:{InData.Triangles}");
				return false;
			}

			if (InData.Vertices.Length == 0 || InData.Normals.Length == 0 || InData.TexCoords.Length == 0 || InData.Triangles.Length == 0)
			{
				Addin.Instance.LogDebugThread($"  skipping - Vertices: {InData.Vertices.Length}, Normals: {InData.Normals.Length}, TexCoords: {InData.TexCoords.Length} Triangles: {InData.Triangles.Length}");
				return false;
			}

			using(FDatasmithFacadeMesh Mesh = new FDatasmithFacadeMesh())
			{
				Mesh.SetName(MeshNameInitial);

				FDatasmithFacadeMeshElement MeshElement = new FDatasmithFacadeMeshElement(MeshNameInitial);

				Mesh.SetVerticesCount(InData.Vertices.Length);
				Mesh.SetFacesCount(InData.Triangles.Length);

				for (int i = 0; i < InData.Vertices.Length; i++)
				{
					Mesh.SetVertex(i, InData.Vertices[i].X, InData.Vertices[i].Y, InData.Vertices[i].Z);
				}

				for (int i = 0; i < InData.Normals.Length; i++)
				{
					Mesh.SetNormal(i, InData.Normals[i].X, InData.Normals[i].Y, InData.Normals[i].Z);
				}

				if (InData.TexCoords != null)
				{
					Mesh.SetUVChannelsCount(1);
					Mesh.SetUVCount(0, InData.TexCoords.Length);
					for (int i = 0; i < InData.TexCoords.Length; i++)
					{
						Mesh.SetUV(0, i, InData.TexCoords[i].X, InData.TexCoords[i].Y);
					}

				}

				HashSet<int> MeshAddedMaterials = new HashSet<int>();

				HashSet<int> MeshMaterialIds = new HashSet<int>();

				for (int TriIndex = 0; TriIndex < InData.Triangles.Length; TriIndex++)
				{
					FTriangle Triangle = InData.Triangles[TriIndex];
					int MatID = 0;

					MeshMaterialIds.Add(Triangle.MaterialID);
					if (Triangle.MaterialID >= 1)
					{
						if (!MeshAddedMaterials.Contains(Triangle.MaterialID))
						{
							Addin.Instance.LogDebugThread($"  set material if it's exported: {Triangle.MaterialID}");

							FDatasmithFacadeMaterialInstance Material = null;
							ExportedMaterialsMap.TryGetValue(Triangle.MaterialID, out Material);

							if (Material != null)
							{
								Addin.Instance.LogDebugThread(
									$" SetMaterial({Material.GetName()}, SlotId: {Triangle.MaterialID})");

								MeshAddedMaterials.Add(Triangle.MaterialID);
								MeshElement.SetMaterial(Material.GetName(), Triangle.MaterialID);
								MatID = Triangle.MaterialID;
							}
						}
						else
						{
							MatID = Triangle.MaterialID;
						}
					}

					Mesh.SetFace(TriIndex, Triangle[0], Triangle[1], Triangle[2], MatID);
					Mesh.SetFaceUV(TriIndex, 0, Triangle[0], Triangle[1], Triangle[2]);
				}

				Addin.Instance.LogDebugThread($"  material ids assigned to faces: {string.Join(", ", MeshMaterialIds)})");
				if (MeshAddedMaterials.Count > 0)
				{
					Addin.Instance.LogDebugThread($"  assigned materials: {string.Join(", ", MeshAddedMaterials)})");
				}

				DatasmithScene.ExportDatasmithMesh(MeshElement, Mesh);
				OutMeshElement = MeshElement;
			}

			return true;
		}

		public void ExportMetadata(FMetadata InMetadata)
		{
			FDatasmithFacadeElement Element = null;

			if (InMetadata.OwnerType == FMetadata.EOwnerType.Actor)
			{
				
				if (ExportedActorsMap.ContainsKey(InMetadata.OwnerName))
				{
					Tuple<EActorType, FDatasmithFacadeActor> ActorInfo = ExportedActorsMap[InMetadata.OwnerName];
					Element = ActorInfo.Item2;
				}
			}
			else if (InMetadata.OwnerType == FMetadata.EOwnerType.MeshActor)
			{
				//TODO do we need metadata on mesh elements?
				//Element = processor.MeshFactory.GetFacadeElement(cmd.MetadataOwnerName);
			}

			if (Element != null)
			{
				FDatasmithFacadeMetaData DatasmithMetadata = DatasmithScene.GetMetaData(Element);

				if (DatasmithMetadata == null)
				{
					DatasmithMetadata = new FDatasmithFacadeMetaData("SolidWorks Document Metadata");
					DatasmithMetadata.SetAssociatedElement(Element);
					DatasmithScene.AddMetaData(DatasmithMetadata);
				}

				foreach (IMetadataPair Pair in InMetadata.Pairs)
				{
					Pair.WriteToDatasmithMetaData(DatasmithMetadata);
				}
			}
		}

		FDatasmithFacadeActorBinding GetActorBinding(FComponentName InComponentName, FDatasmithFacadeVariant InVariant)
		{
			return GetActorBinding(GetComponentActorName(InComponentName), InVariant);
		}

		private FDatasmithFacadeActorBinding GetActorBinding(FActorName ActorName, FDatasmithFacadeVariant InVariant)
		{
			for (int BindingIndex = 0; BindingIndex < InVariant.GetActorBindingsCount(); ++BindingIndex)
			{
				FDatasmithFacadeActorBinding Binding = InVariant.GetActorBinding(BindingIndex);
				if (ActorName.Equals(Binding.GetName()))
				{
					return Binding;
				}
			}

			// No binding was found, add one

			FDatasmithFacadeActor Actor = null;

			if (ExportedActorsMap.TryGetValue(ActorName, out Tuple<EActorType, FDatasmithFacadeActor> ActorInfo))
			{
				Actor = ActorInfo.Item2;
			}
			else
			{
				// Actor was not found, should not happen
				return null;
			}

			// Make a new binding
			FDatasmithFacadeActorBinding NewBinding = new FDatasmithFacadeActorBinding(Actor);
			InVariant.AddActorBinding(NewBinding);

			return NewBinding;
		}

		private void ExportMaterialVariants(List<Tuple<FConfigurationData, FDatasmithFacadeVariant>> InVariants)
		{
			foreach (Tuple<FConfigurationData, FDatasmithFacadeVariant> KVP in InVariants)
			{
				FConfigurationData Config = KVP.Item1;
				FDatasmithFacadeVariant Variant = KVP.Item2;

				// Iterate over all material assignments
				foreach (var MatKVP in Config.ComponentMaterials)
				{
					FDatasmithFacadeActorBinding Binding = GetActorBinding(MatKVP.Key, Variant);
					if (Binding != null)
					{
						HashSet<int> UniqueMaterialsSet = new HashSet<int>();
						FObjectMaterials Materials = MatKVP.Value;

						if (Materials.ComponentMaterialID != -1 && !UniqueMaterialsSet.Contains(Materials.ComponentMaterialID))
						{
							UniqueMaterialsSet.Add(Materials.ComponentMaterialID);
						}
						if (Materials.PartMaterialID != -1 && !UniqueMaterialsSet.Contains(Materials.ComponentMaterialID))
						{
							UniqueMaterialsSet.Add(Materials.PartMaterialID);
						}
						foreach (var MatID in Materials.BodyMaterialsMap.Values)
						{
							if (!UniqueMaterialsSet.Contains(Materials.ComponentMaterialID))
							{
								UniqueMaterialsSet.Add(MatID);
							}
						}
						foreach (var MatID in Materials.FeatureMaterialsMap.Values)
						{
							if (!UniqueMaterialsSet.Contains(Materials.ComponentMaterialID))
							{
								UniqueMaterialsSet.Add(MatID);
							}
						}
						foreach (var MatID in Materials.FaceMaterialsMap.Values)
						{
							if (!UniqueMaterialsSet.Contains(Materials.ComponentMaterialID))
							{
								UniqueMaterialsSet.Add(MatID);
							}
						}

						foreach (var MatID in UniqueMaterialsSet)
						{
							FMaterial Material = Materials.GetMaterial(MatID);
							FDatasmithFacadeMaterialInstance DatasmithMaterial = null;
							if (Material != null && ExportedMaterialsMap.TryGetValue(Material.ID, out DatasmithMaterial))
							{
								Binding.AddMaterialCapture(DatasmithMaterial);
							}
						}
					}
				}
			}
		}

		private void ExportTransformVariants(List<Tuple<FConfigurationData, FDatasmithFacadeVariant>> InVariants)
		{
			foreach (Tuple<FConfigurationData, FDatasmithFacadeVariant> KVP in InVariants)
			{
				FConfigurationData Config = KVP.Item1;
				FDatasmithFacadeVariant Variant = KVP.Item2;

				// Provide transform variants
				foreach (var TransformMap in Config.ComponentTransform)
				{
					FDatasmithFacadeActorBinding Binding = GetActorBinding(TransformMap.Key, Variant);
					if (Binding != null)
					{
						Binding.AddRelativeTransformCapture(TransformMap.Value);
					}
				}
			}
		}

		private void ExportActorVisibilityVariants(List<Tuple<FConfigurationData, FDatasmithFacadeVariant>> InVariants)
		{
			foreach (Tuple<FConfigurationData, FDatasmithFacadeVariant> KVP in InVariants)
			{
				FConfigurationData Config = KVP.Item1;
				FDatasmithFacadeVariant Variant = KVP.Item2;

				// Build a visibility variant data
				foreach (var VisibilityMap in Config.ComponentVisibility)
				{
					FDatasmithFacadeActorBinding Binding = GetActorBinding(VisibilityMap.Key, Variant);
					if (Binding != null)
					{
						Binding.AddVisibilityCapture(VisibilityMap.Value);
					}
				}
			}
		}

		public void ExportLevelVariantSets(List<FConfigurationData> InConfigs)
		{
			if (InConfigs == null)
			{
				return;
			}

			// Request existing VariantSet, or create a new one
			FDatasmithFacadeLevelVariantSets LevelVariantSets = null;

			if (DatasmithScene.GetLevelVariantSetsCount() == 0)
			{
				LevelVariantSets = new FDatasmithFacadeLevelVariantSets("LevelVariantSets");
				DatasmithScene.AddLevelVariantSets(LevelVariantSets);
			}
			else
			{
				LevelVariantSets = DatasmithScene.GetLevelVariantSets(0);
			}

			FDatasmithFacadeVariantSet GetOrCreateLevelVariantSet(string InName)
			{
				int VariantSetsCount = LevelVariantSets.GetVariantSetsCount();
				FDatasmithFacadeVariantSet VariantSet = null;

				for (int VariantSetIndex = 0; VariantSetIndex < VariantSetsCount; ++VariantSetIndex)
				{
					FDatasmithFacadeVariantSet VSet = LevelVariantSets.GetVariantSet(VariantSetIndex);

					if (VSet.GetName() == InName)
					{
						VariantSet = VSet;
						break;
					}
				}

				if (VariantSet == null)
				{
					VariantSet = new FDatasmithFacadeVariantSet(InName);
					LevelVariantSets.AddVariantSet(VariantSet);
				}
				return VariantSet;
			}

			FDatasmithFacadeVariantSet ConfigurationsVariantSet = null;
			FDatasmithFacadeVariantSet DisplayStatesVariantSet = null;

			List<Tuple<FConfigurationData, FDatasmithFacadeVariant>> ConfigurationVariants = null;
			List<Tuple<FConfigurationData, FDatasmithFacadeVariant>> DisplayStateVariants = null;

			foreach (FConfigurationData Config in InConfigs)
			{
				FDatasmithFacadeVariant Variant = new FDatasmithFacadeVariant(Config.Name);

				if (Config.bIsDisplayStateConfiguration)
				{
					if (DisplayStatesVariantSet == null)
					{
						DisplayStatesVariantSet = GetOrCreateLevelVariantSet("DisplayStates");
						DisplayStateVariants = new List<Tuple<FConfigurationData, FDatasmithFacadeVariant>>();
					}
					DisplayStatesVariantSet.AddVariant(Variant);
					DisplayStateVariants.Add(new Tuple<FConfigurationData, FDatasmithFacadeVariant>(Config, Variant));
				}
				else
				{
					if (ConfigurationsVariantSet == null)
					{
						ConfigurationsVariantSet = GetOrCreateLevelVariantSet("Configurations");
						ConfigurationVariants = new List<Tuple<FConfigurationData, FDatasmithFacadeVariant>>();
					}
					ConfigurationsVariantSet.AddVariant(Variant);
					ConfigurationVariants.Add(new Tuple<FConfigurationData, FDatasmithFacadeVariant>(Config, Variant));
				}
			}

			if (ConfigurationVariants != null)
			{
				// todo: visibility variants may drop ComponentName from FConfigurationData
				// at this point only actor names may stay
				ExportActorVisibilityVariants(ConfigurationVariants);
				ExportMaterialVariants(ConfigurationVariants);
				ExportTransformVariants(ConfigurationVariants);

				// Geometry variants
				foreach (Tuple<FConfigurationData, FDatasmithFacadeVariant> KVP in ConfigurationVariants)
				{
					FConfigurationData Config = KVP.Item1;
					FDatasmithFacadeVariant Variant = KVP.Item2;

					// Make visible mesh actor corresponding this configuration only
					foreach (FConfigurationData.FComponentGeometryVariant GeometryVariant in Config.ComponentGeometry.Values)
					{
						foreach (FActorName ActorName in GeometryVariant.All)
						{
							bool bVisible = GeometryVariant.VisibleActor == ActorName;
							GetActorBinding(ActorName, Variant)?.AddVisibilityCapture(bVisible);
						}
					}
				}
			}

			if (DisplayStateVariants != null)
			{
				ExportMaterialVariants(DisplayStateVariants);
			}
		}

		public void ExportMaterials(ConcurrentDictionary<int, FMaterial> InMaterialsMap)
		{
			Addin.Instance.LogDebug($"ExportMaterials: \n  {string.Join("  \n", InMaterialsMap.Select(KVP => $"{KVP.Key}: {KVP.Value}" ))}");

			ConcurrentBag<FDatasmithFacadeTexture> CreatedTextures = new ConcurrentBag<FDatasmithFacadeTexture>();
			ConcurrentBag<FDatasmithFacadeMaterialInstance> CreatedMaterials = new ConcurrentBag<FDatasmithFacadeMaterialInstance>();
			foreach(KeyValuePair<int, FMaterial> MatKVP in InMaterialsMap)
			{
				List<FDatasmithFacadeTexture> NewMaterialTextures = null;
				FDatasmithFacadeMaterialInstance NewMaterial = null;
				if (CreateAndCacheMaterial(MatKVP.Value, out NewMaterialTextures, out NewMaterial))
				{
					CreatedMaterials.Add(NewMaterial);

					foreach (FDatasmithFacadeTexture Texture in NewMaterialTextures)
					{
						CreatedTextures.Add(Texture);
					}
				}
			}

			Addin.Instance.LogDebug($"  AddMaterials");
			// Adding stuff to a datasmith scene cannot be multithreaded!
			foreach (FDatasmithFacadeMaterialInstance Mat in CreatedMaterials)
			{
				Addin.Instance.LogDebug($"  AddMaterial: {Mat.GetName()}: {Mat.GetLabel()}");
				DatasmithScene.AddMaterial(Mat);
			}
			foreach (FDatasmithFacadeTexture Texture in CreatedTextures)
			{
				DatasmithScene.AddTexture(Texture);
			}
		}

		private bool CreateAndCacheMaterial(FMaterial InMaterial, out List<FDatasmithFacadeTexture> OutCreatedTextures, out FDatasmithFacadeMaterialInstance OutCreatedMaterial)
		{
			Addin.Instance.LogDebug($"CreateAndCacheMaterial({InMaterial.Name}: {InMaterial.ID})");

			OutCreatedTextures = null;
			OutCreatedMaterial = null;

			if (ExportedMaterialsMap.ContainsKey(InMaterial.ID))
			{
				Addin.Instance.LogDebug($"  skipping, already present in map");
				return false;
			}

			Addin.Instance.LogDebug($"  making, ShaderName: '{InMaterial.ShaderName}'");

			FMaterial.EMaterialType Type = FMaterial.GetMaterialType(InMaterial.ShaderName);

			float Roughness = (float)InMaterial.Roughness;
			float Metallic = 0f;

			if (Type != FMaterial.EMaterialType.TYPE_LIGHTWEIGHT)
			{
				if (Type == FMaterial.EMaterialType.TYPE_METAL)
				{
					Metallic = 1f;
				}
				else if (Type == FMaterial.EMaterialType.TYPE_METALLICPAINT)
				{
					Metallic = 0.7f;
				}

				if (InMaterial.BlurryReflections)
				{
					Roughness = (float)InMaterial.SpecularSpread;
				}
				else
				{
					if (InMaterial.Reflectivity > 0.0)
					{
						Roughness = (1f - (float)InMaterial.Reflectivity) * 0.2f;
					}
					else
					{
						Roughness = 1f;
					}
				}
			}

			float Mult = (Type == FMaterial.EMaterialType.TYPE_LIGHTWEIGHT) ? (float)InMaterial.Diffuse : 1.0f;

			float R = Mult * InMaterial.PrimaryColor.R * 1.0f / 255.0f;
			float G = Mult * InMaterial.PrimaryColor.G * 1.0f / 255.0f;
			float B = Mult * InMaterial.PrimaryColor.B * 1.0f / 255.0f;

			FDatasmithFacadeMaterialInstance MaterialInstance = new FDatasmithFacadeMaterialInstance(InMaterial.Name);

			OutCreatedTextures = new List<FDatasmithFacadeTexture>();
			OutCreatedMaterial = MaterialInstance;

			MaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Opaque);
			MaterialInstance.AddColor("TintColor", R, G, B, 1.0F);
			MaterialInstance.AddFloat("RoughnessAmount", Roughness);

			if (InMaterial.Transparency > 0.0)
			{
				MaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Transparent);
				MaterialInstance.AddFloat("Metalness", Metallic);

				MaterialInstance.AddColor("OpacityAndRefraction",
					0.25f,                              // Opacity
					1.0f,                               // Refraction
					0.0f,                               // Refraction Exponent
					1f - (float)InMaterial.Transparency // Fresnel Opacity
				);

				FDatasmithFacadeTexture NormalMap = ExportNormalMap(InMaterial, MaterialInstance, "NormalMap");

				if (NormalMap != null)
				{
					OutCreatedTextures.Add(NormalMap);
				}
			}
			else
			{
				MaterialInstance.AddFloat("MetallicAmount", Metallic);

				if (InMaterial.Emission > 0.0)
				{
					MaterialInstance.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Emissive);
					MaterialInstance.AddFloat("LuminanceAmount", (float)InMaterial.Emission);
					MaterialInstance.AddColor("LuminanceFilter", R, G, B, 1.0f);
				}

				FDatasmithFacadeTexture DiffuseMap = ExportDiffuseMap(InMaterial, MaterialInstance, "ColorMap");
				FDatasmithFacadeTexture NormalMap = ExportNormalMap(InMaterial, MaterialInstance, "NormalMap");

				if (DiffuseMap != null)
				{
					OutCreatedTextures.Add(DiffuseMap);
				}
				if (NormalMap != null)
				{
					OutCreatedTextures.Add(NormalMap);
				}
			}

			ExportedMaterialsMap[InMaterial.ID] = MaterialInstance;

			return true;
		}

		public FActorName GetComponentActorName(FComponentName ComponentName)
		{
			return ComponentName.GetActorName();
		}

		public FActorName GetComponentActorName(Component2 Component)
		{
			return GetComponentActorName(new FComponentName(Component));
		}


		public void ExportAnimation(FAnimation InAnim)
		{
			FDatasmithFacadeLevelSequence LevelSeq = new FDatasmithFacadeLevelSequence(InAnim.Name);

			LevelSeq.SetFrameRate(InAnim.FPS);

			foreach (var NodePair in InAnim.ComponentToChannelMap)
			{
				FAnimation.FChannel Chan = NodePair.Value;
				Component2 Component = Chan.Target;

				FDatasmithFacadeTransformAnimation Anim = new FDatasmithFacadeTransformAnimation(GetComponentActorName(Component).GetString());

				foreach (var Keyframe in Chan.Keyframes)
				{
					FMatrix4 LocalMatrix = Keyframe.LocalMatrix;

					FVec3 Euler = MathUtils.ToEuler(LocalMatrix);

					float Scale = LocalMatrix[15];

					FVec3 Translation = new FVec3(LocalMatrix[12], LocalMatrix[13], LocalMatrix[14]);

					Anim.AddFrame(EDatasmithFacadeAnimationTransformType.Rotation, Keyframe.Step, -Euler.X, Euler.Y, -Euler.Z);
					Anim.AddFrame(EDatasmithFacadeAnimationTransformType.Scale, Keyframe.Step, Scale, Scale, Scale);
					Anim.AddFrame(EDatasmithFacadeAnimationTransformType.Translation, Keyframe.Step, Translation.X, -Translation.Y, Translation.Z);
				}

				LevelSeq.AddAnimation(Anim);
			}

			// Check if we already have a sequence with the same name and remove it if we do
			int SequencesSetsCount = DatasmithScene.GetLevelSequencesCount();
			for (int Index = 0; Index < SequencesSetsCount; ++Index)
			{
				FDatasmithFacadeLevelSequence ExistingSeq = DatasmithScene.GetLevelSequence(Index);

				if (ExistingSeq.GetName() == LevelSeq.GetName())
				{
					DatasmithScene.RemoveLevelSequence(ExistingSeq);
					break;
				}
			}

			DatasmithScene.AddLevelSequence(LevelSeq);
		}

		private FDatasmithFacadeTexture ExportDiffuseMap(FMaterial InMaterial, FDatasmithFacadeMaterialInstance InMaterialInstance, string InParamName)
		{
			if (!string.IsNullOrEmpty(InMaterial.Texture) && !File.Exists(InMaterial.Texture))
			{
				InMaterial.Texture = MaterialUtils.ComputeAssemblySideTexturePath(InMaterial.Texture);
			}

			FDatasmithFacadeTexture TextureElement = null;

			if (!string.IsNullOrEmpty(InMaterial.Texture) && File.Exists(InMaterial.Texture))
			{
				string TextureName = SanitizeName(Path.GetFileNameWithoutExtension(InMaterial.Texture));

				if (!ExportedTexturesMap.TryGetValue(InMaterial.Texture, out TextureElement))
				{
					TextureElement = new FDatasmithFacadeTexture(TextureName);
					TextureElement.SetFile(InMaterial.Texture);
					TextureElement.SetTextureFilter(FDatasmithFacadeTexture.ETextureFilter.Default);
					TextureElement.SetRGBCurve(1);
					TextureElement.SetTextureAddressX(FDatasmithFacadeTexture.ETextureAddress.Wrap);
					TextureElement.SetTextureAddressY(FDatasmithFacadeTexture.ETextureAddress.Wrap);
					FDatasmithFacadeTexture.ETextureMode TextureMode = FDatasmithFacadeTexture.ETextureMode.Diffuse;
					TextureElement.SetTextureMode(TextureMode);
					ExportedTexturesMap.TryAdd(InMaterial.Texture, TextureElement);

					InMaterialInstance.AddTexture(InParamName, TextureElement);
				}
			}

			return TextureElement;
		}

		private FDatasmithFacadeTexture ExportNormalMap(FMaterial InMaterial, FDatasmithFacadeMaterialInstance InMaterialInstance, string InParamName)
		{
			if (!string.IsNullOrEmpty(InMaterial.BumpTextureFileName) && !File.Exists(InMaterial.BumpTextureFileName))
			{
				InMaterial.BumpTextureFileName = MaterialUtils.ComputeAssemblySideTexturePath(InMaterial.BumpTextureFileName);
			}

			FDatasmithFacadeTexture TextureElement = null;

			if (!string.IsNullOrEmpty(InMaterial.BumpTextureFileName) && File.Exists(InMaterial.BumpTextureFileName))
			{
				string textureName = SanitizeName(Path.GetFileNameWithoutExtension(InMaterial.BumpTextureFileName));

				if (!ExportedTexturesMap.TryGetValue(InMaterial.BumpTextureFileName, out TextureElement))
				{
					TextureElement = new FDatasmithFacadeTexture(textureName);
					TextureElement.SetFile(InMaterial.BumpTextureFileName);
					TextureElement.SetTextureFilter(FDatasmithFacadeTexture.ETextureFilter.Default);
					TextureElement.SetRGBCurve(1);
					TextureElement.SetTextureAddressX(FDatasmithFacadeTexture.ETextureAddress.Wrap);
					TextureElement.SetTextureAddressY(FDatasmithFacadeTexture.ETextureAddress.Wrap);
					FDatasmithFacadeTexture.ETextureMode TextureMode = FDatasmithFacadeTexture.ETextureMode.Normal;
					TextureElement.SetTextureMode(TextureMode);
					ExportedTexturesMap.TryAdd(InMaterial.BumpTextureFileName, TextureElement);

					InMaterialInstance.AddTexture(InParamName, TextureElement);
				}
			}

			return TextureElement;
		}

		public static string SanitizeName(string InStringToSanitize)
		{
			const string Original = "^/()#$&.?!ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿБбВвГгДдЁёЖжЗзИиЙйКкЛлМмНнОоПпРрСсТтУуФфХхЦцЧчШшЩщЪъЫыЬьЭэЮюЯя'\"";
			const string Modified = "_____S____AAAAAAECEEEEIIIIDNOOOOOx0UUUUYPsaaaaaaeceeeeiiiiOnoooood0uuuuypyBbVvGgDdEeJjZzIiYyKkLlMmNnOoPpRrSsTtUuFfJjTtCcSsSs__ii__EeYyYy__";

			string Result = "";
			for (int i = 0; i < InStringToSanitize.Length; i++)
			{
				if (InStringToSanitize[i] <= 32)
				{
					Result += '_';
				}
				else
				{
					bool bReplaced = false;
					for (int j = 0; j < Original.Length; j++)
					{
						if (InStringToSanitize[i] == Original[j])
						{
							Result += Modified[j];
							bReplaced = true;
							break;
						}
					}
					if (!bReplaced)
					{
						Result += InStringToSanitize[i];
					}
				}
			}
			return Result;
		}

		private FMatrix4 AdjustTransformForDatasmith(float[] InXForm)
		{
			FMatrix4 RotMatrix = FMatrix4.FromRotationX(-90f);
			if (InXForm != null)
			{
				FMatrix4 Mat = new FMatrix4(InXForm);
				Mat = Mat * RotMatrix;
				return Mat;
			}
			return RotMatrix;
		}

		// Get Datasmith Name for MeshName
		// todo: Currently, name for the DatasmithMeshElement(as well as other elements) is computed within DatasmithCore's FDatasmithUtils::SanitizeObjectName
		//   if we want to properly identify an element we need to construct it(or SetName) and then GetName from it.
		//   The way to make element name ahead of constructing and element(e.g. to properly make unique names for every case) is to expose FDatasmithUtils::SanitizeObjectName
		public string GetDatasmithMeshName(FMeshName MeshName)  
		{
			if (ExportedMeshesMap.TryGetValue(MeshName, out FDatasmithFacadeMeshElement MeshElement))
			{
				return MeshElement.GetName();
			}

			return null;
		}

		// Export meshes
		public void ExportMeshes(List<FMeshExportInfo> MeshExportInfos, out List<FMeshExportInfo> OutCreatedMeshes)
		{
			Addin.Instance.LogDebug($"ExportMeshes: {string.Join(", ", MeshExportInfos)}");

			ConcurrentQueue<FMeshExportInfo> CreatedMeshes = new ConcurrentQueue<FMeshExportInfo>();

			Parallel.ForEach(MeshExportInfos, Info =>
			{
				if (ExportMesh(Info.MeshName, Info.MeshData, out Info.MeshElement))
				{
					CreatedMeshes.Enqueue(Info);
				}
			});

			// Remember exported meshes and assign to actor
			foreach (FMeshExportInfo Info in CreatedMeshes)
			{
				Addin.Instance.LogDebug($"  AddMesh(DatasmithMeshName: {Info}");
				// Info.MeshElement should be valid here
				AddMesh(Info.MeshName, Info.MeshElement);

				if (Info.ActorName.IsValid())
				{
					Tuple<EActorType, FDatasmithFacadeActor> ExportedActorInfo = null;
					if (ExportedActorsMap.TryGetValue(Info.ActorName, out ExportedActorInfo) && ExportedActorInfo.Item1 == EActorType.MeshActor)
					{
						FDatasmithFacadeActorMesh MeshActor = ExportedActorInfo.Item2 as FDatasmithFacadeActorMesh;
						Addin.Instance.LogDebug($"  SetMesh on MeshActor: {MeshActor.GetName()}: {MeshActor.GetLabel()}  )");
						MeshActor.SetMesh(Info.MeshElement.GetName());
					}
				}
			}

			OutCreatedMeshes = CreatedMeshes.ToList();
		}
	}
}