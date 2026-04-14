#pragma once

#include "GDevice.h"
#include "GCommandList.h"
#include "ComputePSO.h"
#include "GraphicPSO.h"
#include "GTexture.h"
#include "GDescriptor.h"

#include "SkyAtmosphereBuffersData.h"

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

    class SkyAtmosphere
    {
        std::shared_ptr<GDevice> mDevice;
        std::shared_ptr<GDevice> mSecondDevice;
        std::shared_ptr<GRootSignature> mRootSignature;

        custom_unordered_map<std::string, std::shared_ptr<GShader>> mAtmosphereShaders =
            MemoryAllocator::CreateUnorderedMap<std::string, std::shared_ptr<GShader>>();
        custom_unordered_map<std::string, std::shared_ptr<ComputePSO>> mAtmospherePSOs =
            MemoryAllocator::CreateUnorderedMap<std::string, std::shared_ptr<ComputePSO>>();

        std::shared_ptr<GraphicPSO> mTerrainPSO;
		
        std::shared_ptr<GTexture> terrainRenderTarget;
		GDescriptor terrainRenderTargetSRV;
		GDescriptor terrainRenderTargetRTV;

        std::shared_ptr<GTexture> depthMap;
		GDescriptor depthMapSRV;
		GDescriptor depthMapDSV;

        std::shared_ptr<GTexture> mTransmittanceLut;
        GDescriptor mTransmittanceLutUAV;
        GDescriptor mTransmittanceLutSRV;

        std::shared_ptr<GTexture> mMultiScatLut;
        GDescriptor mMultiScatLutUAV;
        GDescriptor mMultiScatLutSRV;

        std::shared_ptr<GTexture> mSkyViewLut;
        GDescriptor mSkyViewLutUAV;
        GDescriptor mSkyViewLutSRV;

        std::shared_ptr<GTexture> mAerialPerpspectiveLut;
        GDescriptor mAerialPerpspectiveLutUAV;
        GDescriptor mAerialPerpspectiveLutSRV;

        std::shared_ptr<GTexture> mRayMarchingResult;
        GDescriptor mRayMarchingResultUAV;
        GDescriptor mRayMarchingResultSRV;

		AtmosphereInfo mAtmosphereInfos;
		LookUpTablesInfo mLutInfos;

        /*
        Terrain stuff
        */

		std::shared_ptr<GTexture> mHeightMapTex;
		GDescriptor mHeightMapTexSrv;

		struct TerrainDataBufferStructure
		{
			Matrix viewProjMat;
			Vector3 terrainPosDelta;
		} mTerrainData;

		std::shared_ptr<ConstantUploadBuffer<TerrainDataBufferStructure>> mTerrainCB;
		Vector3 terrainPos = Vector3(-1.02f, -0.33f, -2.38f);

		uint32_t mTerrainResolution = 512u;
		D3D12_VIEWPORT mTerrainViewport{};
		D3D12_RECT mTerrainScissorRect{};

        enum class CBSlots : UINT {
            Common = 0u, Atmosphere, Count
        };

        enum class TextureSlots : UINT {
            Transmittance = 0u, Multiscat, SkyView, Aerial, Shadow, Depth, Count
        };

        enum class UavSlots : UINT {
            Transmittance = 0u, Multiscat, SkyView, Aerial, RayMarching, Count
		};

        static constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		static constexpr DXGI_FORMAT DepthMapFormat = DXGI_FORMAT_R32_TYPELESS;

	public:
		UINT mScreenWidth = 1920;
		UINT mScreenHeight = 1080;

        Matrix mShadowmapViewProjMat;
        Matrix mViewMat;
        Matrix mProjMat;
        Matrix mViewProjMat;
        Vector3 mCamPosFinal;
        Vector3 mViewDir;
        Vector3 mSunDir;

        int viewRayMarchMinSPP = 4;
        int viewRayMarchMaxSPP = 14;

        AtmosphereCommonConstants mCommonConstanants;

        SkyAtmosphere(const std::shared_ptr<GDevice>& device,
			UINT screenWidth = 1920, UINT screenHeight = 1080, uint32_t terrainResolution = 512u);

        void OnResize(const UINT newScreenWidth, const UINT newScreenHeight);

        void InitAtmosphereData();
        void SetupEarthAtmosphere();

        void InitRootSignatures();

        void LoadShaders();

        void InitPSOs();

        void LoadResources();
        void LoadTerrainResource();
        void LoadTransmittanceLutResource();
        void LoadMultiScatLutResource();
        void LoadSkyViewLutResource();
        void LoadAerialPerpspectiveLutResource();
        void LoadRayMarchingResource();

		void UpdateSkyAtmosphereBuffer(
            const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB);
        void UpdateTerrain();

        std::shared_ptr<GTexture> GetRayMarchTexture();
        GDescriptor* GetRayMarchTextureSrv();
        GDescriptor* GetTerrainRenderSrv();
        std::shared_ptr<GRootSignature> GetRootSignature() { return mRootSignature; }

		const GTexture& GetDepthMap() const { return *depthMap.get(); }
		const GDescriptor* GetDepthMapSRV() const { return &depthMapSRV; }
		const GDescriptor* GetDepthMapDSV() const { return &depthMapDSV; }

        /*
        void InitTransmittanceLutPass();
        void InitMultiScattTexPass();
        void InitSkyViewLutPass();
        void InitAerialPerspectivePass();
        void InitRayMarchingPass();
        */


		void PopulateTerrainCommands(const std::shared_ptr<GCommandList>& cmdList,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB);

		void PopulateTransmittanceLutCommands(const std::shared_ptr<GCommandList>& cmdList,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
            const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB);

        void PopulateMultiScatLutCommands(const std::shared_ptr<GCommandList>& cmdList);
        void PopulateSkyViewLutCommands(const std::shared_ptr<GCommandList>& cmdList);
        void PopulateAerialPerspectiveCommands(const std::shared_ptr<GCommandList>& cmdList);
        void PopulateRayMarchingCommands(const std::shared_ptr<GCommandList>& cmdList);
        /*
        */


    };

}