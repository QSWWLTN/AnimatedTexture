#include "AnimatedTexture2D.h"
#include "AnimatedTextureResource.h"
#include "gif_load/gif_load.h"

bool isGifData(const void* data) {
	return FMemory::Memcmp(data, "GIF", 3) == 0;
}

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
	UE_LOG(LogTexture, Log, TEXT("NotCompressSize: %d CompressSize: %d"), NumPixel, Frame->CompPixelIndicesSize);

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

FGIFFrame UAnimatedTexture2D::GetCacheFrame() {
	if (Frames.Contains(NowFramIndex)) {

		FGIFFrame Frame;
		FGIFFrame* T = Frames[NowFramIndex];
		
		Frame.Time = T->Time;
		Frame.Index = T->Index;
		Frame.Width = T->Width;
		Frame.Height = T->Height;
		Frame.OffsetX = T->OffsetX;
		Frame.OffsetY = T->OffsetY;
		Frame.Interlacing = T->Interlacing;
		Frame.Mode = T->Mode;
		Frame.TransparentIndex = T->TransparentIndex;
		Frame.Palette = T->Palette;

		Frame.PixelIndicesSize = T->PixelIndicesSize;
		Frame.CompPixelIndicesSize = T->CompPixelIndicesSize;

		uint64 MaxSize = LZ4_compressBound(T->PixelIndicesSize) * 4;
		uint8* PixelIndices = (uint8*)calloc(MaxSize, sizeof(uint8));
		int32 Temp = LZ4_decompress_safe((char*)T->PixelIndices, (char*)PixelIndices, T->CompPixelIndicesSize, MaxSize);

		UE_LOG(LogTexture, Log, TEXT("%d"), Temp);

		Frame.PixelIndices = PixelIndices;

		return Frame;
	}
	return FGIFFrame();
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
			free(Data.Value->PixelIndices);
		}
		free(Data.Value);
	}
}

bool UAnimatedTexture2D::ImportGIF(const uint8* Buffer, uint32 BufferSize)
{
	IsStart = true;
	Frames.Empty();
	RawData.SetNumUninitialized(BufferSize);
	FMemory::Memcpy(RawData.GetData(), Buffer, BufferSize);

	GIF_Load((void*)RawData.GetData(), RawData.Num(), GIFFrameLoader1, 0, (void*)this, NowFramIndex);

	return ParseRawData();
}

void UAnimatedTexture2D::Import_Init(uint32 InGlobalWidth, uint32 InGlobalHeight, uint8 InBackground, uint32 InFrameCount)
{
	GlobalWidth = InGlobalWidth;
	GlobalHeight = InGlobalHeight;
	Background = InBackground;

	FrameNum = InFrameCount;
}

void UAnimatedTexture2D::PostInitProperties()
{
	Super::PostInitProperties();
}

bool UAnimatedTexture2D::ParseRawData()
{
	int Ret = GIF_Load((void*)RawData.GetData(), RawData.Num(), GIFFrameLoader1, 0, (void*)this, 0L);
	IsStart = false;

	if (Ret < 0) {
		UE_LOG(LogTexture, Warning, TEXT("gif format error."));
		return false;
	}
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
