; This code is a modification/extension of "Cargando Leches" by Antonio Villena

; ------------------------------
; THE 'SAVE/LOAD RETURN' ROUTINE
; ------------------------------
;   The address of this routine is pushed on the stack prior to any load/save
;   operation and it handles normal completion with the restoration of the
;   border and also abnormal termination when the break key, or to be more
;   precise the space key is pressed during a tape operation.
;
; - - >

;; SA/LD-RET
L053F:  PUSH    AF              ; preserve accumulator throughout.
        XOR A	;	LD      A,($5C48)       ; fetch border colour from BORDCR. -> BLACK BORDER
        AND     $38             ; mask off paper bits.
        RRCA                    ; rotate
        RRCA                    ; to the
        RRCA                    ; range 0-7.

        OUT     ($FE),A         ; change the border colour.

        LD      A,$7F           ; read from port address $7FFE the
        IN      A,($FE)         ; row with the space key at outside.
 
        RRA                     ; test for space key pressed.
        ;EI                      ; enable interrupts
        ;JR      C,L0554         ; forward to SA/LD-END if not


;; REPORT-Da
L0552:   ;RST     08H             ; ERROR-1
        ;DEFB    $0C             ; Error Report: BREAK - CONT repeats

; ---

;; SA/LD-END
L0554:  POP     AF              ; restore the accumulator.
        RET                     ; return.
		
; ------------------------------------
; Load header or block of information
; ------------------------------------
;   This routine is used to load bytes and on entry A is set to $00 for a 
;   header or to $FF for data.  IX points to the start of receiving location 
;   and DE holds the length of bytes to be loaded. If, on entry the carry flag 
;   is set then data is loaded, if reset then it is verified.

;; LD-BYTES
LD_BYTES:
L0556:
		PUSH AF
		LD A,H
		LD (myregH+1),A			;Initialize regH register for timing (delay for long edge detection)
		LD A,L
		LD (myregL+1),A			;Initialize regL register for timing (middle value on time for choice between 0 and 1)
		POP AF

		INC     D               ; reset the zero flag without disturbing carry.
        EX      AF,AF'          ; preserve entry flags.'
        DEC     D               ; restore high byte of length.

        DI                      ; disable interrupts

        LD      A,$0F           ; make the border white and mic off.
        OUT     ($FE),A         ; output to port.
;
        LD      HL,L053F;LD_RET        ; Address: SA/LD-RET
        PUSH    HL              ; is saved on stack as terminating routine.
;
;   the reading of the EAR bit (D6) will always be preceded by a test of the 
;   space key (D0), so store the initial post-test state.
;
        IN      A,($FE)         ; read the ear state - bit 6.
        RRA                     ; rotate to bit 5.
        AND     $20             ; isolate this bit.
        CALL    ULTRA
        CP      A               ; set the zero flag.
		LD		C,$25			; Initialize if no ULTRA loading (colour Cyan)

;; LD-BREAK
L056B:  RET     NZ              ; return if at any time space is pressed.

;; LD-START
L056C:  CALL    L05E7           ; routine LD-EDGE-1
        JR      NC,L056B        ; back to LD-BREAK with time out and no
                                ; edge present on tape.

;   but continue when a transition is found on tape.

        LD      HL,$0100 ;$0415        ; set up 16-bit outer loop counter for 
                                ; approx 1 second delay. -> approx 0.25 second delay

;; LD-WAIT
L0574:  DJNZ    L0574           ; self loop to LD-WAIT (for 256 times)

        DEC     HL              ; decrease outer loop counter.
        LD      A,H             ; test for
        OR      L               ; zero.
        JR      NZ,L0574        ; back to LD-WAIT, if not zero, with zero in B.

;   continue after delay with H holding zero and B also.
;   sample 256 edges to check that we are in the middle of a lead-in section. 

        CALL    L05E3           ; routine LD-EDGE-2
        JR      NC,L056B        ; back to LD-BREAK
                                ; if no edges at all.

;; LD-LEADER
L0580:  LD      B,$9C           ; two edges must be spaced apart.
        CALL    L05E3           ; routine LD-EDGE-2
        JR      NC,L056B        ; back to LD-BREAK if time-out

        LD      A,$C6           ; two edges must be spaced apart.
        CP      B               ; compare
        JR      NC,L056C        ; back to LD-START if too close together for a 
                                ; lead-in.

        INC     H               ; proceed to test 256 edged sample.
        JR      NZ,L0580        ; back to LD-LEADER while more to do.

