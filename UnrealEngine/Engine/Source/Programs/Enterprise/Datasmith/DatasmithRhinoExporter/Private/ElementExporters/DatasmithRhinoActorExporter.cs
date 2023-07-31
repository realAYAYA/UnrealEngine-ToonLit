// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.ExportContext;
using DatasmithRhino.Properties.Localization;
using DatasmithRhino.Utils;

using Rhino;
using Rhino.DocObjects;
using Rhino.Geometry;
using System.Collections.Generic;
using System.Collections.Specialized;

namespace DatasmithRhino.ElementExporters
{
	class DatasmithRhinoActorExporter : IDatasmithRhinoElementExporter<DatasmithRhinoActorExporter, DatasmithActorInfo>
	{
		///// BEGIN IDatasmithRhinoElementExporter Interface /////

		protected override int GetElementsToSynchronizeCount()
		{
			return ExportContext.SceneRoot.GetDescendantsCount();
		}

		protected override IEnumerable<DatasmithActorInfo> GetElementsToSynchronize()
		{
			// We don't export the root, only its descendants.
			return ExportContext.SceneRoot.GetDescendantEnumerator();
		}

		protected override FDatasmithFacadeElement CreateElement(DatasmithActorInfo ElementInfo)
		{
			FDatasmithFacadeActor CreatedActor = CreateActor(ElementInfo);

			if (CreatedActor != null)
			{
				SyncActor(ElementInfo, CreatedActor);
			}

			return CreatedActor;
		}

		protected override void AddElement(DatasmithActorInfo ElementInfo)
		{
			AddActorToParent(ElementInfo.DatasmithActor, ElementInfo, DatasmithScene);
		}

		protected override void ModifyElement(DatasmithActorInfo ElementInfo)
		{
			SyncActor(ElementInfo, ElementInfo.DatasmithActor);
			SyncActorWithParent(ElementInfo.DatasmithActor, ElementInfo, DatasmithScene);
		}

		protected override void DeleteElement(DatasmithActorInfo ElementInfo)
		{
			FDatasmithFacadeMetaData DatasmithMetaData = DatasmithScene.GetMetaData(ElementInfo.DatasmithActor);
			if (DatasmithMetaData != null)
			{
				DatasmithScene.RemoveMetaData(DatasmithMetaData);
			}

			FDatasmithFacadeActor ParentActor = ElementInfo.DatasmithActor.GetParentActor();
			if (ParentActor != null)
			{
				ParentActor.RemoveChild(ElementInfo.DatasmithActor);
			}
			else
			{
				//The actor is directly under the scene root.
				//No need to reparent the children here, ModifyElement() will be called for them if needed, so we can use EActorRemovalRule.RemoveChildren which is faster.
				DatasmithScene.RemoveActor(ElementInfo.DatasmithActor, FDatasmithFacadeScene.EActorRemovalRule.RemoveChildren);
			}

		}

		///// END IDatasmithRhinoElementExporter Interface /////

