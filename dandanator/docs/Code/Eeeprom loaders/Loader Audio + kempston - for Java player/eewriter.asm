	OUTPUT eewriter.bin
	ORG	$F000
			LD SP,$FFFF						; Set Stack to Ramtop
			
COPYROM:	LD HL,0
			LD DE,40000
			LD BC,256
			LDIR						; Copy first 256 bytes of rom memory to RAM
			CALL DDNTRUNLOCK			; UnLock Dandanator Commands. Also page in slot 0
			LD B,0
			LD HL,0
			LD IX,40000
CHECKROMLP:	LD A, (HL)
			LD D, (IX)
			XOR D
			JR NZ, CHECKROMOK
			INC HL
			INC IX
			DJNZ CHECKROMLP
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


CHECKROMOK:		

			LD A,(BEEPLOAD)				; Skip Pause if beeper is not active
			BIT 0,A						;  BIT 0 : Beep Function				1 = ACTIVE / 0 = Inactive
			JR Z, AFTERPAUSE	
			LD D,5						; Wait 1 sg aprox
			LD BC,0
WAIT500ms:
			DJNZ WAIT500ms
			DEC C
			JR NZ,WAIT500ms
			DEC D			
			JR NZ,WAIT500ms
AFTERPAUSE:			
			LD A,1
			LD (NBLOQUE),A
			
			CALL fftOK					; Send first FFTOK to indicate ready
			
BUC:
			ld A,(NBLOQUE)
			CALL DISPDIGIT				; Show number of block processing (1..16)
			DEC A						; A=0..15
			OR A						; Assure Carry is off
			RLA
			RLA
			RLA							; A * 8
			LD C,1						; 1 = LOADING, A = N.SECTOR
			LD B,8
BUCLOAD8:
			CALL DISPBAR				; DISPBAR returns with A=N.SECTOR (BC returns untouched PUSH-POP)
			INC A
			DJNZ BUCLOAD8
			
			LD IX,loadarea				; Destination of data 
			LD DE,loadsize				; Length of data (+1 for slot number + 2 for crc)
			EXX
			PUSH HL						; Save alternative registers needed for correct working
			PUSH DE
			PUSH BC
			EXX
			EX AF,AF
			PUSH AF						; Not for sure AF' is needed, but saved anyway
			EX AF,AF
			
			LD A, (BEEPLOAD)			; Check, whether Load from tape or Joystick
			LD B,A						; Copy to B for allow using A
			LD A,$FF					; Flag  for LD_BYTES
			RR B						;   throw away bit 0 (Beeper)
			RR B						;   check BIT 1 : Kempston Loading 			1 = ACTIVE
			JR C,LOADSERIAL				;   If Bit 1 active then Load From Serial Port
			;							Let's check if Standard Loading (1500bps)
			LD HL,$13C9					; H=$13 L=$C9 for Standard Loading (1500bps)
			RR B						;   check BIT 2 : Standard Loading (1500bps)	1 = ACTIVE
			JR C,LOADTAPE
			;							Let's check if Fast Loading		(3000bps)
			LD HL,$05BE					; H=$05 L=$BE for Fast Loading		(3000bps)
			RR B						;   check BIT 3 : Fast Loading		(3000bps)	1 = ACTIVE
			JR C,LOADTAPE
			;							Let's check if Turbo Loading	(4500bps)
			LD HL,$01BA					; H=$01 L=$BA for Turbo Loading	(4500bps)
			RR B						;   check BIT 4 : Turbo Loading	(4500bps)	1 = ACTIVE
			JR C,LOADTAPE
			;If Bits 1-4 were 0 is because Ultra was selected (This bit is not checked)
										;  BIT 5 : Ultra Loading				1 = ACTIVE
										;  It does not uses HL value for adjust speed so no need to initialize it
LOADTAPE:
			SCF
			CALL LD_BYTES				; call turbo load DIRECTLY (Expects Z=1)
			JR AFTER_LOAD	
