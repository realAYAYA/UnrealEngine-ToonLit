// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.FDatasmithExporter;
using System.Diagnostics;
using System.Linq;


namespace DatasmithSolidworks
{
	// Checks if scene is updated iteratively
	//
	// Solidworks notifications lack a number of notifications for material changes - 
	// material (re)assignment to various entity types and change in material properties themselves
	// This class lazily iterates over all the components to identify these changes
	// This lazy process it performed in small portions ove multiple 'ticks' in the main thread.
	// It's done in the main thread(not in a separate thread) for simple reason that SW api 
	// is very slow to use from another thread(although it may be convenient for simple tasks)
	// Doing this in a thread takes dozens of seconds for models which take a second to fully export.
	// Making this unusable. And also unstable as any change to the model in the main thread may result in a crash in the thread
	public abstract class FLazyUpdateCheckerBase
	{
		private readonly FDocumentTracker DocumentTracker;
		private int MaxTickTimeMilliseconds = 100;
		private IEnumerator<bool> Updating = null;

		protected FLazyUpdateCheckerBase(FDocumentTracker InDocumentTracker)
		{
			DocumentTracker = InDocumentTracker;
		}

		public void Reset()
		{
			if (Updating != null)
			{
				Updating.Dispose();
				Updating = null;
			}
		}

		// Advances checking for updates in the model
		public void Tick()
		{
			if (DocumentTracker.GetDirty())
			{
				// If document is already market dirty don't need to check for modifications
				Reset();
				return;
			}

			if (Updating == null)
			{
				Restart();
				Updating = CheckForModificationsEnum().GetEnumerator();
			}

			Stopwatch Watch = Stopwatch.StartNew();

			bool bPostponed = false;
			try
			{
				while (Updating.MoveNext())
				{
					if (Watch.ElapsedMilliseconds > MaxTickTimeMilliseconds)
					{
						// Postpone the reset of update if it has already taken too long for this tick, so the application doesn't stutter
						bPostponed = true;
						break;
					}
				}
			}
			catch
			{
				// This will just restart the checker in case any exception occurs during update
			}

			if (HasUpdates())
			{
				DocumentTracker.SetDirty(true);
			}

			if (!bPostponed)
			{
				Reset();
			}
		}

		/// Reset state of updates check
		public abstract void Restart();

		/// Get state of updates check
		public abstract bool HasUpdates();

		///  Resumable check for modifications
		public abstract IEnumerable<bool> CheckForModificationsEnum();
	};

	public class FPartLazyUpdateChecker: FLazyUpdateCheckerBase
	{
		private FPartDocumentTracker PartDocumentTracker;
		private bool bHasDirtyMaterials;

		public FPartLazyUpdateChecker(FPartDocumentTracker InDocumentTracker) : base(InDocumentTracker)
		{
			PartDocumentTracker = InDocumentTracker;
		}

		public override void Restart()
		{
			bHasDirtyMaterials = false;
		}

		public override bool HasUpdates()
		{
			return bHasDirtyMaterials;
		}

		public override IEnumerable<bool> CheckForModificationsEnum()
		{
			FObjectMaterials CurrentMaterials =
				FObjectMaterials.LoadPartMaterials(PartDocumentTracker, PartDocumentTracker.SwPartDoc, swDisplayStateOpts_e.swThisDisplayState, null, PartDocumentTracker.ExportedMaterialsMap);
			if (CurrentMaterials == null && PartDocumentTracker.ExportedPartMaterials == null)
			{
				yield break;
			}
			else if ((CurrentMaterials != null && PartDocumentTracker.ExportedPartMaterials == null) ||
			         (CurrentMaterials == null && PartDocumentTracker.ExportedPartMaterials != null))
			{
				bHasDirtyMaterials = true;
				yield break;
			}

			bHasDirtyMaterials = !CurrentMaterials.EqualMaterials(PartDocumentTracker.ExportedPartMaterials);
		}
	};

	public class FPartDocumentTracker : FDocumentTracker
	{
		public PartDoc SwPartDoc { get; private set; } = null;
		private FAssemblyDocumentTracker AsmDoc = null;
		private FComponentName ComponentName;
		private FMeshName MeshName = new FMeshName();
		public readonly FPartDocument PartDocument;

