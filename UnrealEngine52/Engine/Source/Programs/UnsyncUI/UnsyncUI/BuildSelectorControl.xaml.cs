// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Threading.Tasks.Dataflow;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;

namespace UnsyncUI
{
	/// <summary>
	/// Interaction logic for BuildSelectorControl.xaml
	/// </summary>
	public partial class BuildSelectorControl : UserControl
	{
		GridViewColumnHeader lastHeaderClicked = null;
		ListSortDirection lastDirection = ListSortDirection.Ascending;

		public BuildSelectorControl()
		{
			InitializeComponent();
		}

		private void SyncHyperlink_Click(object sender, RoutedEventArgs e)
		{
			(DataContext as ProjectModel).StartSync(SelectedPlatforms.SelectedItems.OfType<BuildPlatformModel>());
		}

		private void BuildsHeader_Click(object sender, RoutedEventArgs e)
		{
			var headerClicked = e.OriginalSource as GridViewColumnHeader;
			if (headerClicked == null || headerClicked.Role == GridViewColumnHeaderRole.Padding)
				return;

			ListSortDirection direction;
			if (headerClicked != lastHeaderClicked)
			{
				direction = ListSortDirection.Ascending;
			}
			else
			{
				direction = lastDirection == ListSortDirection.Ascending
					? ListSortDirection.Descending
					: ListSortDirection.Ascending;
			}

			if (headerClicked.Column == colStream)
			{
				Sort((colStream.Header as string, direction), (colCL.Header as string, ListSortDirection.Descending));
			}
			else
			{
				var columnBinding = headerClicked.Column.DisplayMemberBinding as Binding;
				var sortBy = columnBinding?.Path.Path ?? headerClicked.Column.Header as string;

				Sort((sortBy, direction));
			}

			lastHeaderClicked = headerClicked;
			lastDirection = direction;
		}

		private void Sort(params (string field, ListSortDirection direction)[] args)
		{
			var dataView = CollectionViewSource.GetDefaultView(SelectedBuild.ItemsSource);
			dataView.SortDescriptions.Clear();
			foreach (var desc in args)
			{
				dataView.SortDescriptions.Add(new SortDescription(desc.field, desc.direction));
			}
			dataView.Refresh();
		}
	}

	public sealed class BuildPlatformModel
	{

		public BuildModel Build { get; }
		public string Platform { get; }
		public string Flavor { get; }
		public string Description { get; }
		public string FullPath { get; }
		public string DestPathRelative { get; }
		public string Include { get;  }

		public string Name => Build == null
			? "Custom Sync" 
			: $"{Build.Stream} - {Platform} - CL {Build.CL}";

		public string SafeName => Name.Replace('/', '+').Replace(" ", "");

		public BuildPlatformModel(BuildModel build, string platform, string fullPath, string flavor, string include)
		{
			Build = build;
			Platform = platform;
			FullPath = fullPath;
			Flavor = flavor;
			DestPathRelative = Flavor == null ? Platform : Platform + Flavor;

			Description = Flavor == null ? Platform : $"{Platform} {Flavor}";
			Include = include;
		}
	}

	public sealed class BuildModel : BaseModel
	{
		private CancellationTokenSource cts;

		public string Stream => Template.Stream;
		public string CL => Template.CL;
		public string Suffix => Template.Suffix;

		public Config.Directory RootDir { get; }
		public string Path { get; }

		public Config.BuildTemplate Template { get; }

		public Command OnRefreshPlatformsClicked { get; }
		public Command OnStopRefreshPlatformsClicked { get; }

		public ObservableCollection<BuildPlatformModel> Platforms { get; } = new ObservableCollection<BuildPlatformModel>();

		public BuildModel(string path, Config.Directory rootDir, Config.BuildTemplate template)
		{
			Path = path;
			RootDir = rootDir;
			Template = template;

			OnRefreshPlatformsClicked = new Command(() => PopulatePlatforms()) { Enabled = true };
			OnStopRefreshPlatformsClicked = new Command(() =>
			{
				cts?.Cancel();
			});
		}

		public bool HasPlatform(string platform) => Platforms.Where(p => p.Platform == platform).Any();

