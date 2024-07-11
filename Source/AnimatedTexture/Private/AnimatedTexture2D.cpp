#include "AnimatedTexture2D.h"
#include "AnimatedTextureResource.h"

#include "AnimatedTextureModule.h"
/*
#include "gif_load/gif_load.h"
*/

#ifdef UseZstd
#include "zstd.h"
#endif

#include "GifLib/gif_lib.h"

bool isGifData(const void* data) {
	return FMemory::Memcmp(data, "GIF", 3) == 0;
}

FGIFFrame* UAnimatedTexture2D::GetCacheFrame() {
	if (Frames.Contains(NowFramIndex)) {
		return Frames[NowFramIndex];
	}
	return nullptr;
}

float UAnimatedTexture2D::GetCacheFrameDelay() {
	return DefaultFrameDelay;
}

float UAnimatedTexture2D::GetSurfaceWidth() const
{
	return GlobalWidth;
}

float UAnimatedTexture2D::GetSurfaceHeight() const
{
	return GlobalHeight;
}

FTextureResource* UAnimatedTexture2D::CreateResource()
{
	FTextureResource* NewResource = new FAnimatedTextureResource(this);
	return NewResource;
}

uint32 UAnimatedTexture2D::CalcTextureMemorySizeEnum(ETextureMipCount Enum) const
{
	if(GlobalWidth>0 && GlobalHeight>0) 
	{

		uint32 Flags = SRGB ? (uint32)TexCreate_SRGB : 0;
		uint32 NumMips = 1;
		uint32 NumSamples = 1;
		uint32 TextureAlign;
		FRHIResourceCreateInfo CreateInfo(TEXT(""));
		uint32 Size = (uint32)RHICalcTexture2DPlatformSize(GlobalWidth, GlobalHeight, PF_B8G8R8A8, 1, 1, (ETextureCreateFlags)Flags, FRHIResourceCreateInfo(TEXT("")), TextureAlign);

		return Size;
	}

	return 4;
}

#if WITH_EDITOR
void UAnimatedTexture2D::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool RequiresNotifyMaterials = false;
	bool ResetAnimState = false;

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		static const FName SupportsTransparencyName = GET_MEMBER_NAME_CHECKED(UAnimatedTexture2D, SupportsTransparency);

		if (PropertyName == SupportsTransparencyName)
		{
			RequiresNotifyMaterials = true;
			ResetAnimState = true;
		}
	}

	if (RequiresNotifyMaterials)
		NotifyMaterials();
}
#endif

float UAnimatedTexture2D::GetAnimationLength() const
{
	return Duration;
}


UAnimatedTexture2D::UAnimatedTexture2D(const FObjectInitializer& ObjectInitializer)
:Super(ObjectInitializer)
{

}

void UAnimatedTexture2D::PostLoad()
{
	ParseRawData();
	Super::PostLoad();
}

void UAnimatedTexture2D::BeginDestroy() 
{
	Super::BeginDestroy();

	for (auto& Data : Frames) {
		if (Data.Value->PixelIndices != nullptr) {

			FMemory::Free(Data.Value->PixelIndices);
			Data.Value->PixelIndices = nullptr;
		}
		FMemory::Free(Data.Value);
		Data.Value = nullptr;
	}
}

struct GifData {
	const unsigned char* m_lpBuffer;
	size_t m_nBufferSize;
	size_t m_nPosition;
};

int32 ANIMATEDTEXTURE_API InternalRead_Mem(GifFileType* gif, GifByteType* buf, int len) {
	if (len == 0) {
		return 0;
	}

	GifData* pData = (GifData*)gif->UserData;
	if (pData->m_nPosition > pData->m_nBufferSize) {
		return 0;
	}

	uint32 nRead;
	if (pData->m_nPosition + len > pData->m_nBufferSize || pData->m_nPosition + len < pData->m_nPosition) {
		nRead = (uint32)(pData->m_nBufferSize - pData->m_nPosition);
	}
	else {
		nRead = len;
	}

	memcpy((char*)buf, (char*)pData->m_lpBuffer + pData->m_nPosition, nRead);
	pData->m_nPosition += nRead;

	return nRead;
}

