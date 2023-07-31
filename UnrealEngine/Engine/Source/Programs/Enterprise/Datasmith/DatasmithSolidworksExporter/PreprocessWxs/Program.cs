// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Threading;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Windows.Forms;

namespace PreprocessWxs
{
	class Program
	{
		static void ProcessBody(ref string text, ref bool copying)
		{
			if (text.Contains("<Component"))
			{
				text = text.Replace(">", " Win64=\"yes\">");
				copying = true;
			}
			else if (text.Contains("<File"))
			{
				if (text.Contains("SourceDir\\Debug\\"))
					text = text.Replace("SourceDir\\Debug\\", "$(var.SolidworksBinDir)/");
				else if (text.Contains("SourceDir\\Release\\"))
					text = text.Replace("SourceDir\\Release\\", "$(var.SolidworksBinDir)/");
			}
			//<TypeLib Id="{14E2A942-3C3F-3B90-A32E-857D2B46BCBF}" Description="SolidworksDatasmith" HelpDirectory="dir39B22699688E51DCD8DCBB99A47E835B" Language="0" MajorVersion="1" MinorVersion="0">
			else if (text.Contains("<TypeLib"))
			{
				int pos1 = text.IndexOf("HelpDirectory=\"");
				int pos2 = text.IndexOf("Language=\"");
				if (pos2 < 0)
					pos2 = text.IndexOf("MajorVersion=\"");
				string temp = text.Substring(0, pos1);
				temp += text.Substring(pos2);
				text = temp;
			}
		}

		static void ProcessEnd(string text, ref bool copying)
		{
			if (text.Contains("</Component>"))
				copying = false;
		}

		static void BuildCandleScript(string Config, string ProjectPath, string EngineDir)
		{
			string binPath = Path.Combine(ProjectPath, "bin", Config);
			string objPath = Path.Combine(ProjectPath, "obj");
			string wixPath = string.Format(@"{0}\Source\ThirdParty\WiX\3.8", EngineDir);
			string script = "";
			script += string.Format(":: Config = {0}, Path = {1}\n\n", Config, ProjectPath);
			script += Path.Combine(wixPath, "Candle.exe");
			script += " -sw1076 -sw1077";
			script += string.Format(" -dConfiguration={0}", Config);
			script += string.Format(@" -dOutDir={0}\\", binPath);
			script += " -dPlatform=x86";
			script += string.Format(@" -dProjectName=DatasmithSolidworks");
			script += string.Format(@" -dTargetDir={0}\\", binPath);
			script += " -dTargetExt=.msi";
			script += " -dTargetFileName=DatasmithSolidworks.msi";
			script += " -dTargetName=DatasmithSolidworks";
			script += string.Format(@" -dTargetPath=""{0}\DatasmithSolidworks.msi""", binPath);
			script += string.Format(@" -out {0}\\", objPath);
			script += " -arch x86";
			script += string.Format(@" -ext ""{0}\WixUtilExtension.dll""", wixPath);
			script += string.Format(@" -ext ""{0}\WixUIExtension.dll""", wixPath);
			script += string.Format(@" -ext ""{0}\WixNetFxExtension.dll""", wixPath);

			string[] files = Directory.GetFiles(ProjectPath, "*.wxs");
			foreach (var f in files)
				script += string.Format(@" ""{0}""", f);

			File.WriteAllText(Path.Combine(ProjectPath, "candle.cmd"), script);
		}

