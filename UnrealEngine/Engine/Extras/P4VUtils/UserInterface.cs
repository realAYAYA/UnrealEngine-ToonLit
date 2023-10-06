// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using Microsoft.Extensions.Logging;
using System.Linq;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading.Channels;



#if IS_WINDOWS
using System.Windows.Forms;
#endif


namespace P4VUtils
{
	public static class UserInterface
	{
#if IS_WINDOWS
		[System.Runtime.InteropServices.DllImport("user32.dll")]
		private static extern bool SetProcessDPIAware();

		public static void SetupVisuals()
		{
			// make the form look good on modern displays!
			SetProcessDPIAware();

			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
		}
#else
		public static void SetupVisuals() { }
#endif

		static UserInterface()
		{
			SetupVisuals();
		}


		#region Dialog Boxes

		public enum Button
		{
			Yes,
			No,
			OK,
			Cancel,
		}

		public static readonly Button[] YesNoCancel = { Button.Yes, Button.No, Button.Cancel };
		public static readonly Button[] YesNo = { Button.Yes, Button.No };
		public static readonly Button[] OK = { Button.OK };
		public static readonly Button[] OKCancel = { Button.OK, Button.Cancel };

#if IS_WINDOWS
		private static Dictionary<Button[], MessageBoxButtons> ButtonsToWindows = new () {
			{ YesNoCancel, MessageBoxButtons.YesNoCancel },
			{ YesNo, MessageBoxButtons.YesNo },
			{ OK, MessageBoxButtons.OK },
			{ OKCancel, MessageBoxButtons.OKCancel },
		};

		private static Dictionary<DialogResult, Button> WindowsToButton = new () {
			{ DialogResult.Yes, Button.Yes },
			{ DialogResult.No, Button.No },
			{ DialogResult.OK, Button.OK },
			{ DialogResult.Cancel, Button.Cancel },
		};
#endif

		public static void ShowSimpleDialog(string Message, String Title, ILogger Logger)
		{
			ShowDialog(Message, Title, OK, Button.OK, Logger);
		}

		public static Button ShowDialog(string Message, string Title, Button[] Buttons, Button DefaultButton, ILogger Logger)
		{
			Button Response = Button.OK;

			if (OperatingSystem.IsWindows())
			{
#if IS_WINDOWS
				DialogResult Result = MessageBox.Show(Message, Title, ButtonsToWindows[Buttons], MessageBoxIcon.Information);
				Response = WindowsToButton[Result];
#endif
			}
			else if (OperatingSystem.IsMacOS())
			{
				string ButtonString = string.Join(", ", Buttons.Select(x => $"\\\"{x}\\\""));
				int DefaultIndex = Buttons.ToList().IndexOf(DefaultButton) + 1;


				string Output = RunProcessAsync("/bin/bash",
					$"-c \" osascript -e 'display dialog \\\"{Message}\\\" buttons {{{ButtonString}}} default button {DefaultIndex} with icon caution'\"").Result;

				Output = Output.Replace("button returned:", "", StringComparison.OrdinalIgnoreCase);
				Response = Enum.Parse<Button>(Output);
			}
			else
			{
				Logger.LogWarning("{Message}", Message);
				if (Buttons.Length > 1)
				{
					Logger.LogWarning("Note: This message oringally was meant for a dialog with buttons {Buttons}, returning {Default}", string.Join(", ", Buttons), DefaultButton);
				}
				Response = DefaultButton;
			}

			return Response;
		}

		#endregion


		#region Clipboard

		public static async Task CopyTextToClipboard(string Text, ILogger Logger)
		{
			if (OperatingSystem.IsWindows())
			{
#if IS_WINDOWS
				// Jump through some hoops to make async & windows com happy and force STA on the clipboard call
				var tcs = new TaskCompletionSource<int>();

				var SetClipThread = new System.Threading.Thread(() => 
					{ 
						System.Windows.Forms.Clipboard.SetText(Text);
						tcs.SetResult(0);
					}
				);

				SetClipThread.SetApartmentState(System.Threading.ApartmentState.STA);
				SetClipThread.Start();
				SetClipThread.Join();

				await tcs.Task;
#endif
			}
			else if (OperatingSystem.IsMacOS())
			{
				await UserInterface.RunProcessAsync("/bin/bash", $" -c \"osascript -e 'set the clipboard to \\\"{Text}\\\"'\"");
			}
			else
			{
				Logger.LogError("Unable to copy to clipboard on this platform");
			}
		}

		#endregion


		#region Utils

		public static async Task<string> RunProcessAsync(string Executable, string Params)
		{
			ProcessStartInfo SI = new ProcessStartInfo(Executable, Params);
			SI.RedirectStandardOutput = true;
			using (Process Proc = Process.Start(SI)!)
			{
				StreamReader Reader = Proc.StandardOutput;
				string Output = (await Reader.ReadToEndAsync()).Trim();

				return Output;
			}
		}

		#endregion
	}
}

