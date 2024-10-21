#include "cpu.h"

#include "K6502_rw.h"

#define RAM_SIZE 0x800
uint8 RAM[RAM_SIZE];

uint16 PC;
uint8 A, X, Y, SP;
bool C, Z, I, D, V, N;
bool nmi, irq;
bool HALT;

#define NTH_BIT(x, n) (((x) >> (n)) & 1)

inline uint8 get_flags() { return (C | (Z << 1) | (I << 2) | (D << 3) | (1 << 5) | (V << 6) | (N << 7)); }
inline void set_flags(uint8 p) { C = NTH_BIT(p, 0); Z = NTH_BIT(p, 1); I = NTH_BIT(p, 2); D = NTH_BIT(p, 3); V = NTH_BIT(p, 6); N = NTH_BIT(p, 7); }

// Remaining clocks to end frame:
const int TOTAL_CYCLES = 29781;
int remainingCycles;
inline int elapsed() { return TOTAL_CYCLES - remainingCycles; }

/* Cycle emulation */
#define T   remainingCycles--;
// void tick() { ppu_step(); ppu_step(); ppu_step(); remainingCycles--; }

/* Flags updating */
inline void upd_cv(uint8 x, uint8 y, int16 r) { C = (r>0xFF); V = ~(x^y) & (x^r) & 0x80; }
inline void upd_nz(uint8 x)              { N = x & 0x80; Z = (x == 0);              }
// Does adding I to A cross a page?
inline bool cross(uint16 a, uint8 i) { return ((a+i) & 0xFF00) != ((a & 0xFF00)); }

/* Memory access */
void dma_oam(uint8 bank);

/*
uint8 read_access(uint16 addr) {
    uint8* r;
    switch (addr) {
        case 0x0000 ... 0x1FFF:  r = &RAM[addr % 0x800]; return *r;                  // RAM.
        case 0x2000 ... 0x3FFF:  return ppu_read(addr % 8);                          // PPU.

        // APU:
        case 0x4000 ... 0x4013:
        case            0x4015:  // return APU::access<wr>(elapsed(), addr, v);
        case            0x4017:  // return Joypad::read_state(1);                  // Joypad 1.

        case            0x4016:  // return Joypad::read_state(0);                  // Joypad 0.
        case 0x4018 ... 0xFFFF:  return cartridge_cpu_read(addr);              // Cartridge.
    }
    return 0;
}
uint8 write_access(uint16 addr, uint8 value) {
    uint8* r;
    switch (addr) {
        case 0x0000 ... 0x1FFF:  r = &RAM[addr % 0x800]; *r = value; return *r;  // RAM.
        case 0x2000 ... 0x3FFF:  return ppu_write(addr % 8, value);                // PPU.

        // APU:
        case 0x4000 ... 0x4013:
        case            0x4015:  //        return APU::access<wr>(elapsed(), addr, v);
        case            0x4017:  // if (wr) return APU::access<wr>(elapsed(), addr, v);
                                 // else return Joypad::read_state(1);                  // Joypad 1.

        case            0x4014:  // if (wr) dma_oam(v); break;                          // OAM DMA.
        case            0x4016:  // if (wr) { Joypad::write_strobe(v & 1); break; }     // Joypad strobe.
                                 // else return Joypad::read_state(0);                  // Joypad 0.
        case 0x4018 ... 0xFFFF:  return cartridge_cpu_write(addr, value);              // Cartridge.
    }
    return 0;    
}
*/

inline uint8  wr(uint16 a, uint8 v)   { T; return K6502_Write(a, v); }
inline uint8  rd(uint16 a)            { T; return K6502_Read(a);      }
inline uint16 rd16_d(uint16 a, uint16 b) { return rd(a) | (rd(b) << 8); }  // Read from A and B and merge.
inline uint16 rd16(uint16 a)          { return rd16_d(a, a+1);       }
inline uint8  push(uint8 v)           { return wr(0x100 + (SP--), v); }
inline uint8  pop()                   { return rd(0x100 + (++SP));    }
void dma_oam(uint8 bank) { for (int i = 0; i < 256; i++)  wr(0x2014, rd(bank*0x100 + i)); }

/* Addressing modes */
inline uint16 imm()   { return PC++;                                       }
inline uint16 imm16() { PC += 2; return PC - 2;                            }
inline uint16 _abs()   { return rd16(imm16());                              }
inline uint16 _abx()  { T; return _abs() + X;                               }  // Exception.
inline uint16 abx()   { uint16 a = _abs(); if (cross(a, X)) T; return a + X;   }
inline uint16 aby()   { uint16 a = _abs(); if (cross(a, Y)) T; return a + Y;   }
inline uint16 zp()    { return rd(imm());                                  }
inline uint16 zpx()   { T; return (zp() + X) % 0x100;                      }
inline uint16 zpy()   { T; return (zp() + Y) % 0x100;                      }
inline uint16 izx()   { uint16 i = zpx(); return rd16_d(i, (i+1) % 0x100);     }
inline uint16 _izy()  { uint16 i = zp();  return rd16_d(i, (i+1) % 0x100) + Y; }  // Exception.
inline uint16 izy()   { uint16 a = _izy(); if (cross(a-Y, Y)) T; return a;    }