;   sample indicates we are in the middle of a two or five second lead-in.
;   Now test every edge looking for the terminal sync signal.

;; LD-SYNC
L058F:  LD      B,$C9           ; two edges must be spaced apart.
        CALL    L05E7           ; routine LD-EDGE-1
        JR      NC,L056B        ; back to LD-BREAK with time-out.

        LD      A,B             ; fetch augmented timing value from B.
        CP      $D4             ; compare 
        JR      NC,L058F        ; back to LD-SYNC if gap too big, that is,
                                ; a normal lead-in edge gap.

;   but a short gap will be the sync pulse.
;   in which case another edge should appear before B rises to $FF

        CALL    L05E7           ; routine LD-EDGE-1
        RET     NC              ; return with time-out.

; proceed when the sync at the end of the lead-in is found.
; We are about to load data so change the border colours.

        LD      A,C             ; fetch long-term mask from C
        XOR     $07;;;$03        ; and make blue/yellow. -> RED/YELLOW

        LD      C,A             ; store the new long-term byte.

        LD      H,$00           ; set up parity byte as zero.
        LD      B,$B0           ; two edges must be spaced apart.
        JR      L05C8           ; forward to LD-MARKER 
                                ; the loop mid entry point with the alternate 
                                ; zero flag reset to indicate first byte 
                                ; is discarded.

; --------------
;   the loading loop loads each byte and is entered at the mid point.

;; LD-LOOP
L05A9:  EX      AF,AF'          ; restore entry flags and type in A.'
        JR      NZ,L05B3        ; forward to LD-FLAG if awaiting initial flag
                                ; which is to be discarded.

        ;JR      NC,L05BD        ; forward to LD-VERIFY if not to be loaded.->Deleted not needed, verify not used

        LD      (IX+$00),L      ; place loaded byte at memory location.
        JR      L05C2           ; forward to LD-NEXT

; ---

;; LD-FLAG
L05B3:  RL      C               ; preserve carry (verify) flag in long-term
                                ; state byte. Bit 7 can be lost.

        XOR     L               ; compare type in A with first byte in L.
        RET     NZ              ; return if no match e.g. CODE vs. DATA.

;   continue when data type matches.

        LD      A,C             ; fetch byte with stored carry
        RRA                     ; rotate it to carry flag again
        LD      C,A             ; restore long-term port state.

        INC     DE              ; increment length ??
        JR      L05C4           ; forward to LD-DEC.
                                ; but why not to location after ?

; ---
;   for verification the byte read from tape is compared with that in memory.

;; LD-VERIFY
;L05BD:  LD      A,(IX+$00)      ; fetch byte from memory. . MARIO not needed
;        XOR     L               ; compare with that on tape . MARIO not needed
;        RET     NZ              ; return if not zero. . MARIO not needed

;; LD-NEXT
L05C2:  INC     IX              ; increment byte pointer.

;; LD-DEC
L05C4:  DEC     DE              ; decrement length.
        EX      AF,AF'          ; store the flags.'
        LD      B,$B2           ; timing.

;   when starting to read 8 bits the receiving byte is marked with bit at right.
;   when this is rotated out again then 8 bits have been read.

;; LD-MARKER
L05C8:  LD      L,$01           ; initialize as %00000001

;; LD-8-BITS
L05CA:  CALL    L05E3           ; routine LD-EDGE-2 increments B relative to
                                ; gap between 2 edges.
        RET     NC              ; return with time-out.

myregL:
        LD      A,$CB           ; the comparison byte.
        CP      B               ; compare to incremented value of B.
                                ; if B is higher then bit on tape was set.
                                ; if <= then bit on tape is reset. 

        RL      L               ; rotate the carry bit into L.

        LD      B,$B0           ; reset the B timer byte.
        JP      NC,L05CA        ; JUMP back to LD-8-BITS

;   when carry set then marker bit has been passed out and byte is complete.

        LD      A,H             ; fetch the running parity byte.
        XOR     L               ; include the new byte.
        LD      H,A             ; and store back in parity register.

        LD      A,D             ; check length of
        OR      E               ; expected bytes.
        JR      NZ,L05A9        ; back to LD-LOOP 
                                ; while there are more.

