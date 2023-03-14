using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using static System.Int32;

namespace CruncherSharp
{
    public partial class CruncherSharpForm : Form
    {
        public enum SearchType
        {
            None,
            UnusedVTables,
            MSVCExtraPadding,
            MSVCEmptyBaseClass,
            UnusedInterfaces,
            UnusedVirtual,
            MaskingFunction,
            RemovedInline,
        }

        private readonly List<string> _FunctionsToIgnore;
        private readonly Stack<SymbolInfo> _NavigationStack;
		private readonly Stack<SymbolInfo> _RedoNavigationStack;
		private readonly SymbolAnalyzer _SymbolAnalyzer;
        private readonly DataTable _Table;
		private readonly List<uint> _MemPools;
		public bool _CloseRequested = false;
        public bool _HasInstancesCount = false;
        public bool _HasSecondPDB = false;
		public bool _HasMemPools = false;
		public bool _IgnoreSelectionChange = false;
		public bool _RestrictToSymbolsImportedFromCSV = false;
		private ulong _PrefetchStartOffset = 0;
        private SearchType _SearchCategory = SearchType.None;

        private SymbolInfo _SelectedSymbol;

        public CruncherSharpForm(SymbolAnalyzer symbolAnalyzer)
        {
            InitializeComponent();

            _SymbolAnalyzer = symbolAnalyzer;
            _Table = CreateDataTable();
            _Table.CaseSensitive = checkBoxMatchCase.Checked;
            _NavigationStack = new Stack<SymbolInfo>();
			_RedoNavigationStack = new Stack<SymbolInfo>();
			_MemPools = new List<uint>();
			_FunctionsToIgnore = new List<string>();
            _SelectedSymbol = null;
            bindingSourceSymbols.DataSource = _Table;
            dataGridSymbols.DataSource = bindingSourceSymbols;

            dataGridSymbols.Columns[0].Width = 271;

            WindowState = FormWindowState.Maximized;
        }

        public SearchType SearchCategory
        {
            get => _SearchCategory;
            private set
            {
                _SearchCategory = value;
                UpdateCheckedOption();
            }
        }

        private void Reset()
        {
            checkedListBoxNamespaces.Items.Clear();
			_Table.Clear();
            dataGridViewSymbolInfo.Rows.Clear();
            dataGridViewFunctionsInfo.Rows.Clear();
            _SelectedSymbol = null;
			_RestrictToSymbolsImportedFromCSV = false;
			_MemPools.Clear();
			labelCurrentSymbol.Text = "";
            _SymbolAnalyzer.Reset();
			UpdateBtnLoadText();
		}

		private void UpdateBtnLoadText()
		{
			if (textBoxFilter.Text.Length > 0)
				btnLoad.Text = "Load filtered symbols";
			else
				btnLoad.Text = "Load all symbols";
		}

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void loadPDBToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (loadPDBBackgroundWorker.IsBusy || loadCSVBackgroundWorker.IsBusy)
                return;

            if (openPdbDialog.ShowDialog() != DialogResult.OK)
                return;

			Reset();

			_SymbolAnalyzer.FileName = openPdbDialog.FileName;
            Text = "Cruncher# - " + _SymbolAnalyzer.FileName;

            btnLoad.Enabled = true;
            btnReset.Enabled = true;
			loadInstanceCountToolStripMenuItem.Enabled = true;
			textBoxFilter.Focus();
        }

        public void LoadPdb(string fileName, bool secondPDB)
        {
            _SelectedSymbol = null;
            toolStripProgressBar.Value = 0;
            toolStripStatusLabel.Text = "Loading PDB...";
            if (!secondPDB)
            {
                Text = "Cruncher# - " + fileName;
                if (textBoxFilter.Text.Length > 0)
                    Text += " Filter: " + textBoxFilter.Text;
            }

            var task = new LoadPDBTask
            {
                FileName = fileName,
                SecondPDB = secondPDB,
                Filter = textBoxFilter.Text,
                MatchCase = checkBoxMatchCase.Checked,
                WholeExpression = checkBoxMatchWholeExpression.Checked,
                UseRegularExpression = checkBoxRegularExpressions.Checked,
                UseProgressBar = String.IsNullOrEmpty(textBoxFilter.Text) || !checkBoxMatchWholeExpression.Checked
			};
			loadPDBBackgroundWorker.RunWorkerAsync(task);
        }

        private static DataTable CreateDataTable()
        {
            var table = new DataTable("Symbols");
            table.Columns.Add(new DataColumn {ColumnName = "Symbol", ReadOnly = true});
            table.Columns.Add(new DataColumn
            {
                ColumnName = "Size",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt32")
            });
            table.Columns.Add(new DataColumn
            {
                ColumnName = "Padding",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt32")
            });
            table.Columns.Add(new DataColumn
            {
                ColumnName = "Padding zones",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt32")
            });
            table.Columns.Add(new DataColumn
            {
                ColumnName = "Total padding",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt32")
            });
            table.Columns.Add(new DataColumn
            {
                ColumnName = "Padding %",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt32")
            });

            return table;
        }

        private void AddInstancesCount()
        {
            if (_HasInstancesCount)
                return;
            _HasInstancesCount = true;
            if (_HasSecondPDB)
                _Table.Columns.Add(new DataColumn
                {
                    ColumnName = "Total delta",
                    ReadOnly = true,
                    DataType = Type.GetType("System.Int32")
                });
            _Table.Columns.Add(new DataColumn
            {
                ColumnName = "Instances",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt64")
            });
            _Table.Columns.Add(new DataColumn
            {
                ColumnName = "Total count",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt64")
            });
            _Table.Columns.Add(new DataColumn
            {
                ColumnName = "Total size",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt64")
            });
            _Table.Columns.Add(new DataColumn
            {
                ColumnName = "Total waste",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt64")
            });
        }

