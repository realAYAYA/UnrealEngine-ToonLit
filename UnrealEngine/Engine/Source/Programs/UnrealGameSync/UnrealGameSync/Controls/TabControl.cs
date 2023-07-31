// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Drawing.Drawing2D;

namespace UnrealGameSync
{
	class TabControl : Control
	{
		const int TabPadding = 13;//18;
		const int TabCloseButtonWidth = 8 + 13;

		class TabData
		{
			public string Name;
			public object Data;
			public int MinX;
			public int Width;
			public int CloseButtonWidth;
			public Size TextSize;
			public Tuple<Color, float>? Highlight;
			public Color? TintColor;

			public TabData(string name, object data)
			{
				this.Name = name;
				this.Data = data;
			}
		}

		class TabDragData
		{
			public TabData Tab;
			public int InitialIdx;
			public int MouseX;
			public int RelativeMouseX;

			public TabDragData(TabData tab, int initialIdx, int mouseX, int relativeMouseX)
			{
				this.Tab = tab;
				this.InitialIdx = initialIdx;
				this.MouseX = mouseX;
				this.RelativeMouseX = relativeMouseX;
			}
		}

		List<TabData> _tabs = new List<TabData>();
		int _selectedTabIdx;
		int _hoverTabIdx;
		int _highlightTabIdx;
		TabDragData? _dragState;

		public delegate void OnTabChangedDelegate(object? newTabData);
		public event OnTabChangedDelegate? OnTabChanged;

		public delegate void OnNewTabClickDelegate(Point location, MouseButtons buttons);
		public event OnNewTabClickDelegate? OnNewTabClick;

		public delegate void OnTabClickedDelegate(object? tabData, Point location, MouseButtons buttons);
		public event OnTabClickedDelegate? OnTabClicked;

		public delegate bool OnTabClosingDelegate(object tabData);
		public event OnTabClosingDelegate? OnTabClosing;

		public delegate void OnTabClosedDelegate(object tabData);
		public event OnTabClosedDelegate? OnTabClosed;

		public delegate void OnTabReorderDelegate();
		public event OnTabReorderDelegate? OnTabReorder;

		public delegate void OnButtonClickDelegate(int buttonIdx, Point location, MouseButtons buttons);
		public event OnButtonClickDelegate? OnButtonClick;

		public TabControl()
		{
			DoubleBuffered = true;
			_tabs.Add(new TabData("+", "+"));
			_selectedTabIdx = -1;
			_hoverTabIdx = -1;
			_highlightTabIdx = -1;
		}

		public void LockHover()
		{
			_highlightTabIdx = _hoverTabIdx;
		}

		public void UnlockHover()
		{
			_highlightTabIdx = -1;
			Invalidate();
		}

		public int FindTabIndex(object data)
		{
			int result = -1;
			for(int tabIdx = 0; tabIdx < _tabs.Count - 1; tabIdx++)
			{
				if(_tabs[tabIdx].Data == data)
				{
					result = tabIdx;
					break;
				}
			}
			return result;
		}

		public void SetHighlight(int tabIdx, Tuple<Color, float>? highlight)
		{
			Tuple<Color, float>? currentHighlight = _tabs[tabIdx].Highlight;
			if(highlight == null || currentHighlight == null)
			{
				if(highlight != currentHighlight)
				{
					_tabs[tabIdx].Highlight = highlight;
					Invalidate();
				}
			}
			else
			{
				if(highlight.Item1 != currentHighlight.Item1 || highlight.Item2 != currentHighlight.Item2)
				{
					_tabs[tabIdx].Highlight = highlight;
					Invalidate();
				}
			}
		}

		public void SetTint(int tabIdx, Color? tintColor)
		{
			if(_tabs[tabIdx].TintColor != tintColor)
			{
				_tabs[tabIdx].TintColor = tintColor;
				Invalidate();
			}
		}

