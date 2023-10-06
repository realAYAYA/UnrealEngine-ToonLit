// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.Addin;
using static DatasmithSolidworks.FAssemblyDocumentTracker;

namespace DatasmithSolidworks
{

	public class FAssemblyLazyUpdateChecker: FLazyUpdateCheckerBase
	{
		private readonly FAssemblyDocumentTracker AsmDocumentTracker;

		private bool bHasDirtyMaterials;
		private bool bHasDirtyComponents;
		HashSet<FComponentName> AllExportedComponents;

		public FAssemblyLazyUpdateChecker(FAssemblyDocumentTracker InDocumentTracker): base(InDocumentTracker)
		{
			AsmDocumentTracker = InDocumentTracker;
		}

		public override bool HasUpdates()
		{
			return bHasDirtyComponents || bHasDirtyMaterials;
		}

		public override void Restart()
		{
			AllExportedComponents = new HashSet<FComponentName>();
			bHasDirtyMaterials = false;
			bHasDirtyComponents = false;
		}

		public override IEnumerable<bool> CheckForModificationsEnum()
		{
			FSyncState SyncState = AsmDocumentTracker.SyncState;

			// Dig into part level materials (they wont be read by LoadDocumentMaterials)
			AllExportedComponents.UnionWith(SyncState.CleanComponents);
			AllExportedComponents.UnionWith(SyncState.DirtyComponents.Keys);

			Dictionary<int, FMaterial> MaterialsMap = new Dictionary<int, FMaterial>();
			Dictionary<FComponentName, FObjectMaterials> CurrentDocMaterialsMap  = new Dictionary<FComponentName, FObjectMaterials>();

			List<FComponentName> InvalidComponents = new List<FComponentName>();
			foreach (bool _ in FObjectMaterials.LoadAssemblyMaterialsEnum(AsmDocumentTracker, AllExportedComponents, CurrentDocMaterialsMap, MaterialsMap, InvalidComponents))
			{
				yield return true;
			}

			// Workaround for lack of notifications for some types of component deletion:
			//   - a component was deleted which is internal to a subassembly of another assembly. As opposed to a component representing subassembly instance in the parent assembly which is notified when deleted.
			//      in UI: right-click on such a subcomponent, select Delete and dialog should appear whether to delete the whole subassembly or the component in this subassembly. Select deleting just the component
			foreach (FComponentName ComponentName in InvalidComponents)
			{
				AsmDocumentTracker.ComponentDeleted(ComponentName);	
			}

			HashSet<FComponentName> Components =  SyncState.ComponentsMaterialsMap == null ? new HashSet<FComponentName>() : new HashSet<FComponentName>(SyncState.ComponentsMaterialsMap.Keys);
			HashSet<FComponentName> CurrentComponents =  CurrentDocMaterialsMap == null ? new HashSet<FComponentName>() : new HashSet<FComponentName>(CurrentDocMaterialsMap.Keys);

			// Components which stayed in the materials map
			HashSet<FComponentName> CommonComponents = new HashSet<FComponentName>(CurrentComponents.Intersect(Components));
			 
			IEnumerable<FComponentName> ComponentsWithAddedOrRemovedMaterial = Components.Union(CurrentComponents).Except(CommonComponents);
			foreach (FComponentName CompName in ComponentsWithAddedOrRemovedMaterial)
			{
				bool bShouldSyncComponentMaterial = false;

				if (SyncState.CollectedComponentsMap.ContainsKey(CompName))
				{
					Component2 Comp = SyncState.CollectedComponentsMap[CompName];
					bShouldSyncComponentMaterial = !Comp.IsSuppressed() &&
					                               (Comp.Visible == (int)swComponentVisibilityState_e
						                               .swComponentVisible);
					yield return true;
				}

				if (bShouldSyncComponentMaterial)
				{
					bHasDirtyMaterials = true;
					AsmDocumentTracker.SetComponentDirty(CompName, EComponentDirtyState.Material);
				}
			}

			// Check if components have their material modified
			foreach (FComponentName ComponentName in CommonComponents)
			{
				if (!SyncState.ComponentsMaterialsMap[ComponentName]
					    .EqualMaterials(CurrentDocMaterialsMap[ComponentName]))
				{
					AsmDocumentTracker.SetComponentDirty(ComponentName, EComponentDirtyState.Material);
					bHasDirtyComponents = true;
					yield return true;
				}
			}
		}
	};

	public class FAssemblyDocumentTracker : FDocumentTracker
	{
		public enum EComponentDirtyState
		{
			Geometry,
			Visibility,
			Material,
			Transform,
			Delete
		};

		public class FSyncState
		{
			public Dictionary<string, FPartDocument> PartsMap = new Dictionary<string, FPartDocument>();

			public Dictionary<FComponentName, FObjectMaterials> ComponentsMaterialsMap = null;

			public Dictionary<FComponentName, FConvertedTransform> ComponentsTransformsMap =
				new Dictionary<FComponentName, FConvertedTransform>();

			/// All updated(not dirty) components. Updated component will be removed from dirty on export end
			public HashSet<FComponentName> CleanComponents = new HashSet<FComponentName>();

			/// Flags for each component that needs update. Cleared when export completes*/
			public Dictionary<FComponentName, uint> DirtyComponents = new Dictionary<FComponentName, uint>();

			public HashSet<FComponentName> ComponentsToDelete = new HashSet<FComponentName>();

			/// all components in the document
			public Dictionary<FComponentName, Component2> CollectedComponentsMap = new Dictionary<FComponentName, Component2>();

			/** Stores which mesh was exported for each component*/
			public Dictionary<FComponentName, List<FMeshName>> ComponentNameToMeshNameMap =
				new Dictionary<FComponentName, List<FMeshName>>();
			public Dictionary<FMeshName, HashSet<FComponentName>> ComponentsForMesh =
				new Dictionary<FMeshName, HashSet<FComponentName>>();

