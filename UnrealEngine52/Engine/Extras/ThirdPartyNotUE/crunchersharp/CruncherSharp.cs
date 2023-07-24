using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using Dia2Lib;

namespace CruncherSharp
{
    public partial class CruncherSharp : Form
    {
        public CruncherSharp()
        {
            InitializeComponent();
            m_table = CreateDataTable();
            bindingSourceSymbols.DataSource = m_table;
            dataGridSymbols.DataSource = bindingSourceSymbols;

            dataGridSymbols.Columns[0].Width = 271;
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void loadPDBToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (openPdbDialog.ShowDialog() == DialogResult.OK)
            {
                m_symbols.Clear();

                m_source = new DiaSourceClass();
                try
                {
                    Cursor.Current = Cursors.WaitCursor;
                    m_source.loadDataFromPdb(openPdbDialog.FileName);
                    m_source.openSession(out m_session);
                    Cursor.Current = Cursors.Default;
                }
                catch (System.Runtime.InteropServices.COMException exc)
                {
                    MessageBox.Show(this, exc.ToString());
                    return;
                }

                Cursor.Current = Cursors.WaitCursor;
                IDiaEnumSymbols allSymbols;
                m_session.findChildren(m_session.globalScope, SymTagEnum.SymTagUDT, null, 0, out allSymbols);

				// Temporarily clear the filter so, if current filter is invalid, we don't generate a ton of exceptions while populating the table
				var preExistingFilter = textBoxFilter.Text;
				textBoxFilter.Text = "";

				{
					PopulateDataTable(m_table, allSymbols);
					Cursor.Current = Cursors.Default;

					// Sort by name by default (ascending)
					dataGridSymbols.Sort(dataGridSymbols.Columns[0], ListSortDirection.Ascending);
					bindingSourceSymbols.Filter = null;// "Symbol LIKE '*rde*'";
				}

				// Restore the filter now that the table is populated
				textBoxFilter.Text = preExistingFilter;

				ShowSelectedSymbolInfo();
            }
        }

        DataTable CreateDataTable()
        {
            DataTable table = new DataTable("Symbols");

            DataColumn column = new DataColumn();
            column.ColumnName = "Symbol";
            column.ReadOnly = true;
            table.Columns.Add(column);
            column = new DataColumn();
            column.ColumnName = "Size";
            column.ReadOnly = true;
            column.DataType = System.Type.GetType("System.Int32");
            table.Columns.Add(column);
            column = new DataColumn();
            column.ColumnName = "Padding";
            column.ReadOnly = true;
            column.DataType = System.Type.GetType("System.Int32");
            table.Columns.Add(column);
            column = new DataColumn();
            column.ColumnName = "Padding/Size";
            column.ReadOnly = true;
            column.DataType = System.Type.GetType("System.Double");
            table.Columns.Add(column);

            return table;
        }

        void PopulateDataTable(DataTable table, IDiaEnumSymbols symbols)
        {
            ulong cacheLineSize = GetCacheLineSize();
            table.Rows.Clear();

			table.BeginLoadData();
            foreach (IDiaSymbol sym in symbols)
            {
                if (sym.length > 0 && !HasSymbol(sym.name))
                {
                    SymbolInfo info = new SymbolInfo();
                    info.Set(sym.name, "", sym.length, 0);
                    info.ProcessChildren(sym);

                    long totalPadding = info.CalcTotalPadding();

                    DataRow row = table.NewRow();
                    string symbolName = sym.name;
                    row["Symbol"] = symbolName;
                    row["Size"] = info.m_size;
                    row["Padding"] = totalPadding;
                    row["Padding/Size"] = (double)totalPadding / info.m_size;
                    table.Rows.Add(row);

                    m_symbols.Add(info.m_name, info);
                }
            }
			table.EndLoadData();

        }
        bool HasSymbol(string name)
        {
            return m_symbols.ContainsKey(name);
        }
        ulong GetCacheLineSize()
        {
            return Convert.ToUInt32(textBoxCache.Text);
        }