/* STx */
#define st(r, m) wr( m(), r )
#define st_A_izy() T; wr(_izy(), A);
#define st_A_abx() T; wr(_abs() + X, A);
#define st_A_aby() T; wr(_abs() + Y, A);

#define G(m)  uint16 a = m(); uint8 p = rd(a)  /* Fetch parameter */
#define ld(r, m) { G(m); upd_nz(r = p); }
#define cmp(r, m) { G(m); upd_nz(r - p); C = (r >= p); }

/* Arithmetic and bitwise */
#define ADC(m) { G(m); int16 r = A + p + C; upd_cv(A, p, r); upd_nz(A = r); }
#define SBC(m) { G(m) ^ 0xFF; int16 r = A + p + C; upd_cv(A, p, r); upd_nz(A = r); }
#define BIT(m) { G(m); Z = !(A & p); N = p & 0x80; V = p & 0x40; }
#define AND(m) { G(m); upd_nz(A &= p); }
#define EOR(m) { G(m); upd_nz(A ^= p); }
#define ORA(m) { G(m); upd_nz(A |= p); }
/* Read-Modify-Write */
#define ASL(m) { G(m); C = p & 0x80; T; upd_nz(wr(a, p << 1)); }
#define LSR(m) { G(m); C = p & 0x01; T; upd_nz(wr(a, p >> 1)); }
#define ROL(m) { G(m); uint8 c = C     ; C = p & 0x80; T; upd_nz(wr(a, (p << 1) | c) ); }
#define ROR(m) { G(m); uint8 c = C << 7; C = p & 0x01; T; upd_nz(wr(a, c | (p >> 1)) ); }
#define DEC(m) { G(m); T; upd_nz(wr(a, --p)); }
#define INC(m) { G(m); T; upd_nz(wr(a, ++p)); }
// #undef G

/* DEx, INx */
#define dec(r) upd_nz(--r); T;
#define inc(r) upd_nz(++r); T;
/* Bit shifting on the accumulator */
inline void ASL_A() { C = A & 0x80; upd_nz(A <<= 1); T; }
inline void LSR_A() { C = A & 0x01; upd_nz(A >>= 1); T; }
inline void ROL_A() { uint8 c = C     ; C = A & 0x80; upd_nz(A = ((A << 1) | c) ); T; }
inline void ROR_A() { uint8 c = C << 7; C = A & 0x01; upd_nz(A = (c | (A >> 1)) ); T; }

/* Txx (move values between registers) */
#define tr(s, d) upd_nz(d = s); T;
#define tr_X_SP() SP = X; T;

/* Stack operations */
inline void PLP() { T; T; set_flags(pop()); }
inline void PHP() { T; push(get_flags() | (1 << 4)); }  // B flag set.
inline void PLA() { T; T; A = pop(); upd_nz(A);  }
inline void PHA() { T; push(A); }

/* Flow control (branches, jumps) */
#define br(f, v) { int8 j = rd(imm()); if (f == v) { if (cross(PC, j)) T; T; PC += j; } }
inline void JMP_IND() { uint16 i = rd16(imm16()); PC = rd16_d(i, (i&0xFF00) | ((i+1) % 0x100)); }
inline void JMP()     { PC = rd16(imm16()); }
inline void JSR()     { uint16 t = PC+1; T; push(t >> 8); push(t); PC = rd16(imm16()); }

/* Return instructions */
inline void RTS() { T; T;  PC = (pop() | (pop() << 8)) + 1; T; }
inline void RTI() { PLP(); PC =  pop() | (pop() << 8);         }

#define NMI 0
#define RESET 1
#define IRQ 2
#define BRK 3
uint16 VECTORS[] = { 0xFFFA, 0xFFFC, 0xFFFE, 0xFFFE };

#define flag(f, v) f = v; T;
#define INT(t) {\
    T;\
    if (t != BRK)\
        T;\
    if (t != RESET) {\
        push(PC >> 8); push(PC & 0xFF);\
        push(get_flags() | ((t == BRK) << 4));\
    }\
    else { SP -= 3; T; T; T; }\
    I = true;\
    PC = rd16(VECTORS[t]);\
    if (t == NMI) nmi = false;\
}
#define NOP() { T; }