		private async Task EnumeratePlatforms(Config.Directory currentDir, string path, Config.BuildTemplate template, CancellationTokenSource cancellationToken)
		{
			if (!currentDir.Parse(path, ref template))
				return;

			foreach (var childDir in await AsyncIO.EnumerateDirectoriesAsync(path, cancellationToken.Token))
			{
				if (System.IO.Path.GetFileName(childDir) == ".unsync")
				{
					List<Config.BuildTemplate> fileGroups = null;
					if (currentDir.FileGroups?.Any() == true)
					{
						var files = await AsyncIO.EnumerateFilesAsync(path, cancellationToken.Token);
						currentDir.ParseFileGroups(files.ToList(), template, out fileGroups);
					}
					if (fileGroups?.Any() == true)
					{
						foreach (var fileGroup in fileGroups)
						{
							Platforms.Add(new BuildPlatformModel(this, fileGroup.Platform, path, fileGroup.Flavor, fileGroup.Include));
						}
						Platforms.Add(new BuildPlatformModel(this, template.Platform, path, "All", null));
					}
					else
					{
						// This folder is a valid build
						Platforms.Add(new BuildPlatformModel(this, template.Platform, path, template.Flavor, null));
					}
				}
				else
				{
					foreach (var subDir in currentDir.SubDirectories)
					{
						await EnumeratePlatforms(subDir, childDir, template, cancellationToken);
					}
				}
			}
		}

		public async void PopulatePlatforms()
		{
			cts?.Cancel();
			cts = new CancellationTokenSource();

			OnRefreshPlatformsClicked.Enabled = false;
			OnStopRefreshPlatformsClicked.Enabled = true;

			Platforms.Clear();

			try
			{
				await EnumeratePlatforms(RootDir, Path, Template, cts);
			}
			catch (OperationCanceledException)
			{ }
			finally
			{
				OnRefreshPlatformsClicked.Enabled = true;
				OnStopRefreshPlatformsClicked.Enabled = false;
			}
		}
	}

	public sealed class ProjectModel : TabModel
	{
		private bool firstRefresh = true;
		private Action<IEnumerable<(string DstPath, string[] Exclusions, BuildPlatformModel Model)>> onBuildsSelected;

		public Config.Project Definition { get; }
		public string Name => Definition.Name;

		public Command OnRefreshBuildsClicked { get; }
		public Command OnStopRefreshBuildsClicked { get; }
		public Command<IList> OnStartSync { get; }
		public Command CopyBuildCLClicked { get; }
		public Command OpenBuildLocationClicked { get; }
		public Command ResetDestinationClicked { get; }

		public ObservableCollection<BuildModel> Builds { get; } = new ObservableCollection<BuildModel>();
		public ICollectionView BuildsView { get; }

		public bool BuildsViewFilter(object item)
		{
			BuildModel build = (BuildModel)item;

			foreach (var term in searchTerms.Split(' '))
			{
				if (!build.CL.Contains(term, StringComparison.InvariantCultureIgnoreCase)
				&& !build.Stream.Contains(term, StringComparison.InvariantCultureIgnoreCase)
				&& !build.Suffix.Contains(term, StringComparison.InvariantCultureIgnoreCase))
				{
					return false;
				}
			}

			return true;
		}

		private BuildModel selectedBuild = null;
		public BuildModel SelectedBuild
		{
			get => selectedBuild;
			set
			{
				SetProperty(ref selectedBuild, value);

				if (selectedBuild != null)
				{
					selectedBuild.PopulatePlatforms();
				}
			}
		}

		private string searchTerms = "";
		public string SearchTerms
		{
			get => searchTerms;
			set
			{
				SetProperty(ref searchTerms, value);
				BuildsView.Refresh();
			}
		}

		private CancellationTokenSource cts = null;

		/// Default download directory (either auto-detected or loaded from project config)
		private string defaultDstPath = null;

		/// Current download path selected by user or loaded from user preferences.
		/// This value persists between application sessions (saved per project).
		/// It can be reset to default by pressing "Reset" button in the UI.
		private string dstPath = null;

		public string DstPath
		{
			get => dstPath;
			set
			{
				SetProperty(ref dstPath, value);
				App.Current.UserConfig.ProjectDestinationMap[Name] = value;

				ResetDestinationClicked.Enabled = CanResetDestinationToDefault;
			}
		}