			public FMeshes Meshes = null;
		}

		public AssemblyDoc SwAsmDoc { get; private set; } = null;

		public FSyncState SyncState { get; private set; } = new FSyncState();

		private FAssemblyLazyUpdateChecker LazyUpdateChecker;

		public FAssemblyDocumentTracker(FAssemblyDocument InDoc, AssemblyDoc InSwDoc, FDatasmithExporter InExporter) : base(InDoc, InExporter)
		{
			SwAsmDoc = InSwDoc;
		}

		public override void Destroy()
		{
			// Release all contained parts
			foreach (FPartDocument PartDocument in SyncState.PartsMap.Values)
			{
				PartDocument.Destroy();
			}

			base.Destroy();
		}

		public override FMeshes GetMeshes(string ActiveConfigName)
		{
			return SyncState.Meshes ?? (SyncState.Meshes = new FMeshes(ActiveConfigName));
		}

		// Export to datasmith scene extracted configurations data
		public override void ExportToDatasmithScene(FConfigurationExporter ConfigurationExporter, FVariantName ActiveVariantName)
		{
			SetExportStatus("Actors");

			ProcessComponentsPendingDelete();

			Configuration CurrentConfig = SwDoc.GetActiveConfiguration() as Configuration;

			// Configurations combined tree should have single child(root component of each config is the same)
			Debug.Assert(ConfigurationExporter.CombinedTree.Children.Count == 1);
			ExportComponentRecursive(ActiveVariantName, ConfigurationExporter,
				ConfigurationExporter.CombinedTree.Children[0], null);
			SyncState.DirtyComponents.Clear();

			ConfigurationExporter.FinalizeExport(this);

			// todo: check animation export for DL
			// Export animations (only allow when exporting to file)
			{
				Component2 Root = CurrentConfig.GetRootComponent3(true);
				List<FAnimation> Animations = FAnimationExtractor.ExtractAnimations(SwAsmDoc, Root);
				if (Animations != null)
				{
					SetExportStatus($"Animations");

					foreach (FAnimation Anim in Animations)
					{
						Exporter.ExportAnimation(Anim);
					}
				}
			}
		}

		private void ProcessComponentsPendingDelete()
		{
			foreach (FComponentName CompName in SyncState.ComponentsToDelete)
			{
				FActorName ActorName = Exporter.GetComponentActorName(CompName);
				SyncState.CollectedComponentsMap.Remove(CompName);

				ReleaseComponentMeshes(CompName);

				SyncState.ComponentsMaterialsMap?.Remove(CompName);
				SyncState.ComponentsTransformsMap.Remove(CompName);
				Exporter.RemoveActor(ActorName);
			}

			SyncState.ComponentsToDelete.Clear();
		}

		public override void ReleaseComponentMeshes(FComponentName CompName)
		{
			if (SyncState.ComponentNameToMeshNameMap.TryGetValue(CompName, out List<FMeshName> MeshNames))
			{
				SyncState.ComponentNameToMeshNameMap.Remove(CompName);

				foreach (FMeshName MeshName in MeshNames)
				{
					HashSet<FComponentName> ComponentNames = SyncState.ComponentsForMesh[MeshName];
					ComponentNames.Remove(CompName);
					if (ComponentNames.Count == 0)
					{
						SyncState.ComponentsForMesh.Remove(MeshName);
					}
				}
			}
		}
		public override void CleanupComponentMeshes()
		{
			// Filter meshes without components using them
			List<FMeshName> MeshesToRemove = (SyncState.ComponentsForMesh.Where(KVP => KVP.Value.Count == 0).Select(KVP => KVP.Key)).ToList();

			LogDebug("Removing unused meshes:");
			foreach (FMeshName MeshName in MeshesToRemove)
			{
				LogIndent();
				Exporter.RemoveMesh(MeshName);
				LogDedent();

				SyncState.ComponentsForMesh.Remove(MeshName);
			}
		}

		public override Dictionary<FComponentName, FObjectMaterials> LoadDocumentMaterials(
			HashSet<FComponentName> ComponentNamesToExportSet)
		{
			return FObjectMaterials.LoadAssemblyMaterials(this, ComponentNamesToExportSet,
				swDisplayStateOpts_e.swThisDisplayState, null);
		}

		public override void AddComponentMaterials(FComponentName ComponentName, FObjectMaterials Materials)
		{
			LogDebug($"AddComponentMaterials: {ComponentName} - {Materials}");

			if (SyncState.ComponentsMaterialsMap == null)
			{
				SyncState.ComponentsMaterialsMap = new Dictionary<FComponentName, FObjectMaterials>();
			}
			SyncState.ComponentsMaterialsMap[ComponentName] = Materials;
		}

		public override void AddMeshForComponent(FComponentName ComponentName, FMeshName MeshName)
		{
			SyncState.ComponentNameToMeshNameMap.FindOrAdd(ComponentName).Add(MeshName);
			SyncState.ComponentsForMesh.FindOrAdd(MeshName).Add(ComponentName);
		}

		public override FObjectMaterials GetComponentMaterials(Component2 Comp)
		{
			FObjectMaterials ComponentMaterials = null;
			SyncState.ComponentsMaterialsMap?.TryGetValue(new FComponentName(Comp), out ComponentMaterials);
			return ComponentMaterials;
		}

		public override FMeshData ExtractComponentMeshData(Component2 Comp)
		{
			FObjectMaterials ComponentMaterials = GetComponentMaterials(Comp);
			ConcurrentBag<FBody> Bodies = FBody.FetchBodies(Comp);
			FMeshData MeshData = FStripGeometry.CreateMeshData(Bodies, ComponentMaterials);
			return MeshData;
		}

