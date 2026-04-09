#pragma once

#include "GDevice.h"
#include "GCommandList.h"
#include "ComputePSO.h"
#include "GTexture.h"
#include "GDescriptor.h"

using namespace DirectX::SimpleMath;

using namespace PEPEngine;
using namespace Graphics;

namespace Atmosphere
{
    struct DensityProfileLayer
    {
        float width;
        float exp_term;
        float exp_scale;
        float linear_term;
        float constant_term;
    };

    struct DensityProfile
    {
        DensityProfileLayer layers[2];
    };

    struct AtmosphereInfo
    {
        // Solar irradiance at top of atmosphere (spectral, stored as float3)
        Vector3 solar_irradiance;
        // Sun angular radius (radians)
        float sun_angular_radius;
        // Planet center to bottom/top of atmosphere
        float bottom_radius;
        float top_radius;
        // Density profiles
        DensityProfile rayleigh_density;
        Vector3 rayleigh_scattering; // spectral scattering at bottom
        DensityProfile mie_density;
        Vector3 mie_scattering;      // spectral scattering at bottom
        Vector3 mie_extinction;      // spectral extinction at bottom
        float mie_phase_function_g;            // asymmetry parameter g
        DensityProfile absorption_density;
        Vector3 absorption_extinction;
        Vector3 ground_albedo;
        float mu_s_min; // cosine of max Sun zenith angle to precompute
    };

    struct LookUpTablesInfo
    {
        unsigned int TRANSMITTANCE_TEXTURE_WIDTH = 256;
        unsigned int TRANSMITTANCE_TEXTURE_HEIGHT = 64;

        unsigned int SCATTERING_TEXTURE_R_SIZE = 32;
        unsigned int SCATTERING_TEXTURE_MU_SIZE = 128;
        unsigned int SCATTERING_TEXTURE_MU_S_SIZE = 32;
        unsigned int SCATTERING_TEXTURE_NU_SIZE = 8;

        unsigned int IRRADIANCE_TEXTURE_WIDTH = 64;
        unsigned int IRRADIANCE_TEXTURE_HEIGHT = 16;

        // Derived from above
        unsigned int SCATTERING_TEXTURE_WIDTH = 0xDEADBEEF;
        unsigned int SCATTERING_TEXTURE_HEIGHT = 0xDEADBEEF;
        unsigned int SCATTERING_TEXTURE_DEPTH = 0xDEADBEEF;

        void updateDerivedData()
        {
            SCATTERING_TEXTURE_WIDTH = SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
            SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
            SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;
        }

        LookUpTablesInfo() { updateDerivedData(); }
    };

    const unsigned int MultiScatteringLUTRes = 32;

    struct alignas(16) AtmosphereCB
    {
        DirectX::XMFLOAT3 solar_irradiance;
        float sun_angular_radius;

        DirectX::XMFLOAT3 absorption_extinction;
        float mu_s_min;

        DirectX::XMFLOAT3 rayleigh_scattering;
        float mie_phase_function_g;

        DirectX::XMFLOAT3 mie_scattering;
        float bottom_radius;

        DirectX::XMFLOAT3 mie_extinction;
        float top_radius;

        DirectX::XMFLOAT3 mie_absorption;
        float pad00;

        DirectX::XMFLOAT3 ground_albedo;
        float pad0;

        float rayleigh_density[12];
        float mie_density[12];
        float absorption_density[12];

        int TRANSMITTANCE_TEXTURE_WIDTH;
        int TRANSMITTANCE_TEXTURE_HEIGHT;
        int IRRADIANCE_TEXTURE_WIDTH;
        int IRRADIANCE_TEXTURE_HEIGHT;

        int SCATTERING_TEXTURE_R_SIZE;
        int SCATTERING_TEXTURE_MU_SIZE;
        int SCATTERING_TEXTURE_MU_S_SIZE;
        int SCATTERING_TEXTURE_NU_SIZE;

        DirectX::XMFLOAT3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
        float pad3;
        DirectX::XMFLOAT3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
        float pad4;

        DirectX::XMFLOAT4X4 gSkyViewProjMat;
        DirectX::XMFLOAT4X4 gSkyInvViewProjMat;
        DirectX::XMFLOAT4X4 gSkyInvProjMat;
        DirectX::XMFLOAT4X4 gSkyInvViewMat;
        DirectX::XMFLOAT4X4 gShadowmapViewProjMat;

        DirectX::XMFLOAT3 camera;
        float pad5;
        DirectX::XMFLOAT3 sun_direction;
        float pad6;
        DirectX::XMFLOAT3 view_ray;
        float pad7;

        float MultipleScatteringFactor;
        float MultiScatteringLUTRes;
        float pad9;
        float pad10;
    };

    class SkyAtmosphere
    {
        std::shared_ptr<GDevice> mDevice;
        std::shared_ptr<GRootSignature> mRootSignature;

        custom_unordered_map<std::string, std::shared_ptr<GShader>> mAtmosphereShaders =
            MemoryAllocator::CreateUnorderedMap<std::string, std::shared_ptr<GShader>>();
        custom_unordered_map<std::string, std::shared_ptr<ComputePSO>> mAtmospherePSOs =
            MemoryAllocator::CreateUnorderedMap<std::string, std::shared_ptr<ComputePSO>>();

        std::shared_ptr<GTexture> transmittanceLut;
        GDescriptor transmittanceLutUAV;
        AtmosphereInfo atmosphereInfos;
        LookUpTablesInfo lutInfos;
        std::shared_ptr<ConstantUploadBuffer<AtmosphereCB>> atmosphereCB;

        struct CommonConstantBufferStructure
        {
            Matrix gViewProjMat;

            Vector4 gColor;

            Vector3 gSunIlluminance;
            int gScatteringMaxPathDepth;

            unsigned int gResolution[2];
            float gFrameTimeSec;
            float gTimeSec;

            unsigned int gMouseLastDownPos[2];
            unsigned int gFrameId;
            unsigned int gTerrainResolution;
            float gScreenshotCaptureActive;

            float RayMarchMinMaxSPP[2];
            float pad[2];
        };
        std::shared_ptr<ConstantUploadBuffer<CommonConstantBufferStructure>> commonCB;

    public:
        Matrix mShadowmapViewProjMat;
        Matrix mViewMat;
        Matrix mProjMat;
        Matrix mViewProjMat;
        Vector3 mCamPosFinal;
        Vector3 mViewDir;
        Vector3 mSunDir;

        CommonConstantBufferStructure commonConstanants;

        SkyAtmosphere(const std::shared_ptr<GDevice>& device);

        void SetupEarthAtmosphere();

        void InitRootSignatures();

        void LoadShaders();

        void InitPSOs();

        void LoadResources();

        void UpdateSkyAtmosphereBuffer();

        /*
        void InitTransmittanceLutPass();
        void InitMultiScattTexPass();
        void InitSkyViewLutPass();
        void InitAerialPerspectivePass();
        void InitRayMarchingPass();
        */


        void PopulateTransmittanceLutCommands(const std::shared_ptr<GCommandList>& cmdList);
        /*
        void PopulateMultiScattTexCommands(const std::shared_ptr<GCommandList>& cmdList);
        void PopulateSkyViewLutCommands(const std::shared_ptr<GCommandList>& cmdList);
        void PopulateAerialPerspectiveCommands(const std::shared_ptr<GCommandList>& cmdList);
        void PopulateRayMarchingCommands(const std::shared_ptr<GCommandList>& cmdList);
        */
    };

}