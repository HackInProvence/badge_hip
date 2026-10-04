/* Always reserve a sector for persistent configuration */
__reserved_sector_start = ORIGIN(FLASH) + LENGTH(FLASH) - 4096;
ASSERT(__flash_binary_end <= __reserved_sector_start, "firmware overlaps the reserved flash sector")

/* This section let a binary flash to this section, see reset.h */
SECTIONS
{
    .reserved_sector __reserved_sector_start : {
        KEEP(*(.reserved_sector))
    } /*> FLASH */ /* It's the LENGTH(FLASH) that ensures the zone is in the FLASH region */
}
