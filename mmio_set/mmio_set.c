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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <pciaccess.h>
#include <errno.h>

struct pci_dev
{
    struct pci_device * dev;
    int bar;
    uint32_t bar_base;
    uint32_t bar_size;
    void *map;
};

static void
usage (void)
{
  fprintf (stderr, "Usage:\n"
	   "mmio_set bb:dd.f bar reg[.bwl]\n"
	   "mmio_set bb:dd.f bar reg[.bwl][|&^]=[~]val\n"
	   "\n"
	   "o= is equivalent to |=\n"
	   "n= is equivalent to &=\n"
	   "x= is equivalent to ^=\n"
	   "reg and val can be in hex octal or decimal\n");
  exit (1);
}

#define mmio_read(map, reg, type)    \
    *(volatile type *)(((void *)((char*)map) + reg))

static uint8_t mmio_read8(struct pci_dev *p, uint32_t reg) { return mmio_read(p->map, reg, uint8_t); }
static uint16_t mmio_read16(struct pci_dev *p, uint32_t reg) { return mmio_read(p->map, reg, uint16_t); }
static uint32_t mmio_read32(struct pci_dev *p, uint32_t reg) { return mmio_read(p->map, reg, uint32_t); }

#define mmio_write(map, reg, val, type)    \
    *(volatile type *)(((void *)((char*)map) + reg)) = val

static void mmio_write8(struct pci_dev *p, uint32_t reg, uint8_t val) { mmio_write(p->map, reg, val, uint8_t); }
static void mmio_write16(struct pci_dev *p, uint32_t reg, uint16_t val) { mmio_write(p->map, reg, val, uint16_t); }
static void mmio_write32(struct pci_dev *p, uint32_t reg, uint32_t val) { mmio_write(p->map, reg, val, uint32_t); }

static void
print_write_register (struct pci_dev *p, uint32_t reg, int len, uint32_t val)
{
  switch (len)
    {
    case 8:
      printf ("*( uint8_t) (%08x+0x%08x)  =       0x%02x\n", p->bar_base, reg, val);
      break;
    case 16:
      printf ("*(uint16_t) (%08x+0x%08x)  =     0x%04x\n", p->bar_base, reg, val);
      break;
    case 32:
      printf ("*(uint32_t) (%08x+0x%08x)  = 0x%08x\n", p->bar_base, reg, val);
      break;
    }
}


static uint32_t
print_read_register (struct pci_dev *p, uint32_t reg, int len)
{
  uint32_t ret = -1;
  switch (len)
    {
    case 8:
      ret = mmio_read8 (p, reg);
      printf ("*( uint8_t) (%08x+0x%08x) ==       0x%02x\n", p->bar_base, reg, ret);
      break;
    case 16:
      ret = mmio_read16 (p, reg);
      printf ("*(uint16_t) (%08x+0x%08x) ==     0x%04x\n", p->bar_base, reg, ret);
      break;
    case 32:
      ret = mmio_read32 (p, reg);
      printf ("*(uint32_t) (%08x+0x%08x) == 0x%08x\n", p->bar_base, reg, ret);
      break;
    }
  return ret;
}

static void
open_pci_device(struct pci_dev *p, const char *bdf, const char *bar)
{
    unsigned int b = 0, d = 0, f = 0;

    if (sscanf(bdf, "%02x:%02x.%01x", &b, &d, &f) != 3)
    {
        fprintf(stderr, "wrong bdf format, format is bb:dd.f exampe 00:02.0\n");
        usage();
    }

    p->dev = pci_device_find_by_slot(0, b, d, f);
    if (!p->dev)
    {
        fprintf(stderr, "pci_device_find_by_slot failed: %s\n", strerror(errno));
        usage();
    }

    if (pci_device_probe(p->dev))
    {
        fprintf(stderr, "pci_device_probe failed: %s\n", strerror(errno));
        usage();
    }

    p->bar = strtol(bar, NULL, 0);
    if (p->bar < 0 || p->bar > 5)
    {
        fprintf(stderr, "bar out of range bar:%d\n", p->bar);
        usage();
    }

    p->bar_base = p->dev->regions[p->bar].base_addr;
    p->bar_size = p->dev->regions[p->bar].size;

    if (pci_device_map_range(p->dev,
                p->bar_base, p->bar_size,
                PCI_DEV_MAP_FLAG_WRITABLE | PCI_DEV_MAP_FLAG_WRITE_COMBINE,
                ((void**)&p->map)))
    {
        fprintf(stderr, "pci_device_map_range failed: %s\n", strerror(errno));
        usage();
    }

    printf("map %08x[%x]\n", p->bar_base, p->bar_size);
}

int
main (int argc, char *argv[])
{
  uint32_t reg, val = 0, reg_val = 0;
  struct pci_dev p;

  char sreg[80], *sval, *sreg_size, *ptr;
  int reg_len, and = 0, or = 0, xor = 0;

  if (argc != 4)
  {
    fprintf(stderr, "wrong number of arguments\n");
    usage ();
  }

  pci_system_init();

  open_pci_device(&p, argv[1], argv[2]);

  strcpy (sreg, argv[3]);

  if ((sval = strstr (sreg, "&=")) || (sval = strstr (sreg, "n=")))
    {
      and++;
      *sval = 0;
      sval += 2;
    }
  else if ((sval = strstr (sreg, "|=")) || (sval = strstr (sreg, "o=")))
    {
      or++;
      *sval = 0;
      sval += 2;
    }
  else if ((sval = strstr (sreg, "^=")) || (sval = strstr (sreg, "x=")))
    {
      xor++;
      *sval = 0;
      sval += 2;
    }
  else if ((sval = index (sreg, '=')))
    {
      *sval = 0;
      sval++;
    }

  if (sval)
    {
      if (*sval == '~')
	{
	  val = strtoll (sval + 1, &ptr, 0);
	  val = ~val;
	}
      else
	{
	  val = strtoll (sval, &ptr, 0);
	}

      if (*ptr)
	usage ();
    }

  sreg_size = index (sreg, '.');
  if (sreg_size)
    {
      *sreg_size = 0;
      sreg_size++;
      if (strlen (sreg_size) != 1)
	usage ();

      switch (*sreg_size)
	{
	case 'b':
	  reg_len = 8;
	  break;
	case 'w':
	  reg_len = 16;
	  break;
	case 'l':
	  reg_len = 32;
	  break;
	default:
	  usage ();
	}
    }
  else
    {
      reg_len = 32;
    }


  if (!strlen (sreg))
    usage ();

  reg = strtol (sreg, &ptr, 0);

  if (*ptr)
    usage ();

  reg_val = print_read_register (&p, reg, reg_len);

  if (!sval)
    return 0;

  if (and)
    {
      reg_val &= val;
    }
  else if (or)
    {
      reg_val |= val;
    }
  else if (xor)
    {
      reg_val ^= val;
    }
  else
    {
      reg_val = val;
    }

  print_write_register (&p, reg, reg_len, reg_val);

  switch (reg_len)
    {
    case 8:
      mmio_write8 (&p, reg, reg_val);
      break;
    case 16:
      mmio_write16 (&p, reg, reg_val);
      break;
    case 32:
      mmio_write32 (&p, reg, reg_val);
      break;
    }


  (void) print_read_register (&p, reg, reg_len);

  return 0;
}