        class SymbolInfo
        {
            public void ProcessChildren(IDiaSymbol symbol)
            {
                IDiaEnumSymbols children;
                symbol.findChildren(SymTagEnum.SymTagNull, null, 0, out children);
                foreach (IDiaSymbol child in children)
                {
                    SymbolInfo childInfo;
                    if (ProcessChild(child, out childInfo))
                        AddChild(childInfo);
                }
                // Sort children by offset, recalc padding.
                // Sorting is not needed normally (for data fields), but sometimes base class order is wrong.
                if (HasChildren())
                {
                    m_children.Sort(CompareOffsets);
                    for (int i = 0; i < m_children.Count; ++i)
                    {
                        SymbolInfo child = m_children[i];
                        child.m_padding = CalcPadding(child.m_offset, i);
                    }
                    m_padding = CalcPadding((long)m_size, m_children.Count);
                }
            }
            bool ProcessChild(IDiaSymbol symbol, out SymbolInfo info)
            {
                info = new SymbolInfo();
                if (symbol.isStatic != 0 || (symbol.symTag != (uint)SymTagEnum.SymTagData && symbol.symTag != (uint)SymTagEnum.SymTagBaseClass))
                {
                    return false;
                }
                // LocIsThisRel || LocIsNull || LocIsBitField
                if (symbol.locationType != 4 && symbol.locationType != 0 && symbol.locationType != 6)
                    return false;

                ulong len = symbol.length;
                IDiaSymbol typeSymbol = symbol.type;
                if (typeSymbol != null)
                    len = typeSymbol.length;

                string symbolName = symbol.name;
                if (symbol.symTag == (uint)SymTagEnum.SymTagBaseClass)
                    symbolName = "Base: " + symbolName;

                info.Set(symbolName, (typeSymbol != null ? typeSymbol.name : ""), len, symbol.offset);

                return true;
            }
            long CalcPadding(long offset, int index)
            {
                long padding = 0;
                if (HasChildren() && index > 0)
                {
                    SymbolInfo lastInfo = m_children[index - 1];
                    padding = offset - (lastInfo.m_offset + (long)lastInfo.m_size);
                }
                return padding > 0 ? padding : 0;
            }

            public void Set(string name, string typeName, ulong size, long offset)
            {
                m_name = name;
                m_typeName = typeName;
                m_size = size;
                m_offset = offset;
            }
            public bool HasChildren() { return m_children != null; }
            public long CalcTotalPadding()
            {
                long totalPadding = m_padding;
                if (HasChildren())
                {
                    foreach (SymbolInfo info in m_children)
                        totalPadding += info.m_padding;
                }
                return totalPadding;
            }
            void AddChild(SymbolInfo child)
            {
                if (m_children == null)
                    m_children = new List<SymbolInfo>();
                m_children.Add(child);
            }
            public bool IsBase()
            {
                return m_name.IndexOf("Base: ") == 0;
            }

            private static int CompareOffsets(SymbolInfo x, SymbolInfo y)
            {
                // Base classes have to go first.
                if (x.IsBase() && !y.IsBase())
                    return -1;
                if (!x.IsBase() && y.IsBase())
                    return 1;

                if (x.m_offset == y.m_offset)
                {
                    return (x.m_size == y.m_size ? 0 : (x.m_size < y.m_size) ? -1 : 1);
                }
                else return (x.m_offset < y.m_offset) ? -1 : 1;
            }

            public string m_name;
            public string m_typeName;
            public ulong m_size;
            public long m_offset;
            public long m_padding;
            public List<SymbolInfo> m_children;
        };

        SymbolInfo FindSelectedSymbolInfo()
        {
            if (dataGridSymbols.SelectedRows.Count == 0)
                return null;

            DataGridViewRow selectedRow = dataGridSymbols.SelectedRows[0];
            string symbolName = selectedRow.Cells[0].Value.ToString();

            SymbolInfo info = FindSymbolInfo(symbolName);
            return info;
        }
        SymbolInfo FindSymbolInfo(string name)
        {
            SymbolInfo info;
            m_symbols.TryGetValue(name, out info);
            return info;
        }

