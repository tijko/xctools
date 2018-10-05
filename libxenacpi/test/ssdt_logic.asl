DefinitionBlock ("ssdt_logic.aml", "SSDT", 2, "Logic", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		Name (LGCX, 0xA5A5A5A5)
		Method (LOG1, 1, Serialized)
		{
			Store (0x1, Local0)
			If ( LEqual( Arg0, Local0) )
			{
				Store ( LNot (Arg0), LGCX)
			}
			Else
			{
				Store ( LLess (Local0, Arg0), LGCX)
			}			
		}
		
		Method (LOG2, 2, Serialized)
		{
			Store (0x4, Local0)			
			If ( LGreaterEqual( Arg0, Local0))
			{
				Store ( LAnd (Local0, Arg1), Local1)
			}
			Else
			{
				Store ( LOr (Local0, Arg1), Local1)
			}
			Store (0xEEEEEEEE, LGCX)
			Return (Local1)
		}
	}
}