		public FConvertedTransform GetComponentDatasmithTransform(Component2 InComponent)
		{
			MathTransform ComponentTransform = InComponent.GetTotalTransform(true);
			if (ComponentTransform == null)
			{
				ComponentTransform = InComponent.Transform2;
			}

			FConvertedTransform DatasmithTransform;
			if (ComponentTransform != null)
			{
				DatasmithTransform = MathUtils.ConvertFromSolidworksTransform(ComponentTransform, 100f /*GeomScale*/);
			}
			else
			{
				DatasmithTransform = FConvertedTransform.Identity();
			}

			return DatasmithTransform;
		}

		// Get component transform in specified configuration
		private FConvertedTransform GetComponentDatasmithTransform(FConfigurationTree.FComponentTreeNode InNode,
			FConfigurationTree.FComponentConfig ComponentConfig)
		{
			if (ComponentConfig != null && ComponentConfig.Transform.IsValid())
			{
				return ComponentConfig.Transform;
			}

			if (InNode.CommonConfig != null && InNode.CommonConfig.Transform.IsValid())
			{
				return InNode.CommonConfig.Transform;
			}

			return FConvertedTransform.Identity();
		}

		private void ExportComponentRecursive(FVariantName ActiveConfigName,
			FConfigurationExporter ConfigurationExporter,
			FConfigurationTree.FComponentTreeNode InNode, FConfigurationTree.FComponentTreeNode InParent)
		{
			SetExportStatus(InNode.ComponentName.GetString());

			FConfigurationTree.FComponentConfig ActiveConfig = InNode.Configurations?.Find(Config => Config.ConfigName == ActiveConfigName);
			FConvertedTransform Transform = GetComponentDatasmithTransform(InNode, ActiveConfig);
			SyncState.ComponentsTransformsMap[InNode.ComponentName] = Transform;

			if (InNode.bGeometrySame)
			{
				FActorName ActorName = Exporter.GetComponentActorName(InNode.ComponentName);

				FDatasmithActorExportInfo ActorExportInfo = new FDatasmithActorExportInfo();

				ActorExportInfo.Label = InNode.ComponentName.GetLabel();
				ActorExportInfo.Name = ActorName;

				if (InParent != null)
				{
					ActorExportInfo.ParentName = Exporter.GetComponentActorName(InParent.ComponentName);
				}

				ActorExportInfo.bVisible = true;
				ActorExportInfo.Type = Exporter.GetExportedActorType(ActorName) ?? EActorType.SimpleActor;
				ActorExportInfo.Transform = Transform;

				ActorExportInfo.bVisible = ActiveConfig?.bVisible ?? InNode.CommonConfig.bVisible;

				if (InNode.IsPartComponent())
				{
					// This component has associated part document -- treat is as a mesh actor
					ActorExportInfo.Type = EActorType.MeshActor;
				}

				if (ActorExportInfo.Type == EActorType.MeshActor)
				{
					ConfigurationExporter.AddActorForMesh(ActorExportInfo.Name, InNode.ComponentName);
				}

				Exporter.ExportOrUpdateActor(ActorExportInfo);
			}
			else
			{
				Debug.Assert(InNode.IsPartComponent());  // Expecting only Part components to have 'mesh' variants

				FActorName ParentName;
				{
					FActorName ActorName = Exporter.GetComponentActorName(InNode.ComponentName);
					ParentName = ActorName;

					FDatasmithActorExportInfo ActorExportInfo = new FDatasmithActorExportInfo();

					ActorExportInfo.Label = InNode.ComponentName.GetLabel();
					ActorExportInfo.Name = ActorName;

					if (InParent != null)
					{
						ActorExportInfo.ParentName = Exporter.GetComponentActorName(InParent.ComponentName);
					}

					ActorExportInfo.bVisible = true;
					ActorExportInfo.Type = EActorType.SimpleActor;  // Actor for Component with Mesh Variants is a 'simple' actor(i.e. just a node which has children)
					ActorExportInfo.Transform = Transform;

					ActorExportInfo.bVisible = ActiveConfig?.bVisible ?? InNode.CommonConfig.bVisible;

					Exporter.ExportOrUpdateActor(ActorExportInfo);
				}

				foreach (FConfigurationTree.FComponentConfig ComponentConfig in InNode.Configurations)
				{
					FActorName ActorName = ConfigurationExporter.GetMeshActorName(ComponentConfig.ConfigName, InNode.ComponentName);
					string Label = ActorName.GetString();

					FDatasmithActorExportInfo ActorExportInfo = new FDatasmithActorExportInfo();

					ActorExportInfo.Label = Label;
					ActorExportInfo.Name = ActorName;

					ActorExportInfo.ParentName = ParentName;

					ActorExportInfo.bVisible = true;
					ActorExportInfo.Type = Exporter.GetExportedActorType(ActorName) ?? EActorType.SimpleActor;
					ActorExportInfo.Transform = Transform;

					SyncState.ComponentsTransformsMap[InNode.ComponentName] = ActorExportInfo.Transform;

					ActorExportInfo.bVisible = ComponentConfig.bVisible && (ComponentConfig.ConfigName == ActiveConfigName);

					ActorExportInfo.Type = EActorType.MeshActor;

					ConfigurationExporter.AddActorForMesh(ActorExportInfo.Name, ComponentConfig.ConfigName, InNode.ComponentName);

					Exporter.ExportOrUpdateActor(ActorExportInfo);
				}
			}

			SyncState.CleanComponents.Add(InNode.ComponentName);

			// Export component children
			foreach (FConfigurationTree.FComponentTreeNode Child in InNode.EnumChildren())
			{
				ExportComponentRecursive(ActiveConfigName, ConfigurationExporter, Child, InNode);
			}
		}

