.globl _pNesX_BuildCharAsm
_pNesX_BuildCharAsm:
	mov        #0, r0
	mov        r4, r7
	mov        r0, r6

.start_loop:
	!first build address of ppu base
	mov        r7, r4
	mov        r6, r0

	shll2      r0
	shll2      r0

	add        r0, r4
	
	!start masking calculation
	!---------------------------------------------------iteration 1
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 2
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 3
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 4
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 5
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 6
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 7
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!Move r4 to the next pixel... #2
	add        #-7, r4
	
	mov.l      r2, @r5

	add        #4, r5

	!---------------------------------------------------iteration 8
	!1 byte = pbyBGData[0] is in r1
	mov.b      @r4, r1

	!Build Mask 0x55 in r0
	mov        #0x55, r0

	!Shift Right 1 bit
	shar       r1

	!1 byte = pbyBGData[0] is in r2
	mov.b      @r4, r2

	!And Mask With r1
	and        r0, r1

	!And Mask With r2
	and        r0, r2

	!Shift r2 over 8 bits
	shll8      r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains half of the pattern at this point

	!Add 8 into r4 (point to pbyBGData[8])
	add        #8, r4

	!Build Mask 0xAA in r0 - watch for sign extend
	mov        #0x00, r0
	
	!Move pbyBGData[8] into r2
	mov.b      @r4, r2

	!Finish Mask
	or         #0xAA, r0

	!1 byte = pbyBGData[8] is in r3
	mov.b      @r4, r3
	
	!And Mask With r2
	and        r0, r2
	
	!shift r3 left 1 bit
	shal       r3
		
	!And Mask With r3
	and        r0, r3

	!shift r3 over 8 bits
	shll8      r3

	!Or r3 into r2
	or         r3, r2

	!Or r1 and r2 into r1
	or         r2, r1

	!r1 contains the full 16 bit pattern at this point

	!Build Mask 0x0303 in register r0
	mov        #0x03, r0

	!Parallel load a copy of the pattern into r2
	mov        r1, r2

	!Shift r0 over 8
	shll8      r0

	!Shift r2 over 2
	shlr2      r2
	
	!Finish pattern
	or         #0x03, r0

	shlr2      r2

	!Parallel Work on r3
	mov        r1, r3

	shlr2      r2

	shlr2      r3

	and        r0, r2

	shlr2      r3

	and        r0, r3

	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5
	mov.l      r2, @r5

	!load a copy of the pattern into r2
	mov        r1, r2

	add        #4, r5

    !do the character setup
	shlr2      r2

	mov        r1, r3
	
	and        r0, r2

	and        r0, r3
	shll16     r3
	or         r3, r2

	!finalized data for 4 pixels is in r2
	!store data in the character buffer - post increment buffer
	pref       @r5

	!stick some comparison instructions in the prefetch time
	add        #1, r6
	mov        #64, r0

	mov.l      r2, @r5

	add        #4, r5

	cmp/hi     r6, r0

	bt         .start_loop

	rts
	nop