        IDiaDataSource m_source;
        IDiaSession m_session;
        Dictionary<string, SymbolInfo> m_symbols = new Dictionary<string, SymbolInfo>();
        DataTable m_table = null;
        long m_prefetchStartOffset = 0;

        private void dataGridSymbols_SelectionChanged(object sender, EventArgs e)
        {
            m_prefetchStartOffset = 0;
            ShowSelectedSymbolInfo();
        }

        void ShowSelectedSymbolInfo()
        {
            dataGridViewSymbolInfo.Rows.Clear();
            SymbolInfo info = FindSelectedSymbolInfo();
            if (info != null)
                ShowSymbolInfo(info);
        }

        void ShowSymbolInfo(SymbolInfo info)
        {
            dataGridViewSymbolInfo.Rows.Clear();
            if (!info.HasChildren())
                return;

            long cacheLineSize = (long)GetCacheLineSize();
            long prevCacheBoundaryOffset = m_prefetchStartOffset;

            if (prevCacheBoundaryOffset > (long)info.m_size)
                prevCacheBoundaryOffset = (long)info.m_size;

            long numCacheLines = 0;
            foreach (SymbolInfo child in info.m_children)
            {
                if (child.m_padding > 0)
                {
                    long paddingOffset = child.m_offset - child.m_padding;
                    string[] paddingRow = { "Padding", paddingOffset.ToString(), child.m_padding.ToString() };
                    dataGridViewSymbolInfo.Rows.Add(paddingRow);
                }

//                long childEndOffset = child.m_offset + (long)child.m_size;
                while (child.m_offset - prevCacheBoundaryOffset >= cacheLineSize)
                {
                    numCacheLines = numCacheLines + 1;
                    long cacheLineOffset = numCacheLines * cacheLineSize + m_prefetchStartOffset;
                    string[] boundaryRow = { "Cacheline boundary", cacheLineOffset.ToString(), "" };
                    dataGridViewSymbolInfo.Rows.Add(boundaryRow);
                    prevCacheBoundaryOffset = cacheLineOffset;
                }

                string[] row = { child.m_name, child.m_offset.ToString(), child.m_size.ToString() };
                dataGridViewSymbolInfo.Rows.Add(row);
                dataGridViewSymbolInfo.Rows[dataGridViewSymbolInfo.Rows.Count - 1].Tag = child;
                if (child.m_typeName != null && child.m_typeName.Length != 0)
                {
                    dataGridViewSymbolInfo.Rows[dataGridViewSymbolInfo.Rows.Count - 1].Cells[0].ToolTipText = child.m_typeName;
                }
            }
            // Final structure padding.
            if (info.m_padding > 0)
            {
                long paddingOffset = (long)info.m_size - info.m_padding;
                string[] paddingRow = { "Padding", paddingOffset.ToString(), info.m_padding.ToString() };
                dataGridViewSymbolInfo.Rows.Add(paddingRow);
            }
        }

        private void dataGridSymbols_SortCompare(object sender, DataGridViewSortCompareEventArgs e)
        {
            if (e.Column.Index > 0)
            {
                e.Handled = true;
                int val1;
                int val2;
                Int32.TryParse(e.CellValue1.ToString(), out val1);
                Int32.TryParse(e.CellValue2.ToString(), out val2);
                e.SortResult = val1 - val2;
            }
        }

        private void dataGridViewSymbolInfo_CellPainting(object sender, DataGridViewCellPaintingEventArgs e)
        {
            foreach (DataGridViewRow row in dataGridViewSymbolInfo.Rows)
            {
                DataGridViewCell cell = row.Cells[0];
                if (cell.Value.ToString() == "Padding")
                {
                    cell.Style.BackColor = Color.LightPink;
                    row.Cells[1].Style.BackColor = Color.LightPink;
                    row.Cells[2].Style.BackColor = Color.LightPink;
                }
                else if (cell.Value.ToString().IndexOf("Base: ") == 0)
                {
                    cell.Style.BackColor = Color.LightGreen;
                    row.Cells[1].Style.BackColor = Color.LightGreen;
                    row.Cells[2].Style.BackColor = Color.LightGreen;
                }
                else if (cell.Value.ToString() == "Cacheline boundary")
                {
                    cell.Style.BackColor = Color.LightGray;
                    row.Cells[1].Style.BackColor = Color.LightGray;
                    row.Cells[2].Style.BackColor = Color.LightGray;
                }
            }
        }

