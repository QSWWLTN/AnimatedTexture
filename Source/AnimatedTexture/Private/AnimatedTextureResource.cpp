// Copyright 2019 Neil Fang. All Rights Reserved.

#include "AnimatedTextureResource.h"
#include "AnimatedTexture2D.h"

#include "DeviceProfiles/DeviceProfile.h"	// Engine
#include "DeviceProfiles/DeviceProfileManager.h"	// Engine

FAnimatedTextureResource::FAnimatedTextureResource(UAnimatedTexture2D * InOwner) 
:FTickableObjectRenderThread(false, false),
Owner(InOwner),
LastFrame(0)
{
#ifdef UseZstd
	DeCompress = ZSTD_createDStream();
#endif
}

void FAnimatedTextureResource::InitRHI(
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
	FRHICommandListBase&
#endif
)
{
	CreateSamplerStates(
		GetDefaultMipMapBias()
	);

	uint32 Flags = Owner->SRGB ? (int32)TexCreate_SRGB : 0;
	uint32 NumMips = 1;
	uint32 NumSamples = 1;

	FRHIResourceCreateInfo CreateInfo(TEXT(""));
	TextureRHI = RHICreateTexture(
		FRHITextureCreateDesc::Create2D(CreateInfo.DebugName)
		.SetExtent((int32)FMath::Max(GetSizeX(), 1u), (int32)FMath::Max(GetSizeY(), 1u))
		.SetFormat((EPixelFormat)(uint8)PF_B8G8R8A8)
		.SetNumMips((uint8)NumMips)
		.SetNumSamples((uint8)NumSamples)
		.SetFlags((ETextureCreateFlags)Flags)
		.SetInitialState(ERHIAccess::Unknown)
		.SetExtData(CreateInfo.ExtData)
		.SetBulkData(CreateInfo.BulkData)
		.SetGPUMask(CreateInfo.GPUMask)
		.SetClearValue(CreateInfo.ClearValueBinding)
	);
	TextureRHI->SetName(Owner->GetFName());

	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);

	/*
	if(Owner->GlobalHeight > 0 && Owner->GlobalWidth > 0)
	{
		DecodeFrameToRHI();
	}
	*/

	Register();
}

void FAnimatedTextureResource::ReleaseRHI()
{
	Unregister();

#ifdef UseZstd
	ZSTD_freeDStream(DeCompress);
#endif

	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, nullptr);
	FTextureResource::ReleaseRHI();
}

void FAnimatedTextureResource::Tick(float DeltaTime)
{
	float duration = FApp::GetCurrentTime() - Owner->GetLastRenderTimeForStreaming();
	bool bShouldTick = /*Owner->bAlwaysTickEvenNoSee ||*/ duration < 2.5f;

	if(Owner && Owner->GlobalHeight >0 && Owner->GlobalWidth > 0 && bShouldTick)
	{
		TickAnim(DeltaTime * Owner->PlayRate);
		return;
	}
}

bool FAnimatedTextureResource::TickAnim(float DeltaTime)
{
	static bool NextFrame = false;

	//如果没有加载完成先跳过这次刷新
	if (Owner->IsLoading) {
		return NextFrame;
	}

	//获取这一帧的时间
	float FrameDelay = Owner->GetFrameDelay();
	AnimState.FrameTime += DeltaTime;

	/*
	//当帧数过低的时候跳帧
	if (DeltaTime > 1.f / 20.f) {
		if (FMath::RandBool() && NextFrame) {
			NextFrame = false;
			return false;
		}
		NextFrame = true;
	}
	*/

	// step to next frame
	if (AnimState.FrameTime > FrameDelay) {

		AnimState.CurrentFrame++;
		Owner->NowFramIndex = AnimState.CurrentFrame;

		AnimState.FrameTime -= FrameDelay;
		NextFrame = true;

		// loop
		int NumFrame = Owner->GetFrameCount();
		if (AnimState.CurrentFrame >= NumFrame)
			Owner->NowFramIndex = AnimState.CurrentFrame = Owner->bLooping ? 0 : NumFrame - 1;
	}
	if(NextFrame)
	{
		DecodeFrameToRHI();
	}

	return NextFrame;
}

