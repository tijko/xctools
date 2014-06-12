DefinitionBlock ("ssdt_large_res.aml", "SSDT", 2, "LRes", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		Device (ECDC)
		{
			Name (_HID, EisaId ("PNP0C09"))
			Name (_UID, 0x01)

			Name (_CRS, ResourceTemplate ()
			{
				Memory32Fixed (ReadOnly,
					0xFED4C000,         // Address Base
					0x012B4000,         // Address Length
				)
				QWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
					0x0000000000000000,         // Granularity
					0x00000C1000000000,         // Range Minimum
					0x00000C1FFFFFFFFF,         // Range Maximum
					0x0000000000000000,         // Translation Offset
					0x0000001000000000,         // Length
					,, , AddressRangeMemory, TypeStatic)				
				DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
					0x00000000,         // Granularity
					0xD8000000,         // Range Minimum
					0xDAFFFFFF,         // Range Maximum
					0x00000000,         // Translation Offset
					0x03000000,         // Length
					,, , AddressRangeMemory, TypeStatic)
				DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
					0x00000000,         // Granularity
					0x00000D00,         // Range Minimum
					0x0000FFFF,         // Range Maximum
					0x00000000,         // Translation Offset
					0x0000F300,         // Length
					,, , TypeStatic)
				DWordSpace (0xCA, ResourceProducer, PosDecode, MinFixed, MaxFixed, 0x6B,
					0x00000000,         // Granularity
					0xF1000000,         // Range Minimum
					0xF100FFFF,         // Range Maximum
					0x00000000,         // Translation Offset
					0x00010000,         // Length
					,, )
				WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
					0x0000,             // Granularity
					0x0000,             // Range Minimum
					0x0CF7,             // Range Maximum
					0x0000,             // Translation Offset
					0x0CF8,             // Length
					,, , TypeStatic)
				ExtendedMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
					0x0000000000000000,         // Granularity
					0x00E4400000000000,         // Range Minimum
					0x00E47FFFFFFFFFFF,         // Range Maximum
					0x0000000000000000,         // Translation Offset
					0x0000400000000000,         // Length
					0x0000000000000004,         // Type Specific Attributes
					, AddressRangeMemory, TypeStatic)
			})
		}

		Device (ECDD)
		{
			Name (_HID, EisaId ("PNP0C09"))
			Name (_UID, 0x02)

			Name (_PRS, ResourceTemplate ()
			{
				// The ResourceSourceIndex and ResourceSource are a bit confusing and it is hard to
				// find examples but basically it indicated that this resource is sourced from the
				// 2nd (DWordMemory) descriptor in ECDC._CRS
				DWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
					0x00000000,         // Granularity
					0xD8000000,         // Range Minimum
					0xD8FFFFFF,         // Range Maximum
					0x00000000,         // Translation Offset
					0x01000000,         // Length
					3, "_SB.ECDC", , AddressRangeMemory, TypeStatic)
				// Resource with fixed size, variable location
				DWordMemory (ResourceProducer, PosDecode, MinNotFixed, MaxNotFixed, WriteCombining, ReadWrite,
					0x0000001F,         // Granularity
					0x00C00000,         // Range Minimum
					0x00E80000,         // Range Maximum
					0x00000000,         // Translation Offset
					0x00040000,         // Length
					,, , AddressRangeMemory, TypeStatic)
				// Resource with variable size, variable location
				DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxNotFixed, Prefetchable, ReadWrite,
					0x000003FF,         // Granularity
					0x07000000,         // Range Minimum
					0x0740FFFF,         // Range Maximum
					0x00000000,         // Translation Offset
					0x00000000,         // Length
					,, , AddressRangeMemory, TypeStatic)
				Interrupt (ResourceConsumer, Edge, ActiveLow, Exclusive, ,, )
				{
					0x00000019,
					0x00000023,
				}
				Register (SystemIO, 
					0x10,               // Bit Width
					0x00,               // Bit Offset
					0x0000000000001000, // Address
					,)
			})
		}
	}
}
