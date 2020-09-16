/* This generated file is in the public domain */
/* $FreeBSD: head/lib/libefivar/uefi-guid.c 314926 2017-03-09 00:31:31Z imp $ */

#include <uuid.h>
#include <stdint.h>

typedef struct {          
    uint32_t  Data1;
    uint16_t  Data2;
    uint16_t  Data3;
    uint8_t   Data4[8]; 
} EFI_GUID;

extern EFI_GUID    gEfiDebugPortProtocolGuid;
extern EFI_GUID    gEfiPcAnsiGuid;
extern EFI_GUID    gEfiPersistentVirtualCdGuid;
extern EFI_GUID    gEfiPersistentVirtualDiskGuid;
extern EFI_GUID    gEfiSasDevicePathGuid;
extern EFI_GUID    gEfiUartDevicePathGuid;
extern EFI_GUID    gEfiVT100Guid;
extern EFI_GUID    gEfiVT100PlusGuid;
extern EFI_GUID    gEfiVTUTF8Guid;
extern EFI_GUID    gEfiVirtualCdGuid;
extern EFI_GUID    gEfiVirtualDiskGuid;

EFI_GUID    gEfiDebugPortProtocolGuid      = { 0xEBA4E8D2, 0x3858, 0x41EC, { 0xA2, 0x81, 0x26, 0x47, 0xBA, 0x96, 0x60, 0xD0 }} ;
EFI_GUID    gEfiPcAnsiGuid                 = { 0xE0C14753, 0xF9BE, 0x11D2, { 0x9A, 0x0C, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }} ;
EFI_GUID    gEfiPersistentVirtualCdGuid    = { 0x08018188, 0x42CD, 0xBB48, {0x10, 0x0F, 0x53, 0x87, 0xD5, 0x3D, 0xED, 0x3D }} ;
EFI_GUID    gEfiPersistentVirtualDiskGuid  = { 0x5CEA02C9, 0x4D07, 0x69D3, {0x26, 0x9F ,0x44, 0x96, 0xFB, 0xE0, 0x96, 0xF9 }} ;
EFI_GUID    gEfiSasDevicePathGuid          = { 0xd487ddb4, 0x008b, 0x11d9, { 0xaf, 0xdc, 0x00, 0x10, 0x83, 0xff, 0xca, 0x4d }} ;
EFI_GUID    gEfiUartDevicePathGuid         = { 0x37499a9d, 0x542f, 0x4c89, { 0xa0, 0x26, 0x35, 0xda, 0x14, 0x20, 0x94, 0xe4 }} ;
EFI_GUID    gEfiVT100Guid                  = { 0xDFA66065, 0xB419, 0x11D3, { 0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }} ;
EFI_GUID    gEfiVT100PlusGuid              = { 0x7BAEC70B, 0x57E0, 0x4C76, { 0x8E, 0x87, 0x2F, 0x9E, 0x28, 0x08, 0x83, 0x43 }} ;
EFI_GUID    gEfiVTUTF8Guid                 = { 0xAD15A0D6, 0x8BEC, 0x4ACF, { 0xA0, 0x73, 0xD0, 0x1D, 0xE7, 0x7E, 0x2D, 0x88 }} ;
EFI_GUID    gEfiVirtualCdGuid              = { 0x3D5ABD30, 0x4175, 0x87CE, {0x6D, 0x64, 0xD2, 0xAD, 0xE5, 0x23, 0xC4, 0xBB }} ;
EFI_GUID    gEfiVirtualDiskGuid            = { 0x77AB535A, 0x45FC, 0x624B, {0x55, 0x60, 0xF7, 0xB2, 0x81, 0xD1, 0xF9, 0x6E }} ;