        private void AddSecondPDB()
        {
            if (_HasSecondPDB)
                return;
            _HasSecondPDB = true;
            _Table.Columns.Add(new DataColumn
            {
                ColumnName = "New size",
                ReadOnly = true,
                DataType = Type.GetType("System.UInt32")
            });
            _Table.Columns.Add(new DataColumn
            {
                ColumnName = "Delta",
                ReadOnly = true,
                DataType = Type.GetType("System.Int32")
            });
            if (_HasInstancesCount)
                _Table.Columns.Add(new DataColumn
                {
                    ColumnName = "Total delta",
                    ReadOnly = true,
                    DataType = Type.GetType("System.Int32")
                });
        }

        private void AddSymbolToTable(SymbolInfo symbolInfo)
        {
            var row = _Table.NewRow();

            row["Symbol"] = symbolInfo.Name;
            row["Size"] = symbolInfo.Size;
            row["Padding"] = symbolInfo.Padding;
            row["Padding zones"] = symbolInfo.PaddingZonesCount;
            row["Total padding"] = symbolInfo.TotalPadding.Value;
            row["Padding %"] = (uint) ((double) symbolInfo.TotalPadding.Value / symbolInfo.Size * 100);
            if (_HasInstancesCount)
            {
                row["Instances"] = symbolInfo.NumInstances;
                row["Total count"] = symbolInfo.TotalCount;
                row["Total size"] = symbolInfo.TotalCount * symbolInfo.Size;
                row["Total waste"] = symbolInfo.TotalCount * symbolInfo.Padding;
            }

            if (_HasSecondPDB)
            {
                row["New size"] = symbolInfo.NewSize;
                row["Delta"] = (long) symbolInfo.NewSize - (long) symbolInfo.Size;
                if (_HasInstancesCount && symbolInfo.NumInstances > 0)
                    row["Total delta"] = ((long) symbolInfo.NewSize - (long) symbolInfo.Size) *
                                         (long) symbolInfo.NumInstances;
            }
			if (_HasMemPools)
			{
				uint previousMemPool = 0;
				foreach (var memPool in _MemPools)
				{
					if (symbolInfo.Size > memPool)
					{
						previousMemPool = memPool;
						continue;
					}
					row["MemPool waste"] = memPool - symbolInfo.Size;
					row["MemPool total waste"] = (long)(memPool - symbolInfo.Size) * (long)symbolInfo.NumInstances;
					row["MemPool delta"] = symbolInfo.Size - previousMemPool;
					break;

				}
			}

            _Table.Rows.Add(row);
        }


        private ulong GetCacheLineSize()
        {
            return Convert.ToUInt32(textBoxCache.Text);
        }

        private SymbolInfo FindSelectedSymbolInfo()
        {
            if (dataGridSymbols.SelectedRows.Count == 0) return null;

            var selectedRow = dataGridSymbols.SelectedRows[0];
            var symbolName = selectedRow.Cells[0].Value.ToString();
            return _SymbolAnalyzer.FindSymbolInfo(symbolName);
        }