		static void BuildLightScript(string Config, string ProjectPath, string EngineDir)
		{
			string binPath = Path.Combine(ProjectPath, "bin", Config);
			string objPath = Path.Combine(ProjectPath, "obj");
			string wixPath = string.Format(@"{0}\Source\ThirdParty\WiX\3.8", EngineDir);
			string script = "";
			script += Path.Combine(wixPath, "Light.exe");
			script += string.Format(@" -out ""{0}\en-US\DatasmithSolidworks.msi""", binPath);
			script += string.Format(@" -pdbout ""{0}\en-US\DatasmithSolidworks.wixpdb""", binPath);
			script += " -sw1076 -sw1077";
			script += " -cultures:en-US";
			script += string.Format(@" -ext ""{0}\WixUtilExtension.dll""", wixPath);
			script += string.Format(@" -ext ""{0}\WixUIExtension.dll""", wixPath);
			script += string.Format(@" -ext ""{0}\WixNetFxExtension.dll""", wixPath);
			script += string.Format(@" -loc ""{0}\Localization\de-de.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\en-us.wxl""", ProjectPath); ;
			script += string.Format(@" -loc ""{0}\Localization\es-es.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\fr-fr.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\ja-jp.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\ko-kr.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\pt-pt.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\UtilExtension_es-es.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\UtilExtension_fr-fr.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\UtilExtension_ja-jp.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\UtilExtension_ko-kr.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\UtilExtension_pt-pt.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\UtilExtension_zh_cn.wxl""", ProjectPath);
			script += string.Format(@" -loc ""{0}\Localization\zh-cn.wxl""", ProjectPath);
			script += " -sice:ICE30";
			script += " -sice:ICE82";
			script += " -sice:ICE03";
			script += " -sice:ICE57";
			script += " -spdb";
			script += string.Format(@" -contentsfile ""{0}\{1}\DatasmithSolidworks.wixproj.BindContentsFileListen-US.txt""", objPath, Config);
			script += string.Format(@" -outputsfile ""{0}\{1}\DatasmithSolidworks.wixproj.BindOutputsFileListen-US.txt""", objPath, Config);
			script += string.Format(@" -builtoutputsfile ""{0}\{1}\DatasmithSolidworks.wixproj.BindBuiltOutputsFileListen-US.txt""", objPath, Config);

			string[] files = Directory.GetFiles(ProjectPath, "*.wxs");
			foreach (var f in files)
			{
				string fileName = Path.GetFileNameWithoutExtension(f);
				string newPath = Path.Combine(objPath, fileName + ".wixobj");
				script += string.Format(@" ""{0}""", newPath);
			}

			File.WriteAllText(Path.Combine(ProjectPath, "light.cmd"), script);
		}

		static void Main(string[] args)
		{
			// uncomment for to debug
			//int processId = Process.GetCurrentProcess().Id;
			//string message = string.Format("Please attach the debugger (elevated on Vista or Win 7) to process [{0}].", processId);
			//MessageBox.Show(message, "Debug");

			string[] asm = File.ReadAllLines(args[1]);
			string[] tlb = File.ReadAllLines(args[2]);
			string installerDir = args[3];
			string EngineDir = args[4];

			// // process TLB
			List<string> tlb_chunk = new List<string>();
			bool copying = false;
			for (int i = 0; i < tlb.Length; i++)
			{
				ProcessBody(ref tlb[i], ref copying);
				if (copying)
					tlb_chunk.Add(tlb[i]);
				ProcessEnd(tlb[i], ref copying);
			}

			// process ASM
			List<string> asm_chunk = new List<string>();
			copying = false;
			for (int i = 0; i < asm.Length; i++)
			{
				ProcessBody(ref asm[i], ref copying);
				if (copying)
					asm_chunk.Add(asm[i]);
				ProcessEnd(asm[i], ref copying);
			}

			// get the version info
			FileVersionInfo version = FileVersionInfo.GetVersionInfo(args[0]);

			List<string> Product = new List<string>();
			string[] template = File.ReadAllLines(Path.Combine(installerDir,"Product.template"));
			foreach (var line in template)
			{
				if (line.Contains("$ASM_COMPONENT$"))
				{
					foreach (var ll in asm_chunk)
						Product.Add(ll);
				}
				else if (line.Contains("$TLB_COMPONENT$"))
				{
					foreach (var ll in tlb_chunk)
						Product.Add(ll);
				}
				else if (line.Contains("$ASM_VERSION$"))
				{
					string completed = line.Replace("$ASM_VERSION$", version.ProductVersion);
					Product.Add(completed);
				}
				else if (line.Contains("$ENGINE_DIR$"))
				{
					string completed = line.Replace("$ENGINE_DIR$", EngineDir);
					Product.Add(completed);
				}
				else if (line.Contains("$PRODUCT_BIN_DIR$"))
				{
					string completed = line.Replace("$PRODUCT_BIN_DIR$", Path.GetDirectoryName(args[0]));
					Product.Add(completed);
				}
				else
					Product.Add(line);
			}

			File.WriteAllLines(Path.Combine(installerDir, "Product.wxs"), Product.ToArray());

			BuildCandleScript("Release", installerDir, EngineDir);
			BuildLightScript("Release", installerDir, EngineDir);
		}
	}
}