		public override void AddCollectedComponent(FConfigurationTree.FComponentTreeNode InNode)
		{
			SyncState.CollectedComponentsMap[InNode.ComponentName] = InNode.Component;
		}

		public override bool NeedExportComponent(FConfigurationTree.FComponentTreeNode InComponent,
			FConfigurationTree.FComponentConfig ActiveComponentConfig)
		{
			bool bHasDirtyTransform = false;
			if (SyncState.ComponentsTransformsMap.ContainsKey(InComponent.ComponentName))
			{
				FConvertedTransform ComponentTm = GetComponentDatasmithTransform(InComponent, ActiveComponentConfig);
				bHasDirtyTransform =
					!MathUtils.TransformsAreEqual(SyncState.ComponentsTransformsMap[InComponent.ComponentName],
						ComponentTm);
			}

			bool bNeedExportComponent =
				bHasDirtyTransform || !SyncState.CleanComponents.Contains(InComponent.ComponentName);
			return bNeedExportComponent;
		}

		public override void AddPartDocument(FConfigurationTree.FComponentTreeNode InNode)
		{
			string PartPath = InNode.PartPath;
			Component2 Component = InNode.Component;
			FComponentName ComponentName = InNode.ComponentName;

			// Add part document to track its changes
			// todo: replace this with only document notifications. Doesn't seem that anything else is needed (assembly component itself is used to extract geometry)
			// Probably, we might use Part document info to identify components built from the same Part(beware - same Part can be different in different components - configured differently)
			// Anyway, 'PartDocument' is a lot for now, something simple can be used here definitely
			if (!SyncState.PartsMap.ContainsKey(PartPath))
			{
				FPartDocument PartTracker = AddTrackedAssemblyPart(Component, ComponentName);
				SyncState.PartsMap[PartPath] = PartTracker;
			}
		}

		// Tracks changes in a Part that is a component in the assembly
		private FPartDocument AddTrackedAssemblyPart(IComponent2 Component, FComponentName ComponentName)
		{
			ModelDoc2 ComponentDoc = Component.GetModelDoc2();
			// New part
			int PartDocId = Addin.Instance.GetDocumentId(ComponentDoc);

			FPartDocument PartTracker = new FPartDocument(PartDocId, ComponentDoc as PartDoc,
				Exporter, 
				this,  // Propagate change notifications to this assembly
				ComponentName);
			return PartTracker;
		}

		public void SetComponentDirty(FComponentName InComponent, EComponentDirtyState InState)
		{
			if (SyncState.CleanComponents.Contains(InComponent))
			{
				SyncState.CleanComponents.Remove(InComponent);
			}

			uint DirtyFlags = 0u;
			SyncState.DirtyComponents.TryGetValue(InComponent, out DirtyFlags);
			DirtyFlags |= (1u << (int)InState);
			SyncState.DirtyComponents[InComponent] = DirtyFlags;

			SetDirty(true);
		}

		public void SetComponentDirty(Component2 InComponent, EComponentDirtyState InState)
		{
			SetComponentDirty(new FComponentName(InComponent), InState);
		}

		public void ComponentDeleted(FComponentName ComponentName)
		{
			if (SyncState.CollectedComponentsMap.ContainsKey(ComponentName) &&
				!SyncState.ComponentsToDelete.Contains(ComponentName))
			{
				SyncState.ComponentsToDelete.Add(ComponentName);
				SetComponentDirty(ComponentName, FAssemblyDocumentTracker.EComponentDirtyState.Delete);
			}
		}

		// Active configuration changed
		public void ActiveConfigChanged()
		{
			SyncState.CleanComponents?.Clear();
			SyncState.DirtyComponents?.Clear();
			SyncState.CollectedComponentsMap?.Clear();
		}

		public void Tick()
		{
			LazyUpdateChecker?.Tick();
		}

		public void TrackChanges()
		{
			LazyUpdateChecker = new FAssemblyLazyUpdateChecker(this);
		}
	}

	// Handles Solidworks API document events and propagates them to the DocumentTracker
	// todo: extract api which is used by the Tracker to receive events. This should help to be clear on how Tracker is controlled by external events
	public class FAssemblyDocumentEvents
	{
		private readonly FAssemblyDocumentTracker AsmDocumentTracker;
		private readonly FAssemblyDocument AssemblyDocument;
		private AssemblyDoc SwAsmDoc => AssemblyDocument.SwAsmDoc;
		private int DocId => AssemblyDocument.DocId;

