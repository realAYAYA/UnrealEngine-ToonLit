// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;

namespace EpicGames.Localization
{
	public class LocalizationConfigFileGeneratorParams
	{
		public string LocalizationTargetName { get; set; } = "";
		public string LocalizationTargetRootDirectory { get; set; } = "";
	}

	public abstract class LocalizationConfigFileGenerator
	{
		public static LocalizationConfigFileGenerator GetGeneratorForFileFormat(LocalizationConfigFileFormat fileFormat)
		{
			switch (fileFormat)
			{
				case LocalizationConfigFileFormat.Modular:
					return new ModularLocalizationConfigFileGenerator();
				default:
					throw new NotImplementedException("File format is not yet supported.");
			}
		}

		public abstract List<LocalizationConfigFile> GenerateDefaultSettingsConfigFiles(LocalizationConfigFileGeneratorParams generatorParams);
		
	}

	public class ModularLocalizationConfigFileGenerator : LocalizationConfigFileGenerator
	{
		public override List<LocalizationConfigFile> GenerateDefaultSettingsConfigFiles(LocalizationConfigFileGeneratorParams generatorParams)
		{
			List<LocalizationConfigFile> configFiles = new List<LocalizationConfigFile>();
			configFiles.Add(GenerateLocalizationGatherConfigFile(generatorParams));
			configFiles.Add(GenerateLocalizationExportConfigFile(generatorParams));
			configFiles.Add(GenerateLocalizationImportConfigFile(generatorParams));
			configFiles.Add(GenerateLocalizationCompileConfigFile(generatorParams));
			configFiles.Add(GenerateLocalizationGenerateReportsConfigFile(generatorParams));
			return configFiles;
		}

		private LocalizationConfigFile GenerateLocalizationGatherConfigFile(LocalizationConfigFileGeneratorParams generatorParam)
		{
			CommonSettingsParams commonSettings = CommonSettingsParams.CreateDefault(generatorParam.LocalizationTargetName, generatorParam.LocalizationTargetRootDirectory);
			LocalizationConfigFileBuilder builder = new LocalizationConfigFileBuilder(generatorParam.LocalizationTargetName + ModularLocalizationConfigFileSuffixes.GatherSuffix + LocalizationFileExtensions.ModularLocalizationConfigFileExtension);
			builder.AddCommonSettingsSection(commonSettings);

			GatherTextFromSourceParams gatherTextFromSourceParams = GatherTextFromSourceParams.CreateDefault(generatorParam.LocalizationTargetName, generatorParam.LocalizationTargetRootDirectory); 
			builder.AddGatherTextFromSourceSection(gatherTextFromSourceParams);

			GatherTextFromAssetsParams gatherTextFromAssetsParams = GatherTextFromAssetsParams.CreateDefault(generatorParam.LocalizationTargetName, generatorParam.LocalizationTargetRootDirectory);
			builder.AddGatherTextFromAssetsSection(gatherTextFromAssetsParams);

			GenerateGatherManifestParams generateGatherManifestParams = GenerateGatherManifestParams.CreateDefault(generatorParam.LocalizationTargetName, generatorParam.LocalizationTargetRootDirectory);
			builder.AddGenerateGatherManifestSection(generateGatherManifestParams);

			GenerateGatherArchiveParams generateGatherArchiveParams = GenerateGatherArchiveParams.CreateDefault(generatorParam.LocalizationTargetName, generatorParam.LocalizationTargetRootDirectory);
			builder.AddGenerateGatherArchiveSection(	generateGatherArchiveParams);

			GenerateTextLocalizationReportParams generateTextLocalizationReportParams = GenerateTextLocalizationReportParams.CreateDefault(generatorParam.LocalizationTargetName, generatorParam.LocalizationTargetRootDirectory);
			builder.AddGenerateTextLocalizationReportSection(generateTextLocalizationReportParams);

			return builder.Get();
		}

		private LocalizationConfigFile GenerateLocalizationExportConfigFile(LocalizationConfigFileGeneratorParams generatorParams)
		{
			CommonSettingsParams commonSettings = CommonSettingsParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			LocalizationConfigFileBuilder builder = new LocalizationConfigFileBuilder(generatorParams.LocalizationTargetName + ModularLocalizationConfigFileSuffixes.ExportSuffix + LocalizationFileExtensions.ModularLocalizationConfigFileExtension);
			builder.AddCommonSettingsSection(commonSettings);

			ExportPortableObjectFilesParams exportParams = ExportPortableObjectFilesParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			builder.AddExportPortableObjectFilesSection(exportParams);
			return builder.Get();
		}

		private LocalizationConfigFile GenerateLocalizationImportConfigFile(LocalizationConfigFileGeneratorParams generatorParams)
		{
			CommonSettingsParams commonSettings = CommonSettingsParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			LocalizationConfigFileBuilder builder = new LocalizationConfigFileBuilder(generatorParams.LocalizationTargetName + ModularLocalizationConfigFileSuffixes.ImportSuffix + LocalizationFileExtensions.ModularLocalizationConfigFileExtension);
			builder.AddCommonSettingsSection(commonSettings);

			ImportPortableObjectFilesParams importParams = ImportPortableObjectFilesParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			builder.AddImportPortableObjectFilesSection(importParams);
			return builder.Get();
		}

		private LocalizationConfigFile GenerateLocalizationCompileConfigFile(LocalizationConfigFileGeneratorParams generatorParams)
		{
			CommonSettingsParams commonSettings = CommonSettingsParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			LocalizationConfigFileBuilder builder = new LocalizationConfigFileBuilder(generatorParams.LocalizationTargetName + ModularLocalizationConfigFileSuffixes.CompileSuffix + LocalizationFileExtensions.ModularLocalizationConfigFileExtension);
			builder.AddCommonSettingsSection(commonSettings);

			GenerateTextLocalizationResourceParams generateTextLocalizationResourceParams = GenerateTextLocalizationResourceParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			builder.AddGenerateTextLocalizationResourceSection(generateTextLocalizationResourceParams);
			return builder.Get();
		}

		private LocalizationConfigFile GenerateLocalizationGenerateReportsConfigFile(LocalizationConfigFileGeneratorParams generatorParams)
		{
			CommonSettingsParams commonSettings = CommonSettingsParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			LocalizationConfigFileBuilder builder = new LocalizationConfigFileBuilder(generatorParams.LocalizationTargetName + ModularLocalizationConfigFileSuffixes.GenerateReportsSuffix + LocalizationFileExtensions.ModularLocalizationConfigFileExtension);
			builder.AddCommonSettingsSection(commonSettings);

			GenerateTextLocalizationReportParams reportParams = GenerateTextLocalizationReportParams.CreateDefault(generatorParams.LocalizationTargetName, generatorParams.LocalizationTargetRootDirectory);
			// We don't need the conflict report 
			reportParams.bConflictReport = false;
			reportParams.ConflictReportName = "";
			builder.AddGenerateTextLocalizationReportSection(reportParams);
			return builder.Get();
		}
	}
}