        private string GetFilterString()
        {
            var filters = new List<string>();
            if (textBoxFilter.Text.Length > 0)
                if (checkBoxMatchWholeExpression.Checked)
                    filters.Add($"Symbol = '{textBoxFilter.Text.Replace("'", string.Empty)}'");
                else if (checkBoxRegularExpressions.Checked)
                    filters.Add($"Symbol LIKE '{textBoxFilter.Text.Replace("'", string.Empty)}'");
                else
                    filters.Add($"Symbol LIKE '*{textBoxFilter.Text.Replace("'", string.Empty)}*'");
            if (!chkShowTemplates.Checked)
                filters.Add("Symbol NOT LIKE '*<*'");
            if (checkedListBoxNamespaces.CheckedItems.Count > 0)
            {
                var namespaceFilters = checkedListBoxNamespaces.CheckedItems.Cast<string>()
                    .Select(item => $"Symbol LIKE '*{item.Replace("'", string.Empty)}::*'");
                filters.Add($"({string.Join(" OR ", namespaceFilters)})");
            }

            return string.Join(" AND ", filters);
        }

        private void dataGridSymbols_SelectionChanged(object sender, EventArgs e)
        {
			if (_IgnoreSelectionChange)
				return;

            _PrefetchStartOffset = 0;
            if (dataGridSymbols.SelectedRows.Count > 1)
                ShowSelectionSummary();
            else
                ShowSelectedSymbolInfo();
        }

        private void dataGridSymbols_ParentSelected(object sender, EventArgs e)
        {
            dataGridSymbols.ClearSelection();
            if (TrySelectSymbol(sender.ToString()) == false)
            {
                var symbolInfo = _SymbolAnalyzer.FindSymbolInfo(sender.ToString());
				if (symbolInfo != null)
				{
					ShowSymbolInfo(symbolInfo);
				}
            }
        }

        private void dataGridSymbols_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            _PrefetchStartOffset = 0;
            ShowSelectedSymbolInfo();
        }

        private void ShowSelectionSummary()
        {
            dataGridViewSymbolInfo.Rows.Clear();

            ulong totalMemory = 0;
            ulong totalWaste = 0;
            long totalDiff = 0;
            foreach (DataGridViewRow selectedRow in dataGridSymbols.SelectedRows)
            {
                var symbolName = selectedRow.Cells[0].Value.ToString();
                var symbolInfo = _SymbolAnalyzer.FindSymbolInfo(symbolName);

                if (_HasInstancesCount)
                {
                    totalMemory += symbolInfo.NumInstances * symbolInfo.Size;
                    totalWaste += symbolInfo.NumInstances * symbolInfo.TotalPadding.Value;
                    if (_HasSecondPDB)
                        totalDiff += ((long) symbolInfo.NewSize - (long) symbolInfo.Size) *
                                     (long) symbolInfo.NumInstances;
                }
                else
                {
                    totalMemory += symbolInfo.Size;
                    totalWaste += symbolInfo.TotalPadding.Value;
                }
            }

            labelCurrentSymbol.Text = "Total memory: " + totalMemory + "    total waste: " + totalWaste;
            if (_HasInstancesCount && _HasSecondPDB)
                labelCurrentSymbol.Text += "    total diff: " + totalDiff;
        }

        private void ShowSelectedSymbolInfo()
        {
            dataGridViewSymbolInfo.Rows.Clear();
            var info = FindSelectedSymbolInfo();
            if (info != null)
			{
				ShowSymbolInfo(info);
			}
        }

        private bool TrySelectSymbol(string name, bool ShowSelection = true)
        {
            foreach (DataGridViewRow dr in dataGridSymbols.Rows)
            {
                var dc = dr.Cells[0]; // name
                if (dc.Value.ToString() != name)
                    continue;
				_IgnoreSelectionChange = !ShowSelection;
				dataGridSymbols.CurrentCell = dc;
				_IgnoreSelectionChange = false;
				return true;
            }

            return false;
        }

		private void ShowSymbolInfo(SymbolInfo info)
        {
			if (_SelectedSymbol != info)
			{
				labelCurrentSymbol.Text = info.Name;

				if (_SelectedSymbol != null)
				{
					_NavigationStack.Push(_SelectedSymbol);
				}
				_RedoNavigationStack.Clear();

				_SelectedSymbol = info;
			}

            UpdateSymbolGrid();
        }

		private void ShowPreviousSymbolInfo()
		{
			if (_NavigationStack.Count == 0)
				return;
			if (_SelectedSymbol != null)
			{
				_RedoNavigationStack.Push(_SelectedSymbol);
			}
			_SelectedSymbol = _NavigationStack.Pop();
			labelCurrentSymbol.Text = _SelectedSymbol.Name;
			UpdateSymbolGrid();

			TrySelectSymbol(labelCurrentSymbol.Text, false);
		}

		private void ShowNextSymbolInfo()
		{
			if (_RedoNavigationStack.Count == 0)
				return;
			if (_SelectedSymbol != null)
			{
				_NavigationStack.Push(_SelectedSymbol);
			}
			_SelectedSymbol = _RedoNavigationStack.Pop();
			labelCurrentSymbol.Text = _SelectedSymbol.Name;
			UpdateSymbolGrid();

			TrySelectSymbol(labelCurrentSymbol.Text, false);
		}

		private void UpdateSymbolGrid()
        {
            dataGridViewSymbolInfo.Rows.Clear();
            dataGridViewFunctionsInfo.Rows.Clear();

            var prevCacheBoundaryOffset = _PrefetchStartOffset;

			if (_SelectedSymbol != null)
			{
				if (prevCacheBoundaryOffset > _SelectedSymbol.Size) 
					prevCacheBoundaryOffset = _SelectedSymbol.Size;

				ulong numCacheLines = 0;
				var increment = 0;

				AddSymbolToGrid(_SelectedSymbol, ref prevCacheBoundaryOffset, ref numCacheLines, 0, increment);
			}
        }

        private void RefreshSymbolGrid(int rowIndex)
        {
            var firstRow = dataGridViewSymbolInfo.FirstDisplayedScrollingRowIndex;
            UpdateSymbolGrid();
			if (rowIndex < dataGridViewSymbolInfo.Rows.Count)
			{
				dataGridViewSymbolInfo.Rows[rowIndex].Selected = true;
				dataGridViewSymbolInfo.FirstDisplayedScrollingRowIndex = firstRow;
			}
        }

        private void AddSymbolToGrid(SymbolInfo symbol, ref ulong prevCacheBoundaryOffset, ref ulong numCacheLines,
            ulong previousOffset, int increment)
        {
            var cacheLineSize = GetCacheLineSize();

            var incrementText = "";
            var incrementTextEmpty = "";

            var whitespaceIncrementText = "";
            for (var i = 0; i < increment; i++)
            {
                whitespaceIncrementText += "   ";
                if (i == increment - 1)
                    incrementText += "   |__  ";
                else
                    incrementText += "   |    ";
                incrementTextEmpty += "   |    ";
            }

            foreach (var member in symbol.Members)
            {
                var currentOffset = previousOffset + member.Offset;
                if (member.PaddingBefore > 0 && checkBoxPadding.Checked)
                {
                    var paddingOffset = member.Offset - member.PaddingBefore;
                    string[] paddingRow =
                    {
                        string.Empty,
                        incrementTextEmpty,
                        "Padding",
                        paddingOffset.ToString(),
                        string.Empty,
                        member.PaddingBefore.ToString(),
                        string.Empty
                    };
                    dataGridViewSymbolInfo.Rows.Add(paddingRow);
                }

                if (checkBoxCacheLines.Checked)
                {
                    var cacheLines = new List<ulong>();
                    while (currentOffset >= cacheLineSize + prevCacheBoundaryOffset)
                    {
                        numCacheLines++;
                        var cacheLineOffset = numCacheLines * cacheLineSize + _PrefetchStartOffset;
                        cacheLines.Add(cacheLineOffset);
                        prevCacheBoundaryOffset = cacheLineOffset;
                    }

                    if (cacheLines.Count > 3 && chkSmartCacheLines.Checked)
                    {
                        string[] firstBoundaryRow =
                        {
                            string.Empty, incrementTextEmpty, "Cacheline boundary", cacheLines[0].ToString(),
                            string.Empty, string.Empty, string.Empty
                        };
                        dataGridViewSymbolInfo.Rows.Add(firstBoundaryRow);
                        string[] middleBoundariesRow =
                        {
                            string.Empty, incrementTextEmpty, "Cacheline boundaries (...)", string.Empty,
                            string.Empty, string.Empty, string.Empty
                        };
                        dataGridViewSymbolInfo.Rows.Add(middleBoundariesRow);
                        string[] lastBoundaryRow =
                        {
                            string.Empty, incrementTextEmpty, "Cacheline boundary",
                            cacheLines[cacheLines.Count - 1].ToString(), string.Empty, string.Empty, string.Empty
                        };
                        dataGridViewSymbolInfo.Rows.Add(lastBoundaryRow);
                    }
                    else
                    {
                        cacheLines.ForEach(offset =>
                        {
                            string[] boundaryRow =
                            {
                                string.Empty,
                                incrementTextEmpty,
                                "Cacheline boundary",
                                offset.ToString(),
                                string.Empty,
                                string.Empty
                            };
                            dataGridViewSymbolInfo.Rows.Add(boundaryRow);
                        });
                    }
                }

                var baseInfo = _SymbolAnalyzer.FindSymbolInfo(member.TypeName);
                var expand = "";
                if (member.Expanded)
                    expand = whitespaceIncrementText + "- ";
                else if (member.IsExapandable)
                    expand = whitespaceIncrementText + "+ ";

                object[] row =
                {
                    expand,
                    incrementText + member.DisplayName,
                    member.TypeName,
                    currentOffset.ToString(),
                    (member.BitField ? member.BitPosition.ToString() : string.Empty),
                    member.BitField ? (member.BitSize.ToString() + (member.BitSize == 1 ? " bit" : " bits")) : (member.Size.ToString() + (member.Size == 1 ? " byte" : " bytes")),
                    baseInfo?.TotalPadding.ToString() ?? string.Empty
                };
                dataGridViewSymbolInfo.Rows.Add(row);
                dataGridViewSymbolInfo.Rows[dataGridViewSymbolInfo.Rows.Count - 1].Tag = member;

                if (member.Expanded && baseInfo != null)
                    AddSymbolToGrid(baseInfo, ref prevCacheBoundaryOffset, ref numCacheLines, currentOffset,
                        increment + 1);

                if (member.BitField && member.BitPaddingAfter > 0 && checkBoxBitPadding.Checked)
                {
                    var paddingOffset = member.BitPaddingAfter.ToString() + " bits";
                    string[] paddingRow =
                    {
                        string.Empty,
                        incrementTextEmpty,
                        "Bitfield padding",
                        currentOffset.ToString(),
                        (member.BitPosition + member.BitSize).ToString(),
                        paddingOffset,
                        string.Empty
                    };
                    dataGridViewSymbolInfo.Rows.Add(paddingRow);
                }
            }

            // Final structure padding.
            if (symbol.EndPadding > 0 && checkBoxPadding.Checked)
            {
                var endPaddingOffset = symbol.Size - symbol.EndPadding;
                string[] paddingRow =
                {
                    string.Empty, string.Empty, "Padding", endPaddingOffset.ToString(), symbol.EndPadding.ToString(),
                    symbol.EndPadding.ToString()
                };
                dataGridViewSymbolInfo.Rows.Add(paddingRow);
            }

            foreach (var function in symbol.Functions)
            {
                object[] row =
                {
                    function.DisplayName, function.Virtual, function.IsPure, function.IsOverride, function.IsOverloaded,
                    function.IsMasking
                };
                dataGridViewFunctionsInfo.Rows.Add(row);
                dataGridViewFunctionsInfo.Rows[dataGridViewFunctionsInfo.Rows.Count - 1].Tag = function;
            }
        }

        private void dataGridSymbols_SortCompare(object sender, DataGridViewSortCompareEventArgs e)
        {
            if (e.Column.Index > 0)
            {
                e.Handled = true;
                TryParse(e.CellValue1.ToString(), out var val1);
                TryParse(e.CellValue2.ToString(), out var val2);
                e.SortResult = val1 - val2;
            }
        }

        private void dataGridViewSymbolInfo_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
        {
            var row = dataGridViewSymbolInfo.Rows[e.RowIndex];
            var nameCell = row.Cells[2];

            if (row.Tag is SymbolMemberInfo info)
            {
                if (info.Category == SymbolMemberInfo.MemberCategory.VTable)
                    e.CellStyle.BackColor = Color.LemonChiffon;
                else if (info.IsBase)
                    e.CellStyle.BackColor = Color.LightGreen;
				else if (info.BitField)
                {
                    if (e.ColumnIndex == 3 ) // Offset column
                    {
                        e.CellStyle.BackColor = Color.FromArgb((int)((info.Offset * 37) % 256), 192, 192); // random color not too dark
                        row.Cells[e.ColumnIndex].ToolTipText = "Bitfield";

                    }
                }
                else if (info.AlignWithPrevious)
				{
					if (e.ColumnIndex == 3) // Offset column
					{
						e.CellStyle.BackColor = Color.Salmon;
						row.Cells[e.ColumnIndex].ToolTipText = "Aligned with previous";
					}
				}
				else if (checkBoxCacheLines.Checked && checkBoxShowOverlap.Checked)
				{
					var cacheLineSize = GetCacheLineSize();
					var cacheOffset = (info.Offset - _PrefetchStartOffset) % cacheLineSize;
					if (cacheOffset + info.Size > cacheLineSize)
					{
						e.CellStyle.BackColor = Color.LightYellow;
					}
				}

				if (info.Volatile)
				{
					if (e.ColumnIndex == 0) // Name (Field) column
					{
						e.CellStyle.BackColor = Color.LightBlue;
						row.Cells[e.ColumnIndex].ToolTipText = "Volatile";
					}
				}
            }
            else if (nameCell.Value.ToString() == "Padding")
            {
                e.CellStyle.BackColor = Color.LightPink;
                if (e.ColumnIndex == 2)
                    e.CellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;
            }
            else if (nameCell.Value.ToString() == "Bitfield padding")
            {
                e.CellStyle.BackColor = Color.PaleVioletRed;
                if (e.ColumnIndex == 2)
                    e.CellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;
            }
            else if (nameCell.Value.ToString().StartsWith("Cacheline"))
            {
                e.CellStyle.BackColor = Color.LightGray;
                if (e.ColumnIndex == 2)
                    e.CellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;
            }
        }

        private void textBoxFilter_TextChanged(object sender, EventArgs e)
        {
            try
            {
                bindingSourceSymbols.Filter = GetFilterString();
                textBoxFilter.BackColor = Color.Empty;
                textBoxFilter.ForeColor = Color.Empty;
				UpdateBtnLoadText();
			}
            catch (EvaluateException)
            {
                textBoxFilter.BackColor = Color.Red;
                textBoxFilter.ForeColor = Color.White;
            }
        }

        private void textBoxFilter_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Return && !loadPDBBackgroundWorker.IsBusy && !loadCSVBackgroundWorker.IsBusy)
            {
                LoadPdb(_SymbolAnalyzer.FileName, false);
                btnLoad.Text = "Cancel";
            }
        }

        private void copyTypeLayoutToClipboardToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var info = FindSelectedSymbolInfo();
            if (info != null) Clipboard.SetText(info.ToString());
        }

