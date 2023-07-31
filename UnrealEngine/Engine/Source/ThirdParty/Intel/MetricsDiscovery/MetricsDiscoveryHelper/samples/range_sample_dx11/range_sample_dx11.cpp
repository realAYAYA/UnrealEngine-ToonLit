/*
Copyright 2015-2018 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <metrics_discovery/metrics_discovery_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

// impl_dx11.cpp:
bool InitializeGraphics(HWND hwnd);
void FinalizeGraphics();
void Resize(UINT width, UINT height);
void Render(UINT width, UINT height);

bool InitializeMetrics(MDH_Context const& mdhContext, MetricsDiscovery::IConcurrentGroup_1_0* concurrentGroup, MetricsDiscovery::IMetricSet_1_0* metricSet);
void FinalizeMetrics();
MDH_ReportValues* UpdateMetrics();

// ImGui:
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

UINT Width = 0;
UINT Height = 0;

LRESULT CALLBACK WindowProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_SIZE:
        Width  = LOWORD(lParam);
        Height = HIWORD(lParam);
        Resize(Width, Height);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND InitializeWindow(
    char const* title,
    uint32_t width,
    uint32_t height)
{
    auto hInstance = GetModuleHandle(nullptr);

    WNDCLASSEX windowClass = {};
    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = WindowProc;
    windowClass.hInstance     = hInstance;
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = "range_sample_dx11_class";
    RegisterClassEx(&windowClass);

    RECT windowRect = {};
    windowRect.right  = width;
    windowRect.bottom = height;
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    return CreateWindow(
        windowClass.lpszClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);
}

void FinalizeWindow(
    HWND hwnd)
{
    DestroyWindow(hwnd);
    UnregisterClass("range_sample_dx11_class", NULL);
}

bool HandleWindowMessages(
    HWND hwnd,
    int* ret)
{
    MSG msg = {};
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            *ret = (int) msg.wParam;
            return true;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return false;
}

}

int main(
    int argc,
    char** argv)
{
    // Define default parameters

    char const* concurrentGroupName = "OA";
    char const* metricSetName = "RenderBasic";

    // Parse command line

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--mdapi") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "error: --mdapi arguments are missing\n");
            } else {
                concurrentGroupName = argv[++i];
                metricSetName = argv[++i];
                continue;
            }
        }
        else {
            fprintf(stderr, "error: unrecognized argument '%s'\n", argv[i]);
        }

        fprintf(stderr, "usage: range_sample_dx11 [options]\n");
        fprintf(stderr, "options:\n");
        fprintf(stderr, "    --mdapi concurrentGroupName metricSetName\n");
        fprintf(stderr, "e.g.:\n");
        fprintf(stderr, "    range_sample_dx11 --mdapi OA RenderBasic\n");

        return 1;
    }

    MDH_Context mdhContext;
    int ret = 1;

    // Create sample window

    char title[1024] = {};
    snprintf(title, 1024, "MetricsDiscovery range_sample_dx11 - %s::%s",
        concurrentGroupName,
        metricSetName);
    HWND hwnd = InitializeWindow(title, 720, 640);
    if (hwnd == 0) {
        fprintf(stderr, "error: failed to initialize window\n");
        goto finalize;
    }

    // Initialize MetricsDiscoveryHelper context, and search for the specified
    // metrics.
    //
    // NOTE: You need to initialize the MDH_Context instance before creating
    // the device.

    auto result = mdhContext.Initialize();
    if (result != MDH_Context::RESULT_OK) {
        fprintf(stderr, "error: no metrics are available\n");
        goto finalize;
    }

    auto concurrentGroup = MDH_FindConcurrentGroup(mdhContext.MDDevice, concurrentGroupName);
    if (concurrentGroup == nullptr) {
        fprintf(stderr, "error: could not find concurrent group '%s'\n", concurrentGroupName);
        goto finalize;
    }

    auto metricSet = MDH_FindMetricSet(concurrentGroup, metricSetName);
    if (metricSet == nullptr) {
        fprintf(stderr, "error: could not find metric set '%s'\n", metricSetName);
        goto finalize;
    }

    // Initialize the graphics device and resources.

    if (!InitializeGraphics(hwnd)) {
        fprintf(stderr, "error: failed to initialize graphics device\n");
        goto finalize;
    }

    // Initialize the metrics context for the specified metrics.

    if (!InitializeMetrics(mdhContext, concurrentGroup, metricSet)) {
        fprintf(stderr, "error: no metrics are available\n");
        goto finalize;
    }

    // Show window and enter application loop

    ShowWindow(hwnd, SW_SHOW);
    for (;;) {

        // Update metric values based on the last frame rendered.

        auto mdhReportValues = UpdateMetrics();

        // Handle window messages and exit on request

        if (HandleWindowMessages(hwnd, &ret)) {
            break;
        }

        // Specify the GUI (just print all the metrics in this metric set)

        ImGui_ImplDX11_NewFrame();

        if (mdhReportValues != nullptr) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.0f));
            ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiSetCond_Always);
            ImGui::SetNextWindowSize(ImVec2((float) Width, (float) Height), ImGuiSetCond_Always);
            if (ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                for (uint32_t i = 0, N = metricSet->GetParams()->MetricsCount; i < N; ++i) {
                    auto metric = metricSet->GetMetric(i);
                    auto metricParams = metric->GetParams();

                    uint32_t rangeIndex = 0;
                    auto typedValue = mdhReportValues->GetValue(rangeIndex, i);

                    float fValue = 0.f;
                    switch (typedValue.ValueType) {
                    case MetricsDiscovery::VALUE_TYPE_UINT32: fValue = (float) typedValue.ValueUInt32; break;
                    case MetricsDiscovery::VALUE_TYPE_UINT64: fValue = (float) typedValue.ValueUInt64; break;
                    case MetricsDiscovery::VALUE_TYPE_FLOAT:  fValue = typedValue.ValueFloat; break;
                    case MetricsDiscovery::VALUE_TYPE_BOOL:   fValue = typedValue.ValueBool ? 1.f : 0.f; break;
                    }

                    ImGui::Text("%s = %f %s", metricParams->ShortName, fValue, metricParams->MetricResultUnits);
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        // Render this frame

        Render(Width, Height);
    }

    // Cleanup

finalize:
    FinalizeMetrics();
    mdhContext.Finalize();

    FinalizeGraphics();
    FinalizeWindow(hwnd);

    return ret;
}
