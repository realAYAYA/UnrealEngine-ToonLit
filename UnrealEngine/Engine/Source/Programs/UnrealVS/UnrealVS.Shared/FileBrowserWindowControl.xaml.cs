// Copyright Epic Games, Inc. All Rights Reserved.

using EnvDTE;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;

namespace UnrealVS
{
	[global::System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("", "VSTHRD010")]
	public partial class FileBrowserWindowControl : UserControl
	{
		FileBrowserWindow Window;
		bool ClearFilter;
		bool IsRefreshing;
		ListBox ActiveFilesListBox;

		public class FileItem
		{
			public string Name { get; set; }
			public string File { get; set; }
			public string Project { get; set; }
			public bool Selectable { get; set; }
		}

		String UnrealVsFileName;

		FileItem[] AllFileItems;
		List<FileItem> BookmarkedFileItems;
		List<FileItem> RecentFileItems;

		Dictionary<string, FileItem> AllFileItemsLookup;

		public FileBrowserWindowControl(FileBrowserWindow window)
		{
			Window = window;
			this.InitializeComponent();

			AllFileItems = Array.Empty<FileItem>();
			BookmarkedFileItems = new List<FileItem>();
			RecentFileItems = new List<FileItem>();
			IsVisibleChanged += FileBrowserWindowControl_IsVisibleChanged;
			foreach (var listView in new[] { AllFilesListBox, BookmarkedFilesListBox, RecentFilesListBox })
			{
				listView.ItemsSource = new List<FileItem>();
				listView.KeyDown += FileListView_KeyDown;
				listView.PreviewKeyDown += FileListView_PreviewKeyDown;
				listView.SelectionChanged += FileListView_SelectionChanged;
				listView.GotFocus += FileListView_GotFocus;
				listView.MouseDoubleClick += ListView_MouseDoubleClick;
				listView.SelectionMode = SelectionMode.Extended;
			}
			FilterEditBox.TextChanged += FilterEditBox_TextChanged;
			FilterEditBox.KeyDown += FilterEditBox_KeyDown;
			FilterEditBox.PreviewKeyDown += FilterEditBox_PreviewKeyDown;
			DataObject.AddPastingHandler(FilterEditBox, FilterEditBox_OnPaste);
			FilesListTab.SelectionChanged += FilesListTab_SelectionChanged;
			FilesListTab.SelectedIndex = 0;

			ActiveFilesListBox = AllFilesListBox;
			this.PreviewKeyDown += FileBrowserWindowControl_PreviewKeyDown;
		}

		private void FilterEditBox_OnPaste(object sender, DataObjectPastingEventArgs e)
		{
			var isText = e.SourceDataObject.GetDataPresent(DataFormats.UnicodeText, true);
			if (!isText)
				return;

			var text = e.SourceDataObject.GetData(DataFormats.UnicodeText) as string;

			if (string.IsNullOrEmpty(text) || text.IndexOfAny(Path.GetInvalidPathChars()) != -1)
				return;

			FilterEditBox.Text = Path.GetFileName(text);
			e.CancelCommand();
			FilterEditBox.CaretIndex = FilterEditBox.Text.Length;
		}

		[STAThread]
		private void FileBrowserWindowControl_PreviewKeyDown(object sender, KeyEventArgs e)
		{
			if (FilterEditBox.IsKeyboardFocused)
				return;

			if (Keyboard.Modifiers.HasFlag(ModifierKeys.Control) && e.Key == Key.V || Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) && e.Key == Key.Insert)
			{
				string text = Clipboard.GetText();

				if (string.IsNullOrEmpty(text) || text.IndexOfAny(Path.GetInvalidPathChars()) != -1)
					return;

				FilterEditBox.Text = Path.GetFileName(text);
				FilterEditBox.SelectAll();
			}
		}