LOADSERIAL:	CALL LoadSerBlk				; Load block using Serial Kempston
AFTER_LOAD:			
			EX AF,AF
			EXX		
			
			POP AF						; Not for sure AF' saving was needed, restore it back
			EX AF,AF
			POP BC						; Retrieve alternative registers needed for correct working
			POP DE
			POP HL
			EXX
			JR NC,LOADERR				; If wrong load, repeat segment			
			JR NZ,LOADERR				; If SPACE is pressed, repeat segment

			ld hl,loadarea+loadsize-3	; Address of number of slot from file
			LD A,(NBLOQUE)				; number of 16k slot to write to eeprom (1-32)
			CP (HL)						; check if slot loaded is correct
			JR NZ,LOADERR				; If not the correct slot, play sound for error and try loading again

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
			jr z, ALLGOOD				; IF not ZERO then repeat block: Play sound for error and try loading again
LOADERR:
			call fftERR					; Play Sound for Error
			jp BUC						; Try loading this slot again
ALLGOOD:
			XOR A						; Border to black (0)
			OUT ($FE),A
			CALL	fftOK				; Send FFT for OK, go to next segment
			LD HL,loadarea				; First address is scratch area
			PUSH IY
			PUSH IX
			CALL	PROGRAMSECTOR		; Erase and then burn 8 4k eeprom Sectors
			POP IX
			POP IY
			ld A,(NBLOQUE)
			INC A
			ld (NBLOQUE),A
			CP 17						
			JP C,BUC					; Cycle all 16 Blocks
			
ENDEND:
			JP DDNTRRESET				; Enable Dandanator and jumpt to menu

NBLOQUE:	DEFB 0x00
	

	
;------------------------------------------------------------------------------
;FFT sounds for PC
;------------------------------------------------------------------------------
fftERR:		ld hl,C#_ERR			; pitch
			ld de,TM_ERR			; duration.
fftBEEP:	CALL BEEPER				; Beeper routine.
			RET
fftOK:		ld hl,C#_OK				; pitch
			ld de,TM_OK				; duration.
			jr fftBEEP
			
			
;------------------------------------------------------------------------------
; SOUND VALUES
;------------------------------------------------------------------------------
C#_ERR		equ		57				;(437500 / 5000)) - 30.125   C#_ERR Freq = 5000 Hz
TM_ERR		equ		4000			; 5000 Hz * 0.8 sg = 4000 
C#_OK		equ		79				;(437500 / 4000)) - 30.125   C#_OK Freq = 4000 Hz
TM_OK		equ		3200			; 4000 Hz * 0.8 sg = 3200 
BEEPLOAD	equ		$FFFF			; Combines Beeper function and System for loading slot data:
									;  BIT 0 : Beep Function				1 = ACTIVE / 0 = Inactive
									;  BIT 1 : Kempston Loading 			1 = ACTIVE
									;  BIT 2 : Standard Loading (1500bps)	1 = ACTIVE
									;  BIT 3 : Fast Loading		(3000bps)	1 = ACTIVE
									;  BIT 4 : Turbo Loading	(4520bps)	1 = ACTIVE
									;  BIT 5 : Ultra Loading				1 = ACTIVE

;------------------------------------------------------------------------------
; VARS
;------------------------------------------------------------------------------
loadsize		equ		$8003			; Length of data loaded = 32kbyte data + 1 byte + 2 bytes crc
loadarea		equ		$6F00			; Destination of load data
TXTSLOTCOUNTER	equ 	$5800+(8*32)+8	; Position of Slot Counter
TXTSECTCOUNTER	equ 	$5800+(18*32)+8	; Position of Slot Counter


				
;------------------------------------------------------------------------------
;PROGRAMSECTOR - 
; (NBLOQUE) = Number of slot (1..16)
; HL = Address of data (32k = 8 sectors x 4K)
; C will be counting from 0 to 7
; Number in screen will be 1 to 8
; Combining (NBLOQUE) and C will have the sector in range 0..127 stored in B register
; B will be copied to A prior to calling SSTSECERASE and SSTSECTPROG
; HL will begin with the address of first 4K sector and incremented 4k by 4k prior to calling SSTSECTPROG
;------------------------------------------------------------------------------
PROGRAMSECTOR:
			PUSH HL						; Page in External eeprom -> Needed for programming
			LD A,1
			LD HL,1
			CALL SENDNRCMD 
			POP HL
			LD C,0						;N.of sector in this programming area (0..7)
