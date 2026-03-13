SJASMPLUS=../sjasmplusMAC

all: 	eewriter_romset.bin

eewriter_romset.bin : eewriter_romset.asm sstwriter_romset_6.5.asm dzx7_turbo.asm digits.asm kempston_serial_romset.asm
	$(SJASMPLUS) eewriter_romset.asm

clean:
	rm -rf *.bin


