/******************************************************************************

	systems/compis.c
	machine driver

	Per Ola Ingvarsson
	Tomas Karlsson

	Hardware:
		- Intel 80186 CPU 8MHz, integrated DMA(8237?), PIC(8259?), PIT(8253?)
                - Intel 80130 OSP Operating system processor (PIC 8259, PIT 8254)
		- Intel 8274 MPSC Multi-protocol serial communications controller (NEC 7201)
		- Intel 8255 PPI Programmable peripheral interface 
		- Intel 8253 PIT Programmable interval timer
		- Intel 8251 USART Universal synchronous asynchronous receiver transmitter
		- National 58174 Real-time clock (compatible with 58274)
	Peripheral:
		- Intel 82720 GDC Graphic display processor (NEC uPD 7220)
		- Intel 8272 FDC Floppy disk controller (Intel iSBX-218A)
		- Western Digital WD1002-05 Winchester controller

	Memory map:

	00000-3FFFF	RAM	LMCS (Low Memory Chip Select)
	40000-4FFFF	RAM	MMCS 0 (Midrange Memory Chip Select)
	50000-5FFFF	RAM	MMCS 1 (Midrange Memory Chip Select)
	60000-6FFFF	RAM	MMCS 2 (Midrange Memory Chip Select)
	70000-7FFFF	RAM	MMCS 3 (Midrange Memory Chip Select)
	80000-EFFFF	NOP
	F0000-FFFFF	ROM	UMCS (Upper Memory Chip Select)

 ******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/i82720.h"
#include "includes/compis.h"
#include "devices/mflopimg.h"
#include "devices/printer.h"
#include "formats/cpis_dsk.h"
#include "cpuintrf.h"

static ADDRESS_MAP_START( compis_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE( 0x00000, 0x3ffff) AM_RAM
	AM_RANGE( 0x40000, 0x4ffff) AM_RAM
	AM_RANGE( 0x50000, 0x5ffff) AM_RAM
	AM_RANGE( 0x60000, 0x6ffff) AM_RAM
	AM_RANGE( 0x70000, 0x7ffff) AM_RAM
	AM_RANGE( 0x80000, 0xeffff) AM_NOP
	AM_RANGE( 0xf0000, 0xfffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( compis_io, ADDRESS_SPACE_IO, 8)
	AM_RANGE( 0x0001, 0x0008) AM_READWRITE( compis_ppi_r, compis_ppi_w )	/* PPI 8255			*/
	AM_RANGE( 0x0080, 0x0087) AM_READWRITE( compis_pit_r, compis_pit_w )	/* PIT 8253			*/
	AM_RANGE( 0x0100, 0x011a) AM_READWRITE( compis_rtc_r, compis_rtc_w ) 	/* RTC 58174			*/
	AM_RANGE( 0x0280, 0x0282) AM_READWRITE( compis_osp_pic_r, compis_osp_pic_w ) /* PIC 8259 (80150/80130)	*/
//  AM_RANGE( 0x0288, 0x028e) AM_READWRITE( compis_osp_pit_r, compis_osp_pit_w ) /* PIT 8254 (80150/80130)	*/
	AM_RANGE( 0x0311, 0x031f) AM_READWRITE( compis_usart_r, compis_usart_w )	/* USART 8251 Keyboard		*/
	AM_RANGE( 0x0330, 0x033e) AM_READWRITE( compis_gdc_r, compis_gdc_w )	/* GDC 82720 PCS6:6		*/
	AM_RANGE( 0x0340, 0x0342) AM_READWRITE( compis_fdc_r, compis_fdc_w )	/* iSBX0 (J8) FDC 8272		*/
	AM_RANGE( 0x0351, 0x0351) AM_READ( compis_fdc_dack_r)	/* iSBX0 (J8) DMA ACK		*/
	AM_RANGE( 0xff00, 0xffff) AM_READWRITE( i186_internal_port_r, i186_internal_port_w)/* CPU 80186			*/
//{ 0x0100, 0x017e, compis_null_r },	/* RTC				*/
//{ 0x0180, 0x01ff, compis_null_r },	/* PCS3?			*/
//{ 0x0200, 0x027f, compis_null_r },	/* Reserved			*/
//{ 0x0280, 0x02ff, compis_null_r },	/* 80150 not used?		*/
//{ 0x0300, 0x0300, compis_null_r },	/* Cassette  motor		*/
//{ 0x0301, 0x030f, compis_null_r}, 	/* DMA ACK Graphics		*/
//{ 0x0310, 0x031e, compis_null_r },	/* SCC 8274 Int Ack		*/
//{ 0x0320, 0x0320, compis_null_r },	/* SCC 8274 Serial port		*/
//{ 0x0321, 0x032f, compis_null_r },	/* DMA Terminate		*/
//{ 0x0331, 0x033f, compis_null_r },	/* DMA Terminate		*/
//{ 0x0341, 0x034f, compis_null_r },	/* J8 CS1 (16-bit)		*/
//{ 0x0350, 0x035e, compis_null_r },	/* J8 CS1 (8-bit)		*/
//{ 0x0360, 0x036e, compis_null_r },	/* J9 CS0 (8/16-bit)		*/
//{ 0x0361, 0x036f, compis_null_r },	/* J9 CS1 (16-bit)		*/
//{ 0x0370, 0x037e, compis_null_r },	/* J9 CS1 (8-bit)		*/
//{ 0x0371, 0x037f, compis_null_r },	/* J9 CS1 (8-bit)		*/
//{ 0xff20, 0xffff, compis_null_r },	/* CPU 80186			*/
ADDRESS_MAP_END

