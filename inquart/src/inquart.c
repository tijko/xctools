/*
 * inquart.c:
 *
 * Copyright (c) 2011 James McKenzie <20@madingley.org>,
 * All rights reserved.
 *
 */

/*
 * Copyright (c) 2011 Citrix Systems, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


static char rcsid[] = "$Id:$";

/*
 * $Log:$
 */

#include "project.h"

void
print_bin (uint8_t v)
{
  printf ("%c%c%c%c%c%c%c%c",
          (v & 0x80) ? '1' : '0',
          (v & 0x40) ? '1' : '0',
          (v & 0x20) ? '1' : '0',
          (v & 0x10) ? '1' : '0',
          (v & 0x8) ? '1' : '0',
          (v & 0x4) ? '1' : '0',
          (v & 0x2) ? '1' : '0', (v & 0x1) ? '1' : '0');
}

void
print_bits (uint8_t v, char *s[])
{
  int c;

  for (c = 0x80; c; c >>= 1)
    {
      printf (" %c%s", (c & v) ? '+' : '-', *(s++));
    }
}




void
rx (uint16_t reg)
{
  uint8_t v = inb (reg);

  printf (" RX:      0x%02x ", v);
  print_bin (v);
  printf (" %c", ((v > 0x20) && (v < 0x7f)) ? v : '.');
  printf ("\n");
}

void
iir (uint16_t reg)
{
  uint8_t v = inb (reg);

  printf (" IIR:     0x%02x ", v);
  print_bin (v);


  switch (v & 0x6)
    {
    case 0x00:
      if (!(v & 1))
        printf (" (modem status)");
      break;
    case 0x02:
      printf (" (TX holding empty)");
      break;
    case 0x04:
      printf (" (RX data)");
      break;
    case 0x06:
      printf (" (line status)");
      break;
    }

  if (v == 0x1)
    printf (" (No pending interrupts)");

  printf ("\n");

}

void
ier (uint16_t reg)
{
  uint8_t v = inb (reg);
  char *bits[] =
    { "0x80", "0x40", "0x20", "0x10", "MSTATUS", "LSTATUS", "THREI", "RX" };

  printf (" IER:     0x%02x ", v);
  print_bin (v);

  print_bits (v, bits);
  printf ("\n");
}

void
lcr (uint16_t reg)
{
  uint8_t v = inb (reg);
  char *bits[] =
    { "DLAB", "SBC", "SPAR", "EPAR", "PARITY", "STOP", "WL1", "WL0" };

  printf (" LCR:     0x%02x ", v);
  print_bin (v);

  print_bits (v, bits);
  printf ("\n");
}

void
mcr (uint16_t reg)
{
  uint8_t v = inb (reg);
  char *bits[] =
    { "CLKSEL", "TCRTLR", "XONANY", "LOOP", "OUT2", "OUT1(INTEN)", "RTS",
    "DTR"
  };

  printf (" MCR:     0x%02x ", v);
  print_bin (v);

  print_bits (v ^ 0xf, bits);
  printf ("\n");
}

void
lsr (uint16_t reg)
{
  uint8_t v = inb (reg);
  char *bits[] = { "0x80", "TEMT", "THRE", "BI", "FE", "PE", "OE", "DR" };

  printf (" LSR:     0x%02x ", v);
  print_bin (v);

  print_bits (v, bits);
  printf ("\n");
}


void
msr (uint16_t reg)
{
  uint8_t v = inb (reg);
  char *bits[] = { "DCD", "RI", "DSR", "CTS", "DDCD", "DRI", "DDSR", "DCTS" };

  printf (" MSR:     0x%02x ", v);
  print_bin (v);

  print_bits (v, bits);
  printf ("\n");
}

void
divisor (uint16_t base)
{
  uint8_t dll, dlh, lcr;
  int divisor;
  float baud;

  lcr = inb (base + 3);
  outb (lcr | 0x80, base + 3);
  dll = inb (base);
  dlh = inb (base + 1);
  outb (lcr, base + 3);

  divisor = ((int) dlh << 8) | dll;

  printf (" DIVISOR: 0x%02x 0x%02x divisor=%d", (int) dlh, (int) dll,
          divisor);

  if (divisor)
    {
      printf (" baud=");
      baud = 115200 / (float) divisor;
      printf ("%.1f, ", baud);
      baud = 921600 / (float) divisor;
      printf ("%.1f, ", baud);
      baud = 1152000 / (float) divisor;
      printf ("%.1f, ", baud);
      baud = 4000000 / (float) divisor;
      printf ("%.1f", baud);
    }

  printf ("\n");
}

int dull(uint16_t base)
{

if (inb(base+3)!=0xff) return 0;
return 1;
}

void
inquart (uint16_t base)
{
  printf ("Uart at 0x%04x\n", base);

  if (dull(base)) return;

  iir (base + 2);
  ier (base + 1);
  lcr (base + 3);
  mcr (base + 4);
  lsr (base + 5);
  msr (base + 6);
  divisor (base);
  //rx (base);

  printf ("\n");
}

void
pci (void)
{
  struct pci_device_iterator *iter;
  struct pci_device *dev;
  int i;
  int err;
  uint16_t cmd;

  err = pci_system_init ();

  if (err)
    {
      fprintf (stderr, "Couldn't initialize PCI system: %s\n",
               strerror (err));
      exit (1);
    }

  iter = pci_id_match_iterator_create (NULL);
  while ((dev = pci_device_next (iter)))
    {

      if ((dev->device_class & 0xffff00) != 0x070000)
        continue;

      if (pci_device_probe (dev))
        {
          fprintf (stderr,
                   "pci_device_probe on %04x:%02x:%02x.%02x class %08x failed %s\n",
                   dev->domain, dev->bus, dev->dev, dev->func,
                   dev->device_class, strerror (err));
          continue;
        }

      if (pci_device_cfg_read_u16 (dev, &cmd, 4))
        {
          fprintf (stderr,
                   "attempt to read command register on %04x:%02x:%02x.%02x class %08x failed %s\n",
                   dev->domain, dev->bus, dev->dev, dev->func,
                   dev->device_class, strerror (err));

          continue;
        }

      if (!(cmd & 1))
        {
          fprintf (stderr,
                   "command register on %04x:%02x:%02x.%02x reads 0x%04x prohibiting access\n",
                   dev->domain, dev->bus, dev->dev, dev->func, cmd);

          continue;
        }




      for (i = 0; i < PCI_NUM_REGIONS; ++i)
        {

          if (!dev->regions[i].is_IO)
            continue;
          if (dev->regions[i].size != 8)
            continue;




          printf ("PCI %04x:%02x:%02x.%02x class %08x bar %d: ",
                  dev->domain, dev->bus, dev->dev, dev->func,
                  dev->device_class, i);

          inquart (dev->regions[i].base_addr);

        }

    }
  pci_iterator_destroy (iter);

}

void
usage (void)
{
  fprintf (stderr, "Usage:\n");
  fprintf (stderr, "inquart [base] [base] ...\n");
}


int
main (int argc, char *argv[])
{
  int base;
  char *ptr;

  iopl (3);

  argc--;
  argv++;

  if (!argc)
    {
      inquart (0x3f8);
      inquart (0x2f8);
      inquart (0x3e8);
      inquart (0x2e8);

      pci ();
      return 0;
    }

  while (argc--)
    {
      base = strtol (*argv, &ptr, 0);

      if (base <= 0 || !ptr || !*ptr)
        usage ();
      inquart (base);
    }

  return 0;
}