#ifdef DEBUG
extern uint32 currentCRC32;
file_t traceFile;
bool traceEnabled = false;
char* pTraceBuffer;
#define TRACE_BUFFER_LENGTH 1048576
char traceBuffer[TRACE_BUFFER_LENGTH];

void UploadTraceBuffer() {
	fs_write(traceFile, traceBuffer, (pTraceBuffer - traceBuffer));
	pTraceBuffer = traceBuffer;
}

void TraceInstruction(char* mnemonic, char* fmt, uint16 value) {
	if (traceEnabled) {
		char p[32];
		char trace[80];
		if (fmt) {
			snprintf(p, 32, fmt, value);
		} else {
			p[0] = '\0';
		}
		snprintf(trace, 80, "A:%02X X:%02X Y:%02X S:%02X    $%04X: %s %s\n", A, X, Y, SP, PC, mnemonic, p);
		uint32 traceLength = strlen(trace);
		if ((TRACE_BUFFER_LENGTH - (pTraceBuffer - traceBuffer)) < traceLength) {
			UploadTraceBuffer();
		}
		strcpy(pTraceBuffer, trace);
		pTraceBuffer += traceLength;
	}
}

void TraceInstruction2(char* mnemonic, char* fmt, uint16 value, uint16 value2) {
	if (traceEnabled) {
		char p[32];
		char trace[64];
		if (fmt) {
			snprintf(p, 32, fmt, value, value2);
		} else {
			p[0] = '\0';
		}
		snprintf(trace, 80, "A:%02X X:%02X Y:%02X S:%02X    $%04X: %s %s\n", A, X, Y, SP, PC, mnemonic, p);
		uint32 traceLength = strlen(trace);
		if ((TRACE_BUFFER_LENGTH - (pTraceBuffer - traceBuffer)) < traceLength) {
			UploadTraceBuffer();
		}
		strcpy(pTraceBuffer, trace);
		pTraceBuffer += traceLength;
	}
}

void TraceInstruction3(char* mnemonic, char* fmt, uint16 value, uint16 value2, uint16 value3) {
	if (traceEnabled) {
		char p[32];
		char trace[64];
		if (fmt) {
			snprintf(p, 32, fmt, value, value2, value3);
		} else {
			p[0] = '\0';
		}
		snprintf(trace, 80, "A:%02X X:%02X Y:%02X S:%02X    $%04X: %s %s\n", A, X, Y, SP, PC, mnemonic, p);
		uint32 traceLength = strlen(trace);
		if ((TRACE_BUFFER_LENGTH - (pTraceBuffer - traceBuffer)) < traceLength) {
			UploadTraceBuffer();
		}
		strcpy(pTraceBuffer, trace);
		pTraceBuffer += traceLength;
	}
}

void EndTracing() {
	printf("Closing tracing with PC Host\n");
	UploadTraceBuffer();
	fs_close(traceFile);
}

void StartTracing() {
	printf("Connecting to PC Host for Tracing\n");
	char PCPath[256];
	// TODO: use a log time or something like that in the filename here
	snprintf(PCPath, 256, "/pc/Users/maslevin/Documents/Projects/numechanix/frnes/trace_%08lX.txt", currentCRC32);
	traceFile = fs_open(PCPath, O_WRONLY);
	if (traceFile != -1) {
		printf("Opened Trace File [%s] on PC host\n", PCPath);
		traceEnabled = true;
		pTraceBuffer = traceBuffer;
	} else {
		printf("Error: Unable to start tracing with PC host, disabling trace\n");
		traceEnabled = false;
	}
}