		public FAssemblyDocumentEvents(FAssemblyDocument InAssemblyDocument, FAssemblyDocumentTracker InAsmDocumentTracker)
		{
			AssemblyDocument = InAssemblyDocument;
			AsmDocumentTracker = InAsmDocumentTracker;

			SwAsmDoc.RegenNotify += new DAssemblyDocEvents_RegenNotifyEventHandler(OnRegenNotify);
			SwAsmDoc.ActiveDisplayStateChangePreNotify += new DAssemblyDocEvents_ActiveDisplayStateChangePreNotifyEventHandler(OnActiveDisplayStateChangePreNotify);
			SwAsmDoc.ActiveViewChangeNotify += new DAssemblyDocEvents_ActiveViewChangeNotifyEventHandler(OnActiveViewChangeNotify);
			SwAsmDoc.SuppressionStateChangeNotify += new DAssemblyDocEvents_SuppressionStateChangeNotifyEventHandler(OnSuppressionStateChangeNotify);
			SwAsmDoc.ComponentReorganizeNotify += new DAssemblyDocEvents_ComponentReorganizeNotifyEventHandler(OnComponentReorganizeNotify);
			SwAsmDoc.ActiveDisplayStateChangePostNotify += new DAssemblyDocEvents_ActiveDisplayStateChangePostNotifyEventHandler(OnActiveDisplayStateChangePostNotify);
			SwAsmDoc.ConfigurationChangeNotify += new DAssemblyDocEvents_ConfigurationChangeNotifyEventHandler(OnConfigurationChangeNotify);
			SwAsmDoc.DestroyNotify2 += new DAssemblyDocEvents_DestroyNotify2EventHandler(OnDocumentDestroyNotify2);
			SwAsmDoc.ComponentConfigurationChangeNotify += new DAssemblyDocEvents_ComponentConfigurationChangeNotifyEventHandler(OnComponentConfigurationChangeNotify);
			SwAsmDoc.UndoPostNotify += new DAssemblyDocEvents_UndoPostNotifyEventHandler(OnUndoPostNotify);
			SwAsmDoc.RenamedDocumentNotify += new DAssemblyDocEvents_RenamedDocumentNotifyEventHandler(OnRenamedDocumentNotify);
			SwAsmDoc.DragStateChangeNotify += new DAssemblyDocEvents_DragStateChangeNotifyEventHandler(OnDragStateChangeNotify);
			SwAsmDoc.RegenPostNotify2 += new DAssemblyDocEvents_RegenPostNotify2EventHandler(OnRegenPostNotify2);
			SwAsmDoc.ComponentReferredDisplayStateChangeNotify += new DAssemblyDocEvents_ComponentReferredDisplayStateChangeNotifyEventHandler(OnComponentReferredDisplayStateChangeNotify);
			SwAsmDoc.RedoPostNotify += new DAssemblyDocEvents_RedoPostNotifyEventHandler(OnRedoPostNotify);
			SwAsmDoc.ComponentStateChangeNotify3 += new DAssemblyDocEvents_ComponentStateChangeNotify3EventHandler(OnComponentStateChangeNotify3);
			SwAsmDoc.ComponentStateChangeNotify += new DAssemblyDocEvents_ComponentStateChangeNotifyEventHandler(OnComponentStateChangeNotify);
			SwAsmDoc.ModifyNotify += new DAssemblyDocEvents_ModifyNotifyEventHandler(OnModifyNotify);
			SwAsmDoc.DeleteItemNotify += new DAssemblyDocEvents_DeleteItemNotifyEventHandler(OnDeleteItemNotify);
			SwAsmDoc.RenameItemNotify += new DAssemblyDocEvents_RenameItemNotifyEventHandler(OnRenameItemNotify);
			SwAsmDoc.AddItemNotify += new DAssemblyDocEvents_AddItemNotifyEventHandler(OnAddItemNotify);
			SwAsmDoc.FileReloadNotify += new DAssemblyDocEvents_FileReloadNotifyEventHandler(OnFileReloadNotify);
			SwAsmDoc.ActiveConfigChangeNotify += new DAssemblyDocEvents_ActiveConfigChangeNotifyEventHandler(OnActiveConfigChangeNotify);
			SwAsmDoc.FileSaveAsNotify += new DAssemblyDocEvents_FileSaveAsNotifyEventHandler(OnFileSaveAsNotify);
			SwAsmDoc.FileSaveNotify += new DAssemblyDocEvents_FileSaveNotifyEventHandler(OnFileSaveNotify);
			SwAsmDoc.RegenPostNotify += new DAssemblyDocEvents_RegenPostNotifyEventHandler(OnRegenPostNotify);
			SwAsmDoc.ActiveConfigChangePostNotify += new DAssemblyDocEvents_ActiveConfigChangePostNotifyEventHandler(OnActiveConfigChangePostNotify);
			SwAsmDoc.ComponentStateChangeNotify2 += new DAssemblyDocEvents_ComponentStateChangeNotify2EventHandler(OnComponentStateChangeNotify2);
			SwAsmDoc.AddCustomPropertyNotify += new DAssemblyDocEvents_AddCustomPropertyNotifyEventHandler(OnAddCustomPropertyNotify);
			SwAsmDoc.ChangeCustomPropertyNotify += new DAssemblyDocEvents_ChangeCustomPropertyNotifyEventHandler(OnChangeCustomPropertyNotify);
			SwAsmDoc.DimensionChangeNotify += new DAssemblyDocEvents_DimensionChangeNotifyEventHandler(OnDimensionChangeNotify);
			SwAsmDoc.ComponentDisplayStateChangeNotify += new DAssemblyDocEvents_ComponentDisplayStateChangeNotifyEventHandler(OnComponentDisplayStateChangeNotify);
			SwAsmDoc.ComponentVisualPropertiesChangeNotify += new DAssemblyDocEvents_ComponentVisualPropertiesChangeNotifyEventHandler(OnComponentVisualPropertiesChangeNotify);
			SwAsmDoc.ComponentMoveNotify2 += new DAssemblyDocEvents_ComponentMoveNotify2EventHandler(OnComponentMoveNotify2);
			SwAsmDoc.BodyVisibleChangeNotify += new DAssemblyDocEvents_BodyVisibleChangeNotifyEventHandler(OnBodyVisibleChangeNotify);
			SwAsmDoc.ComponentVisibleChangeNotify += new DAssemblyDocEvents_ComponentVisibleChangeNotifyEventHandler(OnComponentVisibleChangeNotify);
			SwAsmDoc.ComponentMoveNotify += new DAssemblyDocEvents_ComponentMoveNotifyEventHandler(OnComponentMoveNotify);
			SwAsmDoc.FileReloadPreNotify += new DAssemblyDocEvents_FileReloadPreNotifyEventHandler(OnFileReloadPreNotify);
			SwAsmDoc.DeleteSelectionPreNotify += new DAssemblyDocEvents_DeleteSelectionPreNotifyEventHandler(OnDeleteSelectionPreNotify);
			SwAsmDoc.FileSaveAsNotify2 += new DAssemblyDocEvents_FileSaveAsNotify2EventHandler(OnFileSaveAsNotify2);
		}

