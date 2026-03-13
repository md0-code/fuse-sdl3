	output 	boot.bin
	org     $5ccb
	jr	begin
	block	$5ccf-$,0
        db      $de, $c0, $37, $0e, $8f, $39, $96 ;OVER USR 7 ($5ccb)
begin:	di
	xor	a
	out 	($fe),a		;Border to black
	ld	hl,$5800
	ld 	de,$5801
	ld	bc,767
	ld 	(hl),a
	ldir			;Screen attributes to black

	ld	hl, screen
	ld	de, $4000
	call	zx7		;Uncompress screen
	ld	hl, (scrlen)
	ld	de, screen
	add 	hl, de
	ld	de, $F000
	call	zx7
	ld a,(flagbeeper)			;=0->No Beeper, <>0 -> Beeper active
	ld ($FFFF),A				; Store in $FFFF for eewriter routine
	jp $f000
zx7:
	include	"dzx7_turbo.asm"	;dzx7 decompression routine
flagbeeper:	defb 0
scrlen	defw	0
screen:

