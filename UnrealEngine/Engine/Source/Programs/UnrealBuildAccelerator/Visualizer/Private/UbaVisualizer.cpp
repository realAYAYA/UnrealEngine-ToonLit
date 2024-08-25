// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaVisualizer.h"
#include <algorithm>

#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment (lib, "UxTheme.lib")
#pragma comment (lib, "Dwmapi.lib")
#define WM_NEWTRACE WM_USER+1

enum
{
	Popup_CopySessionInfo = 3,
	Popup_CopyProcessInfo,
	Popup_CopyProcessLog,
	Popup_Replay,
	Popup_Pause,
	Popup_Play,
	Popup_JumpToEnd,
	Popup_ShowText,
	Popup_ShowCreateWriteColors,
	Popup_SaveAs,
	Popup_Quit,
};

namespace uba
{
	class LineCountLogger : public Logger
	{
	public:
		virtual void BeginScope() override {}
		virtual void EndScope() override {}
		virtual void Log(LogEntryType type, const wchar_t* str, u32 strLen) override { ++lineCount; }
		u32 lineCount = 0;
	};

	class DrawTextLogger : public Logger
	{
	public:
		DrawTextLogger(HDC h, const RECT& r, int fh) : hdc(h), rect(r), fontHeight(fh) {}

		virtual void BeginScope() override {}
		virtual void EndScope() override {}
		virtual void Log(LogEntryType type, const wchar_t* str, u32 strLen) override
		{
			DrawTextW(hdc, str, strLen, &rect, DT_SINGLELINE);
			rect.top += fontHeight;
		}

		DrawTextLogger& SetColor(COLORREF c) { SetTextColor(hdc, c); return *this; }

		HDC hdc;
		RECT rect;
		int fontHeight;
	};

	class WriteTextLogger : public Logger
	{
	public:
		WriteTextLogger(TString& out) : m_out(out) {}
		virtual void BeginScope() override {}
		virtual void EndScope() override {}
		virtual void Log(LogEntryType type, const wchar_t* str, u32 strLen) override { m_out.append(str, strLen).append(TC("\n")); }
		TString& m_out;
	};

	Visualizer::Visualizer(Logger& logger)
	:	m_logger(logger)
	,	m_trace(logger)
	{
		for (bool& visible : m_visibleComponents)
			visible = true;
		m_visibleComponents[ComponentType_DetailedData] = false;
		m_visibleComponents[ComponentType_Workers] = false;
	}

	Visualizer::~Visualizer()
	{
		m_looping = false;

		// Make sure GetMessage is triggered out of its slumber
		PostMessage(m_hwnd, WM_QUIT, 0, 0);

		m_thread.Wait();
		delete m_client;
	}

	void Visualizer::SetTheme(bool dark)
	{
		m_isThemeSet = true;
		m_useDarkMode = dark;
	}

	bool Visualizer::ShowUsingListener(const wchar_t* channelName)
	{
		TraceChannel channel(m_logger);
		if (!channel.Init(channelName))
		{
			m_logger.Error(L"TODO");
			return false;
		}

		m_listenChannel.Append(channelName);
		m_looping = true;
		m_autoScroll = false;
		m_thread.Start([this]() { ThreadLoop(); return 0;});

		while (!m_hwnd)
			if (m_thread.Wait(10))
				return true;

		StringBuffer<256> traceName;
		while (m_hwnd)
		{
			traceName.Clear();
			if (!channel.Read(traceName))
			{
				m_logger.Error(L"TODO2");
				return false;
			}

			if (traceName.count && !traceName.Equals(m_newTraceName.data))
			{
				m_newTraceName.Clear().Append(traceName);
				PostMessage(m_hwnd, WM_NEWTRACE, 0, 0);
			}
			Sleep(1000);
		}

		return true;
	}

	bool Visualizer::ShowUsingNamedTrace(const wchar_t* namedTrace)
	{
		if (!m_trace.StartReadNamed(m_traceView, namedTrace))
			return false;
		m_namedTrace.Append(namedTrace);
		m_looping = true;
		m_thread.Start([this]() { ThreadLoop(); return 0;});
		return true;
	}

	bool Visualizer::ShowUsingSocket(NetworkBackend& backend, const wchar_t* host, u16 port)
	{
		auto destroyClient = MakeGuard([&]() { delete m_client; m_client = nullptr; });
		m_looping = true;
		m_autoScroll = false;
		m_thread.Start([this]() { ThreadLoop(); return 0; });

		while (!m_hwnd)
			if (m_thread.Wait(10))
				return true;

		wchar_t dots[] = TC("....");
		u32 dotsCounter = 0;

		StringBuffer<256> traceName;
		while (m_hwnd)
		{
			if (!m_client)
			{
				bool ctorSuccess = true;
				m_client = new NetworkClient(ctorSuccess, {});
				if (!ctorSuccess)
					return false;
			}

			StringBuffer<> title;
			GetTitlePrefix(title);
			title.Appendf(L"Trying to connect to %s:%u%s", host, port, dots + ((dotsCounter--) % 4));
			SetWindowTextW(m_hwnd, title.data);

			if (!m_client->Connect(backend, host, port))
				continue;

			PostMessage(m_hwnd, WM_NEWTRACE, 0, 0);

			while (m_hwnd && m_client->IsConnected())
				Sleep(1000);

			m_client->Disconnect();
			delete m_client;
			m_client = nullptr;
			Sleep(2000); // To prevent it from reconnecting to the same thing again and get thrown out (since it will post a WM_NEWTRACE and clean everything
		}
		return true;
	}

	bool Visualizer::ShowUsingFile(const wchar_t* fileName, u32 replay)
	{
		m_looping = true;
		m_autoScroll = false;
		m_thread.Start([this]() { ThreadLoop(); return 0;});

		while (!m_hwnd)
			if (m_thread.Wait(10))
				return true;
		m_fileName.Append(fileName);
		m_replay = replay;
		PostMessage(m_hwnd, WM_NEWTRACE, 0, 0);
		return true;
	}

	bool Visualizer::HasWindow()
	{
		return m_looping == true;
	}

	HWND Visualizer::GetHwnd()
	{
		return m_hwnd;
	}

	void Visualizer::GetTitlePrefix(StringBufferBase& out)
	{
		out.Append(L"UbaVisualizer");
		#if UBA_DEBUG
		out.Append(L" (DEBUG)");
		#endif
		out.Append(L" - ");
	}

	void Visualizer::Reset()
	{
		for (HBITMAP bm : m_textBitmaps)
			DeleteObject(bm);
		DeleteObject(m_lastBitmap);
		m_contentWidth = 0;
		m_contentHeight = 0;
		m_textBitmaps.clear();
		m_lastBitmap = 0;
		m_lastBitmapOffset = BitmapCacheHeight;
		m_traceView.Clear();
		m_autoScroll = true;
		m_scrollPosX = 0;
		m_scrollPosY = 0;
		//m_zoomValue = 0.75f;
		//m_horizontalScaleValue = 1.0f;

		//m_replay = 0;
		m_startTime = GetTime();
		m_pauseTime = 0;
	}