		public void Destroy()
		{
			SwAsmDoc.RegenNotify -= new DAssemblyDocEvents_RegenNotifyEventHandler(OnRegenNotify);
			SwAsmDoc.ActiveDisplayStateChangePreNotify -= new DAssemblyDocEvents_ActiveDisplayStateChangePreNotifyEventHandler(OnActiveDisplayStateChangePreNotify);
			SwAsmDoc.ActiveViewChangeNotify -= new DAssemblyDocEvents_ActiveViewChangeNotifyEventHandler(OnActiveViewChangeNotify);
			SwAsmDoc.SuppressionStateChangeNotify -= new DAssemblyDocEvents_SuppressionStateChangeNotifyEventHandler(OnSuppressionStateChangeNotify);
			SwAsmDoc.ComponentReorganizeNotify -= new DAssemblyDocEvents_ComponentReorganizeNotifyEventHandler(OnComponentReorganizeNotify);
			SwAsmDoc.ActiveDisplayStateChangePostNotify -= new DAssemblyDocEvents_ActiveDisplayStateChangePostNotifyEventHandler(OnActiveDisplayStateChangePostNotify);
			SwAsmDoc.ConfigurationChangeNotify -= new DAssemblyDocEvents_ConfigurationChangeNotifyEventHandler(OnConfigurationChangeNotify);
			SwAsmDoc.DestroyNotify2 -= new DAssemblyDocEvents_DestroyNotify2EventHandler(OnDocumentDestroyNotify2);
			SwAsmDoc.ComponentConfigurationChangeNotify -= new DAssemblyDocEvents_ComponentConfigurationChangeNotifyEventHandler(OnComponentConfigurationChangeNotify);
			SwAsmDoc.UndoPostNotify -= new DAssemblyDocEvents_UndoPostNotifyEventHandler(OnUndoPostNotify);
			SwAsmDoc.RenamedDocumentNotify -= new DAssemblyDocEvents_RenamedDocumentNotifyEventHandler(OnRenamedDocumentNotify);
			SwAsmDoc.DragStateChangeNotify -= new DAssemblyDocEvents_DragStateChangeNotifyEventHandler(OnDragStateChangeNotify);
			SwAsmDoc.RegenPostNotify2 -= new DAssemblyDocEvents_RegenPostNotify2EventHandler(OnRegenPostNotify2);
			SwAsmDoc.ComponentReferredDisplayStateChangeNotify -= new DAssemblyDocEvents_ComponentReferredDisplayStateChangeNotifyEventHandler(OnComponentReferredDisplayStateChangeNotify);
			SwAsmDoc.RedoPostNotify -= new DAssemblyDocEvents_RedoPostNotifyEventHandler(OnRedoPostNotify);
			SwAsmDoc.ComponentStateChangeNotify3 -= new DAssemblyDocEvents_ComponentStateChangeNotify3EventHandler(OnComponentStateChangeNotify3);
			SwAsmDoc.ComponentStateChangeNotify -= new DAssemblyDocEvents_ComponentStateChangeNotifyEventHandler(OnComponentStateChangeNotify);
			SwAsmDoc.ModifyNotify -= new DAssemblyDocEvents_ModifyNotifyEventHandler(OnModifyNotify);
			SwAsmDoc.DeleteItemNotify -= new DAssemblyDocEvents_DeleteItemNotifyEventHandler(OnDeleteItemNotify);
			SwAsmDoc.RenameItemNotify -= new DAssemblyDocEvents_RenameItemNotifyEventHandler(OnRenameItemNotify);
			SwAsmDoc.AddItemNotify -= new DAssemblyDocEvents_AddItemNotifyEventHandler(OnAddItemNotify);
			SwAsmDoc.FileReloadNotify -= new DAssemblyDocEvents_FileReloadNotifyEventHandler(OnFileReloadNotify);
			SwAsmDoc.ActiveConfigChangeNotify -= new DAssemblyDocEvents_ActiveConfigChangeNotifyEventHandler(OnActiveConfigChangeNotify);
			SwAsmDoc.FileSaveAsNotify -= new DAssemblyDocEvents_FileSaveAsNotifyEventHandler(OnFileSaveAsNotify);
			SwAsmDoc.FileSaveNotify -= new DAssemblyDocEvents_FileSaveNotifyEventHandler(OnFileSaveNotify);
			SwAsmDoc.RegenPostNotify -= new DAssemblyDocEvents_RegenPostNotifyEventHandler(OnRegenPostNotify);
			SwAsmDoc.ActiveConfigChangePostNotify -= new DAssemblyDocEvents_ActiveConfigChangePostNotifyEventHandler(OnActiveConfigChangePostNotify);
			SwAsmDoc.ComponentStateChangeNotify2 -= new DAssemblyDocEvents_ComponentStateChangeNotify2EventHandler(OnComponentStateChangeNotify2);
			SwAsmDoc.AddCustomPropertyNotify -= new DAssemblyDocEvents_AddCustomPropertyNotifyEventHandler(OnAddCustomPropertyNotify);
			SwAsmDoc.ChangeCustomPropertyNotify -= new DAssemblyDocEvents_ChangeCustomPropertyNotifyEventHandler(OnChangeCustomPropertyNotify);
			SwAsmDoc.DimensionChangeNotify -= new DAssemblyDocEvents_DimensionChangeNotifyEventHandler(OnDimensionChangeNotify);
			SwAsmDoc.ComponentDisplayStateChangeNotify -= new DAssemblyDocEvents_ComponentDisplayStateChangeNotifyEventHandler(OnComponentDisplayStateChangeNotify);
			SwAsmDoc.ComponentVisualPropertiesChangeNotify -= new DAssemblyDocEvents_ComponentVisualPropertiesChangeNotifyEventHandler(OnComponentVisualPropertiesChangeNotify);
			SwAsmDoc.ComponentMoveNotify2 -= new DAssemblyDocEvents_ComponentMoveNotify2EventHandler(OnComponentMoveNotify2);
			SwAsmDoc.BodyVisibleChangeNotify -= new DAssemblyDocEvents_BodyVisibleChangeNotifyEventHandler(OnBodyVisibleChangeNotify);
			SwAsmDoc.ComponentVisibleChangeNotify -= new DAssemblyDocEvents_ComponentVisibleChangeNotifyEventHandler(OnComponentVisibleChangeNotify);
			SwAsmDoc.ComponentMoveNotify -= new DAssemblyDocEvents_ComponentMoveNotifyEventHandler(OnComponentMoveNotify);
			SwAsmDoc.FileReloadPreNotify -= new DAssemblyDocEvents_FileReloadPreNotifyEventHandler(OnFileReloadPreNotify);
			SwAsmDoc.DeleteSelectionPreNotify -= new DAssemblyDocEvents_DeleteSelectionPreNotifyEventHandler(OnDeleteSelectionPreNotify);
			SwAsmDoc.FileSaveAsNotify2 -= new DAssemblyDocEvents_FileSaveAsNotify2EventHandler(OnFileSaveAsNotify2);
		}

