// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Windows.Media.Imaging;
using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Events;
using Autodesk.Revit.UI;
using Autodesk.Revit.UI.Events;
using Microsoft.Win32;

namespace DatasmithRevitExporter
{
	// Add-in external application Datasmith Revit Exporter.
	public class DatasmithRevitApplication : IExternalApplication
	{
		// Updater notifying us when 3D views got updated/added/deleted.
		public class View3DUpdater : IUpdater
		{
			static AddInId AppId;
			static UpdaterId AppUpdaterId;

			public View3DUpdater(AddInId InId)
			{
				AppId = InId;
				AppUpdaterId = new UpdaterId(AppId, new Guid("BE5DA5A4-73C2-42BB-9346-E54F632E2B54"));
			}

			public void Execute(UpdaterData InData)
			{
				if (InData.GetAddedElementIds().Count > 0 || InData.GetDeletedElementIds().Count > 0 || InData.GetModifiedElementIds().Count > 0)
				{
					Instance.SetViewList(ElementId.InvalidElementId);
				}
			}

			public string GetAdditionalInformation()
			{
				return "";
			}

			public ChangePriority GetChangePriority()
			{
				return ChangePriority.FloorsRoofsStructuralWalls;
			}

			public UpdaterId GetUpdaterId()
			{
				return AppUpdaterId;
			}

			public string GetUpdaterName()
			{
				return "View3DUpdater";
			}
		}

		private static DatasmithRevitExportMessages ExportMessagesDialog = null;

		private static string ExportMessages;

		private EventHandler<DocumentClosingEventArgs> DocumentClosingHandler;
		private EventHandler<IdlingEventArgs> IdlingEventHandler;
		private EventHandler<DocumentOpenedEventArgs> DocumentOpenedHandler;
		private EventHandler<DocumentCreatedEventArgs> DocumentCreatedHandler;
		private EventHandler<ViewActivatedEventArgs> ViewActivatedHandler;

		private PushButton AutoSyncPushButton;
		private PushButton SyncPushButton;

		private View3DUpdater ViewsUpdater;
		private ElementId SelectedViewId = ElementId.InvalidElementId;
		private ComboBox ComboViews;
		private List<View3D> All3DViews;

		BitmapImage AutoSyncIconOn_Small;
		BitmapImage AutoSyncIconOn_Large;
		BitmapImage AutoSyncIconOff_Small;
		BitmapImage AutoSyncIconOff_Large;
		private FDebugLog DebugLog;

		public DatasmithRevitApplication()
		{
			CreateDebugLog();
		}

		public object Properties { get; private set; }

		public static DatasmithRevitApplication Instance { get; private set; }

		public void SetAutoSyncButtonToggled(bool bToggled)
		{
			if (bToggled)
			{
				AutoSyncPushButton.Image = AutoSyncIconOff_Small;
				AutoSyncPushButton.LargeImage = AutoSyncIconOff_Large;
			}
			else
			{
				AutoSyncPushButton.Image = AutoSyncIconOn_Small;
				AutoSyncPushButton.LargeImage = AutoSyncIconOn_Large;
			}
			SyncPushButton.Enabled = !bToggled;
		}

		private void OnComboBoxChanged(object s, Autodesk.Revit.UI.Events.ComboBoxCurrentChangedEventArgs e)
		{
			if (e.NewValue != null)
			{
				View3D SelectedView = All3DViews.Find(View => View.Name == e.NewValue.ItemText);

				if (SelectedView != null)
				{
					SelectedViewId = SelectedView.Id;
					FDocument.ActiveDocument?.SetActiveDirectLinkInstance(SelectedView);
				}
			}
		}

		private void ClearViews()
		{
			All3DViews.Clear();

			foreach (ComboBoxMember Item in ComboViews.GetItems())
			{
				Item.Visible = false;
			}

			SelectedViewId = ElementId.InvalidElementId;
		}