		private FMeshes Meshes;

		public FObjectMaterials ExportedPartMaterials { get; set; }

		public string PathName { get; private set; }

		private FPartLazyUpdateChecker LazyUpdateChecker;

		public FPartDocumentTracker(FPartDocument InDoc, PartDoc InPartDoc, FDatasmithExporter InExporter,
			FAssemblyDocumentTracker InAsmDoc, FComponentName InComponentName)
			: base(InDoc, InExporter)
		{
			PartDocument = InDoc;
			SwPartDoc = InPartDoc;
			AsmDoc = InAsmDoc;
			ComponentName = InComponentName;

			PathName = SwDoc?.GetPathName() ?? "";
			if (string.IsNullOrEmpty(PathName))
			{
				// Unsaved imported parts have no path or name
				PathName = SwDoc.GetExternalReferenceName();
			}
		}

		static public string GetBodyPath(PartDoc InDoc, IBody2 InBody)
		{
			ModelDoc2 SwDoc = InDoc as ModelDoc2;
			string Path = "";
			if (SwDoc != null && InBody != null)
			{
				try
				{
					string Title = SwDoc.GetPathName();
					string Name = InBody.Name;
					Path = Title + "\\\\" + Name;
				}
				catch
				{
					Debug.Assert(false);
				}
			}

			return Path;
		}

		static public string GetFeaturePath(PartDoc InDoc, IFeature InFeature)
		{
			ModelDoc2 SwDoc = InDoc as ModelDoc2;
			string Path = "";
			if (SwDoc != null && InFeature != null)
			{
				try
				{
					string Title = SwDoc.GetPathName();
					string Name = InFeature.Name;
					Path = Title + "\\\\" + Name;
				}
				catch
				{
					Debug.Assert(false);
				}
			}

			return Path;
		}

		public override void SetDirty(bool bInDirty)
		{
			base.SetDirty(bInDirty);

			if (bInDirty && AsmDoc != null && ComponentName.IsValid())
			{
				// Notify owner assembly that a component needs re-export
				AsmDoc.SetComponentDirty(ComponentName, FAssemblyDocumentTracker.EComponentDirtyState.Geometry);
			}
		}

		public override FMeshes GetMeshes(string ActiveConfigName)
		{
			return Meshes ?? (Meshes = new FMeshes(ActiveConfigName));
		}

