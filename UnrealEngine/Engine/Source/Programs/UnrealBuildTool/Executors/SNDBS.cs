// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.ServiceProcess;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{
	sealed class SNDBS : ActionExecutor
	{
		public override string Name => "SNDBS";

		private static readonly string? ProgramFilesx86 = Environment.GetEnvironmentVariable("ProgramFiles(x86)");
		private static readonly string? SCERoot = Environment.GetEnvironmentVariable("SCE_ROOT_DIR");

		private static string FindDbsExe(string ExeName)
		{
			string InstallPath = Path.Combine(ProgramFilesx86 ?? String.Empty, "SCE", "Common", "SN-DBS", "bin", ExeName);
			if (File.Exists(InstallPath))
			{
				return InstallPath;
			}
			else
			{
				// Legacy install location using SCE_ROOT_DIR
				return Path.Combine(SCERoot ?? String.Empty, "Common", "SN-DBS", "bin", ExeName);
			}
		}

		private static string SNDBSBuildExe => FindDbsExe("dbsbuild.exe");
		private static string SNDBSUtilExe => FindDbsExe("dbsutil.exe");

		private static readonly DirectoryReference IntermediateDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", "SNDBS");
		private static readonly FileReference IncludeRewriteRulesFile = FileReference.Combine(IntermediateDir, "include-rewrite-rules.ini");
		private static readonly FileReference ScriptFile = FileReference.Combine(IntermediateDir, "sndbs.json");

		private Dictionary<string, string> ActiveTemplates = BuiltInTemplates.ToDictionary(p => p.Key, p => p.Value);

		/// <summary>
		/// When enabled, SN-DBS will stop compiling targets after a compile error occurs.  Recommended, as it saves computing resources for others.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bStopSNDBSCompilationAfterErrors = false;

		/// <summary>
		/// When set to false, SNDBS will not be enabled when running connected to the coordinator over VPN. Configure VPN-assigned subnets via the VpnSubnets parameter.
		/// </summary>
		[XmlConfigFile(Category = "SNDBS")]
		static bool bAllowOverVpn = true;

		/// <summary>
		/// List of subnets containing IP addresses assigned by VPN
		/// </summary>
		[XmlConfigFile(Category = "SNDBS")]
		static string[]? VpnSubnets = null;

		private const string ProgressMarkupPrefix = "@action:";

		private List<TargetDescriptor> TargetDescriptors;

		public SNDBS(List<TargetDescriptor> InTargetDescriptors, ILogger Logger)
			: base(Logger)
		{
			XmlConfig.ApplyTo(this);
			TargetDescriptors = InTargetDescriptors;
		}

		public SNDBS AddTemplate(string ExeName, string TemplateContents)
		{
			ActiveTemplates.Add(ExeName, TemplateContents);
			return this;
		}

		static bool TryGetBrokerHost([NotNullWhen(true)] out string? OutBroker)
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				string BrokerHostName = "";
				Regex FindHost = new Regex(@"Active broker is ""(\S +)"" \((\S+)\)");
				Process LocalProcess = new Process();
				LocalProcess.StartInfo = new ProcessStartInfo(SNDBSUtilExe, $"-connected");
				LocalProcess.OutputDataReceived += (Sender, Args) =>
				{
					if (Args.Data != null)
					{
						Match Result = FindHost.Match(Args.Data);
						if (Result.Success)
						{
							BrokerHostName = Result.Groups[1].Value;
						}
					}
				};
				if (Utils.RunLocalProcess(LocalProcess) == 1 && BrokerHostName.Length > 0)
				{
					OutBroker = BrokerHostName;
					return true;
				}
			}

			OutBroker = null;
			return false;
		}

		[DllImport("iphlpapi")]
		static extern int GetBestInterface(uint dwDestAddr, ref int pdwBestIfIndex);

		static NetworkInterface? GetInterfaceForHost(string Host)
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				IPHostEntry HostEntry = Dns.GetHostEntry(Host);
				foreach (IPAddress HostAddress in HostEntry.AddressList)
				{
					int InterfaceIdx = 0;
					if (GetBestInterface(BitConverter.ToUInt32(HostAddress.GetAddressBytes(), 0), ref InterfaceIdx) == 0)
					{
						foreach (NetworkInterface Interface in NetworkInterface.GetAllNetworkInterfaces())
						{
							IPv4InterfaceProperties Properties = Interface.GetIPProperties().GetIPv4Properties();
							if (Properties.Index == InterfaceIdx)
							{
								return Interface;
							}
						}
					}
				}
			}
			return null;
		}

		public static bool IsHostOnVpn(string HostName, ILogger Logger)
		{
			// If there aren't any defined subnets, just early out
			if (VpnSubnets == null || VpnSubnets.Length == 0)
			{
				return false;
			}

			// Parse all the subnets from the config file
			List<Subnet> ParsedVpnSubnets = new List<Subnet>();
			foreach (string VpnSubnet in VpnSubnets)
			{
				ParsedVpnSubnets.Add(Subnet.Parse(VpnSubnet));
			}

			// Check if any network adapters have an IP within one of these subnets
			try
			{
				NetworkInterface? Interface = GetInterfaceForHost(HostName);
				if (Interface != null && Interface.OperationalStatus == OperationalStatus.Up)
				{
					IPInterfaceProperties Properties = Interface.GetIPProperties();
					foreach (UnicastIPAddressInformation UnicastAddressInfo in Properties.UnicastAddresses)
					{
						byte[] AddressBytes = UnicastAddressInfo.Address.GetAddressBytes();
						foreach (Subnet Subnet in ParsedVpnSubnets)
						{
							if (Subnet.Contains(AddressBytes))
							{
								if (!bAllowOverVpn)
								{
									Log.TraceInformationOnce("XGE coordinator {0} will be not be used over VPN (adapter '{1}' with IP {2} is in subnet {3}). Set <XGE><bAllowOverVpn>true</bAllowOverVpn></XGE> in BuildConfiguration.xml to override.", HostName, Interface.Description, UnicastAddressInfo.Address, Subnet);
								}
								return true;
							}
						}
					}
				}
			}
			catch (Exception Ex)
			{
				Logger.LogWarning("Unable to check whether host {HostName} is connected to VPN:\n{Ex}", HostName, ExceptionUtils.FormatExceptionDetails(Ex));
			}
			return false;
		}

		public static bool IsAvailable(ILogger Logger)
		{
			// Check the executable exists on disk
			if (!File.Exists(SNDBSBuildExe))
			{
				return false;
			}

			// Check the service is running
			if (!ServiceController.GetServices().Any(s => s.ServiceName.StartsWith("SNDBS") && s.Status == ServiceControllerStatus.Running))
			{
				return false;
			}

			// Check if we're connected over VPN
			if (!bAllowOverVpn && VpnSubnets != null && VpnSubnets.Length > 0)
			{
				string? BrokerHost;
				if (TryGetBrokerHost(out BrokerHost) && IsHostOnVpn(BrokerHost, Logger))
				{
					return false;
				}
			}

			return true;
		}

		/// <inheritdoc/>
		public override Task<bool> ExecuteActionsAsync(IEnumerable<LinkedAction> ActionsToExecute, ILogger Logger, IActionArtifactCache? actionArtifactCache)
		{
			return Task.FromResult(ExecuteActions(ActionsToExecute, Logger));
		}

		bool ExecuteActions(IEnumerable<LinkedAction> Actions, ILogger Logger)
		{
			if (!Actions.Any())
			{
				return true;
			}

			// Clean the intermediate directory in case there are any leftovers from previous builds
			if (DirectoryReference.Exists(IntermediateDir))
			{
				DirectoryReference.Delete(IntermediateDir, true);
			}

			DirectoryReference.CreateDirectory(IntermediateDir);
			if (!DirectoryReference.Exists(IntermediateDir))
			{
				throw new BuildException($"Failed to create directory \"{IntermediateDir}\".");
			}

			int IdCounter = 0;
			// Build the json script file to describe all the actions and their dependencies
			Dictionary<LinkedAction, string> ActionIds = Actions.ToDictionary(a => a, a => new Guid(++IdCounter, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).ToString());
			JsonSerializerOptions JsonOption = new JsonSerializerOptions();
			JsonOption.Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping;
			File.WriteAllText(ScriptFile.FullName, JsonSerializer.Serialize(new Dictionary<string, object>()
			{
				["jobs"] = Actions.Select(a =>
				{
					Dictionary<string, object> Job = new Dictionary<string, object>()
					{
						["id"] = ActionIds[a],
						["title"] = a.StatusDescription,
						["command"] = $"\"{a.CommandPath}\" {a.CommandArguments}",
						["working_directory"] = a.WorkingDirectory.FullName,
						["dependencies"] = a.PrerequisiteActions.Select(p => ActionIds[p]).ToArray(),
						["run_locally"] = !(a.bCanExecuteRemotely && a.bCanExecuteRemotelyWithSNDBS)
					};

					if (a.PrerequisiteItems.Any())
					{
						Job["explicit_input_files"] = a.PrerequisiteItems.Where(i => !(i.AbsolutePath.EndsWith(".rsp") || i.AbsolutePath.EndsWith(".response"))).Select(i => new Dictionary<string, object>()
						{
							["filename"] = i.AbsolutePath
						}).ToList();
					}

					// Add PCH source file dependencies for clang or clang-cl
					if (a.bIsGCCCompiler)
					{
						// Look for any prerequisite actions that produce .pch files and add any .cpp files they depend on
						var ExplicitInputFiles = Job["explicit_input_files"] as List<Dictionary<string, object>>;

						if (ExplicitInputFiles != null)
						{
							ExplicitInputFiles.AddRange(a.PrerequisiteActions
								.Where(Prereq => Prereq.ProducedItems.Any(Produced => Produced.AbsolutePath.EndsWith(".gch")))
								.SelectMany(Prereq => Prereq.PrerequisiteItems, (_, PrereqFile) => PrereqFile.AbsolutePath)
								.Where(Path => Path.EndsWith(".cpp"))
								.Select(Path => new Dictionary<string, object>()
								{
									["filename"] = Path
								}));
						}
					}

					string CommandDescription = String.IsNullOrWhiteSpace(a.CommandDescription) ? a.ActionType.ToString() : a.CommandDescription;
					Job["echo"] = $"{ProgressMarkupPrefix}{CommandDescription}:{a.StatusDescription}";

					return Job;
				}).ToArray()
			}, JsonOption));

			PrepareToolTemplates();
			bool bHasRewrites = GenerateSNDBSIncludeRewriteRules();

			IEnumerable<string> ConfigList = TargetDescriptors.Select(Descriptor => $"{Descriptor.Name}|{Descriptor.Platform}|{Descriptor.Configuration}");
			string ConfigDescription = String.Join(",", ConfigList);

			ProcessStartInfo StartInfo = new ProcessStartInfo(
				SNDBSBuildExe,
				$"-q -p \"{ConfigDescription}\" -s \"{ScriptFile}\" -templates \"{IntermediateDir}\""
				);
			StartInfo.UseShellExecute = false;
			if (bHasRewrites)
			{
				StartInfo.Arguments += $" --include-rewrite-rules \"{IncludeRewriteRulesFile}\"";
			}
			if (!bStopSNDBSCompilationAfterErrors)
			{
				StartInfo.Arguments += " -k";
			}
			return ExecuteProcessWithProgressMarkup(StartInfo, Actions.Count(), Logger);
		}

		/// <summary>
		/// Executes the process, parsing progress markup as part of the output.
		/// </summary>
		private bool ExecuteProcessWithProgressMarkup(ProcessStartInfo SnDbsStartInfo, int NumActions, ILogger Logger)
		{
			using (ProgressWriter Writer = new ProgressWriter("Compiling C++ source files...", false, Logger))
			{
				int NumCompletedActions = 0;
				string CurrentStatus = "";

				// Create a wrapper delegate that will parse the output actions
				DataReceivedEventHandler EventHandlerWrapper = (Sender, Args) =>
				{
					if (Args.Data != null)
					{
						string Text = Args.Data;
						if (Text.StartsWith(ProgressMarkupPrefix))
						{
							Writer.Write(++NumCompletedActions, NumActions);

							Text = Args.Data.Substring(ProgressMarkupPrefix.Length).Trim();
							string[] ActionInfo = Text.Split(':');
							Logger.LogInformation("[{NumCompletedActions}/{NumActions}] {ActionInfo0} {ActionInfo1}", NumCompletedActions, NumActions, ActionInfo[0], ActionInfo[1]);
							CurrentStatus = ActionInfo[1];
							return;
						}
						// Suppress redundant tool output of status we already printed (e.g., msvc cl prints compile unit name always)
						if (!Text.Equals(CurrentStatus))
						{
							WriteToolOutput(Text);
						}
					}
				};

				try
				{
					// Start the process, redirecting stdout/stderr if requested.
					Process LocalProcess = new Process();
					LocalProcess.StartInfo = SnDbsStartInfo;
					bool bShouldRedirectOuput = EventHandlerWrapper != null;
					if (bShouldRedirectOuput)
					{
						SnDbsStartInfo.RedirectStandardError = true;
						SnDbsStartInfo.RedirectStandardOutput = true;
						LocalProcess.EnableRaisingEvents = true;
						LocalProcess.OutputDataReceived += EventHandlerWrapper;
						LocalProcess.ErrorDataReceived += EventHandlerWrapper;
					}
					LocalProcess.Start();
					if (bShouldRedirectOuput)
					{
						LocalProcess.BeginOutputReadLine();
						LocalProcess.BeginErrorReadLine();
					}

					Logger.LogInformation("Distributing {NumAction} action{ActionS} to SN-DBS",
						NumActions,
						NumActions == 1 ? "" : "s");

					// Wait until the process is finished and return whether it all the tasks successfully executed.
					LocalProcess.WaitForExit();
					return LocalProcess.ExitCode == 0;
				}
				catch (Exception Ex)
				{
					Log.WriteException(Ex, null);
					return false;
				}
			}
		}

		private void PrepareToolTemplates()
		{
			foreach (KeyValuePair<string, string> Template in ActiveTemplates)
			{
				FileReference TemplateFile = FileReference.Combine(IntermediateDir, $"{Template.Key}.sn-dbs-tool.ini");
				string TemplateText = Template.Value;

				foreach (Nullable<DictionaryEntry> Variable in Environment.GetEnvironmentVariables(EnvironmentVariableTarget.Process))
				{
					if (Variable.HasValue)
					{
						TemplateText = TemplateText.Replace($"{{{Variable.Value.Key}}}", Variable.Value.Value!.ToString());
					}
				}

				File.WriteAllText(TemplateFile.FullName, TemplateText);
			}
		}

		private bool GenerateSNDBSIncludeRewriteRules()
		{
			// Get all distinct platform names being used in this build.
			List<string> Platforms = TargetDescriptors
				.Select(TargetDescriptor => UEBuildPlatform.GetBuildPlatform(TargetDescriptor.Platform).GetPlatformName())
				.Distinct()
				.ToList();

			if (Platforms.Count > 0)
			{
				// language=regex
				string[] Lines = new[]
				{
					@"pattern1=^COMPILED_PLATFORM_HEADER\(\s*([^ ,]+)\s*\)",
					$"expansions1={String.Join("|", Platforms.Select(Name => $"{Name}/{Name}$1|{Name}$1"))}",

					@"pattern2=^COMPILED_PLATFORM_HEADER_WITH_PREFIX\(\s*([^ ,]+)\s*,\s*([^ ,]+)\s*\)",
					$"expansions2={String.Join("|", Platforms.Select(Name => $"$1/{Name}/{Name}$2|$1/{Name}$2"))}",

					@"pattern3=^[A-Z]{5}_PLATFORM_HEADER_NAME\(\s*([^ ,]+)\s*\)",
					$"expansions3={String.Join("|", Platforms.Select(Name => $"{Name}/{Name}$1|{Name}$1"))}",

					@"pattern4=^[A-Z]{5}_PLATFORM_HEADER_NAME_WITH_PREFIX\(\s*([^ ,]+)\s*,\s*([^ ,]+)\s*\)",
					$"expansions4={String.Join("|", Platforms.Select(Name => $"$1/{Name}/{Name}$2|$1/{Name}$2"))}"
				};

				File.WriteAllText(IncludeRewriteRulesFile.FullName, String.Join(Environment.NewLine, new[] { "[computed-include-rules]" }.Concat(Lines)));
				return true;
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// SN-DBS templates that are automatically included in the build.
		/// </summary>
		private static readonly IReadOnlyDictionary<string, string> BuiltInTemplates = new Dictionary<string, string>()
		{
			["cl-filter.exe"] = @"
[tool]
family=msvc
vc_major_version=14
use_surrogate=true
force_synchronous_pdb_writes=true
error_report_mode=prompt

[group]
server={VC_COMPILER_DIR}\mspdbsrv.exe

[files]
main=cl-filter.exe
file01={VC_COMPILER_DIR}\c1.dll
file01={VC_COMPILER_DIR}\c1ui.dll
file02={VC_COMPILER_DIR}\c1xx.dll
file03={VC_COMPILER_DIR}\c2.dll
file04={VC_COMPILER_DIR}\mspdb140.dll
file05={VC_COMPILER_DIR}\mspdbcore.dll
file06={VC_COMPILER_DIR}\mspdbsrv.exe
file07={VC_COMPILER_DIR}\mspft140.dll
file08={VC_COMPILER_DIR}\vcmeta.dll
file09={VC_COMPILER_DIR}\*\clui.dll
file10={VC_COMPILER_DIR}\*\mspft140ui.dll
file11={VC_COMPILER_DIR}\localespc.dll
file12={VC_COMPILER_DIR}\cppcorecheck.dll
file13={VC_COMPILER_DIR}\experimentalcppcorecheck.dll
file14={VC_COMPILER_DIR}\espxengine.dll
file15={VC_COMPILER_DIR}\c1.exe

[output-file-patterns]
outputfile01=\s*""([^ "",]+\.cpp\.txt)\""

[output-file-rules]
rule01=*\sqmcpp*.log|discard=true
rule02=*\vctoolstelemetry*.dat|discard=true
rule03=*\Microsoft\Windows\Temporary Internet Files\*|discard=true
rule04=*\Microsoft\Windows\INetCache\*|discard=true

[input-file-rules]
rule01=*\sqmcpp*.log|ignore_transient_errors=true;ignore_unexpected_input=true
rule02=*\vctoolstelemetry*.dat|ignore_transient_errors=true;ignore_unexpected_input=true
rule03=*\Microsoft\Windows\Temporary Internet Files\*|ignore_transient_errors=true;ignore_unexpected_input=true
rule04=*\Microsoft\Windows\INetCache\*|ignore_transient_errors=true;ignore_unexpected_input=true

[system-file-filters]
filter01=msvcr*.dll
filter02=msvcp*.dll
filter03=vcruntime140*.dll
filter04=appcrt140*.dll
filter05=desktopcrt140*.dll
filter06=concrt140*.dll",
			["cl.exe"] = @"
[tool]
family=msvc
vc_major_version=14
use_surrogate=true
force_synchronous_pdb_writes=true
error_report_mode=prompt

[group]
server={VC_COMPILER_DIR}\mspdbsrv.exe

[files]
main={VC_COMPILER_DIR}\cl.exe
file01={VC_COMPILER_DIR}\c1.dll
file01={VC_COMPILER_DIR}\c1ui.dll
file02={VC_COMPILER_DIR}\c1xx.dll
file03={VC_COMPILER_DIR}\c2.dll
file04={VC_COMPILER_DIR}\mspdb140.dll
file05={VC_COMPILER_DIR}\mspdbcore.dll
file06={VC_COMPILER_DIR}\mspdbsrv.exe
file07={VC_COMPILER_DIR}\mspft140.dll
file08={VC_COMPILER_DIR}\vcmeta.dll
file09={VC_COMPILER_DIR}\*\clui.dll
file10={VC_COMPILER_DIR}\*\mspft140ui.dll
file11={VC_COMPILER_DIR}\localespc.dll
file12={VC_COMPILER_DIR}\cppcorecheck.dll
file13={VC_COMPILER_DIR}\experimentalcppcorecheck.dll
file14={VC_COMPILER_DIR}\espxengine.dll

[output-file-patterns]
outputfile01=\s*""([^ "",]+\.cpp\.txt\.json)\""

[output-file-rules]
rule01=*\sqmcpp*.log|discard=true
rule02=*\vctoolstelemetry*.dat|discard=true
rule03=*\Microsoft\Windows\Temporary Internet Files\*|discard=true
rule04=*\Microsoft\Windows\INetCache\*|discard=true

[input-file-rules]
rule01=*\sqmcpp*.log|ignore_transient_errors=true;ignore_unexpected_input=true
rule02=*\vctoolstelemetry*.dat|ignore_transient_errors=true;ignore_unexpected_input=true
rule03=*\Microsoft\Windows\Temporary Internet Files\*|ignore_transient_errors=true;ignore_unexpected_input=true
rule04=*\Microsoft\Windows\INetCache\*|ignore_transient_errors=true;ignore_unexpected_input=true

[system-file-filters]
filter01=msvcr*.dll
filter02=msvcp*.dll
filter03=vcruntime140*.dll
filter04=appcrt140*.dll
filter05=desktopcrt140*.dll
filter06=concrt140*.dll",
			["mspdbsrv.exe"] = @"
[tool]
use_cache=no

[files]
main={VC_COMPILER_DIR}\mspdbsrv.exe

[openmp]
omp=true
omp_min_threads=1",
			["clang++.exe"] = @"
[tool]
family=clang
extensions=.c;.cc;.cpp;.cxx;.c++;.h;.hpp;.s;.asm
adjacent_metadata_extensions=.pch;.gch;.pth;.o;.obj
include_path01=..\include
include_path02=..\include\c++\v1
include_path03=..\lib\clang\*\include
include_path04=%CPATH%;%C_INCLUDE_PATH%;%CPLUS_INCLUDE_PATH%

[files]
main=clang++.exe

[include-path-patterns]
include01=-(?:I|F|isystem|idirafter)[ \t]*(\""[^\""]+\""|[^ ]+)

[additional-include-file-patterns]
includefile01=--?include[ \t]*(\""[^\""]+\""|[^ ]+)

[additional-input-file-patterns]
inputfile01=--?(?:include-pch|fprofile-instr-use=|fprofile-sample-use=|fprofile-use=|fsanitize-ignorelist=)[ \t]*(\""[^\""]+\""|[^ ]+)

[output-file-patterns]
outputfile01=-o[ \t]*(\""[^\""]+\""|[^ ]+)

[definition-patterns]
definition01=-D[ \t]*""?([a-zA-Z_0-9]+)(?:[\s""]|$)()
definition02=-D[ \t]*""?([a-zA-Z_0-9]+)=(?:\\""|<)(.+?)(?:\\""|>)
definition03=-D[ \t]*""?([a-zA-Z_0-9]+)=((?!(?:\\""|<))[^ ""]*)

[input-scanners]
scanner01=c .*
scanner02=assembler .s;.asm",
		};
	}
}