;   when all bytes loaded then parity byte should be zero.

        LD      A,H             ; fetch parity byte.
        SUB     $01             ; set carry if zero.
        INC 	A				; Ensure Z=1 if carry is set
        RET                     ; return
                                ; in no carry then error as checksum disagrees.

								
; -------------------------
; Check signal being loaded
; -------------------------
;   An edge is a transition from one mic state to another.
;   More specifically a change in bit 6 of value input from port $FE.
;   Graphically it is a change of border colour, say, blue to yellow.
;   The first entry point looks for two adjacent edges. The second entry point
;   is used to find a single edge.
;   The B register holds a count, up to 256, within which the edge (or edges) 
;   must be found. The gap between two edges will be more for a '1' than a '0'
;   so the value of B denotes the state of the bit (two edges) read from tape.


;; LD-EDGE-2
L05E3:  CALL    L05E7           ; call routine LD-EDGE-1 below.
        RET     NC              ; return if space pressed or time-out.
                                ; else continue and look for another adjacent 
                                ; edge which together represent a bit on the 
                                ; tape.

; -> 
;   this entry point is used to find a single edge from above but also 
;   when detecting a read-in signal on the tape.

;; LD-EDGE-1
L05E7:
myregH:  LD      A,$16           ; a delay value of twenty two.

;; LD-DELAY
L05E9:  DEC     A               ; decrement counter
        JR      NZ,L05E9        ; loop back to LD-DELAY 22 times.

        AND      A              ; clear carry.
	