		private void SetViewList(ElementId InActiveViewId)
		{
			if (FDocument.ActiveDocument == null)
			{
				return;
			}

			// Cache prev selected view
			All3DViews = new FilteredElementCollector(FDocument.ActiveDocument.RevitDoc).OfClass(typeof(View3D)).Cast<View3D>().ToList();
			All3DViews.RemoveAll(view => (view.IsTemplate || !view.CanBePrinted));

			// Combo box does not allow removal of items :). This is a workaround to always only add items and 
			// reuse them.
			int ViewIndex = 0;
			for (; ViewIndex < All3DViews.Count; ++ViewIndex)
			{
				View3D View = All3DViews[ViewIndex];

				var Items = ComboViews.GetItems();
				if (Items.Count == ViewIndex)
				{
					ComboViews.AddItem(new ComboBoxMemberData($"View_{ViewIndex}", "ViewName"));
				}

				Items = ComboViews.GetItems();
				ComboBoxMember Item = Items[ViewIndex];
				Item.ItemText = View.Name;
				Item.Visible = true;
			}

			// Hide the excessive items
			for (; ViewIndex < ComboViews.GetItems().Count; ++ViewIndex)
			{
				ComboViews.GetItems()[ViewIndex].Visible = false;
			}

			if (InActiveViewId != ElementId.InvalidElementId)
			{
				SelectedViewId = InActiveViewId;
			}

			// Set the active view
			View3D ComboActiveView = null;

			if (SelectedViewId != ElementId.InvalidElementId)
			{
				ComboActiveView = All3DViews.Find(View => View.Id == SelectedViewId);
			}

			if (ComboActiveView == null)
			{
				ComboActiveView = All3DViews.Count > 0 ? All3DViews.First() : null;
			}

			if (ComboActiveView != null)
			{
				foreach (ComboBoxMember Item in ComboViews.GetItems())
				{
					if (Item.ItemText == ComboActiveView.Name)
					{
						ComboViews.Current = Item;
						SelectedViewId = ComboActiveView.Id;
						FDocument.ActiveDocument?.SetActiveDirectLinkInstance(ComboActiveView);
						break;
					}
				}
			}
		}

