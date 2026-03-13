SJASMPLUS=../sjasmplusMAC

all: 	eewriter_slot31.rom

eewriter_slot31.rom : eewriter_slot31.asm sstwriter_romset_6.5.asm digits.asm kempston_serial_romset.asm screen.scr
	$(SJASMPLUS) eewriter_slot31.asm

clean:
	rm -rf *.rom


