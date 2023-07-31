// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Autodesk.Revit.DB;
using Autodesk.Revit.UI;

namespace DatasmithRevitExporter
{
	public class FDocument
	{
		public static FDocument ActiveDocument { get; private set; } = null;
		public static List<FDocument> AllDocuments {  get; private set; } = new List<FDocument>();
		public List<FDirectLink> DirectLinkInstances { get; private set; }  = new List<FDirectLink>();
		public FDirectLink ActiveDirectLinkInstance { get; private set; } = null;
		public Document RevitDoc { get; private set; } = null;
		public FSettings Settings { get; private set; } = null;
		private static UIApplication UIApp { get; set; } = null;

		private EventHandler SettingsChangedHandler;

		public static void SetActiveDocument(Document InRevitDoc)
		{
			FDocument NewActiveDocument = null;

			foreach (FDocument Doc in AllDocuments)
			{
				if (Doc.RevitDoc.Equals(InRevitDoc))
				{
					NewActiveDocument = Doc;
					break;
				}
			}

			if (NewActiveDocument == null)
			{
				NewActiveDocument = new FDocument();
				NewActiveDocument.RevitDoc = InRevitDoc;
				NewActiveDocument.Settings = FSettingsManager.ReadSettings(InRevitDoc);

				if (UIApp == null)
				{
					UIApp = new UIApplication(InRevitDoc.Application);
				}

				NewActiveDocument.SettingsChangedHandler = new EventHandler((object Sender, EventArgs Args) =>
				{
					if (NewActiveDocument.ActiveDirectLinkInstance != null)
					{
						NewActiveDocument.ActiveDirectLinkInstance.bSettingsDirty = true;
					}
				});

				FSettingsManager.SettingsUpdated += NewActiveDocument.SettingsChangedHandler;

				AllDocuments.Add(NewActiveDocument);
			}

			ActiveDocument = NewActiveDocument;
		}

		public static void Destroy(Document InDocument)
		{
			FDocument DocumentToDestroy = null;

			// Find document instance
			foreach (FDocument Doc in AllDocuments)
			{
				if (Doc.RevitDoc.Equals(InDocument))
				{
					DocumentToDestroy = Doc;
					break;
				}
			}

			if (DocumentToDestroy != null)
			{
				DestroyInstance(DocumentToDestroy);
			}
		}

		private static void DestroyInstance(FDocument InDoc)
		{
			FSettingsManager.SettingsUpdated -= InDoc.SettingsChangedHandler;
			InDoc.SettingsChangedHandler = null;

			// Destroy active DirectLink instances
			for (int InstanceIndex = InDoc.DirectLinkInstances.Count - 1; InstanceIndex >= 0; --InstanceIndex)
			{
				FDirectLink Instance = InDoc.DirectLinkInstances[InstanceIndex];

				if (InDoc.ActiveDirectLinkInstance == Instance)
				{
					InDoc.ActiveDirectLinkInstance = null;
				}

				InDoc.DirectLinkInstances.RemoveAt(InstanceIndex);
				Instance?.Destroy(UIApp.Application);
			}

			if (ActiveDocument == InDoc)
			{
				ActiveDocument = null;
			}

			AllDocuments.Remove(InDoc);
		}

		public static void DestroyAll()
		{
			foreach (FDocument Doc in AllDocuments)
			{
				DestroyInstance(Doc);
			}
		}

		public void SetActiveDirectLinkInstance(View3D InView)
		{
			if (ActiveDirectLinkInstance != null && ActiveDirectLinkInstance.SyncView.Id == InView.Id)
			{
				// This view is already the active one
				return;
			}

			// Disable existing instance, if there's active one.
			ActiveDirectLinkInstance?.MakeActive(false);
			ActiveDirectLinkInstance = null;

			// Find out if we already have instance for this document and 
			// activate it if we do. Otherwise, create new one.

			FDirectLink InstanceToActivate = null;

			foreach (FDirectLink DL in DirectLinkInstances)
			{
				if (DL.SyncView == null || !DL.SyncView.IsValidObject)
				{
					continue;
				}

				if (DL.GetRootDocument().Equals(InView.Document) && DL.SyncView.Id == InView.Id)
				{
					InstanceToActivate = DL;
					break;
				}
			}

			if (InstanceToActivate == null)
			{
				InstanceToActivate = new FDirectLink(InView, Settings);
				DirectLinkInstances.Add(InstanceToActivate);
			}

			InstanceToActivate.MakeActive(true);
			ActiveDirectLinkInstance = InstanceToActivate;

			if (Settings.SyncViewId.IntegerValue != InView.Id.IntegerValue)
			{
				Settings.SyncViewId = InView.Id;
				FSettingsManager.WriteSettings(RevitDoc, Settings);
			}
		}
	}
}