		// Implement the interface to execute some tasks when Revit starts.
		public Result OnStartup(
			UIControlledApplication InApplication // handle to the application being started
		)
		{
			Instance = this;

			// Create a custom ribbon tab
			string TabName = DatasmithRevitResources.Strings.DatasmithTabName;
			InApplication.CreateRibbonTab(TabName);

			// Add a new ribbon panel
			RibbonPanel DirectLinkRibbonPanel = InApplication.CreateRibbonPanel(TabName, DatasmithRevitResources.Strings.RibbonSection_DirectLink);
			RibbonPanel FileExportRibbonPanel = InApplication.CreateRibbonPanel(TabName, DatasmithRevitResources.Strings.RibbonSection_FileExport);
			RibbonPanel DatasmithRibbonPanel = InApplication.CreateRibbonPanel(TabName, DatasmithRevitResources.Strings.RibbonSection_Datasmith);

			string AssemblyPath = Assembly.GetExecutingAssembly().Location;
			PushButtonData ExportButtonData = new PushButtonData("Export3DView", DatasmithRevitResources.Strings.ButtonExport3DView, AssemblyPath, "DatasmithRevitExporter.DatasmithExportRevitCommand");
			PushButtonData SyncButtonData = new PushButtonData("Sync3DView", DatasmithRevitResources.Strings.ButtonSync, AssemblyPath, "DatasmithRevitExporter.DatasmithSyncRevitCommand");
			PushButtonData AutoSyncButtonData = new PushButtonData("AutoSync3DView", DatasmithRevitResources.Strings.ButtonAutoSync, AssemblyPath, "DatasmithRevitExporter.DatasmithAutoSyncRevitCommand");
			PushButtonData ManageConnectionsButtonData = new PushButtonData("Connections", DatasmithRevitResources.Strings.ButtonConnections, AssemblyPath, "DatasmithRevitExporter.DatasmithManageConnectionsRevitCommand");
			PushButtonData SettingsButtonData = new PushButtonData("Settings", DatasmithRevitResources.Strings.ButtonSettings, AssemblyPath, "DatasmithRevitExporter.DatasmithShowSettingsRevitCommand");
			PushButtonData LogButtonData = new PushButtonData("Messages", DatasmithRevitResources.Strings.ButtonMessages, AssemblyPath, "DatasmithRevitExporter.DatasmithShowMessagesRevitCommand");

			SyncPushButton = DirectLinkRibbonPanel.AddItem(SyncButtonData) as PushButton;
			AutoSyncPushButton = DirectLinkRibbonPanel.AddItem(AutoSyncButtonData) as PushButton;

			PushButton ManageConnectionsButton = DirectLinkRibbonPanel.AddItem(ManageConnectionsButtonData) as PushButton;
			PushButton ExportPushButton = FileExportRibbonPanel.AddItem(ExportButtonData) as PushButton;

			// Create the view sync combo
			ComboBoxData CbData = new ComboBoxData("ComboBoxViews");
			PushButtonData LabelData = new PushButtonData("ComboLabel", "Select 3D view to sync", Assembly.GetExecutingAssembly().Location, "DatasmithRevitExporter.DatasmithSyncRevitCommand");

			IList<RibbonItem> StackedItems = DatasmithRibbonPanel.AddStackedItems(LabelData, CbData);
			if (StackedItems.Count > 1)
			{
				// We emulate a label on the ribbon with disabled push button (the Revit way to do things)
				PushButton LabelButton = StackedItems[0] as PushButton;
				if (LabelButton != null)
				{
					LabelButton.ToolTip = "Select 3D view to sync";
					LabelButton.Enabled = false;
				}

				ComboViews = StackedItems[1] as ComboBox;
				if (ComboViews != null)
				{
					ComboViews.CurrentChanged += OnComboBoxChanged;
					ComboViews.ItemText = "3D Views";
					ComboViews.ToolTip = "Select 3D View to Sync";
				}
			}

			PushButton ShowLogButton = DatasmithRibbonPanel.AddItem(LogButtonData) as PushButton;
			PushButton SettingsButton = DatasmithRibbonPanel.AddItem(SettingsButtonData) as PushButton;

			string DatasmithIconBase = Path.Combine(Path.GetDirectoryName(AssemblyPath), "DatasmithIcon");
			ExportPushButton.Image = new BitmapImage(new Uri(DatasmithIconBase + "16.png"));
			ExportPushButton.LargeImage = new BitmapImage(new Uri(DatasmithIconBase + "32.png"));
			ExportPushButton.ToolTip = DatasmithRevitResources.Strings.ButtonExport3DViewHint;

			DatasmithIconBase = Path.Combine(Path.GetDirectoryName(AssemblyPath), "DatasmithSyncIcon");
			SyncPushButton.Image = new BitmapImage(new Uri(DatasmithIconBase + "16.png"));
			SyncPushButton.LargeImage = new BitmapImage(new Uri(DatasmithIconBase + "32.png"));
			SyncPushButton.ToolTip = DatasmithRevitResources.Strings.ButtonSyncHint;

			DatasmithIconBase = Path.Combine(Path.GetDirectoryName(AssemblyPath), "DatasmithAutoSyncIcon");

			AutoSyncIconOn_Small = new BitmapImage(new Uri(DatasmithIconBase + "On16.png"));
			AutoSyncIconOn_Large = new BitmapImage(new Uri(DatasmithIconBase + "On32.png"));
			AutoSyncIconOff_Small = new BitmapImage(new Uri(DatasmithIconBase + "Off16.png"));
			AutoSyncIconOff_Large = new BitmapImage(new Uri(DatasmithIconBase + "Off32.png"));

			AutoSyncPushButton.Image = AutoSyncIconOn_Small;
			AutoSyncPushButton.LargeImage = AutoSyncIconOn_Large;
			AutoSyncPushButton.ToolTip = DatasmithRevitResources.Strings.ButtonAutoSyncHint;

			DatasmithIconBase = Path.Combine(Path.GetDirectoryName(AssemblyPath), "DatasmithManageConnectionsIcon");
			ManageConnectionsButton.Image = new BitmapImage(new Uri(DatasmithIconBase + "16.png"));
			ManageConnectionsButton.LargeImage = new BitmapImage(new Uri(DatasmithIconBase + "32.png"));
			ManageConnectionsButton.ToolTip = DatasmithRevitResources.Strings.ButtonConnectionsHint;

			DatasmithIconBase = Path.Combine(Path.GetDirectoryName(AssemblyPath), "DatasmithSettingsIcon");
			SettingsButton.Image = new BitmapImage(new Uri(DatasmithIconBase + "16.png"));
			SettingsButton.LargeImage = new BitmapImage(new Uri(DatasmithIconBase + "32.png"));
			SettingsButton.ToolTip = DatasmithRevitResources.Strings.ButtonSettingsHint;

			DatasmithIconBase = Path.Combine(Path.GetDirectoryName(AssemblyPath), "DatasmithLogIcon");
			ShowLogButton.Image = new BitmapImage(new Uri(DatasmithIconBase + "16.png"));
			ShowLogButton.LargeImage = new BitmapImage(new Uri(DatasmithIconBase + "32.png"));
			ShowLogButton.ToolTip = DatasmithRevitResources.Strings.ButtonMessagesHint;

			DocumentOpenedHandler = new EventHandler<DocumentOpenedEventArgs>(OnDocumentOpened);
			InApplication.ControlledApplication.DocumentOpened += DocumentOpenedHandler;

			DocumentCreatedHandler = new EventHandler<DocumentCreatedEventArgs>(OnDocumentCreated);
			InApplication.ControlledApplication.DocumentCreated += DocumentCreatedHandler;

			DocumentClosingHandler = new EventHandler<DocumentClosingEventArgs>(OnDocumentClosing);
			InApplication.ControlledApplication.DocumentClosing += DocumentClosingHandler;

			IdlingEventHandler = new EventHandler<IdlingEventArgs>(OnIdling);
			InApplication.Idling += IdlingEventHandler;

			ViewActivatedHandler = new EventHandler<ViewActivatedEventArgs>(OnViewActivated);
			InApplication.ViewActivated += ViewActivatedHandler;

			// Setup Direct Link

			string RevitEngineDir = null;

			try
			{
				using (RegistryKey Key = Registry.LocalMachine.OpenSubKey("Software\\Wow6432Node\\EpicGames\\Unreal Engine"))
				{
					RevitEngineDir = Key?.GetValue("RevitEngineDir") as string;
				}
			}
			finally
			{
				if (RevitEngineDir == null)
				{
					// If we could not read the registry, fallback to hardcoded engine dir
					RevitEngineDir = "C:\\ProgramData\\Epic\\Exporter\\RevitEngine\\";
				}
			}

			bool bDirectLinkInitOk = FDatasmithFacadeDirectLink.Init(true, RevitEngineDir);

			Debug.Assert(bDirectLinkInitOk);

			// Register updater to react to view modification

			ViewsUpdater = new View3DUpdater(InApplication.ControlledApplication.ActiveAddInId);
			UpdaterRegistry.RegisterUpdater(ViewsUpdater);

			ElementCategoryFilter Filter = new ElementCategoryFilter( BuiltInCategory.OST_Views);
			UpdaterRegistry.AddTrigger(ViewsUpdater.GetUpdaterId(), Filter, Element.GetChangeTypeAny());
			UpdaterRegistry.AddTrigger(ViewsUpdater.GetUpdaterId(), Filter, Element.GetChangeTypeElementAddition());
			UpdaterRegistry.AddTrigger(ViewsUpdater.GetUpdaterId(), Filter, Element.GetChangeTypeElementDeletion());

			return Result.Succeeded;
		}