		public override void ExportToDatasmithScene(FConfigurationExporter ConfigurationExporter, FVariantName ActiveVariantName)
		{
			FMeshes Meshes = ConfigurationExporter.Meshes;

			string PartName = Path.GetFileNameWithoutExtension(PathName);

			SetExportStatus($"{PartName} Meshes");

			ConfigurationManager ConfigManager = (SwPartDoc as ModelDoc2).ConfigurationManager;
			if (bHasConfigurations)
			{
				string ActiveConfigurationName = ConfigManager.ActiveConfiguration.Name;

				foreach (string ConfigurationName in SwDoc.GetConfigurationNames())
				{

					// todo: might want to not use GetRootComponent3 for Part - docs says that it's null for Part
					FComponentName RootComponentName =
						new FComponentName(ConfigManager.ActiveConfiguration.GetRootComponent3(true));
					FMeshData MeshData = Meshes.GetMeshData(ConfigurationName, RootComponentName);
					if (MeshData == null)
					{
						continue;
					}

					// todo: not make extra copy - use existing(i.e. instead of bHasConfigurations) 
					FActorName ActorName =
						ConfigurationExporter.GetMeshActorName(FVariantName.PartVariant(ConfigurationName), RootComponentName);

					FMeshName MeshNameForConfiguration = new FMeshName(RootComponentName, ConfigurationName);

					List<FMeshExportInfo> MeshExportInfos = new List<FMeshExportInfo>
					{
						new FMeshExportInfo
						{
							ComponentName = RootComponentName,
							MeshName = MeshNameForConfiguration,
							MeshData = MeshData
						}
					};

					List<FMeshExportInfo> OutCreatedMeshes = Exporter.ExportMeshes(MeshExportInfos).ToList();

					// OutCreatedMeshes might be empty(e.g. mesh is empty it won't be created)
					foreach (FMeshExportInfo Info in OutCreatedMeshes)
					{
						Exporter.AddMesh(Info);
					}

					ExportMaterials();
					Exporter.AssignMaterialsToDatasmithMeshes(OutCreatedMeshes);
					bool bHasMesh = OutCreatedMeshes.Count > 0;

					FDatasmithActorExportInfo ExportInfo = new FDatasmithActorExportInfo();
					ExportInfo.Name = ActorName;
					ExportInfo.bVisible = ConfigurationName == ActiveConfigurationName;
					ExportInfo.Label = ActorName.GetString();
					ExportInfo.MeshName = MeshNameForConfiguration;
					ExportInfo.Type = bHasMesh ? EActorType.MeshActor : EActorType.SimpleActor;
					Exporter.ExportOrUpdateActor(ExportInfo);
				}
			}
			else
			{
				FComponentName RootComponentName =
					new FComponentName(ConfigManager.ActiveConfiguration.GetRootComponent3(true));


				FActorName ActorName = FActorName.FromString(PartName);
				
				FMeshData MeshData = ExtractPartMeshData();

				Exporter.RemoveMesh(MeshName);
				MeshName = new FMeshName();
				bool bHasMesh = false;

				if (MeshData != null)
				{
					MeshName = new FMeshName(RootComponentName);

					List<FMeshExportInfo> MeshExportInfos = new List<FMeshExportInfo>
					{
						new FMeshExportInfo
						{
							ComponentName = RootComponentName,
							MeshName = MeshName,
							MeshData = MeshData
						}
					};

					List<FMeshExportInfo> OutCreatedMeshes = Exporter.ExportMeshes(MeshExportInfos).ToList();

					// OutCreatedMeshes might be empty(e.g. mesh is empty it won't be created)
					foreach (FMeshExportInfo Info in OutCreatedMeshes)
					{
						Exporter.AddMesh(Info);
					}

					ExportMaterials();
					Exporter.AssignMaterialsToDatasmithMeshes(OutCreatedMeshes);
					bHasMesh = OutCreatedMeshes.Count > 0;
				}

				FDatasmithActorExportInfo ExportInfo = new FDatasmithActorExportInfo();
				ExportInfo.Name = ActorName;
				ExportInfo.bVisible = true;
				ExportInfo.Label = ActorName.GetString();
				ExportInfo.MeshName = MeshName;
				ExportInfo.Type = bHasMesh ? EActorType.MeshActor : EActorType.SimpleActor;
				Exporter.ExportOrUpdateActor(ExportInfo);
			}

			
		}

		public override FObjectMaterials GetComponentMaterials(Component2 Comp)
		{
			return null;
		}

		public override void AddMeshForComponent(FComponentName ComponentName, FMeshName MeshName)
		{
		}

		public override void ReleaseComponentMeshes(FComponentName CompName)
		{
		}

		public override void CleanupComponentMeshes()
		{
		}

		public override FMeshData ExtractComponentMeshData(Component2 Comp)
		{
			return ExtractPartMeshData();  // Part document has single component
		}

		// Extract mesh data for current active configuration. MeshData is ready for export to DatasmithMesh 
		public FMeshData ExtractPartMeshData()
		{
			FObjectMaterials PartMaterials = FObjectMaterials.LoadPartMaterials(this, SwPartDoc, swDisplayStateOpts_e.swThisDisplayState, null, ExportedMaterialsMap);
			ExportedPartMaterials = PartMaterials;
			ConcurrentBag<FBody> Bodies = FBody.FetchBodies(SwPartDoc);
			return FStripGeometry.CreateMeshData(Bodies, PartMaterials);
		}

		public override void AddComponentMaterials(FComponentName ComponentName, FObjectMaterials Materials)
		{
		}

		public override Dictionary<FComponentName, FObjectMaterials> LoadDocumentMaterials(HashSet<FComponentName> ComponentNamesToExportSet)
		{
			return null;
		}