		int OnComponentStateChangeNotify(object componentModel, short newCompState)
		{
			return 0;
		}

		int OnComponentStateChangeNotify(object componentModel)
		{
			OnComponentStateChangeNotify(componentModel, (short)swComponentSuppressionState_e.swComponentResolved);
			return 0;
		}

		int OnComponentStateChangeNotify(object componentModel, short oldCompState, short newCompState)
		{
			return OnComponentStateChangeNotify(componentModel, newCompState);
		}

		int OnComponentDisplayStateChangeNotify(object InComponentObj)
		{
			if (InComponentObj is Component2 Comp)
			{
				AsmDocumentTracker.SetComponentDirty(Comp, FAssemblyDocumentTracker.EComponentDirtyState.Visibility);
			}
			return 0;
		}

		int OnComponentVisualPropertiesChangeNotify(object InCompObject)
		{
			if (InCompObject is Component2 Comp)
			{
				AsmDocumentTracker.SetComponentDirty(Comp, FAssemblyDocumentTracker.EComponentDirtyState.Material);
			}
			return 0;
		}

		int OnRegenNotify()
		{
			return 0;
		}

		int OnActiveDisplayStateChangePreNotify()
		{
			return 0;
		}

		int OnActiveViewChangeNotify()
		{
			return 0;
		}

		int OnSuppressionStateChangeNotify(Feature InFeature, int InNewSuppressionState, int InPreviousSuppressionState, int InConfigurationOption, ref object InConfigurationNames)
		{
			return 0;
		}

		int OnComponentReorganizeNotify(string sourceName, string targetName)
		{
			return 0;
		}

		int OnActiveDisplayStateChangePostNotify(string DisplayStateName)
		{
			return 0;
		}

		int OnConfigurationChangeNotify(string ConfigurationName, object Object, int ObjectType, int changeType)
		{
			return 0;
		}

		int OnDocumentDestroyNotify2(int DestroyType)
		{
			Addin.Instance.CloseDocument(DocId);
			return 0;
		}

		int OnComponentConfigurationChangeNotify(string componentName, string oldConfigurationName, string newConfigurationName)
		{
			AsmDocumentTracker.SetComponentDirty(FComponentName.FromApiString(componentName), FAssemblyDocumentTracker.EComponentDirtyState.Geometry);
			return 0;
		}

		int OnUndoPostNotify()
		{
			// Check each exported component's transform for changes, since 
			// this callback does not tell us what changed (and there's no other way to know that)!
			foreach (var KVP in AsmDocumentTracker.SyncState.CollectedComponentsMap)
			{
				Component2 Comp = KVP.Value;
				FConvertedTransform PrevCompTransform;
				if (AsmDocumentTracker.SyncState.ComponentsTransformsMap.TryGetValue(new FComponentName(Comp), out PrevCompTransform))
				{
					FConvertedTransform CompTransform = AsmDocumentTracker.GetComponentDatasmithTransform(Comp);

					if (!MathUtils.TransformsAreEqual(CompTransform, PrevCompTransform))
					{
						AsmDocumentTracker.SetDirty(true);
						break;
					}
				}
			}
			return 0;
		}

		int OnRenamedDocumentNotify(ref object RenamedDocumentInterface)
		{
			return 0;
		}

		int OnDisplayModeChangePostNotify(object Component)
		{
			return 0;
		}

		int OnDragStateChangeNotify(Boolean State)
		{
			return 0;
		}

		int OnRegenPostNotify2(object stopFeature)
		{
			return 0;
		}

		int OnComponentReferredDisplayStateChangeNotify(object componentModel, string CompName, int oldDSId, string oldDSName, int newDSId, string newDSName)
		{
			return 0;
		}

		int OnRedoPostNotify()
		{
			return 0;
		}

		int OnComponentStateChangeNotify3(object InComponentObj, string InCompName, short InOldCompState, short InNewCompState)
		{
			if (InComponentObj is Component2 Comp)
			{
				AsmDocumentTracker.SetComponentDirty(new FComponentName(Comp), FAssemblyDocumentTracker.EComponentDirtyState.Visibility);
			}
			return 0;
		}

		int OnModifyNotify()
		{
			return 0;
		}

		int OnDeleteItemNotify(int InEntityType, string InItemName)
		{
			if (InEntityType == (int)swNotifyEntityType_e.swNotifyComponent)
			{
				AsmDocumentTracker.ComponentDeleted(FComponentName.FromApiString(InItemName));
			}
			return 0;
		}

