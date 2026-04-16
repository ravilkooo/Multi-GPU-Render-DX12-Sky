#pragma once

#include "GDevice.h"
#include "GCommandList.h"
#include "ComputePSO.h"
#include "GraphicPSO.h"
#include "GTexture.h"
#include "GDescriptor.h"
#include "GCrossAdapterResource.h"

#include "SkyAtmosphereBuffersData.h"

using namespace DirectX::SimpleMath;

using namespace PEPEngine;
using namespace Graphics;

namespace Atmosphere
{
    class SkyAtmosphereResources;

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

	enum class CBSlots : UINT {
		Common = 0u, Atmosphere, Count
	};

	enum class TextureSlots : UINT {
		Transmittance = 0u, Multiscat, SkyView, Aerial, Shadow, Depth, TerrainRender, Count
	};

	enum class UavSlots : UINT {
		Transmittance = 0u, Multiscat, SkyView, Aerial, RayMarching, Count
	};

	class SkyAtmosphereCrossResources
	{
		// From Prime to Second
        std::shared_ptr<GCrossAdapterResource> mTerrainRenderTarget;
		std::shared_ptr<GCrossAdapterResource> mDepthMap;

        // From Second to Prime
		std::shared_ptr<GCrossAdapterResource> mRayMarchingResult;
		std::shared_ptr<GCrossAdapterResource> mTransmittanceLut;

	public:
		void Initialize(const SkyAtmosphereResources& Resources, const std::shared_ptr<GDevice>& primeDevice, const std::shared_ptr<GDevice>& secondDevice);

		void OnResize(uint32_t width, uint32_t height) const;

		const GCrossAdapterResource& GetTerrainRenderTarget() const { return *mTerrainRenderTarget; }
		const GCrossAdapterResource& GetDepthMap() const { return *mDepthMap; }

		const GCrossAdapterResource& GetRayMarchingResult() const { return *mRayMarchingResult; }
		const GCrossAdapterResource& GetTransmittanceLut() const { return *mTransmittanceLut; }
	};

    class SkyAtmosphereResources
	{
        std::shared_ptr<GDevice> mDevice;
        std::shared_ptr<GRootSignature> mRootSignature;

        custom_unordered_map<std::string, std::shared_ptr<GShader>> mAtmosphereShaders =
            MemoryAllocator::CreateUnorderedMap<std::string, std::shared_ptr<GShader>>();

		std::shared_ptr<ComputePSO> mComputeTransmittancePSO;
		std::shared_ptr<ComputePSO> mComputeMultiscatPSO;
		std::shared_ptr<ComputePSO> mComputeSkyviewPSO;
		std::shared_ptr<ComputePSO> mComputeAerialPSO;
		std::shared_ptr<ComputePSO> mComputeRaymarchPSO;

        std::shared_ptr<GraphicPSO> mTerrainPSO;
		
        std::shared_ptr<GTexture> mTerrainRenderTarget;
		GDescriptor mTerrainRenderTargetSRV;
		GDescriptor mTerrainRenderTargetRTV;

        std::shared_ptr<GTexture> mDepthMap;
		GDescriptor mDepthMapSRV;
		GDescriptor mDepthMapDSV;

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

		static constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		static constexpr DXGI_FORMAT DepthMapFormat = DXGI_FORMAT_R32_TYPELESS;

		UINT mScreenWidth = 1920;
		UINT mScreenHeight = 1080;

        /*
        * Terrain stuff
        */

		std::shared_ptr<GTexture> mHeightMapTex;
		GDescriptor mHeightMapTexSRV;

		uint32_t mTerrainResolution = 512u;
		D3D12_VIEWPORT mTerrainViewport{};
		D3D12_RECT mTerrainScissorRect{};

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

	public:

        SkyAtmosphereResources();
        void Initialize(const std::shared_ptr<GDevice>& device,
			UINT screenWidth = 1920, UINT screenHeight = 1080, uint32_t terrainResolution = 512u);

		const GRootSignature& GetRootSignature() const { return *mRootSignature.get(); }
		const GraphicPSO& GetTerrainPSO() const { return *mTerrainPSO.get(); }

		const ComputePSO& GetComputeTransmittancePSO() const { return *mComputeTransmittancePSO.get(); }
        const ComputePSO& GetComputeMultiscatPSO() const { return *mComputeMultiscatPSO.get(); }
        const ComputePSO& GetComputeSkyviewPSO() const { return *mComputeSkyviewPSO.get(); }
        const ComputePSO& GetComputeAerialPSO() const { return *mComputeAerialPSO.get(); }
        const ComputePSO& GetComputeRaymarchPSO() const { return *mComputeRaymarchPSO.get(); }

		// std::shared_ptr<GShader> GetAtmosphereShaders(std::string shaderName) { return mAtmosphereShaders[shaderName]; }
        // ComputePSO& GetComputePSO(std::string PsoName) const { return *mAtmospherePSOs[PsoName].get(); }

