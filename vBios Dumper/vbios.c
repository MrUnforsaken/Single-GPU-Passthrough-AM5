#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint32_t ULONG;
typedef uint8_t UCHAR;
typedef uint16_t USHORT;

typedef struct {
    ULONG Signature;
    ULONG TableLength; // Length
    UCHAR Revision;
    UCHAR Checksum;
    UCHAR OemId[6];
    UCHAR OemTableId[8]; // UINT64  OemTableId;
    ULONG OemRevision;
    ULONG CreatorId;
    ULONG CreatorRevision;
} AMD_ACPI_DESCRIPTION_HEADER;

typedef struct {
    AMD_ACPI_DESCRIPTION_HEADER SHeader;
    UCHAR TableUUID[16]; // 0x24
    ULONG VBIOSImageOffset; // 0x34. Offset to the first GOP_VBIOS_CONTENT block from the beginning of the stucture.
    ULONG Lib1ImageOffset; // 0x38. Offset to the first GOP_LIB1_CONTENT block from the beginning of the stucture.
    ULONG Reserved[4]; // 0x3C
} UEFI_ACPI_VFCT;

typedef struct {
    ULONG PCIBus; // 0x4C
    ULONG PCIDevice; // 0x50
    ULONG PCIFunction; // 0x54
    USHORT VendorID; // 0x58
    USHORT DeviceID; // 0x5A
    USHORT SSVID; // 0x5C
    USHORT SSID; // 0x5E
    ULONG Revision; // 0x60
    ULONG ImageLength; // 0x64
} VFCT_IMAGE_HEADER;

typedef struct {
    VFCT_IMAGE_HEADER VbiosHeader;
    UCHAR VbiosContent[1];
} GOP_VBIOS_CONTENT;

int main(int argc, char** argv)
{
    FILE* fp_vfct;
    FILE* fp_vbios;
    UEFI_ACPI_VFCT* pvfct;
    char vbios_name[0x400];

    if (!(fp_vfct = fopen("/sys/firmware/acpi/tables/VFCT", "r"))) {
        perror(argv[0]);
        return -1;
    }

    if (!(pvfct = malloc(sizeof(UEFI_ACPI_VFCT)))) {
        perror(argv[0]);
        return -1;
    }

    if (sizeof(UEFI_ACPI_VFCT) != fread(pvfct, 1, sizeof(UEFI_ACPI_VFCT), fp_vfct)) {
        fprintf(stderr, "%s: failed to read VFCT header!\n", argv[0]);
        return -1;
    }

    ULONG offset = pvfct->VBIOSImageOffset;
    ULONG tbl_size = pvfct->SHeader.TableLength;

    if (!(pvfct = realloc(pvfct, tbl_size))) {
        perror(argv[0]);
        return -1;
    }

    if (tbl_size - sizeof(UEFI_ACPI_VFCT) != fread(pvfct + 1, 1, tbl_size - sizeof(UEFI_ACPI_VFCT), fp_vfct)) {
        fprintf(stderr, "%s: failed to read VFCT body!\n", argv[0]);
        return -1;
    }

    fclose(fp_vfct);

    while (offset < tbl_size) {
        GOP_VBIOS_CONTENT* vbios = (GOP_VBIOS_CONTENT*)((char*)pvfct + offset);
        VFCT_IMAGE_HEADER* vhdr = &vbios->VbiosHeader;

        if (!vhdr->ImageLength)
            break;

        snprintf(vbios_name, sizeof(vbios_name), "vbios_%x_%x.bin", vhdr->VendorID, vhdr->DeviceID);

        if (!(fp_vbios = fopen(vbios_name, "wb"))) {
            perror(argv[0]);
            return -1;
        }

        if (vhdr->ImageLength != fwrite(&vbios->VbiosContent, 1, vhdr->ImageLength, fp_vbios)) {
            fprintf(stderr, "%s: failed to dump vbios %x:%x\n", argv[0], vhdr->VendorID, vhdr->DeviceID);
            return -1;
        }

        fclose(fp_vbios);

        printf("dump vbios %x:%x to %s\n", vhdr->VendorID, vhdr->DeviceID, vbios_name);

        offset += sizeof(VFCT_IMAGE_HEADER);
        offset += vhdr->ImageLength;
    }

    return 0;
}