		void OnIdling(object Sender, IdlingEventArgs Args)
		{
			FDirectLink.OnApplicationIdle();
		}

		static void OnDocumentOpened(object sender, DocumentOpenedEventArgs e)
		{
			FDocument.SetActiveDocument(e.Document);
			Instance.SetViewList(FDocument.ActiveDocument?.Settings.SyncViewId ?? ElementId.InvalidElementId);
		}

		static void OnDocumentCreated(object sender, DocumentCreatedEventArgs e)
		{
			FDocument.SetActiveDocument(e.Document);
			Instance.SetViewList(ElementId.InvalidElementId);
		}

		static void OnDocumentClosing(object sender, DocumentClosingEventArgs e)
		{
			Instance.ClearViews();
			FDocument.Destroy(e.Document);
		}

		static void OnViewActivated(object sender, ViewActivatedEventArgs e)
		{
			View Previous = e.PreviousActiveView;
			View Current = e.CurrentActiveView;

			if (Previous != null && !Previous.Document.Equals(Current.Document))
			{
				FDocument.SetActiveDocument(e.Document);
				Instance.SetViewList(FDocument.ActiveDocument?.Settings.SyncViewId ?? ElementId.InvalidElementId);
			}
		}

		// Implement the interface to execute some tasks when Revit shuts down.
		public Result OnShutdown(
			UIControlledApplication InApplication // handle to the application being shut down
		)
		{
			FDocument.DestroyAll();

			InApplication.ControlledApplication.DocumentClosing -= DocumentClosingHandler;
			InApplication.ControlledApplication.DocumentOpened -= DocumentOpenedHandler;
			InApplication.ControlledApplication.DocumentCreated -= DocumentCreatedHandler;
			InApplication.ViewActivated -= ViewActivatedHandler;

			DocumentClosingHandler = null;
			DocumentOpenedHandler = null;
			DocumentCreatedHandler = null;
			ViewActivatedHandler = null;

			UpdaterRegistry.UnregisterUpdater(ViewsUpdater.GetUpdaterId());
			ViewsUpdater = null;

			if (ExportMessagesDialog != null && !ExportMessagesDialog.IsDisposed)
			{
				ExportMessagesDialog.Close();
			}
			FDatasmithFacadeDirectLink.Shutdown();
			return Result.Succeeded;
		}

