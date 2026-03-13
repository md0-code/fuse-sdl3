SJASMPLUS=../sjasmplusMAC

all: 	eewriter.bin boot.bin

eewriter.bin : eewriter.asm misleches.asm sstwriter_6.5v.asm dzx7_turbo.asm digits.asm
	$(SJASMPLUS) eewriter.asm

boot.bin: boot.asm
	$(SJASMPLUS) boot.asm

clean:
	rm -rf *.bin