		public void Tick()
		{
			LazyUpdateChecker?.Tick();
		}

		public void TrackChanges()
		{
			LazyUpdateChecker = new FPartLazyUpdateChecker(this);
		}
	}

	public class FPartDocumentEvents
	{
		private readonly FPartDocumentTracker DocumentTracker;
		private PartDoc SwPartDoc => DocumentTracker.PartDocument.SwPartDoc;
		private int DocId => DocumentTracker.PartDocument.DocId;

		public FPartDocumentEvents(FPartDocumentTracker InDocumentTracker)
		{
			DocumentTracker = InDocumentTracker;

			SwPartDoc.UndoPostNotify += new DPartDocEvents_UndoPostNotifyEventHandler(OnPartUndoPostNotify);
			SwPartDoc.SuppressionStateChangeNotify += new DPartDocEvents_SuppressionStateChangeNotifyEventHandler(OnPartSuppressionStateChangeNotify);
			SwPartDoc.DestroyNotify2 += new DPartDocEvents_DestroyNotify2EventHandler(OnPartDocumentDestroyNotify2);
			SwPartDoc.EquationEditorPostNotify += new DPartDocEvents_EquationEditorPostNotifyEventHandler(OnPartEquationEditorPostNotify);
			SwPartDoc.ConfigurationChangeNotify += new DPartDocEvents_ConfigurationChangeNotifyEventHandler(OnPartConfigurationChangeNotify);
			SwPartDoc.ActiveDisplayStateChangePostNotify += new DPartDocEvents_ActiveDisplayStateChangePostNotifyEventHandler(OnPartActiveDisplayStateChangePostNotify);
			SwPartDoc.DragStateChangeNotify += new DPartDocEvents_DragStateChangeNotifyEventHandler(OnPartDragStateChangeNotify);
			SwPartDoc.UndoPreNotify += new DPartDocEvents_UndoPreNotifyEventHandler(OnPartUndoPreNotify);
			SwPartDoc.RedoPreNotify += new DPartDocEvents_RedoPreNotifyEventHandler(OnPartRedoPreNotify);
			SwPartDoc.RedoPostNotify += new DPartDocEvents_RedoPostNotifyEventHandler(OnPartRedoPostNotify);
			SwPartDoc.AddItemNotify += new DPartDocEvents_AddItemNotifyEventHandler(OnPartAddItemNotify);
			SwPartDoc.ActiveConfigChangePostNotify += new DPartDocEvents_ActiveConfigChangePostNotifyEventHandler(OnPartActiveConfigChangePostNotify);
			SwPartDoc.FileReloadNotify += new DPartDocEvents_FileReloadNotifyEventHandler(OnPartFileReloadNotify);
			SwPartDoc.ActiveConfigChangeNotify += new DPartDocEvents_ActiveConfigChangeNotifyEventHandler(OnPartActiveConfigChangeNotify);
			SwPartDoc.FileSaveAsNotify += new DPartDocEvents_FileSaveAsNotifyEventHandler(OnPartFileSaveAsNotify);
			SwPartDoc.FileSaveNotify += new DPartDocEvents_FileSaveNotifyEventHandler(OnPartFileSaveNotify);
			SwPartDoc.ViewNewNotify += new DPartDocEvents_ViewNewNotifyEventHandler(OnPartViewNewNotify);
			SwPartDoc.DimensionChangeNotify += new DPartDocEvents_DimensionChangeNotifyEventHandler(OnPartDimensionChangeNotify);
			SwPartDoc.RegenPostNotify2 += new DPartDocEvents_RegenPostNotify2EventHandler(OnPartRegenPostNotify2);
			SwPartDoc.BodyVisibleChangeNotify += new DPartDocEvents_BodyVisibleChangeNotifyEventHandler(OnPartBodyVisibleChangeNotify);
			SwPartDoc.FileSaveAsNotify2 += new DPartDocEvents_FileSaveAsNotify2EventHandler(OnPartFileSaveAsNotify2);
			SwPartDoc.FileSavePostNotify += new DPartDocEvents_FileSavePostNotifyEventHandler(OnPartFileSavePostNotify);

		}

