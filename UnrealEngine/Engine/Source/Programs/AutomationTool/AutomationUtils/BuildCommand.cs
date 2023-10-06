// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace AutomationTool
{
	/// <summary>
	/// Base class for buildcommands.
	/// </summary>
	public abstract class BuildCommand : CommandUtils
	{
		/// <summary>
		/// Command line parameters for this command (empty by non-null by default)
		/// </summary>
		private string[] CommandLineParams = new string[0];
		public string[] Params
		{
			get { return CommandLineParams; }
			set { CommandLineParams = value; }
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and returns whether it is defined or not.
		/// </summary>
		/// <param name="Param">Param to check for.</param>
		/// <returns>True if param was found, false otherwise.</returns>
		public bool ParseParam(string Param)
		{
			return ParseParam(Params, Param);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public string ParseParamValue(string Param, string Default = null, string ObsoleteParam = null)
		{
			string ParamValue = ParseParamValue(Params, Param, null);

			if (ObsoleteParam != null)
			{
				string ObsoleteParamValue = ParseParamValue(Params, ObsoleteParam, null);

				if (ObsoleteParamValue != null)
				{
					if (ParamValue == null)
					{
						Logger.LogWarning("Param name \"{ObsoleteParam}\" is deprecated, use \"{Param}\" instead.", ObsoleteParam, Param);
					}
					else
					{
						Logger.LogWarning("Deprecated param name \"{ObsoleteParam}\" was ignored because \"{Param}\" was set.", ObsoleteParam, Param);
					}
				}

			}

			return ParamValue ?? Default;
		}

		/// <summary>
		/// Parses an argument.
		/// </summary>
		/// <param name="Param"></param>
		/// <returns></returns>
		public string ParseOptionalStringParam(string Param)
		{
			return ParseParamValue(Param, null);
		}

		/// <summary>
		/// Parses an argument. Throws an exception if the parameter is not specified.
		/// </summary>
		/// <param name="Param">Name of the argument</param>
		/// <returns>Value of the argument</returns>
		public string ParseRequiredStringParam(string Param)
		{
			string Value = ParseOptionalStringParam(Param);
			if(Value == null)
			{
				throw new AutomationException("Missing -{0}=... parameter", Param);
			}
			return Value;
		}

		/// <summary>
		/// Parses an file reference argument.
		/// </summary>
		/// <param name="Param">Name of the argument</param>
		/// <returns>Value of the argument</returns>
		public FileReference ParseOptionalFileReferenceParam(string Param)
		{
			string StringValue = ParseParamValue(Param);
			if(StringValue == null)
			{
				return null;
			}
			else
			{
				return new FileReference(StringValue);
			}
		}

		/// <summary>
		/// Parses an file reference argument. Throws an exception if the parameter is not specified.
		/// </summary>
		/// <param name="Param">Name of the argument</param>
		/// <returns>Value of the argument</returns>
		public FileReference ParseRequiredFileReferenceParam(string Param)
		{
			FileReference Value = ParseOptionalFileReferenceParam(Param);
			if(Value == null)
			{
				throw new AutomationException("Missing -{0}=... parameter", Param);
			}
			return Value;
		}

		/// <summary>
		/// Parses a directory reference argument.
		/// </summary>
		/// <param name="Param">Name of the argument</param>
		/// <returns>Value of the argument</returns>
		public DirectoryReference ParseOptionalDirectoryReferenceParam(string Param)
		{
			string StringValue = ParseOptionalStringParam(Param);
			if(StringValue == null)
			{
				return null;
			}
			else
			{
				return new DirectoryReference(StringValue);
			}
		}

		/// <summary>
		/// Parses a directory reference argument. Throws an exception if the parameter is not specified.
		/// </summary>
		/// <param name="Param">Name of the argument</param>
		/// <returns>Value of the argument</returns>
		public DirectoryReference ParseRequiredDirectoryReferenceParam(string Param)
		{
			DirectoryReference Value = ParseOptionalDirectoryReferenceParam(Param);
			if(Value == null)
			{
				throw new AutomationException("Missing -{0}=... parameter", Param);
			}
			return Value;
		}

		/// <summary>
		/// Parses an argument as an enum.
		/// </summary>
		/// <param name="Param">Name of the parameter to read.</param>
		/// <returns>Returns the value that was parsed.</returns>
		public Nullable<T> ParseOptionalEnumParam<T>(string Param) where T : struct
		{
			string ValueString = ParseParamValue(Param);
			if(ValueString == null)
			{
				return null;
			}
			else
			{
				T Value;
				if(!Enum.TryParse<T>(ValueString, out Value))
				{
					throw new AutomationException("'{0}' is not a valid value for {1}", ValueString, typeof(T).Name);
				}
				return Value;
			}
		}

		/// <summary>
		/// Parses an argument as an enum. Throws an exception if the parameter is not specified.
		/// </summary>
		/// <param name="Param">Name of the parameter to read.</param>
		/// <returns>Returns the value that was parsed.</returns>
		public T ParseRequiredEnumParamEnum<T>(string Param) where T : struct
		{
			Nullable<T> Value = ParseOptionalEnumParam<T>(Param);
			if(!Value.HasValue)
			{
				throw new AutomationException("Missing -{0}=... parameter", Param);
			}
			return Value.Value;
		}

		/// <summary>
		/// Parses the argument list for any number of parameters.
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns an array of values for this parameter (or an empty array if one was not found.</returns>
		public string[] ParseParamValues(string Param)
		{
			return ParseParamValues(Params, Param);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public bool ParseParamBool(string Param, bool Default = false)
		{
			string boolValue = ParseParamValue(Params, Param, Default.ToString());
			return bool.Parse(boolValue);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public int ParseParamInt(string Param, int Default = 0)
		{
			string num = ParseParamValue(Params, Param, Default.ToString());
			return int.Parse(num);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public int? ParseParamNullableInt(string Param)
		{
			string Value = ParseParamValue(Params, Param, null);
			if(Value == null)
			{
				return null;
			}
			else
			{
				return int.Parse(Value);
			}
		}

		public FileReference ParseProjectParam()
		{
			FileReference ProjectFullPath = null;

			var bForeign = ParseParam("foreign");
			var bForeignCode = ParseParam("foreigncode");
			if (bForeign)
			{
				var DestSample = ParseParamValue("DestSample", "CopiedHoverShip");
				var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue\foreign\", DestSample + "_ _Dir"));
				ProjectFullPath = new FileReference(CombinePaths(Dest, DestSample + ".uproject"));
			}
			else if (bForeignCode)
			{
				var DestSample = ParseParamValue("DestSample", "PlatformerGame");
				var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue\foreign\", DestSample + "_ _Dir"));
				ProjectFullPath = new FileReference(CombinePaths(Dest, DestSample + ".uproject"));
			}
			else
			{
				var OriginalProjectName = ParseParamValue("project", "");

				if (string.IsNullOrEmpty(OriginalProjectName))
				{
					return null;
				}

				var ProjectName = OriginalProjectName;
				ProjectName = ProjectName.Trim(new char[] { '\"' });
				if (ProjectName.IndexOfAny(new char[] { '\\', '/' }) < 0)
				{
					ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName, ProjectName + ".uproject");
				}
				else if (!FileExists_NoExceptions(ProjectName))
				{
					ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName);
				}
				if (FileExists_NoExceptions(ProjectName))
				{
					ProjectFullPath = new FileReference(ProjectName);
				}
				else
				{
					var Branch = new BranchInfo();
					var GameProj = Branch.FindGame(OriginalProjectName);
					if (GameProj != null)
					{
						ProjectFullPath = GameProj.FilePath;
					}
					if (ProjectFullPath == null || !FileExists_NoExceptions(ProjectFullPath.FullName))
					{
						throw new AutomationException("Could not find a project file {0}.", ProjectName);
					}
				}
			}

			return ProjectFullPath;
		}

		/// <summary>
		/// Checks that all of the required params are present, throws an exception if not
		/// </summary>
		/// <param name="Args"></param>
		public void CheckParamsArePresent(params string[] Args)
		{
			List<string> MissingParams = new List<string>();
			foreach (string Arg in Args)
			{
				if (ParseParamValue(Arg, null) == null)
				{
					MissingParams.Add(Arg);
				}
			}

			if (MissingParams.Count > 0)
			{
				throw new AutomationException("Params {0} are missing but required. Required params are {1}", string.Join(",", MissingParams), string.Join(",", Args));
			}
		}

		/// <summary>
		/// Build command entry point.  Throws AutomationExceptions on failure.
		/// </summary>
		public virtual void ExecuteBuild()
		{
			throw new AutomationException("Either Execute() or ExecuteBuild() should be implemented for {0}", GetType().Name);
		}

		/// <summary>
		/// Command entry point.
		/// </summary>
		public virtual ExitCode Execute()
		{
			ExecuteBuild();
			return ExitCode.Success;
		}

		/// <summary>
		/// Async command entry point.
		/// </summary>
		public virtual Task<ExitCode> ExecuteAsync()
		{
			return Task.FromResult(Execute());
		}

		/// <summary>
		/// Executes a new command as a child of another command.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="ParentCommand"></param>
		public static ExitCode Execute<T>(BuildCommand ParentCommand) where T : BuildCommand, new()
		{
			T Command = new T();
			if (ParentCommand != null)
			{
				Command.Params = ParentCommand.Params;
			}
			return Command.Execute();
		}
	}
}