		public static void SetExportMessages(string InMessages)
		{
			ExportMessages = InMessages;

			if (ExportMessagesDialog != null)
			{
				ExportMessagesDialog.Messages = ExportMessages;
			}
		}

		public static void ShowExportMessages(ExternalCommandData InCommandData)
		{
			if (ExportMessagesDialog == null || ExportMessagesDialog.IsDisposed)
			{
				int CenterX = (InCommandData.Application.MainWindowExtents.Left + InCommandData.Application.MainWindowExtents.Right) / 2;
				int CenterY = (InCommandData.Application.MainWindowExtents.Top + InCommandData.Application.MainWindowExtents.Bottom) / 2;

				ExportMessagesDialog = new DatasmithRevitExportMessages(new System.Drawing.Point(CenterX, CenterY), () => ExportMessages = "");
				ExportMessagesDialog.Messages = ExportMessages;
				ExportMessagesDialog.Show();
			}
			else
			{
				ExportMessagesDialog.Focus();
			}
		}

		public static bool IsPreHandshakeRevitBuild(string VersionBuild)
		{
#if REVIT_API_2023
			return Version.Parse(VersionBuild) < Version.Parse("23.1");
#else
			return true;
#endif
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void CreateDebugLog()
		{
			DebugLog = new FDebugLog();
		}

		[Conditional("DatasmithRevitDebugOutput")]
		public void LogDebug(string Message)  
		{
			DebugLog.LogDebug(Message);
		}
	}

	class FDebugLog
	{
		private ConcurrentQueue<string> MessagesQueue = new ConcurrentQueue<string>();
		private Thread LogWriterThread;

		public FDebugLog()
		{
			LogWriterThread = new Thread(() =>
			{
				LogWriterProc();
			});

			LogWriterThread.Start();
		}

		private void LogWriterProc()
		{
			string LogPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
				"UnrealDatasmithExporter/Saved/Logs/UnrealDatasmithRevitExporterDebug.log");

			StreamWriter LogFile = new StreamWriter(LogPath);

			while (true)
			{
				while (MessagesQueue.TryDequeue(out string Message))
				{
					LogFile.WriteLine(Message);
				}
				LogFile.Flush();

				Thread.Sleep(10);
			}
		}

		public void LogDebug(string Message)  
		{
			MessagesQueue.Enqueue(Message);
		}
	};

}
