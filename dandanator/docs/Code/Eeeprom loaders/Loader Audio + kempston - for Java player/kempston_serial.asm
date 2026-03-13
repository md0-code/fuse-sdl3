;------------------------------------------------------------------------------------------------------------------------
; Load serial block of data at address specified by IX, DE contains size
;------------------------------------------------------------------------------------------------------------------------
LoadSerBlk: PUSH HL						; Save registers
			PUSH IY
			PUSH BC
			PUSH DE
			LD HL,IX					; HL Contains destination pointer
			LD IY,DE					; IY Contains remaining load size

			LD C,$1F					; Kempston IO port
DetectIdle:	IN A,(C)					; Detect Idle line status for Inverting/non inverting Kempston interfaces	
			AND 1
			JR Z, NOKempsCHG
			LD A, INST_JRC				; Change Kempston detection routine for start bit
			LD (CheckStart), A
			XOR A						; Do not invert payload
			LD (InvertByte), A				
			
NOKempsCHG:				
BucSerial:	LD B,08						; Load 1 byte 57.600bps inline
			LD D,0						; Clear D (will hold the payload)
WaitStartb:	IN A,(C)					; Wait for Start bit
			RRCA
CheckStart:	JR NC, WaitStartb
										; At this point, 12ts+4ts+7s = 23ts since detection (not exact by sampling error max=15ts+-)
										; 57600 bps is 61ts per sample. Lets get past the start bit with 38 extra ts
			LD A,R						; 9ts
			AND $01						; 7ts - Dummy
			AND $01						; 7ts - Dummy
			NOP		 					; 4ts	
			NOP							; Extra 4 ts - Needed for 128k
			NOP							; Extra 4 ts - Needed for 128k			
			OUT ($FE),A					; 11ts - 46 ts total
					
PayloadLp:								; Start of payload sample. Let's move to almost half way to sample in the middle of the bit (should but can't. Move a bit ahead)
			NOP							; +4ts = 4ts - Dummy
			NOP							; +4ts = 8ts - Dummy
			IN A,(C)					; +12ts = 20ts
			AND 1						; +7ts = 27ts - Clear all but lsb (Kempston is 000FUDLR -> So "Right", pin 4 and GND, pin 8 should be connected to the serial port)
			OR D						; +4ts = 31ts - Add sample from D to A
			LD D,A						; +4ts = 35ts - Save back samples to D
			RRC D						; +8ts = 43ts - Rotate Right D (LSbits will always be 0 in 8 rotations, so no need to clear them)
			NOP							; +4ts  =47ts
			DJNZ PayloadLp				; +13ts = 60ts when jumping -> Offset of -1 ts. In 8 bits, offset will be 8, still acceptable since the signal lasts for 61ts
			LD A,D
InvertByte:	CPL		
			LD (HL),A					; Save Byte loaded to RAM
			INC HL						; Next Ram position
			DEC IY						; One byte less
			LD A,IYL					; Check if zero bytes remaining
			OR IYH
			JR NZ,BucSerial				; Loop if some bytes remain
			
			POP DE						; Restore registers
			POP BC
			POP IY
			POP HL
			XOR A						; Border 0
			OUT ($FE),A
			OR A						; Set Zero -> No abort pressed (Compatibility with tape loading)
			SCF							; Set Carry Flag -> Load Ok (Compatibility with tape loading)
			RET
;------------------------------------------------------------------------------------------------------------------------


INST_CPL	EQU	$2F						; CPL Instruction code
INST_JRC	EQU $38						; JR C Instruction code
INST_JRNC	EQU $30						; JR NC Instruction code
INST_NOP	EQU $00						; Nop Instruction code
			