// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Autodesk.Revit.DB;
using Autodesk.Revit.ApplicationServices;
using Autodesk.Revit.DB.Events;
using System.Linq;
using Autodesk.Revit.UI;

namespace DatasmithRevitExporter
{
	public class FDirectLink
	{
		private class FCachedDocumentData
		{
			public Document															SourceDocument;
			public Dictionary<ElementId, FDocumentData.FBaseElementData>			CachedElements = new Dictionary<ElementId, FDocumentData.FBaseElementData>();
			public Queue<KeyValuePair<ElementId, FDocumentData.FBaseElementData>>	ElementsWithoutMetadataQueue = new Queue<KeyValuePair<ElementId, FDocumentData.FBaseElementData>>();
			public HashSet<ElementId>												ElementsWithoutMetadataSet = new HashSet<ElementId>();
			public HashSet<ElementId>												ExportedElements = new HashSet<ElementId>();
			private HashSet<ElementId>												ModifiedElements = new HashSet<ElementId>();
			public Dictionary<string, FDatasmithFacadeActor>						ExportedActorsMap = new Dictionary<string, FDatasmithFacadeActor>();

			public Dictionary<ElementId, FCachedDocumentData>						LinkedDocumentsCache = new Dictionary<ElementId, FCachedDocumentData>();
		
			public FCachedDocumentData(Document InDocument)
			{
				SourceDocument = InDocument;
			}

			public bool ElementIsModified(ElementId ElemId)
			{
				return ModifiedElements.Contains(ElemId);
			}

			public void SetElementModified(bool bModified, ElementId ElemId)
			{
				if (bModified)
				{
					if (!ModifiedElements.Contains(ElemId))
					{
						ModifiedElements.Add(ElemId);
						if (CachedElements.ContainsKey(ElemId))
						{
							FDocumentData.FBaseElementData ElemData = CachedElements[ElemId];

							if (!ElementsWithoutMetadataSet.Contains(ElemId))
							{
								ElementsWithoutMetadataQueue.Enqueue(new KeyValuePair<ElementId, FDocumentData.FBaseElementData>(ElemId, ElemData));
								ElementsWithoutMetadataSet.Add(ElemId);
							}
						}
					}
				}
				else
				{
					ModifiedElements.Remove(ElemId);
				}
			}

			public int GetModifiedElementsCount()
			{
				return ModifiedElements.Count;
			}

			public void SetAllElementsModified()
			{
				foreach (var Link in LinkedDocumentsCache.Values)
				{
					Link.SetAllElementsModified();
				}

				ModifiedElements.Clear();
				ElementsWithoutMetadataSet.Clear();
				ElementsWithoutMetadataQueue.Clear();

				foreach (var ElemId in CachedElements.Keys)
				{
					SetElementModified(true, ElemId);
				}
			}

			public void ClearModified()
			{
				foreach (var Link in LinkedDocumentsCache.Values)
				{
					Link.ClearModified();
				}
				ModifiedElements.Clear();
			}

			// Intersect elements exported in this sync with cached elements.
			// Those out of the intersection set are either deleted or hidden 
			// and need to be removed from cache.
			public void Purge(FDatasmithFacadeScene DatasmithScene, bool bInRecursive)
			{
				if (bInRecursive)
				{
					// Call purge on linked docs first.
					foreach (var Link in LinkedDocumentsCache.Values)
					{
						Link.Purge(DatasmithScene, true);
					}
				}

				List<ElementId> ElementsToRemove = new List<ElementId>();
				foreach (var ElemId in CachedElements.Keys)
				{
					if (!ExportedElements.Contains(ElemId))
					{
						ElementsToRemove.Add(ElemId);
					}
				}

				// Apply deletions according to the accumulated sets of elements.
				foreach (var ElemId in ElementsToRemove)
				{
					if (!CachedElements.ContainsKey(ElemId))
					{
						continue;
					}
					FDocumentData.FBaseElementData ElementData = CachedElements[ElemId];
					CachedElements.Remove(ElemId);
					ElementData.Parent?.ChildElements.Remove(ElementData);

					// Remove all owned children
					foreach (FDocumentData.FBaseElementData ChildElementData in ElementData.ChildElements)
					{
						if (ChildElementData.bOwnedByParent && (ChildElementData.ElementActor != null))
						{
							ElementData.ElementActor.RemoveChild(ChildElementData.ElementActor);
						}
					}

					DatasmithScene.RemoveActor(ElementData.ElementActor, FDatasmithFacadeScene.EActorRemovalRule.KeepChildrenAndKeepRelativeTransform);
					ExportedActorsMap.Remove(ElementData.ElementActor.GetName());
				}
			}
		};