        private void textBoxCache_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char) Keys.Enter || e.KeyChar == (char) Keys.Escape) 
				ShowSelectedSymbolInfo();
            OnKeyPress(e);
        }

        private void textBoxCache_Leave(object sender, EventArgs e)
        {
            ShowSelectedSymbolInfo();
        }

        private void setPrefetchStartOffsetToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (dataGridViewSymbolInfo.SelectedRows.Count != 0)
            {
                var selectedRow = dataGridViewSymbolInfo.SelectedRows[0];
                ulong symbolOffset = Convert.ToUInt32(selectedRow.Cells[3].Value);
                _PrefetchStartOffset = symbolOffset % GetCacheLineSize();
                RefreshSymbolGrid(selectedRow.Index);
            }
        }


        private void checkBoxCacheLines_CheckedChanged(object sender, EventArgs e)
        {
            if (dataGridViewSymbolInfo.SelectedRows.Count != 0)
            {
                var selectedRow = dataGridViewSymbolInfo.SelectedRows[0];
                RefreshSymbolGrid(selectedRow.Index);
            }
            else
            {
                RefreshSymbolGrid(0);
            }
        }

        private void dataGridViewSymbolInfo_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                var currentMouseOverRow = dataGridViewSymbolInfo.HitTest(e.X, e.Y).RowIndex;
                if (currentMouseOverRow >= 0)
                {
                    dataGridViewSymbolInfo.Rows[currentMouseOverRow].Selected = true;
                    contextMenuStripMembers.Show(dataGridViewSymbolInfo, new Point(e.X, e.Y));
                }
            }
			else if (e.Button == MouseButtons.XButton1)
			{
				ShowPreviousSymbolInfo();
			}
			else if (e.Button == MouseButtons.XButton2)
			{
				ShowNextSymbolInfo();
			}
		}

        private void dataGridViewSymbolInfo_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            var selectedRow = dataGridViewSymbolInfo.Rows[e.RowIndex];
            var currentSymbolInfo = selectedRow.Tag as SymbolMemberInfo;
            if (currentSymbolInfo != null && currentSymbolInfo.Category != SymbolMemberInfo.MemberCategory.Member)
            {
                var typeName = currentSymbolInfo.TypeName;
                if (typeName.Contains("[")) typeName = typeName.Substring(0, typeName.IndexOf("["));

                if (currentSymbolInfo.Category == SymbolMemberInfo.MemberCategory.Pointer)
                {
                    if (typeName.Contains("*")) typeName = typeName.Substring(0, typeName.IndexOf("*"));
                    if (typeName.Contains("&")) typeName = typeName.Substring(0, typeName.IndexOf("&"));
                }

				Cursor.Current = Cursors.WaitCursor;
				var jumpToSymbolInfo = _SymbolAnalyzer.FindSymbolInfo(typeName, true);
                if (jumpToSymbolInfo != null)
				{
                    if (e.ColumnIndex == 0)
                    {
                        if (currentSymbolInfo.IsExapandable)
                        {
                            currentSymbolInfo.Expanded = !currentSymbolInfo.Expanded;
                            RefreshSymbolGrid(e.RowIndex);
                        }
                    }
                    else
                    {
                        ShowSymbolInfo(jumpToSymbolInfo);
						TrySelectSymbol(typeName, false);
					}				
				}
				Cursor.Current = Cursors.Default;
			}
        }

        private void chkShowTemplates_CheckedChanged(object sender, EventArgs e)
        {
            bindingSourceSymbols.Filter = GetFilterString();
        }

        private void chkSmartCacheLines_CheckedChanged(object sender, EventArgs e)
        {
            ShowSelectedSymbolInfo();
        }

        private void checkedListBoxNamespaces_ItemCheck(object sender, ItemCheckEventArgs e)
        {
            this.BeginInvoke(new Action(() =>
            {
                bindingSourceSymbols.Filter = GetFilterString();
            }));
        }

        private void dataGridViewSymbolInfo_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Back)
			{
				ShowPreviousSymbolInfo();
			}
			if (e.KeyCode == Keys.Next)
			{
				ShowNextSymbolInfo();
			}

			OnKeyDown(e);
        }

        private void loadPDBBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            toolStripProgressBar.Value = e.ProgressPercentage;
            toolStripStatusLabel.Text = e.UserState as String;
        }

        private void loadPDBBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
			UpdateBtnLoadText();

			if (e.Cancelled)
            {
                if (_CloseRequested)
                {
                    Close();
                }
                else
                {
                    toolStripStatusLabel.Text = "";
                    toolStripProgressBar.Value = 0;
                }

                return;
            }

            var loadPdbSuccess = (bool) e.Result;

            if (!loadPdbSuccess)
            {
                MessageBox.Show(this, _SymbolAnalyzer.LastError);
                toolStripStatusLabel.Text = "Failed to load PDB.";
                return;
            }

            OnPDBLoaded();
        }

        private void loadPDBBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            e.Result = _SymbolAnalyzer.LoadPdb(sender, e);
        }


		private void loadCSVBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
		{
			toolStripProgressBar.Value = e.ProgressPercentage;
			toolStripStatusLabel.Text = e.UserState as String;
		}

		private void loadCSVBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
		{
			UpdateBtnLoadText();

			if (e.Cancelled)
			{
				if (_CloseRequested)
				{
					Close();
				}
				else
				{
					toolStripStatusLabel.Text = "";
					toolStripProgressBar.Value = 0;
				}

				return;
			}

			var loadCSVSuccess = (bool)e.Result;

			if (!loadCSVSuccess)
			{
				MessageBox.Show(this, _SymbolAnalyzer.LastError);
				toolStripStatusLabel.Text = "Failed to load CSV.";
				return;
			}

			foreach (var symbol in _SymbolAnalyzer.Symbols.Values)
				if (symbol.NumInstances > 0)
					symbol.UpdateTotalCount(_SymbolAnalyzer, symbol.NumInstances);

			AddInstancesCount();
			OnPDBLoaded();
		}

		private void loadCSVBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
		{
			e.Result = _SymbolAnalyzer.LoadCSV(sender, e);
		}


		private void OnPDBLoaded()
        {
            compareWithPDBToolStripMenuItem.Enabled = true;
            exportCsvToolStripMenuItem.Enabled = true;
            findUnusedVtablesToolStripMenuItem.Enabled = true;
            findMSVCExtraPaddingToolStripMenuItem.Enabled = true;
            findMSVCEmptyBaseClassToolStripMenuItem.Enabled = true;
            findUnusedInterfacesToolStripMenuItem.Enabled = true;
            findUnusedVirtualToolStripMenuItem.Enabled = true;
            findMaskingFunctionsToolStripMenuItem.Enabled = true;
            findRemovedInlineToolStripMenuItem.Enabled = true;

            // Temporarily clear the filter so, if current filter is invalid, we don't generate a ton of exceptions while populating the table
            var preExistingFilter = textBoxFilter.Text;
            textBoxFilter.Text = string.Empty;

            PopulateDataTable();

            checkedListBoxNamespaces.Items.Clear();
            foreach (var name in _SymbolAnalyzer.RootNamespaces) checkedListBoxNamespaces.Items.Add(name);

            // Sort by name by default (ascending)
            dataGridSymbols.Sort(dataGridSymbols.Columns[0], ListSortDirection.Ascending);
            bindingSourceSymbols.Filter = null; // "Symbol LIKE '*rde*'";

            // Restore the filter now that the table is populated
            textBoxFilter.Text = preExistingFilter;
            bindingSourceSymbols.Filter = GetFilterString();

            ShowSelectedSymbolInfo();

            _NavigationStack.Clear();
        }

        private void loadInstanceCountToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (loadPDBBackgroundWorker.IsBusy || loadCSVBackgroundWorker.IsBusy) 
				return;
            if (openCsvDialog.ShowDialog() == DialogResult.OK) 
				LoadCsv(openPdbDialog.FileName);
        }

        private void LoadCsv(string fileName)
        {
            Cursor.Current = Cursors.WaitCursor;

            if (_HasInstancesCount)
                foreach (var symbol in _SymbolAnalyzer.Symbols.Values)
                {
                    symbol.NumInstances = 0;
                    symbol.TotalCount = 0;
                }

            using (var sourceStream =
                File.Open(openCsvDialog.FileName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            {
				List<LoadCSVTask> tasks = new List<LoadCSVTask>();
				using (var reader = new StreamReader(sourceStream))
				{
;					while (!reader.EndOfStream)
					{
						var line = reader.ReadLine();
						var values = line.Split(',');
						var count = ulong.Parse(values[1]);
						if (count > 0)
						{
							var task = new LoadCSVTask
							{
								ClassName = values[0],
								Count = count
							};

							tasks.Add(task);
						}
	
					}
					loadCSVBackgroundWorker.RunWorkerAsync(tasks);
					btnLoad.Text = "Cancel";
				}
            }

        }

        private void compareWithPDBToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (loadPDBBackgroundWorker.IsBusy || loadCSVBackgroundWorker.IsBusy)
				return;
            if (openPdbDialog.ShowDialog() == DialogResult.OK)
            {
                AddSecondPDB();
                LoadPdb(openPdbDialog.FileName, true);
            }
        }

        private void exportCsvToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (loadPDBBackgroundWorker.IsBusy || loadCSVBackgroundWorker.IsBusy) 
				return;
            if (saveCsvDialog.ShowDialog() == DialogResult.OK)
                using (var writer = new StreamWriter(saveCsvDialog.FileName))
                {
                    writer.WriteLine(
                        "Name,Size,Padding,Padding zones,Total padding,Num Instances,Total size,Total waste,");

                    foreach (var symbolInfo in _SymbolAnalyzer.Symbols.Values)
                        writer.WriteLine("{0},{1},{2},{3},{4},{5},{6},{7},", symbolInfo.Name, symbolInfo.Size,
                            symbolInfo.Padding, symbolInfo.PaddingZonesCount, symbolInfo.TotalPadding.Value,
                            symbolInfo.NumInstances, symbolInfo.NumInstances * symbolInfo.Size,
                            symbolInfo.NumInstances * symbolInfo.Padding);
                }
        }

        private void CruncherSharpForm_FormClosing(object sender, FormClosingEventArgs e)
        {
			if (loadCSVBackgroundWorker.IsBusy)
			{
				e.Cancel = true; // Can't close form while background worker is busy!
				loadCSVBackgroundWorker.CancelAsync();
				_CloseRequested = true;
			}
			if (loadPDBBackgroundWorker.IsBusy)
            {
                e.Cancel = true; // Can't close form while background worker is busy!
				loadPDBBackgroundWorker.CancelAsync();
                _CloseRequested = true;
            }

        }

        private void findUnusedVtablesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.UnusedVTables;

            PopulateDataTable();
        }

        private void Reset_Click(object sender, EventArgs e)
        {
            chkShowTemplates.Checked = false;
            chkSmartCacheLines.Checked = true;
            for (var i = 0; i < checkedListBoxNamespaces.Items.Count; i++)
                checkedListBoxNamespaces.SetItemChecked(i, false);
            textBoxFilter.Clear();
            SearchCategory = SearchType.None;
            PopulateDataTable();
        }

        private void findMSVCExtraPaddingToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.MSVCExtraPadding;

            PopulateDataTable();
        }

        private void findMSVCEmptyBaseClassToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.MSVCEmptyBaseClass;

            PopulateDataTable();
        }

        private void findUnusedInterfacesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.UnusedInterfaces;

            PopulateDataTable();
        }

        private void findUnusedVirtualToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.UnusedVirtual;

            PopulateDataTable();
        }

        private void dataGridViewFunctionsInfo_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
        {
            var row = dataGridViewFunctionsInfo.Rows[e.RowIndex];
            var info = row.Tag as SymbolFunctionInfo;
            if (_FunctionsToIgnore.Contains(info.Name))
                e.CellStyle.BackColor = Color.LightGray;
            else
                switch (info.Category)
                {
                    case SymbolFunctionInfo.FunctionCategory.Constructor:
                        e.CellStyle.BackColor = Color.LightBlue;
                        break;
                    case SymbolFunctionInfo.FunctionCategory.Destructor:
                        e.CellStyle.BackColor = Color.LightGreen;
                        break;
                    case SymbolFunctionInfo.FunctionCategory.StaticFunction:
                        e.CellStyle.BackColor = Color.LightPink;
                        break;
                    default:
                        switch (SearchCategory)
                        {
                            case SearchType.UnusedVirtual:
                                if (info.UnusedVirtual) e.CellStyle.BackColor = Color.IndianRed;

                                break;
                            case SearchType.MaskingFunction:
                                if (info.IsMasking) e.CellStyle.BackColor = Color.IndianRed;

                                break;
                            case SearchType.RemovedInline:
                                if (info.WasInlineRemoved) e.CellStyle.BackColor = Color.IndianRed;

                                break;
                        }

                        break;
                }
        }

        private void findMaskingFunctionsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.MaskingFunction;

            PopulateDataTable();
        }

        private void ignoreFunctionToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (dataGridViewFunctionsInfo.SelectedRows.Count != 0)
            {
                var selectedRow = dataGridViewFunctionsInfo.SelectedRows[0];
                var info = selectedRow.Tag as SymbolFunctionInfo;
                if (!_FunctionsToIgnore.Contains(info.Name))
                {
                    _FunctionsToIgnore.Add(info.Name);
                    PopulateDataTable();
                }
            }
        }

        private void dataGridViewFunctionsInfo_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                var currentMouseOverRow = dataGridViewFunctionsInfo.HitTest(e.X, e.Y).RowIndex;
                if (currentMouseOverRow >= 0)
                {
                    dataGridViewFunctionsInfo.Rows[currentMouseOverRow].Selected = true;
                    contextMenuStripFunctions.Show(dataGridViewFunctionsInfo, new Point(e.X, e.Y));
                }
            }
			else if (e.Button == MouseButtons.XButton1)
			{
				ShowPreviousSymbolInfo();
			}
			else if (e.Button == MouseButtons.XButton2)
			{
				ShowNextSymbolInfo();
			}
		}

        private void dataGridSymbols_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                var currentMouseOverRow = dataGridSymbols.HitTest(e.X, e.Y).RowIndex;
                if (currentMouseOverRow >= 0)
                {
                    foreach (DataGridViewRow selectedRow in dataGridSymbols.SelectedRows)
                        selectedRow.Selected = false;
                    dataGridSymbols.Rows[currentMouseOverRow].Selected = true;

                    toolStripMenuItemParentClasses.DropDownItems.Clear();
                    var info = FindSelectedSymbolInfo();
                    if (info != null)
                    {
                        if (info.DerivedClasses != null)
                        {
                            toolStripMenuItemParentClasses.Enabled = true;
                            foreach (var parent in info.DerivedClasses)
                                toolStripMenuItemParentClasses.DropDownItems.Add(parent.Name, null,
                                    dataGridSymbols_ParentSelected);
                        }
                        else
                        {
                            toolStripMenuItemParentClasses.Enabled = false;
                        }

                        ShowSymbolInfo(info);
                    }

                    contextMenuStripClassInfo.Show(dataGridSymbols, new Point(e.X, e.Y));
                }
            }
			else if (e.Button == MouseButtons.XButton1)
			{
				ShowPreviousSymbolInfo();
			}
			else if (e.Button == MouseButtons.XButton2)
			{
				ShowNextSymbolInfo();
			}
		}

        private void findRemovedInlineToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SearchCategory = SearchType.RemovedInline;

            PopulateDataTable();
        }

        private void UpdateCheckedOption()
        {
            findUnusedVtablesToolStripMenuItem.Checked = SearchCategory == SearchType.UnusedVTables;
            findUnusedVirtualToolStripMenuItem.Checked = SearchCategory == SearchType.UnusedVirtual;
            findMSVCExtraPaddingToolStripMenuItem.Checked = SearchCategory == SearchType.MSVCExtraPadding;
            findMSVCEmptyBaseClassToolStripMenuItem.Checked = SearchCategory == SearchType.MSVCEmptyBaseClass;
            findUnusedInterfacesToolStripMenuItem.Checked = SearchCategory == SearchType.UnusedInterfaces;
            findMaskingFunctionsToolStripMenuItem.Checked = SearchCategory == SearchType.MaskingFunction;
            findRemovedInlineToolStripMenuItem.Checked = SearchCategory == SearchType.RemovedInline;
        }

        private void PopulateDataTable()
        {
            _Table.Rows.Clear();

            _Table.BeginLoadData();

            foreach (var symbolInfo in _SymbolAnalyzer.Symbols.Values)
			{
				if (restrictToSymbolsImportedFroCSVToolStripMenuItem.Checked && ! symbolInfo.IsImportedFromCSV)
				{
					continue;
				}

				switch (SearchCategory)
                {
                    case SearchType.None:
                        AddSymbolToTable(symbolInfo);
                        break;
                    case SearchType.UnusedVTables:
                        if (symbolInfo.HasVtable && !symbolInfo.HasBaseClass && symbolInfo.DerivedClasses == null)
                            AddSymbolToTable(symbolInfo);
                        break;
                    case SearchType.MSVCExtraPadding:
                        if (symbolInfo.HasMSVCExtraPadding) 
							AddSymbolToTable(symbolInfo);
                        break;
                    case SearchType.MSVCEmptyBaseClass:
                        if (symbolInfo.HasMSVCEmptyBaseClass) 
							AddSymbolToTable(symbolInfo);
                        break;
                    case SearchType.UnusedInterfaces:
                        if (symbolInfo.IsAbstract && symbolInfo.DerivedClasses == null) 
							AddSymbolToTable(symbolInfo);
                        break;
                    case SearchType.UnusedVirtual:
                        foreach (var function in symbolInfo.Functions)
                            if (function.UnusedVirtual)
                                if (!_FunctionsToIgnore.Contains(function.Name))
                                {
                                    AddSymbolToTable(symbolInfo);
                                    break;
                                }

                        break;
                    case SearchType.MaskingFunction:
                        foreach (var function in symbolInfo.Functions)
                            if (function.IsMasking)
                                if (!_FunctionsToIgnore.Contains(function.Name))
                                {
                                    AddSymbolToTable(symbolInfo);
                                    break;
                                }

                        break;
                    case SearchType.RemovedInline:
                        foreach (var function in symbolInfo.Functions)
                            if (function.WasInlineRemoved)
                                if (!_FunctionsToIgnore.Contains(function.Name))
                                {
                                    AddSymbolToTable(symbolInfo);
                                    break;
                                }

                        break;
                }
			}
			_Table.EndLoadData();
        }

        private void btnLoad_Click(object sender, EventArgs e)
        {
			if (loadCSVBackgroundWorker.IsBusy)
			{
				loadCSVBackgroundWorker.CancelAsync();
				toolStripStatusLabel.Text = "Canceling...";
			}
			else if (loadPDBBackgroundWorker.IsBusy)
            {
				loadPDBBackgroundWorker.CancelAsync();
                toolStripStatusLabel.Text = "Canceling...";
            }
            else
            {
                LoadPdb(_SymbolAnalyzer.FileName, false);
                btnLoad.Text = "Cancel";
            }
        }

        private void checkBoxMatchCase_CheckedChanged(object sender, EventArgs e)
        {
            _Table.CaseSensitive = checkBoxMatchCase.Checked;
            bindingSourceSymbols.Filter = GetFilterString();
        }

        private void checkBoxMatchWholeExpression_CheckedChanged(object sender, EventArgs e)
        {
            bindingSourceSymbols.Filter = GetFilterString();
        }

        private void checkBoxRegularExpressions_CheckedChanged(object sender, EventArgs e)
        {
            if (checkBoxRegularExpressions.Checked)
            {
                checkBoxMatchWholeExpression.Checked = false;
                checkBoxMatchWholeExpression.Enabled = false;
            }
            else
            {
                checkBoxMatchWholeExpression.Enabled = true;
            }

            bindingSourceSymbols.Filter = GetFilterString();
        }

        private void checkBoxPadding_CheckedChanged(object sender, EventArgs e)
        {
            if (dataGridViewSymbolInfo.SelectedRows.Count != 0)
            {
                var selectedRow = dataGridViewSymbolInfo.SelectedRows[0];
                RefreshSymbolGrid(selectedRow.Index);
            }
            else
            {
                RefreshSymbolGrid(0);
            }
        }

        private void checkBoxBitPadding_CheckedChanged(object sender, EventArgs e)
        {
            if (dataGridViewSymbolInfo.SelectedRows.Count != 0)
            {
                var selectedRow = dataGridViewSymbolInfo.SelectedRows[0];
                RefreshSymbolGrid(selectedRow.Index);
            }
            else
            {
                RefreshSymbolGrid(0);
            }
        }

		private void checkBoxFunctionAnalysis_CheckedChanged(object sender, EventArgs e)
		{
			Cursor.Current = Cursors.WaitCursor;
			_SymbolAnalyzer.FunctionAnalysis = checkBoxFunctionAnalysis.Checked;
			Cursor.Current = Cursors.Default;
		}

		private void restrictToSymbolsImportedFroCSVToolStripMenuItem_Click(object sender, EventArgs e)
		{
			restrictToSymbolsImportedFroCSVToolStripMenuItem.Checked = !restrictToSymbolsImportedFroCSVToolStripMenuItem.Checked;
			PopulateDataTable();
		}

		private void addMemPoolsToolStripMenuItem_Click(object sender, EventArgs e)
		{
			var memPoolsForm = new AddMemPoolsForm();
			memPoolsForm.SetMemPools(_MemPools);
			memPoolsForm.ShowDialog();
			_MemPools.Clear();
			_MemPools.AddRange(memPoolsForm.GetMemPool());
			if (!_HasMemPools && _MemPools.Count > 0)
			{
				_HasMemPools = true;
				_Table.Columns.Add(new DataColumn
				{
					ColumnName = "MemPool waste",
					ReadOnly = true,
					DataType = Type.GetType("System.UInt32")
				});
				_Table.Columns.Add(new DataColumn
				{
					ColumnName = "MemPool total waste",
					ReadOnly = true,
					DataType = Type.GetType("System.UInt64")
				});
				_Table.Columns.Add(new DataColumn
				{
					ColumnName = "MemPool delta",
					ReadOnly = true,
					DataType = Type.GetType("System.UInt32")
				});
			}
			PopulateDataTable();
		}

	}
}