		public int InsertTab(int insertIdx, string name, object data, Color? tintColor)
		{
			int idx = _tabs.Count - 1;
			if(insertIdx >= 0 && insertIdx < _tabs.Count - 1)
			{
				idx = insertIdx;
			}
			if(_selectedTabIdx >= idx)
			{
				_selectedTabIdx++;
			}

			_tabs.Insert(idx, new TabData(name, data){ TintColor = tintColor });
			LayoutTabs();
			Invalidate();
			return idx;
		}

		public int GetTabCount()
		{
			return _tabs.Count - 1;
		}

		public void SetTabName(int tabIdx, string name)
		{
			_tabs[tabIdx].Name = name;
			LayoutTabs();
			Invalidate();
		}

		public object GetTabData(int tabIdx)
		{
			return _tabs[tabIdx].Data;
		}

		public void SetTabData(int tabIdx, object data)
		{
			_tabs[tabIdx].Data = data;
		}

		public int GetSelectedTabIndex()
		{
			return _selectedTabIdx;
		}

		public object GetSelectedTabData()
		{
			return _tabs[_selectedTabIdx].Data;
		}

		public void SelectTab(int tabIdx)
		{
			if(tabIdx != _selectedTabIdx)
			{
				ForceSelectTab(tabIdx);
			}
		}

		private void ForceSelectTab(int tabIdx)
		{
			_selectedTabIdx = tabIdx;
			LayoutTabs();
			Invalidate();

			if(OnTabChanged != null)
			{
				if(_selectedTabIdx == -1)
				{
					OnTabChanged(null);
				}
				else
				{
					OnTabChanged(_tabs[_selectedTabIdx].Data);
				}
			}
		}

		public void RemoveTab(int tabIdx)
		{
			object tabData = _tabs[tabIdx].Data;
			if(OnTabClosing == null || OnTabClosing(tabData))
			{
				_tabs.RemoveAt(tabIdx);

				if(_hoverTabIdx == tabIdx)
				{
					_hoverTabIdx = -1;
				}

				if(_highlightTabIdx == tabIdx)
				{
					_highlightTabIdx = -1;
				}

				if(_selectedTabIdx == tabIdx)
				{
					if(_selectedTabIdx < _tabs.Count - 1)
					{
						ForceSelectTab(_selectedTabIdx);
					}
					else
					{
						ForceSelectTab(_selectedTabIdx - 1);
					}
				}
				else
				{
					if(_selectedTabIdx > tabIdx)
					{
						_selectedTabIdx--;
					}
				}

				LayoutTabs();
				Invalidate();

				if(OnTabClosed != null)
				{
					OnTabClosed(tabData);
				}
			}
		}

