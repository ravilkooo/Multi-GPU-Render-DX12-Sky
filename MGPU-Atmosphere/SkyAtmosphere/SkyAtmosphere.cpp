#include "d3dApp.h"
#include "SkyAtmosphere.h"
#include <string>
#include "../CustomShaderLoader/GShaderCustomInclude.h"

namespace Atmosphere
{
    SkyAtmosphere::SkyAtmosphere()
    {

    }

    void SkyAtmosphere::Initialize(const std::shared_ptr<GDevice>& primeDevice,
        const std::shared_ptr<GDevice>& secondDevice,
        UINT screenWidth, UINT screenHeight, uint32_t terrainResolution)
    {
        mScreenWidth = screenWidth;
	    mScreenHeight = screenHeight;
	    mTerrainResolution = terrainResolution;

	    InitAtmosphereData();

        mPrimeResources.Initialize(primeDevice, screenWidth, screenHeight, terrainResolution);
        mSecondResources.Initialize(secondDevice, screenWidth, screenHeight, terrainResolution);

        mCrossResources.Initialize(mPrimeResources, primeDevice, secondDevice);
    }

    void SkyAtmosphere::OnResize(const UINT newScreenWidth, const UINT newScreenHeight)
    {


    }

    void SkyAtmosphere::InitAtmosphereData()
    {
	    // All units in kilometers
	    const float EarthBottomRadius = 6360.0f;
	    const float EarthTopRadius = 6460.0f;   // 100km atmosphere radius, less edge visible and it contain 99.99% of the atmosphere medium https://en.wikipedia.org/wiki/K%C3%A1rm%C3%A1n_line
	    const float EarthRayleighScaleHeight = 8.0f;
	    const float EarthMieScaleHeight = 1.2f;

	    // Sun - This should not be part of the sky model...
	    //info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };
	    mAtmosphereInfos.solar_irradiance = { 1.0f, 1.0f, 1.0f };	// Using a normalise sun illuminance. This is to make sure the LUTs acts as a transfert factor to apply the runtime computed sun irradiance over.
	    mAtmosphereInfos.sun_angular_radius = 0.004675f;

	    // Earth
	    mAtmosphereInfos.bottom_radius = EarthBottomRadius;
	    mAtmosphereInfos.top_radius = EarthTopRadius;
	    mAtmosphereInfos.ground_albedo = { 0.0f, 0.0f, 0.0f };

	    // Raleigh scattering
	    mAtmosphereInfos.rayleigh_density.layers[0] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	    mAtmosphereInfos.rayleigh_density.layers[1] = { 0.0f, 1.0f, -1.0f / EarthRayleighScaleHeight, 0.0f, 0.0f };
	    mAtmosphereInfos.rayleigh_scattering = { 0.005802f, 0.013558f, 0.033100f };		// 1/km

	    // Mie scattering
	    mAtmosphereInfos.mie_density.layers[0] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	    mAtmosphereInfos.mie_density.layers[1] = { 0.0f, 1.0f, -1.0f / EarthMieScaleHeight, 0.0f, 0.0f };
	    mAtmosphereInfos.mie_scattering = { 0.003996f, 0.003996f, 0.003996f };			// 1/km
	    mAtmosphereInfos.mie_extinction = { 0.004440f, 0.004440f, 0.004440f };			// 1/km
	    mAtmosphereInfos.mie_phase_function_g = 0.8f;

	    // Ozone absorption
	    mAtmosphereInfos.absorption_density.layers[0] = { 25.0f, 0.0f, 0.0f, 1.0f / 15.0f, -2.0f / 3.0f };
	    mAtmosphereInfos.absorption_density.layers[1] = { 0.0f, 0.0f, 0.0f, -1.0f / 15.0f, 8.0f / 3.0f };
	    mAtmosphereInfos.absorption_extinction = { 0.000650f, 0.001881f, 0.000085f };	// 1/km

	    const double max_sun_zenith_angle = DirectX::XM_PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
	    mAtmosphereInfos.mu_s_min = (float)cos(max_sun_zenith_angle);


	    mCommonConstanants.gameResolution[0] = 1920;
	    mCommonConstanants.gameResolution[1] = 1080;

	    mCommonConstanants.transmittanceLutResolution[0] = 1920;
        mCommonConstanants.transmittanceLutResolution[1] = 1080;

	    mCommonConstanants.multiScatLutResolution[0] = 32;
	    mCommonConstanants.multiScatLutResolution[1] = 32;

	    mCommonConstanants.skyViewLutResolution[0] = 192;
        mCommonConstanants.skyViewLutResolution[1] = 108;

	    mCommonConstanants.aerialPerpspectiveLutResolution[0] = 32;
	    mCommonConstanants.aerialPerpspectiveLutResolution[1] = 32;
	    mCommonConstanants.aerialPerpspectiveLutResolution[2] = 32;

	    mCommonConstanants.rayMarchingResolution[0] = 1920;
	    mCommonConstanants.rayMarchingResolution[1] = 1080;

	    mCommonConstanants.rayMarchMinMaxSPP[0] = float(viewRayMarchMinSPP);
	    mCommonConstanants.rayMarchMinMaxSPP[1] = float(viewRayMarchMaxSPP);

        mCamPosFinal = Vector3(0.0f, 0.0f, 0.1f);
    }

