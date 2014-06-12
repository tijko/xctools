DefinitionBlock ("ssdt_math.aml", "SSDT", 2, "Math", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		Name (PICC, 0x00000000)
		Name (PICD, 0x00000000)
		Method (MAT1, 2, Serialized)
		{
			Store (0x10, Local0)
			Add (Arg0, Local0, Local1)
			Subtract (0x4000, Arg1, Local2)
			Store (Divide (Local2, 0x10), Local3)
			Store (Mod (Local1, 0x3333), Local4)
			Multiply (Arg0, Arg1, PICD)
			Increment (Local1)
			Decrement (Local2)
		}
		
		Method (MAT2, 1, Serialized)
		{
			Store (0x4040, Local0)
			Store (0x4044, Local1)
			And (Local0, Local1, Local2)
			Or (Local0, Local1, Local3)
			XOr (0xCCCC, Local0, Local1)
			Store (Not (Local3), Local2)
			ShiftRight (Arg0, 8, PICC)
			NAnd (0x88888888, Local1, Local0)
		}
	}
}