		struct SectionBoxInfo
		{
			public Element SectionBox;
			// Store bounding box because removed section boxes lose their bounding box 
			// and we can't query it anymore
			public BoundingBoxXYZ SectionBoxBounds;
		};

		public FDatasmithFacadeScene									DatasmithScene { get; private set; }

		private FCachedDocumentData										RootCache = null;
		private FCachedDocumentData										CurrentCache = null;
		
		public View3D													SyncView { get; private set; } = null;

		private HashSet<Document>										ModifiedLinkedDocuments = new HashSet<Document>();
		private HashSet<ElementId>										ExportedLinkedDocuments = new HashSet<ElementId>();
		private Stack<FCachedDocumentData>								CacheStack = new Stack<FCachedDocumentData>();

		private IList<SectionBoxInfo>									PrevSectionBoxes = new List<SectionBoxInfo>();

		// Sets of elements related to current sync.
		private HashSet<ElementId>										DeletedElements = new HashSet<ElementId>();
		
		public HashSet<string>											UniqueTextureNameSet = new HashSet<string>();
		public Dictionary<string, FMaterialData>						MaterialDataMap = new Dictionary<string, FMaterialData>();

		private FDatasmithFacadeDirectLink								DatasmithDirectLink;
		private string													SceneName;

		public Dictionary<string, FDocumentData> ExportedDocuments 		= new Dictionary<string, FDocumentData>();
		private Dictionary<string, int>									ExportedActorNames = new Dictionary<string, int>();

		// The number of times this document was synced (sent to receiver)
		public int														SyncCount { get; private set; } = 0;

		private EventHandler<DocumentChangedEventArgs>					DocumentChangedHandler;

		private FSettings												Settings = null;

		private static UIApplication UIApp { get; set; } = null;

		private bool bHasChanges = false;
		private bool bSyncInProgress = false;

		public bool bSettingsDirty = false;

		private static bool _bAutoSync = false;
		public static bool bAutoSync
		{
			get
			{
				return _bAutoSync;
			}
			set
			{
				_bAutoSync = value;
				if (_bAutoSync)
				{
					FDocument.ActiveDocument?.ActiveDirectLinkInstance?.RunAutoSync();
				}
			}
		}

		public Dictionary<ElementId, ElementId> DecalIdToOwnerObjectIdMap = new Dictionary<ElementId, ElementId>();

		public static void OnApplicationIdle()
		{
			FDirectLink ActiveInstance = FDocument.ActiveDocument?.ActiveDirectLinkInstance ?? null;

			if (ActiveInstance == null)
			{
				return;
			}

			//OnDocumentChanged event can happen in intervals where PostCommand is blocked/ignored by Revit
			//so instead of executing RunAutoSync in OnDocumentChanged, we execute it here (OnApplicationIdle),
			//OnApplicationIdle is only executed in intervals where PostCommand is NOT blocked/ignored.
			if (ActiveInstance.bHasChanges && bAutoSync)
			{
				ActiveInstance.RunAutoSync();
			}
		}