		private FDatasmithFacadeActor CreateActor(DatasmithActorInfo Node)
		{
			FDatasmithFacadeActor ExportedActor = null;

			if (Node.RhinoCommonObject is RhinoObject CurrentObject)
			{
				bool bExportEmptyActor = false;

				if (CurrentObject.ObjectType == ObjectType.InstanceReference
					|| CurrentObject.ObjectType == ObjectType.Point)
				{
					// The Instance Reference node is exported as an empty actor under which we create the instanced block.
					// Export points as empty actors as well.
					ExportedActor = CreateEmptyActor(Node);
				}
				else if (CurrentObject.ObjectType == ObjectType.Light)
				{
					ExportedActor = CreateLightActor(Node);

					if (ExportedActor == null)
					{
						bExportEmptyActor = true;
					}
				}
				else if (ExportContext.ObjectIdToMeshInfoDictionary.ContainsKey(CurrentObject.Id))
				{
					// If the node's object has a mesh associated to it, export it as a MeshActor.
					ExportedActor = CreateMeshActor(Node);
				}
				
				if (ExportedActor == null)
				{
					if(bExportEmptyActor)
					{
						ExportedActor = CreateEmptyActor(Node);
					}
					else
					{
						DatasmithRhinoPlugin.Instance.LogManager.AddLog(DatasmithRhinoLogType.Info, string.Format(Resources.UnsupportedActor, CurrentObject.Id, CurrentObject.ObjectType));
					}
				}
			}
			else if(Node is DatasmithCameraActorInfo ActorCameraInfo)
			{
				ExportedActor = CreateCameraActor(ActorCameraInfo);
			}
			else if(!Node.bIsRoot)
			{
				// This node has no RhinoObject (likely a layer), export an empty Actor.
				ExportedActor = CreateEmptyActor(Node);
			}

			return ExportedActor;
		}

		private void SyncActor(DatasmithActorInfo Node, FDatasmithFacadeActor DatasmithActor)
		{
			switch(DatasmithActor)
			{
				case FDatasmithFacadeActorLight LightActor:
					SyncLightActor(Node, LightActor);
					break;
				case FDatasmithFacadeActorMesh MeshActor:
					RhinoObject CurrentObject = Node.RhinoCommonObject as RhinoObject;
					if (ExportContext.ObjectIdToMeshInfoDictionary.TryGetValue(CurrentObject.Id, out DatasmithMeshInfo MeshInfo))
					{
						SyncMeshActor(MeshActor, Node, MeshInfo);
					}
					break;
				case FDatasmithFacadeActorCamera CameraActor:
					SyncCameraActor(Node, CameraActor);
					break;
				default:
					SyncEmptyActor(Node, DatasmithActor);
					break;
			}

			SyncTags(DatasmithActor, Node);
			SyncMetaData(DatasmithActor, Node, DatasmithScene);
			DatasmithActor.SetLayer(GetLayers(Node));
		}

		private static FDatasmithFacadeActorMesh CreateMeshActor(DatasmithActorInfo InNode)
		{
			string HashedActorName = FDatasmithFacadeActor.GetStringHash("A:" + InNode.Name);
			return new FDatasmithFacadeActorMesh(HashedActorName);
		}

		private void SyncMeshActor(FDatasmithFacadeActorMesh DatasmithMeshActor, DatasmithActorInfo InNode, DatasmithMeshInfo InMeshInfo)
		{
			DatasmithMeshActor.SetLabel(InNode.UniqueLabel);

			Transform OffsetTransform = InMeshInfo.OffsetTransform;
			Transform WorldTransform = Transform.Multiply(InNode.WorldTransform, OffsetTransform);
			DatasmithMeshActor.SetWorldTransform(WorldTransform.ToFloatArray(false));

			string MeshName = FDatasmithFacadeElement.GetStringHash(InMeshInfo.Name);
			DatasmithMeshActor.SetMesh(MeshName);

			SyncMeshActorMaterialOverrides(DatasmithMeshActor, InNode);
		}

		private void SyncMeshActorMaterialOverrides(FDatasmithFacadeActorMesh DatasmithMeshActor, DatasmithActorInfo InNode)
		{
			if (InNode.bOverrideMaterial)
			{
				DatasmithMaterialInfo MaterialInfo = ExportContext.GetMaterialInfoFromMaterialIndex(InNode.MaterialIndex);
				if (DatasmithMeshActor.GetMaterialOverridesCount() > 0)
				{
					FDatasmithFacadeMaterialID MaterialID = DatasmithMeshActor.GetMaterialOverride(0);
					if (MaterialID.GetName() != MaterialInfo.Name)
					{
						DatasmithMeshActor.RemoveMaterialOverride(MaterialID);
					}
				}

				if (DatasmithMeshActor.GetMaterialOverridesCount() == 0)
				{
					DatasmithMeshActor.AddMaterialOverride(MaterialInfo.Name, 0);
				}
			}
			else
			{
				DatasmithMeshActor.ResetMaterialOverrides();
			}
		}

