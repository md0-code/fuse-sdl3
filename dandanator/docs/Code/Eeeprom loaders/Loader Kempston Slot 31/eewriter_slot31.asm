	OUTPUT eewriter_slot31.rom
	ORG	0x0000
			DI
			LD SP,$0000					; Set Stack to Ramtop
			EI							; Pause at least 20ms
			HALT
			HALT
			DI
			XOR A
			OUT ($FE),A
LOADSCR:	LD HL, ScrPTR				;	Load Screen
			LD DE, $4000
			LD BC, $1B00
			LDIR
			LD HL, ENDBOOT
			LD DE,$F000
			LD BC, ENDUP-COPYROM
			LDIR
			JP COPYROM
			
		block 0x38-$, 0x00
INTISR:		EI
			RET
		
	
ENDBOOT:			
	ORG $F000
			
COPYROM:	LD HL,0
			LD DE,40000
			LD BC,256
			LDIR						; Copy first 256 bytes of rom memory to RAM
			CALL DDNTRUNLOCK			; UnLock Dandanator Commands. Also page in slot 2
			LD B,0
			LD HL,0
			LD IX,40000
CHECKROMLP:	LD A, (HL)					; Check that internal rom is different from paged in slot 2
			LD D, (IX)
			XOR D
			JR NZ, CHECKROMOK			; If different at any point, we are good to go. Dandanator is detected
			INC HL
			INC IX
			DJNZ CHECKROMLP				; Check All bytes
NODANDANATOR:							; No dandanator detected or dandanator is disabled
TOSBOMB:	LD A, $47					; Bright White on paper 0
			LD B, 6						; 6 chars in TOSBOMB Graphic 
			LD IX, TBPOS
			LD HL, $4000+$1800			; Attributes on Screen for TOSBOMB
			LD D,0
PAINTTB:	LD E, (IX)					; Get offset
			ADD HL,DE					; Move HL to Attr position
			LD (HL),A					; Change TOSBOMB Color
			INC IX						; Next icon offset
			DJNZ PAINTTB				; Loop
			LD A,%01000110				; Wick Bright yellow
TBDLOCK:	
			LD (TOSBOMBATTR),A			; Change attribute to A colour
			LD BC,64
WAIT50ms:
			DJNZ WAIT50ms
			DEC C
			JR NZ,WAIT50ms
			XOR %00000100				; alternate yellow (110) with red (010)
			JR TBDLOCK					; Deadlock		
TBPOS:		DEFB	$41,$1,$1F,$1,$1,$1F; TOSBOMB attr relative position			
TOSBOMBATTR		equ		$5800+(2*32)+1	; Attribute of bomb's wick


CHECKROMOK:								; Dandanator was detected and it's now ready to receive 
			LD HL,1						; Select slot 0 and set it as reset slot
			LD A,1
			CALL SENDNRCMD
			LD A,39
			CALL SENDNRCMD
			
			LD A,1
			LD (NBLOCK),A
			
BUC:
			LD A,(NBLOCK)
			CALL DISPDIGIT				; Show number of block processing (1..16)
			DEC A						; A=0..15
			OR A						; Assure Carry is off
			RLA
			RLA
			RLA							; A * 8
			LD C,1						; 1 = LOADING, A = N.SECTOR
			LD B,8
BUCLOAD8:
			CALL DISPBAR				; DISPBAR returns with A=SECTOR Num (BC returns untouched PUSH-POP)
			INC A
			DJNZ BUCLOAD8
			
			LD HL,loadarea				; Destination of data 
			LD IY,loadsize				; Length of data (+1 for slot number + 2 for crc)
LOADSERIAL:	CALL LoadSerBlk				; Load block using Serial Kempston
AFTER_LOAD:	LD HL,loadarea+loadsize-3	; Address of number of slot from file
			LD A,(NBLOCK)				; number of 16k slot to write to eeprom (1-32)
			CP (HL)						; check if slot loaded is correct
			JR NZ,LOADERR				; If not the correct slot Show TOSBOMB

			ld de,loadarea				; IX=Begining of loaded area
			ld hl,0						; HL will have the sum
			ld bc,$8001					; $8000 + 1 (length of area and n.slot byte)
bucCRC:
			ld a,(de)
			add	a,l
			ld l,a
			jr nc,bucNOCarry
			inc h						; Include Carry if existing
bucNOCarry:
			inc de
			dec bc
			ld a,b
			or c
			jr nz,bucCRC
			
			ld ix,de
			ld de,(ix)					; Load DE with CRC from file

			or a						; clear carry flag
			sbc hl,de					; substract computed CRC
			jr z, ALLGOOD				; ok, continue with burning
