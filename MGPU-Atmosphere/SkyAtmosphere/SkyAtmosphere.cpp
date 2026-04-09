#include "SkyAtmosphere.h"

Atmosphere::SkyAtmosphere::SkyAtmosphere(const std::shared_ptr<GDevice>& device)
{
	mDevice = device;

    LoadResources();
    InitRootSignatures();
    LoadShaders();
    InitPSOs();
    SetupEarthAtmosphere();
}

void Atmosphere::SkyAtmosphere::SetupEarthAtmosphere()
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
    
    // Ensure constant buffers exist and are populated before dispatching LUT passes
    mAtmosphereCB = std::make_shared<ConstantUploadBuffer<AtmosphereCB>>(mDevice, 1, L"SkyAtmosphere CB");
    AtmosphereCB cb{};
    memset(&cb, 0xBA, sizeof(AtmosphereCB));
    mAtmosphereCB->CopyData(0, cb);

    mCommonCB = std::make_shared<ConstantUploadBuffer<CommonConstantBufferStructure>>(mDevice, 1, L"CommonConstantsCB CB");
    mCommonCB->CopyData(0, mCommonConstanants);
}

void Atmosphere::SkyAtmosphere::InitRootSignatures()
{
    // Build a dedicated root signature for the transmittance pass (it needs b1 and b2 CBs)
    mRootSignature = std::make_shared<GRootSignature>();
    mRootSignature->AddConstantBufferParameter(0); // commmon_BUFFER
    mRootSignature->AddConstantBufferParameter(1); // SKYATMOSPHERE_BUFFER
    //mRootSignature->AddConstantBufferParameter(0, 1); // LutConstants (Dimensions, InvDimensions)

    CD3DX12_DESCRIPTOR_RANGE uavRange0;
    CD3DX12_DESCRIPTOR_RANGE uavRange1;
    CD3DX12_DESCRIPTOR_RANGE uavRange2;
    uavRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0); // u0 transmittance output
    uavRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0); // u1 multiscat output
    uavRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 1); // u0, space1 skyview output
    mRootSignature->AddDescriptorParameter(&uavRange0, 1);
    mRootSignature->AddDescriptorParameter(&uavRange1, 1);
    mRootSignature->AddDescriptorParameter(&uavRange2, 1);

    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    CD3DX12_DESCRIPTOR_RANGE srvRange2;
    CD3DX12_DESCRIPTOR_RANGE srvRange3;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0); // t0 transmittance
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0); // t1 multiscat
    srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0); // t2, skyview
    srvRange3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0); // t3, shadow
    mRootSignature->AddDescriptorParameter(&srvRange0, 1);
    mRootSignature->AddDescriptorParameter(&srvRange1, 1);
    mRootSignature->AddDescriptorParameter(&srvRange2, 1);
    mRootSignature->AddDescriptorParameter(&srvRange3, 1);

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

void Atmosphere::SkyAtmosphere::LoadShaders()
{
    /*
    std::vector<std::string> dirs{
    "Shaders",
    "Shaders/Terrain"
    };
    */

    mAtmosphereShaders["brunetonTransmittanceCS"] = std::move(
        std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\Bruneton\\BrunetonTransmittanceLut.hlsl", ComputeShader, nullptr, "TransmittanceLutCS_Bruneton", "cs_5_1"));

    mAtmosphereShaders["transmittanceCS"] = std::move(
        std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, nullptr, "ComputeTransmittanceLutCS", "cs_5_1"));

    mAtmosphereShaders["multiscatCS"] = std::move(
        std::make_shared<GShader>(L"Shaders\\SkyAtmosphere\\ComputeSkyRayMarching.hlsl", ComputeShader, nullptr, "ComputeMultiScattCS", "cs_5_1"));

    for (auto&& pair : mAtmosphereShaders)
    {
        pair.second->LoadAndCompile();
    }
}

void Atmosphere::SkyAtmosphere::InitPSOs()
{
    auto transmittancePSO = std::make_shared<ComputePSO>();
    transmittancePSO->SetShader(mAtmosphereShaders["transmittanceCS"].get());
    transmittancePSO->SetRootSignature(*mRootSignature);

    mAtmospherePSOs["transmittance"] = std::move(transmittancePSO);
    
    auto multiscatPSO = std::make_shared<ComputePSO>();
    multiscatPSO->SetShader(mAtmosphereShaders["multiscatCS"].get());
    multiscatPSO->SetRootSignature(*mRootSignature);

    mAtmospherePSOs["multiscat"] = std::move(multiscatPSO);

    for (auto& pso : mAtmospherePSOs)
    {
        pso.second->Initialize(mDevice);
    }
}