		private static FDatasmithFacadeActor CreateEmptyActor(DatasmithActorInfo InNode)
		{
			string HashedName = FDatasmithFacadeElement.GetStringHash(InNode.Name);
			return new FDatasmithFacadeActor(HashedName);
		}

		private static void SyncEmptyActor(DatasmithActorInfo InNode, FDatasmithFacadeActor DatasmithActor)
		{
			DatasmithActor.SetLabel(InNode.UniqueLabel);

			float[] MatrixArray = InNode.WorldTransform.ToFloatArray(false);
			DatasmithActor.SetWorldTransform(MatrixArray);
		}

		private static FDatasmithFacadeActorLight CreateLightActor(DatasmithActorInfo InNode)
		{
			LightObject RhinoLightObject = InNode.RhinoCommonObject as LightObject;

			string HashedName = FDatasmithFacadeElement.GetStringHash(InNode.Name);
			switch (RhinoLightObject.LightGeometry.LightStyle)
			{
				case LightStyle.CameraSpot:
				case LightStyle.WorldSpot:
					return new FDatasmithFacadeSpotLight(HashedName);
				case LightStyle.WorldLinear:
				case LightStyle.WorldRectangular:
					return new FDatasmithFacadeAreaLight(HashedName);
				case LightStyle.CameraDirectional:
				case LightStyle.WorldDirectional:
					return new FDatasmithFacadeDirectionalLight(HashedName);
				case LightStyle.CameraPoint:
				case LightStyle.WorldPoint:
					return new FDatasmithFacadePointLight(HashedName);
				case LightStyle.Ambient: // not supported as light
				default:
					return null;
			}
		}

		private static FDatasmithFacadeActorCamera CreateCameraActor(DatasmithCameraActorInfo InCameraInfo)
		{
			string HashedName = FDatasmithFacadeElement.GetStringHash(InCameraInfo.Name);
			return new FDatasmithFacadeActorCamera(HashedName);
		}

		private static void SyncLightActor(DatasmithActorInfo InNode, FDatasmithFacadeActorLight LightActor)
		{
			LightObject RhinoLightObject = InNode.RhinoCommonObject as LightObject;
			Light RhinoLight = RhinoLightObject.LightGeometry;

			if (RhinoLight.LightStyle == LightStyle.CameraSpot || RhinoLight.LightStyle == LightStyle.WorldSpot)
			{
				FDatasmithFacadeSpotLight SpotLightElement = LightActor as FDatasmithFacadeSpotLight;
				double OuterSpotAngle = DatasmithRhinoUtilities.RadianToDegree(RhinoLight.SpotAngleRadians);
				double InnerSpotAngle = RhinoLight.HotSpot * OuterSpotAngle;

				SpotLightElement.SetOuterConeAngle((float)OuterSpotAngle);
				SpotLightElement.SetInnerConeAngle((float)InnerSpotAngle);

			}
			else if (RhinoLight.LightStyle == LightStyle.WorldLinear || RhinoLight.LightStyle == LightStyle.WorldRectangular)
			{
				FDatasmithFacadeAreaLight AreaLightElement = LightActor as FDatasmithFacadeAreaLight;
				double Length = RhinoLight.Length.Length;
				AreaLightElement.SetLength((float)Length);

				if (RhinoLight.IsRectangularLight)
				{
					double Width = RhinoLight.Width.Length;

					AreaLightElement.SetWidth((float)Width);
					AreaLightElement.SetLightShape(FDatasmithFacadeAreaLight.EAreaLightShape.Rectangle);
					AreaLightElement.SetLightType(FDatasmithFacadeAreaLight.EAreaLightType.Rect);
				}
				else
				{
					AreaLightElement.SetWidth((float)(0.01f * Length));
					AreaLightElement.SetLightShape(FDatasmithFacadeAreaLight.EAreaLightShape.Cylinder);
					AreaLightElement.SetLightType(FDatasmithFacadeAreaLight.EAreaLightType.Point);
					// The light in Rhino doesn't have attenuation, but the attenuation radius was found by testing in Unreal to obtain a visual similar to Rhino
					float DocumentScale = (float)Rhino.RhinoMath.UnitScale(Rhino.RhinoDoc.ActiveDoc.ModelUnitSystem, UnitSystem.Centimeters);
					AreaLightElement.SetAttenuationRadius(1800f / DocumentScale);
				}
			}

			System.Drawing.Color DiffuseColor = RhinoLight.Diffuse;
			LightActor.SetColor(DiffuseColor.R, DiffuseColor.G, DiffuseColor.B, DiffuseColor.A);
			LightActor.SetIntensity(RhinoLight.Intensity * 100f);
			LightActor.SetEnabled(RhinoLight.IsEnabled);
			LightActor.SetLabel(InNode.UniqueLabel);

			FDatasmithFacadePointLight PointLightElement = LightActor as FDatasmithFacadePointLight;
			if (PointLightElement != null)
			{
				PointLightElement.SetIntensityUnits(FDatasmithFacadePointLight.EPointLightIntensityUnit.Candelas);
			}

			float[] MatrixArray = InNode.WorldTransform.ToFloatArray(false);
			LightActor.SetWorldTransform(MatrixArray);
		}

