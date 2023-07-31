// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.Properties.Localization;

using Rhino;
using Rhino.Commands;
using System;

namespace DatasmithRhino.Commands
{
	/**
	 * Command used to export the scene to a .udatasmith file.
	 */
	public class DatasmithExportCommand : Command
	{
		public DatasmithExportCommand()
		{
			// Rhino only creates one instance of each command class defined in a
			// plug-in, so it is safe to store a reference in a static property.
			Instance = this;
		}

		/**
		 * The only instance of this command.
		 */
		public static DatasmithExportCommand Instance {
			get; private set;
		}

		/**
		 * The command name as it appears on the Rhino command line.
		 */
		public override string EnglishName {
			get { return "DatasmithExport"; }
		}

		public override string LocalName {
			get { return Resources.DatasmithExportCommand; }
		}

		protected override Result RunCommand(RhinoDoc RhinoDocument, RunMode Mode)
		{
			Result CommandResult = Result.Failure;

			Eto.Forms.SaveFileDialog SaveDialog = new Eto.Forms.SaveFileDialog();
			Eto.Forms.FileFilter DatasmithFileFilter = new Eto.Forms.FileFilter("Unreal Datasmith", new string[] { ".udatasmith" });
			SaveDialog.Filters.Add(DatasmithFileFilter);
			SaveDialog.CurrentFilter = DatasmithFileFilter;
			SaveDialog.Title = Resources.SaveDialogTitle;
			SaveDialog.FileName = string.IsNullOrEmpty(RhinoDocument.Name) ? Resources.UntitledFileName : System.IO.Path.GetFileNameWithoutExtension(RhinoDocument.Name);
			if (!string.IsNullOrEmpty(RhinoDocument.Path))
			{
				Uri PathUri = new Uri(System.IO.Path.GetDirectoryName(RhinoDocument.Path));
				SaveDialog.Directory = PathUri;
			}

			Eto.Forms.DialogResult SaveDialogResult = SaveDialog.ShowDialog(Rhino.UI.RhinoEtoApp.MainWindow);
			if (SaveDialogResult == Eto.Forms.DialogResult.Ok)
			{
				string FileName = SaveDialog.FileName;
				const bool bSkipHidden = true;
				FDatasmithFacadeScene DatasmithScene = DatasmithRhinoSceneExporter.CreateDatasmithScene(FileName, RhinoDocument);
				DatasmithRhinoExportOptions ExportOptions = new DatasmithRhinoExportOptions(RhinoDocument, DatasmithScene, bSkipHidden);
				Rhino.PlugIns.WriteFileResult ExportResult = DatasmithRhinoSceneExporter.ExportToFile(ExportOptions);

				switch (ExportResult)
				{
					case Rhino.PlugIns.WriteFileResult.Success:
						CommandResult = Result.Success;
						break;
					case Rhino.PlugIns.WriteFileResult.Cancel:
						CommandResult = Result.Cancel;
						break;
					case Rhino.PlugIns.WriteFileResult.Failure:
					default:
						break;
				}
			}
			else if (SaveDialogResult == Eto.Forms.DialogResult.Cancel)
			{
				CommandResult = Result.Cancel;
			}

			return CommandResult;
		}
	}
}