/*
void ANIMATEDTEXTURE_API GIFFrameLoader1(void* data, struct GIF_WHDR* whdr)
{
	UAnimatedTexture2D* OutGIF = (UAnimatedTexture2D*)data;

	if (OutGIF->IsStart) {
		OutGIF->Duration += whdr->time >= 0 ? whdr->time * 0.01f : (-whdr->time - 1) * 0.01f;
	}

	if (OutGIF->GetFrameCount() == 0) {
		OutGIF->Import_Init(whdr->xdim, whdr->ydim, whdr->bkgd, whdr->nfrm);
	}

	if (OutGIF->IsStart) {
		OutGIF->FramNum++;
	}

	int FrameIndex = whdr->ifrm;

	FGIFFrame* Frame = (FGIFFrame*)calloc(1, sizeof(FGIFFrame));
	check(Frame);

	if (whdr->time >= 0)
		Frame->Time = whdr->time * 0.01f;
	else
		Frame->Time = (-whdr->time - 1) * 0.01f;

	Frame->Index = whdr->ifrm;
	Frame->Width = whdr->frxd;
	Frame->Height = whdr->fryd;
	Frame->OffsetX = whdr->frxo;
	Frame->OffsetY = whdr->fryo;
	Frame->Interlacing = whdr->intr != 0;
	Frame->Mode = whdr->mode;
	Frame->TransparentIndex = whdr->tran;

	int NumPixel = Frame->Width * Frame->Height;
	Frame->PixelIndicesSize = NumPixel;

	int32 MaxCompSize = LZ4_compressBound(NumPixel);
	Frame->PixelIndices = (uint8*)calloc(MaxCompSize, sizeof(uint8));

	Frame->CompPixelIndicesSize = LZ4_compress_default((char*)whdr->bptr, (char*)Frame->PixelIndices, NumPixel, MaxCompSize);

	OutGIF->NotCompSize += NumPixel;
	OutGIF->CompSize += Frame->CompPixelIndicesSize;

	int PaletteSize = whdr->clrs;
	Frame->Palette.Init(FColor(0, 0, 0, 255), PaletteSize);
	for (int i = 0; i < PaletteSize; i++)
	{
		FColor& uc = Frame->Palette[i];
		uc.R = whdr->cpal[i].R;
		uc.G = whdr->cpal[i].G;
		uc.B = whdr->cpal[i].B;
	}// end of for

	OutGIF->NowFramIndex = FrameIndex;
	OutGIF->Frames.Add(FrameIndex, Frame);
}
*/