void Atmosphere::SkyAtmosphere::LoadResources()
{
    LoadTransmittanceLutResource();
    LoadMultiScatLutResource();
}

void Atmosphere::SkyAtmosphere::LoadTransmittanceLutResource()
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

    mTransmittanceLut = std::make_shared<GTexture>(mDevice, transDesc, L"TransmittanceLut", TextureUsage::Normalmap);

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

void Atmosphere::SkyAtmosphere::LoadMultiScatLutResource()
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

    mMultiScatLut = std::make_shared<GTexture>(mDevice, multiscatDesc, L"MultiScatLut", TextureUsage::Normalmap);

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

void Atmosphere::SkyAtmosphere::PopulateTransmittanceLutCommands(const std::shared_ptr<GCommandList>& cmdList)
{
    if (!mTransmittanceLut || !mTransmittanceLut->GetD3D12Resource())
        return;

    cmdList->StartMark(L"TransmittanceLUT");
    // Set viewport/scissor to transmittance texture size

    // Transition resource to UAV and clear
    cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->FlushResourceBarriers();

    // Bind root signature and atmosphere CBs
    cmdList->SetComputeRootSignature(*mRootSignature);
    cmdList->SetPipelineState(*mAtmospherePSOs["transmittance"]);
    
    if (mCommonCB)
        cmdList->SetComputeRootConstantBufferView(0, *mCommonCB);
    if (mAtmosphereCB)
        cmdList->SetComputeRootConstantBufferView(1, *mAtmosphereCB);

    cmdList->SetComputeRootDescriptorTable(2, &mTransmittanceLutUAV);

    auto IntDivRoundUp = [](UINT a, UINT b) { return (a + b - 1) / b; };

    auto tgx = IntDivRoundUp(mLutInfos.TRANSMITTANCE_TEXTURE_WIDTH, 32);
    auto tgy = IntDivRoundUp(mLutInfos.TRANSMITTANCE_TEXTURE_HEIGHT, 32);
    cmdList->Dispatch(tgx, tgy, 1);

    // cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_COMMON);
    cmdList->FlushResourceBarriers();
    cmdList->EndMark();
}