		public ProjectModel(Config.Project definition, Action<IEnumerable<(string DstPath, string[] Exclusions, BuildPlatformModel Model)>> onBuildsSelected)
		{
			this.Definition = definition;
			this.onBuildsSelected = onBuildsSelected;
			OnRefreshBuildsClicked = new Command(RefreshBuildsAsync) { Enabled = true };
			OnStopRefreshBuildsClicked = new Command(StopRefreshBuilds) { Enabled = false };
			OnStartSync = new Command<IList>(l => StartSync(l.OfType<BuildPlatformModel>())) { Enabled = true };
			CopyBuildCLClicked = new Command(CopyBuildCL) { Enabled = true };
			OpenBuildLocationClicked = new Command(OpenBuildLocation) { Enabled = true };

			BuildsView = CollectionViewSource.GetDefaultView(Builds);
			BuildsView.Filter = BuildsViewFilter;
			BuildsView.SortDescriptions.Clear();
			BuildsView.SortDescriptions.Add(new SortDescription("CL", ListSortDirection.Descending));

			SearchTerms = App.Current.DefaultSearchTerms;

			if (definition.Destination != null)
			{
				defaultDstPath = Path.GetFullPath(definition.Destination);
			}
			else
			{
				// If this is the first time running, choose the destination dir as the largest local disk
				// @todo: remember user choice
				var bestDrive = DriveInfo.GetDrives()
				.Where(d => d.DriveType == DriveType.Fixed)
				.Where(d => !d.VolumeLabel.Contains("google", StringComparison.InvariantCultureIgnoreCase)) // Ignore Google Drive (it appears as a 'fixed' disk)
				.OrderByDescending(d => d.TotalSize).FirstOrDefault();
				if (bestDrive != null)
				{
					defaultDstPath = Path.Combine(bestDrive.RootDirectory.FullName, "Unsync", definition.Name);
				}
			}

			// If we failed to find a reasonable default destination then set it to a non-null empty string,
			// which prevents sync command until user explicitly sets the path
			if (defaultDstPath == null)
			{
				defaultDstPath = "";
			}

			string userDstPath = null;

			if (App.Current.UserConfig.ProjectDestinationMap.TryGetValue(Name, out userDstPath))
			{
				dstPath = userDstPath;
			}
			else
			{
				dstPath = defaultDstPath;
			}

			ResetDestinationClicked = new Command(ResetDestinationToDefault) { Enabled = CanResetDestinationToDefault };
		}
		private bool CanResetDestinationToDefault => dstPath != defaultDstPath && defaultDstPath.Length != 0;

		private void ResetDestinationToDefault()
		{
			DstPath = defaultDstPath;
		}

		private void StopRefreshBuilds()
		{
			if (cts != null)
			{
				cts.Cancel();
				OnStopRefreshBuildsClicked.Enabled = false;
			}
		}

		public async void RefreshBuildsAsync()
		{
			cts = new CancellationTokenSource();

			OnRefreshBuildsClicked.Enabled = false;
			OnStopRefreshBuildsClicked.Enabled = true;

			Builds.Clear();
			SelectedBuild = null;

			try
			{
				var timer = new Stopwatch();
				timer.Start();
				var buildsPipe = Definition.EnumerateBuilds(cts.Token);

				while (await buildsPipe.OutputAvailableAsync(cts.Token))
				{
					Builds.Add(await buildsPipe.ReceiveAsync());
				}

				timer.Stop();
				Debug.WriteLine($"Search took {timer.Elapsed.TotalSeconds} seconds.");
			}
			catch (IOException)
			{
				// @todo show an error message
			}
			catch (OperationCanceledException)
			{ }
			finally
			{
				OnRefreshBuildsClicked.Enabled = true;
				OnStopRefreshBuildsClicked.Enabled = false;
			}
		}

		public void StartSync(IEnumerable<BuildPlatformModel> selectedBuilds)
		{
			onBuildsSelected?.Invoke(selectedBuilds.Select(s => (Path.Combine(dstPath, s.DestPathRelative), Definition.Exclusions?.ToArray(), s)).ToList());
		}

		public override void OnSelected()
		{
			if (firstRefresh)
			{
				RefreshBuildsAsync();
				firstRefresh = false;
			}
		}

		private void CopyBuildCL()
		{
			if (selectedBuild != null)
			{
				// Clipboard can be locked if timing is just right(wrong), retry a few times on failure.
                int retryDelay = 0;
				const int attempts = 7;
				for (int attempt = 0; attempt < attempts; attempt++)
				{
					if (retryDelay > 0)
					{
                        System.Threading.Thread.Sleep(retryDelay);
                        retryDelay *= 2;
                    }
					else
					{
						retryDelay = 5;
                    }
                    try
                    {
                        Clipboard.SetText(selectedBuild.CL, TextDataFormat.Text);
						break;
                    }
                    catch (System.Runtime.InteropServices.COMException)
                    {
	                }
                }
			}
		}

		private void OpenBuildLocation()
		{
			if (selectedBuild != null)
			{
				Shell.LaunchExplorer(selectedBuild.Path);
			}
		}
	}
}