void trace() {
    switch (rd(PC)) {
        case 0x00:
            TraceInstruction("BRK", NULL, 0);
            break;

        case 0x01:
            TraceInstruction("ORA", "($%04x,x)", rd16(PC+1));
            break;

        case 0x05:
            TraceInstruction("ORA", "$%02x", rd(PC + 1));
            break;

        case 0x06:
            TraceInstruction("ASL", "$%02x", rd(PC + 1));
            break;

        case 0x08:
            TraceInstruction("PHP", NULL, 0);
            break;

        case 0x09:
            TraceInstruction("ORA", "#$%02x", rd(PC + 1));
            break;

        case 0x0A:
            TraceInstruction("ASL", NULL, 0);
            break;

        case 0x0D:
            TraceInstruction("ORA", "$%04x", rd16(PC+1));
            break;

        case 0x0E:
            TraceInstruction("ASL", "$%04x", rd16(PC+1));
            break;

        case 0x10:
            TraceInstruction("BPL", "$%04x", (PC + 2) + (char)rd(PC+1));
            break;

        case 0x11:
            TraceInstruction("ORA", "($%04x),y", rd16(PC+1));
            break;
        
        case 0x15:
            TraceInstruction("ORA", "$%02x,x", rd(PC + 1));	
            break;

        case 0x16:
            TraceInstruction("ASL", "$%02x,x", rd(PC + 1));	
            break;

        case 0x18:
            TraceInstruction("CLC", NULL, 0);
            break;

        case 0x19:
            TraceInstruction("ORA", "$%04x,y", rd16(PC+1));	
            break;

        case 0x1D:
            TraceInstruction("ORA", "$%04x,x", rd16(PC+1));
            break;

        case 0x1E:
            TraceInstruction("ASL", "$%04x,x", rd16(PC+1));
            break;

        case 0x20:
            TraceInstruction("JSR", "$%04x", rd16(PC+1));
            break;

        case 0x21:
            TraceInstruction("AND", "($%02x,x)", rd(PC + 1));
            break;

        case 0x24:
            TraceInstruction("BIT", "$%02x", rd(PC + 1));
            break;

        case 0x25:
            TraceInstruction("AND", "$%02x", rd(PC + 1));
            break;

        case 0x26:
            TraceInstruction("ROL", "$%02x", rd(PC + 1));
            break;

        case 0x28:
            TraceInstruction("PLP", NULL, 0);
            break;

        case 0x29:
            TraceInstruction("AND", "#$%02x", rd(PC + 1));	
            break;

        case 0x2A:
            TraceInstruction("ROL", NULL, 0);	
            break;

        case 0x2C:
            TraceInstruction2("BIT", "$%04x = #$%02x", rd16(PC+1), rd(rd16(PC+1)));	
            break;

        case 0x2D:
            TraceInstruction("AND", "$%04x", rd16(PC+1));
            break;

        case 0x2E:
            TraceInstruction("ROL", "$%04x", rd16(PC+1));
            break;

        case 0x30:
            TraceInstruction("BMI", "$%02x", rd(PC + 1));	
            break;

        case 0x31:
            TraceInstruction("AND", "($%02x),y", rd(PC + 1));
            break;

        case 0x35:
            TraceInstruction("AND", "$%02x,x", rd(PC + 1));	
            break;

        case 0x36:
            TraceInstruction("ROL", "$%02x,x", rd(PC + 1));	
            break;				

        case 0x38:
            TraceInstruction("SEC", NULL, 0);
            break;

        case 0x39:
            TraceInstruction("AND", "$%04x,y", rd16(PC+1));
            break;

        case 0x3D:
            TraceInstruction("AND", "$%04x,x", rd16(PC+1));	
            break;

        case 0x3E:
            TraceInstruction("ROL", "$%04x,x", rd16(PC+1));
            break;

        case 0x40:
            TraceInstruction("RTI", NULL, 0);
            break;

        case 0x41:
            TraceInstruction("EOR", "($%02x,x)", rd(PC + 1));	
            break;

        case 0x45:
            TraceInstruction("EOR", "$%02x", rd(PC + 1));	
            break;

        case 0x46:
            TraceInstruction("LSR", "$%02x", rd(PC + 1));
            break;

        case 0x48:
            TraceInstruction("PHA", NULL, 0);
            break;

        case 0x49:
            TraceInstruction("EOR", "#$%02x", rd(PC + 1));
            break;				

        case 0x4A:
            TraceInstruction("LSR", NULL, 0);	
            break;

        case 0x4C:
            TraceInstruction("JMP", "$%04x", rd16(PC+1));	
            break;

        case 0x4D:
            TraceInstruction("EOR", "$%04x", rd16(PC+1));	
            break;

        case 0x4E:
            TraceInstruction("LSR", "$%04x", rd16(PC+1));	
            break;

        case 0x50:
            TraceInstruction("BVC", "$%02x", rd(PC + 1));
            break;

        case 0x51:
            TraceInstruction("EOR", "($%02x),y", rd(PC + 1));
            break;				

        case 0x55:
            TraceInstruction("EOR", "$%02x,x", rd(PC + 1));	
            break;

        case 0x56:
            TraceInstruction("LSR", "$%02x,x", rd(PC + 1));	
            break;						

        case 0x58:
            TraceInstruction("CLI", NULL, 0);	
            break;

        case 0x59:
            TraceInstruction("EOR", "$%04x,y", rd16(PC+1));	
            break;

        case 0x5D:
            TraceInstruction("EOR", "$%04x,x", rd16(PC+1));
            break;

        case 0x5E:
            TraceInstruction("LSR", "$%04x,x", rd16(PC+1));
            break;

        case 0x60:
            TraceInstruction("RTS", NULL, 0);	
            break;

        case 0x61:
            TraceInstruction("ADC", "($%02x,x)", rd(PC + 1));
            break;

        case 0x65:
            TraceInstruction("ADC", "$%02x", rd(PC + 1));	
            break;

        case 0x66:
            TraceInstruction("ROR", "$%02x", rd(PC + 1));
            break;

        case 0x68:
            TraceInstruction("PLA", NULL, 0);	
            break;

        case 0x69:
            TraceInstruction("ADC", "#$%02x", rd(PC + 1));
            break;

        case 0x6A:
            TraceInstruction("ROR", NULL, 0);	
            break;

        case 0x6C:
            TraceInstruction2("JMP", "($%04x) = $%04x", rd16(PC+1), K6502_ReadW(rd16(PC+1)));
            break;
    
        case 0x6D:
            TraceInstruction("ADC", "$%04x", rd16(PC+1));
            break;

        case 0x6E:
            TraceInstruction("ROR", "$%04x", rd16(PC+1));	
            break;

        case 0x70:
            TraceInstruction("BVS", "$%02x", rd(PC + 1));
            break;

        case 0x71:
            TraceInstruction("ADC", "($%04x),y", rd16(PC+1));
            break;				

        case 0x75:
            TraceInstruction("ADC", "$%02x,x", rd(PC + 1));	
            break;

        case 0x76:
            TraceInstruction("ROR", "$%02x,x", rd(PC + 1));	
            break;

        case 0x78:
            TraceInstruction("SEI", NULL, 0);	
            break;

        case 0x79:
            TraceInstruction("ADC", "$%04x,y", rd16(PC+1));
            break;				

        case 0x7D:
            TraceInstruction("ADC", "$%04x,x", rd16(PC+1));	
            break;				

        case 0x7E:
            TraceInstruction("ROR", "$%04x,x", rd16(PC+1));
            break;

        case 0x81:
            TraceInstruction("STA", "($%02x,x)", rd(PC + 1));	
            break;

        case 0x84:
            TraceInstruction("STY", "$%02x", rd(PC + 1));
            break;

        case 0x85:
            TraceInstruction("STA", "$%02x", rd(PC + 1));
            break;

        case 0x86:
            TraceInstruction("STX", "$%02x", rd(PC + 1));	
            break;

        case 0x88:
            TraceInstruction("DEY", NULL, 0);
            break;

        case 0x89:
            TraceInstruction("BIT", "#$%02x", rd(PC + 1));
            break;

        case 0x8A:
            TraceInstruction("TXA", NULL, 0);
            break;

        case 0x8C:
            TraceInstruction("STY", "$%04x", rd16(PC+1));
            break;

        case 0x8D:
            TraceInstruction("STA", "$%04x", rd16(PC+1));
            break;

        case 0x8E:
            TraceInstruction("STX", "$%04x", rd16(PC+1));	
            break;

        case 0x90:
            TraceInstruction("BCC", "$%02x", rd(PC + 1));
            break;

        case 0x91:
            TraceInstruction("STA", "($%02x),y", rd(PC+1));	
            break;			

        case 0x94:
            TraceInstruction("STY", "$%02x,x", rd(PC + 1));
            break;

        case 0x95:
            TraceInstruction("STA", "$%02x,x", rd(PC + 1));	
            break;

        case 0x96:
            TraceInstruction("STX", "$%02x,x", rd(PC + 1));	
            break;

        case 0x98:
            TraceInstruction("TYA", NULL, 0);	
            break;

        case 0x99:
            TraceInstruction("STA", "$%04x,y", rd16(PC+1));
            break;

        case 0x9A:
            TraceInstruction("TXS", NULL, 0);	
            break;

        case 0x9D:
            TraceInstruction("STA", "$%04x,x", rd16(PC+1));
            break;

        case 0xA0:
            TraceInstruction("LDY", "#$%02x", rd(PC + 1));	
            break;

        case 0xA1:
            TraceInstruction("LDA", "($%04x,x)", rd16(PC+1));	
            break;

        case 0xA2:
            TraceInstruction("LDX", "#$%02x", rd(PC + 1));	
            break;

        case 0xA4:
            TraceInstruction("LDY", "$%02x", rd(PC + 1));
            break;

        case 0xA5:
            TraceInstruction("LDA", "$%02x", rd(PC + 1));
            break;

        case 0xA6:
            TraceInstruction("LDX", "$%02x", rd(PC + 1));	
            break;

        case 0xA8:
            TraceInstruction("TAY", NULL, 0);	
            break;

        case 0xA9:
            TraceInstruction("LDA", "#$%02x", rd(PC + 1));	
            break;

        case 0xAA:
            TraceInstruction("TAX", NULL, 0);	
            break;

        case 0xAC:
            TraceInstruction("LDY", "$%04x", rd16(PC+1));	
            break;

        case 0xAD:
            TraceInstruction("LDA", "$%04x", rd16(PC+1));	
            break;

        case 0xAE:
            TraceInstruction("LDX", "$%04x", rd16(PC+1));	
            break;

        case 0xB0:
            TraceInstruction("BCS", "$%04x", (PC + 2) + (char)rd(PC + 1));	
            break;

        case 0xB1:
            TraceInstruction("LDA", "($%04x),y", rd16(PC+1));	
            break;

        case 0xB4:
            TraceInstruction("LDY", "$%02x,x", rd(PC + 1));
            break;

        case 0xB5:
            TraceInstruction("LDA", "$%02x,x", rd(PC + 1));
            break;

        case 0xB6:
            TraceInstruction("LDX", "$%02x,x", rd(PC + 1));
            break;

        case 0xB8:
            TraceInstruction("CLV", NULL, 0);	
            break;

        case 0xB9:
            TraceInstruction3("LDA", "$%04x,y @ $%04x = #$%02x", rd16(PC+1), rd16(PC+1) | Y, RAM[rd16(PC+1) | Y]);
            break;																

        case 0xBA:
            TraceInstruction("TSX", NULL, 0);
            break;

        case 0xBC:
            TraceInstruction("LDY", "$%04x,x", rd16(PC+1));
            break;

        case 0xBD:
            TraceInstruction("LDA", "$%04x,x", rd16(PC+1));
            break;

        case 0xBE:
            TraceInstruction("LDX", "$%04x,x", rd16(PC+1));
            break;

        case 0xC0:
            TraceInstruction("CPY", "#$%02x", rd(PC + 1));	
            break;

        case 0xC1:
            TraceInstruction("CMP", "($%04x,x)", rd16(PC+1));	
            break;

        case 0xC4:
            TraceInstruction("CPY", "$%02x", rd(PC + 1));	
            break;

        case 0xC5:
            TraceInstruction("CMP", "$%02x", rd(PC + 1));	
            break;

        case 0xC6:
            TraceInstruction("DEC", "$%02x", rd(PC + 1));
            break;

        case 0xC8:
            TraceInstruction("INY", NULL, 0);	
            break;

        case 0xC9:
            TraceInstruction("CMP", "#$%02x", rd(PC + 1));
            break;

        case 0xCA:
            TraceInstruction("DEX", NULL, 0);	
            break;

        case 0xCB:
            TraceInstruction("AXS", "$%02x", rd(PC + 1));	
            break;				

        case 0xCC:
            TraceInstruction("CPY", "$%04x", rd16(PC+1));	
            break;

        case 0xCD:
            TraceInstruction("CMP", "$%04x", rd16(PC+1));	
            break;

        case 0xCE:
            TraceInstruction("DEC", "$%04x", rd16(PC+1));	
            break;

        case 0xD0:
            TraceInstruction("BNE", "$%04x", (PC + 1) + (char)rd(PC + 1));
            break;																																					

        case 0xD1:
            TraceInstruction("CMP", "($%04x),y", rd16(PC+1));	
            break;

        case 0xD5:
            TraceInstruction("CMP", "$%02x,x", rd(PC + 1));	
            break;

        case 0xD6:
            TraceInstruction("DEC", "$%02x,x", rd(PC + 1));	
            break;

        case 0xD8:
            TraceInstruction("CLD", NULL, 0);	
            break;

        case 0xD9:
            TraceInstruction("CMP", "$%04x,y", rd16(PC+1));
            break;

        case 0xDD:
            TraceInstruction("CMP", "$%04x,x", rd16(PC+1));
            break;

        case 0xDE:
            TraceInstruction("DEC", "$%04x,x", rd16(PC+1));
            break;

        case 0xE0:
            TraceInstruction("CPX", "#$%02x", rd(PC + 1));
            break;

        case 0xE1:
            TraceInstruction("SBC", "($%04x,x)", rd16(PC+1));	
            break;

        case 0xE4:
            TraceInstruction("CPX", "$%02x", rd(PC + 1));
            break;

        case 0xE5:
            TraceInstruction("SBC", "$%02x", rd(PC + 1));
            break;

        case 0xE6:
            TraceInstruction("INC", "$%02x", rd(PC + 1));
            break;

        case 0xE8:
            TraceInstruction("INX", NULL, 0);
            break;

        case 0xE9:
            TraceInstruction("SBC", "#$%02x", rd(PC + 1));
            break;

        case 0xEA:
            TraceInstruction("NOP", NULL, 0);	
            break;

        case 0xEC:
            TraceInstruction("CPX", "$%04x", rd16(PC+1));
            break;

        case 0xED:
            TraceInstruction("SBC", "$%04x", rd16(PC+1));	
            break;
            
        case 0xEE:
            TraceInstruction("INC", "$%04x", rd16(PC+1));	
            break;

        case 0xF0:
            TraceInstruction("BEQ", "$%04x", PC + rd(PC + 1));	
            break;

        case 0xF1:
            TraceInstruction("SBC", "($%04x),y", rd16(PC+1));	
            break;

        case 0xF5:
            TraceInstruction("SBC", "$%02x,x", rd(PC + 1));	
            break;

        case 0xF6:
            TraceInstruction("INC", "$%02x,x", rd(PC + 1));	
            break;

        case 0xF8:
            TraceInstruction("SED", NULL, 0);	
            break;

        case 0xF9:
            TraceInstruction("SBC", "$%04x,y", rd16(PC+1));
            break;

        case 0xFD:
            TraceInstruction("SBC", "$%04x,x", rd16(PC+1));
            break;

        case 0xFE:
            TraceInstruction("INC", "$%04x,x", rd16(PC+1));
            break; 
    } 
}

