// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;


namespace UnrealWindowsForms
{
	public partial class BuildLauncher : Form
	{
		public List<String> RunParams;

		public string BuildLocation { get; private set; }


		public BuildLauncher(String InBuildLocation)
		{
			InitializeComponent();

			BuildLocation = InBuildLocation;

			RunParams = new List<string>();

			ListItem[] PlatformItems = {
				new ListItem("PS4", "platform=PS4", "target=default"),
				new ListItem("Windows", "platform=Win64"),
				new ListItem("Android", "platform=Android"),
				new ListItem("IOS", "platform=IOS"),
				new ListItem("Switch", "platform=Switch"),
				new ListItem("EditorGame", "build=editor")           
			};

			platformList.DataSource = PlatformItems;
			platformList.SelectedIndex = 0;

			ListItem[] ConfigItems = {
				new ListItem("Development", "configuration=Development"),
				new ListItem("Test", "configuration=Test"),
				new ListItem("Shipping", "configuration=Shipping")
			};

			configurationList.DataSource = ConfigItems;
			configurationList.SelectedIndex = 0;

			ListItem[] ModeItems = {
				new ListItem("Normal"),
				new ListItem("Athena Terrain (nomcp)", "nomcp", "map=Apollo_Terrain")
			};

			modeList.DataSource = ModeItems;
			modeList.SelectedIndex = 0;

			UpdateBuildList();
			UpdateEnabledState();
			UpdateEstimatedTime();
		}
		
		private void refreshButton_Click(object sender, EventArgs e)
		{
			UpdateBuildList();
		}

		private bool IsBuildValid(DirectoryInfo Dir)
		{
			if (Dir.Name.StartsWith("++") == false)
				return false;

			try
			{
			DirectoryInfo[] Subdirs = Dir.GetDirectories();

			// if we are not using an editor server then we need the windows server to be build
			if (optionEditorServer.Checked == false)
			{
				if (Dir.GetDirectories().Where(D => D.Name == "WindowsServer").Count() < 1 )
				{
					return false;
				}
			}

			// check that this platform is built
			if (Dir.GetDirectories().Where(D => D.Name == platformList.SelectedItem.ToString()).Count() < 1)
			{
				return false;
			}
			}
			catch (System.Exception )
			{
				return false;
			}

			

			return true;
		}

		private void UpdateBuildList()
		{

			string Path = BuildLocation; //  ;
			string SelectedBuild = "";
			if (buildList.SelectedItem != null)
			{
				SelectedBuild = buildList.SelectedItem.ToString();
			}

			buildList.Items.Clear();

			DirectoryInfo Di = new DirectoryInfo(Path);

			IEnumerable<DirectoryInfo> Dirs = Di.GetDirectories().OrderByDescending(D => D.CreationTime);

			buildList.Items.Add(new ListItem("Currently synced Branch+CL", "build=usesyncedbuild"));

			foreach (DirectoryInfo Dir in Dirs)
			{
				if (IsBuildValid(Dir) == false)
				{
					continue;
				}

				DateTime Creation = Dir.CreationTime;

				Match M = Regex.Match(Dir.Name, @".+\+(.+)-CL-(\d+)");

				string Name = string.Format("{0} CL-{1}  ({2:H:mmtt on M/d})", M.Groups[1].Value, M.Groups[2].Value, Creation);

				buildList.Items.Add(new ListItem(Name, string.Format("build={0}", Dir.Name)));
			}

			buildList.SelectedIndex = 0;

			for ( int I = 0; I < buildList.Items.Count; ++I )
			{
				if ( buildList.Items[I].ToString() == SelectedBuild )
				{
					buildList.SelectedIndex = I;
					break;
				}
			}
		}
		
		private void goButton_Click(object sender, EventArgs e)
		{	

			RunParams.AddRange((platformList.SelectedValue as ListItem).Args);
			RunParams.AddRange((configurationList.SelectedValue as ListItem).Args);
			RunParams.AddRange((modeList.SelectedValue as ListItem).Args);

			if (testTextBox.Text.Length > 0)
			{
				RunParams.Add("Test=" + testTextBox.Text);
			}

			if (optionTab.SelectedIndex == 0)
			{
				if (platformList.SelectedText.Contains("Editor") == false)
				{
					RunParams.AddRange((buildList.SelectedItem as ListItem).Args);
				}

				RunParams.AddRange(new string[] { "skipbuild", "skipcook" });

				if (optionEditorServer.Checked == true)
				{
					RunParams.Add("EditorServer");
				}
			}
			else
			{

				// Because we use the BuildCookTest script we say what we don't want to do...
				
				if (optionRealTimeUpdates.Checked == true)
				{
					RunParams.Add("realtimecooking");
					RunParams.Add("skipcook");

					RunParams.Add("build=usesyncedbuild" );
				}
				else if (optionSharedCookedBuild.Checked == true && optionSharedCookedBuild.Enabled == true)
				{
					RunParams.Add("iteratesharedcookedbuild=any");
				}

				if (optionUsePrecompiledBuild.Checked == true && optionUsePrecompiledBuild.Enabled == true)
				{
					RunParams.Add("IterateSharedBuildUsePrecompiledExe");
				}

				if ( optionForceStage.Checked == true && optionForceStage.Enabled == true)
				{
					RunParams.Add("stage");
				}

				RunParams.Add("EditorServer");

				if (optionBuildClient.Checked == false)
				{
					RunParams.Add("skipbuildclient");
				}
				if (optionBuildEditor.Checked == false)
				{
					RunParams.Add("skipbuildeditor");
				}
				if (optionCook.Checked == false )
				{
					RunParams.Add("skipcook");
				}
				if (platformList.SelectedItem.ToString() == "PS4" && optionSharedCookedBuild.Checked)
				{
					//disable packaging step for LaunchBuild
					//attempts to create a PS4 package with a mismatched TitleID, which the publishing tools explode on.packages are useless for anything other than installing, so this should be skippable
				   RunParams.Add("skippackage");
				}
			}

			this.DialogResult = DialogResult.OK;
			this.Close();
		}