		private static void SyncCameraActor(DatasmithActorInfo ActorInfo, FDatasmithFacadeActorCamera CameraActor)
		{
			ViewportInfo Viewport = ActorInfo.RhinoCommonObject as ViewportInfo;
			CameraActor.SetLabel(ActorInfo.UniqueLabel);

			CameraActor.SetAspectRatio((float)Viewport.FrustumAspect);
			CameraActor.SetFocalLength((float)Viewport.Camera35mmLensLength);

			CameraActor.SetCameraPosition((float)Viewport.CameraLocation.X, (float)Viewport.CameraLocation.Y, (float)Viewport.CameraLocation.Z);
			CameraActor.SetCameraRotation((float)Viewport.CameraDirection.X, (float)Viewport.CameraDirection.Y, (float)Viewport.CameraDirection.Z,
				(float)Viewport.CameraUp.X, (float)Viewport.CameraUp.Y, (float)Viewport.CameraUp.Z);
		}

		private static void AddActorToParent(FDatasmithFacadeActor InDatasmithActor, DatasmithActorInfo InNode, FDatasmithFacadeScene InDatasmithScene)
		{
			if (InNode.Parent.bIsRoot)
			{
				InDatasmithScene.AddActor(InDatasmithActor);
			}
			else
			{
				InNode.Parent.DatasmithActor.AddChild(InDatasmithActor);
			}
		}

		private static void SyncActorWithParent(FDatasmithFacadeActor InDatasmithActor, DatasmithActorInfo InNode, FDatasmithFacadeScene InDatasmithScene)
		{
			FDatasmithFacadeActor ParentActor = InDatasmithActor.GetParentActor();
			bool bNeedParentUpdate = false;

			if (ParentActor != null)
			{
				string HashedParentActorName = FDatasmithFacadeActor.GetStringHash("A:" + InNode.Parent.Name);
				if (ParentActor.GetName() != HashedParentActorName)
				{
					//Parent changed, remove actor from previous parent
					ParentActor.RemoveChild(InDatasmithActor);
					bNeedParentUpdate = true;
				}
			}
			else if (!InNode.Parent.bIsRoot)
			{
				//Parent actor was directly under scene root, but the actor has been moved and should be reparented.
				InDatasmithScene.RemoveActor(InDatasmithActor, FDatasmithFacadeScene.EActorRemovalRule.RemoveChildren);
				bNeedParentUpdate = true;
			}

			if(bNeedParentUpdate)
			{
				AddActorToParent(InDatasmithActor, InNode, InDatasmithScene);
			}
		}