		void LayoutTabs()
		{
			using(Graphics graphics = CreateGraphics())
			{
				float dpiScaleX = graphics.DpiX / 96.0f;
				float dpiScaleY = graphics.DpiY / 96.0f;

				for(int idx = 0; idx < _tabs.Count; idx++)
				{
					TabData tab = _tabs[idx];
					tab.TextSize = TextRenderer.MeasureText(graphics, tab.Name, Font, new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
					tab.Width = TabPadding + tab.TextSize.Width + (int)(TabPadding * dpiScaleX);
					if(idx == _selectedTabIdx)
					{
						tab.CloseButtonWidth = (int)(TabCloseButtonWidth * dpiScaleX);
						tab.Width += tab.CloseButtonWidth;
					}
				}

				int leftX = 0;
				for(int idx = 0; idx < _tabs.Count; idx++)
				{
					TabData tab = _tabs[idx];
					tab.MinX = leftX;
					leftX += tab.Width;
				}

				int rightX = Width - 1;
				if(leftX > rightX)
				{
					int usedWidth = _tabs.Take(_tabs.Count - 1).Sum(x => x.Width);
					int remainingWidth = rightX + 1 - _tabs[_tabs.Count - 1].Width;
					if(_selectedTabIdx != -1)
					{
						usedWidth -= _tabs[_selectedTabIdx].Width;
						remainingWidth -= _tabs[_selectedTabIdx].Width;
					}

					int newX = 0;
					for(int idx = 0; idx < _tabs.Count; idx++)
					{
						_tabs[idx].MinX = newX;
						int prevWidth = _tabs[idx].Width;
						if(idx != _selectedTabIdx && idx != _tabs.Count - 1)
						{
							_tabs[idx].Width = Math.Max((_tabs[idx].Width * remainingWidth) / usedWidth, TabPadding * 3);
						}
						_tabs[idx].TextSize.Width -= prevWidth - _tabs[idx].Width;
						newX += _tabs[idx].Width;
					}
				}

				if(_dragState != null)
				{
					int minOffset = -_dragState.Tab.MinX;
					int maxOffset = _tabs[_tabs.Count - 1].MinX - _dragState.Tab.Width - _dragState.Tab.MinX;

					int offset = (_dragState.MouseX - _dragState.RelativeMouseX) - _dragState.Tab.MinX;
					offset = Math.Max(offset, minOffset);
					offset = Math.Min(offset, maxOffset);

					_dragState.Tab.MinX += offset;
				}
			}
		}

		protected override void OnResize(EventArgs e)
		{
			base.OnResize(e);

			LayoutTabs();
			Invalidate();
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);

			OnMouseMove(e);

			if(_hoverTabIdx != -1)
			{
				if(e.Button == MouseButtons.Left)
				{
					if(_hoverTabIdx == _selectedTabIdx)
					{
						if(e.Location.X > _tabs[_hoverTabIdx].MinX + _tabs[_hoverTabIdx].Width - _tabs[_hoverTabIdx].CloseButtonWidth)
						{
							RemoveTab(_hoverTabIdx);
						}
						else
						{
							_dragState = new TabDragData(_tabs[_selectedTabIdx], _selectedTabIdx, e.Location.X, e.Location.X - _tabs[_selectedTabIdx].MinX);
						}
					}
					else
					{
						if(_hoverTabIdx > _tabs.Count - 1)
						{
							OnButtonClick?.Invoke(_hoverTabIdx - _tabs.Count, e.Location, e.Button);
						}
						else if(_hoverTabIdx == _tabs.Count - 1)
						{
							OnNewTabClick?.Invoke(e.Location, e.Button);
						}
						else
						{
							SelectTab(_hoverTabIdx);
						}
					}
				}
				else if(e.Button == MouseButtons.Middle)
				{
					if(_hoverTabIdx < _tabs.Count - 1)
					{
						RemoveTab(_hoverTabIdx);
					}
				}
			}

			if(OnTabClicked != null)
			{
				object? tabData = (_hoverTabIdx == -1)? null : _tabs[_hoverTabIdx].Data;
				OnTabClicked(tabData, e.Location, e.Button);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			base.OnMouseUp(e);

			if(_dragState != null)
			{
				if(OnTabReorder != null)
				{
					OnTabReorder();
				}

				_dragState = null;

				LayoutTabs();
				Invalidate();
			}
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			if(_dragState != null)
			{
				_dragState.MouseX = e.Location.X;

				_tabs.Remove(_dragState.Tab);

				int totalWidth = 0;
				for(int insertIdx = 0;;insertIdx++)
				{
					if(insertIdx == _tabs.Count - 1 || _dragState.MouseX - _dragState.RelativeMouseX < totalWidth + _tabs[insertIdx].Width / 2)
					{
						_hoverTabIdx = _selectedTabIdx = insertIdx;
						_tabs.Insert(insertIdx, _dragState.Tab);
						break;
					}
					totalWidth += _tabs[insertIdx].Width;
				}

				LayoutTabs();
				Invalidate();
			}
			else
			{
				int newHoverTabIdx = -1;
				if(ClientRectangle.Contains(e.Location))
				{
					for(int idx = 0; idx < _tabs.Count; idx++)
					{
						if(e.Location.X > _tabs[idx].MinX && e.Location.X < _tabs[idx].MinX + _tabs[idx].Width)
						{
							newHoverTabIdx = idx;
							break;
						}
					}
				}

				if(_hoverTabIdx != newHoverTabIdx)
				{
					_hoverTabIdx = newHoverTabIdx;
					LayoutTabs();
					Invalidate();

					if(_hoverTabIdx != -1)
					{
						Capture = true;
					}
					else
					{
						Capture = false;
					}
				}
			}
		}

		protected override void OnMouseCaptureChanged(EventArgs e)
		{
			base.OnMouseCaptureChanged(e);

			if(_hoverTabIdx != -1)
			{
				_hoverTabIdx = -1;
				Invalidate();
			}
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			LayoutTabs();

			using(SolidBrush hoverBrush = new SolidBrush(Color.FromArgb(192, 192, 192)))
			{
				for(int idx = 0; idx < _tabs.Count; idx++)
				{
					TabData tab = _tabs[idx];
					if(idx == _hoverTabIdx || idx == _highlightTabIdx)
					{
						DrawBackground(e.Graphics, tab, SystemBrushes.Window, hoverBrush, tab.Highlight);
					}
					else
					{
						DrawBackground(e.Graphics, tab, SystemBrushes.Control, SystemBrushes.Control, tab.Highlight);
					}
				}
			}

			using(Pen separatorPen = new Pen(Color.FromArgb(192, SystemColors.ControlDarkDark)))
			{
				for(int idx = 0; idx < _tabs.Count; idx++)
				{
					TabData tab = _tabs[idx];
					if((idx > 0 && idx < _tabs.Count - 1) || idx >= _tabs.Count || idx == _selectedTabIdx || idx == _hoverTabIdx || idx == _highlightTabIdx || _tabs.Count == 1)
					{
						e.Graphics.DrawLine(separatorPen, tab.MinX, 0, tab.MinX, ClientSize.Height - 2);
					}
					if(idx < _tabs.Count - 1 || idx >= _tabs.Count || idx == _hoverTabIdx || idx == _highlightTabIdx || _tabs.Count == 1)
					{
						e.Graphics.DrawLine(separatorPen, tab.MinX + tab.Width, 0, tab.MinX + tab.Width, ClientSize.Height - 2);
					}
					if(idx == _hoverTabIdx || idx == _highlightTabIdx || idx >= _tabs.Count)
					{
						e.Graphics.DrawLine(separatorPen, tab.MinX, 0, tab.MinX + tab.Width, 0);
					}
				}
			}

			for(int idx = 0; idx < _tabs.Count; idx++)
			{
				if(idx != _selectedTabIdx)
				{
					DrawText(e.Graphics, _tabs[idx]);
				}
			}


			if(_selectedTabIdx != -1)
			{
				TabData selectedTab = _tabs[_selectedTabIdx];

				using(SolidBrush selectedBrush = new SolidBrush(Color.FromArgb(0, 136, 204)))
				{
					DrawBackground(e.Graphics, selectedTab, SystemBrushes.Window, selectedBrush, selectedTab.Highlight);
				}

				DrawText(e.Graphics, selectedTab);
	
				// Draw the close button
				SmoothingMode prevSmoothingMode = e.Graphics.SmoothingMode;
				e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

				float dpiScaleX = e.Graphics.DpiX / 96.0f;
				float dpiScaleY = e.Graphics.DpiY / 96.0f;

				int closeMidX = selectedTab.MinX + selectedTab.Width - (int)((TabPadding + TabCloseButtonWidth) * dpiScaleX / 2);// + (13 / 2);
				int closeMidY = (ClientSize.Height - (int)(4 * dpiScaleY)) / 2;

				Rectangle closeButton = new Rectangle(closeMidX - (int)((13 / 2) * dpiScaleX), closeMidY - (int)((13 / 2) * dpiScaleY), (int)(13 * dpiScaleX), (int)(13 * dpiScaleY));
				e.Graphics.FillEllipse(SystemBrushes.ControlDark, closeButton);

				using(Pen crossPen = new Pen(SystemBrushes.Window, 2.0f * dpiScaleX))
				{
					float indentX = 3.5f * dpiScaleX;
					float indentY = 3.5f * dpiScaleY;
					e.Graphics.DrawLine(crossPen, closeButton.Left + indentX, closeButton.Top + indentY, closeButton.Right - indentX, closeButton.Bottom - indentY);
					e.Graphics.DrawLine(crossPen, closeButton.Left + indentX, closeButton.Bottom - indentY, closeButton.Right - indentX, closeButton.Top + indentY);
				}
				e.Graphics.SmoothingMode = prevSmoothingMode;

				// Border
				e.Graphics.DrawLine(SystemPens.ControlDark, selectedTab.MinX, 0, selectedTab.MinX + selectedTab.Width, 0);
				e.Graphics.DrawLine(SystemPens.ControlDark, selectedTab.MinX, 0, selectedTab.MinX, ClientSize.Height - 2);
				e.Graphics.DrawLine(SystemPens.ControlDark, selectedTab.MinX + selectedTab.Width, 0, selectedTab.MinX + selectedTab.Width, ClientSize.Height - 2);
			}

			e.Graphics.DrawLine(SystemPens.ControlDarkDark, 0, ClientSize.Height - 2,  ClientSize.Width, ClientSize.Height - 2);
			e.Graphics.DrawLine(SystemPens.ControlLightLight, 0, ClientSize.Height - 1, ClientSize.Width, ClientSize.Height - 1);
		}

		void DrawBackground(Graphics graphics, TabData tab, Brush backgroundBrush, Brush stripeBrush, Tuple<Color, float>? highlight)
		{
			float dpiScaleY = graphics.DpiY / 96.0f;

			graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;

			graphics.FillRectangle(backgroundBrush, tab.MinX + 1, 1, tab.Width - 1, ClientSize.Height - 3);
			graphics.FillRectangle(stripeBrush, tab.MinX + 1, ClientSize.Height - (int)(5 * dpiScaleY), tab.Width - 1, (int)(5 * dpiScaleY) - 2);

			if (tab.TintColor.HasValue)
			{
				int midX = tab.MinX + tab.Width / 2;
				using (LinearGradientBrush brush = new LinearGradientBrush(new Rectangle(midX, 1, 1, ClientSize.Height), Color.FromArgb(64, tab.TintColor.Value), Color.FromArgb(32, tab.TintColor.Value), LinearGradientMode.Vertical))
				{
					graphics.FillRectangle(brush, tab.MinX + 1, 1, tab.Width - 1, ClientSize.Height - 3);
				}
			}

			if (highlight != null && highlight.Item2 > 0.0f)
			{
				using(SolidBrush brush = new SolidBrush(highlight.Item1))
				{
					graphics.FillRectangle(brush, tab.MinX + 1, ClientSize.Height - (int)(5 * dpiScaleY), (int)((tab.Width - 2) * highlight.Item2), (int)(5 * dpiScaleY) - 2);
				}
			}
		}

		void DrawText(Graphics graphics, TabData tab)
		{
			float dpiScaleX = graphics.DpiX / 96.0f;
			float dpiScaleY = graphics.DpiY / 96.0f;
			TextRenderer.DrawText(graphics, tab.Name, Font, new Rectangle(tab.MinX + (int)(TabPadding * dpiScaleX), 0, tab.TextSize.Width, ClientSize.Height - (int)(3 * dpiScaleY)), Color.Black, TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis | TextFormatFlags.VerticalCenter);
		}
	}
}