		public static void OnDocumentChanged(
		  object InSender,
		  DocumentChangedEventArgs InArgs) 
		{
			FDirectLink ActiveInstance = FDocument.ActiveDocument?.ActiveDirectLinkInstance ?? null;

			Debug.Assert(ActiveInstance != null);

			if (ActiveInstance == null)
			{
				return;
			}

			// Handle modified elements
			foreach (ElementId ElemId in InArgs.GetModifiedElementIds())
			{
				Element ModifiedElement = ActiveInstance.RootCache.SourceDocument.GetElement(ElemId);

				ActiveInstance.bHasChanges = true;

				if (ModifiedElement != null)
				{
					if (ModifiedElement.GetType() == typeof(RevitLinkInstance))
					{
						ActiveInstance.ModifiedLinkedDocuments.Add((ModifiedElement as RevitLinkInstance).GetLinkDocument());
					}
					else if (ModifiedElement.GetType() == typeof(RevitLinkType))
					{
						foreach (KeyValuePair<ElementId, FCachedDocumentData> Link in ActiveInstance.RootCache.LinkedDocumentsCache)
						{
							RevitLinkInstance LinkInstance = ActiveInstance.RootCache.SourceDocument.GetElement(Link.Key) as RevitLinkInstance;
							if (LinkInstance != null)
							{
								RevitLinkType LinkType = ActiveInstance.RootCache.SourceDocument.GetElement(LinkInstance.GetTypeId()) as RevitLinkType;
								if (LinkType != null && LinkType.Id == ModifiedElement.Id && RevitLinkType.IsLoaded(ActiveInstance.RootCache.SourceDocument, LinkType.Id))
								{
									Link.Value.SetAllElementsModified();
								}
							}
						}
					}
					else
					{
						if (FUtils.IsElementDecal(ModifiedElement))
						{
							//decal:
							//modifying decal does not modify the owner object
							if (ActiveInstance.DecalIdToOwnerObjectIdMap.TryGetValue(ElemId, out var OwnerObjectElementId))
							{
								ActiveInstance.RootCache.SetElementModified(true, OwnerObjectElementId);
							}
						}

						// Handles a case where Revit won't notify us about modified mullions and their transform remains obsolte, thus wrong.
						ElementCategoryFilter Filter = new ElementCategoryFilter(BuiltInCategory.OST_CurtainWallMullions);
						IList<ElementId> DependentElements = ModifiedElement.GetDependentElements(Filter);
						if (DependentElements != null && DependentElements.Count > 0)
						{
							foreach (ElementId DepElemId in DependentElements)
							{
								ActiveInstance.RootCache.SetElementModified(true, DepElemId);
							}
						}
					}

					ActiveInstance.RootCache.SetElementModified(true, ElemId);
				}
			}

			foreach (ElementId ElemId in InArgs.GetAddedElementIds())
			{
				Element AddedElement = ActiveInstance.RootCache.SourceDocument.GetElement(ElemId);

				if (FUtils.IsElementDecal(AddedElement))
				{
					FilteredElementCollector Collector = new FilteredElementCollector(ActiveInstance.RootCache.SourceDocument, ActiveInstance.RootCache.SourceDocument.ActiveView.Id);
					IList<Element> AllElementsInView = Collector.ToElements();

					bool ElementFound = false;

					//find the Element that is dependent on the newly added decal:
					foreach (Element ElementInView in AllElementsInView)
					{
#if REVIT_API_2023
						if (ElementInView.Category != null && ElementInView.Category.BuiltInCategory != BuiltInCategory.OST_Levels)
#else
						if (ElementInView.Category != null && (BuiltInCategory)ElementInView.Category.Id.IntegerValue != BuiltInCategory.OST_Levels)
#endif
						{
							foreach (ElementId DependentElementId in FUtils.GetAllDependentElements(ElementInView))
							{
								if (DependentElementId == ElemId)
								{
									ActiveInstance.RootCache.SetElementModified(true, ElementInView.Id);
									ElementFound = true;
									break;
								}
							}

							if (ElementFound)
							{
								break;
							}
						}
					}
				}
			}

			foreach (ElementId ElemId in InArgs.GetDeletedElementIds())
			{
				//checking if decal was removed:
				if (ActiveInstance.DecalIdToOwnerObjectIdMap.ContainsKey(ElemId))
				{
					ActiveInstance.DecalIdToOwnerObjectIdMap.Remove(ElemId);
				}
				//checking if owner object was removed:
				if (ActiveInstance.DecalIdToOwnerObjectIdMap.ContainsValue(ElemId))
				{
					KeyValuePair<ElementId, ElementId>[] PairsToRemove = ActiveInstance.DecalIdToOwnerObjectIdMap.Where(CurrentPair => CurrentPair.Value == ElemId).ToArray();
					foreach (KeyValuePair<ElementId, ElementId> PairToRemove in PairsToRemove)
					{
						ActiveInstance.DecalIdToOwnerObjectIdMap.Remove(PairToRemove.Key);
					}
				}
			}

			ActiveInstance.bHasChanges = ActiveInstance.bHasChanges || InArgs.GetDeletedElementIds().Any();
			ActiveInstance.bHasChanges = ActiveInstance.bHasChanges || InArgs.GetAddedElementIds().Any();
		}