    void SkyAtmosphereResources::InitRootSignatures()
    {
        // Build a dedicated root signature for the transmittance pass
        mRootSignature = std::make_shared<GRootSignature>();
        mRootSignature->AddConstantBufferParameter((UINT) CBSlots::Common); // commmon_BUFFER
        mRootSignature->AddConstantBufferParameter((UINT) CBSlots::Atmosphere); // SKYATMOSPHERE_BUFFER
        //mRootSignature->AddConstantBufferParameter(0, 1); // LutConstants (Dimensions, InvDimensions)

        CD3DX12_DESCRIPTOR_RANGE srvRange0;
        CD3DX12_DESCRIPTOR_RANGE srvRange1;
        CD3DX12_DESCRIPTOR_RANGE srvRange2;
        CD3DX12_DESCRIPTOR_RANGE srvRange3;
        CD3DX12_DESCRIPTOR_RANGE srvRange4;
        CD3DX12_DESCRIPTOR_RANGE srvRange5;
        CD3DX12_DESCRIPTOR_RANGE srvRange6;
        srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::Transmittance, 0); // t0 transmittance
        srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::Multiscat, 0); // t1 multiscat
        srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::SkyView, 0); // t2, skyview
        srvRange3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::Aerial, 0); // t3, aerial
        srvRange4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::Shadow, 0); // t4, shadow
        srvRange5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::Depth, 0); // t5, depth
        srvRange6.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)TextureSlots::TerrainRender, 0); // t6, terrain
        mRootSignature->AddDescriptorParameter(&srvRange0, 1);
        mRootSignature->AddDescriptorParameter(&srvRange1, 1);
        mRootSignature->AddDescriptorParameter(&srvRange2, 1);
        mRootSignature->AddDescriptorParameter(&srvRange3, 1);
        mRootSignature->AddDescriptorParameter(&srvRange4, 1);
        mRootSignature->AddDescriptorParameter(&srvRange5, 1);
        mRootSignature->AddDescriptorParameter(&srvRange6, 1);

        CD3DX12_DESCRIPTOR_RANGE uavRange0;
        CD3DX12_DESCRIPTOR_RANGE uavRange1;
        CD3DX12_DESCRIPTOR_RANGE uavRange2;
        CD3DX12_DESCRIPTOR_RANGE uavRange3;
        CD3DX12_DESCRIPTOR_RANGE uavRange4;
        uavRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, (UINT)UavSlots::Transmittance, 0); // u0 transmittance output
        uavRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, (UINT)UavSlots::Multiscat, 0); // u1 multiscat output
        uavRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, (UINT)UavSlots::SkyView, 0); // u2 skyview output
        uavRange3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, (UINT)UavSlots::Aerial, 0); // u3 aerial output
        uavRange4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, (UINT)UavSlots::RayMarching, 0); // u4 ray marching output
        mRootSignature->AddDescriptorParameter(&uavRange0, 1);
        mRootSignature->AddDescriptorParameter(&uavRange1, 1);
        mRootSignature->AddDescriptorParameter(&uavRange2, 1);
        mRootSignature->AddDescriptorParameter(&uavRange3, 1);
        mRootSignature->AddDescriptorParameter(&uavRange4, 1);

	    CD3DX12_DESCRIPTOR_RANGE srvHeightMap;
	    srvHeightMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0u, 1u); // t0, space1 Terrain HeightMap
        mRootSignature->AddDescriptorParameter(&srvHeightMap, 1u, D3D12_SHADER_VISIBILITY_VERTEX);

        const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
            0, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
            1, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
            2, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
            0.0f,
            0,
            D3D12_COMPARISON_FUNC_LESS_EQUAL,
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

        const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
            3, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC shadowClamp(
            4, // shaderRegister
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
            0.0f, 1, D3D12_COMPARISON_FUNC_LESS,
            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK); 

        std::array<CD3DX12_STATIC_SAMPLER_DESC, 5> staticSamplers =
        {
            pointClamp, linearClamp, depthMapSam, linearWrap, shadowClamp
        };

        for (auto&& sampler : staticSamplers)
        {
            mRootSignature->AddStaticSampler(sampler);
        }

        mRootSignature->Initialize(mDevice);
    }

    void SkyAtmosphereResources::LoadShaders()
    {
        /*
        Graphics shaders
        */

	    std::vector<std::string> dirs{
		    "Shaders",
		    "Shaders/SkyAtmosphere",
		    "Shaders/Terrain"
	    };

        mAtmosphereShaders["terrainVS"] = std::move(std::make_shared<GShaderCustomInclude>(2u, dirs,
            L"Shaders\\Terrain\\Terrain.hlsl", VertexShader, nullptr, "TerrainVS", "vs_5_1"));

        mAtmosphereShaders["terrainPS"] = std::move(std::make_shared<GShaderCustomInclude>(2u, dirs,
		    L"Shaders\\Terrain\\Terrain.hlsl", PixelShader, nullptr, "TerrainPS", "ps_5_1"));

        /*
        Compute shaders
        */

        mAtmosphereShaders["brunetonTransmittanceCS"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\Bruneton\\BrunetonTransmittanceLut.hlsl", ComputeShader, nullptr, "TransmittanceLutCS_Bruneton", "cs_5_1"));

        mAtmosphereShaders["transmittanceCS"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, nullptr, "ComputeTransmittanceLutCS", "cs_5_1"));

        mAtmosphereShaders["multiscatCS"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, nullptr, "ComputeMultiScattCS", "cs_5_1"));

        mAtmosphereShaders["skyViewLutCS_ms_disabled"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, nullptr, "ComputeSkyViewLutCS", "cs_5_1"));

        constexpr D3D_SHADER_MACRO multiScatEnabledDefines[] =
        {
            "MULTISCATAPPROX_ENABLED", "1",
            nullptr, nullptr
        };
        mAtmosphereShaders["skyViewLutCS"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, multiScatEnabledDefines, "ComputeSkyViewLutCS", "cs_5_1"));
    
        mAtmosphereShaders["aerialPerspCS"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, multiScatEnabledDefines, "ComputeCameraVolumeCS", "cs_5_1"));
    
        constexpr D3D_SHADER_MACRO rayMarchingDefines[] =
        {
            "MULTISCATAPPROX_ENABLED", "1",
            "FASTSKY_ENABLED", "1",
            "COLORED_TRANSMITTANCE_ENABLED", "0",
            "FASTAERIALPERSPECTIVE_ENABLED", "1",
            "SHADOWMAP_ENABLED", "0",
            nullptr, nullptr
        };
        mAtmosphereShaders["rayMarchCS"] = std::move(
            std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, rayMarchingDefines, "ComputeRayMarchingCS", "cs_5_1"));

        for (auto&& pair : mAtmosphereShaders)
        {
            pair.second->LoadAndCompile();
        }
    }

    void SkyAtmosphereResources::InitPSOs()
    {
        /*
        Graphics PSO
        */
	    const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = { nullptr, 0 };

	    D3D12_GRAPHICS_PIPELINE_STATE_DESC terrainPsoDesc;

	    ZeroMemory(&terrainPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	    terrainPsoDesc.InputLayout = inputLayoutDesc;
	    terrainPsoDesc.pRootSignature = mRootSignature->GetNativeSignature().Get();
	    terrainPsoDesc.VS = mAtmosphereShaders["terrainVS"]->GetShaderResource();
	    terrainPsoDesc.PS = mAtmosphereShaders["terrainPS"]->GetShaderResource();
	    terrainPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	    terrainPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	    // terrainPsoDesc.RasterizerState.DepthClipEnable = false;
	    terrainPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	    terrainPsoDesc.SampleMask = UINT_MAX;
	    terrainPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	    terrainPsoDesc.NumRenderTargets = 1;
	    terrainPsoDesc.RTVFormats[0] = GetSRGBFormat(BackBufferFormat);

	    terrainPsoDesc.SampleDesc.Count = 1;
	    terrainPsoDesc.SampleDesc.Quality = 0;

	    terrainPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	    terrainPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	    terrainPsoDesc.DepthStencilState.DepthEnable = TRUE;
	    terrainPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	    terrainPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

        mTerrainPSO = std::make_shared<GraphicPSO>(RenderMode::Terrain);
        mTerrainPSO->SetPsoDesc(terrainPsoDesc);
        mTerrainPSO->Initialize(mDevice);

        /*
        Compute PSOs
        */

        mComputeTransmittancePSO = std::make_shared<ComputePSO>();
        mComputeTransmittancePSO->SetShader(mAtmosphereShaders["transmittanceCS"].get());
        mComputeTransmittancePSO->SetRootSignature(*mRootSignature);
    
        mComputeMultiscatPSO = std::make_shared<ComputePSO>();
        mComputeMultiscatPSO->SetShader(mAtmosphereShaders["multiscatCS"].get());
        mComputeMultiscatPSO->SetRootSignature(*mRootSignature);

        mComputeSkyviewPSO = std::make_shared<ComputePSO>();
        mComputeSkyviewPSO->SetShader(mAtmosphereShaders["skyViewLutCS"].get());
        mComputeSkyviewPSO->SetRootSignature(*mRootSignature);

        mComputeAerialPSO = std::make_shared<ComputePSO>();
        mComputeAerialPSO->SetShader(mAtmosphereShaders["aerialPerspCS"].get());
        mComputeAerialPSO->SetRootSignature(*mRootSignature);

        mComputeRaymarchPSO = std::make_shared<ComputePSO>();
        mComputeRaymarchPSO->SetShader(mAtmosphereShaders["rayMarchCS"].get());
        mComputeRaymarchPSO->SetRootSignature(*mRootSignature);

		mComputeTransmittancePSO->Initialize(mDevice);
        mComputeMultiscatPSO->Initialize(mDevice);
        mComputeSkyviewPSO->Initialize(mDevice);
        mComputeAerialPSO->Initialize(mDevice);
        mComputeRaymarchPSO->Initialize(mDevice);
    }

    void SkyAtmosphereResources::LoadResources()
    {
        LoadTerrainResource();
        LoadTransmittanceLutResource();
        LoadMultiScatLutResource();
        LoadSkyViewLutResource();
        LoadAerialPerpspectiveLutResource();
        LoadRayMarchingResource();
    }

    void SkyAtmosphereResources::LoadTerrainResource()
    {
	    D3D12_RESOURCE_DESC renderTargetDesc;
	    renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	    renderTargetDesc.Alignment = 0;
	    renderTargetDesc.Width = mScreenWidth;
	    renderTargetDesc.Height = mScreenHeight;
	    renderTargetDesc.DepthOrArraySize = 1;
	    renderTargetDesc.MipLevels = 1;
	    renderTargetDesc.Format = BackBufferFormat;
	    renderTargetDesc.SampleDesc.Count = 1;
	    renderTargetDesc.SampleDesc.Quality = 0;
	    renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	    D3D12_CLEAR_VALUE optClear;
	    optClear = CD3DX12_CLEAR_VALUE(BackBufferFormat, DirectX::Colors::Black);

        mTerrainRenderTarget = std::make_shared<GTexture>(mDevice, renderTargetDesc,
            L"Terrain RTV", TextureUsage::RenderTarget, &optClear);

        mTerrainRenderTargetRTV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
        mTerrainRenderTargetSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

	    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	    rtvDesc.Format = BackBufferFormat;
	    rtvDesc.Texture2D.MipSlice = 0;
	    rtvDesc.Texture2D.PlaneSlice = 0;
        mTerrainRenderTarget->CreateRenderTargetView(&rtvDesc, &mTerrainRenderTargetRTV);

	    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	    srvDesc.Format = BackBufferFormat;
	    srvDesc.Texture2D.MostDetailedMip = 0;
	    srvDesc.Texture2D.MipLevels = 1;
        mTerrainRenderTarget->CreateShaderResourceView(&srvDesc, &mTerrainRenderTargetSRV);



	    D3D12_RESOURCE_DESC texDesc;
	    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	    texDesc.Alignment = 0;
	    texDesc.Width = mScreenWidth;
	    texDesc.Height = mScreenHeight;
	    texDesc.DepthOrArraySize = 1;
	    texDesc.MipLevels = 1;
	    texDesc.SampleDesc.Count = 1;
	    texDesc.SampleDesc.Quality = 0;
	    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	    optClear.Format = DXGI_FORMAT_D32_FLOAT;
	    optClear.DepthStencil.Depth = 1.0f;

        mDepthMap = std::make_shared<GTexture>(mDevice, texDesc,
		    L"Terrain Depth Map " + mDevice->GetName(),
		    TextureUsage::Depth, &optClear);

	    mDepthMapSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	    mDepthMapDSV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

	    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	    dsvDesc.Texture2D.MipSlice = 0;
	    mDepthMap->CreateDepthStencilView(&dsvDesc, &mDepthMapDSV);

	    srvDesc = {};
	    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	    srvDesc.Texture2D.MostDetailedMip = 0;
	    srvDesc.Texture2D.MipLevels = 1;
	    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	    mDepthMap->CreateShaderResourceView(&srvDesc, &mDepthMapSRV);

	    // Height Map
	    auto queue = mDevice->GetCommandQueue(GQueueType::Compute);
	    const auto cmdList = queue->GetCommandList();
	    mHeightMapTex = GTexture::LoadTextureFromFile(L"Data\\Textures\\heightmap.dds", cmdList);
	    mHeightMapTex->SetName(L"heightMapTex");
	    queue->WaitForFenceValue(queue->ExecuteCommandList(cmdList));
	    mDevice->Flush();

	    mHeightMapTexSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

	    srvDesc = {};
	    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	    auto desc = mHeightMapTex->GetD3D12Resource()->GetDesc();
	    srvDesc.Format = GetSRGBFormat(desc.Format);
	    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	    srvDesc.Texture2D.MipLevels = desc.MipLevels;
	    srvDesc.Texture2D.MostDetailedMip = 0;
	    mHeightMapTex->CreateShaderResourceView(&srvDesc, &mHeightMapTexSRV);

	    // Terrain

        mTerrainViewport.Height = static_cast<float>(mScreenHeight);
        mTerrainViewport.Width = static_cast<float>(mScreenWidth);
        mTerrainViewport.MinDepth = 0.0f;
        mTerrainViewport.MaxDepth = 1.0f;
        mTerrainViewport.TopLeftX = 0;
        mTerrainViewport.TopLeftY = 0;

	    mTerrainScissorRect = { 0, 0, (int) mScreenWidth, (int) mScreenHeight };
    }

    void SkyAtmosphereResources::LoadTransmittanceLutResource()
    {
        // Create simple transmittance LUT (2D) as a starting point. Sizes chosen to match UE sample sizes.
        D3D12_RESOURCE_DESC transDesc = {};
        ZeroMemory(&transDesc, sizeof(D3D12_RESOURCE_DESC));
        transDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        transDesc.Alignment = 0;
        transDesc.DepthOrArraySize = 1;
        transDesc.MipLevels = 1;
        transDesc.SampleDesc.Count = 1;
        transDesc.SampleDesc.Quality = 0;
        transDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        transDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        transDesc.Width = 256; // TRANSMITTANCE_TEXTURE_WIDTH
        transDesc.Height = 64; // TRANSMITTANCE_TEXTURE_HEIGHT
        transDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // transmittance LUT format

        mTransmittanceLut = std::make_shared<GTexture>(mDevice, transDesc,
            L"TransmittanceLut " + mDevice->GetName(), TextureUsage::Normalmap);

        mTransmittanceLutUAV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
        mTransmittanceLutSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;  // transmittance LUT format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.PlaneSlice = 0;
        uavDesc.Texture2D.MipSlice = 0;
        mTransmittanceLut->CreateUnorderedAccessView(&uavDesc, &mTransmittanceLutUAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;  // transmittance LUT format
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        mTransmittanceLut->CreateShaderResourceView(&srvDesc, &mTransmittanceLutSRV);
    }

    void SkyAtmosphereResources::LoadMultiScatLutResource()
    {
        const UINT MultiScatteringLUTRes = 32;

        // Create simple MultiScat LUT (2D) as a starting point. Sizes chosen to match UE sample sizes.
        D3D12_RESOURCE_DESC multiscatDesc = {};
        ZeroMemory(&multiscatDesc, sizeof(D3D12_RESOURCE_DESC));
        multiscatDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        multiscatDesc.Alignment = 0;
        multiscatDesc.DepthOrArraySize = 1;
        multiscatDesc.MipLevels = 1;
        multiscatDesc.SampleDesc.Count = 1;
        multiscatDesc.SampleDesc.Quality = 0;
        multiscatDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        multiscatDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        multiscatDesc.Width = MultiScatteringLUTRes;
        multiscatDesc.Height = MultiScatteringLUTRes;
        multiscatDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // MultiScat LUT format

        mMultiScatLut = std::make_shared<GTexture>(mDevice, multiscatDesc,
            L"MultiScatLut " + mDevice->GetName(), TextureUsage::Normalmap);

        mMultiScatLutUAV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
        mMultiScatLutSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // MultiScat LUT format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.PlaneSlice = 0;
        uavDesc.Texture2D.MipSlice = 0;
        mMultiScatLut->CreateUnorderedAccessView(&uavDesc, &mMultiScatLutUAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // MultiScat LUT format
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        mMultiScatLut->CreateShaderResourceView(&srvDesc, &mMultiScatLutSRV);
    }

    void SkyAtmosphereResources::LoadSkyViewLutResource()
    {
        // Create simple SkyView LUT (2D) as a starting point. Sizes chosen to match UE sample sizes.
        D3D12_RESOURCE_DESC skyviewDesc = {};
        ZeroMemory(&skyviewDesc, sizeof(D3D12_RESOURCE_DESC));
        skyviewDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        skyviewDesc.Alignment = 0;
        skyviewDesc.DepthOrArraySize = 1;
        skyviewDesc.MipLevels = 1;
        skyviewDesc.SampleDesc.Count = 1;
        skyviewDesc.SampleDesc.Quality = 0;
        skyviewDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        skyviewDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        skyviewDesc.Width = 192; // SKYVIEW_TEXTURE_WIDTH
        skyviewDesc.Height = 108; // SKYVIEW_TEXTURE_HEIGHT
        skyviewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // SkyView LUT format

        mSkyViewLut = std::make_shared<GTexture>(mDevice, skyviewDesc,
            L"SkyViewLut " + mDevice->GetName(), TextureUsage::Normalmap);

        mSkyViewLutUAV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
        mSkyViewLutSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // SkyView LUT format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.PlaneSlice = 0;
        uavDesc.Texture2D.MipSlice = 0;
        mSkyViewLut->CreateUnorderedAccessView(&uavDesc, &mSkyViewLutUAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // SkyView LUT format
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        mSkyViewLut->CreateShaderResourceView(&srvDesc, &mSkyViewLutSRV);
    }

    void SkyAtmosphereResources::LoadAerialPerpspectiveLutResource()
    {
        const UINT volumeRes = 32; // 32x32x32

        // Create simple Aerial Persp LUT (2D) as a starting point. Sizes chosen to match UE sample sizes.
        D3D12_RESOURCE_DESC aerialPerspDesc = {};
        ZeroMemory(&aerialPerspDesc, sizeof(D3D12_RESOURCE_DESC));
        aerialPerspDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        aerialPerspDesc.Width = volumeRes;
        aerialPerspDesc.Height = volumeRes;
        aerialPerspDesc.DepthOrArraySize = static_cast<UINT16>(volumeRes);
        aerialPerspDesc.Alignment = 0;
        aerialPerspDesc.MipLevels = 1;
        aerialPerspDesc.SampleDesc.Count = 1;
        aerialPerspDesc.SampleDesc.Quality = 0;
        aerialPerspDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        aerialPerspDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        aerialPerspDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // Aerial Persp LUT format

        mAerialPerpspectiveLut = std::make_shared<GTexture>(mDevice, aerialPerspDesc,
            L"AerialPerspLut " + mDevice->GetName(), TextureUsage::Normalmap);

        mAerialPerpspectiveLutUAV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
        mAerialPerpspectiveLutSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // Aerial Persp LUT format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.MipSlice = 0;
        uavDesc.Texture3D.WSize = volumeRes;
        mAerialPerpspectiveLut->CreateUnorderedAccessView(&uavDesc, &mAerialPerpspectiveLutUAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // Aerial Persp LUT format
        srvDesc.Texture3D.MostDetailedMip = 0;
        srvDesc.Texture3D.MipLevels = 1;
        mAerialPerpspectiveLut->CreateShaderResourceView(&srvDesc, &mAerialPerpspectiveLutSRV);
    }

    void SkyAtmosphereResources::LoadRayMarchingResource()
    {
        // Create simple RayMarching (2D) as a starting point. Sizes chosen to match UE sample sizes.
        D3D12_RESOURCE_DESC raymarchDesc = {};
        ZeroMemory(&raymarchDesc, sizeof(D3D12_RESOURCE_DESC));
        raymarchDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        raymarchDesc.Alignment = 0;
        raymarchDesc.DepthOrArraySize = 1;
        raymarchDesc.MipLevels = 1;
        raymarchDesc.SampleDesc.Count = 1;
        raymarchDesc.SampleDesc.Quality = 0;
        raymarchDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        raymarchDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        raymarchDesc.Width = 1920;
        raymarchDesc.Height = 1080;
        raymarchDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // RayMarching format

        mRayMarchingResult = std::make_shared<GTexture>(mDevice, raymarchDesc,
            L"RayMarching " + mDevice->GetName(), TextureUsage::Normalmap);

        mRayMarchingResultUAV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
        mRayMarchingResultSRV = mDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;  // RayMarching format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.PlaneSlice = 0;
        uavDesc.Texture2D.MipSlice = 0;
        mRayMarchingResult->CreateUnorderedAccessView(&uavDesc, &mRayMarchingResultUAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;  // RayMarching format
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        mRayMarchingResult->CreateShaderResourceView(&srvDesc, &mRayMarchingResultSRV);
    }

    void SkyAtmosphere::ComputeAtmosphere(const std::shared_ptr<GCommandList>& cmdList,
        const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
	    const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB,
	    const SkyAtmosphereResources& Resources)
	{
	    PopulateTransmittanceLutCommands(cmdList,
            AtmosphereCommonConstantsCB,
            AtmosphereConstantsCB, Resources);

	    PopulateMultiScatLutCommands(cmdList, Resources);
	    PopulateSkyViewLutCommands(cmdList, Resources);
	    PopulateAerialPerspectiveCommands(cmdList, Resources);

	    PopulateRayMarchingCommands(cmdList, Resources);
    }

    void SkyAtmosphere::PopulateTerrainCommands(const std::shared_ptr<GCommandList>& cmdList,
	    const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
	    const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB,
	    const SkyAtmosphereResources& Resources)
    {
	    cmdList->StartMark(L"Terrain Draw");

        cmdList->GetGraphicsCommandList()->SetGraphicsRootSignature(Resources.GetRootSignature().GetNativeSignature().Get());
		cmdList->SetPipelineState(Resources.GetTerrainPSO());
		cmdList->SetDescriptorsHeap(Resources.GetRayMarchingResultSRV());

	    cmdList->SetViewports(&Resources.GetTerrainViewport(), 1);
	    cmdList->SetScissorRects(&Resources.GetTerrainScissorRect(), 1);
	    cmdList->TransitionBarrier(Resources.GetDepthMap(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	    cmdList->TransitionBarrier(Resources.GetTerrainRender(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	    cmdList->TransitionBarrier(Resources.GetTransmittanceLut(), D3D12_RESOURCE_STATE_GENERIC_READ);
	    cmdList->FlushResourceBarriers();
	    cmdList->ClearDepthStencil(Resources.GetDepthMapDSV(), 0, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0);
	    cmdList->ClearRenderTarget(Resources.GetTerrainRenderRTV(), 0);
	    cmdList->SetRenderTargets(1, Resources.GetTerrainRenderRTV(), 0, Resources.GetDepthMapDSV());
    
        cmdList->SetGraphicsRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Transmittance, Resources.GetTransmittanceLutSRV());

        // 14th root parameter
        cmdList->SetGraphicsRootDescriptorTable((UINT) CBSlots::Count + (UINT) TextureSlots::Count + (UINT) UavSlots::Count, Resources.GetHeightMapTexSRV());

	    cmdList->SetRootConstantBufferView((UINT)CBSlots::Common, *AtmosphereCommonConstantsCB);
	    cmdList->SetRootConstantBufferView((UINT)CBSlots::Atmosphere, *AtmosphereConstantsCB);
	    // 15th root parameter
	    // cmdList->SetRootConstantBufferView((UINT)CBSlots::Count + (UINT)TextureSlots::Count + (UINT)UavSlots::Count + 1, *mTerrainCB, 0);

	    cmdList->GetGraphicsCommandList()->IASetVertexBuffers(0, 0, nullptr);
	    cmdList->GetGraphicsCommandList()->IASetIndexBuffer(nullptr);
	    cmdList->GetGraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	    cmdList->Draw(6, mTerrainResolution * mTerrainResolution);

	    // cmdList->TransitionBarrier(*mTerrainRenderTarget.get(), D3D12_RESOURCE_STATE_GENERIC_READ);
	    // cmdList->FlushResourceBarriers();

	    cmdList->EndMark();
    }

    void SkyAtmosphere::PopulateTransmittanceLutCommands(const std::shared_ptr<GCommandList>& cmdList,
	    const std::shared_ptr<ConstantUploadBuffer<AtmosphereCommonConstants>>& AtmosphereCommonConstantsCB,
	    const std::shared_ptr<ConstantUploadBuffer<AtmosphereConstants>>& AtmosphereConstantsCB,
	    const SkyAtmosphereResources& Resources)
    {

        cmdList->StartMark(L"TransmittanceLUT");
        // Set viewport/scissor to transmittance texture size

        // Transition resource to UAV and clear
        cmdList->TransitionBarrier(Resources.GetTransmittanceLut(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList->FlushResourceBarriers();

        // Bind root signature and atmosphere CBs
        cmdList->GetGraphicsCommandList()->SetComputeRootSignature(Resources.GetRootSignature().GetNativeSignature().Get());
        cmdList->SetPipelineState(Resources.GetComputeTransmittancePSO());
        cmdList->SetDescriptorsHeap(Resources.GetRayMarchingResultSRV());
    
        cmdList->SetComputeRootConstantBufferView((UINT)CBSlots::Common, *AtmosphereCommonConstantsCB);
        cmdList->SetComputeRootConstantBufferView((UINT)CBSlots::Atmosphere, *AtmosphereConstantsCB);

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Count + (UINT)UavSlots::Transmittance,
            Resources.GetTransmittanceLutUAV());

        auto IntDivRoundUp = [](UINT a, UINT b) { return (a + b - 1) / b; };

        auto tgx = IntDivRoundUp(mLutInfos.TRANSMITTANCE_TEXTURE_WIDTH, 32);
        auto tgy = IntDivRoundUp(mLutInfos.TRANSMITTANCE_TEXTURE_HEIGHT, 32);
        cmdList->Dispatch(tgx, tgy, 1);

        // cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_COMMON);
        cmdList->FlushResourceBarriers();
        cmdList->EndMark();
    }

    void SkyAtmosphere::PopulateMultiScatLutCommands(const std::shared_ptr<GCommandList>& cmdList,
	    const SkyAtmosphereResources& Resources)
    {
        cmdList->StartMark(L"MultiScatLUT");
        // Set viewport/scissor to transmittance texture size

        // Transition resource to UAV and clear
        cmdList->TransitionBarrier(Resources.GetTransmittanceLut(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetMultiScatLut(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList->FlushResourceBarriers();

        // cmdList->SetComputeRootSignature(*mRootSignature);
        cmdList->SetPipelineState(Resources.GetComputeMultiscatPSO());
    
        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots ::Transmittance, Resources.GetTransmittanceLutSRV());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Count + (UINT)UavSlots::Multiscat, Resources.GetMultiScatLutUAV());

        const UINT MultiScatteringLUTRes = 32;
        cmdList->Dispatch(MultiScatteringLUTRes, MultiScatteringLUTRes, 1);

        // cmdList->TransitionBarrier(*mMultiScatLut, D3D12_RESOURCE_STATE_COMMON);
        // cmdList->FlushResourceBarriers();
        cmdList->EndMark();
    }

    void SkyAtmosphere::PopulateSkyViewLutCommands(const std::shared_ptr<GCommandList>& cmdList,
	    const SkyAtmosphereResources& Resources)
    {

        cmdList->StartMark(L"SkyViewLUT");
        // Set viewport/scissor to transmittance texture size

        // Transition resource to UAV and clear
        // cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetMultiScatLut(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetSkyViewLut(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList->FlushResourceBarriers();

        // cmdList->SetComputeRootSignature(*mRootSignature);
        cmdList->SetPipelineState(Resources.GetComputeSkyviewPSO());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Multiscat, Resources.GetMultiScatLutSRV());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Count + (UINT)UavSlots::SkyView, Resources.GetSkyViewLutUAV());

        // cmdList->SetComputeRootDescriptorTable(5, &mTransmittanceLutSRV);

        auto IntDivRoundUp = [](UINT a, UINT b) { return (a + b - 1) / b; };

        auto tgx = IntDivRoundUp(192, 32);
        auto tgy = IntDivRoundUp(108, 32);
        cmdList->Dispatch(tgx, tgy, 1);

        // cmdList->TransitionBarrier(*mSkyViewLut, D3D12_RESOURCE_STATE_COMMON);
        cmdList->FlushResourceBarriers();
        cmdList->EndMark();
    }

    void SkyAtmosphere::PopulateAerialPerspectiveCommands(const std::shared_ptr<GCommandList>& cmdList,
	    const SkyAtmosphereResources& Resources)
    {
        cmdList->StartMark(L"AerialPerspLUT");
        // Set viewport/scissor to transmittance texture size

        // Transition resource to UAV and clear
        // cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_GENERIC_READ);
        // cmdList->TransitionBarrier(*mMultiScatLut, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetSkyViewLut(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetAerialPerpspectiveLut(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList->FlushResourceBarriers();

        // cmdList->SetComputeRootSignature(*mRootSignature);
        cmdList->SetPipelineState(Resources.GetComputeAerialPSO());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::SkyView, Resources.GetSkyViewLutSRV());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Count + (UINT)UavSlots::Aerial,
            Resources.GetAerialPerpspectiveLutUAV());

        // cmdList->SetComputeRootDescriptorTable(5, &mTransmittanceLutSRV);

        auto IntDivRoundUp = [](UINT a, UINT b) { return (a + b - 1) / b; };

        auto tgx = IntDivRoundUp(32, 32);
        auto tgy = IntDivRoundUp(32, 32);
        auto tgz = IntDivRoundUp(32, 1);
        cmdList->Dispatch(tgx, tgy, tgz);

        cmdList->EndMark();
    }

    void SkyAtmosphere::PopulateRayMarchingCommands(const std::shared_ptr<GCommandList>& cmdList,
	    const SkyAtmosphereResources& Resources)
    {
        cmdList->StartMark(L"RayMarchingLUT");
        // Set viewport/scissor to transmittance texture size

        // Transition resource to UAV and clear
        // cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_GENERIC_READ);
        // cmdList->TransitionBarrier(*mMultiScatLut, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetAerialPerpspectiveLut(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetDepthMap(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetTerrainRender(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->TransitionBarrier(Resources.GetRayMarchingResult(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList->FlushResourceBarriers();

        // cmdList->SetComputeRootSignature(*mRootSignature);
        cmdList->SetPipelineState(Resources.GetComputeRaymarchPSO());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Aerial, Resources.GetAerialPerpspectiveLutSRV());
        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Depth, Resources.GetDepthMapSRV());
        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::TerrainRender, Resources.GetTerrainRenderSRV());

        cmdList->SetComputeRootDescriptorTable((UINT)CBSlots::Count + (UINT)TextureSlots::Count + (UINT)UavSlots::RayMarching,
            Resources.GetRayMarchingResultUAV());

        // cmdList->SetComputeRootDescriptorTable(5, &mTransmittanceLutSRV);

        auto IntDivRoundUp = [](UINT a, UINT b) { return (a + b - 1) / b; };

        auto tgx = IntDivRoundUp(1920, 32);
        auto tgy = IntDivRoundUp(1080, 32);
        cmdList->Dispatch(tgx, tgy, 1);

        cmdList->TransitionBarrier(Resources.GetRayMarchingResult(), D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->FlushResourceBarriers();
        cmdList->EndMark();
    }

    void SkyAtmosphere::UpdateSkyAtmosphereBuffer()
    {
        // Populate AtmosphereConstants with sensible defaults / current values and upload to GPU
        
        // Fill with a pattern like the original project to help detect uninitialized fields in debug
        //memset(&mAtmosphereConstants, 0xBA, sizeof(AtmosphereConstants));

        // Match initialization values used in InitTransmittanceLutPass and Game::updateSkyAtmosphereConstant
        mAtmosphereConstants.solar_irradiance = mAtmosphereInfos.solar_irradiance;
        mAtmosphereConstants.sun_angular_radius = mAtmosphereInfos.sun_angular_radius;
        mAtmosphereConstants.absorption_extinction = mAtmosphereInfos.absorption_extinction;
        mAtmosphereConstants.mu_s_min = mAtmosphereInfos.mu_s_min;

        memcpy(mAtmosphereConstants.rayleigh_density, &mAtmosphereInfos.rayleigh_density, sizeof(mAtmosphereInfos.rayleigh_density));
        memcpy(mAtmosphereConstants.mie_density, &mAtmosphereInfos.mie_density, sizeof(mAtmosphereInfos.mie_density));
        memcpy(mAtmosphereConstants.absorption_density, &mAtmosphereInfos.absorption_density, sizeof(mAtmosphereInfos.absorption_density));

        mAtmosphereConstants.mie_phase_function_g = mAtmosphereInfos.mie_phase_function_g;
        mAtmosphereConstants.rayleigh_scattering = mAtmosphereInfos.rayleigh_scattering;
        const float RayleighScatScale = 1.0f;
        mAtmosphereConstants.rayleigh_scattering.x *= RayleighScatScale;
        mAtmosphereConstants.rayleigh_scattering.y *= RayleighScatScale;
        mAtmosphereConstants.rayleigh_scattering.z *= RayleighScatScale;
        mAtmosphereConstants.mie_scattering = mAtmosphereInfos.mie_scattering;

        auto MaxZero3 = [](Vector3& a) {Vector3 r; r.x = a.x > 0.0f ? a.x : 0.0f; r.y = a.y > 0.0f ? a.y : 0.0f; r.z = a.z > 0.0f ? a.z : 0.0f; return r; };
        mAtmosphereConstants.mie_absorption = MaxZero3(mAtmosphereInfos.mie_extinction - mAtmosphereInfos.mie_scattering);
        mAtmosphereConstants.mie_extinction = mAtmosphereInfos.mie_extinction;
        mAtmosphereConstants.ground_albedo = mAtmosphereInfos.ground_albedo;
        mAtmosphereConstants.bottom_radius = mAtmosphereInfos.bottom_radius;
        mAtmosphereConstants.top_radius = mAtmosphereInfos.top_radius;
        mAtmosphereConstants.MultipleScatteringFactor = 1;
        mAtmosphereConstants.MultiScatteringLUTRes = mCommonConstanants.multiScatLutResolution[0];

        //
        // mAtmosphereConstants.TRANSMITTANCE_TEXTURE_WIDTH = mLutInfos.TRANSMITTANCE_TEXTURE_WIDTH;
        // mAtmosphereConstants.TRANSMITTANCE_TEXTURE_HEIGHT = mLutInfos.TRANSMITTANCE_TEXTURE_HEIGHT;
        // mAtmosphereConstants.IRRADIANCE_TEXTURE_WIDTH = mLutInfos.IRRADIANCE_TEXTURE_WIDTH;
        // mAtmosphereConstants.IRRADIANCE_TEXTURE_HEIGHT = mLutInfos.IRRADIANCE_TEXTURE_HEIGHT;
        // mAtmosphereConstants.SCATTERING_TEXTURE_R_SIZE = mLutInfos.SCATTERING_TEXTURE_R_SIZE;
        // mAtmosphereConstants.SCATTERING_TEXTURE_MU_SIZE = mLutInfos.SCATTERING_TEXTURE_MU_SIZE;
        // mAtmosphereConstants.SCATTERING_TEXTURE_MU_S_SIZE = mLutInfos.SCATTERING_TEXTURE_MU_S_SIZE;
        // mAtmosphereConstants.SCATTERING_TEXTURE_NU_SIZE = mLutInfos.SCATTERING_TEXTURE_NU_SIZE;
        mAtmosphereConstants.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = Vector3(114974.916437f, 71305.954816f, 65310.548555f); // Not used if using LUTs as transfert
        mAtmosphereConstants.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = Vector3(98242.786222f, 69954.398112f, 66475.012354f);  // idem


        mAtmosphereConstants.gSkyViewProjMat = mViewProjMat;
        {
            mAtmosphereConstants.gSkyInvViewProjMat = mViewProjMat.Invert();
        }
        {
            mAtmosphereConstants.gSkyInvProjMat = mProjMat.Invert();
        }
        {
            mAtmosphereConstants.gSkyInvViewMat = mViewMat.Invert();
        }

        mAtmosphereConstants.gShadowmapViewProjMat = mShadowmapViewProjMat;

        mAtmosphereConstants.camera = mCamPosFinal;
        mAtmosphereConstants.view_ray = mViewDir;
        mAtmosphereConstants.sun_direction = mSunDir;

        viewRayMarchMaxSPP = viewRayMarchMinSPP >= viewRayMarchMaxSPP ? viewRayMarchMinSPP + 1 : viewRayMarchMaxSPP;
        mCommonConstanants.rayMarchMinMaxSPP[0] = float(viewRayMarchMinSPP);
        mCommonConstanants.rayMarchMinMaxSPP[1] = float(viewRayMarchMaxSPP);
        mCommonConstanants.terrainResolution = mTerrainResolution;
    }

    void SkyAtmosphereCrossResources::Initialize(const SkyAtmosphereResources& Resources, const std::shared_ptr<GDevice>& primeDevice, const std::shared_ptr<GDevice>& secondDevice)
    {
        mTerrainRenderTarget = std::make_shared<GCrossAdapterResource>(Resources.GetTerrainRender().GetD3D12ResourceDesc(), primeDevice, secondDevice, 
            L"Cross TerrainRenderTarget");
		mDepthMap = std::make_shared<GCrossAdapterResource>(Resources.GetDepthMap().GetD3D12ResourceDesc(), primeDevice, secondDevice,
			L"Cross DepthMap");

		mRayMarchingResult = std::make_shared<GCrossAdapterResource>(Resources.GetRayMarchingResult().GetD3D12ResourceDesc(), primeDevice, secondDevice,
			L"Cross RayMarchingResult");
		mTransmittanceLut = std::make_shared<GCrossAdapterResource>(Resources.GetTransmittanceLut().GetD3D12ResourceDesc(), primeDevice, secondDevice,
			L"Cross TransmittanceLut");
    }

    void SkyAtmosphereCrossResources::OnResize(uint32_t width, uint32_t height) const
    {
        mRayMarchingResult->Resize(width, height);
    }

    SkyAtmosphereResources::SkyAtmosphereResources()
    {

    }

    void SkyAtmosphereResources::Initialize(const std::shared_ptr<GDevice>& device, UINT screenWidth /*= 1920*/, UINT screenHeight /*= 1080*/, uint32_t terrainResolution /*= 512u*/)
    {

	    mScreenWidth = screenWidth;
	    mScreenHeight = screenHeight;
        mDevice = device;
        mTerrainResolution = terrainResolution;

	    LoadResources();
	    InitRootSignatures();
	    LoadShaders();
	    InitPSOs();
    }
}
