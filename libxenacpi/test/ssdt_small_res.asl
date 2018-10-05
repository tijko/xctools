DefinitionBlock ("ssdt_small_res.aml", "SSDT", 2, "SRes", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		Device (LNKA)
		{
			Name (_HID, EisaId ("PNP0C0F"))
			Name (_UID, 0x00)

			Name (_PRS, ResourceTemplate ()
			{
				IRQ (Level, ActiveLow, Shared, )
					{1,3,4,5,6,7,10,12,14,15}
				IRQ (Level, ActiveLow, Shared, IRQZ)
					{2}
			})
		}

		Device (ECDA)
		{
			Name (_HID, EisaId ("PNP0C09"))
			Name (_UID, 0x01)
			Name (_CRS, ResourceTemplate ()
			{
				IO (Decode16,
					0x00F4,             // Range Minimum
					0x00F8,             // Range Maximum
					0x01,               // Alignment
					0x20,               // Length
					_Y00
				)
				IO (Decode16,
					0x4000,             // Range Minimum
					0x40f0,             // Range Maximum
					0x01,               // Alignment
					0x20,               // Length
				)
				DMA (Compatibility, NotBusMaster, Transfer8_16, )
					{2,4}
				FixedIO (0x0130,             // Address
						 0x04,               // Length
				)
				VendorShort ()		// Length = 0x03
				{
					0xA7, 0x45, 0x3D
				}
			})
			// Note the descriptor name is simply used within the ASL
			// code as a convenience. It disappears in the AML. This is how
			// it would be used though to reference a value within a given
			// resource descriptor.
			CreateWordField (_CRS, \_SB.ECDA._Y00._MIN, MIO7) 

			Name (RBFA, ResourceTemplate ()
			{
				StartDependentFn (0x00, 0x00)
				{
					IO (Decode16,
						0x0378,             // Range Minimum
						0x0378,             // Range Maximum
						0x01,               // Alignment
						0x08,               // Length
					)
					IRQNoFlags ()
						{5,7}					
				}
				EndDependentFn ()
			})
		}
	}
}
