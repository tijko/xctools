DefinitionBlock ("ssdt_sync.aml", "SSDT", 2, "Sync", "AMLTEST", 0)
{
	Scope (\_SB)
	{
		Name (GPID, 0x00)
		Mutex (GPIX, 0x02)
		Name (BEVD, 0x33)
		Event (\_SB.BEVT)

		Method (GPIZ, 1, NotSerialized)
		{
			Acquire (GPIX, 0xFFFF)
			Store (Arg0, GPID)
			Release (GPIX)
			Sleep (0x0A)
		}

		Method (BEVW, 0, NotSerialized)
		{
			Wait (\_SB.BEVT, 0xFFFF)
			Return (BEVD)
		}

		Method (BEVS, 1, NotSerialized)
		{
			Store (Arg0, BEVD)
			Signal (\_SB.BEVT)
		}

		Method (BEVR, 0, NotSerialized)
		{
			Reset (\_SB.BEVT)
			Stall (0x32)
		}
	}
}