void UAnimatedTexture2D::OpenGIF() {

	LLM_SCOPE(ELLMTag((int)EAnimatedTextureLLMTag::AnimTexture));

	UE_LOG(LogTexture, Log, TEXT("%s GifFileDataSize: %.1f MB"), *GetName(), (float)RawData.Num() / 1024.f / 1024.f);

	GifData data;
	memset(&data, 0, sizeof(data));
	data.m_lpBuffer = RawData.GetData();
	data.m_nBufferSize = RawData.Num();

	GifFileType* GifFileData = DGifOpen(&data, InternalRead_Mem, nullptr);
	if (!GifFileData) {
		UE_LOG(LogTexture, Error, TEXT("Load Gif File Error"));
		return;
	}

	double Delay = 0.f;
	int32 DisposalMode = -1;
	int32 TransparentIndex = -1;
	GifRecordType RecordType;

	do {
		if (DGifGetRecordType(GifFileData, &RecordType) == GIF_ERROR) {

			UE_LOG(LogTexture, Error, TEXT("Load Gif File Error"));

			DGifCloseFile(GifFileData, nullptr);
			return;
		}

		Background = GifFileData->SBackGroundColor;

		switch (RecordType) {
			case IMAGE_DESC_RECORD_TYPE:
			{
				FGIFFrame* Frame = (FGIFFrame*)FMemory::Malloc(sizeof(FGIFFrame));
				memset(Frame, 0, sizeof(FGIFFrame));

				if (DGifGetImageDesc(GifFileData) == GIF_ERROR) {
					UE_LOG(LogTexture, Error, TEXT("Load Gif File Error"));
					DGifCloseFile(GifFileData, nullptr);
					return;
				}

				if (GifFileData->Image.Width <= 0 || GifFileData->Image.Height <= 0 || GifFileData->Image.Width > (INT_MAX / GifFileData->Image.Height) 
					|| GifFileData->Image.Left + GifFileData->Image.Width > GifFileData->SWidth || GifFileData->Image.Top + GifFileData->Image.Height > GifFileData->SHeight)
				{
					UE_LOG(LogTexture, Error, TEXT("Gif Width Or Height Error"));
					DGifCloseFile(GifFileData, nullptr);
					return;
				}

				Frame->Time = Delay;
				Frame->Index = GifFileData->ImageCount;
				Frame->Width = GifFileData->Image.Width;
				Frame->Height = GifFileData->Image.Height;
				Frame->OffsetX = GifFileData->Image.Left;
				Frame->OffsetY = GifFileData->Image.Top;
				Frame->Mode = DisposalMode;
				Frame->Interlacing = GifFileData->Image.Interlace;
				Frame->TransparentIndex = TransparentIndex;

				if (GetFrameCount() == 0) {
					Import_Init(GifFileData->SWidth, GifFileData->SHeight);
				}

				if (IsStart) {
					Duration += Frame->Time;
					FramNum++;
				}

				Frame->PixelIndicesSize = Frame->Width * Frame->Height;

				uint8** TextureTempBuff = (uint8**)FMemory::Malloc(Frame->Height * sizeof(uint8*));
				for (uint32 Y = 0; Y < Frame->Height; Y++) {
					TextureTempBuff[Y] = (uint8*)FMemory::Malloc(Frame->Width);
				}

				if (Frame->Interlacing) {
					int InterlacedOffset[] = { 0, 4, 2, 1 };
					int InterlacedJumps[] = { 8, 8, 4, 2 };

					for (uint32 i = 0; i < 4; ++i) {
						for (uint32 Row = Frame->OffsetY + InterlacedOffset[i]; Row < Frame->OffsetY + Frame->Height; Row += InterlacedJumps[i]) {
							if (DGifGetLine(GifFileData, TextureTempBuff[Row], Frame->Width) == GIF_ERROR) {

								for (uint32 Y = 0; Y < Frame->Height; Y++) {
									FMemory::Free(TextureTempBuff[Y]);
								}
								FMemory::Free(TextureTempBuff);

								FMemory::Free(Frame->PixelIndices);
								FMemory::Free(Frame);
								
								UE_LOG(LogTexture, Error, TEXT("Gif GetLine Error"));
								DGifCloseFile(GifFileData, nullptr);
								return;
							}
						}
					}
				}
				else {
					for (uint32 Row = 0; Row < Frame->Height; ++Row) {
						if (DGifGetLine(GifFileData, TextureTempBuff[Row], Frame->Width) == GIF_ERROR) {

							for (uint32 Y = 0; Y < Frame->Height; Y++) {
								FMemory::Free(TextureTempBuff[Y]);
							}
							FMemory::Free(TextureTempBuff);

							FMemory::Free(Frame->PixelIndices);
							FMemory::Free(Frame);

							UE_LOG(LogTexture, Error, TEXT("Gif GetLine Error"));
							DGifCloseFile(GifFileData, nullptr);
							return;
						}
					}
				}

				Frame->PixelIndices = (uint8*)FMemory::Malloc(Frame->PixelIndicesSize);
				//需要换另一种方法
				for (uint32 Y = 0; Y < Frame->Height; Y++) {
					for (uint32 X = 0; X < Frame->Width; X++) {
						uint64 TexIndex = Frame->Width * Y + X;

						Frame->PixelIndices[TexIndex] = TextureTempBuff[Y][X];
					}
					FMemory::Free(TextureTempBuff[Y]);
				}
				FMemory::Free(TextureTempBuff);

#ifdef UseZstd
				int32 MaxCompSize = ZSTD_compressBound(Frame->PixelIndicesSize);
				uint8* TempBuff = (uint8*)FMemory::Malloc(MaxCompSize);
				check(TempBuff);

				uint64 CompPixelIndicesSize = ZSTD_compress(TempBuff, MaxCompSize, Frame->PixelIndices, Frame->PixelIndicesSize, CompressLevel);
				FMemory::Free(Frame->PixelIndices);
				TempBuff = (uint8*)FMemory::Realloc(TempBuff, CompPixelIndicesSize);
				Frame->PixelIndices = TempBuff;

				Frame->CompPixelIndicesSize = CompPixelIndicesSize;
				CompSize += Frame->CompPixelIndicesSize;
#else
				int32 MaxCompSize = LZ4_compressBound(Frame->PixelIndicesSize);
				uint8* TempBuff = (uint8*)FMemory::Malloc(MaxCompSize);
				check(TempBuff);

				uint64 CompPixelIndicesSize = LZ4_compress_default((char*)Frame->PixelIndices, (char*)TempBuff, Frame->PixelIndicesSize, MaxCompSize);
				FMemory::Free(Frame->PixelIndices);
				TempBuff = (uint8*)FMemory::Realloc(TempBuff, CompPixelIndicesSize);
				Frame->PixelIndices = TempBuff;

				Frame->CompPixelIndicesSize = CompPixelIndicesSize;
				CompSize += Frame->CompPixelIndicesSize;
#endif

				NowFramIndex = GifFileData->ImageCount - 1;
				NotCompSize += Frame->PixelIndicesSize;
				
				ColorMapObject* TargetColorMap = GifFileData->Image.ColorMap == nullptr ? GifFileData->SColorMap : GifFileData->Image.ColorMap;
				int PaletteSize = TargetColorMap->ColorCount;
				Frame->Palette.Init(FColor(0, 0, 0, 255), PaletteSize);
				for (int i = 0; i < PaletteSize; i++)
				{
					FColor& uc = Frame->Palette[i];
					uc.R = TargetColorMap->Colors[i].Red;
					uc.G = TargetColorMap->Colors[i].Green;
					uc.B = TargetColorMap->Colors[i].Blue;
				}

				Frames.Add(NowFramIndex, Frame);
				break;
			}
			case EXTENSION_RECORD_TYPE:
			{
				GifByteType* ExGifData = nullptr;
				int32 ExType = 0;

				if (DGifGetExtension(GifFileData, &ExType, &ExGifData)) {
					while (ExGifData) {
						switch (ExType) {
							case GRAPHICS_EXT_FUNC_CODE:
							{
								GraphicsControlBlock GCB;
								if (DGifExtensionToGCB(ExGifData[0], ExGifData + 1, &GCB)) {
									Delay = GCB.DelayTime * 0.01;
									DisposalMode = GCB.DisposalMode;
									TransparentIndex = GCB.TransparentColor;
								}
								break;
							}
						}

						if (DGifGetExtensionNext(GifFileData, &ExGifData) == GIF_ERROR) {
							UE_LOG(LogTexture, Error, TEXT("Gif ExData Error"));
							DGifCloseFile(GifFileData, nullptr);
							return;
						}
					}
				}
				break;
			}
		default:
			break;
		}

	} while (RecordType != TERMINATE_RECORD_TYPE);

	DGifCloseFile(GifFileData, nullptr);
	GifFileData = nullptr;

	UE_LOG(LogTexture, Log, TEXT("Texture: %s, NotCompSize: %.1f MB, CompSize: %.1f MB"), *GetName(), (float)NotCompSize / 1024.f /1024.f, CompSize / 1024.f / 1024.f);
}

