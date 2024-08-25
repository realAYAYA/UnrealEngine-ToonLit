// Copyright Epic Games, Inc. All Rights Reserved.
using AutomationTool;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using UnrealBuildTool;

using static AutomationTool.CommandUtils;

namespace EpicGames.Localization
{
	public abstract class LocalizationTargetCommand
	{
		protected static BuildCommand _commandLineHelper;
		private static Dictionary<string, LocalizationTargetCommand> _nameToLocalizationTargetCommandMap = new Dictionary<string, LocalizationTargetCommand>();

		public string Name { get; protected set; } = "";
		public string DisplayName { get; protected set; } = "";
		public bool bIsExecutingInPreview { get; private set; } = false;

		static LocalizationTargetCommand()
		{
			// look for all subclasses, and cache by their ProviderToken
			foreach (Assembly assembly in ScriptManager.AllScriptAssemblies)
			{
				Type[] allTypesInAssembly = assembly.GetTypes();
				foreach (Type typeInAssembly in allTypesInAssembly)
				{
					// we also guard against abstract classes as we can have abstract classes as children of LocalizationTargetEditorCommand e.g PluginLocalizationTargetCommand
					if (typeof(LocalizationTargetCommand).IsAssignableFrom(typeInAssembly) && typeInAssembly != typeof(LocalizationTargetCommand) && !typeInAssembly.IsAbstract)
					{
						LocalizationTargetCommand provider = (LocalizationTargetCommand)Activator.CreateInstance(typeInAssembly);
						_nameToLocalizationTargetCommandMap[provider.Name] = provider;
					}
				}
			}
		}

		public static void Initialize(BuildCommand commandLineHelper)
		{
			_commandLineHelper = commandLineHelper;
		}

		public static LocalizationTargetCommand GetLocalizationTargetCommandFromName(string commandName)
		{
			LocalizationTargetCommand command = null;
			_nameToLocalizationTargetCommandMap.TryGetValue(commandName, out command);
			return command;
		}


		protected virtual bool ParseCommandLine()
		{
			bIsExecutingInPreview = _commandLineHelper.ParseParam("Preview");
			return true;
		}

		public abstract bool Execute();

		public void PrintHelpText()
		{
			Logger.LogInformation("Help:");
			Logger.LogInformation($"{GetHelpText()}");
		}

		public virtual string GetHelpText()
		{
			return "";
		}

		public static List<LocalizationTargetCommand> GetAllCommands()
		{
			return _nameToLocalizationTargetCommandMap.Values.ToList();
		}
	}
}