		private int EnabledStateRecurseProtection = 0;
		private void UpdateEnabledState()
		{
			++EnabledStateRecurseProtection;
			if (EnabledStateRecurseProtection == 1)
			{
				optionUsePrecompiledBuild.Enabled = true;
				optionSharedCookedBuild.Enabled = true;
				optionBuildClient.Enabled = true;
				optionUsePrecompiledBuild.Enabled = true;
				optionCook.Enabled = true;
				optionForceStage.Enabled = true;

				if (optionTab.SelectedIndex == 1)
				{
					if (optionBuildClient.Checked == true)
					{
						optionUsePrecompiledBuild.Enabled = false;
					}

					if (optionCook.Checked == true)
					{
						optionForceStage.Enabled = false;
					}
					else
					{
						optionSharedCookedBuild.Enabled = false;
					}

					if ( (optionSharedCookedBuild.Checked == false || optionSharedCookedBuild.Enabled == false) && 
						(optionRealTimeUpdates.Checked == false || optionRealTimeUpdates.Enabled == false ))
					{
						optionUsePrecompiledBuild.Enabled = false;
					}

					if (optionRealTimeUpdates.Checked == true)
					{
						optionSharedCookedBuild.Checked = true;
						optionSharedCookedBuild.Enabled = false;

						optionBuildClient.Checked = false;
						optionBuildClient.Enabled = false;

						optionUsePrecompiledBuild.Checked = true;
						optionUsePrecompiledBuild.Enabled = false;

						optionCook.Enabled = false;
						optionCook.Checked = false;
					}
				}
			}
			--EnabledStateRecurseProtection;
		}

		private void UpdateEstimatedTime()
		{
			int firstTimeEta = 0;
			int iterativeEta = 0;
			if (optionTab.SelectedIndex == 0)
			{
				firstTimeEta += 10;
				iterativeEta += 1;
			}
			else
			{
				iterativeEta += 1; // deploy
				if (optionRealTimeUpdates.Checked)
				{
					firstTimeEta += 10; // first time deploy from network
					iterativeEta += 1; // load time overhead
				}
				if (optionBuildEditor.Checked)
				{
					firstTimeEta += 20; // full compile with xge
					iterativeEta += 2; // assume a minimal amount to compile..
				}
				if ( optionBuildClient.Checked )
				{
					firstTimeEta += 20;
					iterativeEta += 2; // assume a minimal amount to compile..
				}
				if (optionCook.Checked)
				{
					if (optionSharedCookedBuild.Checked)
					{
						firstTimeEta += 10; // 1 minute to copy from network + iterative cook time 4 minutes + 3 minutes staging + 1 deploy minute
						iterativeEta += 9;// no need to copy from network second time
					}
					else
					{
						firstTimeEta += 29; // 25 for cook + unreal pak 4 minutes
						iterativeEta += 9; // iterative cook
					}
					firstTimeEta += 10; // 10 minutes for a deploy
					iterativeEta += 0; // 0 minutes for iterative deploy
				}
			}

			etaLabel.Text = "First time ETA: " + firstTimeEta + "minutes\nIterative ETA: " + iterativeEta + "minutes";
		}

		private void optionSharedCookedBuild_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
			UpdateEstimatedTime();
		}

		private void optionUsePrecompiledBuild_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
			UpdateEstimatedTime();
		}

		private void optionTab_TabIndexChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
			UpdateEstimatedTime();
		}

		private void optionTab_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
			UpdateEstimatedTime();
		}

		private void optionBuildEditor_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEstimatedTime();
		}

		private void optionBuildClient_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
			UpdateEstimatedTime();
		}

		private void optionRealTimeUpdates_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
			UpdateEstimatedTime();
		}

		private void label5_Click(object sender, EventArgs e)
		{

		}

		private void platformList_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdateBuildList();
		}

		private void EditorServer_CheckedChanged(object sender, EventArgs e)
		{
			UpdateBuildList();
		}

		private void optionCook_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledState();
		}
	}

	class ListItem
	{
		public ListItem(string InText, params string[] InArgs)
		{
			Text = InText;
			Args = new List<string>(InArgs);
		}
		public ListItem(string InText)
		{
			Text = InText;
			Args = new List<string>();
		}

		public string Text;
		public List<string> Args;
		public override string ToString() { return Text; }
	}
}