		private void FilesListTab_SelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			switch (FilesListTab.SelectedIndex)
			{
				case 0:
					if (ActiveFilesListBox == AllFilesListBox)
						return;
					ActiveFilesListBox = AllFilesListBox;
					break;
				case 1:
					if (ActiveFilesListBox == BookmarkedFilesListBox)
						return;
					ActiveFilesListBox = BookmarkedFilesListBox;
					break;
				case 2:
					if (ActiveFilesListBox == RecentFilesListBox)
						return;
					ActiveFilesListBox = RecentFilesListBox;
					break;
			}
			FilesListTab.UpdateLayout();
			RefreshStatusBox();
			AsyncFocusSelectedItem();
		}

		private void FilterEditBox_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.Key == Key.Tab)
			{
				e.Handled = true;
				FocusListViewItem(ActiveFilesListBox, ActiveFilesListBox.SelectedIndex);
				return;
			}
		}
		private void FilterEditBox_PreviewKeyDown(object sender, KeyEventArgs e)
		{
			var listBox = ActiveFilesListBox;
			var getSelectedItem = new Func<FileItem>(() =>
			{
				if (listBox.SelectedItems.Count == 0)
					return (FileItem)listBox.Items[0];
				else
					return (FileItem)listBox.SelectedItems[listBox.SelectedItems.Count - 1];
			});

			var moveFunc = new Action<int>((int steps) =>
			{
				FileItem selectedItem = getSelectedItem();
				var index = listBox.Items.IndexOf(selectedItem);
				var newIndex = Math.Min(Math.Max(0, index + steps), listBox.Items.Count - 1);
				if (newIndex == index)
					return;
				selectedItem = (FileItem)listBox.Items[newIndex];
				if (selectedItem == null)
					return;
				listBox.SelectedItems.Clear();
				listBox.SelectedItems.Add(selectedItem);
				listBox.ScrollIntoView(selectedItem);
			});

			if (e.Key == Key.Down)
			{
				moveFunc(1);
				return;
			}
			if (e.Key == Key.Up)
			{
				moveFunc(-1);
				return;
			}
			if (e.Key == Key.Enter)
			{
				if (OpenSelectedFile())
					HidePanel();
				return;
			}
			if (e.Key == Key.PageDown)
			{
				e.Handled = true;
				var lastVisible = GetVisibleListViewElement(listBox, (int)listBox.ActualHeight - 22);
				if (lastVisible == null)
					return;
				var lastItem = listBox.ItemContainerGenerator.ItemFromContainer(lastVisible);
				FileItem selectedItem = getSelectedItem();
				if (lastItem != selectedItem)
				{
					listBox.SelectedItems.Clear();
					listBox.SelectedItems.Add(lastItem);
					listBox.ScrollIntoView(lastItem);
				}
				else
					moveFunc(((int)(listBox.ActualHeight / lastVisible.ActualHeight)) - 1);
				return;
			}
			if (e.Key == Key.PageUp)
			{
				e.Handled = true;
				var firstVisible = GetVisibleListViewElement(listBox, 10);
				if (firstVisible == null)
					return;
				var firstItem = listBox.ItemContainerGenerator.ItemFromContainer(firstVisible);
				FileItem selectedItem = getSelectedItem();
				if (firstItem != selectedItem)
				{
					listBox.SelectedItems.Clear();
					listBox.SelectedItems.Add(firstItem);
					listBox.ScrollIntoView(firstItem);
				}
				else
					moveFunc(1 - (int)(listBox.ActualHeight / firstVisible.ActualHeight));
			}
		}

		private void FilterEditBox_TextChanged(object sender, TextChangedEventArgs e)
		{
			RefreshListViews();
			//AllFilesListBox.SelectedIndex = 0;
		}

		private void FileListView_GotFocus(object sender, RoutedEventArgs e)
		{
			ActiveFilesListBox = (ListBox)e.Source;
		}

		private void FileListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			RefreshStatusBox();
		}

		private void FileListView_KeyDown(object sender, KeyEventArgs e)
		{
			if ((Keyboard.Modifiers & ModifierKeys.Control) != 0)
			{
				if (e.Key == Key.I)
				{
					if (!CreateIncludePath())
					{
						return;
					}
					
					HidePanel();

					if ((Keyboard.Modifiers & ModifierKeys.Shift) == 0)
					{
						return;
					}

					// Let's add directly in to active document
					var activeDoc = UnrealVSPackage.Instance.DTE.ActiveDocument;
					var doc = (TextDocument)(activeDoc.Object("TextDocument"));
					if (doc == null)
						return;

					bool addSpace = false;
					var includePath = GetIncludePath((FileItem)(ActiveFilesListBox.SelectedItems[0]));
					string pasteString = $"#include \"{includePath}\"";

					bool skipFirstInclude = activeDoc.FullName.EndsWith(".cpp");
					int addBeforeLine = -1;
					var lastIncludeLine = -1;

					bool isInComment = false;
					var p = doc.StartPoint.CreateEditPoint();
					var lastLine = doc.EndPoint.Line;
					for (int lineIndex = 1; lineIndex < lastLine; ++lineIndex)
					{
						var str = p.GetLines(lineIndex, lineIndex + 1).Trim();
						if (str.Length == 0)
						{
							continue;
						}

						if (isInComment)
						{
							if (str.IndexOf("*/") != -1)
								isInComment = false;
							continue;
						}

						if (str.StartsWith("//"))
						{
							continue;
						}

						if (str.StartsWith("/*"))
						{
							isInComment = true;
							continue;
						}

						if (str.StartsWith("#include"))
						{
							if (str == pasteString)
							{
								doc.Selection.MoveTo(lineIndex, 0);
								doc.Selection.SelectLine();
								p.TryToShow();
								return;
							}

							bool isFirst = lastIncludeLine == -1;
							int prevLastIncludeLine = lastIncludeLine;
							lastIncludeLine = lineIndex;
							if (skipFirstInclude && isFirst)
							{
								continue;
							}

							var span = str.AsSpan(9);
							var firstQuote = span.IndexOf('"');
							if (firstQuote != -1)
							{
								span = span.Slice(firstQuote + 1);
								var secondQuote = span.IndexOf('"');
								if (secondQuote != -1)
								{
									span = span.Slice(0, secondQuote).Trim();
									bool isGeneratedInclude = span.IndexOf(".generated.".AsSpan()) != -1;
									if (includePath.AsSpan().CompareTo(span, StringComparison.Ordinal) < 0 || isGeneratedInclude)
									{
										if (isGeneratedInclude && prevLastIncludeLine != -1)
											lastIncludeLine = prevLastIncludeLine;
										else
											addBeforeLine = lineIndex;
										break;
									}
								}
							}
							else if (span.IndexOf("UE_INLINE_GENERATED_CPP".AsSpan()) != -1)
							{
								lastIncludeLine = prevLastIncludeLine;
								break;
							}
							continue;
						}
							
						if (str.StartsWith("#pragma once"))
						{
							lastIncludeLine = lineIndex + 1;
							continue;
						}

						lastIncludeLine = lineIndex - 1;
						addSpace = true;
						break;  // Unknown code
					}

					if (addBeforeLine == -1 && lastIncludeLine != -1)
					{
						addBeforeLine = lastIncludeLine + 1;
					}

					if (addBeforeLine == -1)
					{
						addBeforeLine = lastLine;
					}

					p.LineDown(addBeforeLine - 1);
					p.Insert($"{pasteString}\r\n");
					if (addSpace)
					{
						p.Insert("\r\n");
					}
					doc.Selection.MoveTo(addBeforeLine, 0);
					doc.Selection.SelectLine();
					p.TryToShow();
				}
				return;
			}
			if (e.Key == Key.Left)
			{
				if (ActiveFilesListBox != AllFilesListBox)
					--FilesListTab.SelectedIndex;
				//if (ActiveFilesListBox == BookmarkedFilesListBox)
				//    ToggleListView(BookmarkedFilesListBox, AllFilesListBox);
				return;
			}

			if (e.Key == Key.Right)
			{
				if (ActiveFilesListBox != RecentFilesListBox)// && ((List<FileItem>)BookmarkedFilesListBox.ItemsSource).Count > 0)
					++FilesListTab.SelectedIndex;
				//if (ActiveFilesListBox == AllFilesListBox)// && ((List<FileItem>)BookmarkedFilesListBox.ItemsSource).Count > 0)
				//   FilesListTab.SelectedIndex = 1;
				//if (ActiveFilesListBox == AllFilesListBox && ((List<FileItem>)BookmarkedFilesListBox.ItemsSource).Count > 0)
				//     ToggleListView(AllFilesListBox, BookmarkedFilesListBox);
				return;
			}

			if (e.Key == Key.Tab)
			{
				e.Handled = true;
				FilterEditBox.Focus();
				return;
			}

			if (e.Key == Key.Insert)
			{
				if (ActiveFilesListBox != AllFilesListBox)
					return;
				bool BookmarksModified = false;
				foreach (var i in ActiveFilesListBox.SelectedItems)
				{
					var item = (FileItem)i;
					if (item == null || item.Name == "")
						continue;
					if (BookmarkedFileItems.Contains(item))
						continue;
					BookmarkedFileItems.Add(item);
					BookmarksModified = true;
				}
				if (!BookmarksModified)
					return;
				RefreshListView(BookmarkedFilesListBox, BookmarkedFileItems.ToArray());
				//if (AllFilesListBox.SelectedIndex < AllFilesListBox.Items.Count - 1)
				//    FocusListViewItem(AllFilesListBox, AllFilesListBox.SelectedIndex + 1);
				SaveSolutionSettings();
				return;
			}

			if (e.Key == Key.Delete)
			{
				if (ActiveFilesListBox != BookmarkedFilesListBox)
					return;
				bool BookmarksModified = false;

				var index = BookmarkedFilesListBox.SelectedIndex;
				foreach (var i in ActiveFilesListBox.SelectedItems)
				{
					var item = (FileItem)i;
					if (item == null || item.Name == "")
						continue;
					if (!BookmarkedFileItems.Remove(item))
						continue;
					BookmarksModified = true;
				}
				if (BookmarksModified)
				{
					RefreshListView(BookmarkedFilesListBox, BookmarkedFileItems.ToArray());
					if (index == BookmarkedFilesListBox.Items.Count)
					{
						if (index == 0)
							FilesListTab.SelectedIndex = 0;//ActiveFilesListBox = AllFilesListBox;
						else
							index -= 1;
					}
					BookmarkedFilesListBox.SelectedIndex = index;
					AsyncFocusSelectedItem();
					SaveSolutionSettings();
				}
				return;
			}

			if (e.Key == Key.Enter)
			{
				if (OpenSelectedFile())
					HidePanel();
				return;
			}

			bool resetSelection = false;

			if (e.Key == Key.Back)
			{
				string str = FilterEditBox.Text;
				if (string.IsNullOrEmpty(str))
					return;
				if (ClearFilter)
				{
					resetSelection = true;
					ClearFilter = false;
					FilterEditBox.Text = "";
				}
				else
					FilterEditBox.Text = str.Substring(0, str.Length - 1);
			}
			else
			{
				char c = GetCharFromKey(e.Key);
				if (c == 0)
					return;
				if (ClearFilter)
				{
					resetSelection = true;
					ClearFilter = false;
					FilterEditBox.Text = "";
				}
				FilterEditBox.Text += c;
				FilterEditBox.SelectAll();
			}

			if (resetSelection)
			{
				AllFilesListBox.SelectedIndex = 0;
				BookmarkedFilesListBox.SelectedIndex = 0;
				RecentFilesListBox.SelectedIndex = 0;
			}

			AsyncFocusSelectedItem();

			e.Handled = true;
		}

		private void ListView_MouseDoubleClick(object sender, MouseButtonEventArgs e)
		{
			if (OpenSelectedFile())
				HidePanel();
		}

		private bool OpenSelectedFile()
		{
			var toOpen = new List<string>();
			foreach (var i in ActiveFilesListBox.SelectedItems)
			{
				var item = (FileItem)i;
				if (item == null || item.Name == "")
					continue;
				toOpen.Add(item.File);
			}
			ThreadHelper.JoinableTaskFactory.Run(async delegate
			{
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				foreach (var f in toOpen)
					UnrealVSPackage.Instance.DTE.ExecuteCommand("File.OpenFile", "\"" + f + "\"");
			});
			return toOpen.Count != 0;
		}

		private string GetIncludePath(FileItem item)
		{
                if (item == null || item.Name == "")
				return null;
                string includePath = item.File;
                includePath = includePath.Replace("\\", "/");
                string includePathLwr = includePath.ToLower();
			var includeStartIndex = Math.Max(Math.Max(includePathLwr.LastIndexOf("/public/"), includePathLwr.LastIndexOf("/private/")), includePathLwr.LastIndexOf("/classes/"));
                if (includeStartIndex != -1)
                    includePath = includePath.Substring(includePath.IndexOf('/', includeStartIndex + 1) + 1);
			return includePath;
		}

		private bool CreateIncludePath()
		{
			string includeStrings = "";
			foreach (var i in ActiveFilesListBox.SelectedItems)
			{
				var includePath = GetIncludePath((FileItem)i);
				if (includePath != null)
                includeStrings += $"#include \"{includePath}\"\r\n";
			}
			if (includeStrings == "")
				return false;
			Clipboard.SetText(includeStrings);
			return true;
		}

		private void ToggleListView(ListBox from, ListBox to)
		{
			var firstVisibleFrom = GetVisibleListViewElement(from, 10);
			if (firstVisibleFrom == null)
				return;
			var firstVisibleFromIndex = from.ItemContainerGenerator.IndexFromContainer(firstVisibleFrom);
			var firstVisibleTo = GetVisibleListViewElement(to, 10);
			if (firstVisibleTo == null)
				return;
			var firstVisibleToIndex = to.ItemContainerGenerator.IndexFromContainer(firstVisibleTo);
			var toSelectIndex = firstVisibleToIndex + from.SelectedIndex - firstVisibleFromIndex;
			if (toSelectIndex >= to.Items.Count)
				toSelectIndex = to.Items.Count - 1;
			FocusListViewItem(to, toSelectIndex);
		}

		private void FocusListViewItem(ListBox listView, int itemIndex)
		{
			if (itemIndex == -1)
				itemIndex = 0;
			listView.SelectedIndex = itemIndex;
			if (listView.Items.Count > 0)
				(listView.ItemContainerGenerator.ContainerFromIndex(itemIndex) as ListBoxItem)?.Focus();
			else
				listView.Focus();
			RefreshStatusBox();
		}

		private void AsyncFocusSelectedItem()
		{
			double interval = 0.1;
			var timer = new DispatcherTimer(DispatcherPriority.Normal);
			timer.Tick += (s, e) =>
			{
				var items = (List<FileItem>)ActiveFilesListBox.ItemsSource;
				if (items.Count == 0)
				{
					ActiveFilesListBox.Focus();
					timer.Stop();
					return;
				}
				var item = (ActiveFilesListBox.ItemContainerGenerator.ContainerFromIndex(Math.Max(0, ActiveFilesListBox.SelectedIndex)) as ListBoxItem);
				if (item == null)
				{
					timer.Interval = TimeSpan.FromSeconds(interval);
					interval = Math.Min(interval + 0.1, 1.0);
					return;
				}
				item.Focus();
				timer.Stop();
			};
			timer.Start();
		}

		private void RefreshListViews()
		{
			RefreshListView(AllFilesListBox, AllFileItems);
			RefreshListView(BookmarkedFilesListBox, BookmarkedFileItems.ToArray());
			RefreshListView(RecentFilesListBox, RecentFileItems.ToArray());
		}

		struct Score
		{
			public int Sorting;
			public int Index;
		}

		private void RefreshListView(ListBox listView, FileItem[] source)
		{
			var fileItems = (List<FileItem>)listView.ItemsSource;
			fileItems.Clear();

			string filterStr = FilterEditBox.Text;
			var filter = filterStr.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);

			var selections = new HashSet<FileItem>();
			foreach (var i in listView.SelectedItems)
			{
				var item = (FileItem)i;
				if (item != null && item.Name != "")
					selections.Add(item);
			}

			var filterLen = filter.Length;
			if (filterLen > 0)
			{
				var filterSortedItems = new List<ValueTuple<Score, FileItem>>();
				var scoreArray = new ValueTuple<int, int>[5];
				int ItemIndex = 0;
				foreach (var item in source)
				{
					int scoreArrayIndex = 0;
					for (int i = 0; i != filterLen; ++i)
					{
						var f = filter[i];
						int index = item.Name.IndexOf(f, System.StringComparison.OrdinalIgnoreCase);
						if (index == -1)
						{
							scoreArrayIndex = 0;
							break;
						}
						scoreArray[scoreArrayIndex].Item1 = index;
						scoreArray[scoreArrayIndex].Item2 = scoreArrayIndex;
						++scoreArrayIndex;
						if (scoreArrayIndex == 5)
							break;
					}

					if (scoreArrayIndex == 0)
						continue;

					// We want to sort the ones that match the filter orders first and prioritize the ones that starts with the first filter entry
					int sortingScore = 0;
					Array.Sort(scoreArray, 0, scoreArrayIndex);
					for (int i = 0; i != scoreArrayIndex; ++i)
					{
						var Tuple = scoreArray[i];
						if (i == 0 && Tuple.Item1 == 0 && Tuple.Item2 == 0)
							sortingScore -= 100;
						sortingScore += (10 >> Tuple.Item2) * i + Tuple.Item1;
					}
					var s = new Score() { Sorting = sortingScore, Index = ItemIndex++ };
					filterSortedItems.Add(new ValueTuple<Score, FileItem>(s, item));
				}

				filterSortedItems.Sort((a, b) =>
				{
					if (a.Item1.Sorting != b.Item1.Sorting)
						return a.Item1.Sorting - b.Item1.Sorting;
					return a.Item1.Index - b.Item1.Index;
				});
				foreach (var tuple in filterSortedItems)
					fileItems.Add(tuple.Item2);
			}
			else
			{
				fileItems.AddRange(source);
			}

			int fileCount = fileItems.Count;

			// If empty we need to add one entry to make sure listview can be focused in windows
			if (fileItems.Count == 0)
				fileItems.Add(new FileItem() { Name = "" });

			CollectionViewSource.GetDefaultView(fileItems)?.Refresh();

			if (selections.Count != 0)
			{
				foreach (var item in fileItems)
				{
					if (!selections.Contains(item))
						continue;
					listView.SelectedItems.Add(item);
					listView.ScrollIntoView(item);
				}
			}

			if (listView.SelectedItems.Count == 0)
				listView.SelectedItems.Add(fileItems[0]);


			if (listView == AllFilesListBox)
				AllFilesTab.Header = $"All Files ({fileCount}/{source.Length})";
			else if (listView == BookmarkedFilesListBox)
				BookmarkedFilesTab.Header = $"Bookmarked Files ({fileCount}/{source.Length})";
			else
				RecentFilesTab.Header = $"Recent Files ({fileCount}/{source.Length})";
		}

		private void RefreshStatusBox()
		{
			var items = ActiveFilesListBox.SelectedItems;
			if (items.Count == 0)
				StatusText.Text = "";
			else if (items.Count == 1)
			{
				var item = (FileItem)items[0];
				StatusText.Text = item.File;
			}
			else
				StatusText.Text = "Multiple files selected";
		}

		private void FileListView_PreviewKeyDown(object sender, KeyEventArgs e)
		{
			if (e.Key != Key.Space)
				return;
			if (ClearFilter)
			{
				ClearFilter = false;
				return;
			}
			FilterEditBox.Text += ' ';
			e.Handled = true;
		}

		internal void HandleEscape()
		{
			if (HelpDialog.Visibility == Visibility.Visible)
				HelpDialog.Visibility = Visibility.Collapsed;
			else
				HidePanel();
		}

		internal void HandleF1()
		{
			HelpDialog.Visibility = Visibility.Visible;
		}

		internal void HandleF5()
		{
			AsyncRefreshFileList();
		}

		internal void HandleSolutionChanged()
		{
			SaveSolutionSettings();
			if (IsVisible)
				AsyncRefreshFileList();
			else
				AllFileItems = Array.Empty<FileItem>(); // Will trigger AsyncRefreshFileList when visible
		}

		internal void HandleDocumentActivated(Document Document)
		{
			var fileName = Document.FullName;
			FileItem fileItem;
			if (AllFileItemsLookup == null || !AllFileItemsLookup.TryGetValue(fileName, out fileItem))
				return;

			RecentFileItems.RemoveAll((item) => item == fileItem);
			RecentFileItems.Insert(0, fileItem);

			int RecentMaxCount = 100;
			if (RecentFileItems.Count > RecentMaxCount)
				RecentFileItems.RemoveRange(RecentMaxCount, RecentFileItems.Count - RecentMaxCount);

			RefreshListView(RecentFilesListBox, RecentFileItems.ToArray());
		}

		private void AsyncRefreshFileList()
		{
			if (IsRefreshing)
				return;
			IsRefreshing = true;
			RefreshingText.Text = "Refreshing File Lists (0)";
			RefreshingDialog.Visibility = Visibility.Visible;

			DispatcherTimer timer = new DispatcherTimer(DispatcherPriority.Render);
			timer.Interval = TimeSpan.FromSeconds(0.2);
			var traverser = new SolutionTraverser();
			timer.Tick += (s, e_) =>
			{
				var listItems = traverser.Update();
				if (listItems == null)
				{
					RefreshingText.Text = $"Refreshing File Lists ({traverser.HandledItemCount})";
					timer.Interval = TimeSpan.FromSeconds(0.01);
					return;
				}
				AllFileItems = listItems;

				var lookup = new Dictionary<string, FileItem>();
				foreach (var item in AllFileItems)
					try { lookup.Add(item.File, item); } catch (Exception) { }
				AllFileItemsLookup = lookup;

				LoadSolutionSettings();

				//ClearFilter = true;
				RefreshListViews();

				//AllFilesListBox.SelectedIndex = 0;
				//BookmarkedFilesListBox.SelectedIndex = 0;
				//RecentFilesListBox.SelectedIndex = 0;
				AsyncFocusSelectedItem();

				timer.Stop();
				RefreshingDialog.Visibility = Visibility.Collapsed;
				IsRefreshing = false;
			};
			timer.Start();
		}

		private void FileBrowserWindowControl_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
		{
			if (!(bool)e.NewValue)
				return;
			FilterEditBox.SelectAll();
			ClearFilter = true;
			//FilesListTab.SelectedIndex = 0;

			if (AllFileItems.Length == 0)
			{
				RefreshListViews(); // To make sure we get one invisible entry
				AsyncRefreshFileList();
			}

			AsyncFocusSelectedItem();
		}

		private void HidePanel()
		{
			ThreadHelper.ThrowIfNotOnUIThread();
			IVsWindowFrame ToolWindowFrame = (IVsWindowFrame)Window.Frame;
			ToolWindowFrame.Hide();
		}

		private class FileBrowserSettings
		{
			public List<string> Bookmarks { get; set; } = new List<string>();
			public List<string> Recents { get; set; } = new List<string>();
		}


		private void SaveSolutionSettings()
		{
			if (String.IsNullOrEmpty(UnrealVsFileName))
				return;
			using (var file = File.CreateText(UnrealVsFileName))
			{
				var settings = new FileBrowserSettings();
				foreach (var item in BookmarkedFileItems)
					settings.Bookmarks.Add(item.File);
				foreach (var item in RecentFileItems)
					settings.Recents.Add(item.File);
				string json = JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true });
				file.Write(json);
			}
		}

		private void LoadSolutionSettings()
		{
			var solutionFileName = UnrealVSPackage.Instance.DTE.Solution.FileName;
			if (String.IsNullOrEmpty(solutionFileName))
			{
				BookmarkedFileItems.Clear();
				RecentFileItems.Clear();
				UnrealVsFileName = null;
				return;
			}
			UnrealVsFileName = solutionFileName.Substring(0, solutionFileName.Length - 3) + "unrealvs";
			if (!File.Exists(UnrealVsFileName))
				return;
			string json = File.ReadAllText(UnrealVsFileName);
			if (String.IsNullOrEmpty(json))
				return;
			var settings = JsonSerializer.Deserialize<FileBrowserSettings>(json, new JsonSerializerOptions { PropertyNameCaseInsensitive = true });
			if (settings == null)
				return;


			BookmarkedFileItems.Clear();
			if (settings.Bookmarks.Count == 0 && settings.Recents.Count == 0)
				return;

			var fileItems = new[] { BookmarkedFileItems, RecentFileItems };
			var settingItems = new[] { settings.Bookmarks, settings.Recents };
			FileItem fileItem;

			for (int i = 0; i != 2; ++i)
			{
				var added = new HashSet<FileItem>();
				foreach (var item in settingItems[i])
					if (AllFileItemsLookup.TryGetValue(item, out fileItem))
						if (added.Add(fileItem))
							fileItems[i].Add(fileItem);
			}
		}

		private static ListBoxItem GetVisibleListViewElement(ListBox listView, int y)
		{
			HitTestResult hitTest = VisualTreeHelper.HitTest(listView, new Point(10, y));
			DependencyObject depObj = hitTest.VisualHit as DependencyObject;
			if (depObj == null)
				return null;
			DependencyObject current = depObj;
			while (current != null && current != listView)
			{
				ListBoxItem listBoxItem = current as ListBoxItem;
				if (listBoxItem != null)
					return listBoxItem;
				current = VisualTreeHelper.GetParent(current);
			}
			return null;
		}
		private static char GetCharFromKey(Key key)
		{
			char ch = '\0';

			int virtualKey = KeyInterop.VirtualKeyFromKey(key);
			byte[] keyboardState = new byte[256];
			NativeMethods.GetKeyboardState(keyboardState);

			uint scanCode = NativeMethods.MapVirtualKey((uint)virtualKey, NativeMethods.MapType.MAPVK_VK_TO_VSC);
			StringBuilder stringBuilder = new StringBuilder(2);

			int result = NativeMethods.ToUnicode((uint)virtualKey, scanCode, keyboardState, stringBuilder, stringBuilder.Capacity, 0);
			switch (result)
			{
				case -1:
					break;
				case 0:
					break;
				case 1:
				default:
					ch = stringBuilder[0];
					break;
			}
			return ch;
		}

		class StackItem
		{
			public ProjectItems Items;
			public int Index;
			public Project Project;
		}

		class SolutionTraverser
		{
			private SortedDictionary<string, FileItem> SortedFileItems = new SortedDictionary<string, FileItem>();
			private Dictionary<string, SortedDictionary<string, FileItem>> CollidingFileItems = new Dictionary<string, SortedDictionary<string, FileItem>>();
			private Dictionary<string, string> ModuleRoots = new Dictionary<string, string>();

			private Stack<StackItem> ItemStack = new Stack<StackItem>();
			private int ProjectIndex;
			public int HandledItemCount { get; private set; }

			public FileItem[] Update()
			{
				Stack<StackItem> itemStack = ItemStack;
				var projects = UnrealVSPackage.Instance.DTE.Solution.Projects;
				var projectCount = projects.Count;

				int traverseCounter = 0;

				while (ProjectIndex < projectCount)
				{
					var project = projects.Item(ProjectIndex + 1);

					StackItem stackItem;
					if (ItemStack.Count > 0)
						stackItem = ItemStack.Pop();
					else
						stackItem = new StackItem() { Items = project.ProjectItems, Project = project };

					while (true)
					{
						if (traverseCounter > 5000)
						{
							ItemStack.Push(stackItem);
							return null;
						}

						if (stackItem.Items == null || stackItem.Index == stackItem.Items.Count)
						{
							if (itemStack.Count == 0)
								break;
							stackItem = itemStack.Pop();
							++stackItem.Index;
							continue;
						}

						var projectItem = stackItem.Items.Item(stackItem.Index + 1);
						++traverseCounter;

						if (projectItem.FileCount != 0)
						{
							var file = projectItem.FileNames[1];
							if (file != null)
							{
								var name = projectItem.Name;
								if (name != file)
									if (!file.EndsWith("\\")) // Skip folders
									{
										FileItem fileItem = new FileItem() { Name = name, File = file, Project = stackItem.Project.Name };
										FileItem existingFileItem;
										if (SortedFileItems.TryGetValue(name, out existingFileItem))
										{
											if (existingFileItem.File != file)
											{
												SortedDictionary<string, FileItem> colList;
												if (!CollidingFileItems.TryGetValue(name, out colList))
												{
													colList = new SortedDictionary<string, FileItem>();
													colList.Add(existingFileItem.File, existingFileItem);
													CollidingFileItems[name] = colList;
												}
												if (!colList.ContainsKey(fileItem.File))
													colList.Add(fileItem.File, fileItem);
											}
										}
										else
											SortedFileItems.Add(name, fileItem);

										if (file.EndsWith(".build.cs", StringComparison.OrdinalIgnoreCase))
										{
											var lastBackslash = file.LastIndexOf('\\');
											var moduleDir = file.Substring(0, lastBackslash);
											var moduleName = file.Substring(lastBackslash + 1, file.Length - lastBackslash - 10);
											if (!ModuleRoots.ContainsKey(moduleDir))
												ModuleRoots.Add(moduleDir, moduleName);
											//else if (ModuleRoots[moduleDir] != moduleName)
											//    Logging.WriteLine(file);
										}

										++HandledItemCount;
									}
							}
						}

						if (projectItem.SubProject != null)
						{
							itemStack.Push(stackItem);
							stackItem = new StackItem() { Items = projectItem.SubProject.ProjectItems, Project = projectItem.SubProject };
							continue;
						}

						if (projectItem.ProjectItems != null)
						{
							itemStack.Push(stackItem);
							stackItem = new StackItem() { Items = projectItem.ProjectItems, Project = stackItem.Project };
							continue;
						}

						++stackItem.Index;
					}

					++ProjectIndex;
				}

				// We need to rename all collisions
				foreach (var kv in CollidingFileItems)
				{
					SortedFileItems.Remove(kv.Key);
					var usedParents = new Dictionary<string, int>();

					foreach (var itemKv in kv.Value)
					{
						var item = itemKv.Value;
						string parent = item.Project;
						var path = item.File;
						int lastBackslash = path.LastIndexOf('\\');
						while (lastBackslash != -1)
						{
							path = path.Substring(0, lastBackslash);
							string moduleName;
							if (ModuleRoots.TryGetValue(path, out moduleName))
							{
								parent = moduleName;
								break;
							}
							lastBackslash = path.LastIndexOf('\\');
						}

						string sortName = item.Name + $" ({parent})";

						int counter = 0;
						if (usedParents.TryGetValue(parent, out counter))
						{
							if (counter == 0)
							{
								var alreadyAddedItem = SortedFileItems[sortName];
								SortedFileItems.Remove(sortName);
								alreadyAddedItem.Name = item.Name + $" ({parent} 0)";
								SortedFileItems.Add(alreadyAddedItem.Name, alreadyAddedItem);
							}
							++counter;
							usedParents[parent] = counter;
							item.Name = item.Name + $" ({parent} {counter})";
							SortedFileItems.Add(item.Name, item);
						}
						else
						{
							usedParents.Add(parent, 0);
							item.Name = sortName;
							SortedFileItems.Add(item.Name, item);
						}
					}
				}

				var result = SortedFileItems.Values.ToArray();

				// Sort so header is before source file.. since most of the time you are opening header files.
				/*
                for (int i=0, e=result.Length; i!=e; ++i)
                {
                    var name = result[i].Name;
					if (name.EndsWith(".h") && i > 0)
                    {
						var prev = result[i - 1];
						var prevName = prev.Name;
                        if (name.Length + 2 == prevName.Length)
                        {
                            if (name.AsSpan(0, name.Length - 1).SequenceEqual(prevName.AsSpan(0, prevName.Length - 3))) // swap h and cpp
                            {
                                result[i - 1] = result[i];
                                result[i] = prev;
							}
                        }
                    }
                }
                */
				return result;
			}
		}
	}
}