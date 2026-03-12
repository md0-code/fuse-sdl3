
;------------------------------------------------------------------------------------------------------------------------
; CPC Dandanator! Mini HW v1.2/1.3 - USB Serial IN/OUT
; 
; Dandare - May 2018
; ----------------------------------------------------------------------------------------  

;------------------------------------------------------------------------------------------------------------------------
; Load serial block of data at address specified by HL, IY contains size - Serial data is served NOT INVERTED
; WARNING, WHEN SERIAL OPS ARE ON, BUS IS HACKED ON LD A,(HL) (returns serial bit on D0), others <= '0'
;------------------------------------------------------------------------------------------------------------------------
LoadSerBlk: 
			
SerialON:	PUSH IY
			DDNTR_CONFIGURATION 5		; Enable serial reads. Keep "1" idle to serial out	
			POP IY
			LD E,8
			LD BC, $7F09
			OUT (C),C					; Select Border
			
BucSerial:	LD B,E						; Load 1 byte 57.600bps inline			
			LD D,0						; Clear D (will hold the payload)
WaitStartb:	LD A,(HL)					; Wait for Start bit
			RRCA
CheckStart:	JR C, WaitStartb
										; At this point, 1+2+4 = 7 nop-equivalents since detection (not exact by sampling error max=2+-)
										; 57600 bps is 17,36 CPC - Lets get past the start bit with 14 extra nops-- and them some	
		
			LD B,$7F					; 2
			LD A,L						; 1 - 3 
			AND $04						; 2 - 5	
			ADD $4C						; 2 - 7
			OUT (C),A					; 4 - 11
			LD B,E						; 1 - 12
			
			NOP							; 1 - 13
			NOP							; 1 - 14
			NOP 						; 1 - 15
			NOP							; 1 - 16
			NOP							; 1 - 17
			
PayloadLp:			
			NOP 						; 1 - 1 - 18
			NOP							; 1 - 2 - 19
			NOP							; 1 - 3 - 20
			NOP							; 1 - 4 - 21	
			AND $1						; 2 - 6 - 23
			LD A,(HL)					; 1 - 7 - 24  --> since start bit detection = 24 + 7 = 31 in first iteration. Close to beginning of bit
			RRA							; 1 - 8
			RR D						; 2 - 10
			NOP							; 1 - 11
			DJNZ PayloadLp				; 4 = 15 nop_eq when jumping -> getting ahead a little each jump
			
SByteRam:	LD A,D
			LD (HL),A					; Save received Byte to RAM
			INC HL						; Next Ram position
			DEC IY						; One byte less
			LD A,IYL					; Check if zero bytes remaining
			OR IYH
			JR NZ,BucSerial				; Loop if some bytes remain	
			
SerialOFF:	DDNTR_CONFIGURATION 4		; Disable serial reads. Keep "1" idle to serial out	

			RET
;------------------------------------------------------------------------------------------------------------------------



;------------------------------------------------------------------------------------------------------------------------
; Send Serial Byte at 57.600bps
; Must be DI, Sends byte in A
; Start bit. bit 0.... bit 7. Stop bits (2, idle line)
;------------------------------------------------------------------------------------------------------------------------
SerialSendA:
			LD C,A						; Save A
			LD D,0						; 0 is a 0 to serial port -- Disables Serial reads.
			LD E,4						; 4 is a 1 to serial port -- Disables Serial reads.
			LD B,8						; Number of iterations
			LD IY, IYSCRATCH_ADDR		; Save 0xBFFF Contents no slot should be paged in in segment 3
			
Startbit:	LD A,D						; Load a 0
			TRIGGER			
			LD (IY),A					; Send Start bit
			AND 1						; 2
			AND 1						; 2
			AND 1						; 2
			AND 1						; 2	
			AND 1						; 2	
			AND 1						; 2	
			
Bits0_7:	RRC C						; (2) put next bit in Carry
			LD A,D						; (1) Assume a 0 bit
			JR NC, Bit_send				; (3 if jump, 2 if no jump) If no Carry, jump to bit_send with a 0
			LD A,E						; (1) Normalizing previous JR, Put 1 in bit
Bit_send:	TRIGGER						; (4)
			LD (IY),A					; (2)
			DJNZ Bits0_7				; (4 if jump, 3 if not)
										; Total 17
			
			AND 1						; 2
			AND 1						; 2
			AND 1						; 2
			AND 1						; 2	
			LD A, E						; Stop bits
			TRIGGER						; Trigger
			LD (IY),A					; Command
			RET