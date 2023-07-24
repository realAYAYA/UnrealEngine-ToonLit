// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	class TextBoxWithCueBanner : TextBox
	{
		private const int EmSetcuebanner = 0x1501;

		[DllImport("user32.dll", CharSet = CharSet.Unicode)]
		private static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wparam, string lparam);

		private string _cueBannerValue = String.Empty;

		public string CueBanner
		{
			get { return _cueBannerValue; }
			set { _cueBannerValue = value; UpdateCueBanner(); }
		}

		private void UpdateCueBanner()
		{
			if(IsHandleCreated)
			{
				SendMessage(Handle, EmSetcuebanner, (IntPtr)1, _cueBannerValue);
			}
		}

		protected override void OnHandleCreated(EventArgs e)
		{
			base.OnHandleCreated(e);
			UpdateCueBanner();
		}
	}
}