		public void Destroy()
		{
			SwPartDoc.UndoPostNotify -= new DPartDocEvents_UndoPostNotifyEventHandler(OnPartUndoPostNotify);
			SwPartDoc.SuppressionStateChangeNotify -= new DPartDocEvents_SuppressionStateChangeNotifyEventHandler(OnPartSuppressionStateChangeNotify);
			SwPartDoc.DestroyNotify2 -= new DPartDocEvents_DestroyNotify2EventHandler(OnPartDocumentDestroyNotify2);
			SwPartDoc.EquationEditorPostNotify -= new DPartDocEvents_EquationEditorPostNotifyEventHandler(OnPartEquationEditorPostNotify);
			SwPartDoc.ConfigurationChangeNotify -= new DPartDocEvents_ConfigurationChangeNotifyEventHandler(OnPartConfigurationChangeNotify);
			SwPartDoc.ActiveDisplayStateChangePostNotify -= new DPartDocEvents_ActiveDisplayStateChangePostNotifyEventHandler(OnPartActiveDisplayStateChangePostNotify);
			SwPartDoc.DragStateChangeNotify -= new DPartDocEvents_DragStateChangeNotifyEventHandler(OnPartDragStateChangeNotify);
			SwPartDoc.UndoPreNotify -= new DPartDocEvents_UndoPreNotifyEventHandler(OnPartUndoPreNotify);
			SwPartDoc.RedoPreNotify -= new DPartDocEvents_RedoPreNotifyEventHandler(OnPartRedoPreNotify);
			SwPartDoc.RedoPostNotify -= new DPartDocEvents_RedoPostNotifyEventHandler(OnPartRedoPostNotify);
			SwPartDoc.AddItemNotify -= new DPartDocEvents_AddItemNotifyEventHandler(OnPartAddItemNotify);
			SwPartDoc.ActiveConfigChangePostNotify -= new DPartDocEvents_ActiveConfigChangePostNotifyEventHandler(OnPartActiveConfigChangePostNotify);
			SwPartDoc.FileReloadNotify -= new DPartDocEvents_FileReloadNotifyEventHandler(OnPartFileReloadNotify);
			SwPartDoc.ActiveConfigChangeNotify -= new DPartDocEvents_ActiveConfigChangeNotifyEventHandler(OnPartActiveConfigChangeNotify);
			SwPartDoc.FileSaveAsNotify -= new DPartDocEvents_FileSaveAsNotifyEventHandler(OnPartFileSaveAsNotify);
			SwPartDoc.FileSaveNotify -= new DPartDocEvents_FileSaveNotifyEventHandler(OnPartFileSaveNotify);
			SwPartDoc.ViewNewNotify -= new DPartDocEvents_ViewNewNotifyEventHandler(OnPartViewNewNotify);
			SwPartDoc.DimensionChangeNotify -= new DPartDocEvents_DimensionChangeNotifyEventHandler(OnPartDimensionChangeNotify);
			SwPartDoc.RegenPostNotify2 -= new DPartDocEvents_RegenPostNotify2EventHandler(OnPartRegenPostNotify2);
			SwPartDoc.BodyVisibleChangeNotify -= new DPartDocEvents_BodyVisibleChangeNotifyEventHandler(OnPartBodyVisibleChangeNotify);
			SwPartDoc.FileSaveAsNotify2 -= new DPartDocEvents_FileSaveAsNotify2EventHandler(OnPartFileSaveAsNotify2);
			SwPartDoc.FileSavePostNotify -= new DPartDocEvents_FileSavePostNotifyEventHandler(OnPartFileSavePostNotify);
		}

		int OnPartDocumentDestroyNotify2(int DestroyType)
		{
			Addin.Instance.CloseDocument(DocId);
			return 0;
		}

		int OnPartUndoPostNotify()
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		// part has become resolved or lightweight or suppressed
		int OnPartSuppressionStateChangeNotify(Feature Feature, int NewSuppressionState, int PreviousSuppressionState, int ConfigurationOption, ref object ConfigurationNames)
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		int OnPartEquationEditorPostNotify(Boolean Changed)
		{
			return 0;
		}

