#!/usr/bin/env python3
"""
AVR Disassembler for ATmega16
Converts binary firmware to assembly code
"""

import struct

class AVRDisassembler:
    def __init__(self, binary_data):
        self.data = binary_data
        self.pc = 0
        self.labels = {}
        self.output = []
        
    def read_word(self, addr):
        """Read 16-bit word (little endian)"""
        if addr + 1 < len(self.data):
            return struct.unpack('<H', self.data[addr:addr+2])[0]
        return 0
    
    def disassemble_instruction(self, addr):
        """Disassemble single instruction at address"""
        opcode = self.read_word(addr)
        
        # Extract common fields
        Rd = (opcode >> 4) & 0x1F
        Rr = ((opcode >> 5) & 0x10) | (opcode & 0x0F)
        K8 = ((opcode >> 4) & 0xF0) | (opcode & 0x0F)
        K6 = ((opcode >> 2) & 0x30) | (opcode & 0x0F)
        A5 = (opcode >> 3) & 0x1F
        A6 = ((opcode >> 5) & 0x30) | (opcode & 0x0F)
        b = opcode & 0x07
        
        # Two-word instructions
        next_word = 0
        size = 2
        
        # Decode instruction
        if opcode == 0x0000:
            return "nop", size
            
        elif opcode == 0x9508:
            return "ret", size
            
        elif opcode == 0x9518:
            return "reti", size
            
        elif opcode == 0x9588:
            return "sleep", size
            
        elif opcode == 0x9598:
            return "break", size
            
        elif opcode == 0x95A8:
            return "wdr", size
            
        elif opcode == 0x95C8:
            return "lpm", size
            
        elif opcode == 0x95D8:
            return "elpm", size
            
        elif opcode == 0x95E8:
            return "spm", size
            
        elif opcode == 0x95F8:
            return "espm", size
            
        elif opcode == 0x9408:
            return "sec", size
            
        elif opcode == 0x9418:
            return "sez", size
            
        elif opcode == 0x9428:
            return "sen", size
            
        elif opcode == 0x9438:
            return "sev", size
            
        elif opcode == 0x9448:
            return "ses", size
            
        elif opcode == 0x9458:
            return "seh", size
            
        elif opcode == 0x9468:
            return "set", size
            
        elif opcode == 0x9478:
            return "sei", size
            
        elif opcode == 0x9488:
            return "clc", size
            
        elif opcode == 0x9498:
            return "clz", size
            
        elif opcode == 0x94A8:
            return "cln", size
            
        elif opcode == 0x94B8:
            return "clv", size
            
        elif opcode == 0x94C8:
            return "cls", size
            
        elif opcode == 0x94D8:
            return "clh", size
            
        elif opcode == 0x94E8:
            return "clt", size
            
        elif opcode == 0x94F8:
            return "cli", size
            
        elif (opcode & 0xFE0F) == 0x9000:
            # LDS - Load direct from data space (32-bit)
            next_word = self.read_word(addr + 2)
            size = 4
            return f"lds r{Rd}, 0x{next_word:04X}", size
            
        elif (opcode & 0xFE0F) == 0x9200:
            # STS - Store direct to data space (32-bit)
            next_word = self.read_word(addr + 2)
            size = 4
            return f"sts 0x{next_word:04X}, r{Rd}", size
            
        elif (opcode & 0xFE0E) == 0x940C:
            # JMP - Jump (32-bit)
            next_word = self.read_word(addr + 2)
            target = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | next_word
            target *= 2  # Word address to byte address
            size = 4
            return f"jmp 0x{target:04X}", size
            
        elif (opcode & 0xFE0E) == 0x940E:
            # CALL - Call subroutine (32-bit)
            next_word = self.read_word(addr + 2)
            target = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | next_word
            target *= 2  # Word address to byte address
            size = 4
            return f"call 0x{target:04X}", size
            
        elif (opcode & 0xFC00) == 0x0C00:
            # ADD - Add without carry
            return f"add r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x1C00:
            # ADC - Add with carry
            return f"adc r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x0800:
            # SBC - Subtract with carry
            return f"sbc r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x1800:
            # SUB - Subtract without carry
            return f"sub r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x2000:
            # AND - Logical AND
            return f"and r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x2400:
            # EOR - Exclusive OR
            return f"eor r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x2800:
            # OR - Logical OR
            return f"or r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFC00) == 0x2C00:
            # MOV - Copy register
            return f"mov r{Rd}, r{Rr}", size
            
        elif (opcode & 0xF000) == 0x3000:
            # CPI - Compare with immediate
            Rd = ((opcode >> 4) & 0x0F) + 16
            return f"cpi r{Rd}, 0x{K8:02X}", size
            
        elif (opcode & 0xF000) == 0x4000:
            # SBCI - Subtract immediate with carry
            Rd = ((opcode >> 4) & 0x0F) + 16
            return f"sbci r{Rd}, 0x{K8:02X}", size
            
        elif (opcode & 0xF000) == 0x5000:
            # SUBI - Subtract immediate
            Rd = ((opcode >> 4) & 0x0F) + 16
            return f"subi r{Rd}, 0x{K8:02X}", size
            
        elif (opcode & 0xF000) == 0x6000:
            # ORI/SBR - Logical OR with immediate
            Rd = ((opcode >> 4) & 0x0F) + 16
            return f"ori r{Rd}, 0x{K8:02X}", size
            
        elif (opcode & 0xF000) == 0x7000:
            # ANDI/CBR - Logical AND with immediate
            Rd = ((opcode >> 4) & 0x0F) + 16
            return f"andi r{Rd}, 0x{K8:02X}", size
            
        elif (opcode & 0xF000) == 0xE000:
            # LDI - Load immediate
            Rd = ((opcode >> 4) & 0x0F) + 16
            return f"ldi r{Rd}, 0x{K8:02X}", size
            
        elif (opcode & 0xFE0F) == 0x9400:
            # COM - One's complement
            return f"com r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9401:
            # NEG - Two's complement
            return f"neg r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9402:
            # SWAP - Swap nibbles
            return f"swap r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9403:
            # INC - Increment
            return f"inc r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9405:
            # ASR - Arithmetic shift right
            return f"asr r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9406:
            # LSR - Logical shift right
            return f"lsr r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9407:
            # ROR - Rotate right through carry
            return f"ror r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x940A:
            # DEC - Decrement
            return f"dec r{Rd}", size
            
        elif (opcode & 0xFF00) == 0x9600:
            # ADIW - Add immediate to word
            Rd = ((opcode >> 4) & 0x03) * 2 + 24
            K6_val = ((opcode >> 2) & 0x30) | (opcode & 0x0F)
            return f"adiw r{Rd}, 0x{K6_val:02X}", size
            
        elif (opcode & 0xFF00) == 0x9700:
            # SBIW - Subtract immediate from word
            Rd = ((opcode >> 4) & 0x03) * 2 + 24
            K6_val = ((opcode >> 2) & 0x30) | (opcode & 0x0F)
            return f"sbiw r{Rd}, 0x{K6_val:02X}", size
            
        elif (opcode & 0xFF00) == 0x9800:
            # CBI - Clear bit in I/O register
            return f"cbi 0x{A5:02X}, {b}", size
            
        elif (opcode & 0xFF00) == 0x9900:
            # SBIC - Skip if bit in I/O register cleared
            return f"sbic 0x{A5:02X}, {b}", size
            
        elif (opcode & 0xFF00) == 0x9A00:
            # SBI - Set bit in I/O register
            return f"sbi 0x{A5:02X}, {b}", size
            
        elif (opcode & 0xFF00) == 0x9B00:
            # SBIS - Skip if bit in I/O register set
            return f"sbis 0x{A5:02X}, {b}", size
            
        elif (opcode & 0xFC00) == 0x9C00:
            # MUL - Multiply unsigned
            return f"mul r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFE0F) == 0x9004:
            # LPM - Load program memory
            return f"lpm r{Rd}, Z", size
            
        elif (opcode & 0xFE0F) == 0x9005:
            # LPM - Load program memory with post-increment
            return f"lpm r{Rd}, Z+", size
            
        elif (opcode & 0xFE0F) == 0x900C:
            # LD - Load indirect from data space using X
            return f"ld r{Rd}, X", size
            
        elif (opcode & 0xFE0F) == 0x900D:
            # LD - Load indirect with post-increment using X
            return f"ld r{Rd}, X+", size
            
        elif (opcode & 0xFE0F) == 0x900E:
            # LD - Load indirect with pre-decrement using X
            return f"ld r{Rd}, -X", size
            
        elif (opcode & 0xFE0F) == 0x9009:
            # LD - Load indirect from data space using Y
            return f"ld r{Rd}, Y+", size
            
        elif (opcode & 0xFE0F) == 0x900A:
            # LD - Load indirect with pre-decrement using Y
            return f"ld r{Rd}, -Y", size
            
        elif (opcode & 0xFE0F) == 0x8008:
            # LD - Load indirect from data space using Y
            return f"ld r{Rd}, Y", size
            
        elif (opcode & 0xD208) == 0x8008:
            # LDD - Load indirect with displacement using Y
            q = ((opcode >> 8) & 0x20) | ((opcode >> 7) & 0x18) | (opcode & 0x07)
            return f"ldd r{Rd}, Y+{q}", size
            
        elif (opcode & 0xFE0F) == 0x8000:
            # LD - Load indirect from data space using Z
            return f"ld r{Rd}, Z", size
            
        elif (opcode & 0xFE0F) == 0x9001:
            # LD - Load indirect with post-increment using Z
            return f"ld r{Rd}, Z+", size
            
        elif (opcode & 0xFE0F) == 0x9002:
            # LD - Load indirect with pre-decrement using Z
            return f"ld r{Rd}, -Z", size
            
        elif (opcode & 0xD208) == 0x8000:
            # LDD - Load indirect with displacement using Z
            q = ((opcode >> 8) & 0x20) | ((opcode >> 7) & 0x18) | (opcode & 0x07)
            return f"ldd r{Rd}, Z+{q}", size
            
        elif (opcode & 0xFE0F) == 0x920C:
            # ST - Store indirect to data space using X
            return f"st X, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x920D:
            # ST - Store indirect with post-increment using X
            return f"st X+, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x920E:
            # ST - Store indirect with pre-decrement using X
            return f"st -X, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9209:
            # ST - Store indirect with post-increment using Y
            return f"st Y+, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x920A:
            # ST - Store indirect with pre-decrement using Y
            return f"st -Y, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x8208:
            # ST - Store indirect to data space using Y
            return f"st Y, r{Rd}", size
            
        elif (opcode & 0xD208) == 0x8208:
            # STD - Store indirect with displacement using Y
            q = ((opcode >> 8) & 0x20) | ((opcode >> 7) & 0x18) | (opcode & 0x07)
            return f"std Y+{q}, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x8200:
            # ST - Store indirect to data space using Z
            return f"st Z, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9201:
            # ST - Store indirect with post-increment using Z
            return f"st Z+, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9202:
            # ST - Store indirect with pre-decrement using Z
            return f"st -Z, r{Rd}", size
            
        elif (opcode & 0xD208) == 0x8200:
            # STD - Store indirect with displacement using Z
            q = ((opcode >> 8) & 0x20) | ((opcode >> 7) & 0x18) | (opcode & 0x07)
            return f"std Z+{q}, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9004:
            # LPM - Load program memory
            return f"lpm r{Rd}, Z", size
            
        elif (opcode & 0xFE0F) == 0x9005:
            # LPM - Load program memory with post-increment
            return f"lpm r{Rd}, Z+", size
            
        elif (opcode & 0xFE0F) == 0x9006:
            # ELPM - Extended load program memory
            return f"elpm r{Rd}, Z", size
            
        elif (opcode & 0xFE0F) == 0x9007:
            # ELPM - Extended load program memory with post-increment
            return f"elpm r{Rd}, Z+", size
            
        elif (opcode & 0xFE0F) == 0x9204:
            # XCH - Exchange
            return f"xch Z, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9205:
            # LAS - Load and set
            return f"las Z, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9206:
            # LAC - Load and clear
            return f"lac Z, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9207:
            # LAT - Load and toggle
            return f"lat Z, r{Rd}", size
            
        elif (opcode & 0xFE0F) == 0x9508:
            # RET - Return from subroutine
            return "ret", size
            
        elif (opcode & 0xFE0F) == 0x9509:
            # ICALL - Indirect call to subroutine
            return "icall", size
            
        elif (opcode & 0xFE0F) == 0x9519:
            # EICALL - Extended indirect call
            return "eicall", size
            
        elif (opcode & 0xFE0F) == 0x9408:
            # BSET - Bit set in SREG
            s = (opcode >> 4) & 0x07
            return f"bset {s}", size
            
        elif (opcode & 0xFE0F) == 0x9488:
            # BCLR - Bit clear in SREG
            s = (opcode >> 4) & 0x07
            return f"bclr {s}", size
            
        elif (opcode & 0xFE0F) == 0x9508:
            # RET - Return
            return "ret", size
            
        elif (opcode & 0xFC00) == 0xF000:
            # BRBS - Branch if status flag set
            k = (opcode >> 3) & 0x7F
            if k >= 64:
                k -= 128
            s = opcode & 0x07
            return f"brbs {s}, .{k:+d}", size
            
        elif (opcode & 0xFC00) == 0xF400:
            # BRBC - Branch if status flag cleared
            k = (opcode >> 3) & 0x7F
            if k >= 64:
                k -= 128
            s = opcode & 0x07
            return f"brbc {s}, .{k:+d}", size
            
        elif (opcode & 0xF000) == 0xC000:
            # RJMP - Relative jump
            k = opcode & 0x0FFF
            if k >= 2048:
                k -= 4096
            target = addr + (k * 2) + 2
            return f"rjmp 0x{target:04X}", size
            
        elif (opcode & 0xF000) == 0xD000:
            # RCALL - Relative call
            k = opcode & 0x0FFF
            if k >= 2048:
                k -= 4096
            target = addr + (k * 2) + 2
            return f"rcall 0x{target:04X}", size
            
        elif (opcode & 0xF800) == 0xB000:
            # IN - Load from I/O location
            Rd = (opcode >> 4) & 0x1F
            A6_val = ((opcode >> 5) & 0x30) | (opcode & 0x0F)
            return f"in r{Rd}, 0x{A6_val:02X}", size
            
        elif (opcode & 0xF800) == 0xB800:
            # OUT - Store to I/O location
            Rd = (opcode >> 4) & 0x1F
            A6_val = ((opcode >> 5) & 0x30) | (opcode & 0x0F)
            return f"out 0x{A6_val:02X}, r{Rd}", size
            
        elif (opcode & 0xFF00) == 0x0100:
            # MOVW - Copy register word
            Rd = ((opcode >> 4) & 0x0F) * 2
            Rr = (opcode & 0x0F) * 2
            return f"movw r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFF00) == 0x0200:
            # MULS - Multiply signed
            Rd = ((opcode >> 4) & 0x0F) + 16
            Rr = (opcode & 0x0F) + 16
            return f"muls r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFF88) == 0x0300:
            # MULSU - Multiply signed with unsigned
            Rd = ((opcode >> 4) & 0x07) + 16
            Rr = (opcode & 0x07) + 16
            return f"mulsu r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFF88) == 0x0308:
            # FMUL - Fractional multiply unsigned
            Rd = ((opcode >> 4) & 0x07) + 16
            Rr = (opcode & 0x07) + 16
            return f"fmul r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFF88) == 0x0380:
            # FMULS - Fractional multiply signed
            Rd = ((opcode >> 4) & 0x07) + 16
            Rr = (opcode & 0x07) + 16
            return f"fmuls r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFF88) == 0x0388:
            # FMULSU - Fractional multiply signed with unsigned
            Rd = ((opcode >> 4) & 0x07) + 16
            Rr = (opcode & 0x07) + 16
            return f"fmulsu r{Rd}, r{Rr}", size
            
        elif (opcode & 0xFE08) == 0xF800:
            # BLD - Bit load from T flag to register
            b = opcode & 0x07
            return f"bld r{Rd}, {b}", size
            
        elif (opcode & 0xFE08) == 0xFA00:
            # BST - Bit store from register to T flag
            b = opcode & 0x07
            return f"bst r{Rd}, {b}", size
            
        elif (opcode & 0xFE08) == 0xFC00:
            # SBRC - Skip if bit in register cleared
            b = opcode & 0x07
            return f"sbrc r{Rd}, {b}", size
            
        elif (opcode & 0xFE08) == 0xFE00:
            # SBRS - Skip if bit in register set
            b = opcode & 0x07
            return f"sbrs r{Rd}, {b}", size
            
        # If no match found, return raw data
        return f".word 0x{opcode:04X}", size
    
    def disassemble(self):
        """Disassemble entire binary"""
        addr = 0
        
        while addr < len(self.data):
            # Get instruction
            instr, size = self.disassemble_instruction(addr)
            
            # Format output
            hex_bytes = ' '.join(f'{self.data[addr+i]:02X}' for i in range(size))
            self.output.append(f"0x{addr:04X}:  {hex_bytes:16s}  {instr}")
            
            addr += size
            
            # Stop at end of valid code (many 0xFF)
            if addr > 0x100:
                # Check if we hit padding
                all_ff = True
                for i in range(min(64, len(self.data) - addr)):
                    if self.data[addr + i] != 0xFF:
                        all_ff = False
                        break
                if all_ff:
                    break
        
        return '\n'.join(self.output)

# Main execution
if __name__ == "__main__":
    # Read binary
    with open('firmware.bin', 'rb') as f:
        binary = f.read()
    
    print(f"Disassembling {len(binary)} bytes of firmware...")
    
    # Disassemble
    disasm = AVRDisassembler(binary)
    output = disasm.disassemble()
    
    # Save to file
    with open('firmware_disassembled.asm', 'w') as f:
        f.write("; ATmega16 Firmware Disassembly\n")
        f.write("; Generated from m16_controller_prg.hex\n\n")
        f.write(output)
    
    print(f"Disassembly complete! Output saved to firmware_disassembled.asm")
    print(f"Total instructions: {len(disasm.output)}")
