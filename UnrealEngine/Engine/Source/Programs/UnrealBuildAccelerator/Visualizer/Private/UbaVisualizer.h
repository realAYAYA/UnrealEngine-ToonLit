// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetworkClient.h"
#include "UbaThread.h"
#include "UbaTraceReader.h"

namespace uba
{
	class Visualizer
	{
	public:
		Visualizer(Logger& logger);
		~Visualizer();

		void SetTheme(bool dark);

		bool ShowUsingListener(const wchar_t* channelName);
		bool ShowUsingNamedTrace(const wchar_t* namedTrace);
		bool ShowUsingSocket(NetworkBackend& backend, const wchar_t* host, u16 port = DefaultPort);
		bool ShowUsingFile(const wchar_t* fileName, u32 replay);

		bool HasWindow();
		HWND GetHwnd();

	private:
		void Reset();
		void PaintClient(const Function<void(HDC hdc, HDC memDC, RECT& clientRect)>& paintFunc);
		void PaintAll(HDC hdc, const RECT& clientRect);
		void PaintProcessRect(TraceView::Process& process, HDC hdc, RECT rect, const RECT& progressRect, bool selected, bool writingBitmap);
		void PaintTimeline(HDC hdc, const RECT& clientRect);
		using DrawTextFunc = Function<void(const StringBufferBase& text, RECT& rect)>;
		void PaintDetailedStats(int& posY, const RECT& progressRect, TraceView::Session& session, bool isRemote, u64 playTime, const DrawTextFunc& drawTextFunc);

		struct Stats
		{
			u64 recvBytesPerSecond = 0;
			u64 sendBytesPerSecond = 0;
			u64 ping = 0;
			u64 memAvail = 0;
			u64 memTotal = 0;
			float cpuLoad = 0;
		};
		struct HitTestResult
		{
			TraceView::ProcessLocation processLocation;
			bool processSelected = false;
			u32 sessionSelectedIndex = ~0u;
			bool statsSelected = false;
			Stats stats;
			u32 buttonSelected = ~0u;
			float timelineSelected = 0;
			u32 fetchedFilesSelected = ~0u;
			bool workSelected = false;
			u32 workTrack = ~0u;
			u32 workIndex = ~0u;
		};
		void HitTest(HitTestResult& outResult, const POINT& pos);

		void WriteProcessStats(Logger& out, TraceView::Process& process);
		void CopyTextToClipboard(const TString& str);
		void UnselectAndRedraw();
		bool UpdateAutoscroll();
		bool UpdateSelection();
		void UpdateScrollbars(bool redraw);
		void GetTitlePrefix(StringBufferBase& out);
		void ThreadLoop();
		void Pause(bool pause);

		StringBuffer<256> m_namedTrace;
		StringBuffer<256> m_fileName;
		u32 m_replay = 0;
		u64 m_startTime = 0;
		u64 m_pauseTime = 0;

		struct ProcessBrushes
		{
			HBRUSH inProgress = 0;
			HBRUSH success = 0;
			HBRUSH error = 0;
			HBRUSH returned = 0;
			HBRUSH recv = 0;
			HBRUSH send = 0;
		};

		ProcessBrushes m_processBrushes[2]; // Non-selected and selected

		Atomic<bool> m_looping;
		HWND m_hwnd = 0;
		COLORREF m_textColor = {};
		COLORREF m_textWarningColor = {};
		COLORREF m_textErrorColor = {};
		COLORREF m_sendColor = {};
		COLORREF m_recvColor = {};
		COLORREF m_cpuColor = {};
		COLORREF m_memColor = {};
		HBRUSH m_backgroundBrush = 0;
		HBRUSH m_tooltipBackgroundBrush = 0;
		HBRUSH m_workBrush = 0;
		HPEN m_textPen = 0;
		HPEN m_separatorPen = 0;
		HPEN m_sendPen = 0;
		HPEN m_recvPen = 0;
		HPEN m_cpuPen = 0;
		HPEN m_memPen = 0;
		HPEN m_processUpdatePen = 0;
		HPEN m_checkboxPen = 0;
		HFONT m_font = 0;
		HFONT m_popupFont = 0;
		int m_popupFontHeight = 0;
		bool m_useDarkMode = true;
		bool m_isThemeSet = false;
		bool m_showText = true;
		bool m_showCreateWriteColors = true;

		Logger& m_logger;
		NetworkClient* m_client = nullptr;
		TraceReader m_trace;
		TraceView m_traceView;
		
		StringBuffer<256>m_listenChannel;
		StringBuffer<256> m_newTraceName;

		int m_contentWidth = 0;
		int m_contentHeight = 0;

		int m_contentWidthWhenThumbTrack = 0;

		float m_scrollPosX = 0;
		float m_scrollPosY = 0;
		float m_zoomValue = 0.75f;
		float m_horizontalScaleValue = 0.5f;
		bool m_autoScroll = true;
		bool m_paused = false;
		u64 m_pauseStart = 0;

		static constexpr int BitmapCacheHeight = 1024*1024;
		HBITMAP m_lastBitmap = 0;
		int m_lastBitmapOffset = BitmapCacheHeight;

		TraceView::ProcessLocation m_processSelectedLocation;
		bool m_processSelected = false;
		u32 m_sessionSelectedIndex = ~0u;
		bool m_statsSelected = false;
		Stats m_stats;
		u32 m_buttonSelected = ~0u;
		float m_timelineSelected = 0;
		u32 m_fetchedFilesSelected = ~0u;

		bool m_workSelected = false;
		u32 m_workTrack = ~0u;
		u32 m_workIndex = ~0u;
		
		bool m_mouseOverWindow = false;
		bool m_showPopup = false;

		enum ComponentType
		{
			ComponentType_SendRecv,
			ComponentType_CpuMem,
			ComponentType_Bars,
			ComponentType_Timeline,
			ComponentType_DetailedData,
			ComponentType_Workers,
			ComponentType_Count
		};
		bool m_visibleComponents[ComponentType_Count];

		HBITMAP m_cachedBitmap = 0;
		RECT m_cachedBitmapRect = { INT_MIN, INT_MIN, INT_MIN, INT_MIN };

		Vector<HBITMAP> m_textBitmaps;

		POINT m_mouseAnchor = {};
		float m_scrollAtAnchorX = 0;
		float m_scrollAtAnchorY = 0;
		bool m_middleMouseDown = false;

		Thread m_thread;

		LRESULT WinProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
		static LRESULT CALLBACK StaticWinProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	};
}