		private void SyncTags(FDatasmithFacadeActor InDatasmithActor, DatasmithActorInfo InNode)
		{
			if (!InNode.bIsRoot)
			{
				List<string> Tags = InNode.GetTags(ExportContext);
				InDatasmithActor.ResetTags();
				foreach (string CurrentTag in Tags)
				{
					InDatasmithActor.AddTag(CurrentTag);
				}
			}
		}

		private static void SyncMetaData(FDatasmithFacadeActor InDatasmithActor, DatasmithActorInfo InNode, FDatasmithFacadeScene InDatasmithScene)
		{
			FDatasmithFacadeMetaData DatasmithMetaData = InDatasmithScene.GetMetaData(InDatasmithActor);
			bool bHasMetaData = false;

			if (InNode.RhinoCommonObject is RhinoObject NodeObject)
			{
				NameValueCollection UserStrings = NodeObject.Attributes.GetUserStrings();

				if (UserStrings != null && UserStrings.Count > 0)
				{
					if(DatasmithMetaData == null)
					{
						DatasmithMetaData = new FDatasmithFacadeMetaData(InDatasmithActor.GetName() + "_DATA");
						DatasmithMetaData.SetAssociatedElement(InDatasmithActor);
						InDatasmithScene.AddMetaData(DatasmithMetaData);
					}

					DatasmithMetaData.SetLabel(InDatasmithActor.GetLabel());

					//Update or remove existing properties.
					List<string> KeysToSyncList = new List<string>(UserStrings.AllKeys);
					for (int PropertyIndex = DatasmithMetaData.GetPropertiesCount() - 1; PropertyIndex >= 0; --PropertyIndex)
					{
						FDatasmithFacadeKeyValueProperty Property = DatasmithMetaData.GetProperty(PropertyIndex);
						string PropertyName = Property.GetName();

						if (KeysToSyncList.Contains(PropertyName))
						{
							// Property exists, update its value.
							string EvaluatedValue = DatasmithRhinoUtilities.EvaluateAttributeUserText(InNode, UserStrings.Get(PropertyName));
							Property.SetValue(EvaluatedValue);
							KeysToSyncList.Remove(PropertyName);
						}
						else
						{
							DatasmithMetaData.RemoveProperty(Property);
						}
					}

					// Add new properties.
					bHasMetaData = KeysToSyncList.Count > 0;
					for (int KeyIndex = 0; KeyIndex < KeysToSyncList.Count; ++KeyIndex)
					{
						string CurrentKey = KeysToSyncList[KeyIndex];
						string EvaluatedValue = DatasmithRhinoUtilities.EvaluateAttributeUserText(InNode, UserStrings.Get(CurrentKey));

						DatasmithMetaData.AddPropertyString(CurrentKey, EvaluatedValue);
					}
				}
			}

			if (!bHasMetaData && DatasmithMetaData != null)
			{
				InDatasmithScene.RemoveMetaData(DatasmithMetaData);
			}
		}

		private string GetLayers(DatasmithActorInfo InNode)
		{
			bool bIsSameAsParentLayer =
				!(InNode.RhinoCommonObject is Layer
					|| (InNode.Parent.bIsRoot && InNode.RhinoCommonObject == null) //This is a dummy document layer.
					|| (InNode.Parent.RhinoCommonObject as RhinoObject)?.ObjectType == ObjectType.InstanceReference);

			if (bIsSameAsParentLayer && InNode.Parent?.DatasmithActor != null)
			{
				return InNode.Parent.DatasmithActor.GetLayer();
			}
			else
			{
				return ExportContext.GetNodeLayerString(InNode);
			}
		}
	}

}