bool UAnimatedTexture2D::ImportGIF(const uint8* Buffer, uint32 BufferSize)
{
	LLM_SCOPE(ELLMTag((int)EAnimatedTextureLLMTag::LoadAnimTexture));

	IsStart = true;
	Frames.Empty();
	RawData.Init(0, BufferSize);
	FMemory::Memcpy(RawData.GetData(), Buffer, BufferSize);

	UE_LOG(LogTexture, Log, TEXT("GifFileDataSize: %.1f"), (float)BufferSize / 1024.f / 1024.f);

	OpenGIF();

	return ParseRawData();
}

void UAnimatedTexture2D::Import_Init(uint32 InGlobalWidth, uint32 InGlobalHeight, uint8 InBackground, uint32 InFrameCount)
{
	GlobalWidth = InGlobalWidth;
	GlobalHeight = InGlobalHeight;
	Background = InBackground;

	FrameNum = InFrameCount;
}

void UAnimatedTexture2D::Import_Init(uint32 InGlobalWidth, uint32 InGlobalHeight)
{
	GlobalWidth = InGlobalWidth;
	GlobalHeight = InGlobalHeight;
}

void UAnimatedTexture2D::PostInitProperties()
{
	Super::PostInitProperties();
}

bool UAnimatedTexture2D::ParseRawData()
{
	OpenGIF();
	IsStart = false;

	return true;
}

void UAnimatedTexture2D::Play()
{
	bPlaying = true;
}

void UAnimatedTexture2D::PlayFromStart()
{
	bPlaying = true;
}

void UAnimatedTexture2D::Stop()
{
	bPlaying = false;
}