/* COMPIS Keyboard */
INPUT_PORTS_START (compis)
	PORT_START /* 0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Esc") PORT_CODE(KEYCODE_ESC)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1 !") PORT_CODE(KEYCODE_1)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2") PORT_CODE(KEYCODE_2)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3 #") PORT_CODE(KEYCODE_3)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4 $") PORT_CODE(KEYCODE_4)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5 %") PORT_CODE(KEYCODE_5)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6 &") PORT_CODE(KEYCODE_6)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7 /") PORT_CODE(KEYCODE_7)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8 (") PORT_CODE(KEYCODE_8)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9 )") PORT_CODE(KEYCODE_9)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0 =") PORT_CODE(KEYCODE_0)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("+ ?") PORT_CODE(KEYCODE_MINUS)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("? `") PORT_CODE(KEYCODE_EQUALS)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("BACKSPACE") PORT_CODE(KEYCODE_BACKSPACE)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("TAB") PORT_CODE(KEYCODE_TAB)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("q Q") PORT_CODE(KEYCODE_Q)

	PORT_START /* 1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("w W") PORT_CODE(KEYCODE_W)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("e E") PORT_CODE(KEYCODE_E)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("r R") PORT_CODE(KEYCODE_R)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("t T") PORT_CODE(KEYCODE_T)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("y Y") PORT_CODE(KEYCODE_Y)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("u U") PORT_CODE(KEYCODE_U)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("i I") PORT_CODE(KEYCODE_I)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("o O") PORT_CODE(KEYCODE_O)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("p P") PORT_CODE(KEYCODE_P)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("? ?") PORT_CODE(KEYCODE_OPENBRACE)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("? ?") PORT_CODE(KEYCODE_CLOSEBRACE)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("ENTER") PORT_CODE(KEYCODE_ENTER)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("CAPS") PORT_CODE(KEYCODE_CAPSLOCK)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("a A") PORT_CODE(KEYCODE_A)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("s S") PORT_CODE(KEYCODE_S)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("d D") PORT_CODE(KEYCODE_D)

	PORT_START /* 2 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("f F") PORT_CODE(KEYCODE_F)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("g G") PORT_CODE(KEYCODE_G)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("h H") PORT_CODE(KEYCODE_H)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("j J") PORT_CODE(KEYCODE_J)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("k K") PORT_CODE(KEYCODE_K)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("l L") PORT_CODE(KEYCODE_L)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("? ?") PORT_CODE(KEYCODE_COLON)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("? ?") PORT_CODE(KEYCODE_QUOTE)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("'' *") PORT_CODE(KEYCODE_TILDE)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("LSHIFT") PORT_CODE(KEYCODE_LSHIFT)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("< >") PORT_CODE(KEYCODE_BACKSLASH)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("z Z") PORT_CODE(KEYCODE_Z)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("x X") PORT_CODE(KEYCODE_X)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("c C") PORT_CODE(KEYCODE_C)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("v V") PORT_CODE(KEYCODE_V)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("b B") PORT_CODE(KEYCODE_B)

	PORT_START /* 3 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("n N") PORT_CODE(KEYCODE_N)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("m M") PORT_CODE(KEYCODE_M)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", ;") PORT_CODE(KEYCODE_COMMA)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(". :") PORT_CODE(KEYCODE_STOP)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("-") PORT_CODE(KEYCODE_SLASH)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("RSHIFT") PORT_CODE(KEYCODE_RSHIFT)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("LSSHIFT") PORT_CODE(KEYCODE_LALT)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("LCTRL") PORT_CODE(KEYCODE_LCONTROL)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("SPACE") PORT_CODE(KEYCODE_SPACE)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("RCTRL") PORT_CODE(KEYCODE_RCONTROL)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("RSSHIFT") PORT_CODE(KEYCODE_RALT)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("INPASSA") PORT_CODE(KEYCODE_INSERT)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S?K") PORT_CODE(KEYCODE_PRTSCR)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("UTPL?NA") PORT_CODE(KEYCODE_DEL)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("START-STOP") PORT_CODE(KEYCODE_PAUSE)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("UP") PORT_CODE(KEYCODE_UP)

	PORT_START /* 4 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("AVBRYT") PORT_CODE(KEYCODE_SCRLOCK)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("LEFT") PORT_CODE(KEYCODE_LEFT)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("HOME") PORT_CODE(KEYCODE_HOME)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("RIGHT") PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("TABL") PORT_CODE(KEYCODE_PGUP)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("DOWN") PORT_CODE(KEYCODE_DOWN)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("TABR") PORT_CODE(KEYCODE_PGDN)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("COMPIS !") PORT_CODE(KEYCODE_F3)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("COMPIS ?") PORT_CODE(KEYCODE_F4)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("COMPIS |") PORT_CODE(KEYCODE_F5)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("COMPIS S") PORT_CODE(KEYCODE_NUMLOCK)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 7") PORT_CODE(KEYCODE_7_PAD)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 8") PORT_CODE(KEYCODE_8_PAD)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 9") PORT_CODE(KEYCODE_9_PAD)

	PORT_START /* 5 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 4") PORT_CODE(KEYCODE_4_PAD)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 5") PORT_CODE(KEYCODE_5_PAD)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 6") PORT_CODE(KEYCODE_6_PAD)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 1") PORT_CODE(KEYCODE_1_PAD)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 2") PORT_CODE(KEYCODE_2_PAD)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 3") PORT_CODE(KEYCODE_3_PAD)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 0") PORT_CODE(KEYCODE_0_PAD)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 00") PORT_CODE(KEYCODE_SLASH_PAD)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP 000") PORT_CODE(KEYCODE_ASTERISK)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP ENTER") PORT_CODE(KEYCODE_ENTER_PAD)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP ,") PORT_CODE(KEYCODE_DEL_PAD)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP -") PORT_CODE(KEYCODE_MINUS_PAD)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("KP +") PORT_CODE(KEYCODE_PLUS_PAD)

	PORT_START /* 6 */
	PORT_DIPNAME( 0x18, 0x00, "S8 Test mode")
	PORT_DIPSETTING( 0x00, DEF_STR( Normal ) )
	PORT_DIPSETTING( 0x08, "Remote" )
	PORT_DIPSETTING( 0x10, "Stand alone" )
	PORT_DIPSETTING( 0x18, "Reserved" )

	PORT_START /* 7 */
	PORT_DIPNAME( 0x01, 0x00, "iSBX-218A DMA")
	PORT_DIPSETTING( 0x01, "Enabled" )
  PORT_DIPSETTING( 0x00, "Disabled" )
