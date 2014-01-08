/* efiShow.c -- an application to show bmp pictures in uefi 
 * 
 * author : Li Bao & Wang Ran 
 * about  : this is our class project on Embedded System course, PKU, 2013
 * 
 * Usage  : 1. just type "efiShow.efi" in the shell, it will run 
 *          2. bmp files must be put in "\pages" and must be named x.bmp (x=1,2,3 ...)
 *          3. bmp files must in 24-bit or 32-bit format 
 * 
 * Structure :
 *   1. efi_main : main function 
 *   2. ShowPage : show x.bmp picture
 *   3. ConvertBmpToBlt : convert bmp from file into buffer
 *   4. tostring : convert int to filename
 
+----------------------+    +---------------------+    +----------+
|efi_main:             | +->|ShowPage:            | +->|tostring: |
|  Initialize          | |  |  tostring(..)       |-+  |  ...     |
|  Load File System    | |  |  Open/Read Bmp File |    +----------+
|  Load Graphics Output| |  |  ConvertBmpToBlt(..)|-+   
|  while:              | |  |  Grahpics Show      | |  +----------------------+
|    Read Input        | |  +---------------------+ +->|ConvertBmpToBlt:      |   
|    ShowPage(..)      |-+                             |  Read Bmp Header     |
+----------------------+                               |  Put Pixels To Buffer|
                                                       +----------------------+
 * 
 * Notice :  You can copy and modify this code for any purpose!
 *
 */


#include<efi.h>
#include<efilib.h>

#pragma pack(1)

//  the format of bmp pixel (copy from EDK_sourcecode/MdePkg/Include/IndustryStandard/Bmp.h)
typedef struct {
  UINT8   Blue;
  UINT8   Green;
  UINT8   Red;
  UINT8   Reserved;
} BMP_COLOR_MAP;

// the header of bmp file (copy from EDK_sourcecode/MdePkg/Include/IndustryStandard/Bmp.h)
typedef struct {
  CHAR8         CharB;
  CHAR8         CharM;
  UINT32        Size;
  UINT16        Reserved[2];
  UINT32        ImageOffset;
  UINT32        HeaderSize;
  UINT32        PixelWidth;
  UINT32        PixelHeight;
  UINT16        Planes;          
  UINT16        BitPerPixel;     
  UINT32        CompressionType;
  UINT32        ImageSize;      
  UINT32        XPixelsPerMeter;
  UINT32        YPixelsPerMeter;
  UINT32        NumberOfColors;
  UINT32        ImportantColors;
} BMP_IMAGE_HEADER;

#pragma pack()

EFI_STATUS
ConvertBmpToBlt (    // convert bmp file to buffer 
  IN     VOID      *BmpImage,
  IN     UINTN     BmpImageSize,
  IN OUT VOID      **GopBlt,
  IN OUT UINTN     *GopBltSize,
     OUT UINTN     *PixelHeight,
     OUT UINTN     *PixelWidth
  );

EFI_STATUS 
ShowPage(           // show one bmp
	IN EFI_FILE *dir,
	IN INT32 index,
	IN EFI_SYSTEM_TABLE *systab,
	IN EFI_GRAPHICS_OUTPUT_PROTOCOL *grahpics_output
);

EFI_STATUS 
tostring(           // convert digital to string with ".bmp", for example : 9 -> 9.bmp
	IN	INT32 index, 
	OUT CHAR16 *filename
	);


EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab) {

	InitializeLib(image_handle, systab);

	EFI_STATUS status;
	
	EFI_GUID simplefs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL; // a simple file system protocol in efi
	                      // (different from UEFI specification. In specification, this is called EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID )
	EFI_FILE_IO_INTERFACE *simplefs;                      // the simple file system's interface
	                      // (different from UEFI specification. In specification, this is called EFI_SIMPLE_FILE_SYSTEM_PROTOCOL)
	EFI_FILE *root;               //  root directory 
	EFI_FILE *dir;                
	CHAR16 *dirname=L"\\pages"; // name of the directory where bmp pictures are put
	
	status=systab->BootServices->LocateProtocol(&simplefs_guid, NULL, (VOID **)&simplefs);
	if(EFI_ERROR(status)) Print(L"locate protocol failed \n");
	
	status=simplefs->OpenVolume(simplefs, &root);
	if(EFI_ERROR(status)) Print(L"open volume failed\n");
	
	status=root->Open(root, &dir, dirname, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	if(EFI_ERROR(status)) Print(L"open directory failed\n");


	EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics_output;
	EFI_GUID graphics_output_guid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	status=systab->BootServices->LocateProtocol(&graphics_output_guid, NULL, (VOID**)&graphics_output);
	if(EFI_ERROR(status)) Print(L"locate graphics output protocol failed\n");

	graphics_output->SetMode(graphics_output, graphics_output->Mode->Mode);  // set the console graphics mode 

	SIMPLE_INPUT_INTERFACE *input=systab->ConIn;   // get the input interface 
	EFI_INPUT_KEY key;
	INT32 index=1;             // the index of bmp file

	status = ShowPage(dir, index, systab, graphics_output);   // show the first bmp 
	if (EFI_ERROR(status))
	{
		Print(L"1.bmp not found or something wrong with 1.bmp \n");
		return status;
	}
	while(TRUE)
	{
		status=input->ReadKeyStroke(input, &key);
		if(status==EFI_NOT_READY) continue;
		if(key.ScanCode == 0x01)    // click up arrow -> previous bmp  
		{
			if (index>1)
			{	
				index--;
				ShowPage(dir, index, systab, graphics_output);
			}
		}
		if(key.ScanCode == 0x02)   // click down arrow -> next bmp
		{
			index++;
			status=ShowPage(dir, index, systab, graphics_output);
			if (status==EFI_NOT_FOUND)   // no this bmp, so show previous one
			{
				index--;
				ShowPage(dir, index, systab, graphics_output);
			}

		}
		if(key.ScanCode == 0x17)   // click Esc -> exit
			break;										
	}

	systab->ConOut->ClearScreen(systab->ConOut);
	return EFI_SUCCESS;
}


EFI_STATUS 
ShowPage(
	IN EFI_FILE *dir,
	IN INT32 index,
	IN EFI_SYSTEM_TABLE *systab,
	IN EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics_output
)
{
	EFI_STATUS status;
	EFI_FILE *file;
	CHAR16 filename[20];
	tostring(index, filename);

	status=dir->Open(dir, &file, filename, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	if(EFI_ERROR(status)) 
	{
	//	Print(L"open file failed\n");
		return status;
	}

	EFI_FILE_INFO *fileinfo;
	UINTN infosize = SIZE_OF_EFI_FILE_INFO; 
	EFI_GUID info_type = EFI_FILE_INFO_ID;

	status = file->GetInfo(file, &info_type, &infosize, NULL);  // get the info size of file (with NULL, it will return EFI_BUFFER_TOO_SMALL)
	if(EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) 
	{
	//	Print(L"get info size failed\n");
		return status;
	}
	systab->BootServices->AllocatePool(AllocateAnyPages, infosize, (VOID **)&fileinfo);
	status=file->GetInfo(file, &info_type, &infosize, fileinfo);  // get info of file
	if(EFI_ERROR(status))
	{
	//	Print(L"get file info failed \n ");
		return status;
	}

	VOID *image;
	UINTN filesize = fileinfo->FileSize;  // get filesize from info 
	systab->BootServices->AllocatePool(AllocateAnyPages, filesize, (VOID **)&image);
	status=file->Read(file, &filesize, image);  // read bmp file
	if(EFI_ERROR(status)) 
	{	
	//	Print(L"read image failed\n");
		return status;
	}

	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *bltbuffer=NULL;  // buffer to store all pixels of bmp
	UINTN bltsize, height, width; 
	INTN coordinate_x, coordinate_y;  // (coordinate_x,coordinate_y) : the position of the first pixel of bmp in the screen
	status=ConvertBmpToBlt(image, filesize, (VOID **)&bltbuffer, &bltsize, &height, &width );
	coordinate_x=(graphics_output->Mode->Info->HorizontalResolution/2)-(width/2);
	coordinate_y=(graphics_output->Mode->Info->VerticalResolution/2)-(height/2);
	if (coordinate_x < 0 || coordinate_y < 0)
	{
	//	Print(L"Bmp resolution is greater than screen\n");
		return EFI_UNSUPPORTED;
	}

	systab->ConOut->ClearScreen(systab->ConOut);
	
	status=graphics_output->Blt(graphics_output, bltbuffer, EfiBltBufferToVideo, 0, 0, (UINTN)coordinate_x, (UINTN)coordinate_y, width, height, width*sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));           // show bmp picture 
	
	systab->BootServices->FreePool(image);
	systab->BootServices->FreePool(bltbuffer);
	systab->BootServices->FreePool(fileinfo);

	return EFI_SUCCESS;
}


EFI_STATUS
ConvertBmpToBlt (
	IN     VOID      *BmpImage,        
	IN     UINTN     BmpImageSize,
	IN OUT VOID      **GopBlt,
	IN OUT UINTN     *GopBltSize,
	OUT UINTN     *PixelHeight,
	OUT UINTN     *PixelWidth
  )
{

	UINT8                         *ImageData;
	UINT8                         *ImageBegin;
	BMP_IMAGE_HEADER              *BmpHeader;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Blt;
	UINT64                        BltBufferSize;
	UINTN                         Height;
	UINTN                         Width;
	UINTN                         ImageIndex;

	/*
	 *  we don't check any information in the bmp header
	 */

	BmpHeader = (BMP_IMAGE_HEADER *)BmpImage;
	ImageBegin  = ((UINT8 *) BmpImage) + BmpHeader->ImageOffset;

	// just support the 24-bit and 32-bit bmp
	if (BmpHeader->BitPerPixel != 24 && BmpHeader->BitPerPixel != 32)
	{
		Print(L"Bmp is not 24-bit or 32-bit\n");
		return EFI_UNSUPPORTED;
	}

	BltBufferSize = BmpHeader->PixelWidth * BmpHeader->PixelHeight * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
	*GopBltSize = (UINTN) BltBufferSize;
	*GopBlt     = AllocatePool (*GopBltSize);   // the same as   systab->BootServices->AllocatePool 
	if (*GopBlt == NULL) 
	{
		return EFI_OUT_OF_RESOURCES;
	}

	*PixelWidth   = BmpHeader->PixelWidth;
	*PixelHeight  = BmpHeader->PixelHeight;

  /* Convert image from BMP to Blt buffer format 
   * In normal case, the pixels in bmp is stored from down to up, left to right
   */
	ImageData = ImageBegin;
	BltBuffer = *GopBlt;
	for (Height = 0; Height < BmpHeader->PixelHeight; Height++) 
	{
		Blt = &BltBuffer[(BmpHeader->PixelHeight - Height - 1) * BmpHeader->PixelWidth];
		for (Width = 0; Width < BmpHeader->PixelWidth; Width++, Blt++) 
		{
			switch (BmpHeader->BitPerPixel)
			{
				case 24:
					Blt->Blue   = *ImageData++;
					Blt->Green  = *ImageData++;
					Blt->Red    = *ImageData++;
					break;
				case 32:			
					ImageData++;
					Blt->Blue   = *ImageData++;
					Blt->Green  = *ImageData++;
					Blt->Red    = *ImageData++;	
					break;
				default:
					break;
			}
		}

		ImageIndex = (UINTN) (ImageData - ImageBegin);
		if ((ImageIndex % 4) != 0) 
		{
			// Bmp Image starts each row on a 32-bit boundary!
			ImageData = ImageData + (4 - (ImageIndex % 4));
		}
	}
	return EFI_SUCCESS;
}

EFI_STATUS tostring(INT32 index, CHAR16 *filename)
{
	if (index==0)
	{
		StrCpy(filename, L"0.bmp");
		return EFI_SUCCESS;
	}
	INT32 x=100000;
	INT32 c=0;
	while(index/x==0){x=x/10;}
	while(TRUE)
	{
		filename[c]=index/x+48;
		index=index%x;
		c++;
		x=x/10;
		if(x==0)
			break;
	}
	StrCpy(filename+c, L".bmp");
	return EFI_SUCCESS;
}