; ____        _   _
;|  _ \ _   _| |_(_)_ __   __ _
;| |_) | | | | __| | '_ \ / _` |
;|  _ <| |_| | |_| | | | | (_| |
;|_| \_\\__,_|\__|_|_| |_|\__,_|	
	
	
	; LD-SAMPLE
L05ED:  INC     B               ; increment the time-out counter.
        RET     Z               ; return with failure when $FF passed.

        LD      A,$7F           ; prepare to read keyboard and EAR port
        IN      A,($FE)         ; row $7FFE. bit 6 is EAR, bit 0 is SPACE key.
        RRA                     ; test outer key the space. (bit 6 moves to 5)
        RET     NC              ; return if space pressed.

        XOR     C               ; compare with initial long-term state.
        AND     $20             ; isolate bit 5
        JR      Z,L05ED         ; back to LD-SAMPLE if no edge.

;   but an edge, a transition of the EAR bit, has been found so switch the
;   long-term comparison byte containing both border colour and EAR bit. 

        LD      A,C             ; fetch comparison value.
 		XOR		$24	;CPL		; inverse ear bit (6) 	and colour (bits 2 only)						7 cic;		4 cic
		LD      C,A             ; and put back in C for long-term.

        AND     $07             ; isolate new colour bits.
        OR      $08             ; set bit 3 - MIC off.
        OUT     ($FE),A         ; send to port to effect the change of colour. 

        SCF                     ; set carry flag signaling edge found within
                                ; time allowed.
        RET                     ; return.
	
;3802
ULTRA:  PUSH    IX              ; 133 bytes
        POP     HL              ; pongo la direccion de comienzo en HL
        EXX                     ; salvo DE, en caso de volver al cargador estandar y para hacer luego el checksum
        LD      C,$00
ULTR0:  DEFB    $2A				; Convert that JR in LD HL,($1220) that is not used 
ULTR1:  JR      NZ,ULTR3        ; return if at any time space is pressed.
ULTR2:  LD      B,0
        CALL    L05ED           ; leo la duracion de un pulso (positivo o negativo)
        JR      NC,ULTR1        ; si el pulso es muy largo retorno a bucle
        LD      A, B
        CP      40              ; si el contador esta entre 24 y 40
        JR      NC,ULTR4        ; y se reciben 8 pulsos (me falta inicializar HL a 00FF)
        CP      24
        RL      L
        JR      NZ,ULTR4
ULTR3:  EXX
        LD      C,2
        RET
ULTR4:  CP      16              ; si el contador esta entre 10 y 16 es el tono guia
        RR      H               ; de las ultracargas, si los ultimos 8 pulsos
        CP      10              ; son de tono guia H debe valer FF
        JR      NC,ULTR2
        INC     H
        INC     H
        JR      NZ,ULTR0        ; si detecto sincronismo sin 8 pulsos de tono guia retorno a bucle
        CALL    L05ED           ; leo pulso negativo de sincronismo
        LD      L,$01           ; HL vale 0001, marker para leer 16 bits en HL (checksum y byte flag)
        CALL    L39E9           ; leo 16 bits, ahora temporizo cada 2 pulsos
        POP     AF              ; machaco la direccion de retorno de la carga estandar
						;LD ($4A00),SP				; Store in screen MARIO
		
        EX      AF,AF'          ; A es el byte flag que espero'
        CP      L               ; lo comparo con el que me encuentro en la ultracarga
        RET     NZ              ; salgo si no coinciden
        XOR     H               ; xoreo el checksum con en byte flag, resultado en A
        EXX                     ; guardo checksum por duplicado en H' y L'
        PUSH    HL              ; pongo direccion de comienzo en pila
						;LD ($4B00),SP				; Store in screen MARIO
        LD      C,A
        LD      A,$D8           ; A' tiene que valer esto para entrar en Raudo
        EX      AF,AF'			; '
        EXX
        LD      H,$01           ; leo 8 bits en HL
        CALL    L39E9
        PUSH    HL
						;LD ($4C00),SP				; Store in screen MARIO
        POP     IX
        POP     DE              ; recupero en DE la direccion de comienzo del bloque
        INC     C               ; pongo en flag Z el signo del pulso
        LD      BC,$EFFE        ; este valor es el que necesita B para entrar en Raudo
        JR      Z,ULTR6
        LD      H,L37BF>>8		; High byte direccion base (we'll use L36BF,L36FF,L37BF,L37FF)
ULTR5:  IN      F,(C)
        JP      PE,ULTR5
        CALL    L37C3           ; salto a Raudo segun el signo del pulso en flag Z
        JR      ULTR8
ULTR6:  LD      H,L33BF>>8		; High byte direccion base (we'll use L32BF,L32FF,L33BF,L33FF)
ULTR7:  IN      F,(C)
        JP      PO,ULTR7
        CALL    L3403           ; salto a Raudo
ULTR8:  EXX                     ; ya se ha acabado la ultracarga (Raudo)
        DEC     DE
        LD      B,E
        INC     B
        INC     D
        LD      A,IXH
ULTR9:  XOR     (HL)
        INC     HL
        DJNZ    ULTR9
        DEC     D
        JP      NZ,ULTR9
        PUSH    HL              ; ha ido bien
						;LD ($4D00),SP				; Store in screen MARIO
        XOR     C
        LD      H,B
        LD      L,C
        LD      D,B
        LD      E,B
        POP     IX              ; IX debe apuntar al siguiente byte despues del bloque
        RET     NZ              ; si no coincide el checksum salgo con Carry desactivado
        SCF
        RET

;GET16
L39E9:  LD      B,0             ; 16 bytes
        CALL    L05ED           ; esta rutina lee 2 pulsos e inicializa el contador de pulsos
        CALL    L05ED
        LD      A,B
        CP      12
        ADC     HL,HL
        JR      NC,L39E9
        RET

		BLOCK $60,0
		BLOCK (($/256)*256)+$b0-$,0

	
L32CD:  XOR     B               ;4
        ADD     A,A             ;4
        RET     C               ;5
        ADD     A,A             ;4
        EX      AF,AF'          ;4		AF'
        OUT     ($FE),A         ;11
        IN      L,(C)           ;12
        JP      (HL)            ;4

		; -> Insert Here the first $xxBF
	BLOCK (($/256)*256)+$BF-$,0
L32BF:  INC     H               ;4
        JR      NC,L32CD        ;7/12     46/48
        XOR     B               ;4
        XOR     $9C             ;7
        LD      (DE),A          ;7
        INC     DE              ;6
        LD      A,$DC           ;7
        EX      AF,AF'          ;4	AF'
        IN      L,(C)           ;12
        JP      (HL)            ;4


; -> Insert Here the first $80FF
	BLOCK (($/256)*256)+$FF-$,0
L32FF:  IN      L,(C)
        JP      (HL)

	BLOCK (($/256)*256)+$0D-$,0
		
L330D:  DEFB    $EC, $EC, $01   ; 0D
        DEFB    $EC, $EC, $02   ; 10
        DEFB    $EC, $EC, $03   ; 13
        DEFB    $EC, $EC, $04   ; 16
        DEFB    $EC, $EC, $05   ; 19
        DEFB    $EC, $EC, $06   ; 1C
        DEFB    $EC, $EC, $07   ; 1F
        DEFB    $EC, $EC, $08   ; 22
        DEFB    $EC, $EC, $09   ; 25
        DEFB    $ED, $ED, $0A   ; 28
        DEFB    $ED, $ED, $0B   ; 2B
        DEFB    $ED, $ED, $0C   ; 2E
        DEFB    $ED, $ED, $0D   ; 31
        DEFB    $ED, $ED, $0E   ; 34
        DEFB    $ED, $ED, $7F   ; 37
        DEFB    $ED, $ED, $7F   ; 3A
        DEFB    $ED, $ED, $7F   ; 3D
        DEFB    $ED, $ED, $7F   ; 40
        DEFB    $ED, $EE, $7F   ; 43 --
        DEFB    $EE, $EE, $7F   ; 46 --
        DEFB    $EE, $EE, $7F   ; 49
        DEFB    $EE, $EE, $7F   ; 4C
        DEFB    $EE, $EE, $7F   ; 4F
        DEFB    $EE, $EE, $7F   ; 52
        DEFB    $EE, $EE, $0F   ; 55
        DEFB    $EE, $EE, $10   ; 58
        DEFB    $EE, $EE, $11   ; 5B
        DEFB    $EE, $EF, $12   ; 5E
        DEFB    $EE, $EF, $13   ; 61
        DEFB    $EF, $EF, $14   ; 64
        DEFB    $EF, $EF, $15   ; 67
        DEFB    $EF, $EF, $16   ; 6A
        DEFB    $EF, $EF, $17   ; 6D
        DEFB    $EF, $EF, $18   ; 70
        DEFB    $EF, $EF, $19   ; 73
        DEFB    $EF, $EF, $1A   ; 76
        DEFB    $EF, $1B, $1C   ; 79
        DEFB    $EF, $1D, $1E   ; 7C
        DEFB    $EF             ; 7F
        DEFB    $EC, $EC, $1F   ; 80
        DEFB    $EC, $EC, $20   ; 83
        DEFB    $EC, $EC, $21   ; 86
        DEFB    $EC, $EC, $22   ; 89
        DEFB    $EC, $EC, $23   ; 8C
        DEFB    $ED, $ED, $7E   ; 8F
        DEFB    $ED, $ED, $7D   ; 92
        DEFB    $ED, $ED, $7F   ; 95
        DEFB    $ED, $ED, $7F   ; 98
        DEFB    $ED, $EE, $7F   ; 9B --
        DEFB    $EE, $EE, $7F   ; 9E
        DEFB    $EE, $EE, $7F   ; A1
        DEFB    $EE, $EE, $7D   ; A4
        DEFB    $EE, $EE, $7E   ; A7
        DEFB    $EE, $EF, $24   ; AA
        DEFB    $EF, $EF, $25   ; AD
        DEFB    $EF, $EF, $26   ; B0
        DEFB    $EF, $EF, $27   ; B3
        DEFB    $EF, $EF, $28   ; B6
        DEFB    $EF, $29, $2A   ; B9
        DEFB    $2B, $2C, $2D   ; BC

; -> Insert Here the first $81BF
	BLOCK (($/256)*256)+$BF-$,0

L33BF:  IN      L,(C)
        JP      (HL)

; -> Insert Here the first $81FF
	BLOCK (($/256)*256)+$FF-$,0
		
L33FF:  LD      A,R             ;9        49 (41 sin borde)
        LD      L,A             ;4
        LD      B,(HL)          ;7
L3403:  LD      A,IXL           ;8
        LD      R,A             ;9
        LD      A,B             ;4
        EX      AF,AF'          ;4		AF'
        DEC     H               ;4
        IN      L,(C)           ;12
        JP      (HL)            ;4

; -> Insert Here the first $86BF
	BLOCK (($/256)*256)+$BF-$,0
		
L36BF:  IN      L,(C)
        JP      (HL)

		
; -> Insert Here the first $86F5
	BLOCK (($/256)*256)+$F5-$,0
L36F5:  XOR     B
        ADD     A,A
        RET     C
        ADD     A,A
        EX      AF,AF'					;AF'
        OUT     ($FE),A         ;11
        IN      L,(C)
        JP      (HL)
L36FF:  INC     H
        JR      NC,L36F5
        XOR     B
        XOR     $9C
        LD      (DE),A
        INC     DE
        LD      A,$DC
        EX      AF,AF'					;AF'
        IN      L,(C)           ;12
        JP      (HL)            ;4

L370D:	DEFB    $EC, $EC, $01   ; 0D
        DEFB    $EC, $EC, $02   ; 10
        DEFB    $EC, $EC, $03   ; 13
        DEFB    $EC, $EC, $04   ; 16
        DEFB    $EC, $EC, $05   ; 19
        DEFB    $EC, $EC, $06   ; 1C
        DEFB    $EC, $EC, $07   ; 1F
        DEFB    $EC, $EC, $08   ; 22
        DEFB    $EC, $EC, $09   ; 25
        DEFB    $ED, $ED, $0A   ; 28
        DEFB    $ED, $ED, $0B   ; 2B
        DEFB    $ED, $ED, $0C   ; 2E
        DEFB    $ED, $ED, $0D   ; 31
        DEFB    $ED, $ED, $0E   ; 34
        DEFB    $ED, $ED, $7F   ; 37
        DEFB    $ED, $ED, $7F   ; 3A
        DEFB    $ED, $ED, $7F   ; 3D
        DEFB    $ED, $ED, $7F   ; 40
        DEFB    $ED, $EE, $7F   ; 43 --
        DEFB    $EE, $EE, $7F   ; 46 --
        DEFB    $EE, $EE, $7F   ; 49
        DEFB    $EE, $EE, $7F   ; 4C
        DEFB    $EE, $EE, $7F   ; 4F
        DEFB    $EE, $EE, $7F   ; 52
        DEFB    $EE, $EE, $0F   ; 55
        DEFB    $EE, $EE, $10   ; 58
        DEFB    $EE, $EE, $11   ; 5B
        DEFB    $EE, $EF, $12   ; 5E
        DEFB    $EE, $EF, $13   ; 61
        DEFB    $EF, $EF, $14   ; 64
        DEFB    $EF, $EF, $15   ; 67
        DEFB    $EF, $EF, $16   ; 6A
        DEFB    $EF, $EF, $17   ; 6D
        DEFB    $EF, $EF, $18   ; 70
        DEFB    $EF, $EF, $19   ; 73
        DEFB    $EF, $EF, $1A   ; 76
        DEFB    $EF, $1B, $1C   ; 79
        DEFB    $EF, $1D, $1E   ; 7C
        DEFB    $EF             ; 7F
        DEFB    $EC, $EC, $1F   ; 80
        DEFB    $EC, $EC, $20   ; 83
        DEFB    $EC, $EC, $21   ; 86
        DEFB    $EC, $EC, $22   ; 89
        DEFB    $EC, $EC, $23   ; 8C
        DEFB    $ED, $ED, $7E   ; 8F
        DEFB    $ED, $ED, $7D   ; 92
        DEFB    $ED, $ED, $7F   ; 95
        DEFB    $ED, $ED, $7F   ; 98
        DEFB    $ED, $EE, $7F   ; 9B --
        DEFB    $EE, $EE, $7F   ; 9E
        DEFB    $EE, $EE, $7F   ; A1
        DEFB    $EE, $EE, $7D   ; A4
        DEFB    $EE, $EE, $7E   ; A7
        DEFB    $EE, $EF, $24   ; AA
        DEFB    $EF, $EF, $25   ; AD
        DEFB    $EF, $EF, $26   ; B0
        DEFB    $EF, $EF, $27   ; B3
        DEFB    $EF, $EF, $28   ; B6
        DEFB    $EF, $29, $2A   ; B9
        DEFB    $2B, $2C, $2D   ; BC

; -> Insert Here the first $87BF
	BLOCK (($/256)*256)+$BF-$,0
		
L37BF:  LD      A,R
        LD      L,A
        LD      B,(HL)
L37C3:  LD      A,IXL
        LD      R,A
        LD      A,B
        EX      AF,AF'			; af'
        DEC     H
        IN      L,(C)
        JP      (HL)

; -> Insert Here the first $87FF
	BLOCK (($/256)*256)+$FF-$,0
L37FF:  IN      L,(C)
        JP      (HL)
	
;myregH		 3000 = 5	 ($05)	4500=1	 ($01)	1500 = 19 ($13) <-Register H
;myregL		 3000 = 190 ($BE)	4500=186 ($BA)	1500 = 201($C9) <-Register L
;
;    Value to adjust in WAV file for Standard, Fast and Turbo load
;    ts0:784 ts1:1568		1500 bps (standard)
;    ts0:392 ts1:784		3000 bps (fast)
;    ts0:262 ts1:524		4500 bps (normal)