		int OnPartConfigurationChangeNotify(string ConfigurationName, object Object, int ObjectType, int changeType)
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		int OnPartActiveDisplayStateChangePostNotify(string DisplayStateName)
		{
			return 0;
		}

		int OnPartDragStateChangeNotify(Boolean State)
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		int OnPartUndoPreNotify()
		{
			return 0;
		}

		int OnPartRedoPreNotify()
		{
			return 0;
		}

		int OnPartRedoPostNotify()
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		int OnPartAddItemNotify(int InEntityType, string InItemName)
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		int OnPartActiveConfigChangePostNotify()
		{
			return 0;
		}

		int OnPartFileReloadNotify()
		{
			return 0;
		}

		int OnPartActiveConfigChangeNotify()
		{
			return 0;
		}

		int OnPartFileSaveAsNotify(string FileName)
		{
			return 0;
		}

		int OnPartFileSaveNotify(string FileName)
		{
			return 0;
		}

		int OnPartViewNewNotify()
		{
			return 0;
		}

		int OnPartDimensionChangeNotify(object displayDim)
		{
			return 0;
		}

		int OnPartRegenPostNotify2(object stopFeature)
		{
			DocumentTracker.SetDirty(true);
			return 0;
		}

		int OnPartBodyVisibleChangeNotify()
		{
			return 0;
		}

		int OnPartFileSaveAsNotify2(string FileName)
		{
			return 0;
		}

		int OnPartFileSavePostNotify(int saveType, string FileName)
		{
			return 0;
		}
	}

	public class FPartDocumentNotifications
	{
		private readonly FPartDocumentTracker DocumentTracker;
		private readonly FPartDocumentEvents Events;

		public FPartDocumentNotifications(int InDocId, PartDoc InPartDoc, FPartDocumentTracker InDocumentTracker)
		{
			DocumentTracker = InDocumentTracker;
			Events = new FPartDocumentEvents(InDocumentTracker);
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
	
	public class FPartDocumentSyncer: IDocumentSyncer
	{
		public readonly FPartDocumentNotifications Notifications;
		public readonly FPartDocumentTracker DocumentTracker;

		public FPartDocumentSyncer(int InDocId, FPartDocument InDoc, PartDoc InPartDoc, FDatasmithExporter InExporter, FAssemblyDocumentTracker InAsmDoc, FComponentName InComponentName)
		{
			DocumentTracker = new FPartDocumentTracker(InDoc, InPartDoc, InExporter, InAsmDoc, InComponentName);
			Notifications = new FPartDocumentNotifications(InDocId, InPartDoc, DocumentTracker);
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

		public void SetDirty(bool bInDirty)
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

	public class FPartDocument: FDocument
	{
		public PartDoc SwPartDoc { get; private set; } = null;

		private readonly FPartDocumentEvents Events;

		public FPartDocument(int InDocId, PartDoc InPartDoc, FDatasmithExporter InExporter, FAssemblyDocumentTracker InAsmDoc, FComponentName InComponentName) 
				: base(InDocId, InPartDoc as ModelDoc2)
		{
			SwPartDoc = InPartDoc;

			if (InAsmDoc == null)  
			{
				// This is the top document and should track/sync its DatasmithScene
				DocumentSyncer = new FPartDocumentSyncer(InDocId, this, InPartDoc, InExporter, InAsmDoc, InComponentName);
			}
			else
			{
				// Assembly Document passed when this Part Document is within an assembly and only needs to propagate events to that assembly
				// todo: might split this from FPartDocument?
				Events = new FPartDocumentEvents(new FPartDocumentTracker(this, InPartDoc, InExporter, InAsmDoc, InComponentName));
			}
		}

		public override void Export(string InFilePath)
		{
			FPartDocumentTracker Tracker = new FPartDocumentTracker(this, SwPartDoc, null, null, new FComponentName());
			Tracker.DatasmithFileExportPath = InFilePath;
			Tracker.Export();
		}

		public override void Destroy()
		{
			Events?.Destroy();
			base.Destroy();
		}
	}
}