		public FDirectLink(View3D InView, FSettings InSettings)
		{
			Settings = InSettings;
			RootCache = new FCachedDocumentData(InView.Document);
			CurrentCache = RootCache;
			SyncView = InView;

			DatasmithScene = new FDatasmithFacadeScene(
				FDatasmithRevitExportContext.HOST_NAME,
				FDatasmithRevitExportContext.VENDOR_NAME,
				FDatasmithRevitExportContext.PRODUCT_NAME,
				InView.Document.Application.VersionNumber);

			SceneName = $"{Path.GetFileNameWithoutExtension(RootCache.SourceDocument.PathName)}_{InView.Name}";
			string OutputPath = Path.Combine(Path.GetTempPath(), SceneName);
			DatasmithScene.SetName(SceneName);
			DatasmithScene.SetLabel(SceneName);

			DocumentChangedHandler = new EventHandler<DocumentChangedEventArgs>(OnDocumentChanged);
			InView.Document.Application.DocumentChanged += DocumentChangedHandler;
		}

		public Document GetRootDocument()
		{
			return RootCache.SourceDocument;
		}

		private void RunAutoSync()
		{
			if (bSyncInProgress)
			{
				return;
			}

			if (bHasChanges || SyncCount == 0)
			{
				string CmdGUID = null;

#if REVIT_API_2018
				CmdGUID = "D38EF9AC-C9B0-4578-8FD2-B4065FEFFABD";
#elif REVIT_API_2019
				CmdGUID = "44342F25-7B40-4E5C-A3AA-D94C201D95E8";
#elif REVIT_API_2020
				CmdGUID = "0A6A844F-F738-4E7E-A53D-BE8C45CD12A9";
#elif REVIT_API_2021
				CmdGUID = "66E62F4C-9F5A-4A3F-94C4-E6DCE838C413";
#elif REVIT_API_2022
				CmdGUID = "3478AAF5-75A6-4E6F-BBF8-FFD791CA1801";
#elif REVIT_API_2023
				CmdGUID = "CB3186CC-1714-497A-9A54-A5D4B726524A";
#else
#error This version of Revit is not supported yet.
#endif

				if (UIApp == null)
				{
					UIApp = new UIApplication(SyncView.Document.Application);
				}

				RevitCommandId CmdId = RevitCommandId.LookupCommandId(CmdGUID.ToLower());
				if (CmdId != null)
				{
					UIApp.PostCommand(CmdId);
				}
			}
		}

		public void MakeActive(bool bInActive)
		{
			if (!bInActive)
			{
				DatasmithDirectLink = null;
			}
			else if (DatasmithDirectLink == null)
			{
				DatasmithDirectLink = new FDatasmithFacadeDirectLink();

				if (!DatasmithDirectLink.InitializeForScene(DatasmithScene))
				{
					throw new Exception("DirectLink: failed to initialize");
				}
			}
		}

		public void Destroy(Application InApp)
		{
			InApp.DocumentChanged -= DocumentChangedHandler;
			DocumentChangedHandler = null;

			DatasmithDirectLink?.CloseCurrentSource();
			DatasmithDirectLink = null;
			DatasmithScene = null;
			RootCache = null;
			ModifiedLinkedDocuments.Clear();
		}