#else
#define StartTracing() (0)
#define EndTracing() (0)
#define TraceInstruction(...) (0)
#define TraceInstruction2(...) (0)
#define TraceInstruction3(...) (0)
#define trace() (0)
#endif

void PrintRegisters() {
	printf("PC: [$%04x] A: [$%02x] X: [$%02x] Y: [$%02x] SP: [$%04x]\n", PC - 1, A, X, Y, 0x100 + SP);
	printf("Stack: \n");
	for (int SP_Index = SP + 1; SP_Index <= 0xFF; SP_Index++) {
		printf("$%04x: %02x\n", 0x100 + SP_Index, RAM[0x100 + SP_Index]);
	}
}

/* Execute a CPU instruction */
void exec() {
    switch (rd(PC++)) {
        case 0x00: INT(BRK); return;
        case 0x01: ORA(izx); return;
        case 0x05: ORA(zp); return;
        case 0x06: ASL(zp); return;
        case 0x08: PHP(); return;
        case 0x09: ORA(imm); return;
        case 0x0A: ASL_A(); return;
        case 0x0D: ORA(_abs); return;
        case 0x0E: ASL(_abs); return;
        case 0x10: br(N,0); return;
        case 0x11: ORA(izy); return;
        case 0x15: ORA(zpx); return;
        case 0x16: ASL(zpx); return;
        case 0x18: flag(C,0); return;
        case 0x19: ORA(aby); return;
        case 0x1D: ORA(abx); return;
        case 0x1E: ASL(_abx); return;
        case 0x20: JSR(); return;
        case 0x21: AND(izx); return;
        case 0x24: BIT(zp); return;
        case 0x25: AND(zp); return;
        case 0x26: ROL(zp); return;
        case 0x28: PLP(); return;
        case 0x29: AND(imm); return;
        case 0x2A: ROL_A(); return;
        case 0x2C: BIT(_abs); return;
        case 0x2D: AND(_abs); return;
        case 0x2E: ROL(_abs); return;
        case 0x30: br(N,1); return;
        case 0x31: AND(izy); return;
        case 0x35: AND(zpx); return;
        case 0x36: ROL(zpx); return;
        case 0x38: flag(C,1); return;
        case 0x39: AND(aby); return;
        case 0x3D: AND(abx); return;
        case 0x3E: ROL(_abx); return;
        case 0x40: RTI(); return;
        case 0x41: EOR(izx); return;
        case 0x45: EOR(zp); return;
        case 0x46: LSR(zp); return;
        case 0x48: PHA(); return;
        case 0x49: EOR(imm); return;
        case 0x4A: LSR_A(); return;
        case 0x4C: JMP(); return;
        case 0x4D: EOR(_abs); return;
        case 0x4E: LSR(_abs); return;
        case 0x50: br(V,0); return;
        case 0x51: EOR(izy); return;
        case 0x55: EOR(zpx); return;
        case 0x56: LSR(zpx); return;
        case 0x58: flag(I,0); printf("Interrupts Enabled\n");return;
        case 0x59: EOR(aby); return;
        case 0x5D: EOR(abx); return;
        case 0x5E: LSR(_abx); return;
        case 0x60: RTS(); return;
        case 0x61: ADC(izx); return;
        case 0x65: ADC(zp); return;
        case 0x66: ROR(zp); return;
        case 0x68: PLA(); return;
        case 0x69: ADC(imm); return;
        case 0x6A: ROR_A(); return;
        case 0x6C: JMP_IND(); return;
        case 0x6D: ADC(_abs); return;
        case 0x6E: ROR(_abs); return;
        case 0x70: br(V,1); return;
        case 0x71: ADC(izy); return;
        case 0x75: ADC(zpx); return;
        case 0x76: ROR(zpx); return;
        case 0x78: flag(I,1); printf("Interrupts Disabled\n"); return;
        case 0x79: ADC(aby); return;
        case 0x7D: ADC(abx); return;
        case 0x7E: ROR(_abx); return;
        case 0x81: st(A,izx); return;
        case 0x84: st(Y,zp); return;
        case 0x85: st(A,zp); return;
        case 0x86: st(X,zp); return;
        case 0x88: dec(Y); return;
        case 0x8A: tr(X,A); return;
        case 0x8C: st(Y,_abs); return;
        case 0x8D: st(A,_abs); return;
        case 0x8E: st(X,_abs); return;
        case 0x90: br(C,0); return;
        case 0x91: st_A_izy(); return;
        case 0x94: st(Y,zpx); return;
        case 0x95: st(A,zpx); return;
        case 0x96: st(X,zpy); return;
        case 0x98: tr(Y,A); return;
        case 0x99: st_A_aby(); return;
        case 0x9A: tr_X_SP(); return;
        case 0x9D: st_A_abx(); return;
        case 0xA0: ld(Y,imm); return;
        case 0xA1: ld(A,izx); return;
        case 0xA2: ld(X,imm); return;
        case 0xA4: ld(Y,zp); return;
        case 0xA5: ld(A,zp); return;
        case 0xA6: ld(X,zp); return;
        case 0xA8: tr(A,Y); return;
        case 0xA9: ld(A,imm); return;
        case 0xAA: tr(A,X); return;
        case 0xAC: ld(Y,_abs); return;
        case 0xAD: ld(A,_abs); return;
        case 0xAE: ld(X,_abs); return;
        case 0xB0: br(C,1); return;
        case 0xB1: ld(A,izy); return;
        case 0xB4: ld(Y,zpx); return;
        case 0xB5: ld(A,zpx); return;
        case 0xB6: ld(X,zpy); return;
        case 0xB8: flag(V,0); return;
        case 0xB9: ld(A,aby); return;
        case 0xBA: tr(SP,X); return;
        case 0xBC: ld(Y,abx); return;
        case 0xBD: ld(A,abx); return;
        case 0xBE: ld(X,aby); return;
        case 0xC0: cmp(Y,imm); return;
        case 0xC1: cmp(A,izx); return;
        case 0xC4: cmp(Y,zp); return;
        case 0xC5: cmp(A,zp); return;
        case 0xC6: DEC(zp); return;
        case 0xC8: inc(Y); return;
        case 0xC9: cmp(A,imm); return;
        case 0xCA: dec(X); return;
        case 0xCC: cmp(Y,_abs); return;
        case 0xCD: cmp(A,_abs); return;
        case 0xCE: DEC(_abs); return;
        case 0xD0: br(Z,0); return;
        case 0xD1: cmp(A,izy); return;
        case 0xD5: cmp(A,zpx); return;
        case 0xD6: DEC(zpx); return;
        case 0xD8: flag(D,0); return;
        case 0xD9: cmp(A,aby); return;
        case 0xDD: cmp(A,abx); return;
        case 0xDE: DEC(_abx); return;
        case 0xE0: cmp(X,imm); return;
        case 0xE1: SBC(izx); return;
        case 0xE4: cmp(X,zp); return;
        case 0xE5: SBC(zp); return;
        case 0xE6: INC(zp); return;
        case 0xE8: inc(X); return;
        case 0xE9: SBC(imm); return;
        case 0xEA: NOP(); return;
        case 0xEC: cmp(X,_abs); return;
        case 0xED: SBC(_abs); return;
        case 0xEE: INC(_abs); return;
        case 0xF0: br(Z,1); return;
        case 0xF1: SBC(izy); return;
        case 0xF5: SBC(zpx); return;
        case 0xF6: INC(zpx); return;
        case 0xF8: flag(D,1); return;
        case 0xF9: SBC(aby); return;
        case 0xFD: SBC(abx); return;
        case 0xFE: INC(_abx); return;
        default:
            HALT = true;
            PrintRegisters();
//            std::cout << "Invalid Opcode! PC: " << PC << " Opcode: 0x" << std::hex << (int)(rd(PC - 1)) << "\n";
            return;
    }
}

void set_nmi(bool v) { nmi = v; }
void set_irq(bool v) { irq = v; }

/* Turn on the CPU */
void power() {
    remainingCycles = 0;

    set_flags(0x04);
    A = X = Y = SP = 0x00;
    memset(RAM, 0xFF, RAM_SIZE);

    nmi = irq = false;
    INT(RESET);
}

void run_cycles(uint32 cycles) {
    remainingCycles += cycles;

    while ((remainingCycles > 0) && !HALT) {
        if (nmi) {
            INT(NMI);
        } else if (irq && !I) {
            printf("Running Interrupt\n");
            INT(IRQ);
        }
        trace();
        exec();
    }
}
    