// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <winrt/windows.graphics.directx.direct3d11.h>
#include <winrt/windows.perception.spatial.h>
#include <mfapi.h>

namespace HLMediaLibrary
{
    struct __declspec(uuid("27ee71f8-e7d3-435c-b394-42058efa6591")) ITransformPriv : ::IUnknown
    {
        STDMETHOD(Initialize)(
            _In_ winrt::com_ptr<IMFSample> const& sourceSample) PURE;
        STDMETHOD(Update)(
            _In_ winrt::com_ptr<IMFSample> const& sourceSample,
            _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem) PURE;
        STDMETHOD(Update)(
            _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem) PURE;
        STDMETHOD(Reset)() PURE;
    };

    struct Transform : winrt::implements<Transform, ITransformPriv>
    {
        static HRESULT Create(
            _In_ winrt::com_ptr<IMFSample> const& sourceSample,
            _COM_Outptr_ ITransformPriv** ppTransform);

        Transform();
        ~Transform() { Reset(); }

        // ITranform
        IFACEMETHOD(Initialize)(_In_ winrt::com_ptr<IMFSample> const& sourceSample);
        IFACEMETHOD(Update)(
            _In_ winrt::com_ptr<IMFSample> const& sourceSample,
            _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);
        IFACEMETHOD(Update)(
            _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);
        IFACEMETHOD(Reset)();

        winrt::Windows::Foundation::Numerics::float4x4 CameraToWorldTransform();
        winrt::Windows::Foundation::Numerics::float4x4 CameraProjectionMatrix();

    private:
        HRESULT UpdateV2(
            _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

        // Convert a duration value from a source tick frequency to a destination tick frequency.
        static inline int64_t SourceDurationTicksToDestDurationTicks(int64_t sourceDurationInTicks, int64_t sourceTicksPerSecond, int64_t destTicksPerSecond)
        {
            int64_t whole = (sourceDurationInTicks / sourceTicksPerSecond) * destTicksPerSecond;                          // 'whole' is rounded down in the target time units.
            int64_t part = (sourceDurationInTicks % sourceTicksPerSecond) * destTicksPerSecond / sourceTicksPerSecond;    // 'part' is the remainder in the target time units.
            return whole + part;
        }

        static inline winrt::Windows::Foundation::TimeSpan TimeSpanFromQpcTicks(int64_t qpcTicks)
        {
            static const int64_t qpcFrequency = []
            {
                LARGE_INTEGER frequency;
                QueryPerformanceFrequency(&frequency);
                return frequency.QuadPart;
            }();

            return winrt::Windows::Foundation::TimeSpan{ SourceDurationTicksToDestDurationTicks(qpcTicks, qpcFrequency, winrt::clock::period::den) / winrt::clock::period::num };
        }

        static inline winrt::Windows::Foundation::Numerics::float4x4 GetProjection(MFPinholeCameraIntrinsics const& cameraIntrinsics)
        {
            // Default camera projection, which has
            // scale up 2.0f to x and y for (-1, -1) to (1, 1) viewport 
            // and taking camera affine
            winrt::Windows::Foundation::Numerics::float4x4 const defaultProjection(
                2.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -2.0f, 0.0f, 0.0f,
                -1.0f, 1.0f, 1.0f, 1.0f,
                0.0f, 0.0f, 0.0f, 0.0f);

            float fx = cameraIntrinsics.IntrinsicModels[0].CameraModel.FocalLength.x / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Width);
            float fy = cameraIntrinsics.IntrinsicModels[0].CameraModel.FocalLength.y / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Height);
            float px = cameraIntrinsics.IntrinsicModels[0].CameraModel.PrincipalPoint.x / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Width);
            float py = cameraIntrinsics.IntrinsicModels[0].CameraModel.PrincipalPoint.y / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Height);

            winrt::Windows::Foundation::Numerics::float4x4 const cameraAffine(
                fx, 0.0f, 0.0f, 0.0f,
                0.0f, -fy, 0.0f, 0.0f,
                -px, -py, -1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);

            return cameraAffine * defaultProjection;
        }

    private:
        boolean m_useNewApi;
        winrt::guid m_currentDynamicNodeId;
        winrt::Windows::Perception::Spatial::SpatialLocator m_locator{ nullptr };
        winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_frameOfReference{ nullptr };

        winrt::Windows::Foundation::Numerics::float4x4 m_cameraToWorldTransform;
        winrt::Windows::Foundation::Numerics::float4x4 m_cameraProjectionMatrix;

        winrt::com_ptr<IMFSample> m_sample;
    };
}