		public bool IsMaterialDirty(Material InMaterial)
		{
			if (InMaterial != null)
			{
				return RootCache.ElementIsModified(InMaterial.Id);
			}
			return false;
		}

		public void SetMaterialClean(Material InMaterial)
		{
			if (InMaterial != null)
			{
				RootCache.SetElementModified(false, InMaterial.Id);
			}
		}
		public void MarkForExport(Element InElement)
		{
			if (InElement.GetType() == typeof(RevitLinkInstance))
			{
				// We want to track which links are exported and later removed the ones that 
				// were deleted from root document.
				if (!ExportedLinkedDocuments.Contains(InElement.Id))
				{
					ExportedLinkedDocuments.Add(InElement.Id);
				}
			}

			CurrentCache.ExportedElements.Add(InElement.Id);
		}

		public void ClearModified(Element InElement)
		{
			// Clear from modified set since we might get another element with same id and we dont want to skip it.
			CurrentCache.SetElementModified(false, InElement.Id);
		}

		public void CacheElement(Document InDocument, Element InElement, FDocumentData.FBaseElementData InElementData)
		{
			if (!CurrentCache.CachedElements.ContainsKey(InElement.Id))
			{
				CurrentCache.CachedElements[InElement.Id] = InElementData;

				if (!CurrentCache.ElementsWithoutMetadataSet.Contains(InElement.Id))
				{
					CurrentCache.ElementsWithoutMetadataQueue.Enqueue(new KeyValuePair<ElementId, FDocumentData.FBaseElementData>(InElement.Id, InElementData));
					CurrentCache.ElementsWithoutMetadataSet.Add(InElement.Id);
				}
			}
			CacheActorType(InElementData.ElementActor);
		}

		public void CacheActorType(FDatasmithFacadeActor InActor)
		{
			if (CurrentCache != null)
			{
				CurrentCache.ExportedActorsMap[InActor.GetName()] = InActor;
			}
		}

		public FDocumentData.FBaseElementData GetCachedElement(Element InElement)
		{
			FDocumentData.FBaseElementData Result = null;
			if (CurrentCache.CachedElements.TryGetValue(InElement.Id, out Result))
			{
				FDocumentData.FElementData ElementData = Result as FDocumentData.FElementData;
				if (ElementData != null)
				{
					// Re-init the element ref: in some cases (family instance update) it might become invalid.
					ElementData.CurrentElement = InElement; 
				}
			}
			return Result;
		}

		public FDatasmithFacadeActor GetCachedActor(string InActorName)
		{
			FDatasmithFacadeActor Actor = null;
			if (CurrentCache != null)
			{
				CurrentCache.ExportedActorsMap.TryGetValue(InActorName, out Actor);
			}
			return Actor;
		}

		public string EnsureUniqueActorName(string InActorName)
		{
			string UniqueName = InActorName;
			if (ExportedActorNames.ContainsKey(InActorName))
			{
				UniqueName = $"{InActorName}_{ExportedActorNames[InActorName]++}";
			}
			else
			{
				ExportedActorNames[InActorName] = 0;
			}

			return UniqueName;
		}

		public bool IsElementCached(Element InElement)
		{
			return CurrentCache.CachedElements.ContainsKey(InElement.Id);
		}

		public bool IsElementModified(Element InElement)
		{
			return CurrentCache.ElementIsModified(InElement.Id);
		}

		public void OnBeginLinkedDocument(Element InLinkElement)
		{
			Debug.Assert(InLinkElement.GetType() == typeof(RevitLinkInstance));

			Document LinkedDoc = (InLinkElement as RevitLinkInstance).GetLinkDocument();
			Debug.Assert(LinkedDoc != null);

			if (!CurrentCache.LinkedDocumentsCache.ContainsKey(InLinkElement.Id))
			{
				CurrentCache.LinkedDocumentsCache[InLinkElement.Id] = new FCachedDocumentData(LinkedDoc);
			}
			CacheStack.Push(CurrentCache.LinkedDocumentsCache[InLinkElement.Id]);
			CurrentCache = CurrentCache.LinkedDocumentsCache[InLinkElement.Id];
		}