INPUT_PORTS_END

static unsigned i86_address_mask = 0x000fffff;

static const compis_gdc_interface i82720_interface =
{
	GDC_MODE_HRG,
	0x8000
};

static MACHINE_DRIVER_START( compis )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", I186, 8000000)	/* 8 MHz */
	MDRV_CPU_PROGRAM_MAP(compis_mem, 0)
	MDRV_CPU_IO_MAP(compis_io, 0)
	MDRV_CPU_VBLANK_INT(compis_vblank_int, 1)
	MDRV_CPU_CONFIG(i86_address_mask)

	MDRV_FRAMES_PER_SECOND(50)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(1)

	MDRV_MACHINE_RESET(compis)

	MDRV_COMPISGDC( &i82720_interface )

MACHINE_DRIVER_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START (compis)
     ROM_REGION (0x100000, REGION_CPU1, 0)
     ROM_LOAD ("compis.rom", 0xf0000, 0x10000, CRC(89877688) SHA1(7daa1762f24e05472eafc025879da90fe61d0225))
ROM_END

static void compis_printer_getinfo(const device_class *devclass, UINT32 state, union devinfo *info)
{
	/* printer */
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_COUNT:							info->i = 1; break;

		default:										printer_device_getinfo(devclass, state, info); break;
	}
}

static void compis_floppy_getinfo(const device_class *devclass, UINT32 state, union devinfo *info)
{
	/* floppy */
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_COUNT:							info->i = 2; break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_PTR_FLOPPY_OPTIONS:				info->p = (void *) floppyoptions_compis; break;

		default:										floppy_device_getinfo(devclass, state, info); break;
	}
}

SYSTEM_CONFIG_START(compis)
	CONFIG_DEVICE(compis_printer_getinfo)
	CONFIG_DEVICE(compis_floppy_getinfo)
SYSTEM_CONFIG_END

/*   YEAR	NAME		PARENT	COMPAT MACHINE	INPUT	INIT	CONFIG	COMPANY		FULLNAME */
COMP(1985,	compis,		0,		0,     compis,	compis,	compis,	compis,	"Telenova", "Compis" , 0)
