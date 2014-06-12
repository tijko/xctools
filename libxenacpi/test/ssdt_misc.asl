DefinitionBlock ("ssdt_misc.aml", "SSDT", 2, "Misc", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		Name (MBF1, Buffer (0x08) {0x0E, 0x23, 0xF5, 0x51, 0x77, 0x96, 0xCD, 0x46})
		Method (MSC1, 2, Serialized)
		{
			ToBuffer (0x41414141, Local0)			
			ToBuffer ("TESTSTR", Local1)
			Store (ToBuffer (Arg0), Local2)
			ToDecimalString (1024, Local3)
			ToHexString (0x5555, Local4)
			ToString (MBF1, 0x08, Arg1)
		}
		
		Name (BLEN, 0x333)
		Method (MSC2, 2, Serialized)
		{
			Store ( 0x110, Local0)
			Store ( Buffer (Local0) {}, Local1)
			Store ( Buffer (0x88) {}, Arg0)
			Store ( Buffer (BLEN) {}, Arg1)
		}

		Name (DBTB, Package (0x05)
		{
			0x07, 
			0x38, 
			0x01C0, 
			0x0E00, 
			0x3F, 
		})
		Name (LARE, Package (0x06) {})
	}
}