		public void OnEndLinkedDocument()
		{
			CacheStack.Pop();
			CurrentCache = CacheStack.Count > 0 ? CacheStack.Peek() : RootCache;
		}

		public void OnBeginExport()
		{
			if (SyncCount > 0 && bSettingsDirty)
			{
				RootCache?.SetAllElementsModified();
			}

			bSettingsDirty = false;
			bSyncInProgress = true;

			SetSceneCachePath();

			foreach (var Link in RootCache.LinkedDocumentsCache.Values)
			{
				if (Link.SourceDocument.IsValidObject && ModifiedLinkedDocuments.Contains(Link.SourceDocument))
				{
					Link.SetAllElementsModified();
				}
			}

			// Handle section boxes.
			FilteredElementCollector Collector = new FilteredElementCollector(RootCache.SourceDocument, RootCache.SourceDocument.ActiveView.Id);
			List<SectionBoxInfo> CurrentSectionBoxes =  new List<SectionBoxInfo>();

			foreach (Element SectionBox in Collector.OfCategory(BuiltInCategory.OST_SectionBox).ToElements())
			{
				SectionBoxInfo Info = new SectionBoxInfo();
				Info.SectionBox = SectionBox;
				Info.SectionBoxBounds = SectionBox.get_BoundingBox(RootCache.SourceDocument.ActiveView);
				CurrentSectionBoxes.Add(Info);
			}

			List<SectionBoxInfo> ModifiedSectionBoxes = new List<SectionBoxInfo>();

			foreach(SectionBoxInfo CurrentSectionBoxInfo in CurrentSectionBoxes)
			{
				if (!RootCache.ElementIsModified(CurrentSectionBoxInfo.SectionBox.Id))
				{
					continue;
				}

				ModifiedSectionBoxes.Add(CurrentSectionBoxInfo);
			}

			// Check for old section boxes that were disabled since last sync.
			foreach (SectionBoxInfo PrevSectionBoxInfo in PrevSectionBoxes)
			{
				bool bSectionBoxWasDisabled = !CurrentSectionBoxes.Any(Info => Info.SectionBox.Id == PrevSectionBoxInfo.SectionBox.Id);

				if (bSectionBoxWasDisabled)
				{
					// Section box was removed, need to mark the elemets it intersected as modified
					ModifiedSectionBoxes.Add(PrevSectionBoxInfo);
				}
			}

			// Check all elements that need to be re-exported
			foreach(var SectionBoxInfo in ModifiedSectionBoxes)
			{
				MarkIntersectedElementsAsModified(RootCache, SectionBoxInfo.SectionBox, SectionBoxInfo.SectionBoxBounds);
			}

			PrevSectionBoxes = CurrentSectionBoxes;
		}

		void SetSceneCachePath()
		{
			string OutputPath = null;

			IDirectLinkUI DirectLinkUI = IDatasmithExporterUIModule.Get()?.GetDirectLinkExporterUI();
			if (DirectLinkUI != null)
			{
				OutputPath = Path.Combine(DirectLinkUI.GetDirectLinkCacheDirectory(), SceneName);
			}
			else
			{
				OutputPath = Path.Combine(Path.GetTempPath(), SceneName);
			}

			if (!Directory.Exists(OutputPath))
			{
				Directory.CreateDirectory(OutputPath);
			}

			DatasmithScene.SetOutputPath(OutputPath);
		}