int32 FAnimatedTextureResource::GetDefaultMipMapBias() const
{
	return 0;
}

void FAnimatedTextureResource::CreateSamplerStates(float MipMapBias)
{
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap,
		MipMapBias
	);
	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

	FSamplerStateInitializerRHI DeferredPassSamplerStateInitializer
	(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap,
		MipMapBias,
		1,
		0,
		2
	);

	DeferredPassSamplerStateRHI = RHICreateSamplerState(DeferredPassSamplerStateInitializer);
}

void FAnimatedTextureResource::DecodeFrameToRHI()
{
	LLM_SCOPE(ELLMTag((int)EAnimatedTextureLLMTag::RHITexture));
	SCOPE_CYCLE_COUNTER(STAT_ReanderAnimTexture);

	if (FrameBuffer[0].Num() != Owner->GlobalHeight * Owner->GlobalWidth) {
		LastFrame = 0;

		FColor BGColor(0L);
		const FGIFFrame* GIFFrame = Owner->Frames[0];
		if (!Owner->SupportsTransparency) {
			BGColor = GIFFrame->Palette[Owner->Background];
		}

		for (int i = 0; i < 2; i++) {
			FrameBuffer[i].Init(BGColor, Owner->GlobalHeight * Owner->GlobalWidth);
		}
	}

	bool FirstFrame = AnimState.CurrentFrame == 0;
	FTexture2DRHIRef Texture2DRHI = TextureRHI->GetTexture2D();
	if (!Texture2DRHI)
		return;

	FGIFFrame* GIFFrame = Owner->GetCacheFrame();
	if (GIFFrame == nullptr || GIFFrame->PixelIndices == nullptr) {
		return;
	}

	uint32& InLastFrame = LastFrame;
	bool bSupportsTransparency = Owner->SupportsTransparency;

	FColor* PICT = FrameBuffer[InLastFrame].GetData();
	uint32 InBackground = Owner->Background;

	uint32 TexWidth = Texture2DRHI->GetSizeX();
	uint32 TexHeight = Texture2DRHI->GetSizeY();

	//-- decode to frame buffer
	/*
	uint32 DDest = TexWidth * GIFFrame.OffsetY + GIFFrame.OffsetX;
	uint32 Src = 0;
	uint32 Iter = GIFFrame.Interlacing ? 0 : 4;
	uint32 Fin = !Iter ? 4 : 5;

	for (; Iter < Fin; Iter++) // interlacing support
	{
		uint32 YOffset = 16U >> ((Iter > 1) ? Iter : 1);

		for (uint32 Y = (8 >> Iter) & 7; Y < GIFFrame.Height; Y += YOffset)
		{
			for (uint32 X = 0; X < GIFFrame.Width; X++)
			{
				uint32 TexIndex = TexWidth * Y + X + DDest;
				uint8 ColorIndex = GIFFrame.PixelIndices[Src];

				if (ColorIndex != GIFFrame.TransparentIndex)
					PICT[TexIndex] = GIFFrame.Palette[ColorIndex];
				else
				{
					int a = 0;
					a++;
				}

				Src++;
			}// end of for(x)
		}// end of for(y)
	}// end of for(iter)
	*/

#ifdef UseZstd
	uint64 MaxSize = GIFFrame->PixelIndicesSize;
#else
	uint64 MaxSize = LZ4_compressBound(GIFFrame->PixelIndicesSize);
#endif

	{
		LLM_SCOPE(ELLMTag((int)EAnimatedTextureLLMTag::DecompressTexture));

		uint8* LineBuff = (uint8*)FMemory::Malloc(MaxSize);
		memset(LineBuff, 0, MaxSize);

		SCOPE_CYCLE_COUNTER(STAT_DeCompress);
#ifdef UseZstd
		ZSTD_initDStream(DeCompress);
		ZSTD_DCtx_setParameter(DeCompress, ZSTD_d_windowLogMax, GIFFrame->PixelIndicesSize);

		ZSTD_inBuffer InputBuff = { GIFFrame->PixelIndices, GIFFrame->PixelIndicesSize, 0 };
		ZSTD_outBuffer OutBuff = { LineBuff, MaxSize, 0 };

		int32 Code = ZSTD_decompressStream(DeCompress, &OutBuff, &InputBuff);
		if (ZSTD_isError(Code)) {
			UE_LOG(LogTexture, Error, TEXT("deCompress GifData Error"));
		}
#else
		LZ4_decompress_safe((char*)GIFFrame->PixelIndices, (char*)LineBuff, GIFFrame->CompPixelIndicesSize, MaxSize);
#endif

		for (uint32 Y = 0; Y < GIFFrame->Height; Y++) {

			for (uint32 X = 0; X < GIFFrame->Width; X++) {

				uint64 TexIndex = GIFFrame->Width * Y + X;

				if (X < GIFFrame->Width && Y < GIFFrame->Height) {

					uint8 TexColorIndex = LineBuff[TexIndex];
					if (GIFFrame->Palette.IsValidIndex(TexColorIndex) && TexColorIndex != GIFFrame->TransparentIndex) {
						PICT[TexIndex] = GIFFrame->Palette[TexColorIndex];
					}
				}
			}
		}
		FMemory::Free(LineBuff);
	}

	//-- write texture
	uint32 DestPitch = 0;
	FColor* SrcBuffer = PICT;
	FColor* DestBuffer = (FColor*)RHILockTexture2D(Texture2DRHI, 0, RLM_WriteOnly, DestPitch, false);
	if (DestBuffer)
	{
		uint32 MaxRow = TexHeight;
		int ColorSize = sizeof(FColor);

		//UE_LOG(LogTemp, Log, TEXT("%d"), ssss);

		if (DestPitch == TexWidth * ColorSize)
		{
			FMemory::Memcpy(DestBuffer, SrcBuffer, DestPitch * MaxRow);
		}
		else
		{
			// copy row by row
			uint32 SrcPitch = TexWidth * ColorSize;
			uint32 Pitch = FMath::Min(DestPitch, SrcPitch);
			for (uint32 y = 0; y < MaxRow; y++)
			{
				FMemory::Memcpy(DestBuffer, SrcBuffer, Pitch);
				DestBuffer += DestPitch / ColorSize;
				SrcBuffer += SrcPitch / ColorSize;
			}// end of for
		}// end of else

		RHIUnlockTexture2D(Texture2DRHI, 0, false);
	}// end of if
	else
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("Unable to lock texture for write"));
	}// end of else

	//-- frame blending
	int32 Mode = GIFFrame->Mode;

	if (Mode == 3 && FirstFrame)	// loop restart
		Mode = 2;

	switch (Mode)
	{
	case 0:
	case 1:
		break;
	case 2:	// restore background
	{
		FColor BGColor(0L);

		if (bSupportsTransparency)
		{
			if (GIFFrame->TransparentIndex == -1)
				BGColor = GIFFrame->Palette[InBackground];
			else
				BGColor = GIFFrame->Palette[GIFFrame->TransparentIndex];
			BGColor.A = 0;
		}
		else
		{
			BGColor = GIFFrame->Palette[InBackground];
		}

		uint32 BGWidth = GIFFrame->Width;
		uint32 BGHeight = GIFFrame->Height;

		if (FirstFrame)
		{
			BGWidth = TexWidth;
			BGHeight = TexHeight;
		}

		for (uint32 Y = 0; Y < BGHeight; Y++)
		{
			for (uint32 X = 0; X < BGWidth; X++)
			{
				PICT[TexWidth * Y + X] = BGColor;
			}// end of for(x)
		}// end of for(y)
	}
	break;
	case 3:	// restore prevous frame
		InLastFrame = (InLastFrame + 1) % 2;
		break;
	default:
		UE_LOG(LogAnimTexture, Warning, TEXT("Unknown GIF Mode"));
		break;
	}//end of switch
}