		int OnRenameItemNotify(int InEntityType, string InOldName, string InNewName)
		{
			if (InEntityType == (int)swNotifyEntityType_e.swNotifyComponent)
			{
				AsmDocumentTracker.ComponentDeleted(FComponentName.FromApiString(InOldName));
			}
			return 0;
		}

		int OnAddItemNotify(int InEntityType, string InItemName)
		{
			if (InEntityType == (int)swNotifyEntityType_e.swNotifyComponent)
			{
				AsmDocumentTracker.SetDirty(true);
			}
			return 0;
		}

		int OnFileReloadNotify()
		{
			return 0;
		}

		int OnActiveConfigChangeNotify()
		{
			AsmDocumentTracker.SetDirty(true);
			AsmDocumentTracker.ActiveConfigChanged();
			return 0;
		}

		int OnFileSaveAsNotify(string FileName)
		{
			return 0;
		}

		int OnFileSaveNotify(string FileName)
		{
			return 0;
		}

		int OnRegenPostNotify()
		{
			return 0;
		}

		int OnActiveConfigChangePostNotify()
		{
			return 0;
		}

		int OnComponentStateChangeNotify2(object componentModel, string CompName, short oldCompState, short newCompState)
		{
			return 0;
		}

		int OnAddCustomPropertyNotify(string propName, string Configuration, string Value, int valueType)
		{
			return 0;
		}

		int OnChangeCustomPropertyNotify(string propName, string Configuration, string oldValue, string NewValue, int valueType)
		{
			return 0;
		}

		int OnDimensionChangeNotify(object displayDim)
		{
			return 0;
		}

		int OnComponentMoveNotify2(ref object InComponents)
		{
			object[] ObjComps = InComponents as object[];
			foreach (object ObjComp in ObjComps)
			{
				IComponent2 Comp = ObjComp as IComponent2;
				if (Comp != null)
				{
					AsmDocumentTracker.SetComponentDirty(new FComponentName(Comp), FAssemblyDocumentTracker.EComponentDirtyState.Transform);
				}
			}
			return 0;
		}

		int OnBodyVisibleChangeNotify()
		{
			return 0;
		}

		int OnComponentVisibleChangeNotify()
		{
			return 0;
		}

		int OnComponentMoveNotify()
		{
			return 0;
		}

		int OnFileReloadPreNotify()
		{
			return 0;
		}

		int OnDeleteSelectionPreNotify()
		{
			return 0;
		}

		int OnFileSaveAsNotify2(string FileName)
		{
			return 0;
		}
	}

	// Notifications for the top(synced) document, includes material update checker thread
	public class FAssemblyDocumentNotifications
	{
		private readonly FAssemblyDocumentTracker DocumentTracker;
		private readonly FAssemblyDocumentEvents Events;

		public FAssemblyDocumentNotifications(FAssemblyDocument InAssemblyDocument, FAssemblyDocumentTracker InAsmDocumentTracker)
		{
			DocumentTracker = InAsmDocumentTracker;
			Events = new FAssemblyDocumentEvents(InAssemblyDocument, InAsmDocumentTracker);
		}

		public void Destroy()
		{
			Events.Destroy();
		}

		public void Start()
		{
			DocumentTracker.TrackChanges();
		}

		public void Resume()
		{
		}

		public void Pause()
		{
		}

		public void Idle()
		{
			DocumentTracker.Tick();
		}
	}

	// See IDocumentSyncer interface, implemented for Assembly
	public class FAssemblyDocumentSyncer : IDocumentSyncer
	{
		public readonly FAssemblyDocumentNotifications Notifications;
		public readonly FAssemblyDocumentTracker DocumentTracker;

		public FAssemblyDocumentSyncer(FAssemblyDocument InDoc, AssemblyDoc InSwDoc, FDatasmithExporter InExporter)
		{
			DocumentTracker = new FAssemblyDocumentTracker(InDoc, InSwDoc, InExporter);
			Notifications = new FAssemblyDocumentNotifications(InDoc, DocumentTracker);
		}

		public void Destroy()
		{
			Notifications.Destroy();
			DocumentTracker.Destroy();
		}

		public void Idle()
		{
			Notifications.Idle();
		}

		public void Start()
		{
			Notifications.Start();
		}

		public void Resume()
		{
			Notifications.Resume();
		}

		public void Pause()  
		{
			Notifications.Pause();
		}

		public bool GetDirty()  // i.e. in autosync
		{
			return DocumentTracker.GetDirty();
		}

		public FDocumentTracker GetTracker()
		{
			return DocumentTracker;
		}

		public void SetDirty(bool bInDirty)  // After Sync, where called with true??? Probably not needed at all(i.e. all SetDirty is inside in notifiers, and with false can be in Sync)
		{
			DocumentTracker.SetDirty(bInDirty);
		}

		public void Sync(string InOutputPath)
		{
			DocumentTracker.Sync(InOutputPath);
		}

		public FDatasmithFacadeScene GetDatasmithScene()
		{
			return DocumentTracker.DatasmithScene;
		}
	}

	public class FAssemblyDocument : FDocument
	{
		public AssemblyDoc SwAsmDoc { get; private set; } = null;

		public FAssemblyDocument(int InDocId, AssemblyDoc InSwDoc, FDatasmithExporter InExporter) : base(InDocId, InSwDoc as ModelDoc2)
		{
			SwAsmDoc = InSwDoc;

			DocumentSyncer = new FAssemblyDocumentSyncer(this, SwAsmDoc, InExporter);
		}

		public override void Export(string InFilePath)
		{
			FAssemblyDocumentTracker Tracker = new FAssemblyDocumentTracker(this, SwAsmDoc, null);
			Tracker.DatasmithFileExportPath = InFilePath;
			Tracker.Export();
		}

	}
}