		const GTexture& GetTerrainRender() const { return *mTerrainRenderTarget.get(); }
        const GTexture& GetDepthMap() const { return *mDepthMap.get(); }
        const GTexture& GetTransmittanceLut() const { return *mTransmittanceLut.get(); }
        const GTexture& GetMultiScatLut() const { return *mMultiScatLut.get(); }
        const GTexture& GetSkyViewLut() const { return *mSkyViewLut.get(); }
        const GTexture& GetAerialPerpspectiveLut() const { return *mAerialPerpspectiveLut.get(); }
        const GTexture& GetRayMarchingResult() const { return *mRayMarchingResult.get(); }
        const GTexture& GetHeightMapTex() const { return *mHeightMapTex.get(); }

        const GDescriptor* GetTerrainRenderSRV() const { return &mTerrainRenderTargetSRV; }
        const GDescriptor* GetDepthMapSRV() const { return &mDepthMapSRV; }
        const GDescriptor* GetTransmittanceLutSRV() const { return &mTransmittanceLutSRV; }
        const GDescriptor* GetMultiScatLutSRV() const { return &mMultiScatLutSRV; }
        const GDescriptor* GetSkyViewLutSRV() const { return &mSkyViewLutSRV; }
        const GDescriptor* GetAerialPerpspectiveLutSRV() const { return &mAerialPerpspectiveLutSRV; }
        const GDescriptor* GetRayMarchingResultSRV() const { return &mRayMarchingResultSRV; }
        const GDescriptor* GetHeightMapTexSRV() const { return &mHeightMapTexSRV; }

        const GDescriptor* GetTerrainRenderRTV() const { return &mTerrainRenderTargetRTV; }
        const GDescriptor* GetDepthMapDSV() const { return &mDepthMapDSV; }
        const GDescriptor* GetTransmittanceLutUAV() const { return &mTransmittanceLutUAV; }
        const GDescriptor* GetMultiScatLutUAV() const { return &mMultiScatLutUAV; }
        const GDescriptor* GetSkyViewLutUAV() const { return &mSkyViewLutUAV; }
        const GDescriptor* GetAerialPerpspectiveLutUAV() const { return &mAerialPerpspectiveLutUAV; }
        const GDescriptor* GetRayMarchingResultUAV() const { return &mRayMarchingResultUAV; }

        const D3D12_VIEWPORT GetTerrainViewport() const { return mTerrainViewport; }
        const D3D12_RECT GetTerrainScissorRect() const { return mTerrainScissorRect; }
	};

	class SkyAtmosphere
	{
		AtmosphereInfo mAtmosphereInfos;
		LookUpTablesInfo mLutInfos;

        /*
        * Prime/second resources
        */
        SkyAtmosphereResources mPrimeResources;
        SkyAtmosphereResources mSecondResources;
        /*
        * Shared resources
		*/
        SkyAtmosphereCrossResources mCrossResources;

	public:

		UINT mScreenWidth = 1920;
		UINT mScreenHeight = 1080;
		uint32_t mTerrainResolution = 512u;

        Matrix mShadowmapViewProjMat;
        Matrix mViewMat;
        Matrix mProjMat;
        Matrix mViewProjMat;
        Vector3 mCamPosFinal;
        Vector3 mViewDir;
        Vector3 mSunDir;

        int viewRayMarchMinSPP = 4;
		int viewRayMarchMaxSPP = 14;

		Vector3 mTerrainPos = Vector3(-1.02f, -0.33f, -2.38f);

        AtmosphereConstants mAtmosphereConstants;
        AtmosphereCommonConstants mCommonConstanants;

        SkyAtmosphere();
        void Initialize(const std::shared_ptr<GDevice>& primeDevice, const std::shared_ptr<GDevice>& secondDevice,
			UINT screenWidth = 1920, UINT screenHeight = 1080, uint32_t terrainResolution = 512u);

        void OnResize(const UINT newScreenWidth, const UINT newScreenHeight);

        void InitAtmosphereData();

		const SkyAtmosphereResources& GetPrimeResources() { return mPrimeResources; }
        const SkyAtmosphereResources& GetSecondResource() { return mSecondResources; }
		const SkyAtmosphereCrossResources& GetCrossResources() { return mCrossResources; }

		void UpdateSkyAtmosphereBuffer();

		void ComputeAtmosphere(const std::shared_ptr<GCommandList>& cmdList,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB,
            const SkyAtmosphereResources& Resources);


		void PopulateTerrainCommands(const std::shared_ptr<GCommandList>& cmdList,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB,
			const SkyAtmosphereResources& Resources);

		void PopulateTransmittanceLutCommands(const std::shared_ptr<GCommandList>& cmdList,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
			const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB,
			const SkyAtmosphereResources& Resources);

		void PopulateMultiScatLutCommands(const std::shared_ptr<GCommandList>& cmdList,
			const SkyAtmosphereResources& Resources);
		void PopulateSkyViewLutCommands(const std::shared_ptr<GCommandList>& cmdList,
			const SkyAtmosphereResources& Resources);
		void PopulateAerialPerspectiveCommands(const std::shared_ptr<GCommandList>& cmdList,
			const SkyAtmosphereResources& Resources);
		void PopulateRayMarchingCommands(const std::shared_ptr<GCommandList>& cmdList,
			const SkyAtmosphereResources& Resources);

    };

}