	void Visualizer::ThreadLoop()
	{
		if (!m_isThemeSet)
		{
			DWORD value = 1;
			DWORD valueSize = sizeof(value);
			if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &valueSize) == ERROR_SUCCESS)
				m_useDarkMode = value == 0;
		}

		if (m_useDarkMode)
		{
			m_textColor = RGB(190, 190, 190);
			m_textWarningColor = RGB(190, 190, 0);
			m_textErrorColor = RGB(190, 0, 0);

			m_processBrushes[0].inProgress = CreateSolidBrush(RGB(70, 70, 70));
			m_processBrushes[1].inProgress = CreateSolidBrush(RGB(130, 130, 130));

			m_processBrushes[0].error = CreateSolidBrush(RGB(140, 0, 0));
			m_processBrushes[1].error = CreateSolidBrush(RGB(190, 0, 0));

			m_processBrushes[0].returned = CreateSolidBrush(RGB(50, 50, 120));
			m_processBrushes[1].returned = CreateSolidBrush(RGB(70, 70, 160));

			m_processBrushes[0].recv = CreateSolidBrush(RGB(10, 92, 10));
			m_processBrushes[1].recv = CreateSolidBrush(RGB(10, 130, 10));
			m_processBrushes[0].success = CreateSolidBrush(RGB(10, 100, 10));
			m_processBrushes[1].success = CreateSolidBrush(RGB(10, 140, 10));
			m_processBrushes[0].send = CreateSolidBrush(RGB(10, 115, 10));
			m_processBrushes[1].send = CreateSolidBrush(RGB(10, 145, 10));

			m_workBrush = CreateSolidBrush(RGB(70, 70, 100));

			m_backgroundBrush = CreateSolidBrush(0x00252526);
			m_separatorPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
			m_tooltipBackgroundBrush = CreateSolidBrush(0x00404040);
			m_checkboxPen = CreatePen(PS_SOLID, 1, RGB(130, 130, 130));

			m_sendColor = RGB(0, 170, 0);
			m_recvColor = RGB(0, 170, 255);
			m_cpuColor = RGB(170, 170, 0);
			m_memColor = RGB(170, 0, 255);
		}
		else
		{
			m_textColor = GetSysColor(COLOR_INFOTEXT);
			m_textWarningColor = RGB(170, 130, 0);
			m_textErrorColor = RGB(190, 0, 0);

			m_processBrushes[0].inProgress = CreateSolidBrush(RGB(150, 150, 150));
			m_processBrushes[1].inProgress = CreateSolidBrush(RGB(180, 180, 180));

			m_processBrushes[0].error = CreateSolidBrush(RGB(255, 70, 70));
			m_processBrushes[1].error = CreateSolidBrush(RGB(255, 100, 70));

			m_processBrushes[0].returned = CreateSolidBrush(RGB(150, 150, 200));
			m_processBrushes[1].returned = CreateSolidBrush(RGB(170, 170, 200));

			m_processBrushes[0].recv = CreateSolidBrush(RGB(10, 190, 10));
			m_processBrushes[1].recv = CreateSolidBrush(RGB(20, 210, 20));
			m_processBrushes[0].success = CreateSolidBrush(RGB(10, 200, 10));
			m_processBrushes[1].success = CreateSolidBrush(RGB(20, 220, 20));
			m_processBrushes[0].send = CreateSolidBrush(RGB(80, 210, 80));
			m_processBrushes[1].send = CreateSolidBrush(RGB(90, 250, 90));

			m_workBrush = CreateSolidBrush(RGB(150, 150, 200));

			m_backgroundBrush = GetSysColorBrush(0);
			m_separatorPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
			m_tooltipBackgroundBrush = GetSysColorBrush(COLOR_INFOBK);
			m_checkboxPen = CreatePen(PS_SOLID, 1, RGB(130, 130, 130));

			m_sendColor = RGB(0, 170, 0); // Green
			m_recvColor = RGB(63, 72, 204); // Blue
			m_cpuColor = RGB(200, 130, 0); // Orange
			m_memColor = RGB(170, 0, 255); // Purple
		}

		m_textPen = CreatePen(PS_SOLID, 1, m_textColor);
		m_sendPen = CreatePen(PS_SOLID, 1, m_sendColor);
		m_recvPen = CreatePen(PS_SOLID, 1, m_recvColor);
		m_cpuPen = CreatePen(PS_SOLID, 1, m_cpuColor);
		m_memPen = CreatePen(PS_SOLID, 1, m_memColor);

		LOGBRUSH br = { 0 };
		GetObject(m_backgroundBrush, sizeof(br), &br);
		m_processUpdatePen = CreatePen(PS_SOLID, 2, RGB(GetRValue(br.lbColor), GetGValue(br.lbColor), GetBValue(br.lbColor)));

		HINSTANCE hInstance = GetModuleHandle(NULL);
		u32 winPosX = 100;
		u32 winPosY = 100;
		u32 winWidth = 1500;
		u32 winHeight = 1500;

		WNDCLASSEX wndClassEx;
		ZeroMemory(&wndClassEx, sizeof(wndClassEx));
		wndClassEx.cbSize = sizeof(wndClassEx);
		wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
		wndClassEx.lpfnWndProc = &StaticWinProc;
		wndClassEx.hIcon = NULL;//Icon;
		wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClassEx.hInstance = hInstance;
		wndClassEx.hbrBackground = NULL;
		wndClassEx.lpszClassName = TEXT("UbaVisualizer");
		ATOM wndClassAtom = RegisterClassEx(&wndClassEx);

		NONCLIENTMETRICS nonClientMetrics;
		nonClientMetrics.cbSize = sizeof(nonClientMetrics);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nonClientMetrics), &nonClientMetrics, 0);
		//m_font = (HFONT)CreateFontIndirect(&nonClientMetrics.lfMessageFont);
		m_font = (HFONT)CreateFontW(-9, 0, 0, 0, FW_NORMAL, false, false, false, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");

		const TCHAR* fontName = TEXT("Consolas");
		m_popupFont = (HFONT)CreateFontW(-12, 0, 0, 0, FW_NORMAL, false, false, false, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 0, FIXED_PITCH | FF_MODERN, fontName);
		m_popupFontHeight = 14;
		//m_popupFont = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);

		DWORD windowStyle = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL;
		const TCHAR* windowClassName = MAKEINTATOM(wndClassAtom);

		StringBuffer<> title;
		GetTitlePrefix(title);
		if (!m_namedTrace.IsEmpty())
			title.Append(m_namedTrace);
		else if (!m_fileName.IsEmpty())
			title.Append(m_fileName);
		else if (m_listenChannel.count)
			title.Appendf(L"Listening for new sessions on channel '%s'", m_listenChannel.data);
		else
			title.Append(L"Socket");

		m_hwnd = CreateWindowEx(0, windowClassName, title.data, windowStyle, winPosX, winPosY, winWidth, winHeight, NULL, NULL, hInstance, this);
		SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

		BOOL cloak = TRUE;
		DwmSetWindowAttribute(m_hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));

		if (m_useDarkMode)
		{
			SetWindowTheme(m_hwnd, L"DarkMode_Explorer", NULL);
			SendMessageW(m_hwnd, WM_THEMECHANGED, 0, 0);
			BOOL useDarkMode = true;
			u32 attribute = 20; // DWMWA_USE_IMMERSIVE_DARK_MODE
			DwmSetWindowAttribute(m_hwnd, attribute, &useDarkMode, sizeof(useDarkMode));
		}

		HitTestResult res;
		HitTest(res, { -1, -1 });

		ShowWindow(m_hwnd, SW_SHOW);
		UpdateWindow(m_hwnd);
		UpdateScrollbars(true);

		cloak = FALSE;
		DwmSetWindowAttribute(m_hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));

		m_startTime = GetTime();

		while (m_looping)
		{
			MSG msg;
			while (GetMessage(&msg, NULL, 0, 0))
			{
				if (m_hwnd && !IsDialogMessage(m_hwnd, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				// It may happen that we receive the WM_DESTOY message from within the DistpachMessage above and handle it directly in WndProc.
				// So before trying to call GetMessage again, we just need to validate that m_looping is still true otherwise we could end
				// up waiting forever for this thread to exit.
				if (!m_looping || msg.message == WM_QUIT || msg.message == WM_DESTROY || msg.message == WM_CLOSE)
				{
					if (m_hwnd)
						DestroyWindow(m_hwnd);
					DeleteObject(m_font);
					UnregisterClass(windowClassName, hInstance);
					m_hwnd = 0;
					m_looping = false;
					break;
				}
			}
		}
	}

	void Visualizer::Pause(bool pause)
	{
		if (m_paused == pause)
			return;

		m_paused = pause;
		if (pause)
		{
			m_pauseStart = GetTime();
		}
		else
		{
			m_replay = 1;
			m_pauseTime += GetTime() - m_pauseStart;
			m_traceView.finished = false;
			SetTimer(m_hwnd, 0, 200, NULL);
		}
	}

	void Visualizer::PaintClient(const Function<void(HDC hdc, HDC memDC, RECT& clientRect)>& paintFunc)
	{
		HDC hdc = GetDC(m_hwnd);

		RECT rect;
		GetClientRect(m_hwnd, &rect);

		HDC memDC = CreateCompatibleDC(hdc);

		if (!EqualRect(&m_cachedBitmapRect, &rect))
		{
			if (m_cachedBitmap)
				DeleteObject(m_cachedBitmap);
			m_cachedBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
			m_cachedBitmapRect = rect;
		}
		HGDIOBJ oldBmp = SelectObject(memDC, m_cachedBitmap);
		
		paintFunc(hdc, memDC, rect);

		SelectObject(memDC, oldBmp);
		DeleteDC(memDC);

		ReleaseDC(m_hwnd, hdc);
	}

	constexpr int ProgressRectLeft = 30;
	constexpr int GraphHeight = 30;
	constexpr int RawBoxHeight = 15;
	constexpr int SessionStepY = RawBoxHeight + 2;
	constexpr int FontHeight = 13;

	struct SessionRec
	{
		TraceView::Session* session;
		u32 index;
	};
	void Populate(SessionRec* recs, TraceView& traceView)
	{
		u32 count = u32(traceView.sessions.size());
		for (u32 i = 0, e = count; i != e; ++i)
			recs[i] = { &traceView.sessions[i], i };
		if (count <= 1)
			return;
		std::sort(recs + 1, recs + traceView.sessions.size(), [](SessionRec& a, SessionRec& b)
			{
				auto& as = *a.session;
				auto& bs = *b.session;
				if ((as.processActiveCount != 0) != (bs.processActiveCount != 0))
					return as.processActiveCount > bs.processActiveCount;
				if (as.processActiveCount && as.proxyCreated != bs.proxyCreated)
					return int(as.proxyCreated) > int(bs.proxyCreated);
				return a.index < b.index;
			});
	}

	void Visualizer::PaintAll(HDC hdc, const RECT& clientRect)
	{
		u64 currentTime = m_paused ? m_pauseStart : GetTime();
		u64 playTime = currentTime - m_traceView.startTime - m_pauseTime;
		if (m_replay)
			playTime *= m_replay;

		int posY = int(m_scrollPosY);
		float boxHeight = float(RawBoxHeight)*m_zoomValue;
		int stepY = int(boxHeight) + 2;
		float scaleX = 50.0f*m_zoomValue*m_horizontalScaleValue;

		RECT progressRect = clientRect;
		progressRect.left += ProgressRectLeft;
		progressRect.bottom -= 30;

		bool shouldDrawText = m_showText && m_zoomValue > 0.27f;

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, m_textColor);
		SelectObject(hdc, m_font);

		HDC textDC = CreateCompatibleDC(hdc);
		SetTextColor(textDC, m_textColor);
		SelectObject(textDC, m_font);
		SelectObject(textDC, GetStockObject(NULL_BRUSH));
		SetBkMode(textDC, TRANSPARENT);
		SetBkColor(hdc, m_useDarkMode ? RGB(70, 70, 70) : RGB(180, 180, 180));

		//TEXTMETRIC metric;
		//GetTextMetrics(textDC, &metric);

		HBITMAP nullBmp = CreateCompatibleBitmap(hdc, 1, 1);
		HBITMAP oldBmp = (HBITMAP)SelectObject(textDC, nullBmp);
		HBITMAP lastSelectedBitmap = 0;
		HBRUSH lastSelectedBrush = 0;

		u64 lastStop = 0;

		if (!m_traceView.statusMap.empty())
		{
			posY += 4;

			auto drawStatusText = [&](const TString& text, LogEntryType type, int left, bool moveY)
				{
					RECT rect;
					rect.left = left;
					rect.right = clientRect.right;
					rect.top = posY;
					rect.bottom = posY + FontHeight + 2;
					SetTextColor(hdc, type == LogEntryType_Info ? m_textColor : (type == LogEntryType_Error ? m_textErrorColor : m_textWarningColor));
					ExtTextOutW(hdc, left, posY, ETO_CLIPPED, &rect, text.c_str(), u32(text.size()), NULL);
					if (moveY)
						posY = rect.bottom;
				};

			for (auto& kv : m_traceView.statusMap)
			{
				auto& status = kv.second;
				if (status.name.empty() && status.text.empty())
					continue;
				drawStatusText(status.name, status.type, 5 + status.nameIndent*15, false);
				drawStatusText(status.text, status.type, 5 + status.textIndent*15, true);
			}
			SetTextColor(hdc, m_textColor);
			posY += 4;
		}

		TraceView::WorkRecord selectedWork;

		SessionRec sortedSessions[1024];
		Populate(sortedSessions, m_traceView);

		TraceView::ProcessLocation processLocation { 0, 0, 0 };
		for (u64 i = 0, e = m_traceView.sessions.size(); i != e; ++i)
		{
			auto& session = *sortedSessions[i].session;
			bool hasUpdates = !session.updates.empty();
			if (!hasUpdates && session.processors.empty())
				continue;

			processLocation.sessionIndex = sortedSessions[i].index;
			bool isFirst = i == 0;
			if (!isFirst)
				posY += 3;

			if (posY + stepY >= progressRect.top && posY <= progressRect.bottom)
			{
				SelectObject(hdc, m_separatorPen);
				MoveToEx(hdc, 0, posY, NULL);
				LineTo(hdc, clientRect.right, posY);

				StringBuffer<> text;
				text.Append(session.name);

				if (hasUpdates && session.disconnectTime == ~u64(0))
				{
					u64 ping = session.updates.back().ping;
					u64 memAvail = session.updates.back().memAvail;
					float cpuLoad = session.updates.back().cpuLoad;

					text.Appendf(L" - Cpu: %.1f%%", cpuLoad * 100.0f);
					if (memAvail)
						text.Appendf(L" Mem: %ls/%ls", BytesToText(session.memTotal - memAvail).str, BytesToText(session.memTotal).str);
					if (ping)
						text.Appendf(L" Ping: %ls", TimeToText(ping, false, m_traceView.frequency).str);
					if (!session.notification.empty())
						text.Append(L" - ").Append(session.notification);
				}
				else
				{
					text.Append(L" - Disconnected");
					if (!session.notification.empty())
						text.Append(L" (").Append(session.notification).Append(')');
				}

				bool selected = m_sessionSelectedIndex == processLocation.sessionIndex;

				int textBottom = Min(posY + SessionStepY, int(progressRect.bottom));
				{
					RECT rect;
					rect.left = 5;
					rect.right = clientRect.right;
					rect.top = posY;
					rect.bottom = textBottom;

					if (selected)
						SetBkMode(hdc, OPAQUE);
					ExtTextOutW(hdc, 5, posY+2, ETO_CLIPPED, &rect, text.data, text.count, NULL);
					if (selected)
						SetBkMode(hdc, TRANSPARENT);
				}
			}
			posY += SessionStepY;

			bool showGraph = m_visibleComponents[ComponentType_SendRecv] || m_visibleComponents[ComponentType_CpuMem];
			if (showGraph && hasUpdates)
			{
				if (posY + GraphHeight >= progressRect.top && posY + GraphHeight - 5 < progressRect.bottom)
				{
					int posX = int(m_scrollPosX) + progressRect.left;
					bool isFirstUpdate = true;
					u64 prevTime = 0;
					u64 prevSend = 0;
					u64 prevRecv = 0;
					float prevCupLoad = 0;
					int graphBaseY = posY + GraphHeight - 4;
					int prevX = 0;
					int prevSendY = 0;
					int prevRecvY = 0;
					int prevCpuY = 0;
					int prevMemY = 0;
					double sendScale = double(session.highestSendPerS) / (double(GraphHeight) - 2);
					double recvScale = double(session.highestRecvPerS) / (double(GraphHeight) - 2);

					for (auto& update : session.updates)
					{
						float cpuLoad = update.cpuLoad;
						if (cpuLoad < 0 || cpuLoad > 1.0f)
							cpuLoad = prevCupLoad;
						else
							prevCupLoad = cpuLoad;

						auto updateSend = update.send;
						auto updateRecv = update.recv;

						int x = int(posX + TimeToS(update.time) * scaleX);
						int sendY = graphBaseY;
						int recvY = graphBaseY;
						int cpuY = graphBaseY - int(cpuLoad*(GraphHeight-2));
						int memY = graphBaseY - int(double(session.memTotal - update.memAvail)*(GraphHeight - 2)/session.memTotal);

						double duration = TimeToS(update.time - prevTime);
						if (update.time == 0)
							isFirstUpdate = true;
						else if (prevSend > updateSend || prevRecv > updateRecv)
							isFirstUpdate = true;

						if (double sendInvScaleY = duration * sendScale)
							sendY = graphBaseY - int(double(updateSend - prevSend) / sendInvScaleY);
						if (double recvInvScaleY = duration * recvScale)
							recvY = graphBaseY - int(double(updateRecv - prevRecv) / recvInvScaleY) - 1;

						if (!isFirstUpdate && x > clientRect.left && prevX <= clientRect.right)
						{
							if (m_visibleComponents[ComponentType_SendRecv] && updateSend != 0 && updateRecv != 0)
							{
								SelectObject(hdc, m_sendPen);
								MoveToEx(hdc, prevX, prevSendY, NULL);
								LineTo(hdc, x, sendY);
								SelectObject(hdc, m_recvPen);
								MoveToEx(hdc, prevX, prevRecvY, NULL);
								LineTo(hdc, x, recvY);
							}
							if (m_visibleComponents[ComponentType_CpuMem])
							{
								SelectObject(hdc, m_cpuPen);
								MoveToEx(hdc, prevX, prevCpuY, NULL);
								LineTo(hdc, x, cpuY);
								SelectObject(hdc, m_memPen);
								MoveToEx(hdc, prevX, prevMemY, NULL);
								LineTo(hdc, x, memY);
							}
						}
						isFirstUpdate = false;
						prevX = x;
						prevSendY = sendY;
						prevRecvY = recvY;
						prevCpuY = cpuY;
						prevMemY = memY;
						prevTime = update.time;
						prevSend = updateSend;
						prevRecv = updateRecv;
					}
				}
				posY += GraphHeight;
			}

			if (m_visibleComponents[ComponentType_DetailedData])
			{
				auto drawText = [&](const StringBufferBase& text, RECT& rect)
					{
						bool selected = m_fetchedFilesSelected == processLocation.sessionIndex && text.StartsWith(TC("Fetched Files"));
						if (selected)
							SetBkMode(hdc, OPAQUE);
						DrawTextW(hdc, text.data, text.count, &rect, DT_SINGLELINE);
						if (selected)
							SetBkMode(hdc, TRANSPARENT);
					};
				bool isRemote = processLocation.sessionIndex != 0;
				PaintDetailedStats(posY, progressRect, session, isRemote, playTime, drawText);
			}

			if (m_visibleComponents[ComponentType_Bars])
			{
				processLocation.processorIndex = 0;
				for (auto& processor : session.processors)
				{
					if (posY + SessionStepY >= progressRect.top && posY < progressRect.bottom)
					{
						float barHeight = boxHeight;
						int textOffsetY = 0;
						if (posY + int(boxHeight) > progressRect.bottom)
						{
							float newBarHeight = Min(barHeight, float(progressRect.bottom - posY));
							textOffsetY = int(barHeight - newBarHeight);
							barHeight = newBarHeight;
						}

						const int textHeight = int(barHeight);
						const int rectBottom = posY + textHeight;
						const int offsetY = (textHeight - FontHeight + textOffsetY) / 2;

						if (shouldDrawText)
						{
							RECT rect;
							rect.left = 5;
							rect.right = progressRect.left - 5;
							rect.top = posY;
							rect.bottom = rectBottom;

							StringBuffer<> buf;
							buf.AppendValue(u64(processLocation.processorIndex) + 1);
							ExtTextOutW(hdc, 5, posY + offsetY, ETO_CLIPPED, &rect, buf.data, buf.count, NULL);
						}

						processLocation.processIndex = 0;
						int posX = int(m_scrollPosX) + progressRect.left;
						for (auto& process : processor.processes)
						{
							int left = int(posX + TimeToS(process.start) * scaleX);

							if (left >= progressRect.right)
							{
								++processLocation.processIndex;
								continue;
							}

							u64 stop = process.stop;
							bool done = stop != ~u64(0);
							if (!done)
								stop = playTime;

							RECT rect;
							rect.left = left;
							rect.right = int(posX + TimeToS(stop) * scaleX) - 1;
							rect.top = posY;
							rect.bottom = rectBottom;

							if (rect.right <= progressRect.left)
							{
								++processLocation.processIndex;
								continue;
							}

							rect.right = Max(int(rect.right), left + 1);

							bool selected = m_processSelected && m_processSelectedLocation == processLocation;
							if (selected)
								process.bitmapDirty = true;

							--rect.top;
							PaintProcessRect(process, hdc, rect, progressRect, selected, false);
							++rect.top;

							int processWidth = rect.right - rect.left;
							if (shouldDrawText && processWidth > 3)
							{
								if (!process.bitmap || process.bitmapDirty)
								{
									if (!process.bitmap)
									{
										if (m_lastBitmapOffset == BitmapCacheHeight)
										{
											if (m_lastBitmap)
												m_textBitmaps.push_back(m_lastBitmap);

											m_lastBitmapOffset = 0;
											m_lastBitmap = CreateCompatibleBitmap(hdc, 256, BitmapCacheHeight);
										}
										process.bitmap = m_lastBitmap;
										process.bitmapOffset = m_lastBitmapOffset;
										m_lastBitmapOffset += FontHeight;
									}
									if (lastSelectedBitmap != process.bitmap)
									{
										SelectObject(textDC, process.bitmap);
										lastSelectedBitmap = process.bitmap;
									}

									RECT rect2{ 0, int(process.bitmapOffset), 256, int(process.bitmapOffset) + FontHeight };
									RECT rect3{ 0, int(process.bitmapOffset), processWidth, int(process.bitmapOffset) + FontHeight };
									if (!done)
										rect3.right = 256;

									PaintProcessRect(process, textDC, rect3, rect2, selected, true);

									rect2.left += 3; // Move in text a bit
									
									bool dropShadow = m_useDarkMode;
									if (dropShadow)
									{
										SetTextColor(textDC, RGB(5, 60, 5));
										++rect2.left;
										++rect2.top;
										ExtTextOutW(textDC, rect2.left, rect2.top, ETO_CLIPPED, &rect2, process.description.c_str(), int(process.description.size()), NULL);
										--rect2.left;
										--rect2.top;
									}
									SetTextColor(textDC, m_textColor);
									ExtTextOutW(textDC, rect2.left, rect2.top, ETO_CLIPPED, &rect2, process.description.c_str(), int(process.description.size()), NULL);

									if (!selected)
										process.bitmapDirty = false;
								}

								if (lastSelectedBitmap != process.bitmap)
								{
									SelectObject(textDC, process.bitmap);
									lastSelectedBitmap = process.bitmap;
								}

								int width = Min(processWidth, 256);
								int bitmapOffsetY = process.bitmapOffset;
								int bltOffsetY = offsetY;
								if (bltOffsetY < 0)
								{
									bitmapOffsetY -= bltOffsetY;
									bltOffsetY = 0;
								}
								int height = Min(textHeight, FontHeight);
								if (bltOffsetY + height > textHeight)
									height = textHeight - bltOffsetY;

								if (left > -256 && height >= 0)
								{
									int bitmapOffsetX = rect.left - left;

									if (left < progressRect.left)
									{
										int diff = progressRect.left - left;
										rect.left = progressRect.left;
										width -= diff;
										bitmapOffsetX += diff;
									}
									BitBlt(hdc, rect.left, rect.top + bltOffsetY, width, height, textDC, bitmapOffsetX, bitmapOffsetY, SRCCOPY);
								}
							}
							++processLocation.processIndex;
						}
					}

					lastStop = Max(lastStop, processor.processes.rbegin()->stop);

					++processLocation.processorIndex;
					posY += stepY;
				}
			}
			else
			{
				for (auto& processor : session.processors)
					if (!processor.processes.empty())
						lastStop = Max(lastStop, processor.processes.rbegin()->stop);
			}

			if (m_visibleComponents[ComponentType_Workers] && isFirst)
			{
				u32 trackIndex = 0;
				for (auto& workTrack : m_traceView.workTracks)
				{
					if (posY + SessionStepY >= progressRect.top && posY <= progressRect.bottom)
					{
						int textOffsetY = 0;
						float barHeight = boxHeight;
						if (posY + int(boxHeight) > progressRect.bottom)
						{
							float newBarHeight = Min(barHeight, float(progressRect.bottom - posY));
							textOffsetY = int(barHeight - newBarHeight);
							barHeight = newBarHeight;
						}

						const int textHeight = int(barHeight);
						const int rectBottom = posY + textHeight;
						const int offsetY = (textHeight - FontHeight + textOffsetY) / 2;

						if (shouldDrawText)
						{
							RECT rect;
							rect.left = 5;
							rect.right = progressRect.left - 5;
							rect.top = posY;
							rect.bottom = rectBottom;

							StringBuffer<> buf;
							buf.AppendValue(u64(trackIndex) + 1);
							ExtTextOutW(hdc, 5, posY + offsetY, ETO_CLIPPED, &rect, buf.data, buf.count, NULL);
						}

						u32 workIndex = 0;
						int posX = int(m_scrollPosX) + progressRect.left;
						for (auto& work : workTrack.records)
						{
							if (work.start == work.stop)
							{
								++workIndex;
								continue;
							}

							float startTime = TimeToS(work.start);

							int left = int(posX + startTime * scaleX);

							if (left >= progressRect.right)
							{
								++workIndex;
								continue;
							}

							HBRUSH brush;

							u64 stop = work.stop;

							bool done = stop != ~u64(0);
							if (done)
							{
								brush = m_workBrush;
							}
							else
							{
								stop = playTime;
								brush = m_processBrushes[0].inProgress;
							}

							float stopTime = TimeToS(stop);
							if ((stopTime - startTime) * scaleX < 0.05f)
							{
								++workIndex;
								continue;
							}

							RECT rect;
							rect.left = left;
							rect.right = int(posX + stopTime * scaleX) - 1;
							rect.top = posY;
							rect.bottom = rectBottom;
							//rect.bottom = posY + int(float(18) * m_zoomValue);

							if (rect.right <= progressRect.left)
							{
								++workIndex;
								continue;
							}

							rect.right = Max(int(rect.right), left + 1);

							bool selected = m_workSelected && m_workTrack == trackIndex && m_workIndex == workIndex;
							if (selected)
								selectedWork = work;

							if (lastSelectedBrush != brush)
							{
								SelectObject(hdc, brush);
								lastSelectedBrush = brush;
							}

							--rect.top;

							auto clampRect = [&](RECT& r) { r.left = Min(Max(r.left, progressRect.left), progressRect.right); r.right = Max(Min(r.right, progressRect.right), progressRect.left); };

							clampRect(rect);
							FillRect(hdc, &rect, brush);
							++rect.top;

							int processWidth = rect.right - rect.left;
							if (shouldDrawText && processWidth > 3)
							{
								if (!work.bitmap)
								{
									if (m_lastBitmapOffset == BitmapCacheHeight)
									{
										if (m_lastBitmap)
											m_textBitmaps.push_back(m_lastBitmap);

										m_lastBitmapOffset = 0;
										m_lastBitmap = CreateCompatibleBitmap(hdc, 256, BitmapCacheHeight);
									}
									SelectObject(textDC, m_lastBitmap);

									RECT rect2{ 0,m_lastBitmapOffset,256, m_lastBitmapOffset + FontHeight };

									FillRect(textDC, &rect2, m_workBrush);
									ExtTextOutW(textDC, rect2.left, rect2.top, ETO_CLIPPED, &rect2, work.description, int(wcslen(work.description)), NULL);
									work.bitmap = m_lastBitmap;
									work.bitmapOffset = m_lastBitmapOffset;
									m_lastBitmapOffset += FontHeight;
								}

								if (lastSelectedBitmap != work.bitmap)
								{
									SelectObject(textDC, work.bitmap);
									lastSelectedBitmap = work.bitmap;
								}

								int width = Min(processWidth, 256);
								int bitmapOffsetY = work.bitmapOffset;
								int bltOffsetY = offsetY;
								if (bltOffsetY < 0)
								{
									bitmapOffsetY -= bltOffsetY;
									bltOffsetY = 0;
								}
								int height = Min(textHeight, FontHeight);
								if (bltOffsetY + height > textHeight)
									height = textHeight - bltOffsetY;

								if (left > -256 && height >= 0)
								{
									int bitmapOffsetX = rect.left - left;

									if (left < progressRect.left)
									{
										int diff = progressRect.left - left;
										rect.left = progressRect.left;
										width -= diff;
										bitmapOffsetX += diff;
									}
									BitBlt(hdc, rect.left, rect.top + bltOffsetY, width, height, textDC, bitmapOffsetX, bitmapOffsetY, SRCCOPY);
								}
							}
							++workIndex;
						}
					}
					++trackIndex;
					posY += stepY;
				}
			}
		}

		SelectObject(textDC, oldBmp);
		DeleteObject(nullBmp);
		DeleteDC(textDC);

		m_contentWidth = ProgressRectLeft + int(TimeToS(lastStop != ~u64(0) ? lastStop : playTime) * scaleX);
		m_contentHeight = posY - int(m_scrollPosY) + stepY + 14;

		if (m_visibleComponents[ComponentType_Timeline])
			PaintTimeline(hdc, clientRect);

		{
			int top = 5;
			int bottom = SessionStepY - 3;
			int left = progressRect.right - 16;
			int right = progressRect.right - 7;
			for (int i = sizeof_array(m_visibleComponents) - 1; i >= 0; --i)
			{
				SelectObject(hdc, m_buttonSelected == u32(i) ? m_textPen : m_checkboxPen);
				SelectObject(hdc, GetStockObject(NULL_BRUSH));
				Rectangle(hdc, left, top, right, bottom);

				if (m_visibleComponents[i])
				{
					MoveToEx(hdc, left + 2, top + 2, NULL);
					LineTo(hdc, right - 2, bottom - 2);
					MoveToEx(hdc, right - 3, top + 2, NULL);
					LineTo(hdc, left + 1, bottom - 2);
				}

				left -= 14;
				right -= 14;
			}

			top -= 2;
			left -= 20;
			if (m_visibleComponents[ComponentType_CpuMem])
			{
				SetTextColor(hdc, m_cpuColor);
				ExtTextOutW(hdc, left, top, 0, NULL, L"CPU", 3, NULL);
				left -= 25;

				SetTextColor(hdc, m_memColor);
				ExtTextOutW(hdc, left, top, 0, NULL, L"MEM", 3, NULL);
				left -= 25;
			}
			if (m_visibleComponents[ComponentType_SendRecv])
			{
				SetTextColor(hdc, m_sendColor);
				ExtTextOutW(hdc, left, top, 0, NULL, L"SND", 3, NULL);
				left -= 25;

				SetTextColor(hdc, m_recvColor);
				ExtTextOutW(hdc, left, top, 0, NULL, L"RCV", 3, NULL);
			}
			SetTextColor(hdc, m_textColor);
		}

		auto GetClientCursorPos = [this](POINT& p)
			{
				GetCursorPos(&p);
				ScreenToClient(m_hwnd, &p);
				p.x += 3;
				p.y += 3;
			};

		if (m_processSelected)
		{
			TraceView::Process& process = *m_traceView.GetProcess(m_processSelectedLocation);
			bool hasStorageStats = false;
			u32 lineCount = 4;
			u64 duration = 0;
			
			int width = 290;
			Vector<TString> logLines;
			u32 maxCharCount = 50u;

			bool hasExited = process.stop != ~u64(0);
			if (hasExited)
			{
				LineCountLogger counter;
				process.processStats.Print(counter, m_traceView.frequency);
				u32 prevLineCount = counter.lineCount;
				process.sessionStats.Print(counter, m_traceView.frequency);
				process.storageStats.Print(counter, m_traceView.frequency);
				process.systemStats.Print(counter, false, m_traceView.frequency);
				hasStorageStats = prevLineCount != counter.lineCount;
				lineCount = 6 + counter.lineCount;
				if (hasStorageStats)
					lineCount += 6;
				duration = process.stop - process.start;
				if (process.exitCode != 0)
					++lineCount;

				if (!process.logLines.empty())
				{
					u32 lineMaxCount = 0;
					for (auto& line : process.logLines)
					{
						u32 offset = 0;
						u32 left = u32(line.text.size());
						while (left)
						{
							u32 toCopy = Min(left, maxCharCount);
							lineMaxCount = Max(lineMaxCount, toCopy);
							logLines.push_back(line.text.substr(offset, toCopy));
							offset += toCopy;
							left -= toCopy;
						}
					}

					width = Max(width, int(lineMaxCount) * 7 + 2*14);

					lineCount += u32(logLines.size()) + 1;
				}
			}
			else
			{
				duration = playTime - process.start;
			}

			int height = lineCount*m_popupFontHeight;

			POINT p;
			GetClientCursorPos(p);
			RECT r;
			r.left = p.x;
			r.top = p.y;
			r.right = r.left + width;
			r.bottom = r.top + height;

			if (r.right > clientRect.right)
				OffsetRect(&r, -width, 0);
			if (r.bottom > clientRect.bottom)
			{
				OffsetRect(&r, 0, clientRect.bottom - r.bottom);
				if (r.top < 0)
					OffsetRect(&r, 0, -r.top);
			}

			FillRect(hdc, &r, m_tooltipBackgroundBrush);

			r.top += 5;
			SelectObject(hdc, m_popupFont);
			DrawTextLogger logger(hdc, r, m_popupFontHeight);

			logger.Info(L"  %ls", process.description.c_str());
			logger.Info(L"  Start:     %ls", TimeToText(process.start, true).str);
			logger.Info(L"  Duration:  %ls", TimeToText(duration, true).str);
			if (hasExited && process.exitCode != 0)
				logger.Info(L"  ExitCode:  %u", process.exitCode);
			logger.Info(L"");

			if (process.stop != ~u64(0))
			{
				logger.Info(L"  ----------- Process stats -----------");
				process.processStats.Print(logger, m_traceView.frequency);
				if (hasStorageStats)
				{
					logger.Info(L"");
					logger.Info(L"  ----------- Session stats -----------");
					process.sessionStats.Print(logger, m_traceView.frequency);
					logger.Info(L"");
					logger.Info(L"  ----------- Storage stats -----------");
					process.storageStats.Print(logger, m_traceView.frequency);
					logger.Info(L"");
					logger.Info(L"  ----------- System stats ------------");
					process.systemStats.Print(logger, false, m_traceView.frequency);
				}

				if (!logLines.empty())
				{
					logger.Info(L"  ---------------- Log ----------------");
					logger.rect.left += 14;
					for (auto& line : logLines)
						logger.Log(LogEntryType_Info, line.c_str(), u32(line.size()));
				}
			}
		}
		else if (m_workSelected && selectedWork.description)
		{
			int width = 290;
			int height = 3*m_popupFontHeight;
			POINT p;
			GetClientCursorPos(p);
			RECT r;
			r.left = p.x;
			r.top = p.y;
			r.right = r.left + width;
			r.bottom = r.top + height;

			u64 duration;
			if (selectedWork.stop != ~u64(0))
				duration = selectedWork.stop - selectedWork.start;
			else
				duration = playTime - selectedWork.start;

			r.bottom += 10;
			FillRect(hdc, &r, m_tooltipBackgroundBrush);

			r.top += 5;
			SelectObject(hdc, m_popupFont);
			DrawTextLogger logger(hdc, r, m_popupFontHeight);
			logger.Info(L"  %ls", selectedWork.description);
			logger.Info(L"  Start:     %ls", TimeToText(selectedWork.start, true).str);
			logger.Info(L"  Duration:  %ls", TimeToText(duration, true).str);
		}
		else if (m_sessionSelectedIndex != ~0u)
		{
			int width = 290;

			Vector<TString> summary = m_traceView.sessions[m_sessionSelectedIndex].summary;
			if (summary.empty())
			{
				if (m_traceView.finished)
					summary.push_back(L" Session summary not available on this trace version");
				else
					summary.push_back(L" Session summary not available until session is done");
				summary.push_back(L"");
				width = 380;
			}

			int height = int(summary.size())*m_popupFontHeight;

			POINT p;
			GetCursorPos(&p);
			ScreenToClient(m_hwnd, &p);
			RECT r;
			r.left = p.x;
			r.top = p.y;
			r.right = r.left + width;
			r.bottom = r.top + height;

			if (r.right > clientRect.right)
				OffsetRect(&r, -width, 0);
			if (r.bottom > clientRect.bottom)
			{
				OffsetRect(&r, 0, -height);
				if (r.top < 0)
					OffsetRect(&r, 0, -r.top);
			}
			FillRect(hdc, &r, m_tooltipBackgroundBrush);

			r.top += 5;
			SelectObject(hdc, m_popupFont);

			for (auto& line : summary)
			{
				DrawTextW(hdc, line.c_str(), int(line.size()), &r, DT_SINGLELINE);
				r.top += m_popupFontHeight;
			}
		}
		else if (m_statsSelected)
		{
			int width = 160;
			int lineCount = m_stats.ping ? 5 : 4;
			int height = lineCount * m_popupFontHeight + 6;

			POINT p;
			GetCursorPos(&p);
			ScreenToClient(m_hwnd, &p);
			RECT r;
			r.left = p.x;
			r.top = p.y;
			r.right = r.left + width;
			r.bottom = r.top + height;

			if (r.right > clientRect.right)
				OffsetRect(&r, -width, 0);
			if (r.bottom > clientRect.bottom)
			{
				OffsetRect(&r, 0, -height);
				if (r.top < 0)
					OffsetRect(&r, 0, -r.top);
			}
			FillRect(hdc, &r, m_tooltipBackgroundBrush);

			r.top += 3;

			SelectObject(hdc, m_popupFont);
			DrawTextLogger logger(hdc, r, m_popupFontHeight);
			logger.SetColor(m_cpuColor).Info(L"  Cpu: %.1f%%", m_stats.cpuLoad * 100.0f);
			logger.SetColor(m_memColor).Info(L"  Mem: %ls/%ls", BytesToText(m_stats.memTotal - m_stats.memAvail).str, BytesToText(m_stats.memTotal).str);
			logger.SetColor(m_recvColor).Info(L"  Recv: %ls/s", BytesToText(m_stats.recvBytesPerSecond).str);
			logger.SetColor(m_sendColor).Info(L"  Send: %ls/s", BytesToText(m_stats.sendBytesPerSecond).str);
			if (m_stats.ping)
				logger.Info(L"  Ping: %ls", TimeToText(m_stats.ping, false, m_traceView.frequency).str);
		}
		else if (m_buttonSelected != ~0u)
		{
			const wchar_t* tooltip[] =
			{
				L"network stats",
				L"cpu/mem stats",
				L"process bars",
				L"timeline",
				L"detailed data (use -BoxDetailedTrace for even more)",
				L"workers (threads on host taking care of requests from helpers)",
			};

			int width = int(wcslen(tooltip[m_buttonSelected])+5) * 7 + 5;
			int lineCount = 1;
			int height = lineCount * m_popupFontHeight;

			POINT p;
			GetCursorPos(&p);
			ScreenToClient(m_hwnd, &p);
			RECT r;
			r.left = p.x;
			r.top = p.y;
			r.right = r.left + width;
			r.bottom = r.top + height;

			if (r.right > clientRect.right)
				OffsetRect(&r, -width, 0);
			if (r.bottom > clientRect.bottom)
			{
				OffsetRect(&r, 0, -height);
				if (r.top < 0)
					OffsetRect(&r, 0, -r.top);
			}
			FillRect(hdc, &r, m_tooltipBackgroundBrush);

			bool vis = m_visibleComponents[m_buttonSelected];
			SelectObject(hdc, m_popupFont);
			DrawTextLogger logger(hdc, r, m_popupFontHeight);
			logger.Info(L"%ls %ls", vis ? L"Hide" : L"Show", tooltip[m_buttonSelected]);
		}
		else if (m_timelineSelected)
		{
			int posX = int(m_scrollPosX) + progressRect.left;
			int left = int(posX + m_timelineSelected * scaleX);
			int timelineTop = Min(posY, int(progressRect.bottom)) + 2;

			// TODO: Draw line up
			MoveToEx(hdc, left, 2, NULL);
			LineTo(hdc, left, timelineTop);

			if (m_timelineSelected >= 0)
			{
				StringBuffer<> b;
				u32 milliseconds = u32(m_timelineSelected * 1000.0f);
				u32 seconds = milliseconds / 1000;
				milliseconds -= seconds * 1000;
				u32 minutes = seconds / 60;
				seconds -= minutes * 60;
				u32 hours = minutes / 60;
				minutes -= hours * 60;
				if (hours)
				{
					b.AppendValue(hours).Append('h');
					if (minutes < 10)
						b.Append('0');
				}
				if (minutes || hours)
				{
					b.AppendValue(minutes).Append('m');
					if (seconds < 10)
						b.Append('0');
				}
				b.AppendValue(seconds).Append('.');
				if (milliseconds < 100)
					b.Append('0');
				if (milliseconds < 10)
					b.Append('0');
				b.AppendValue(milliseconds);

				RECT r;
				r.left = left + 4;
				r.top = timelineTop - 20;
				r.right = left + b.count*8;
				r.bottom = r.top + 15;

				FillRect(hdc, &r, m_tooltipBackgroundBrush);
				SelectObject(hdc, m_popupFont);
				DrawTextLogger logger(hdc, r, m_popupFontHeight);

				logger.Info(L"%s", b.data);
			}
		}
		else if (m_fetchedFilesSelected != ~0u)
		{
			auto& session = m_traceView.sessions[m_fetchedFilesSelected];
			auto& fetchedFiles = session.fetchedFiles;
			if (!fetchedFiles.empty() && !fetchedFiles[0].hint.empty())
			{
				int colWidth = 500;
				int width = colWidth * 2;
				int height = Min(int(clientRect.bottom), int(fetchedFiles.size() * m_popupFontHeight));


				POINT p;
				GetCursorPos(&p);
				ScreenToClient(m_hwnd, &p);
				RECT r;
				r.left = p.x;
				r.top = p.y;
				r.right = r.left + width;
				r.bottom = r.top + height;

				if (r.right > clientRect.right)
					OffsetRect(&r, -width, 0);
				if (r.bottom > clientRect.bottom)
				{
					OffsetRect(&r, 0, -height);
					if (r.top < 0)
						OffsetRect(&r, 0, -r.top);
				}
				FillRect(hdc, &r, m_tooltipBackgroundBrush);

				SelectObject(hdc, m_font);
				DrawTextLogger logger(hdc, r, FontHeight);
				for (auto& f : fetchedFiles)
				{
					if (f.hint == TC("KnownInput"))
						continue;
					if (logger.rect.top >= r.bottom - FontHeight)
					{
						if (logger.rect.left + colWidth >= r.right)
						{
							logger.Info(L"...");
							break;
						}
						logger.rect.top = r.top;
						logger.rect.left += colWidth;
					}
					logger.Info(L"%s", f.hint.c_str());
				}
			}
		}
	}

	u64 ConvertTime(const TraceView& view, u64 time);

	void Visualizer::PaintProcessRect(TraceView::Process& process, HDC hdc, RECT rect, const RECT& progressRect, bool selected, bool writingBitmap)
	{
		auto clampRect = [&](RECT& r) { r.left = Min(Max(r.left, progressRect.left), progressRect.right); r.right = Max(Min(r.right, progressRect.right), progressRect.left); };
		bool done = process.stop != ~u64(0);

		HBRUSH brush = m_processBrushes[selected].success;
		if (process.returned)
			brush = m_processBrushes[selected].returned;
		else if (!done)
			brush = m_processBrushes[selected].inProgress;
		else if (process.exitCode != 0)
			brush = m_processBrushes[selected].error;

		u64 writeFilesTime = Max(process.processStats.writeFiles.time, process.processStats.sendFiles.time);

		if (!done || process.exitCode != 0 || !m_showCreateWriteColors || (TimeToMs(writeFilesTime, m_traceView.frequency) < 300 && TimeToMs(process.processStats.createFile.time, m_traceView.frequency) < 300))
		{
			if (writingBitmap)
				rect.right = 256;
			clampRect(rect);
			FillRect(hdc, &rect, brush);
			return;
		}

		double duration = double(process.stop - process.start);

		RECT main = rect;
		int width = rect.right - rect.left;

		double recvPart = (double(ConvertTime(m_traceView, process.processStats.createFile.time)) / duration);
		if (int headSize = int(recvPart * width))
		{
			UBA_ASSERT(headSize > 0);
			main.left += headSize;
			RECT r2 = rect;
			r2.right = r2.left + headSize;
			clampRect(r2);
			if (r2.left != r2.right)
				FillRect(hdc, &r2, m_processBrushes[selected].recv);
		}

		double sendPart = (double(ConvertTime(m_traceView, writeFilesTime)) / duration);
		if (int tailSize = int(sendPart * width))
		{
			UBA_ASSERT(tailSize > 0);
			main.right -= tailSize;
			RECT r2 = rect;
			r2.left = r2.right - tailSize;
			clampRect(r2);
			if (r2.left != r2.right)
				FillRect(hdc, &r2, m_processBrushes[selected].send);
		}

		clampRect(main);
		if (main.left != main.right)
			FillRect(hdc, &main, brush);

		//clampRect(rect);
		/*
		if (!process.updates.empty())
		{
			shouldDrawText = false;
			int prevUpdateX = rect.left;
			for (auto& update : process.updates)
			{
				int updateX = int(posX + TimeToS(update.time) * scaleX) - 1;

				RECT textRect{ prevUpdateX + 5, rect.top, updateX, rect.bottom };
				DrawTextW(hdc, update.reason.c_str(), u32(update.reason.size()), &textRect, DT_SINGLELINE);

				SelectObject(hdc, m_processUpdatePen);
				MoveToEx(hdc, updateX, rect.top, NULL);
				LineTo(hdc, updateX, rect.bottom - 1);
				prevUpdateX = updateX;
			}
		}
		*/
	}

	void Visualizer::PaintTimeline(HDC hdc, const RECT& clientRect)
	{
		float boxHeight = float(RawBoxHeight) * m_zoomValue;
		int posY = m_contentHeight - int(boxHeight) - 14;
		float timeScale = (m_horizontalScaleValue*m_zoomValue)*50.0f;
		int top = Min(posY, int(clientRect.bottom - SessionStepY - 10));
			
		float startOffset = ((m_scrollPosX/timeScale) - int(m_scrollPosX/timeScale)) * timeScale;
		int index = -int(startOffset/timeScale);
			
		int number = -int(float(m_scrollPosX)/timeScale);

		int textStepSize = int((5.0f / timeScale) + 1) * 5;
		if (textStepSize > 150)
			textStepSize = 600;
		else if (textStepSize > 120)
			textStepSize = 300;
		else if (textStepSize > 90)
			textStepSize = 240;
		else if (textStepSize > 45)
			textStepSize = 120;
		else if (textStepSize > 30)
			textStepSize = 60;
		else if (textStepSize > 10)
			textStepSize = 30;

		int lineStepSize = textStepSize / 5;

		RECT progressRect = clientRect;
		progressRect.left += ProgressRectLeft;

		SelectObject(hdc, m_textPen);

		while (true)
		{
			int pos = progressRect.left + int(startOffset + index*timeScale);
			if (pos >= clientRect.right)
				break;

			int lineBottom = top + 5;
			if (!(number % textStepSize))
			{
				bool shouldDraw = true;
				int seconds = number;
				StringBuffer<> buffer;
				if (seconds >= 60)
				{
					int min = seconds / 60;
					seconds -= min * 60;
					if (!seconds)
					{
						buffer.Appendf(L"%um", min);
						lineBottom += 4;
					}
				}
				if (!number || seconds)
					buffer.Appendf(L"%u", seconds);

				if (shouldDraw)
				{
					RECT textRect;
					textRect.top = top + 8;
					textRect.bottom = textRect.top + 15;
					textRect.right = pos + 20;
					textRect.left = pos - 20;
					DrawTextW(hdc, buffer.data, buffer.count, &textRect, DT_SINGLELINE | DT_CENTER);
				}
			}
			if (!(number % lineStepSize))
			{
				MoveToEx(hdc, pos, top, NULL);
				LineTo(hdc, pos, lineBottom);
			}

			++number;
			++index;
		}

		MoveToEx(hdc, m_contentWidth, top - 25, NULL);
		LineTo(hdc, m_contentWidth, top);

		/*
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		ScreenToClient(m_hwnd, &cursorPos);
		RECT lineRect;
		lineRect.left = cursorPos.x;
		lineRect.right= cursorPos.x + 1;
		lineRect.top = top;
		lineRect.bottom = top + 15;
		FillRect(hdc, &lineRect, m_lineBrush);
		*/
	}

	void Visualizer::PaintDetailedStats(int& posY, const RECT& progressRect, TraceView::Session& session, bool isRemote, u64 playTime, const DrawTextFunc& drawTextFunc)
	{
		int stepY = FontHeight;
		int startPosY = posY;
		int posX = progressRect.left + 5;
		RECT textRect;
		textRect.top = posY;
		textRect.bottom = posY + 20;
		textRect.left = posX;
		textRect.right = posX + 1000;

		auto drawText = [&](const wchar_t* format, ...)
			{
				textRect.top = posY;
				textRect.bottom = posY + stepY;
				posY += stepY;
				StringBuffer<> str;
				va_list arg;
				va_start(arg, format);
				str.Append(format, arg);
				va_end(arg);
				drawTextFunc(str, textRect);
			};

		if (isRemote)
		{
			drawText(L"Finished Processes: %u", session.processExitedCount);
			drawText(L"Active Processes: %u", session.processActiveCount);

			if (!session.updates.empty())
			{
				auto& u = session.updates.back();
				u64 sendPerS = 0;
				u64 recvPerS = 0;
				if (float duration = TimeToS(u.time - session.prevUpdateTime))
				{
					sendPerS = u64((u.send - session.prevSend) / duration);
					recvPerS = u64((u.recv - session.prevRecv) / duration);
				}
				drawText(L"ClientId: %u  TcpCount: %u", session.clientUid.data1, u.connectionCount);
				drawText(L"Recv: %ls (%s/s)", BytesToText(u.recv), BytesToText(recvPerS));
				drawText(L"Send: %ls (%s/s)", BytesToText(u.send), BytesToText(sendPerS));
			}

			if (session.disconnectTime == ~u64(0))
			{
				if (session.proxyCreated)
					drawText(L"Proxy(HOSTED): %ls", session.proxyName.c_str());
				else if (!session.proxyName.empty())
					drawText(L"Proxy: %ls", session.proxyName.c_str());
				else
					drawText(L"Proxy: None");
			}
			int posY1 = posY;

			int fileWidth = 700;

			auto drawFiles = [&](const wchar_t* fileType, Vector<TraceView::FileTransfer>& files, u64 bytes, u32& maxVisibleFiles)
				{
					textRect.left = posX;
					textRect.right = posX + fileWidth;
					drawText(L"%ls Files: %u (%s)", fileType, u32(files.size()), BytesToText(bytes));
					u32 fileCount = 0;
					for (auto rit = files.rbegin(), rend = files.rend(); rit != rend; ++rit)
					{
						TraceView::FileTransfer& file = *rit;
						if (file.stop != ~u64(0))
							continue;
						u64 time = 0;
						if (file.start < playTime)
							time = playTime - file.start;
						drawText(L"%s - %s, (%ls)", file.hint.c_str(), BytesToText(file.size).str, TimeToText(time, true).str);
						if (fileCount++ > 5)
							break;
					}
					posY += stepY * (maxVisibleFiles - fileCount);
					maxVisibleFiles = Max(maxVisibleFiles, fileCount);
				};

			posY = startPosY;
			posX += 150;
			drawFiles(L"Fetched", session.fetchedFiles, session.fetchedFilesBytes, session.maxVisibleFiles);
			int posY2 = posY;
			posY = startPosY;
			posX += fileWidth;
			drawFiles(L"Stored", session.storedFiles, session.storedFilesBytes, session.maxVisibleFiles);
			posY = Max(posY, Max(posY1, posY2));
		}
		else
		{
			drawText(L"Finished Processes: %u (local: %u)", m_traceView.totalProcessExitedCount, session.processExitedCount);
			drawText(L"Active Processes: %u (local: %u)", m_traceView.totalProcessActiveCount, session.processActiveCount);
			drawText(L"Active Helpers: %u", m_traceView.activeSessionCount - 1);

			if (!session.updates.empty())
			{
				auto& u = session.updates.back();
				if (u.send || u.recv)
				{
					u64 sendPerS = 0;
					u64 recvPerS = 0;
					if (float duration = TimeToS(u.time - session.prevUpdateTime))
					{
						sendPerS = u64((u.send - session.prevSend) / duration);
						recvPerS = u64((u.recv - session.prevRecv) / duration);
					}
					drawText(L"Recv: %ls (%s/s)", BytesToText(u.recv), BytesToText(recvPerS));
					drawText(L"Send: %ls (%s/s)", BytesToText(u.send), BytesToText(sendPerS));
				}
			}
		}
	}

	void Visualizer::HitTest(HitTestResult& outResult, const POINT& pos)
	{
		u64 currentTime = m_paused ? m_pauseStart : GetTime();
		u64 playTime = currentTime - m_traceView.startTime - m_pauseTime;
		if (m_replay)
			playTime *= m_replay;

		RECT clientRect;
		GetClientRect(m_hwnd, &clientRect);

		int posY = int(m_scrollPosY);
		float boxHeight = float(RawBoxHeight)*m_zoomValue;
		int stepY = int(boxHeight) + 2;
		float scaleX = 50.0f*m_zoomValue*m_horizontalScaleValue;

		RECT progressRect = clientRect;
		progressRect.left += ProgressRectLeft;
		progressRect.bottom -= 30;

		{
			int top = 5;
			int bottom = SessionStepY - 3;
			int left = progressRect.right - 16;
			int right = progressRect.right - 7;
			for (int i = sizeof_array(m_visibleComponents) - 1; i >= 0; --i)
			{
				if (pos.x >= left && pos.x <= right && pos.y >= top && pos.y <= bottom)
				{
					outResult.buttonSelected = i;
					return;
				}
				left -= 14;
				right -= 14;
			}
		}

		u64 lastStop = 0;

		if (!m_traceView.statusMap.empty())
		{
			posY += 4;
			auto drawStatusText = [&]() { posY = posY + FontHeight + 2; };
			for (auto& kv : m_traceView.statusMap)
				if (!kv.second.name.empty() || !kv.second.text.empty())
					drawStatusText();
			posY += 4;
		}

		TraceView::ProcessLocation& outLocation = outResult.processLocation;

		SessionRec sortedSessions[1024];
		Populate(sortedSessions, m_traceView);

		for (u64 i = 0, e = m_traceView.sessions.size(); i != e; ++i)
		{
			auto& session = *sortedSessions[i].session;

			bool hasUpdates = !session.updates.empty();
			if (!hasUpdates && session.processors.empty())
				continue;

			u32 sessionIndex = sortedSessions[i].index;
			bool isFirst = i == 0;
			if (!isFirst)
				posY += 3;

			if (pos.y >= posY && pos.y < posY + SessionStepY)
			{
				if (pos.x < 500)
				{
					outResult.sessionSelectedIndex = sessionIndex;
					return;
				}
			}

			posY += SessionStepY;

			bool showGraph = m_visibleComponents[ComponentType_SendRecv] || m_visibleComponents[ComponentType_CpuMem];
			if (showGraph && !session.updates.empty())
			{
				if (pos.y >= posY && pos.y < posY + GraphHeight)
				{
					int posX = int(m_scrollPosX) + progressRect.left;
					u64 prevTime = 0;
					u64 prevSend = 0;
					u64 prevRecv = 0;
					int prevX = 100000;
					for (auto& update : session.updates)
					{
						int x = int(posX + TimeToS(update.time) * scaleX);

						if (prevSend > update.send || prevRecv > update.recv)
						{
							prevSend = update.send;
							prevRecv = update.recv;
							prevX = x;
							continue;
						}

						int hitOffset = (prevX - x)/2;
						if (pos.x + hitOffset >= prevX && pos.x + hitOffset <= x)
						{
							double duration = TimeToS(update.time - prevTime);
							outResult.stats.recvBytesPerSecond = u64((update.recv - prevRecv) / duration);
							outResult.stats.sendBytesPerSecond = u64((update.send - prevSend) / duration);
							outResult.stats.ping = update.ping;
							outResult.stats.memAvail = update.memAvail;
							outResult.stats.cpuLoad = update.cpuLoad;
							outResult.stats.memTotal = session.memTotal;
							outResult.statsSelected = true;
							return;
						}

						prevX = x;
						prevTime = update.time;
						prevSend = update.send;
						prevRecv = update.recv;
					}
					posY += GraphHeight;
				}

				posY += GraphHeight;
			}

			if (m_visibleComponents[ComponentType_DetailedData])
			{
				auto drawText = [&](const StringBufferBase& text, RECT& rect)
					{
						//DrawTextW(hdc, text.data, text.count, &rect, DT_SINGLELINE);
						if (pos.x >= rect.left && pos.x < rect.right && pos.y >= rect.top && pos.y < rect.bottom && text.StartsWith(TC("Fetched Files")))
							outResult.fetchedFilesSelected = sessionIndex;
					};
				PaintDetailedStats(posY, progressRect, session, i != 0, playTime, drawText);
			}

			if (m_visibleComponents[ComponentType_Bars])
			{
				u32 processorIndex = 0;
				for (auto& processor : session.processors)
				{
					if (pos.y < progressRect.bottom && posY + stepY >= progressRect.top && posY <= progressRect.bottom && pos.y >= posY-1 && pos.y < posY-1 + stepY)
					{
						u32 processIndex = 0;
						int posX = int(m_scrollPosX) + progressRect.left;
						for (auto& process : processor.processes)
						{
							int left = int(posX + TimeToS(process.start) * scaleX);

							if (left >= progressRect.right)
							{
								++processIndex;
								continue;
							}
							if (left < progressRect.left)
								left = progressRect.left;

							u64 stopTime = process.stop;
							bool done = stopTime != ~u64(0);
							if (!done)
								stopTime = playTime;

							RECT rect;
							rect.left = left;
							rect.right = int(posX + TimeToS(stopTime) * scaleX);

							if (rect.right <= progressRect.left)
							{
								++processIndex;
								continue;
							}
							rect.right = Max(int(rect.right), left + 1);
							rect.top = posY;
							rect.bottom = posY + int(float(18) * m_zoomValue);

							if (pos.x >= rect.left && pos.x <= rect.right)
							{
								outLocation.sessionIndex = sessionIndex;
								outLocation.processorIndex = processorIndex;
								outLocation.processIndex = processIndex;
								outResult.processSelected = true;
								return;
							}
							++processIndex;
						}
					}

					if (!processor.processes.empty())
						lastStop = Max(lastStop, processor.processes.rbegin()->stop);

					posY += stepY;
					++processorIndex;
				}
			}
			else
			{
				for (auto& processor : session.processors)
					if (!processor.processes.empty())
						lastStop = Max(lastStop, processor.processes.rbegin()->stop);
			}

			if (m_visibleComponents[ComponentType_Workers] && isFirst)
			{
				int trackIndex = 0;
				for (auto& workTrack : m_traceView.workTracks)
				{
					if (pos.y < progressRect.bottom && posY + stepY >= progressRect.top && posY <= progressRect.bottom && pos.y >= posY-1 && pos.y < posY-1 + stepY)
					{
						u32 workIndex = 0;
						int posX = int(m_scrollPosX) + progressRect.left;
						for (auto& work : workTrack.records)
						{
							int left = int(posX + TimeToS(work.start) * scaleX);

							if (left >= progressRect.right)
							{
								++workIndex;
								continue;
							}
							if (left < progressRect.left)
								left = progressRect.left;

							u64 stopTime = work.stop;
							bool done = stopTime != ~u64(0);
							if (!done)
								stopTime = playTime;

							RECT rect;
							rect.left = left;
							rect.right = int(posX + TimeToS(stopTime) * scaleX);

							if (rect.right <= progressRect.left)
							{
								++workIndex;
								continue;
							}
							rect.right = Max(int(rect.right), left + 1);
							rect.top = posY;
							rect.bottom = posY + int(float(18) * m_zoomValue);

							if (pos.x >= rect.left && pos.x <= rect.right)
							{
								outResult.workTrack = trackIndex;
								outResult.workIndex = workIndex;
								outResult.workSelected = true;
								return;
							}
							++workIndex;
						}
					}
					++trackIndex;
					posY += stepY;
				}
			}

			if (m_visibleComponents[ComponentType_Timeline])
			{
				int timelineTop = Min(posY, int(progressRect.bottom));
				if (pos.y >= timelineTop && pos.y < timelineTop + 40)
				{
					float timeScale = (m_horizontalScaleValue * m_zoomValue)*50.0f;
					float startOffset = -(m_scrollPosX / timeScale);
					outResult.timelineSelected = startOffset + (pos.x - ProgressRectLeft) / timeScale;
				}
			}

		}

		m_contentWidth = ProgressRectLeft + int(TimeToS(lastStop != ~u64(0) ? lastStop : playTime) * scaleX);
		m_contentHeight = posY - int(m_scrollPosY) + stepY + 14;
	}

	void Visualizer::WriteProcessStats(Logger& out, TraceView::Process& process)
	{
		bool hasStorageStats = true;
		bool hasExited = process.stop != ~u64(0);
		out.Info(L"  %ls", process.description.c_str());
		out.Info(L"  Start:     %ls", TimeToText(process.start, true).str);
		if (hasExited)
			out.Info(L"  Duration:  %ls", TimeToText(process.stop - process.start, true).str);
		if (hasExited && process.exitCode != 0)
			out.Info(L"  ExitCode:  %u", process.exitCode);
		out.Info(L"");

		if (process.stop != ~u64(0))
		{
			out.Info(L"  ----------- Process stats -----------");
			process.processStats.Print(out, m_traceView.frequency);
			if (hasStorageStats)
			{
				out.Info(L"");
				out.Info(L"  ----------- Session stats -----------");
				process.sessionStats.Print(out, m_traceView.frequency);
				out.Info(L"");
				out.Info(L"  ----------- Storage stats -----------");
				process.storageStats.Print(out, m_traceView.frequency);
				out.Info(L"");
				out.Info(L"  ----------- System stats ------------");
				process.systemStats.Print(out, false, m_traceView.frequency);
			}
		}
	}

	void Visualizer::CopyTextToClipboard(const TString& str)
	{
		if (!OpenClipboard(m_hwnd))
			return;
		if (auto hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (str.size() + 1) * sizeof(TCHAR)))
		{
			if (auto lptstrCopy = GlobalLock(hglbCopy))
			{
				memcpy(lptstrCopy, str.data(), (str.size() + 1) * sizeof(TCHAR));
				GlobalUnlock(hglbCopy);
				EmptyClipboard();
				SetClipboardData(CF_UNICODETEXT, hglbCopy);
			}
		}
		CloseClipboard();
	}

	void Visualizer::UnselectAndRedraw()
	{
		if (m_processSelected || m_sessionSelectedIndex != ~0u || m_statsSelected || m_timelineSelected || m_fetchedFilesSelected != ~0u || m_workSelected)
		{
			m_processSelected = false;
			m_sessionSelectedIndex = ~0u;
			m_statsSelected = false;
			m_buttonSelected = ~0u;
			m_timelineSelected = 0;
			m_fetchedFilesSelected = ~0u;
			m_workSelected = false;
			RedrawWindow(m_hwnd, NULL, NULL, RDW_INVALIDATE);
		}
	}

	bool Visualizer::UpdateAutoscroll()
	{
		if (!m_autoScroll)
			return false;

		u64 currentTime = m_paused ? m_pauseStart : GetTime();
		u64 playTime = currentTime - m_traceView.startTime - m_pauseTime;
		if (m_replay)
			playTime *= m_replay;

		RECT rect;
		GetClientRect(m_hwnd, &rect);
		float timeS = TimeToS(playTime);
		m_scrollPosX = Min(0.0f, (float)rect.right - timeS*50.0f*m_horizontalScaleValue*m_zoomValue - ProgressRectLeft);
		return true;
	}

	bool Visualizer::UpdateSelection()
	{
		if (!m_mouseOverWindow || m_middleMouseDown)
			return false;
		POINT pos;
		GetCursorPos(&pos);
		ScreenToClient(m_hwnd, &pos);

		HitTestResult res;
		HitTest(res, pos);
		if (res.processSelected == m_processSelected && res.processLocation == m_processSelectedLocation &&
			res.sessionSelectedIndex == m_sessionSelectedIndex &&
			res.statsSelected == m_statsSelected && memcmp(&res.stats, &m_stats, sizeof(Stats)) == 0 &&
			res.buttonSelected == m_buttonSelected && res.timelineSelected == m_timelineSelected &&
			res.fetchedFilesSelected == m_fetchedFilesSelected &&
			res.workSelected == m_workSelected && res.workTrack == m_workTrack && res.workIndex == m_workIndex)
			return false;
		m_processSelected = res.processSelected;
		m_processSelectedLocation = res.processLocation;
		m_sessionSelectedIndex = res.sessionSelectedIndex;
		m_statsSelected = res.statsSelected;
		m_stats = res.stats;
		m_buttonSelected = res.buttonSelected;
		m_timelineSelected = res.timelineSelected;
		m_fetchedFilesSelected = res.fetchedFilesSelected;
		m_workSelected = res.workSelected;
		m_workTrack = res.workTrack;
		m_workIndex = res.workIndex;
		return true;
	}

	void Visualizer::UpdateScrollbars(bool redraw)
	{
		RECT rect;
		GetClientRect(m_hwnd, &rect);

		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_ALL;
		si.nMin = 0;
		si.nMax = m_contentHeight;
		si.nPage = rect.bottom;
		si.nPos = -int(m_scrollPosY);
		si.nTrackPos = 0;
		SetScrollInfo(m_hwnd, SB_VERT, &si, redraw);

		si.nMax = m_contentWidth;
		si.nPage = rect.right;
		si.nPos = -int(m_scrollPosX);
		SetScrollInfo(m_hwnd, SB_HORZ, &si, redraw);
	}

	LRESULT Visualizer::WinProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		switch (Msg)
		{
		case WM_NEWTRACE:
		{
			if (!m_traceView.finished)
				return 0;
			Reset();
			StringBuffer<> title;
			GetTitlePrefix(title);

			auto g = MakeGuard([&]()
				{
					RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);
					UpdateScrollbars(true);
				});

			if (m_client)
			{
				if (!m_trace.StartReadClient(m_traceView, *m_client))
					return false;
				m_namedTrace.Clear().Append(m_newTraceName);
				title.Appendf(L"Connected to host");
				m_traceView.finished = false;
			}
			else if (!m_fileName.IsEmpty())
			{
				if (!m_trace.ReadFile(m_traceView, m_fileName.data, m_replay != 0))
					return false;
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				title.Append(m_fileName);
				m_traceView.finished = m_replay == 0;
			}
			else
			{
				if (!m_trace.StartReadNamed(m_traceView, m_newTraceName.data, true))
					return false;
				m_namedTrace.Clear().Append(m_newTraceName);
				title.Appendf(L"%s (Listening for new sessions on channel '%s')", m_namedTrace.data, m_listenChannel.data);
				m_traceView.finished = false;
			}

			SetWindowTextW(m_hwnd, title.data);
			SetTimer(m_hwnd, 0, 200, NULL);
			return 0;
		}

		case WM_SYSCOMMAND:
			// Don't send this message through DefWindowProc as it will destroy the window 
			// and GetMessage will stay stuck indefinitely.
			if (wParam == SC_CLOSE)
			{
				m_looping = false;
				return 0;
			}
		break;

		case WM_DESTROY:
			m_looping = false;
			//PostQuitMessage(0);
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
		{
			PaintClient([&](HDC hdc, HDC memDC, RECT& rect)
				{
					FillRect(memDC, &rect, m_backgroundBrush);
					PaintAll(memDC, rect);
					BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, 0, SRCCOPY);
				});
			break;
		}
		case WM_SIZE:
		{
			int height = HIWORD(lParam);
			if (m_contentHeight && m_contentHeight + m_scrollPosY < height)
				m_scrollPosY = float(Min(0, height - m_contentHeight));
			int width = LOWORD(lParam);
			if (m_contentWidth && m_contentWidth + m_scrollPosX < width)
				m_scrollPosX = float(Min(0, width - m_contentWidth));
			UpdateScrollbars(false);
			break;
		}
		case WM_TIMER:
		{
			bool changed = false;
			if (!m_paused)
			{
				u64 timeOffset = (GetTime() - m_startTime - m_pauseTime) * m_replay;
				if (!m_namedTrace.IsEmpty())
					m_trace.UpdateReadNamed(m_traceView, changed);
				else if (!m_fileName.IsEmpty() && m_replay)
					m_trace.UpdateReadFile(m_traceView, timeOffset, changed);
				else if (m_client)
					m_trace.UpdateReadClient(m_traceView, *m_client, changed);
			}

			if (m_traceView.finished)
			{
				m_autoScroll = false;
				KillTimer(m_hwnd, 0);
				UpdateScrollbars(true);
			}

			changed = UpdateAutoscroll() || changed;
			changed = UpdateSelection() || changed;
			if (changed && !IsIconic(m_hwnd))
			{
				UpdateScrollbars(true);

				u64 startTime = GetTime();
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);
				u64 paintTimeMs = TimeToMs(GetTime() - startTime);
				u32 waitTime = u32(Min(paintTimeMs * 5, 200ull));
				if (!m_traceView.finished)
					SetTimer(m_hwnd, 0, waitTime, NULL);
			}
			break;
		}
		case WM_MOUSEWHEEL:
		{
			if (m_middleMouseDown)
				break;

			int delta = GET_WHEEL_DELTA_WPARAM(wParam);// / WHEEL_DELTA;

			RECT r;
			GetClientRect(hWnd, &r);

			SHORT controlState = GetAsyncKeyState(VK_CONTROL);
			if (controlState & (1<<15))
			{
				float newValue = Max(m_zoomValue + float(delta)*0.0005f, 0.05f);
				m_scrollPosY = Min(0.0f, float(m_scrollPosY)*newValue/m_zoomValue);//LOWORD(lParam);
				m_scrollPosX = Min(0.0f, float(m_scrollPosX)*newValue/m_zoomValue);//LOWORD(lParam);

				m_zoomValue = newValue;
			}
			else
			{
				float newValue = m_horizontalScaleValue + m_horizontalScaleValue*float(delta)*0.0006f;
				m_scrollPosX = Min(0.0f, float(m_scrollPosX)*newValue/m_horizontalScaleValue);//LOWORD(lParam);
				m_horizontalScaleValue = newValue;
			}

			UpdateAutoscroll();
			UpdateSelection();

			int minScroll = r.right - m_contentWidth;
			m_scrollPosX = Min(0.0f, Max(m_scrollPosX, float(minScroll)));
			m_scrollPosY = Min(0.0f, Max(m_scrollPosY, float(r.bottom - m_contentHeight)));

			//if (!m_traceView.finished && m_scrollPosX <= minScroll)
			//	m_autoScroll = true;

		
			if (m_showCreateWriteColors)
				for (auto& session : m_traceView.sessions)
					for (auto& processor : session.processors)
						for (auto& process : processor.processes)
							if (TimeToMs(Max(process.processStats.writeFiles.time, process.processStats.sendFiles.time), m_traceView.frequency) >= 300 || TimeToMs(process.processStats.createFile.time, m_traceView.frequency) >= 300)
								process.bitmapDirty = true;

			UpdateScrollbars(true);
			RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
			break;
		}
		case WM_MOUSEMOVE:
		{
			POINTS p = MAKEPOINTS(lParam);
			POINT pos{ p.x, p.y };
			if (m_middleMouseDown)
			{
				RECT r;
				GetClientRect(hWnd, &r);

				if (m_contentHeight <= r.bottom)
					m_scrollPosY = 0;
				else
					m_scrollPosY = Max(Min(m_scrollAtAnchorY + pos.y - m_mouseAnchor.y, 0.0f), float(r.bottom - m_contentHeight));

				if (m_contentWidth <= r.right)
					m_scrollPosX = 0;
				else
				{
					int minScroll = r.right - m_contentWidth;
					m_scrollPosX = Max(Min(m_scrollAtAnchorX + pos.x - m_mouseAnchor.x, 0.0f), float(minScroll));
					if (!m_traceView.finished && m_scrollPosX <= minScroll)
						m_autoScroll = true;
				}
				UpdateScrollbars(true);
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
			}
			else
			{
				if (UpdateSelection())
					RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
				/*
				else
				{
					PaintClient([&](HDC hdc, HDC memDC, RECT& rect)
						{
							RECT timelineRect = rect;
							rect.top = Min(rect.bottom, m_contentHeight) - 28;
							rect.bottom = Min(rect.bottom, rect.top + 28);
							FillRect(memDC, &rect, m_backgroundBrush);
							SetBkMode(memDC, TRANSPARENT);
							SetTextColor(memDC, m_textColor);
							SelectObject(memDC, m_font);
							PaintTimeline(memDC, timelineRect);
							BitBlt(hdc, 0, rect.top, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, rect.top, SRCCOPY);
						});
				}
				*/
			}

			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			TrackMouseEvent(&tme);
			m_mouseOverWindow = true;
			break;
		}
		case WM_MOUSELEAVE:
			m_mouseOverWindow = false;
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_CANCEL;
			tme.hwndTrack = hWnd;
			TrackMouseEvent(&tme);

			if (!m_showPopup)
				UnselectAndRedraw();
			break;

		case WM_MBUTTONDOWN:
		{
			m_processSelected = false;
			m_sessionSelectedIndex = ~0u;
			m_statsSelected = false;
			m_buttonSelected = ~0u;
			m_timelineSelected = 0;
			m_fetchedFilesSelected = ~0u;
			m_workSelected = false;
			m_autoScroll = false;
			POINTS p = MAKEPOINTS(lParam);
			m_mouseAnchor = {p.x, p.y};
			m_scrollAtAnchorX = m_scrollPosX;
			m_scrollAtAnchorY = m_scrollPosY;
			m_middleMouseDown = true;
			SetCapture(hWnd);
			RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
			break;
		}

		case WM_MOUSEACTIVATE:
		{
			if (LOWORD(lParam) == HTCLIENT)
				return MA_ACTIVATEANDEAT;
			break;
		}

		case WM_LBUTTONDOWN:
		{
			if (m_buttonSelected != ~0u)
			{
				m_visibleComponents[m_buttonSelected] = !m_visibleComponents[m_buttonSelected];
				HitTestResult res;
				HitTest(res, { -1, -1 });
				UpdateScrollbars(true);
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
			}
			else if (m_timelineSelected)
			{
				if (!m_fileName.IsEmpty()) // Only works for files right now
				{
					Reset();
					if (!m_trace.ReadFile(m_traceView, m_fileName.data, true))
						return false;
					bool changed;
					u64 time = MsToTime(u64(m_timelineSelected * 1000.0));
					m_traceView.finished = false;
					m_trace.UpdateReadFile(m_traceView, time, changed);
					m_pauseStart = m_startTime + time;
					if (m_paused || !m_replay)
					{
						m_pauseTime = 0;
						m_paused = true;
					}
					else
					{
						m_pauseTime = GetTime() - m_pauseStart;
					}

					HitTestResult res;
					HitTest(res, { -1, -1 });
					UpdateScrollbars(true);
					RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				}
			}
			break;
		}

		case WM_RBUTTONUP:
		{
			POINT point;
			point.x = LOWORD(lParam);
			point.y = HIWORD(lParam);

			HMENU hMenu = CreatePopupMenu();
			ClientToScreen(hWnd, &point);

			AppendMenuW(hMenu, MF_STRING, Popup_ShowText, m_showText ? L"&Hide Process text" : L"&Show Process text");
			AppendMenuW(hMenu, MF_STRING, Popup_ShowCreateWriteColors, m_showCreateWriteColors ? L"&Hide Create/Write colors" : L"&Show Create/Write colors");
			AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

			if (m_sessionSelectedIndex != ~0u)
			{
				AppendMenuW(hMenu, MF_STRING, Popup_CopySessionInfo, L"&Copy Session Info");
				AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
			}
			else if (m_processSelected)
			{
				TraceView::Process& process = *m_traceView.GetProcess(m_processSelectedLocation);

				AppendMenuW(hMenu, MF_STRING, Popup_CopyProcessInfo, L"&Copy Process Info");
				if (!process.logLines.empty())
					AppendMenuW(hMenu, MF_STRING, Popup_CopyProcessLog, L"Copy Process &Log");
				AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
			}
			if (m_fileName.data)
			{
				if (!m_replay)
					AppendMenuW(hMenu, MF_STRING, Popup_Replay, L"&Replay Trace");
				else
				{
					if (m_paused)
						AppendMenuW(hMenu, MF_STRING, Popup_Play, L"&Play");
					else
						AppendMenuW(hMenu, MF_STRING, Popup_Pause, L"&Pause");
					AppendMenuW(hMenu, MF_STRING, Popup_JumpToEnd, L"&Jump To End");
				}
			}

			AppendMenuW(hMenu, MF_STRING, Popup_SaveAs, L"&Save Trace");
			AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenuW(hMenu, MF_STRING, Popup_Quit, L"&Quit");
			m_showPopup = true;
			switch (TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL))
			{
			case Popup_SaveAs:
			{
				OPENFILENAME ofn;       // common dialog box structure
				TCHAR szFile[260] = { 0 };       // if using TCHAR macros

				// Initialize OPENFILENAME
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof(szFile);
				ofn.lpstrDefExt = TC("uba");
				ofn.lpstrFilter = TC("Uba\0*.uba\0All\0*.*\0");
				ofn.nFilterIndex = 1;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				//ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
				if (GetSaveFileName(&ofn))
					m_trace.SaveAs(ofn.lpstrFile);
				break;
			}
			case Popup_ShowText:
				m_showText = !m_showText;
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				break;
			case Popup_ShowCreateWriteColors:
				m_showCreateWriteColors = !m_showCreateWriteColors;
				for (auto& session : m_traceView.sessions)
					for (auto& processor : session.processors)
						for (auto& process : processor.processes)
								process.bitmapDirty = true;
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				break;

			case Popup_Replay:
				m_replay = 1;
				PostMessage(m_hwnd, WM_NEWTRACE, 0, 0);
				break;

			case Popup_Play:
				Pause(false);
				break;

			case Popup_Pause:
				Pause(true);
				break;

			case Popup_JumpToEnd:
				m_traceView.finished = true;
				m_replay = 0;
				PostMessage(m_hwnd, WM_NEWTRACE, 0, 0);
				break;

			case Popup_Quit: // Quit
				m_looping = false;
				break;

			case Popup_CopySessionInfo:
			{
				TString str;
				auto& session = m_traceView.sessions[m_sessionSelectedIndex];
				str.append(session.name).append(TC("\n"));
				for (auto& line : session.summary)
					str.append(line).append(TC("\n"));
				CopyTextToClipboard(str);
				break;
			}

			case Popup_CopyProcessInfo:
			{
				TString str;
				WriteTextLogger logger(str);
				TraceView::Process& process = *m_traceView.GetProcess(m_processSelectedLocation);
				WriteProcessStats(logger, process);
				CopyTextToClipboard(str);
				break;
			}
			case Popup_CopyProcessLog:
			{
				TString str;
				TraceView::Process& process = *m_traceView.GetProcess(m_processSelectedLocation);
				bool isFirst = true;
				for (auto line : process.logLines)
				{
					if (!isFirst)
						str += '\n';
					isFirst = false;
					str += line.text;
				}
				CopyTextToClipboard(str);
				break;
			}
			}

			DestroyMenu(hMenu);
			m_showPopup = false;
			UnselectAndRedraw();
			break;
		}

		case WM_MBUTTONUP:
		{
			ReleaseCapture();
			m_middleMouseDown = false;
			if (UpdateSelection())
				RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
			//m_processSelected = false;
			break;
		}
		case WM_KEYDOWN:
		{
			if (wParam == VK_SPACE)
				Pause(!m_paused);
			break;
		}
		case WM_VSCROLL:
			{
				RECT r;
				GetClientRect(hWnd, &r);
				float oldScrollY = m_scrollPosY;
				switch (LOWORD(wParam))
				{
				case SB_THUMBTRACK:
				case SB_THUMBPOSITION:
					m_scrollPosY = -float(HIWORD(wParam));
					break;
				case SB_PAGEDOWN:
					m_scrollPosY = m_scrollPosY - r.bottom;
					break;
				case SB_PAGEUP:
					m_scrollPosY = m_scrollPosY + r.bottom;
					break;
				case SB_LINEDOWN:
					m_scrollPosY -= 30;
					break;
				case SB_LINEUP:
					m_scrollPosY += 30;
					break;
				}
				m_scrollPosY = Min(Max(m_scrollPosY, float(r.bottom - m_contentHeight)), 0.0f);

				if (oldScrollY != m_scrollPosY)
				{
					UpdateScrollbars(true);
					RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
				}
				return 0;
			}
		case WM_HSCROLL:
			{
				RECT r;
				GetClientRect(hWnd, &r);
				float oldScrollX = m_scrollPosX;
				bool autoScroll = false;

				switch (LOWORD(wParam))
				{
				case SB_THUMBTRACK:
					m_scrollPosX = -float(HIWORD(wParam));
					if (m_contentWidthWhenThumbTrack == 0)
						m_contentWidthWhenThumbTrack = m_contentWidth;
					break;
				case SB_THUMBPOSITION:
					autoScroll = m_contentWidthWhenThumbTrack - r.right <= HIWORD(wParam) + 10;
					m_contentWidthWhenThumbTrack = 0;
					m_scrollPosX = -float(HIWORD(wParam));
					break;
				case SB_PAGEDOWN:
					m_scrollPosX = m_scrollPosX - r.right;
					break;
				case SB_PAGEUP:
					m_scrollPosX = m_scrollPosX + r.right;
					break;
				case SB_LINEDOWN:
					m_scrollPosX -= 30;
					break;
				case SB_LINEUP:
					m_scrollPosX += 30;
					break;
				case SB_ENDSCROLL:
					return 0;
				}

				int minScroll = r.right - m_contentWidth;
				m_autoScroll = !m_traceView.finished && (m_scrollPosX <= minScroll || autoScroll);
				m_scrollPosX = Min(Max(m_scrollPosX, float(r.right - m_contentWidth)), 0.0f);

				if (oldScrollX != m_scrollPosX)
				{
					UpdateScrollbars(true);
					RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
				}
				return 0;
			}
		}
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	LRESULT CALLBACK Visualizer::StaticWinProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		auto thisPtr = (Visualizer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (!thisPtr && Msg == WM_CREATE)
		{
			thisPtr = (Visualizer*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
		}
		if (thisPtr && hWnd == thisPtr->m_hwnd)
			return thisPtr->WinProc(hWnd, Msg, wParam, lParam);
		else
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
}