		void MarkIntersectedElementsAsModified(FCachedDocumentData InData, Element InSectionBox, BoundingBoxXYZ InSectionBoxBounds)
		{
			ElementFilter IntersectFilter = new BoundingBoxIntersectsFilter(new Outline(InSectionBoxBounds.Min, InSectionBoxBounds.Max));
			ICollection<ElementId> IntersectedElements = new FilteredElementCollector(InData.SourceDocument).WherePasses(IntersectFilter).ToElementIds();

			ElementFilter InsideFilter = new BoundingBoxIsInsideFilter(new Outline(InSectionBoxBounds.Min, InSectionBoxBounds.Max));
			ICollection<ElementId> InsideElements = new FilteredElementCollector(InData.SourceDocument).WherePasses(InsideFilter).ToElementIds();

			// Elements that are fully inside the section box should not be marked modified to save export time
			foreach (ElementId InsideElement in InsideElements)
			{
				IntersectedElements.Remove(InsideElement);
			}

			foreach (var ElemId in IntersectedElements)
			{
				if (!InData.ElementIsModified(ElemId))
				{
					InData.SetElementModified(true, ElemId);
				}
			}

			// Run the linked documents
			foreach (var LinkedDoc in InData.LinkedDocumentsCache)
			{
				MarkIntersectedElementsAsModified(LinkedDoc.Value, InSectionBox, InSectionBoxBounds);

				if (LinkedDoc.Value.GetModifiedElementsCount() > 0)
				{
					InData.SetElementModified(true, LinkedDoc.Key);
					ModifiedLinkedDocuments.Add(LinkedDoc.Value.SourceDocument);
				}
			}
		}

		void ProcessLinkedDocuments()
		{
			List<ElementId> LinkedDocumentsToRemove = new List<ElementId>();

			// Check for modified linked documents.
			foreach (var LinkedDocEntry in RootCache.LinkedDocumentsCache)
			{
				// Check if the link was removed.
				if (!ExportedLinkedDocuments.Contains(LinkedDocEntry.Key))
				{
					LinkedDocumentsToRemove.Add(LinkedDocEntry.Key);
					continue;
				}

				// Check if the link was modified.
				FCachedDocumentData LinkedDocCache = LinkedDocEntry.Value;
				if (ModifiedLinkedDocuments.Contains(LinkedDocCache.SourceDocument))
				{
					LinkedDocCache.Purge(DatasmithScene, true);
				}
				LinkedDocCache.ExportedElements.Clear();
			}

			foreach (var LinkedDoc in LinkedDocumentsToRemove)
			{
				RootCache.LinkedDocumentsCache[LinkedDoc].Purge(DatasmithScene, true);
				RootCache.LinkedDocumentsCache.Remove(LinkedDoc);
			}
		}

		// Sync materials: DatasmithScene.CleanUp() might have deleted some materials that are not referenced by 
		// meshes anymore, so we need to update our map.
		void SyncMaterials()
		{
			HashSet<string> SceneMaterials = new HashSet<string>();
			for (int MaterialIndex = 0; MaterialIndex < DatasmithScene.GetMaterialsCount(); ++MaterialIndex)
			{
				FDatasmithFacadeBaseMaterial Material = DatasmithScene.GetMaterial(MaterialIndex);
				SceneMaterials.Add(Material.GetName());
			}

			List<string> MaterialsToDelete = new List<string>();

			foreach (var MaterialKV in MaterialDataMap)
			{
				string MaterialName = MaterialKV.Key;

				if (!SceneMaterials.Contains(MaterialName))
				{
					MaterialsToDelete.Add(MaterialName);
				}
			}
			foreach (string MaterialName in MaterialsToDelete)
			{
				MaterialDataMap.Remove(MaterialName);
			}
		}

		// Sync textures: DatasmithScene.CleanUp() might have deleted some textures that are not referenced by 
		// materials anymore, so we need to update our cache.
		void SyncTextures()
		{
			HashSet<string> SceneTextures = new HashSet<string>();
			for (int TextureIndex = 0; TextureIndex < DatasmithScene.GetTexturesCount(); ++TextureIndex)
			{
				FDatasmithFacadeTexture Texture = DatasmithScene.GetTexture(TextureIndex);
				SceneTextures.Add(Texture.GetName());
			}

			List<string> TexturesToDelete = new List<string>();

			foreach (var CachedTextureName in UniqueTextureNameSet)
			{
				if (!SceneTextures.Contains(CachedTextureName))
				{
					TexturesToDelete.Add(CachedTextureName);
				}
			}
			foreach (string TextureName in TexturesToDelete)
			{
				UniqueTextureNameSet.Remove(TextureName);
			}
		}