BUCPROGRAMSECTOR:
			PUSH HL						;Save Initial address
			PUSH BC						;Save copy of C (0..7)
			LD A,(NBLOQUE)				;N.slot (1..16), need to convert in sector (*8)
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
; Routine to control loudspeaker - ; Documented by Alvin Albrecht.
;------------------------------------------------------------------------------
; Outputs a square wave of given duration and frequency
; to the loudspeaker.
;   Enter with: DE = #cycles - 1
;               HL = tone period as described next
;
; The tone period is measured in T states and consists of
; three parts: a coarse part (H register), a medium part
; (bits 7..2 of L) and a fine part (bits 1..0 of L) which
; contribute to the waveform timing as follows:
;
;                          coarse    medium       fine
; duration of low  = 118 + 1024*H + 16*(L>>2) + 4*(L&0x3)
; duration of hi   = 118 + 1024*H + 16*(L>>2) + 4*(L&0x3)
; Tp = tone period = 236 + 2048*H + 32*(L>>2) + 8*(L&0x3)
;                  = 236 + 2048*H + 8*L = 236 + 8*HL
;
; As an example, to output five seconds of middle C (261.624 Hz):
;   (a) Tone period = 1/261.624 = 3.822ms
;   (b) Tone period in T-States = 3.822ms*fCPU = 13378
;         where fCPU = clock frequency of the CPU = 3.5MHz
;    ©  Find H and L for desired tone period:
;         HL = (Tp - 236) / 8 = (13378 - 236) / 8 = 1643 = 0x066B
;   (d) Tone duration in cycles = 5s/3.822ms = 1308 cycles
;         DE = 1308 - 1 = 0x051B
;
; The resulting waveform has a duty ratio of exactly 50%.
;
;
;; BEEPER
BEEPER: LD A,(BEEPLOAD)			; Skip beeper if flag is 0
		BIT 0,A					
		RET Z


L03B5:  LD      A,L             ;
        SRL     L               ;
        SRL     L               ; L = medium part of tone period
        CPL                     ;
        AND     $03             ; A = 3 - fine part of tone period
        LD      C,A             ;
        LD      B,$00           ;
        LD      IX,L03D1        ; Address: BE-IX+3
        ADD     IX,BC           ;   IX holds address of entry into the loop
                                ;   the loop will contain 0-3 NOPs, implementing
                                ;   the fine part of the tone period.
        LD      A,8				;($5C48)   -> Disable border colour and enable Mic Output
;; BE-IX+3
L03D1:  NOP              ;(4)   ; optionally executed NOPs for small
                                ;   adjustments to tone period
;; BE-IX+2
L03D2:  NOP              ;(4)   ;

;; BE-IX+1
L03D3:  NOP              ;(4)   ;

;; BE-IX+0
L03D4:  INC     B        ;(4)   ;
        INC     C        ;(4)   ;

;; BE-H&L-LP
L03D6:  DEC     C        ;(4)   ; timing loop for duration of
        JR      NZ,L03D6 ;(12/7);   high or low pulse of waveform

        LD      C,$3F    ;(7)   ;
        DEC     B        ;(4)   ;
        JP      NZ,L03D6 ;(10)  ; to BE-H&L-LP

        XOR     $10      ;(7)   ; toggle output beep bit
        OUT     ($FE),A  ;(11)  ; output pulse
        LD      B,H      ;(4)   ; B = coarse part of tone period
        LD      C,A      ;(4)   ; save port #FE output byte
        BIT     4,A      ;(8)   ; if new output bit is high, go
        JR      NZ,L03F2 ;(12/7);   to BE-AGAIN

        LD      A,D      ;(4)   ; one cycle of waveform has completed
        OR      E        ;(4)   ;   (low->low). if cycle countdown = 0
        JR      Z,L03F6  ;(12/7);   go to BE-END

        LD      A,C      ;(4)   ; restore output byte for port #FE
        LD      C,L      ;(4)   ; C = medium part of tone period
        DEC     DE       ;(6)   ; decrement cycle count
        JP      (IX)     ;(8)   ; do another cycle

;; BE-AGAIN                     ; halfway through cycle
L03F2:  LD      C,L      ;(4)   ; C = medium part of tone period
        INC     C        ;(4)   ; adds 16 cycles to make duration of high = duration of low
        JP      (IX)     ;(8)   ; do high pulse of tone

;; BE-END
L03F6:  RET                     ;
		


;------------------------------------------------------------------------------
; Includes
;------------------------------------------------------------------------------
		
		include "misleches.asm"
		
		include "sstwriter_6.5v.asm"
		
		include "kempston_serial.asm"
		
		include "digits.asm"
