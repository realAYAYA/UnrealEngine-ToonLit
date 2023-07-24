// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;

using DatasmithRevitExporter;
using Autodesk.Revit.DB;
using System.Text.RegularExpressions;

namespace DSNode
{
    public class DatasmithDynamoNode
    {
		public static void Export3DViewsToDatasmith(Document InDocument, string InOutputPath, List<int> InViewIds, int InTesselation = 8)
		{
			if (InDocument == null)
			{
				Console.WriteLine("Invalid document.");
				return;
			}

			Autodesk.Revit.ApplicationServices.Application App = InDocument.Application;

			if (string.Compare(App.VersionNumber, "2018", StringComparison.Ordinal) == 0 && string.Compare(App.SubVersionNumber, "2018.3", StringComparison.Ordinal) < 0)
			{
				string Message = string.Format("The running Revit is not supported.\nYou must use Revit 2018.3 or further updates to export.");
				Console.WriteLine(Message);
				return;
			}

			try
			{
				// Validate the provided path
				InOutputPath = Path.GetFullPath(InOutputPath);
			}
			catch (Exception e)
			{
				Console.WriteLine(e.Message);
				return;
			}

			if (!CustomExporter.IsRenderingSupported())
			{
				string Message = "3D view rendering is not supported in the running Revit.";
				Console.WriteLine(Message);
				return;
			}

			string DocumentPath = InDocument.PathName;

			Dictionary<ElementId, string> FilePaths = new Dictionary<ElementId, string>();
			List<View3D> ExportViews = new List<View3D>();
			foreach (var Id in InViewIds)
			{
				View3D View = InDocument.GetElement(new ElementId(Id)) as View3D;
				if (View != null && !View.IsTemplate && View.CanBePrinted)
				{
					ExportViews.Add(View);
					// Generate file path for each view.
					string FileName = Regex.Replace($"{View.Name}.udatasmith", @"\s+", "_");
					FilePaths.Add(View.Id, Path.Combine(InOutputPath, FileName));
				}
			}

			// Retrieve the Unreal Datasmith export options.
			DatasmithRevitExportOptions ExportOptions = new DatasmithRevitExportOptions(InDocument);

			// Create a custom export context for command Export to Unreal Datasmith.
			FDatasmithRevitExportContext ExportContext = new FDatasmithRevitExportContext(
				App,
				InDocument,
				FDocument.ActiveDocument.Settings,
				FilePaths,
				ExportOptions,
				null);

			// Clamp tesselation parameter to a valid range.
			ExportContext.LevelOfTessellation = Math.Min(Math.Max(InTesselation, 1), 15);

			using (CustomExporter Exporter = new CustomExporter(InDocument, ExportContext))
			{
				try
				{
					// The export process will exclude output of geometric objects such as faces and curves,
					// but the context needs to receive the calls related to Faces or Curves to gather data.
					// The context always receive their tessellated geometry in form of polymeshes or lines.
					Exporter.IncludeGeometricObjects = true;

					// The export process should stop in case an error occurs during any of the exporting methods.
					Exporter.ShouldStopOnError = true;

					// Initiate the export process for all 3D views.
					foreach (var View in ExportViews)
					{
#if REVIT_API_2020
						Exporter.Export(View as Autodesk.Revit.DB.View);
#else
						Exporter.Export(View);
#endif
					}
				}
				catch (System.Exception InException)
				{
					string Message = string.Format("Cannot export the 3D view:\n\n{0}\n\n{1}", InException.Message, InException.StackTrace);
					Console.WriteLine(Message);
					return;
				}
			}
		}
	}
}
