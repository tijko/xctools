DefinitionBlock ("ssdt_device.aml", "SSDT", 2, "Dev", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		OperationRegion (OPR1, SystemIO, 0x96, 0x08)
		Field (OPR1, ByteAcc, NoLock, Preserve)
		{
			P96,   8,
			P97,   8,
			P98,   8,
			Offset(4),
			P100,  32   
		}
		
		Device (DEV0)
		{
			Name (_HID, EisaId ("PNP0A06"))
			Name (_UID, 0x00)
			
			Name (ABUF, Buffer (0x3C)
			{
				0x34, 0xF0, 0xB7, 0x5F, 0x63, 0x2C, 0xE9, 0x45,
				0xBE, 0x91, 0x3D, 0x44, 0xE2, 0xC7, 0x07, 0xE4,
				0x41, 0x41, 0x01, 0x02, 0x79, 0x42, 0xF2, 0x95,
				0x7B, 0x4D, 0x34, 0x43, 0x93, 0x87, 0xAC, 0xCD,
				0x80, 0x00, 0x01, 0x08, 0x18, 0x43, 0x81, 0x2B,
				0x9D, 0x84, 0xA1, 0x90, 0xA8, 0x59, 0xB5, 0xD0,
				0xA0, 0x00, 0xE8, 0x4B, 0x07, 0x47, 0x01, 0xC6,
				0x7E, 0xF6, 0x1C, 0x08
			})
			
			Method (INIT, 1, Serialized)
			{
				Store (100, P96)
				Store (Arg0, P98)
			}

			Method (GUID, 1, Serialized)
			{
				Store (101, P96)
				Store (0x0, Local0)
				Store (Arg0, Local1)

				While ( LLess (Local0, 0x10))
				{
					Add (Local1, Local0, Local2)
					Store (DerefOf (Index (ABUF, Local2)), P98)
					Increment (Local0)
					If ( LGreater (Local2, 0x400))
					{
						Break
					}
				}
			}

			Name (VDP3, Buffer (0x10) {})
			Method (VDP2, 2, NotSerialized)
			{				
				CreateByteField (VDP3, 0x00, VDP4)
				CreateWordField (VDP3, 0x01, VDP5)
				CreateDWordField (VDP3, 0x03, VDP6)
				CreateQWordField (VDP3, 0x08, VDP7)
			}

			Name (PLVL, Ones)
			PowerResource (LPP, 0x03, 0x0102)
			{
				Method (_STA, 0, NotSerialized)
				{
					Return (PLVL)
				}

				Method (_ON, 0, NotSerialized)
				{
					Store (One, PLVL)
				}

				Method (_OFF, 0, NotSerialized)
				{
					Store (Zero, PLVL)
				}
			}
		}
	}

	Scope (\_TZ)
	{
		ThermalZone (DM1Z)
		{
			Method (_TMP, 0, Serialized)
			{
				Return (0x0139)
			}

			Method (_CRT, 0, Serialized)
			{
				Return (0x0FAC) // 'bout 128 C
			}
		}
	}

	Scope (\_PR)
	{
		Processor (CPU0, 0x00, 0x00001010, 0x06) {}
		Processor (CPU1, 0x01, 0x00001010, 0x06) {}
	}

}
