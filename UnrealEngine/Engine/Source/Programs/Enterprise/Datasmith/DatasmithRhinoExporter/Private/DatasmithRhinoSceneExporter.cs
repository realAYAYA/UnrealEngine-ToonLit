// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.ElementExporters;
using DatasmithRhino.ExportContext;
using DatasmithRhino.Properties.Localization;
using DatasmithRhino.Utils;

using Rhino;
using System;

namespace DatasmithRhino
{
	public static class DatasmithRhinoSceneExporter
	{
		public static FDatasmithFacadeScene CreateDatasmithScene(string Filename, RhinoDoc RhinoDocument)
		{
			string RhinoAppName = RhinoApp.Name;
			string RhinoVersion = RhinoApp.ExeVersion.ToString();
			FDatasmithFacadeElement.SetCoordinateSystemType(FDatasmithFacadeElement.ECoordinateSystemType.RightHandedZup);
			FDatasmithFacadeElement.SetWorldUnitScale((float)RhinoMath.UnitScale(RhinoDocument.ModelUnitSystem, UnitSystem.Centimeters));
			FDatasmithFacadeScene DatasmithScene = new FDatasmithFacadeScene("Rhino", "Robert McNeel & Associates", "Rhino3D", RhinoVersion);
			DatasmithScene.SetOutputPath(System.IO.Path.GetDirectoryName(Filename));
			DatasmithScene.SetName(System.IO.Path.GetFileNameWithoutExtension(Filename));

			return DatasmithScene;
		}

		public static Rhino.PlugIns.WriteFileResult ExportToFile(DatasmithRhinoExportOptions Options)
		{
			Func<FDatasmithFacadeScene, bool> OnSceneExportCompleted = (FDatasmithFacadeScene Scene) => { return Scene.ExportScene(); };
			DatasmithRhinoExportContext ExportContext = new DatasmithRhinoExportContext(Options);

			Rhino.Commands.Result ExportResult = ExportScene(Options.DatasmithScene, ExportContext, OnSceneExportCompleted);

			//Return with the corresponding WriteFileResult;
			switch (ExportResult)
			{
				case Rhino.Commands.Result.Success:
					return Rhino.PlugIns.WriteFileResult.Success;
				case Rhino.Commands.Result.Cancel:
				case Rhino.Commands.Result.CancelModelessDialog:
					return Rhino.PlugIns.WriteFileResult.Cancel;
				case Rhino.Commands.Result.Failure:
				default:
					return Rhino.PlugIns.WriteFileResult.Failure;
			}
		}

		public static Rhino.Commands.Result ExportScene(FDatasmithFacadeScene DatasmithScene, DatasmithRhinoExportContext ExportContext, Func<FDatasmithFacadeScene, bool> OnSceneExportCompleted)
		{
			bool bExportSuccess = false;
			try
			{
				RhinoApp.WriteLine(string.Format(Resources.DatasmithExportMessage, System.IO.Path.GetFileName(DatasmithScene.GetName())));
				RhinoApp.WriteLine(Resources.PressEscToCancel);

				DatasmithScene.PreExport();

				DatasmithRhinoProgressManager.Instance.StartMainTaskProgress(Resources.ParsingDocument, 0.1f);
				ExportContext.ParseDocument();

				if (SynchronizeScene(ExportContext, DatasmithScene) == Rhino.Commands.Result.Success)
				{
					DatasmithRhinoProgressManager.Instance.StartMainTaskProgress(Resources.ExportingScene, 1);
					bExportSuccess = OnSceneExportCompleted(DatasmithScene);
				}

				ExportContext.OnPostExport();
			}
			catch (DatasmithExportCancelledException)
			{
				return Rhino.Commands.Result.Cancel;
			}
			catch (Exception e)
			{
				bExportSuccess = false;
				RhinoApp.WriteLine(Resources.UnexpectedError);
				RhinoApp.WriteLine(e.ToString());
			}
			finally
			{
				DatasmithRhinoProgressManager.Instance.StopProgress();
			}

			return bExportSuccess
				? Rhino.Commands.Result.Success
				: Rhino.Commands.Result.Failure;
		}

		private static Rhino.Commands.Result SynchronizeScene(DatasmithRhinoExportContext ExportContext, FDatasmithFacadeScene DatasmithScene)
		{
			DatasmithRhinoProgressManager.Instance.StartMainTaskProgress(Resources.ExportingTextures, 0.1f);
			DatasmithRhinoTextureExporter.Instance.SynchronizeElements(DatasmithScene, ExportContext);

			DatasmithRhinoProgressManager.Instance.StartMainTaskProgress(Resources.ExportingMaterials, 0.2f);
			DatasmithRhinoMaterialExporter.Instance.SynchronizeElements(DatasmithScene, ExportContext);

			DatasmithRhinoProgressManager.Instance.StartMainTaskProgress(Resources.ExportingMeshes, 0.7f);
			DatasmithRhinoMeshExporter.Instance.SynchronizeElements(DatasmithScene, ExportContext);

			DatasmithRhinoProgressManager.Instance.StartMainTaskProgress(Resources.ExportingActors, 0.9f);
			DatasmithRhinoActorExporter.Instance.SynchronizeElements(DatasmithScene, ExportContext);

			return Rhino.Commands.Result.Success;
		}
	}
}