        private void textBoxFilter_TextChanged(object sender, EventArgs e)
        {
			try
			{
				if (textBoxFilter.Text.Length == 0)
					bindingSourceSymbols.Filter = null;
				else
					bindingSourceSymbols.Filter = "Symbol LIKE '" + textBoxFilter.Text + "'";

				textBoxFilter.BackColor = Color.Empty;
				textBoxFilter.ForeColor = Color.Empty;
				filterFeedbackLabel.Text = "";
			}
			catch (System.Data.EvaluateException)
			{
				textBoxFilter.BackColor = Color.Red;
				textBoxFilter.ForeColor = Color.White;
				filterFeedbackLabel.Text = "Invalid filter";
			}
		}

        void dumpSymbolInfo(System.IO.TextWriter tw, SymbolInfo info)
        {
            tw.WriteLine("Symbol: " + info.m_name);
            tw.WriteLine("Size: " + info.m_size.ToString());
            tw.WriteLine("Total padding: " + info.CalcTotalPadding().ToString());
            tw.WriteLine("Members");
            tw.WriteLine("-------");

            foreach (SymbolInfo child in info.m_children)
            {
                if (child.m_padding > 0)
                {
                    long paddingOffset = child.m_offset - child.m_padding;
                    tw.WriteLine(String.Format("{0,-40} {1,5} {2,5}", "****Padding", paddingOffset, child.m_padding));
                }

                tw.WriteLine(String.Format("{0,-40} {1,5} {2,5}", child.m_name, child.m_offset, child.m_size));
            }
            // Final structure padding.
            if (info.m_padding > 0)
            {
                long paddingOffset = (long)info.m_size - info.m_padding;
                tw.WriteLine(String.Format("{0,-40} {1,5} {2,5}", "****Padding", paddingOffset, info.m_padding));
            }
        }

        private void copyTypeLayoutToClipboardToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SymbolInfo info = FindSelectedSymbolInfo();
            if (info != null)
            {
                System.IO.StringWriter tw = new System.IO.StringWriter();
                dumpSymbolInfo(tw, info);
                Clipboard.SetText(tw.ToString());
            }
        }

        private void textBoxCache_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char)Keys.Enter || e.KeyChar == (char)Keys.Escape)
                ShowSelectedSymbolInfo();
            base.OnKeyPress(e);
        }

        private void textBoxCache_Leave(object sender, EventArgs e)
        {
            ShowSelectedSymbolInfo();
        }

        private void setPrefetchStartOffsetToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (dataGridViewSymbolInfo.SelectedRows.Count != 0)
            {
                DataGridViewRow selectedRow = dataGridViewSymbolInfo.SelectedRows[0];
                long symbolOffset = Convert.ToUInt32(selectedRow.Cells[1].Value);
                m_prefetchStartOffset = symbolOffset;
                ShowSelectedSymbolInfo();
            }
        }

        private void dataGridViewSymbolInfo_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            if (dataGridViewSymbolInfo.SelectedRows.Count != 0)
            {
                DataGridViewRow selectedRow = dataGridViewSymbolInfo.SelectedRows[0];
                SymbolInfo info = selectedRow.Tag as SymbolInfo;
                if (info != null)
                {
                    SelectSymbol(info.m_typeName);
                }
            }
        }

        void SelectSymbol(string name)
        {
            foreach (DataGridViewRow dr in dataGridSymbols.Rows)
            {
                DataGridViewCell dc = dr.Cells[0];  // name
                if (dc.Value.ToString() == name)
                {
                    dataGridSymbols.CurrentCell = dc;
                    break;
                }
            }
        }
    }
}