LOADERR:	JP TOSBOMB					; Show Bomb and deadlock if CRC no ok
			
ALLGOOD:	LD HL,loadarea				; First address is scratch area
			CALL	PROGRAMSECTOR		; Erase and then burn 8 4k eeprom Sectors
			LD A,(NBLOCK)
			INC A
			LD (NBLOCK),A
			CP 17						
			JP C,BUC					; Cycle all 16 Blocks
			
ENDEND:		JP DDNTRRESET				; Enable Dandanator and jumpt to menu

NBLOCK:		DEFB 0x00					; Reserve space for var 
	

	


;------------------------------------------------------------------------------
; VARS
;------------------------------------------------------------------------------
loadsize		equ		$8003			; Length of data loaded = 32kbyte data + 1 byte + 2 bytes crc
loadarea		equ		$6F00-3			; Destination of load data (aligned to EF00 when done)
TXTSLOTCOUNTER	equ 	$5800+(8*32)+8	; Position of Slot Counter
TXTSECTCOUNTER	equ 	$5800+(18*32)+8	; Position of Slot Counter


				
;------------------------------------------------------------------------------
;PROGRAMSECTOR - 
; (NBLOCK) = Number of slot (1..16)
; HL = Address of data (32k = 8 sectors x 4K)
; C will be counting from 0 to 7
; Number in screen will be 1 to 8
; Combining (NBLOCK) and C will have the sector in range 0..127 stored in B register
; B will be copied to A prior to calling SSTSECERASE and SSTSECTPROG
; HL will begin with the address of first 4K sector and incremented 4k by 4k prior to calling SSTSECTPROG
;------------------------------------------------------------------------------
PROGRAMSECTOR:
			PUSH HL						; Page in External eeprom -> Needed for programming
			LD A,1
			LD HL,1
			CALL SENDNRCMD 
			POP HL
			LD C,0						; N.of sector in this programming area (0..7)
BUCPROGRAMSECTOR:
			PUSH HL						; Save Initial address
			PUSH BC						; Save copy of C (0..7)
			LD A,(NBLOCK)				; N.slot (1..16), need to convert in sector (*8)
			DEC A						; Convert to 0..15
			OR A						; Be sure there is not Carry
			RLA							; * 2
			RLA							; * 4
			RLA							; * 8 -> stored in acummulator A
			POP BC						; Retrieve copy of C (0..7)
			ADD C						; Add sector subnumber(0..7) to acummulator A
			LD B,A						; Copy Sector (0-127) from A to B for next usage
			PUSH BC						; Save copy of B and C

			LD C,2						; 2 = WRITING, A = N.SECTOR
			CALL DISPBAR				; DISPBAR returns with A=N.SECTOR

			CALL SSTSECERASE			; Tell Dandanator to erase sector in register A
			POP BC						; Retrieve copy of B and C
			POP HL						; Recuperate Address of data
			PUSH HL						; Save this Address of data (4 of 32k)
			PUSH BC						; Save copy of B and C
			LD A, B						; Sector (0.127) to write
			CALL SSTSECTPROG			; Tell Dandanator to write sector in register A with data begining HL
			POP BC						; Retrieve copy of B and C (only C is needed this time, B is n.sector 0..127)
			PUSH BC						; Save copy of B and C
			LD A,B						; A = N.Sector
			LD C,3						; 3 = FINISHED, A = N.SECTOR
			CALL DISPBAR				; DISPBAR returns with A=N.SECTOR

			POP BC						; Retrieve copy of B and C (only C is needed this time, B is discarded now)
			POP HL						; Recuperate Address of data

			LD DE,$1000					; Lenght of sectors = 4Kbyte = 1024*4 = 4096 = $1000
			ADD HL,DE					; Calculate next address
			INC C						; Next subsector 0..7
			LD A,C						; Copy the value of this sector to acumulator A
			CP 8						; Check A<8 (only 0..7 is valid)
			JR C,BUCPROGRAMSECTOR		; REPEAT WHILE subsector<8
			
			RET
cursector:
		DEFB	0

		


;------------------------------------------------------------------------------
; Includes
;------------------------------------------------------------------------------
		
		
		include "sstwriter_romset_6.5.asm"
		
		include "kempston_serial_romset.asm"
		
		include "digits.asm"
ENDUP:		
		
		ORG $-COPYROM+ENDBOOT
		
ScrPTR:	incbin "screen.scr"

		block 0x4000-$, 0x00