void Atmosphere::SkyAtmosphere::PopulateMultiScatLutCommands(const std::shared_ptr<GCommandList>& cmdList)
{
    if (!mMultiScatLut || !mMultiScatLut->GetD3D12Resource())
        return;

    cmdList->StartMark(L"MultiScatLUT");
    // Set viewport/scissor to transmittance texture size

    // Transition resource to UAV and clear
    cmdList->TransitionBarrier(*mTransmittanceLut, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->TransitionBarrier(*mMultiScatLut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->FlushResourceBarriers();

    // cmdList->SetComputeRootSignature(*mRootSignature);
    cmdList->SetPipelineState(*mAtmospherePSOs["multiscat"]);
    
    /*
    if (mCommonCB)
        cmdList->SetComputeRootConstantBufferView(0, *mCommonCB);
    if (mAtmosphereCB)
        cmdList->SetComputeRootConstantBufferView(1, *mAtmosphereCB);
    */
    
    cmdList->SetComputeRootDescriptorTable(3, &mMultiScatLutUAV);
    cmdList->SetComputeRootDescriptorTable(5, &mTransmittanceLutSRV);

    const UINT MultiScatteringLUTRes = 32;
    cmdList->Dispatch(MultiScatteringLUTRes, MultiScatteringLUTRes, 1);

    cmdList->TransitionBarrier(*mMultiScatLut, D3D12_RESOURCE_STATE_COMMON);
    cmdList->FlushResourceBarriers();
    cmdList->EndMark();
}

void Atmosphere::SkyAtmosphere::UpdateSkyAtmosphereBuffer()
{
    // Populate AtmosphereCB with sensible defaults / current values and upload to GPU
    AtmosphereCB cb;
    // Fill with a pattern like the original project to help detect uninitialized fields in debug
    //memset(&cb, 0xBA, sizeof(AtmosphereCB));

    // Match initialization values used in InitTransmittanceLutPass and Game::updateSkyAtmosphereConstant
    cb.solar_irradiance = mAtmosphereInfos.solar_irradiance;
    cb.sun_angular_radius = mAtmosphereInfos.sun_angular_radius;
    cb.absorption_extinction = mAtmosphereInfos.absorption_extinction;
    cb.mu_s_min = mAtmosphereInfos.mu_s_min;

    memcpy(cb.rayleigh_density, &mAtmosphereInfos.rayleigh_density, sizeof(mAtmosphereInfos.rayleigh_density));
    memcpy(cb.mie_density, &mAtmosphereInfos.mie_density, sizeof(mAtmosphereInfos.mie_density));
    memcpy(cb.absorption_density, &mAtmosphereInfos.absorption_density, sizeof(mAtmosphereInfos.absorption_density));

    cb.mie_phase_function_g = mAtmosphereInfos.mie_phase_function_g;
    cb.rayleigh_scattering = mAtmosphereInfos.rayleigh_scattering;
    const float RayleighScatScale = 1.0f;
    cb.rayleigh_scattering.x *= RayleighScatScale;
    cb.rayleigh_scattering.y *= RayleighScatScale;
    cb.rayleigh_scattering.z *= RayleighScatScale;
    cb.mie_scattering = mAtmosphereInfos.mie_scattering;

    auto MaxZero3 = [](Vector3& a) {Vector3 r; r.x = a.x > 0.0f ? a.x : 0.0f; r.y = a.y > 0.0f ? a.y : 0.0f; r.z = a.z > 0.0f ? a.z : 0.0f; return r; };
    cb.mie_absorption = MaxZero3(mAtmosphereInfos.mie_extinction - mAtmosphereInfos.mie_scattering);
    cb.mie_extinction = mAtmosphereInfos.mie_extinction;
    cb.ground_albedo = mAtmosphereInfos.ground_albedo;
    cb.bottom_radius = mAtmosphereInfos.bottom_radius;
    cb.top_radius = mAtmosphereInfos.top_radius;
    cb.MultipleScatteringFactor = 1;
    cb.MultiScatteringLUTRes = MultiScatteringLUTRes;

    //
    cb.TRANSMITTANCE_TEXTURE_WIDTH = mLutInfos.TRANSMITTANCE_TEXTURE_WIDTH;
    cb.TRANSMITTANCE_TEXTURE_HEIGHT = mLutInfos.TRANSMITTANCE_TEXTURE_HEIGHT;
    cb.IRRADIANCE_TEXTURE_WIDTH = mLutInfos.IRRADIANCE_TEXTURE_WIDTH;
    cb.IRRADIANCE_TEXTURE_HEIGHT = mLutInfos.IRRADIANCE_TEXTURE_HEIGHT;
    cb.SCATTERING_TEXTURE_R_SIZE = mLutInfos.SCATTERING_TEXTURE_R_SIZE;
    cb.SCATTERING_TEXTURE_MU_SIZE = mLutInfos.SCATTERING_TEXTURE_MU_SIZE;
    cb.SCATTERING_TEXTURE_MU_S_SIZE = mLutInfos.SCATTERING_TEXTURE_MU_S_SIZE;
    cb.SCATTERING_TEXTURE_NU_SIZE = mLutInfos.SCATTERING_TEXTURE_NU_SIZE;
    cb.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = Vector3(114974.916437f, 71305.954816f, 65310.548555f); // Not used if using LUTs as transfert
    cb.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = Vector3(98242.786222f, 69954.398112f, 66475.012354f);  // idem


    cb.gSkyViewProjMat = mViewProjMat;
    {
        cb.gSkyInvViewProjMat = mViewProjMat.Invert();
    }
    {
        cb.gSkyInvProjMat = mProjMat.Invert();
    }
    {
        cb.gSkyInvViewMat = mViewMat.Invert();
    }

    cb.gShadowmapViewProjMat = mShadowmapViewProjMat;

    cb.camera = mCamPosFinal;
    cb.view_ray = mViewDir;
    cb.sun_direction = mSunDir;


    if (mAtmosphereCB)
        mAtmosphereCB->CopyData(0, cb);

    if (mCommonCB)
        mCommonCB->CopyData(0, mCommonConstanants);
}