		public void OnEndExport()
		{
			if (RootCache.LinkedDocumentsCache.Count > 0)
			{
				ProcessLinkedDocuments();
			}

			RootCache.Purge(DatasmithScene, false);

			ModifiedLinkedDocuments.Clear();
			ExportedLinkedDocuments.Clear();
			RootCache.ClearModified();
			RootCache.ExportedElements.Clear();

			DatasmithScene.CleanUp();
			DatasmithDirectLink.UpdateScene(DatasmithScene);

			SyncMaterials();
			SyncTextures();

			SyncCount++;

			bHasChanges = false;
			bSyncInProgress = false;
		}

		//The number of metadata transfers in one go will be revisited in a future release since it also requires changes in the import of metadata.
		// For now however all metadata will be transfered in one go.
		// ExportMetadataBatch is called from the DatasmithSyncRevitCommand.OnExecute/DatasmithExportRevitCommand.OnExecute functions (instead of getting triggered when Revit is idle).
		public void ExportMetadataBatch()
		{
			int CurrentBatchSize = 0;

			Action<FCachedDocumentData> AddElements = (FCachedDocumentData CacheData) => 
			{
				while (CacheData.ElementsWithoutMetadataQueue.Count > 0)
				{
					var Entry = CacheData.ElementsWithoutMetadataQueue.Dequeue();

					CacheData.ElementsWithoutMetadataSet.Remove(Entry.Key);

					// Handle the case where element might be deleted in the main export path.
					if (!CacheData.CachedElements.ContainsKey(Entry.Key))
					{
						continue;
					}

					if (!CacheData.SourceDocument.IsValidObject)
					{
						return;
					}

					Element RevitElement = CacheData.SourceDocument.GetElement(Entry.Key);

					if (RevitElement == null)
					{
						continue;
					}

					FDocumentData.FBaseElementData ElementData = Entry.Value;
					FDatasmithFacadeActor Actor = ElementData.ElementActor;

					ElementData.ElementMetaData = new FDatasmithFacadeMetaData(Actor.GetName() + "_DATA");
					ElementData.ElementMetaData.SetLabel(Actor.GetLabel());
					ElementData.ElementMetaData.SetAssociatedElement(Actor);

					FUtils.AddActorMetadata(RevitElement, ElementData.ElementMetaData, Settings);

					DatasmithScene.AddMetaData(ElementData.ElementMetaData);

					++CurrentBatchSize;

#if DEBUG
					Debug.WriteLine($"metadata batch element {CurrentBatchSize}, remain in Q {CacheData.ElementsWithoutMetadataQueue.Count}");
#endif
				}
			};

			List<FCachedDocumentData> CachesToExport = new List<FCachedDocumentData>();

			Func<FCachedDocumentData, int> GetDocumentCaches = null;

			GetDocumentCaches = (FCachedDocumentData InParent) =>
			{
				int ElementsInQueue = InParent.ElementsWithoutMetadataQueue.Count;

				CachesToExport.Add(InParent);
				foreach (var Cache in InParent.LinkedDocumentsCache.Values) 
				{
					ElementsInQueue += GetDocumentCaches(Cache);
				}

				return ElementsInQueue;
			};

			int TotalElementsWithoutMetadata = GetDocumentCaches(RootCache);

			if (TotalElementsWithoutMetadata == 0)
			{
				return;
			}

			foreach (var Cache in CachesToExport)
			{
				AddElements(Cache);
			}

			if (CurrentBatchSize > 0)
			{
				// Send remaining chunk of metadata.
				DatasmithDirectLink?.UpdateScene(DatasmithScene);
			}

#if DEBUG
			Debug.WriteLine("metadata exported");
#endif
		}
	}
}
