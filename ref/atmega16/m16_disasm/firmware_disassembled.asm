; ATmega16 Firmware Disassembly
; Generated from m16_controller_prg.hex

0x0000:  0C 94 45 00       jmp 0x008A
0x0004:  0C 94 00 00       jmp 0x0000
0x0008:  0C 94 00 00       jmp 0x0000
0x000C:  0C 94 00 00       jmp 0x0000
0x0010:  0C 94 00 00       jmp 0x0000
0x0014:  0C 94 00 00       jmp 0x0000
0x0018:  0C 94 00 00       jmp 0x0000
0x001C:  0C 94 00 00       jmp 0x0000
0x0020:  0C 94 FF 01       jmp 0x03FE
0x0024:  0C 94 40 01       jmp 0x0280
0x0028:  0C 94 00 00       jmp 0x0000
0x002C:  0C 94 00 00       jmp 0x0000
0x0030:  0C 94 00 00       jmp 0x0000
0x0034:  0C 94 00 00       jmp 0x0000
0x0038:  0C 94 75 00       jmp 0x00EA
0x003C:  0C 94 00 00       jmp 0x0000
0x0040:  0C 94 00 00       jmp 0x0000
0x0044:  0C 94 00 00       jmp 0x0000
0x0048:  0C 94 00 00       jmp 0x0000
0x004C:  0C 94 00 00       jmp 0x0000
0x0050:  0C 94 00 00       jmp 0x0000
0x0054:  00 00             nop
0x0056:  00 00             nop
0x0058:  FF 03             fmulsu r23, r23
0x005A:  CC 03             fmulsu r20, r20
0x005C:  99 03             fmulsu r17, r17
0x005E:  66 03             mulsu r22, r22
0x0060:  33 03             mulsu r19, r19
0x0062:  00 03             mulsu r16, r16
0x0064:  CD 02             muls r28, r29
0x0066:  9A 02             muls r25, r26
0x0068:  67 02             muls r22, r23
0x006A:  34 02             muls r19, r20
0x006C:  01 02             muls r16, r17
0x006E:  CE 01             movw r24, r28
0x0070:  B9 01             movw r22, r18
0x0072:  68 01             movw r12, r16
0x0074:  35 01             movw r6, r10
0x0076:  02 01             movw r0, r4
0x0078:  CF 00             .word 0x00CF
0x007A:  9C 00             .word 0x009C
0x007C:  69 00             .word 0x0069
0x007E:  36 00             .word 0x0036
0x0080:  0F 00             .word 0x000F
0x0082:  04 00             .word 0x0004
0x0084:  04 00             .word 0x0004
0x0086:  54 00             .word 0x0054
0x0088:  00 00             nop
0x008A:  F8 94             cli
0x008C:  EE 27             eor r30, r30
0x008E:  EC BB             out 0x1C, r30
0x0090:  F1 E0             ldi r31, 0x01
0x0092:  FB BF             out 0x3B, r31
0x0094:  EB BF             out 0x3B, r30
0x0096:  E5 BF             out 0x35, r30
0x0098:  F8 E1             ldi r31, 0x18
0x009A:  F1 BD             out 0x21, r31
0x009C:  E1 BD             out 0x21, r30
0x009E:  8D E0             ldi r24, 0x0D
0x00A0:  A2 E0             ldi r26, 0x02
0x00A2:  BB 27             eor r27, r27
0x00A4:  ED 93             st X+, r30
0x00A6:  8A 95             dec r24
0x00A8:  E9 F7             brbc 1, .-3
0x00AA:  80 E0             ldi r24, 0x00
0x00AC:  94 E0             ldi r25, 0x04
0x00AE:  A0 E6             ldi r26, 0x60
0x00B0:  ED 93             st X+, r30
0x00B2:  01 97             sbiw r24, 0x01
0x00B4:  E9 F7             brbc 1, .-3
0x00B6:  E2 E8             ldi r30, 0x82
0x00B8:  F0 E0             ldi r31, 0x00
0x00BA:  85 91             lpm r24, Z+
0x00BC:  95 91             lpm r25, Z+
0x00BE:  00 97             sbiw r24, 0x00
0x00C0:  61 F0             brbs 1, .+12
0x00C2:  A5 91             lpm r26, Z+
0x00C4:  B5 91             lpm r27, Z+
0x00C6:  05 90             lpm r0, Z+
0x00C8:  15 90             lpm r1, Z+
0x00CA:  BF 01             movw r22, r30
0x00CC:  F0 01             movw r30, r0
0x00CE:  05 90             lpm r0, Z+
0x00D0:  0D 92             st X+, r0
0x00D2:  01 97             sbiw r24, 0x01
0x00D4:  E1 F7             brbc 1, .-4
0x00D6:  FB 01             movw r30, r22
0x00D8:  F0 CF             rjmp 0x00BA
0x00DA:  EF E5             ldi r30, 0x5F
0x00DC:  ED BF             out 0x3D, r30
0x00DE:  E4 E0             ldi r30, 0x04
0x00E0:  EE BF             out 0x3E, r30
0x00E2:  C0 E6             ldi r28, 0x60
0x00E4:  D1 E0             ldi r29, 0x01
0x00E6:  0C 94 B1 0D       jmp 0x1B62
0x00EA:  0E 94 CB 0D       call 0x1B96
0x00EE:  E0 91 1E 02       lds r30, 0x021E
0x00F2:  A0 E6             ldi r26, 0x60
0x00F4:  B1 E0             ldi r27, 0x01
0x00F6:  F0 E0             ldi r31, 0x00
0x00F8:  EE 0F             add r30, r30
0x00FA:  FF 1F             adc r31, r31
0x00FC:  AE 0F             add r26, r30
0x00FE:  BF 1F             adc r27, r31
0x0100:  E4 B1             in r30, 0x04
0x0102:  F5 B1             in r31, 0x05
0x0104:  ED 93             st X+, r30
0x0106:  FC 93             st X, r31
0x0108:  0E 94 B6 0C       call 0x196C
0x010C:  A0 91 1E 02       lds r26, 0x021E
0x0110:  AF 5F             subi r26, 0xFF
0x0112:  A0 93 1E 02       sts 0x021E, r26
0x0116:  A2 30             cpi r26, 0x02
0x0118:  10 F4             brbc 0, .+2
0x011A:  0C 94 93 00       jmp 0x0126
0x011E:  E0 E0             ldi r30, 0x00
0x0120:  E0 93 1E 02       sts 0x021E, r30
0x0124:  AE 2E             mov r10, r30
0x0126:  A0 90 1E 02       lds r10, 0x021E
0x012A:  E0 91 1E 02       lds r30, 0x021E
0x012E:  E0 5C             subi r30, 0xC0
0x0130:  E7 B9             out 0x07, r30
0x0132:  E7 B1             in r30, 0x07
0x0134:  E0 6C             ori r30, 0xC0
0x0136:  E7 B9             out 0x07, r30
0x0138:  EF EC             ldi r30, 0xCF
0x013A:  E6 B9             out 0x06, r30
0x013C:  0E 94 D9 0D       call 0x1BB2
0x0140:  18 95             reti
0x0142:  A0 91 6D 01       lds r26, 0x016D
0x0146:  A1 30             cpi r26, 0x01
0x0148:  11 F4             brbc 1, .+2
0x014A:  0C 94 02 01       jmp 0x0204
0x014E:  E0 91 6E 01       lds r30, 0x016E
0x0152:  E0 30             cpi r30, 0x00
0x0154:  11 F0             brbs 1, .+2
0x0156:  0C 94 E7 00       jmp 0x01CE
0x015A:  9B 99             sbic 0x13, 3
0x015C:  04 C0             rjmp 0x0166
0x015E:  E2 E3             ldi r30, 0x32
0x0160:  E0 93 66 01       sts 0x0166, r30
0x0164:  05 C0             rjmp 0x0170
0x0166:  9A 99             sbic 0x13, 2
0x0168:  03 C0             rjmp 0x0170
0x016A:  EA E0             ldi r30, 0x0A
0x016C:  E0 93 66 01       sts 0x0166, r30
0x0170:  E0 91 66 01       lds r30, 0x0166
0x0174:  E0 30             cpi r30, 0x00
0x0176:  11 F4             brbc 1, .+2
0x0178:  0C 94 E6 00       jmp 0x01CC
0x017C:  E0 91 67 01       lds r30, 0x0167
0x0180:  A0 91 66 01       lds r26, 0x0166
0x0184:  EA 17             .word 0x17EA
0x0186:  11 F0             brbs 1, .+2
0x0188:  0C 94 DF 00       jmp 0x01BE
0x018C:  0E 94 E7 0D       call 0x1BCE
0x0190:  10 F4             brbc 0, .+2
0x0192:  0C 94 DE 00       jmp 0x01BC
0x0196:  E1 E0             ldi r30, 0x01
0x0198:  E0 93 6E 01       sts 0x016E, r30
0x019C:  E0 91 66 01       lds r30, 0x0166
0x01A0:  E0 93 68 01       sts 0x0168, r30
0x01A4:  E1 E0             ldi r30, 0x01
0x01A6:  E0 93 6D 01       sts 0x016D, r30
0x01AA:  E0 E0             ldi r30, 0x00
0x01AC:  E0 93 69 01       sts 0x0169, r30
0x01B0:  E0 93 67 01       sts 0x0167, r30
0x01B4:  E0 93 66 01       sts 0x0166, r30
0x01B8:  0E 94 7B 08       call 0x10F6
0x01BC:  07 C0             rjmp 0x01CC
0x01BE:  E0 91 66 01       lds r30, 0x0166
0x01C2:  E0 93 67 01       sts 0x0167, r30
0x01C6:  E0 E0             ldi r30, 0x00
0x01C8:  E0 93 69 01       sts 0x0169, r30
0x01CC:  1B C0             rjmp 0x0204
0x01CE:  9A 9B             sbis 0x13, 2
0x01D0:  03 C0             rjmp 0x01D8
0x01D2:  9B 9B             sbis 0x13, 3
0x01D4:  01 C0             rjmp 0x01D8
0x01D6:  01 C0             rjmp 0x01DA
0x01D8:  13 C0             rjmp 0x0200
0x01DA:  0E 94 E7 0D       call 0x1BCE
0x01DE:  10 F4             brbc 0, .+2
0x01E0:  0C 94 FF 00       jmp 0x01FE
0x01E4:  E0 E0             ldi r30, 0x00
0x01E6:  E0 93 65 01       sts 0x0165, r30
0x01EA:  E0 93 6C 01       sts 0x016C, r30
0x01EE:  E0 93 6E 01       sts 0x016E, r30
0x01F2:  E0 93 69 01       sts 0x0169, r30
0x01F6:  E0 93 9E 01       sts 0x019E, r30
0x01FA:  0E 94 F0 0D       call 0x1BE0
0x01FE:  02 C0             rjmp 0x0204
0x0200:  0E 94 03 01       call 0x0206
0x0204:  08 95             ret
0x0206:  E0 E0             ldi r30, 0x00
0x0208:  E0 93 69 01       sts 0x0169, r30
0x020C:  E0 91 6C 01       lds r30, 0x016C
0x0210:  E1 30             cpi r30, 0x01
0x0212:  11 F0             brbs 1, .+2
0x0214:  0C 94 2B 01       jmp 0x0256
0x0218:  E0 91 65 01       lds r30, 0x0165
0x021C:  E0 30             cpi r30, 0x00
0x021E:  11 F0             brbs 1, .+2
0x0220:  0C 94 2A 01       jmp 0x0254
0x0224:  AA E6             ldi r26, 0x6A
0x0226:  B1 E0             ldi r27, 0x01
0x0228:  0E 94 F6 0D       call 0x1BEC
0x022C:  A0 91 6A 01       lds r26, 0x016A
0x0230:  B0 91 6B 01       lds r27, 0x016B
0x0234:  AC 32             cpi r26, 0x2C
0x0236:  E1 E0             ldi r30, 0x01
0x0238:  BE 07             .word 0x07BE
0x023A:  10 F4             brbc 0, .+2
0x023C:  0C 94 2A 01       jmp 0x0254
0x0240:  0E 94 FC 0D       call 0x1BF8
0x0244:  11 F0             brbs 1, .+2
0x0246:  0C 94 28 01       jmp 0x0250
0x024A:  0E 94 02 0E       call 0x1C04
0x024E:  02 C0             rjmp 0x0254
0x0250:  0E 94 02 0E       call 0x1C04
0x0254:  00 C0             rjmp 0x0256
0x0256:  08 95             ret
0x0258:  E0 E0             ldi r30, 0x00
0x025A:  E0 93 6D 01       sts 0x016D, r30
0x025E:  E0 93 64 01       sts 0x0164, r30
0x0262:  08 95             ret
0x0264:  A0 91 6D 01       lds r26, 0x016D
0x0268:  A1 30             cpi r26, 0x01
0x026A:  11 F0             brbs 1, .+2
0x026C:  0C 94 3F 01       jmp 0x027E
0x0270:  E0 91 68 01       lds r30, 0x0168
0x0274:  E0 93 64 01       sts 0x0164, r30
0x0278:  E0 E0             ldi r30, 0x00
0x027A:  E0 93 6D 01       sts 0x016D, r30
0x027E:  08 95             ret
0x0280:  0E 94 CB 0D       call 0x1B96
0x0284:  E0 EF             ldi r30, 0xF0
0x0286:  E2 BF             out 0x32, r30
0x0288:  A7 EB             ldi r26, 0xB7
0x028A:  B1 E0             ldi r27, 0x01
0x028C:  0E 94 F6 0D       call 0x1BEC
0x0290:  A0 91 B7 01       lds r26, 0x01B7
0x0294:  B0 91 B8 01       lds r27, 0x01B8
0x0298:  A9 3E             cpi r26, 0xE9
0x029A:  E3 E0             ldi r30, 0x03
0x029C:  BE 07             .word 0x07BE
0x029E:  10 F4             brbc 0, .+2
0x02A0:  0C 94 5A 01       jmp 0x02B4
0x02A4:  EC E4             ldi r30, 0x4C
0x02A6:  F4 E0             ldi r31, 0x04
0x02A8:  E0 93 B7 01       sts 0x01B7, r30
0x02AC:  F0 93 B8 01       sts 0x01B8, r31
0x02B0:  C5 9A             sbi 0x18, 5
0x02B2:  13 C0             rjmp 0x02DA
0x02B4:  A0 91 B7 01       lds r26, 0x01B7
0x02B8:  B0 91 B8 01       lds r27, 0x01B8
0x02BC:  A4 3F             cpi r26, 0xF4
0x02BE:  E1 E0             ldi r30, 0x01
0x02C0:  BE 07             .word 0x07BE
0x02C2:  10 F4             brbc 0, .+2
0x02C4:  0C 94 6C 01       jmp 0x02D8
0x02C8:  E0 91 CC 01       lds r30, 0x01CC
0x02CC:  E0 30             cpi r30, 0x00
0x02CE:  11 F0             brbs 1, .+2
0x02D0:  0C 94 6C 01       jmp 0x02D8
0x02D4:  0E 94 A7 01       call 0x034E
0x02D8:  C5 98             cbi 0x18, 5
0x02DA:  A2 E8             ldi r26, 0x82
0x02DC:  B1 E0             ldi r27, 0x01
0x02DE:  0E 94 F6 0D       call 0x1BEC
0x02E2:  E0 91 89 01       lds r30, 0x0189
0x02E6:  E0 30             cpi r30, 0x00
0x02E8:  11 F4             brbc 1, .+2
0x02EA:  0C 94 79 01       jmp 0x02F2
0x02EE:  AF 98             cbi 0x15, 7
0x02F0:  01 C0             rjmp 0x02F4
0x02F2:  AF 9A             sbi 0x15, 7
0x02F4:  E0 91 8A 01       lds r30, 0x018A
0x02F8:  E0 30             cpi r30, 0x00
0x02FA:  11 F4             brbc 1, .+2
0x02FC:  0C 94 82 01       jmp 0x0304
0x0300:  C1 98             cbi 0x18, 1
0x0302:  01 C0             rjmp 0x0306
0x0304:  C1 9A             sbi 0x18, 1
0x0306:  E0 91 96 01       lds r30, 0x0196
0x030A:  E0 30             cpi r30, 0x00
0x030C:  11 F4             brbc 1, .+2
0x030E:  0C 94 8B 01       jmp 0x0316
0x0312:  C0 98             cbi 0x18, 0
0x0314:  01 C0             rjmp 0x0318
0x0316:  C0 9A             sbi 0x18, 0
0x0318:  0E 94 2E 0D       call 0x1A5C
0x031C:  0E 94 C6 09       call 0x138C
0x0320:  0E 94 49 0B       call 0x1692
0x0324:  E0 91 95 01       lds r30, 0x0195
0x0328:  E0 30             cpi r30, 0x00
0x032A:  11 F4             brbc 1, .+2
0x032C:  0C 94 9B 01       jmp 0x0336
0x0330:  0E 94 DE 0A       call 0x15BC
0x0334:  09 C0             rjmp 0x0348
0x0336:  0E 94 33 08       call 0x1066
0x033A:  0E 94 FC 0D       call 0x1BF8
0x033E:  11 F0             brbs 1, .+2
0x0340:  0C 94 A4 01       jmp 0x0348
0x0344:  0E 94 1B 06       call 0x0C36
0x0348:  0E 94 D9 0D       call 0x1BB2
0x034C:  18 95             reti
0x034E:  E0 91 CB 01       lds r30, 0x01CB
0x0352:  E0 30             cpi r30, 0x00
0x0354:  11 F0             brbs 1, .+2
0x0356:  0C 94 FE 01       jmp 0x03FC
0x035A:  E0 91 CD 01       lds r30, 0x01CD
0x035E:  E0 30             cpi r30, 0x00
0x0360:  11 F0             brbs 1, .+2
0x0362:  0C 94 C2 01       jmp 0x0384
0x0366:  0E 94 0A 0E       call 0x1C14
0x036A:  A6 30             cpi r26, 0x06
0x036C:  10 F4             brbc 0, .+2
0x036E:  0C 94 C1 01       jmp 0x0382
0x0372:  0E 94 12 0E       call 0x1C24
0x0376:  E0 93 CD 01       sts 0x01CD, r30
0x037A:  0E 94 16 0E       call 0x1C2C
0x037E:  E0 93 80 01       sts 0x0180, r30
0x0382:  3C C0             rjmp 0x03FC
0x0384:  E0 91 80 01       lds r30, 0x0180
0x0388:  E0 30             cpi r30, 0x00
0x038A:  11 F0             brbs 1, .+2
0x038C:  0C 94 D6 01       jmp 0x03AC
0x0390:  0E 94 0A 0E       call 0x1C14
0x0394:  A5 31             cpi r26, 0x15
0x0396:  10 F4             brbc 0, .+2
0x0398:  0C 94 D5 01       jmp 0x03AA
0x039C:  0E 94 16 0E       call 0x1C2C
0x03A0:  E0 93 96 01       sts 0x0196, r30
0x03A4:  E1 E0             ldi r30, 0x01
0x03A6:  E0 93 80 01       sts 0x0180, r30
0x03AA:  28 C0             rjmp 0x03FC
0x03AC:  E1 30             cpi r30, 0x01
0x03AE:  11 F0             brbs 1, .+2
0x03B0:  0C 94 E8 01       jmp 0x03D0
0x03B4:  0E 94 0A 0E       call 0x1C14
0x03B8:  A1 30             cpi r26, 0x01
0x03BA:  10 F4             brbc 0, .+2
0x03BC:  0C 94 E7 01       jmp 0x03CE
0x03C0:  0E 94 1A 0E       call 0x1C34
0x03C4:  E0 93 81 01       sts 0x0181, r30
0x03C8:  E2 E0             ldi r30, 0x02
0x03CA:  E0 93 80 01       sts 0x0180, r30
0x03CE:  16 C0             rjmp 0x03FC
0x03D0:  E2 30             cpi r30, 0x02
0x03D2:  11 F0             brbs 1, .+2
0x03D4:  0C 94 FE 01       jmp 0x03FC
0x03D8:  0E 94 1E 0E       call 0x1C3C
0x03DC:  0E 94 22 0E       call 0x1C44
0x03E0:  AB 30             cpi r26, 0x0B
0x03E2:  10 F4             brbc 0, .+2
0x03E4:  0C 94 FD 01       jmp 0x03FA
0x03E8:  0E 94 28 0E       call 0x1C50
0x03EC:  E0 93 80 01       sts 0x0180, r30
0x03F0:  E1 E0             ldi r30, 0x01
0x03F2:  E0 93 CC 01       sts 0x01CC, r30
0x03F6:  E0 93 CB 01       sts 0x01CB, r30
0x03FA:  00 C0             rjmp 0x03FC
0x03FC:  08 95             ret
0x03FE:  0E 94 CB 0D       call 0x1B96
0x0402:  EF EF             ldi r30, 0xFF
0x0404:  ED BD             out 0x2D, r30
0x0406:  E1 EB             ldi r30, 0xB1
0x0408:  EC BD             out 0x2C, r30
0x040A:  E0 91 95 01       lds r30, 0x0195
0x040E:  E0 30             cpi r30, 0x00
0x0410:  11 F4             brbc 1, .+2
0x0412:  0C 94 10 02       jmp 0x0420
0x0416:  A3 E7             ldi r26, 0x73
0x0418:  B1 E0             ldi r27, 0x01
0x041A:  0E 94 F6 0D       call 0x1BEC
0x041E:  0D C0             rjmp 0x043A
0x0420:  0E 94 69 03       call 0x06D2
0x0424:  0E 94 FC 0D       call 0x1BF8
0x0428:  11 F0             brbs 1, .+2
0x042A:  0C 94 19 02       jmp 0x0432
0x042E:  0E 94 88 04       call 0x0910
0x0432:  0E 94 A1 00       call 0x0142
0x0436:  0E 94 38 06       call 0x0C70
0x043A:  E0 91 88 01       lds r30, 0x0188
0x043E:  E0 30             cpi r30, 0x00
0x0440:  11 F0             brbs 1, .+2
0x0442:  0C 94 40 02       jmp 0x0480
0x0446:  E0 91 76 01       lds r30, 0x0176
0x044A:  E0 30             cpi r30, 0x00
0x044C:  11 F4             brbc 1, .+2
0x044E:  0C 94 3C 02       jmp 0x0478
0x0452:  0E 94 2C 0E       call 0x1C58
0x0456:  AB 30             cpi r26, 0x0B
0x0458:  10 F4             brbc 0, .+2
0x045A:  0C 94 3B 02       jmp 0x0476
0x045E:  E0 E0             ldi r30, 0x00
0x0460:  E0 93 76 01       sts 0x0176, r30
0x0464:  0E 94 1E 0E       call 0x1C3C
0x0468:  E0 93 9B 01       sts 0x019B, r30
0x046C:  E1 E0             ldi r30, 0x01
0x046E:  E0 93 88 01       sts 0x0188, r30
0x0472:  E0 93 8A 01       sts 0x018A, r30
0x0476:  03 C0             rjmp 0x047E
0x0478:  E0 E0             ldi r30, 0x00
0x047A:  E0 93 9B 01       sts 0x019B, r30
0x047E:  0F C0             rjmp 0x049E
0x0480:  0E 94 2C 0E       call 0x1C58
0x0484:  A4 30             cpi r26, 0x04
0x0486:  10 F4             brbc 0, .+2
0x0488:  0C 94 4F 02       jmp 0x049E
0x048C:  E0 E0             ldi r30, 0x00
0x048E:  E0 93 76 01       sts 0x0176, r30
0x0492:  E0 93 9B 01       sts 0x019B, r30
0x0496:  E0 93 88 01       sts 0x0188, r30
0x049A:  E0 93 8A 01       sts 0x018A, r30
0x049E:  AF EB             ldi r26, 0xBF
0x04A0:  B1 E0             ldi r27, 0x01
0x04A2:  0E 94 F6 0D       call 0x1BEC
0x04A6:  0E 94 CE 08       call 0x119C
0x04AA:  0E 94 02 09       call 0x1204
0x04AE:  0E 94 80 08       call 0x1100
0x04B2:  0E 94 98 08       call 0x1130
0x04B6:  A0 91 8D 01       lds r26, 0x018D
0x04BA:  B0 91 8E 01       lds r27, 0x018E
0x04BE:  A1 32             cpi r26, 0x21
0x04C0:  E3 E0             ldi r30, 0x03
0x04C2:  BE 07             .word 0x07BE
0x04C4:  10 F4             brbc 0, .+2
0x04C6:  0C 94 6D 02       jmp 0x04DA
0x04CA:  0E 94 34 0E       call 0x1C68
0x04CE:  E0 93 77 01       sts 0x0177, r30
0x04D2:  E0 E0             ldi r30, 0x00
0x04D4:  E0 93 7A 01       sts 0x017A, r30
0x04D8:  1D C0             rjmp 0x0514
0x04DA:  0E 94 3A 0E       call 0x1C74
0x04DE:  E4 70             andi r30, 0x04
0x04E0:  0E 94 3F 0E       call 0x1C7E
0x04E4:  11 F0             brbs 1, .+2
0x04E6:  0C 94 87 02       jmp 0x050E
0x04EA:  E0 91 7A 01       lds r30, 0x017A
0x04EE:  EF 5F             subi r30, 0xFF
0x04F0:  E0 93 7A 01       sts 0x017A, r30
0x04F4:  A0 91 7A 01       lds r26, 0x017A
0x04F8:  AA 31             cpi r26, 0x1A
0x04FA:  10 F4             brbc 0, .+2
0x04FC:  0C 94 86 02       jmp 0x050C
0x0500:  E1 E0             ldi r30, 0x01
0x0502:  E0 93 77 01       sts 0x0177, r30
0x0506:  E0 E0             ldi r30, 0x00
0x0508:  E0 93 7A 01       sts 0x017A, r30
0x050C:  03 C0             rjmp 0x0514
0x050E:  E0 E0             ldi r30, 0x00
0x0510:  E0 93 7A 01       sts 0x017A, r30
0x0514:  E0 91 77 01       lds r30, 0x0177
0x0518:  E0 30             cpi r30, 0x00
0x051A:  11 F0             brbs 1, .+2
0x051C:  0C 94 94 02       jmp 0x0528
0x0520:  E0 E0             ldi r30, 0x00
0x0522:  E0 93 86 01       sts 0x0186, r30
0x0526:  45 C0             rjmp 0x05B2
0x0528:  E0 91 86 01       lds r30, 0x0186
0x052C:  E0 30             cpi r30, 0x00
0x052E:  11 F4             brbc 1, .+2
0x0530:  0C 94 D9 02       jmp 0x05B2
0x0534:  E0 91 87 01       lds r30, 0x0187
0x0538:  E0 30             cpi r30, 0x00
0x053A:  11 F0             brbs 1, .+2
0x053C:  0C 94 AC 02       jmp 0x0558
0x0540:  E1 E0             ldi r30, 0x01
0x0542:  E0 93 87 01       sts 0x0187, r30
0x0546:  E0 91 8D 01       lds r30, 0x018D
0x054A:  F0 91 8E 01       lds r31, 0x018E
0x054E:  E0 93 8F 01       sts 0x018F, r30
0x0552:  F0 93 90 01       sts 0x0190, r31
0x0556:  2D C0             rjmp 0x05B2
0x0558:  CC 99             sbic 0x19, 4
0x055A:  19 C0             rjmp 0x058E
0x055C:  E0 E0             ldi r30, 0x00
0x055E:  E0 93 A3 01       sts 0x01A3, r30
0x0562:  E0 91 8F 01       lds r30, 0x018F
0x0566:  F0 91 90 01       lds r31, 0x0190
0x056A:  30 97             sbiw r30, 0x00
0x056C:  11 F0             brbs 1, .+2
0x056E:  0C 94 BF 02       jmp 0x057E
0x0572:  E0 E0             ldi r30, 0x00
0x0574:  E0 93 87 01       sts 0x0187, r30
0x0578:  0E 94 43 0E       call 0x1C86
0x057C:  07 C0             rjmp 0x058C
0x057E:  AF E8             ldi r26, 0x8F
0x0580:  B1 E0             ldi r27, 0x01
0x0582:  ED 91             ld r30, X+
0x0584:  FD 91             ld r31, X+
0x0586:  31 97             sbiw r30, 0x01
0x0588:  FE 93             st -X, r31
0x058A:  EE 93             st -X, r30
0x058C:  12 C0             rjmp 0x05B2
0x058E:  E0 91 A3 01       lds r30, 0x01A3
0x0592:  EF 5F             subi r30, 0xFF
0x0594:  E0 93 A3 01       sts 0x01A3, r30
0x0598:  A0 91 A3 01       lds r26, 0x01A3
0x059C:  A6 30             cpi r26, 0x06
0x059E:  10 F4             brbc 0, .+2
0x05A0:  0C 94 D9 02       jmp 0x05B2
0x05A4:  E0 E0             ldi r30, 0x00
0x05A6:  E0 93 87 01       sts 0x0187, r30
0x05AA:  E0 93 A3 01       sts 0x01A3, r30
0x05AE:  0E 94 43 0E       call 0x1C86
0x05B2:  0E 94 D9 0D       call 0x1BB2
0x05B6:  18 95             reti
0x05B8:  1A 93             st -Y, r17
0x05BA:  11 E0             ldi r17, 0x01
0x05BC:  10 30             cpi r17, 0x00
0x05BE:  11 F4             brbc 1, .+2
0x05C0:  0C 94 FE 02       jmp 0x05FC
0x05C4:  0E 94 32 01       call 0x0264
0x05C8:  E0 91 64 01       lds r30, 0x0164
0x05CC:  EA 30             cpi r30, 0x0A
0x05CE:  11 F0             brbs 1, .+2
0x05D0:  0C 94 F2 02       jmp 0x05E4
0x05D4:  0E 94 2C 01       call 0x0258
0x05D8:  E1 E0             ldi r30, 0x01
0x05DA:  E0 93 6C 01       sts 0x016C, r30
0x05DE:  0E 94 38 03       call 0x0670
0x05E2:  0B C0             rjmp 0x05FA
0x05E4:  E2 33             cpi r30, 0x32
0x05E6:  11 F0             brbs 1, .+2
0x05E8:  0C 94 FD 02       jmp 0x05FA
0x05EC:  0E 94 2C 01       call 0x0258
0x05F0:  0E 94 00 03       call 0x0600
0x05F4:  02 C0             rjmp 0x05FA
0x05F6:  0E 94 2C 01       call 0x0258
0x05FA:  E0 CF             rjmp 0x05BC
0x05FC:  19 91             ld r17, Y+
0x05FE:  08 95             ret
0x0600:  E0 91 9C 01       lds r30, 0x019C
0x0604:  E0 30             cpi r30, 0x00
0x0606:  11 F0             brbs 1, .+2
0x0608:  0C 94 0D 03       jmp 0x061A
0x060C:  E0 91 C5 01       lds r30, 0x01C5
0x0610:  E0 30             cpi r30, 0x00
0x0612:  11 F0             brbs 1, .+2
0x0614:  0C 94 0D 03       jmp 0x061A
0x0618:  01 C0             rjmp 0x061C
0x061A:  29 C0             rjmp 0x066E
0x061C:  22 97             sbiw r28, 0x02
0x061E:  E0 E0             ldi r30, 0x00
0x0620:  E8 83             st Y, r30
0x0622:  E9 83             std Y+1, r30
0x0624:  E1 E0             ldi r30, 0x01
0x0626:  E0 93 9E 01       sts 0x019E, r30
0x062A:  E0 93 89 01       sts 0x0189, r30
0x062E:  E1 E0             ldi r30, 0x01
0x0630:  F0 E0             ldi r31, 0x00
0x0632:  E8 83             st Y, r30
0x0634:  F9 83             std Y+1, r31
0x0636:  E8 81             ld r30, Y
0x0638:  F9 81             ldd r31, Y+1
0x063A:  30 97             sbiw r30, 0x00
0x063C:  11 F4             brbc 1, .+2
0x063E:  0C 94 36 03       jmp 0x066C
0x0642:  E0 91 9E 01       lds r30, 0x019E
0x0646:  E0 30             cpi r30, 0x00
0x0648:  11 F4             brbc 1, .+2
0x064A:  0C 94 2E 03       jmp 0x065C
0x064E:  E0 91 9C 01       lds r30, 0x019C
0x0652:  E0 30             cpi r30, 0x00
0x0654:  11 F0             brbs 1, .+2
0x0656:  0C 94 2E 03       jmp 0x065C
0x065A:  07 C0             rjmp 0x066A
0x065C:  0E 94 1A 0E       call 0x1C34
0x0660:  E0 93 89 01       sts 0x0189, r30
0x0664:  E0 E0             ldi r30, 0x00
0x0666:  E8 83             st Y, r30
0x0668:  E9 83             std Y+1, r30
0x066A:  E5 CF             rjmp 0x0636
0x066C:  22 96             adiw r28, 0x02
0x066E:  08 95             ret
0x0670:  E0 91 9C 01       lds r30, 0x019C
0x0674:  E0 30             cpi r30, 0x00
0x0676:  11 F4             brbc 1, .+2
0x0678:  0C 94 54 03       jmp 0x06A8
0x067C:  0E 94 49 0E       call 0x1C92
0x0680:  AE 98             cbi 0x15, 6
0x0682:  0E 94 50 0E       call 0x1CA0
0x0686:  A8 98             cbi 0x15, 0
0x0688:  A9 98             cbi 0x15, 1
0x068A:  0E 94 60 0E       call 0x1CC0
0x068E:  E0 93 9B 01       sts 0x019B, r30
0x0692:  E1 E0             ldi r30, 0x01
0x0694:  E0 93 76 01       sts 0x0176, r30
0x0698:  E0 E0             ldi r30, 0x00
0x069A:  E0 93 A3 01       sts 0x01A3, r30
0x069E:  0E 94 34 0E       call 0x1C68
0x06A2:  E0 93 88 01       sts 0x0188, r30
0x06A6:  14 C0             rjmp 0x06D0
0x06A8:  0E 94 49 0E       call 0x1C92
0x06AC:  A8 98             cbi 0x15, 0
0x06AE:  0E 94 50 0E       call 0x1CA0
0x06B2:  A9 98             cbi 0x15, 1
0x06B4:  0E 94 60 0E       call 0x1CC0
0x06B8:  E0 93 A3 01       sts 0x01A3, r30
0x06BC:  E0 E0             ldi r30, 0x00
0x06BE:  E0 93 9B 01       sts 0x019B, r30
0x06C2:  E1 E0             ldi r30, 0x01
0x06C4:  E0 93 76 01       sts 0x0176, r30
0x06C8:  0E 94 34 0E       call 0x1C68
0x06CC:  E0 93 88 01       sts 0x0188, r30
0x06D0:  08 95             ret
0x06D2:  E0 91 7C 01       lds r30, 0x017C
0x06D6:  E0 30             cpi r30, 0x00
0x06D8:  11 F4             brbc 1, .+2
0x06DA:  0C 94 72 03       jmp 0x06E4
0x06DE:  0E 94 79 03       call 0x06F2
0x06E2:  02 C0             rjmp 0x06E8
0x06E4:  0E 94 D0 07       call 0x0FA0
0x06E8:  0E 94 32 04       call 0x0864
0x06EC:  0E 94 53 08       call 0x10A6
0x06F0:  08 95             ret
0x06F2:  E0 91 7E 01       lds r30, 0x017E
0x06F6:  E0 30             cpi r30, 0x00
0x06F8:  11 F0             brbs 1, .+2
0x06FA:  0C 94 DA 03       jmp 0x07B4
0x06FE:  CE 99             sbic 0x19, 6
0x0700:  55 C0             rjmp 0x07AC
0x0702:  0E 94 0A 0E       call 0x1C14
0x0706:  A6 30             cpi r26, 0x06
0x0708:  10 F4             brbc 0, .+2
0x070A:  0C 94 D5 03       jmp 0x07AA
0x070E:  E0 91 9C 01       lds r30, 0x019C
0x0712:  E0 30             cpi r30, 0x00
0x0714:  11 F0             brbs 1, .+2
0x0716:  0C 94 94 03       jmp 0x0728
0x071A:  E0 91 C5 01       lds r30, 0x01C5
0x071E:  E0 30             cpi r30, 0x00
0x0720:  11 F0             brbs 1, .+2
0x0722:  0C 94 94 03       jmp 0x0728
0x0726:  10 C0             rjmp 0x0748
0x0728:  0E 94 FC 0D       call 0x1BF8
0x072C:  11 F0             brbs 1, .+2
0x072E:  0C 94 9D 03       jmp 0x073A
0x0732:  0E 94 64 0E       call 0x1CC8
0x0736:  0E 94 6D 0E       call 0x1CDA
0x073A:  0E 94 12 0E       call 0x1C24
0x073E:  0E 94 77 0E       call 0x1CEE
0x0742:  0E 94 7A 0E       call 0x1CF4
0x0746:  31 C0             rjmp 0x07AA
0x0748:  0E 94 FC 0D       call 0x1BF8
0x074C:  11 F0             brbs 1, .+2
0x074E:  0C 94 CF 03       jmp 0x079E
0x0752:  E0 91 C6 01       lds r30, 0x01C6
0x0756:  E0 30             cpi r30, 0x00
0x0758:  11 F4             brbc 1, .+2
0x075A:  0C 94 BC 03       jmp 0x0778
0x075E:  E0 91 C7 01       lds r30, 0x01C7
0x0762:  E0 30             cpi r30, 0x00
0x0764:  11 F4             brbc 1, .+2
0x0766:  0C 94 BC 03       jmp 0x0778
0x076A:  E0 91 C8 01       lds r30, 0x01C8
0x076E:  E0 30             cpi r30, 0x00
0x0770:  11 F4             brbc 1, .+2
0x0772:  0C 94 BC 03       jmp 0x0778
0x0776:  01 C0             rjmp 0x077A
0x0778:  09 C0             rjmp 0x078C
0x077A:  0E 94 64 0E       call 0x1CC8
0x077E:  0E 94 12 0E       call 0x1C24
0x0782:  0E 94 77 0E       call 0x1CEE
0x0786:  0E 94 7A 0E       call 0x1CF4
0x078A:  08 C0             rjmp 0x079C
0x078C:  0E 94 64 0E       call 0x1CC8
0x0790:  0E 94 12 0E       call 0x1C24
0x0794:  0E 94 77 0E       call 0x1CEE
0x0798:  0E 94 7A 0E       call 0x1CF4
0x079C:  06 C0             rjmp 0x07AA
0x079E:  0E 94 12 0E       call 0x1C24
0x07A2:  0E 94 77 0E       call 0x1CEE
0x07A6:  0E 94 7A 0E       call 0x1CF4
0x07AA:  03 C0             rjmp 0x07B2
0x07AC:  E0 E0             ldi r30, 0x00
0x07AE:  E0 93 81 01       sts 0x0181, r30
0x07B2:  57 C0             rjmp 0x0862
0x07B4:  E0 91 7F 01       lds r30, 0x017F
0x07B8:  E0 30             cpi r30, 0x00
0x07BA:  11 F0             brbs 1, .+2
0x07BC:  0C 94 1A 04       jmp 0x0834
0x07C0:  E0 91 80 01       lds r30, 0x0180
0x07C4:  E0 30             cpi r30, 0x00
0x07C6:  11 F0             brbs 1, .+2
0x07C8:  0C 94 F4 03       jmp 0x07E8
0x07CC:  0E 94 0A 0E       call 0x1C14
0x07D0:  A5 31             cpi r26, 0x15
0x07D2:  10 F4             brbc 0, .+2
0x07D4:  0C 94 F3 03       jmp 0x07E6
0x07D8:  0E 94 16 0E       call 0x1C2C
0x07DC:  E0 93 96 01       sts 0x0196, r30
0x07E0:  E1 E0             ldi r30, 0x01
0x07E2:  E0 93 80 01       sts 0x0180, r30
0x07E6:  25 C0             rjmp 0x0832
0x07E8:  E1 30             cpi r30, 0x01
0x07EA:  11 F0             brbs 1, .+2
0x07EC:  0C 94 06 04       jmp 0x080C
0x07F0:  0E 94 0A 0E       call 0x1C14
0x07F4:  A1 30             cpi r26, 0x01
0x07F6:  10 F4             brbc 0, .+2
0x07F8:  0C 94 05 04       jmp 0x080A
0x07FC:  0E 94 1A 0E       call 0x1C34
0x0800:  E0 93 81 01       sts 0x0181, r30
0x0804:  E2 E0             ldi r30, 0x02
0x0806:  E0 93 80 01       sts 0x0180, r30
0x080A:  13 C0             rjmp 0x0832
0x080C:  E2 30             cpi r30, 0x02
0x080E:  11 F0             brbs 1, .+2
0x0810:  0C 94 19 04       jmp 0x0832
0x0814:  0E 94 1E 0E       call 0x1C3C
0x0818:  0E 94 22 0E       call 0x1C44
0x081C:  AB 30             cpi r26, 0x0B
0x081E:  10 F4             brbc 0, .+2
0x0820:  0C 94 19 04       jmp 0x0832
0x0824:  0E 94 28 0E       call 0x1C50
0x0828:  E0 93 80 01       sts 0x0180, r30
0x082C:  E1 E0             ldi r30, 0x01
0x082E:  E0 93 7F 01       sts 0x017F, r30
0x0832:  17 C0             rjmp 0x0862
0x0834:  CE 9B             sbis 0x19, 6
0x0836:  11 C0             rjmp 0x085A
0x0838:  E0 E0             ldi r30, 0x00
0x083A:  E0 93 8A 01       sts 0x018A, r30
0x083E:  0E 94 0A 0E       call 0x1C14
0x0842:  AB 30             cpi r26, 0x0B
0x0844:  10 F4             brbc 0, .+2
0x0846:  0C 94 2C 04       jmp 0x0858
0x084A:  0E 94 28 0E       call 0x1C50
0x084E:  E0 93 7F 01       sts 0x017F, r30
0x0852:  E0 E0             ldi r30, 0x00
0x0854:  E0 93 7E 01       sts 0x017E, r30
0x0858:  04 C0             rjmp 0x0862
0x085A:  0E 94 1A 0E       call 0x1C34
0x085E:  E0 93 81 01       sts 0x0181, r30
0x0862:  08 95             ret
0x0864:  E0 91 93 01       lds r30, 0x0193
0x0868:  E0 30             cpi r30, 0x00
0x086A:  11 F0             brbs 1, .+2
0x086C:  0C 94 45 04       jmp 0x088A
0x0870:  E0 91 9C 01       lds r30, 0x019C
0x0874:  E0 30             cpi r30, 0x00
0x0876:  11 F0             brbs 1, .+2
0x0878:  0C 94 45 04       jmp 0x088A
0x087C:  E0 91 C5 01       lds r30, 0x01C5
0x0880:  E0 30             cpi r30, 0x00
0x0882:  11 F0             brbs 1, .+2
0x0884:  0C 94 45 04       jmp 0x088A
0x0888:  01 C0             rjmp 0x088C
0x088A:  2F C0             rjmp 0x08EA
0x088C:  CD 99             sbic 0x19, 5
0x088E:  29 C0             rjmp 0x08E2
0x0890:  0E 94 8B 0E       call 0x1D16
0x0894:  10 F4             brbc 0, .+2
0x0896:  0C 94 70 04       jmp 0x08E0
0x089A:  0E 94 FC 0D       call 0x1BF8
0x089E:  11 F0             brbs 1, .+2
0x08A0:  0C 94 6E 04       jmp 0x08DC
0x08A4:  E0 91 C6 01       lds r30, 0x01C6
0x08A8:  E0 30             cpi r30, 0x00
0x08AA:  11 F4             brbc 1, .+2
0x08AC:  0C 94 65 04       jmp 0x08CA
0x08B0:  E0 91 C7 01       lds r30, 0x01C7
0x08B4:  E0 30             cpi r30, 0x00
0x08B6:  11 F4             brbc 1, .+2
0x08B8:  0C 94 65 04       jmp 0x08CA
0x08BC:  E0 91 C8 01       lds r30, 0x01C8
0x08C0:  E0 30             cpi r30, 0x00
0x08C2:  11 F4             brbc 1, .+2
0x08C4:  0C 94 65 04       jmp 0x08CA
0x08C8:  01 C0             rjmp 0x08CC
0x08CA:  05 C0             rjmp 0x08D6
0x08CC:  A8 9A             sbi 0x15, 0
0x08CE:  E1 E0             ldi r30, 0x01
0x08D0:  E0 93 C5 01       sts 0x01C5, r30
0x08D4:  02 C0             rjmp 0x08DA
0x08D6:  0E 94 94 0E       call 0x1D28
0x08DA:  02 C0             rjmp 0x08E0
0x08DC:  0E 94 94 0E       call 0x1D28
0x08E0:  03 C0             rjmp 0x08E8
0x08E2:  E0 E0             ldi r30, 0x00
0x08E4:  E0 93 94 01       sts 0x0194, r30
0x08E8:  12 C0             rjmp 0x090E
0x08EA:  CD 9B             sbis 0x19, 5
0x08EC:  0D C0             rjmp 0x0908
0x08EE:  0E 94 8B 0E       call 0x1D16
0x08F2:  10 F4             brbc 0, .+2
0x08F4:  0C 94 83 04       jmp 0x0906
0x08F8:  0E 94 1A 0E       call 0x1C34
0x08FC:  E0 93 94 01       sts 0x0194, r30
0x0900:  E0 E0             ldi r30, 0x00
0x0902:  E0 93 93 01       sts 0x0193, r30
0x0906:  03 C0             rjmp 0x090E
0x0908:  E0 E0             ldi r30, 0x00
0x090A:  E0 93 94 01       sts 0x0194, r30
0x090E:  08 95             ret
0x0910:  E0 91 97 01       lds r30, 0x0197
0x0914:  E0 30             cpi r30, 0x00
0x0916:  11 F0             brbs 1, .+2
0x0918:  0C 94 A4 04       jmp 0x0948
0x091C:  E0 91 C6 01       lds r30, 0x01C6
0x0920:  E0 30             cpi r30, 0x00
0x0922:  11 F0             brbs 1, .+2
0x0924:  0C 94 A1 04       jmp 0x0942
0x0928:  E0 91 C7 01       lds r30, 0x01C7
0x092C:  E0 30             cpi r30, 0x00
0x092E:  11 F4             brbc 1, .+2
0x0930:  0C 94 A1 04       jmp 0x0942
0x0934:  E0 91 C8 01       lds r30, 0x01C8
0x0938:  E0 30             cpi r30, 0x00
0x093A:  11 F4             brbc 1, .+2
0x093C:  0C 94 A1 04       jmp 0x0942
0x0940:  01 C0             rjmp 0x0944
0x0942:  02 C0             rjmp 0x0948
0x0944:  0E 94 A0 0E       call 0x1D40
0x0948:  08 95             ret
0x094A:  0E 94 A5 0E       call 0x1D4A
0x094E:  0E 94 AE 0E       call 0x1D5C
0x0952:  E0 91 CF 01       lds r30, 0x01CF
0x0956:  EF 5F             subi r30, 0xFF
0x0958:  E0 93 CF 01       sts 0x01CF, r30
0x095C:  A0 91 CF 01       lds r26, 0x01CF
0x0960:  A1 30             cpi r26, 0x01
0x0962:  10 F4             brbc 0, .+2
0x0964:  0C 94 16 06       jmp 0x0C2C
0x0968:  E0 E0             ldi r30, 0x00
0x096A:  E0 93 CF 01       sts 0x01CF, r30
0x096E:  0E 94 A5 0E       call 0x1D4A
0x0972:  0E 94 A7 10       call 0x214E
0x0976:  0E 94 BF 0E       call 0x1D7E
0x097A:  0E 94 D0 0E       call 0x1DA0
0x097E:  10 F4             brbc 0, .+2
0x0980:  0C 94 79 05       jmp 0x0AF2
0x0984:  0E 94 D8 0E       call 0x1DB0
0x0988:  0E 94 E1 0E       call 0x1DC2
0x098C:  0E 94 B2 10       call 0x2164
0x0990:  0E 94 BF 0E       call 0x1D7E
0x0994:  0E 94 E6 0E       call 0x1DCC
0x0998:  E0 93 EA 01       sts 0x01EA, r30
0x099C:  F0 93 EB 01       sts 0x01EB, r31
0x09A0:  60 93 EC 01       sts 0x01EC, r22
0x09A4:  70 93 ED 01       sts 0x01ED, r23
0x09A8:  E0 91 EE 01       lds r30, 0x01EE
0x09AC:  F0 91 EF 01       lds r31, 0x01EF
0x09B0:  60 91 F0 01       lds r22, 0x01F0
0x09B4:  70 91 F1 01       lds r23, 0x01F1
0x09B8:  A0 91 EA 01       lds r26, 0x01EA
0x09BC:  B0 91 EB 01       lds r27, 0x01EB
0x09C0:  80 91 EC 01       lds r24, 0x01EC
0x09C4:  90 91 ED 01       lds r25, 0x01ED
0x09C8:  0E 94 EB 0E       call 0x1DD6
0x09CC:  0E 94 D0 0E       call 0x1DA0
0x09D0:  10 F4             brbc 0, .+2
0x09D2:  0C 94 78 05       jmp 0x0AF0
0x09D6:  0E 94 FE 0E       call 0x1DFC
0x09DA:  0E 94 07 0F       call 0x1E0E
0x09DE:  E0 93 F2 01       sts 0x01F2, r30
0x09E2:  F0 93 F3 01       sts 0x01F3, r31
0x09E6:  60 93 F4 01       sts 0x01F4, r22
0x09EA:  70 93 F5 01       sts 0x01F5, r23
0x09EE:  0E 94 FE 0E       call 0x1DFC
0x09F2:  0E 94 E6 0E       call 0x1DCC
0x09F6:  E0 93 F6 01       sts 0x01F6, r30
0x09FA:  F0 93 F7 01       sts 0x01F7, r31
0x09FE:  60 93 F8 01       sts 0x01F8, r22
0x0A02:  70 93 F9 01       sts 0x01F9, r23
0x0A06:  E0 91 F2 01       lds r30, 0x01F2
0x0A0A:  F0 91 F3 01       lds r31, 0x01F3
0x0A0E:  60 91 F4 01       lds r22, 0x01F4
0x0A12:  70 91 F5 01       lds r23, 0x01F5
0x0A16:  0E 94 0C 0F       call 0x1E18
0x0A1A:  0E 94 19 0F       call 0x1E32
0x0A1E:  A0 91 F6 01       lds r26, 0x01F6
0x0A22:  B0 91 F7 01       lds r27, 0x01F7
0x0A26:  80 91 F8 01       lds r24, 0x01F8
0x0A2A:  90 91 F9 01       lds r25, 0x01F9
0x0A2E:  0E 94 A2 10       call 0x2144
0x0A32:  E0 93 FA 01       sts 0x01FA, r30
0x0A36:  F0 93 FB 01       sts 0x01FB, r31
0x0A3A:  60 93 FC 01       sts 0x01FC, r22
0x0A3E:  70 93 FD 01       sts 0x01FD, r23
0x0A42:  0E 94 22 0F       call 0x1E44
0x0A46:  0E 94 2C 0F       call 0x1E58
0x0A4A:  0E 94 D0 0E       call 0x1DA0
0x0A4E:  10 F4             brbc 0, .+2
0x0A50:  0C 94 78 05       jmp 0x0AF0
0x0A54:  0E 94 2C 0F       call 0x1E58
0x0A58:  0E 94 07 0F       call 0x1E0E
0x0A5C:  E0 93 FE 01       sts 0x01FE, r30
0x0A60:  F0 93 FF 01       sts 0x01FF, r31
0x0A64:  60 93 00 02       sts 0x0200, r22
0x0A68:  70 93 01 02       sts 0x0201, r23
0x0A6C:  0E 94 2C 0F       call 0x1E58
0x0A70:  0E 94 E6 0E       call 0x1DCC
0x0A74:  E0 93 12 02       sts 0x0212, r30
0x0A78:  F0 93 13 02       sts 0x0213, r31
0x0A7C:  60 93 14 02       sts 0x0214, r22
0x0A80:  70 93 15 02       sts 0x0215, r23
0x0A84:  0E 94 35 0F       call 0x1E6A
0x0A88:  A0 91 12 02       lds r26, 0x0212
0x0A8C:  B0 91 13 02       lds r27, 0x0213
0x0A90:  80 91 14 02       lds r24, 0x0214
0x0A94:  90 91 15 02       lds r25, 0x0215
0x0A98:  0E 94 A2 10       call 0x2144
0x0A9C:  0E 94 3E 0F       call 0x1E7C
0x0AA0:  E0 91 FE 01       lds r30, 0x01FE
0x0AA4:  F0 91 FF 01       lds r31, 0x01FF
0x0AA8:  60 91 00 02       lds r22, 0x0200
0x0AAC:  70 91 01 02       lds r23, 0x0201
0x0AB0:  0E 94 0C 0F       call 0x1E18
0x0AB4:  0E 94 47 0F       call 0x1E8E
0x0AB8:  0E 94 51 0F       call 0x1EA2
0x0ABC:  0E 94 D0 0E       call 0x1DA0
0x0AC0:  10 F4             brbc 0, .+2
0x0AC2:  0C 94 78 05       jmp 0x0AF0
0x0AC6:  0E 94 51 0F       call 0x1EA2
0x0ACA:  0E 94 07 0F       call 0x1E0E
0x0ACE:  0E 94 3E 0F       call 0x1E7C
0x0AD2:  0E 94 35 0F       call 0x1E6A
0x0AD6:  0E 94 0C 0F       call 0x1E18
0x0ADA:  0E 94 5A 0F       call 0x1EB4
0x0ADE:  E0 E0             ldi r30, 0x00
0x0AE0:  E0 93 16 02       sts 0x0216, r30
0x0AE4:  E0 93 17 02       sts 0x0217, r30
0x0AE8:  E0 93 18 02       sts 0x0218, r30
0x0AEC:  E0 93 19 02       sts 0x0219, r30
0x0AF0:  7C C0             rjmp 0x0BEA
0x0AF2:  E0 91 E6 01       lds r30, 0x01E6
0x0AF6:  F0 91 E7 01       lds r31, 0x01E7
0x0AFA:  60 91 E8 01       lds r22, 0x01E8
0x0AFE:  70 91 E9 01       lds r23, 0x01E9
0x0B02:  0E 94 D8 0E       call 0x1DB0
0x0B06:  0E 94 A2 10       call 0x2144
0x0B0A:  E0 93 E6 01       sts 0x01E6, r30
0x0B0E:  F0 93 E7 01       sts 0x01E7, r31
0x0B12:  60 93 E8 01       sts 0x01E8, r22
0x0B16:  70 93 E9 01       sts 0x01E9, r23
0x0B1A:  0E 94 64 0F       call 0x1EC8
0x0B1E:  0E 94 D0 0E       call 0x1DA0
0x0B22:  10 F4             brbc 0, .+2
0x0B24:  0C 94 F3 05       jmp 0x0BE6
0x0B28:  0E 94 64 0F       call 0x1EC8
0x0B2C:  0E 94 07 0F       call 0x1E0E
0x0B30:  E0 93 DA 01       sts 0x01DA, r30
0x0B34:  F0 93 DB 01       sts 0x01DB, r31
0x0B38:  60 93 DC 01       sts 0x01DC, r22
0x0B3C:  70 93 DD 01       sts 0x01DD, r23
0x0B40:  0E 94 64 0F       call 0x1EC8
0x0B44:  0E 94 E6 0E       call 0x1DCC
0x0B48:  E0 93 06 02       sts 0x0206, r30
0x0B4C:  F0 93 07 02       sts 0x0207, r31
0x0B50:  60 93 08 02       sts 0x0208, r22
0x0B54:  70 93 09 02       sts 0x0209, r23
0x0B58:  0E 94 6D 0F       call 0x1EDA
0x0B5C:  A0 91 06 02       lds r26, 0x0206
0x0B60:  B0 91 07 02       lds r27, 0x0207
0x0B64:  80 91 08 02       lds r24, 0x0208
0x0B68:  90 91 09 02       lds r25, 0x0209
0x0B6C:  0E 94 A2 10       call 0x2144
0x0B70:  0E 94 76 0F       call 0x1EEC
0x0B74:  0E 94 7F 0F       call 0x1EFE
0x0B78:  0E 94 89 0F       call 0x1F12
0x0B7C:  0E 94 D0 0E       call 0x1DA0
0x0B80:  10 F4             brbc 0, .+2
0x0B82:  0C 94 F2 05       jmp 0x0BE4
0x0B86:  0E 94 89 0F       call 0x1F12
0x0B8A:  0E 94 07 0F       call 0x1E0E
0x0B8E:  0E 94 76 0F       call 0x1EEC
0x0B92:  0E 94 89 0F       call 0x1F12
0x0B96:  0E 94 E6 0E       call 0x1DCC
0x0B9A:  E0 93 0A 02       sts 0x020A, r30
0x0B9E:  F0 93 0B 02       sts 0x020B, r31
0x0BA2:  60 93 0C 02       sts 0x020C, r22
0x0BA6:  70 93 0D 02       sts 0x020D, r23
0x0BAA:  0E 94 6D 0F       call 0x1EDA
0x0BAE:  0E 94 0C 0F       call 0x1E18
0x0BB2:  0E 94 92 0F       call 0x1F24
0x0BB6:  0E 94 9C 0F       call 0x1F38
0x0BBA:  0E 94 D0 0E       call 0x1DA0
0x0BBE:  10 F4             brbc 0, .+2
0x0BC0:  0C 94 F2 05       jmp 0x0BE4
0x0BC4:  0E 94 9C 0F       call 0x1F38
0x0BC8:  0E 94 07 0F       call 0x1E0E
0x0BCC:  E0 93 0E 02       sts 0x020E, r30
0x0BD0:  F0 93 0F 02       sts 0x020F, r31
0x0BD4:  60 93 10 02       sts 0x0210, r22
0x0BD8:  70 93 11 02       sts 0x0211, r23
0x0BDC:  0E 94 0C 0F       call 0x1E18
0x0BE0:  0E 94 A5 0F       call 0x1F4A
0x0BE4:  02 C0             rjmp 0x0BEA
0x0BE6:  0E 94 AF 0F       call 0x1F5E
0x0BEA:  0E 94 A5 0E       call 0x1D4A
0x0BEE:  E0 93 DE 01       sts 0x01DE, r30
0x0BF2:  F0 93 DF 01       sts 0x01DF, r31
0x0BF6:  60 93 E0 01       sts 0x01E0, r22
0x0BFA:  70 93 E1 01       sts 0x01E1, r23
0x0BFE:  E0 91 DE 01       lds r30, 0x01DE
0x0C02:  F0 91 DF 01       lds r31, 0x01DF
0x0C06:  A0 91 D4 01       lds r26, 0x01D4
0x0C0A:  B0 91 D5 01       lds r27, 0x01D5
0x0C0E:  EA 0F             add r30, r26
0x0C10:  FB 1F             adc r31, r27
0x0C12:  E0 93 D4 01       sts 0x01D4, r30
0x0C16:  F0 93 D5 01       sts 0x01D5, r31
0x0C1A:  0E 94 AF 0F       call 0x1F5E
0x0C1E:  E0 91 D4 01       lds r30, 0x01D4
0x0C22:  F0 91 D5 01       lds r31, 0x01D5
0x0C26:  0E 94 B9 0F       call 0x1F72
0x0C2A:  04 C0             rjmp 0x0C34
0x0C2C:  0E 94 A5 0E       call 0x1D4A
0x0C30:  0E 94 AE 0E       call 0x1D5C
0x0C34:  08 95             ret
0x0C36:  E0 91 CE 01       lds r30, 0x01CE
0x0C3A:  E0 30             cpi r30, 0x00
0x0C3C:  11 F4             brbc 1, .+2
0x0C3E:  0C 94 34 06       jmp 0x0C68
0x0C42:  E0 91 9C 01       lds r30, 0x019C
0x0C46:  E0 30             cpi r30, 0x00
0x0C48:  11 F0             brbs 1, .+2
0x0C4A:  0C 94 34 06       jmp 0x0C68
0x0C4E:  E0 91 9E 01       lds r30, 0x019E
0x0C52:  E0 30             cpi r30, 0x00
0x0C54:  11 F0             brbs 1, .+2
0x0C56:  0C 94 34 06       jmp 0x0C68
0x0C5A:  E0 91 C5 01       lds r30, 0x01C5
0x0C5E:  E0 30             cpi r30, 0x00
0x0C60:  11 F0             brbs 1, .+2
0x0C62:  0C 94 34 06       jmp 0x0C68
0x0C66:  01 C0             rjmp 0x0C6A
0x0C68:  02 C0             rjmp 0x0C6E
0x0C6A:  0E 94 A5 04       call 0x094A
0x0C6E:  08 95             ret
0x0C70:  E0 91 77 01       lds r30, 0x0177
0x0C74:  E0 30             cpi r30, 0x00
0x0C76:  11 F4             brbc 1, .+2
0x0C78:  0C 94 91 06       jmp 0x0D22
0x0C7C:  E0 91 97 01       lds r30, 0x0197
0x0C80:  E0 30             cpi r30, 0x00
0x0C82:  11 F0             brbs 1, .+2
0x0C84:  0C 94 51 06       jmp 0x0CA2
0x0C88:  E0 91 9C 01       lds r30, 0x019C
0x0C8C:  E0 30             cpi r30, 0x00
0x0C8E:  11 F0             brbs 1, .+2
0x0C90:  0C 94 51 06       jmp 0x0CA2
0x0C94:  E0 91 9E 01       lds r30, 0x019E
0x0C98:  E0 30             cpi r30, 0x00
0x0C9A:  11 F0             brbs 1, .+2
0x0C9C:  0C 94 51 06       jmp 0x0CA2
0x0CA0:  01 C0             rjmp 0x0CA4
0x0CA2:  1A C0             rjmp 0x0CD8
0x0CA4:  CC 99             sbic 0x19, 4
0x0CA6:  12 C0             rjmp 0x0CCC
0x0CA8:  0E 94 BE 0F       call 0x1F7C
0x0CAC:  0E 94 C1 0F       call 0x1F82
0x0CB0:  10 F4             brbc 0, .+2
0x0CB2:  0C 94 65 06       jmp 0x0CCA
0x0CB6:  E1 E0             ldi r30, 0x01
0x0CB8:  E0 93 89 01       sts 0x0189, r30
0x0CBC:  E0 93 86 01       sts 0x0186, r30
0x0CC0:  0E 94 C7 0F       call 0x1F8E
0x0CC4:  E1 E0             ldi r30, 0x01
0x0CC6:  E0 93 97 01       sts 0x0197, r30
0x0CCA:  05 C0             rjmp 0x0CD6
0x0CCC:  0E 94 C7 0F       call 0x1F8E
0x0CD0:  E0 E0             ldi r30, 0x00
0x0CD2:  E0 93 89 01       sts 0x0189, r30
0x0CD6:  24 C0             rjmp 0x0D20
0x0CD8:  E0 91 97 01       lds r30, 0x0197
0x0CDC:  E0 30             cpi r30, 0x00
0x0CDE:  11 F4             brbc 1, .+2
0x0CE0:  0C 94 79 06       jmp 0x0CF2
0x0CE4:  E0 91 9E 01       lds r30, 0x019E
0x0CE8:  E0 30             cpi r30, 0x00
0x0CEA:  11 F0             brbs 1, .+2
0x0CEC:  0C 94 79 06       jmp 0x0CF2
0x0CF0:  01 C0             rjmp 0x0CF4
0x0CF2:  16 C0             rjmp 0x0D20
0x0CF4:  E0 91 86 01       lds r30, 0x0186
0x0CF8:  E0 30             cpi r30, 0x00
0x0CFA:  11 F0             brbs 1, .+2
0x0CFC:  0C 94 90 06       jmp 0x0D20
0x0D00:  CC 9B             sbis 0x19, 4
0x0D02:  0C C0             rjmp 0x0D1C
0x0D04:  0E 94 BE 0F       call 0x1F7C
0x0D08:  0E 94 C1 0F       call 0x1F82
0x0D0C:  10 F4             brbc 0, .+2
0x0D0E:  0C 94 8D 06       jmp 0x0D1A
0x0D12:  0E 94 60 0E       call 0x1CC0
0x0D16:  0E 94 CD 0F       call 0x1F9A
0x0D1A:  02 C0             rjmp 0x0D20
0x0D1C:  0E 94 C7 0F       call 0x1F8E
0x0D20:  3E C1             rjmp 0x0F9E
0x0D22:  E0 91 97 01       lds r30, 0x0197
0x0D26:  E0 30             cpi r30, 0x00
0x0D28:  11 F0             brbs 1, .+2
0x0D2A:  0C 94 AA 06       jmp 0x0D54
0x0D2E:  E0 91 9C 01       lds r30, 0x019C
0x0D32:  E0 30             cpi r30, 0x00
0x0D34:  11 F0             brbs 1, .+2
0x0D36:  0C 94 AA 06       jmp 0x0D54
0x0D3A:  E0 91 9E 01       lds r30, 0x019E
0x0D3E:  E0 30             cpi r30, 0x00
0x0D40:  11 F0             brbs 1, .+2
0x0D42:  0C 94 AA 06       jmp 0x0D54
0x0D46:  E0 91 C5 01       lds r30, 0x01C5
0x0D4A:  E0 30             cpi r30, 0x00
0x0D4C:  11 F0             brbs 1, .+2
0x0D4E:  0C 94 AA 06       jmp 0x0D54
0x0D52:  01 C0             rjmp 0x0D56
0x0D54:  AB C0             rjmp 0x0EAC
0x0D56:  CC 99             sbic 0x19, 4
0x0D58:  A3 C0             rjmp 0x0EA0
0x0D5A:  0E 94 BE 0F       call 0x1F7C
0x0D5E:  0E 94 C1 0F       call 0x1F82
0x0D62:  10 F4             brbc 0, .+2
0x0D64:  0C 94 4F 07       jmp 0x0E9E
0x0D68:  0E 94 FC 0D       call 0x1BF8
0x0D6C:  11 F0             brbs 1, .+2
0x0D6E:  0C 94 47 07       jmp 0x0E8E
0x0D72:  E0 91 C6 01       lds r30, 0x01C6
0x0D76:  E0 30             cpi r30, 0x00
0x0D78:  11 F4             brbc 1, .+2
0x0D7A:  0C 94 C6 06       jmp 0x0D8C
0x0D7E:  E0 91 C7 01       lds r30, 0x01C7
0x0D82:  E0 30             cpi r30, 0x00
0x0D84:  11 F4             brbc 1, .+2
0x0D86:  0C 94 C6 06       jmp 0x0D8C
0x0D8A:  01 C0             rjmp 0x0D8E
0x0D8C:  7B C0             rjmp 0x0E84
0x0D8E:  E1 E0             ldi r30, 0x01
0x0D90:  E0 93 CE 01       sts 0x01CE, r30
0x0D94:  E0 E0             ldi r30, 0x00
0x0D96:  E0 93 1A 02       sts 0x021A, r30
0x0D9A:  E0 93 1B 02       sts 0x021B, r30
0x0D9E:  E0 93 1C 02       sts 0x021C, r30
0x0DA2:  E0 93 1D 02       sts 0x021D, r30
0x0DA6:  E0 E0             ldi r30, 0x00
0x0DA8:  F0 E0             ldi r31, 0x00
0x0DAA:  E0 93 D4 01       sts 0x01D4, r30
0x0DAE:  F0 93 D5 01       sts 0x01D5, r31
0x0DB2:  0E 94 B9 0F       call 0x1F72
0x0DB6:  0E 94 AF 0F       call 0x1F5E
0x0DBA:  E0 E0             ldi r30, 0x00
0x0DBC:  E0 93 DE 01       sts 0x01DE, r30
0x0DC0:  E0 93 DF 01       sts 0x01DF, r30
0x0DC4:  E0 93 E0 01       sts 0x01E0, r30
0x0DC8:  E0 93 E1 01       sts 0x01E1, r30
0x0DCC:  E0 93 D6 01       sts 0x01D6, r30
0x0DD0:  E0 93 D7 01       sts 0x01D7, r30
0x0DD4:  E0 93 E2 01       sts 0x01E2, r30
0x0DD8:  E0 93 E3 01       sts 0x01E3, r30
0x0DDC:  E0 93 E4 01       sts 0x01E4, r30
0x0DE0:  E0 93 E5 01       sts 0x01E5, r30
0x0DE4:  0E 94 7F 0F       call 0x1EFE
0x0DE8:  E0 E0             ldi r30, 0x00
0x0DEA:  E0 93 EA 01       sts 0x01EA, r30
0x0DEE:  E0 93 EB 01       sts 0x01EB, r30
0x0DF2:  E0 93 EC 01       sts 0x01EC, r30
0x0DF6:  E0 93 ED 01       sts 0x01ED, r30
0x0DFA:  0E 94 22 0F       call 0x1E44
0x0DFE:  E0 E0             ldi r30, 0x00
0x0E00:  E0 93 F6 01       sts 0x01F6, r30
0x0E04:  E0 93 F7 01       sts 0x01F7, r30
0x0E08:  E0 93 F8 01       sts 0x01F8, r30
0x0E0C:  E0 93 F9 01       sts 0x01F9, r30
0x0E10:  0E 94 47 0F       call 0x1E8E
0x0E14:  E0 E0             ldi r30, 0x00
0x0E16:  E0 93 FE 01       sts 0x01FE, r30
0x0E1A:  E0 93 FF 01       sts 0x01FF, r30
0x0E1E:  E0 93 00 02       sts 0x0200, r30
0x0E22:  E0 93 01 02       sts 0x0201, r30
0x0E26:  0E 94 92 0F       call 0x1F24
0x0E2A:  E0 E0             ldi r30, 0x00
0x0E2C:  E0 93 06 02       sts 0x0206, r30
0x0E30:  E0 93 07 02       sts 0x0207, r30
0x0E34:  E0 93 08 02       sts 0x0208, r30
0x0E38:  E0 93 09 02       sts 0x0209, r30
0x0E3C:  E0 93 D8 01       sts 0x01D8, r30
0x0E40:  E0 93 D9 01       sts 0x01D9, r30
0x0E44:  0E 94 A5 0F       call 0x1F4A
0x0E48:  E0 E0             ldi r30, 0x00
0x0E4A:  E0 93 0E 02       sts 0x020E, r30
0x0E4E:  E0 93 0F 02       sts 0x020F, r30
0x0E52:  E0 93 10 02       sts 0x0210, r30
0x0E56:  E0 93 11 02       sts 0x0211, r30
0x0E5A:  0E 94 5A 0F       call 0x1EB4
0x0E5E:  E1 E0             ldi r30, 0x01
0x0E60:  E0 93 C8 01       sts 0x01C8, r30
0x0E64:  E0 93 89 01       sts 0x0189, r30
0x0E68:  E0 E0             ldi r30, 0x00
0x0E6A:  E0 93 82 01       sts 0x0182, r30
0x0E6E:  E0 93 83 01       sts 0x0183, r30
0x0E72:  0E 94 C7 0F       call 0x1F8E
0x0E76:  E1 E0             ldi r30, 0x01
0x0E78:  E0 93 97 01       sts 0x0197, r30
0x0E7C:  E0 E0             ldi r30, 0x00
0x0E7E:  E0 93 C5 01       sts 0x01C5, r30
0x0E82:  04 C0             rjmp 0x0E8C
0x0E84:  0E 94 A0 0E       call 0x1D40
0x0E88:  0E 94 C7 0F       call 0x1F8E
0x0E8C:  08 C0             rjmp 0x0E9E
0x0E8E:  E1 E0             ldi r30, 0x01
0x0E90:  E0 93 89 01       sts 0x0189, r30
0x0E94:  0E 94 C7 0F       call 0x1F8E
0x0E98:  E1 E0             ldi r30, 0x01
0x0E9A:  E0 93 97 01       sts 0x0197, r30
0x0E9E:  05 C0             rjmp 0x0EAA
0x0EA0:  0E 94 C7 0F       call 0x1F8E
0x0EA4:  E0 E0             ldi r30, 0x00
0x0EA6:  E0 93 89 01       sts 0x0189, r30
0x0EAA:  79 C0             rjmp 0x0F9E
0x0EAC:  E0 91 97 01       lds r30, 0x0197
0x0EB0:  E0 30             cpi r30, 0x00
0x0EB2:  11 F4             brbc 1, .+2
0x0EB4:  0C 94 63 07       jmp 0x0EC6
0x0EB8:  E0 91 9E 01       lds r30, 0x019E
0x0EBC:  E0 30             cpi r30, 0x00
0x0EBE:  11 F0             brbs 1, .+2
0x0EC0:  0C 94 63 07       jmp 0x0EC6
0x0EC4:  01 C0             rjmp 0x0EC8
0x0EC6:  6B C0             rjmp 0x0F9E
0x0EC8:  CC 9B             sbis 0x19, 4
0x0ECA:  67 C0             rjmp 0x0F9A
0x0ECC:  0E 94 BE 0F       call 0x1F7C
0x0ED0:  0E 94 C1 0F       call 0x1F82
0x0ED4:  10 F4             brbc 0, .+2
0x0ED6:  0C 94 CC 07       jmp 0x0F98
0x0EDA:  0E 94 FC 0D       call 0x1BF8
0x0EDE:  11 F0             brbs 1, .+2
0x0EE0:  0C 94 C8 07       jmp 0x0F90
0x0EE4:  0E 94 19 0F       call 0x1E32
0x0EE8:  0E 94 FE 0E       call 0x1DFC
0x0EEC:  0E 94 A2 10       call 0x2144
0x0EF0:  0E 94 51 0F       call 0x1EA2
0x0EF4:  0E 94 A2 10       call 0x2144
0x0EF8:  0E 94 64 0F       call 0x1EC8
0x0EFC:  0E 94 A2 10       call 0x2144
0x0F00:  0E 94 89 0F       call 0x1F12
0x0F04:  0E 94 A2 10       call 0x2144
0x0F08:  0E 94 9C 0F       call 0x1F38
0x0F0C:  0E 94 EB 0E       call 0x1DD6
0x0F10:  0E 94 07 0F       call 0x1E0E
0x0F14:  E0 93 EE 01       sts 0x01EE, r30
0x0F18:  F0 93 EF 01       sts 0x01EF, r31
0x0F1C:  60 93 F0 01       sts 0x01F0, r22
0x0F20:  70 93 F1 01       sts 0x01F1, r23
0x0F24:  E0 91 EE 01       lds r30, 0x01EE
0x0F28:  F0 91 EF 01       lds r31, 0x01EF
0x0F2C:  A0 91 D6 01       lds r26, 0x01D6
0x0F30:  B0 91 D7 01       lds r27, 0x01D7
0x0F34:  EA 0F             add r30, r26
0x0F36:  FB 1F             adc r31, r27
0x0F38:  0E 94 B9 0F       call 0x1F72
0x0F3C:  A0 91 D6 01       lds r26, 0x01D6
0x0F40:  B0 91 D7 01       lds r27, 0x01D7
0x0F44:  1A 97             sbiw r26, 0x0A
0x0F46:  10 F4             brbc 0, .+2
0x0F48:  0C 94 B5 07       jmp 0x0F6A
0x0F4C:  E0 E0             ldi r30, 0x00
0x0F4E:  E0 93 CE 01       sts 0x01CE, r30
0x0F52:  0E 94 60 0E       call 0x1CC0
0x0F56:  0E 94 CD 0F       call 0x1F9A
0x0F5A:  E0 E0             ldi r30, 0x00
0x0F5C:  E0 93 C6 01       sts 0x01C6, r30
0x0F60:  E0 93 C7 01       sts 0x01C7, r30
0x0F64:  E0 93 C8 01       sts 0x01C8, r30
0x0F68:  12 C0             rjmp 0x0F8E
0x0F6A:  E0 E0             ldi r30, 0x00
0x0F6C:  E0 93 CE 01       sts 0x01CE, r30
0x0F70:  0E 94 60 0E       call 0x1CC0
0x0F74:  0E 94 CD 0F       call 0x1F9A
0x0F78:  0E 94 A0 0E       call 0x1D40
0x0F7C:  E1 E0             ldi r30, 0x01
0x0F7E:  E0 93 D0 01       sts 0x01D0, r30
0x0F82:  E1 E0             ldi r30, 0x01
0x0F84:  F0 E0             ldi r31, 0x00
0x0F86:  E0 93 84 01       sts 0x0184, r30
0x0F8A:  F0 93 85 01       sts 0x0185, r31
0x0F8E:  04 C0             rjmp 0x0F98
0x0F90:  0E 94 60 0E       call 0x1CC0
0x0F94:  0E 94 CD 0F       call 0x1F9A
0x0F98:  02 C0             rjmp 0x0F9E
0x0F9A:  0E 94 C7 0F       call 0x1F8E
0x0F9E:  08 95             ret
0x0FA0:  E0 91 98 01       lds r30, 0x0198
0x0FA4:  E0 30             cpi r30, 0x00
0x0FA6:  11 F0             brbs 1, .+2
0x0FA8:  0C 94 1F 08       jmp 0x103E
0x0FAC:  CE 99             sbic 0x19, 6
0x0FAE:  43 C0             rjmp 0x1036
0x0FB0:  0E 94 D5 0F       call 0x1FAA
0x0FB4:  A6 30             cpi r26, 0x06
0x0FB6:  10 F4             brbc 0, .+2
0x0FB8:  0C 94 1A 08       jmp 0x1034
0x0FBC:  E0 E0             ldi r30, 0x00
0x0FBE:  E0 93 89 01       sts 0x0189, r30
0x0FC2:  0E 94 FC 0D       call 0x1BF8
0x0FC6:  11 F0             brbs 1, .+2
0x0FC8:  0C 94 18 08       jmp 0x1030
0x0FCC:  E0 91 9C 01       lds r30, 0x019C
0x0FD0:  E0 30             cpi r30, 0x00
0x0FD2:  11 F0             brbs 1, .+2
0x0FD4:  0C 94 F3 07       jmp 0x0FE6
0x0FD8:  E0 91 C5 01       lds r30, 0x01C5
0x0FDC:  E0 30             cpi r30, 0x00
0x0FDE:  11 F0             brbs 1, .+2
0x0FE0:  0C 94 F3 07       jmp 0x0FE6
0x0FE4:  07 C0             rjmp 0x0FF4
0x0FE6:  0E 94 64 0E       call 0x1CC8
0x0FEA:  0E 94 6D 0E       call 0x1CDA
0x0FEE:  0E 94 DD 0F       call 0x1FBA
0x0FF2:  1D C0             rjmp 0x102E
0x0FF4:  E0 91 C6 01       lds r30, 0x01C6
0x0FF8:  E0 30             cpi r30, 0x00
0x0FFA:  11 F4             brbc 1, .+2
0x0FFC:  0C 94 0D 08       jmp 0x101A
0x1000:  E0 91 C7 01       lds r30, 0x01C7
0x1004:  E0 30             cpi r30, 0x00
0x1006:  11 F4             brbc 1, .+2
0x1008:  0C 94 0D 08       jmp 0x101A
0x100C:  E0 91 C8 01       lds r30, 0x01C8
0x1010:  E0 30             cpi r30, 0x00
0x1012:  11 F4             brbc 1, .+2
0x1014:  0C 94 0D 08       jmp 0x101A
0x1018:  01 C0             rjmp 0x101C
0x101A:  05 C0             rjmp 0x1026
0x101C:  0E 94 64 0E       call 0x1CC8
0x1020:  0E 94 DD 0F       call 0x1FBA
0x1024:  04 C0             rjmp 0x102E
0x1026:  0E 94 64 0E       call 0x1CC8
0x102A:  0E 94 DD 0F       call 0x1FBA
0x102E:  02 C0             rjmp 0x1034
0x1030:  0E 94 DD 0F       call 0x1FBA
0x1034:  03 C0             rjmp 0x103C
0x1036:  E0 E0             ldi r30, 0x00
0x1038:  E0 93 9A 01       sts 0x019A, r30
0x103C:  13 C0             rjmp 0x1064
0x103E:  CE 9B             sbis 0x19, 6
0x1040:  0E C0             rjmp 0x105E
0x1042:  0E 94 D5 0F       call 0x1FAA
0x1046:  AB 30             cpi r26, 0x0B
0x1048:  10 F4             brbc 0, .+2
0x104A:  0C 94 2E 08       jmp 0x105C
0x104E:  0E 94 1E 0E       call 0x1C3C
0x1052:  E0 93 9A 01       sts 0x019A, r30
0x1056:  E0 E0             ldi r30, 0x00
0x1058:  E0 93 98 01       sts 0x0198, r30
0x105C:  03 C0             rjmp 0x1064
0x105E:  E0 E0             ldi r30, 0x00
0x1060:  E0 93 9A 01       sts 0x019A, r30
0x1064:  08 95             ret
0x1066:  A0 91 89 01       lds r26, 0x0189
0x106A:  A1 30             cpi r26, 0x01
0x106C:  11 F0             brbs 1, .+2
0x106E:  0C 94 4F 08       jmp 0x109E
0x1072:  9C 9B             sbis 0x13, 4
0x1074:  0A C0             rjmp 0x108A
0x1076:  66 24             eor r6, r6
0x1078:  73 94             inc r7
0x107A:  E3 E0             ldi r30, 0x03
0x107C:  E7 15             .word 0x15E7
0x107E:  10 F0             brbs 0, .+2
0x1080:  0C 94 44 08       jmp 0x1088
0x1084:  77 24             eor r7, r7
0x1086:  A9 9A             sbi 0x15, 1
0x1088:  09 C0             rjmp 0x109C
0x108A:  77 24             eor r7, r7
0x108C:  63 94             inc r6
0x108E:  E3 E0             ldi r30, 0x03
0x1090:  E6 15             .word 0x15E6
0x1092:  10 F0             brbs 0, .+2
0x1094:  0C 94 4E 08       jmp 0x109C
0x1098:  66 24             eor r6, r6
0x109A:  A9 98             cbi 0x15, 1
0x109C:  03 C0             rjmp 0x10A4
0x109E:  77 24             eor r7, r7
0x10A0:  66 24             eor r6, r6
0x10A2:  A9 98             cbi 0x15, 1
0x10A4:  08 95             ret
0x10A6:  E0 91 9C 01       lds r30, 0x019C
0x10AA:  E0 30             cpi r30, 0x00
0x10AC:  11 F4             brbc 1, .+2
0x10AE:  0C 94 60 08       jmp 0x10C0
0x10B2:  E0 91 C5 01       lds r30, 0x01C5
0x10B6:  E0 30             cpi r30, 0x00
0x10B8:  11 F0             brbs 1, .+2
0x10BA:  0C 94 60 08       jmp 0x10C0
0x10BE:  1A C0             rjmp 0x10F4
0x10C0:  CF 9B             sbis 0x19, 7
0x10C2:  15 C0             rjmp 0x10EE
0x10C4:  E0 91 99 01       lds r30, 0x0199
0x10C8:  EF 5F             subi r30, 0xFF
0x10CA:  E0 93 99 01       sts 0x0199, r30
0x10CE:  A0 91 99 01       lds r26, 0x0199
0x10D2:  A5 30             cpi r26, 0x05
0x10D4:  10 F4             brbc 0, .+2
0x10D6:  0C 94 76 08       jmp 0x10EC
0x10DA:  E0 E0             ldi r30, 0x00
0x10DC:  E0 93 99 01       sts 0x0199, r30
0x10E0:  E1 E0             ldi r30, 0x01
0x10E2:  E0 93 A2 01       sts 0x01A2, r30
0x10E6:  E0 93 9C 01       sts 0x019C, r30
0x10EA:  A8 9A             sbi 0x15, 0
0x10EC:  03 C0             rjmp 0x10F4
0x10EE:  E0 E0             ldi r30, 0x00
0x10F0:  E0 93 99 01       sts 0x0199, r30
0x10F4:  08 95             ret
0x10F6:  E1 E0             ldi r30, 0x01
0x10F8:  E0 93 6F 01       sts 0x016F, r30
0x10FC:  AE 9A             sbi 0x15, 6
0x10FE:  08 95             ret
0x1100:  E0 91 6F 01       lds r30, 0x016F
0x1104:  E0 30             cpi r30, 0x00
0x1106:  11 F4             brbc 1, .+2
0x1108:  0C 94 97 08       jmp 0x112E
0x110C:  E0 91 70 01       lds r30, 0x0170
0x1110:  EF 5F             subi r30, 0xFF
0x1112:  E0 93 70 01       sts 0x0170, r30
0x1116:  A0 91 70 01       lds r26, 0x0170
0x111A:  A6 30             cpi r26, 0x06
0x111C:  10 F4             brbc 0, .+2
0x111E:  0C 94 97 08       jmp 0x112E
0x1122:  E0 E0             ldi r30, 0x00
0x1124:  E0 93 6F 01       sts 0x016F, r30
0x1128:  AE 98             cbi 0x15, 6
0x112A:  E0 93 70 01       sts 0x0170, r30
0x112E:  08 95             ret
0x1130:  E0 91 9C 01       lds r30, 0x019C
0x1134:  E0 30             cpi r30, 0x00
0x1136:  11 F0             brbs 1, .+2
0x1138:  0C 94 A5 08       jmp 0x114A
0x113C:  E0 91 C5 01       lds r30, 0x01C5
0x1140:  E0 30             cpi r30, 0x00
0x1142:  11 F0             brbs 1, .+2
0x1144:  0C 94 A5 08       jmp 0x114A
0x1148:  28 C0             rjmp 0x119A
0x114A:  E0 91 72 01       lds r30, 0x0172
0x114E:  E0 30             cpi r30, 0x00
0x1150:  11 F0             brbs 1, .+2
0x1152:  0C 94 BD 08       jmp 0x117A
0x1156:  E0 91 71 01       lds r30, 0x0171
0x115A:  EF 5F             subi r30, 0xFF
0x115C:  E0 93 71 01       sts 0x0171, r30
0x1160:  AE 98             cbi 0x15, 6
0x1162:  0E 94 F3 0F       call 0x1FE6
0x1166:  10 F4             brbc 0, .+2
0x1168:  0C 94 BC 08       jmp 0x1178
0x116C:  E0 E0             ldi r30, 0x00
0x116E:  E0 93 71 01       sts 0x0171, r30
0x1172:  E1 E0             ldi r30, 0x01
0x1174:  E0 93 72 01       sts 0x0172, r30
0x1178:  10 C0             rjmp 0x119A
0x117A:  E0 91 71 01       lds r30, 0x0171
0x117E:  EF 5F             subi r30, 0xFF
0x1180:  E0 93 71 01       sts 0x0171, r30
0x1184:  AE 9A             sbi 0x15, 6
0x1186:  0E 94 F3 0F       call 0x1FE6
0x118A:  10 F4             brbc 0, .+2
0x118C:  0C 94 CD 08       jmp 0x119A
0x1190:  E0 E0             ldi r30, 0x00
0x1192:  E0 93 71 01       sts 0x0171, r30
0x1196:  E0 93 72 01       sts 0x0172, r30
0x119A:  08 95             ret
0x119C:  E1 E0             ldi r30, 0x01
0x119E:  E9 15             .word 0x15E9
0x11A0:  11 F0             brbs 1, .+2
0x11A2:  0C 94 01 09       jmp 0x1202
0x11A6:  B3 94             inc r11
0x11A8:  E8 2D             mov r30, r8
0x11AA:  E0 30             cpi r30, 0x00
0x11AC:  11 F0             brbs 1, .+2
0x11AE:  0C 94 E4 08       jmp 0x11C8
0x11B2:  AE 9A             sbi 0x15, 6
0x11B4:  E2 E3             ldi r30, 0x32
0x11B6:  EB 15             .word 0x15EB
0x11B8:  10 F0             brbs 0, .+2
0x11BA:  0C 94 E3 08       jmp 0x11C6
0x11BE:  AE 98             cbi 0x15, 6
0x11C0:  E1 E0             ldi r30, 0x01
0x11C2:  8E 2E             mov r8, r30
0x11C4:  BB 24             eor r11, r11
0x11C6:  1D C0             rjmp 0x1202
0x11C8:  E1 30             cpi r30, 0x01
0x11CA:  11 F0             brbs 1, .+2
0x11CC:  0C 94 F3 08       jmp 0x11E6
0x11D0:  AE 98             cbi 0x15, 6
0x11D2:  E9 E1             ldi r30, 0x19
0x11D4:  EB 15             .word 0x15EB
0x11D6:  10 F0             brbs 0, .+2
0x11D8:  0C 94 F2 08       jmp 0x11E4
0x11DC:  AE 9A             sbi 0x15, 6
0x11DE:  E2 E0             ldi r30, 0x02
0x11E0:  8E 2E             mov r8, r30
0x11E2:  BB 24             eor r11, r11
0x11E4:  0E C0             rjmp 0x1202
0x11E6:  E2 30             cpi r30, 0x02
0x11E8:  11 F0             brbs 1, .+2
0x11EA:  0C 94 01 09       jmp 0x1202
0x11EE:  AE 9A             sbi 0x15, 6
0x11F0:  E2 E3             ldi r30, 0x32
0x11F2:  EB 15             .word 0x15EB
0x11F4:  10 F0             brbs 0, .+2
0x11F6:  0C 94 01 09       jmp 0x1202
0x11FA:  AE 98             cbi 0x15, 6
0x11FC:  88 24             eor r8, r8
0x11FE:  BB 24             eor r11, r11
0x1200:  99 24             eor r9, r9
0x1202:  08 95             ret
0x1204:  E2 E0             ldi r30, 0x02
0x1206:  E9 15             .word 0x15E9
0x1208:  11 F0             brbs 1, .+2
0x120A:  0C 94 12 09       jmp 0x1224
0x120E:  B3 94             inc r11
0x1210:  AE 9A             sbi 0x15, 6
0x1212:  E2 E3             ldi r30, 0x32
0x1214:  EB 15             .word 0x15EB
0x1216:  10 F0             brbs 0, .+2
0x1218:  0C 94 12 09       jmp 0x1224
0x121C:  AE 98             cbi 0x15, 6
0x121E:  88 24             eor r8, r8
0x1220:  BB 24             eor r11, r11
0x1222:  99 24             eor r9, r9
0x1224:  08 95             ret
0x1226:  E0 E0             ldi r30, 0x00
0x1228:  E0 93 73 01       sts 0x0173, r30
0x122C:  E0 93 74 01       sts 0x0174, r30
0x1230:  21 97             sbiw r28, 0x01
0x1232:  E1 E0             ldi r30, 0x01
0x1234:  E8 83             st Y, r30
0x1236:  E8 81             ld r30, Y
0x1238:  E0 30             cpi r30, 0x00
0x123A:  11 F4             brbc 1, .+2
0x123C:  0C 94 C4 09       jmp 0x1388
0x1240:  0E 94 FA 0F       call 0x1FF4
0x1244:  99 97             sbiw r26, 0x29
0x1246:  10 F0             brbs 0, .+2
0x1248:  0C 94 2E 09       jmp 0x125C
0x124C:  E1 E0             ldi r30, 0x01
0x124E:  E0 93 9F 01       sts 0x019F, r30
0x1252:  E4 E0             ldi r30, 0x04
0x1254:  0E 94 FF 0F       call 0x1FFE
0x1258:  AE 9A             sbi 0x15, 6
0x125A:  95 C0             rjmp 0x1386
0x125C:  0E 94 FA 0F       call 0x1FF4
0x1260:  A1 35             cpi r26, 0x51
0x1262:  E0 E0             ldi r30, 0x00
0x1264:  BE 07             .word 0x07BE
0x1266:  10 F0             brbs 0, .+2
0x1268:  0C 94 3D 09       jmp 0x127A
0x126C:  E3 E0             ldi r30, 0x03
0x126E:  E0 93 9F 01       sts 0x019F, r30
0x1272:  EC E0             ldi r30, 0x0C
0x1274:  0E 94 05 10       call 0x200A
0x1278:  86 C0             rjmp 0x1386
0x127A:  0E 94 FA 0F       call 0x1FF4
0x127E:  A9 37             cpi r26, 0x79
0x1280:  E0 E0             ldi r30, 0x00
0x1282:  BE 07             .word 0x07BE
0x1284:  10 F0             brbs 0, .+2
0x1286:  0C 94 4D 09       jmp 0x129A
0x128A:  E7 E0             ldi r30, 0x07
0x128C:  E0 93 9F 01       sts 0x019F, r30
0x1290:  EC E1             ldi r30, 0x1C
0x1292:  0E 94 FF 0F       call 0x1FFE
0x1296:  AE 98             cbi 0x15, 6
0x1298:  76 C0             rjmp 0x1386
0x129A:  0E 94 FA 0F       call 0x1FF4
0x129E:  A1 3A             cpi r26, 0xA1
0x12A0:  E0 E0             ldi r30, 0x00
0x12A2:  BE 07             .word 0x07BE
0x12A4:  10 F0             brbs 0, .+2
0x12A6:  0C 94 5C 09       jmp 0x12B8
0x12AA:  EF E0             ldi r30, 0x0F
0x12AC:  E0 93 9F 01       sts 0x019F, r30
0x12B0:  EC E3             ldi r30, 0x3C
0x12B2:  0E 94 05 10       call 0x200A
0x12B6:  67 C0             rjmp 0x1386
0x12B8:  0E 94 FA 0F       call 0x1FF4
0x12BC:  A9 3C             cpi r26, 0xC9
0x12BE:  E0 E0             ldi r30, 0x00
0x12C0:  BE 07             .word 0x07BE
0x12C2:  10 F0             brbs 0, .+2
0x12C4:  0C 94 6B 09       jmp 0x12D6
0x12C8:  EF E1             ldi r30, 0x1F
0x12CA:  E0 93 9F 01       sts 0x019F, r30
0x12CE:  EC E7             ldi r30, 0x7C
0x12D0:  0E 94 FF 0F       call 0x1FFE
0x12D4:  58 C0             rjmp 0x1386
0x12D6:  0E 94 FA 0F       call 0x1FF4
0x12DA:  A1 3F             cpi r26, 0xF1
0x12DC:  E0 E0             ldi r30, 0x00
0x12DE:  BE 07             .word 0x07BE
0x12E0:  10 F0             brbs 0, .+2
0x12E2:  0C 94 7A 09       jmp 0x12F4
0x12E6:  EF E3             ldi r30, 0x3F
0x12E8:  E0 93 9F 01       sts 0x019F, r30
0x12EC:  EC EF             ldi r30, 0xFC
0x12EE:  0E 94 05 10       call 0x200A
0x12F2:  49 C0             rjmp 0x1386
0x12F4:  0E 94 FA 0F       call 0x1FF4
0x12F8:  A9 31             cpi r26, 0x19
0x12FA:  E1 E0             ldi r30, 0x01
0x12FC:  BE 07             .word 0x07BE
0x12FE:  10 F0             brbs 0, .+2
0x1300:  0C 94 8C 09       jmp 0x1318
0x1304:  EF E7             ldi r30, 0x7F
0x1306:  E0 93 9F 01       sts 0x019F, r30
0x130A:  EC EF             ldi r30, 0xFC
0x130C:  E0 93 A0 01       sts 0x01A0, r30
0x1310:  E1 E0             ldi r30, 0x01
0x1312:  E0 93 A1 01       sts 0x01A1, r30
0x1316:  37 C0             rjmp 0x1386
0x1318:  0E 94 FA 0F       call 0x1FF4
0x131C:  A1 34             cpi r26, 0x41
0x131E:  E1 E0             ldi r30, 0x01
0x1320:  BE 07             .word 0x07BE
0x1322:  10 F0             brbs 0, .+2
0x1324:  0C 94 9B 09       jmp 0x1336
0x1328:  EF EF             ldi r30, 0xFF
0x132A:  E0 93 9F 01       sts 0x019F, r30
0x132E:  EC EF             ldi r30, 0xFC
0x1330:  0E 94 0B 10       call 0x2016
0x1334:  28 C0             rjmp 0x1386
0x1336:  0E 94 FA 0F       call 0x1FF4
0x133A:  A9 36             cpi r26, 0x69
0x133C:  E1 E0             ldi r30, 0x01
0x133E:  BE 07             .word 0x07BE
0x1340:  10 F0             brbs 0, .+2
0x1342:  0C 94 AD 09       jmp 0x135A
0x1346:  EF EF             ldi r30, 0xFF
0x1348:  E0 93 9F 01       sts 0x019F, r30
0x134C:  ED EF             ldi r30, 0xFD
0x134E:  E0 93 A0 01       sts 0x01A0, r30
0x1352:  E7 E0             ldi r30, 0x07
0x1354:  E0 93 A1 01       sts 0x01A1, r30
0x1358:  16 C0             rjmp 0x1386
0x135A:  0E 94 FA 0F       call 0x1FF4
0x135E:  A1 39             cpi r26, 0x91
0x1360:  E1 E0             ldi r30, 0x01
0x1362:  BE 07             .word 0x07BE
0x1364:  10 F0             brbs 0, .+2
0x1366:  0C 94 BD 09       jmp 0x137A
0x136A:  0E 94 11 10       call 0x2022
0x136E:  E0 93 A0 01       sts 0x01A0, r30
0x1372:  EF E0             ldi r30, 0x0F
0x1374:  E0 93 A1 01       sts 0x01A1, r30
0x1378:  06 C0             rjmp 0x1386
0x137A:  E0 E0             ldi r30, 0x00
0x137C:  E0 93 95 01       sts 0x0195, r30
0x1380:  E8 83             st Y, r30
0x1382:  0E 94 DC 02       call 0x05B8
0x1386:  57 CF             rjmp 0x1236
0x1388:  21 96             adiw r28, 0x01
0x138A:  08 95             ret
0x138C:  E0 91 95 01       lds r30, 0x0195
0x1390:  E0 30             cpi r30, 0x00
0x1392:  11 F0             brbs 1, .+2
0x1394:  0C 94 DD 0A       jmp 0x15BA
0x1398:  AA 97             sbiw r28, 0x2A
0x139A:  8A E2             ldi r24, 0x2A
0x139C:  A0 E0             ldi r26, 0x00
0x139E:  B0 E0             ldi r27, 0x00
0x13A0:  E8 E5             ldi r30, 0x58
0x13A2:  F0 E0             ldi r31, 0x00
0x13A4:  0E 94 F8 10       call 0x21F0
0x13A8:  E8 81             ld r30, Y
0x13AA:  F9 81             ldd r31, Y+1
0x13AC:  0E 94 15 10       call 0x202A
0x13B0:  10 F0             brbs 0, .+2
0x13B2:  0C 94 E0 09       jmp 0x13C0
0x13B6:  0E 94 11 10       call 0x2022
0x13BA:  0E 94 0B 10       call 0x2016
0x13BE:  FC C0             rjmp 0x15B8
0x13C0:  EA 81             ldd r30, Y+2
0x13C2:  FB 81             ldd r31, Y+3
0x13C4:  0E 94 15 10       call 0x202A
0x13C8:  10 F0             brbs 0, .+2
0x13CA:  0C 94 EC 09       jmp 0x13D8
0x13CE:  0E 94 11 10       call 0x2022
0x13D2:  0E 94 0B 10       call 0x2016
0x13D6:  F0 C0             rjmp 0x15B8
0x13D8:  EC 81             ldd r30, Y+4
0x13DA:  FD 81             ldd r31, Y+5
0x13DC:  0E 94 15 10       call 0x202A
0x13E0:  10 F0             brbs 0, .+2
0x13E2:  0C 94 F8 09       jmp 0x13F0
0x13E6:  0E 94 11 10       call 0x2022
0x13EA:  0E 94 0B 10       call 0x2016
0x13EE:  E4 C0             rjmp 0x15B8
0x13F0:  EE 81             ldd r30, Y+6
0x13F2:  FF 81             ldd r31, Y+7
0x13F4:  0E 94 15 10       call 0x202A
0x13F8:  10 F0             brbs 0, .+2
0x13FA:  0C 94 04 0A       jmp 0x1408
0x13FE:  0E 94 11 10       call 0x2022
0x1402:  0E 94 0B 10       call 0x2016
0x1406:  D8 C0             rjmp 0x15B8
0x1408:  E8 85             ldd r30, Y+8
0x140A:  F9 85             ldd r31, Y+9
0x140C:  0E 94 15 10       call 0x202A
0x1410:  10 F0             brbs 0, .+2
0x1412:  0C 94 13 0A       jmp 0x1426
0x1416:  0E 94 11 10       call 0x2022
0x141A:  E0 93 A0 01       sts 0x01A0, r30
0x141E:  E1 E0             ldi r30, 0x01
0x1420:  E0 93 A1 01       sts 0x01A1, r30
0x1424:  C9 C0             rjmp 0x15B8
0x1426:  EA 85             ldd r30, Y+10
0x1428:  FB 85             ldd r31, Y+11
0x142A:  0E 94 15 10       call 0x202A
0x142E:  10 F0             brbs 0, .+2
0x1430:  0C 94 1F 0A       jmp 0x143E
0x1434:  0E 94 11 10       call 0x2022
0x1438:  0E 94 05 10       call 0x200A
0x143C:  BD C0             rjmp 0x15B8
0x143E:  EC 85             ldd r30, Y+12
0x1440:  FD 85             ldd r31, Y+13
0x1442:  0E 94 15 10       call 0x202A
0x1446:  10 F0             brbs 0, .+2
0x1448:  0C 94 2D 0A       jmp 0x145A
0x144C:  EF EF             ldi r30, 0xFF
0x144E:  E0 93 9F 01       sts 0x019F, r30
0x1452:  EF E7             ldi r30, 0x7F
0x1454:  0E 94 05 10       call 0x200A
0x1458:  AF C0             rjmp 0x15B8
0x145A:  EE 85             ldd r30, Y+14
0x145C:  FF 85             ldd r31, Y+15
0x145E:  0E 94 15 10       call 0x202A
0x1462:  10 F0             brbs 0, .+2
0x1464:  0C 94 3B 0A       jmp 0x1476
0x1468:  EF EF             ldi r30, 0xFF
0x146A:  E0 93 9F 01       sts 0x019F, r30
0x146E:  EF E3             ldi r30, 0x3F
0x1470:  0E 94 05 10       call 0x200A
0x1474:  A1 C0             rjmp 0x15B8
0x1476:  E8 89             ldd r30, Y+16
0x1478:  F9 89             ldd r31, Y+17
0x147A:  0E 94 15 10       call 0x202A
0x147E:  10 F0             brbs 0, .+2
0x1480:  0C 94 49 0A       jmp 0x1492
0x1484:  EF EF             ldi r30, 0xFF
0x1486:  E0 93 9F 01       sts 0x019F, r30
0x148A:  EF E1             ldi r30, 0x1F
0x148C:  0E 94 05 10       call 0x200A
0x1490:  93 C0             rjmp 0x15B8
0x1492:  EA 89             ldd r30, Y+18
0x1494:  FB 89             ldd r31, Y+19
0x1496:  0E 94 15 10       call 0x202A
0x149A:  10 F0             brbs 0, .+2
0x149C:  0C 94 57 0A       jmp 0x14AE
0x14A0:  EF EF             ldi r30, 0xFF
0x14A2:  E0 93 9F 01       sts 0x019F, r30
0x14A6:  EF E0             ldi r30, 0x0F
0x14A8:  0E 94 05 10       call 0x200A
0x14AC:  85 C0             rjmp 0x15B8
0x14AE:  EC 89             ldd r30, Y+20
0x14B0:  FD 89             ldd r31, Y+21
0x14B2:  0E 94 15 10       call 0x202A
0x14B6:  10 F0             brbs 0, .+2
0x14B8:  0C 94 65 0A       jmp 0x14CA
0x14BC:  EF EF             ldi r30, 0xFF
0x14BE:  E0 93 9F 01       sts 0x019F, r30
0x14C2:  E7 E0             ldi r30, 0x07
0x14C4:  0E 94 05 10       call 0x200A
0x14C8:  77 C0             rjmp 0x15B8
0x14CA:  EE 89             ldd r30, Y+22
0x14CC:  FF 89             ldd r31, Y+23
0x14CE:  0E 94 15 10       call 0x202A
0x14D2:  10 F0             brbs 0, .+2
0x14D4:  0C 94 73 0A       jmp 0x14E6
0x14D8:  EF EF             ldi r30, 0xFF
0x14DA:  E0 93 9F 01       sts 0x019F, r30
0x14DE:  E3 E0             ldi r30, 0x03
0x14E0:  0E 94 05 10       call 0x200A
0x14E4:  69 C0             rjmp 0x15B8
0x14E6:  E8 8D             ldd r30, Y+24
0x14E8:  F9 8D             ldd r31, Y+25
0x14EA:  0E 94 15 10       call 0x202A
0x14EE:  10 F0             brbs 0, .+2
0x14F0:  0C 94 81 0A       jmp 0x1502
0x14F4:  EF EF             ldi r30, 0xFF
0x14F6:  E0 93 9F 01       sts 0x019F, r30
0x14FA:  E1 E0             ldi r30, 0x01
0x14FC:  0E 94 05 10       call 0x200A
0x1500:  5B C0             rjmp 0x15B8
0x1502:  EA 8D             ldd r30, Y+26
0x1504:  FB 8D             ldd r31, Y+27
0x1506:  0E 94 15 10       call 0x202A
0x150A:  10 F0             brbs 0, .+2
0x150C:  0C 94 8C 0A       jmp 0x1518
0x1510:  EF EF             ldi r30, 0xFF
0x1512:  0E 94 1C 10       call 0x2038
0x1516:  50 C0             rjmp 0x15B8
0x1518:  EC 8D             ldd r30, Y+28
0x151A:  FD 8D             ldd r31, Y+29
0x151C:  0E 94 15 10       call 0x202A
0x1520:  10 F0             brbs 0, .+2
0x1522:  0C 94 97 0A       jmp 0x152E
0x1526:  EF E7             ldi r30, 0x7F
0x1528:  0E 94 1C 10       call 0x2038
0x152C:  45 C0             rjmp 0x15B8
0x152E:  EE 8D             ldd r30, Y+30
0x1530:  FF 8D             ldd r31, Y+31
0x1532:  0E 94 15 10       call 0x202A
0x1536:  10 F0             brbs 0, .+2
0x1538:  0C 94 A2 0A       jmp 0x1544
0x153C:  EF E3             ldi r30, 0x3F
0x153E:  0E 94 1C 10       call 0x2038
0x1542:  3A C0             rjmp 0x15B8
0x1544:  E8 A1             ldd r30, Y+32
0x1546:  F9 A1             ldd r31, Y+33
0x1548:  0E 94 15 10       call 0x202A
0x154C:  10 F0             brbs 0, .+2
0x154E:  0C 94 AD 0A       jmp 0x155A
0x1552:  EF E1             ldi r30, 0x1F
0x1554:  0E 94 1C 10       call 0x2038
0x1558:  2F C0             rjmp 0x15B8
0x155A:  EA A1             ldd r30, Y+34
0x155C:  FB A1             ldd r31, Y+35
0x155E:  0E 94 15 10       call 0x202A
0x1562:  10 F0             brbs 0, .+2
0x1564:  0C 94 B8 0A       jmp 0x1570
0x1568:  EF E0             ldi r30, 0x0F
0x156A:  0E 94 1C 10       call 0x2038
0x156E:  24 C0             rjmp 0x15B8
0x1570:  EC A1             ldd r30, Y+36
0x1572:  FD A1             ldd r31, Y+37
0x1574:  0E 94 15 10       call 0x202A
0x1578:  10 F0             brbs 0, .+2
0x157A:  0C 94 C3 0A       jmp 0x1586
0x157E:  E7 E0             ldi r30, 0x07
0x1580:  0E 94 1C 10       call 0x2038
0x1584:  19 C0             rjmp 0x15B8
0x1586:  EE A1             ldd r30, Y+38
0x1588:  FF A1             ldd r31, Y+39
0x158A:  0E 94 15 10       call 0x202A
0x158E:  10 F0             brbs 0, .+2
0x1590:  0C 94 CE 0A       jmp 0x159C
0x1594:  E3 E0             ldi r30, 0x03
0x1596:  0E 94 1C 10       call 0x2038
0x159A:  0E C0             rjmp 0x15B8
0x159C:  E8 A5             ldd r30, Y+40
0x159E:  F9 A5             ldd r31, Y+41
0x15A0:  0E 94 15 10       call 0x202A
0x15A4:  10 F0             brbs 0, .+2
0x15A6:  0C 94 D9 0A       jmp 0x15B2
0x15AA:  E1 E0             ldi r30, 0x01
0x15AC:  0E 94 1C 10       call 0x2038
0x15B0:  03 C0             rjmp 0x15B8
0x15B2:  E0 E0             ldi r30, 0x00
0x15B4:  0E 94 1C 10       call 0x2038
0x15B8:  AA 96             adiw r28, 0x2A
0x15BA:  08 95             ret
0x15BC:  0E 94 43 0B       call 0x1686
0x15C0:  E0 91 95 01       lds r30, 0x0195
0x15C4:  E0 30             cpi r30, 0x00
0x15C6:  11 F4             brbc 1, .+2
0x15C8:  0C 94 42 0B       jmp 0x1684
0x15CC:  E0 91 7C 01       lds r30, 0x017C
0x15D0:  E0 30             cpi r30, 0x00
0x15D2:  11 F0             brbs 1, .+2
0x15D4:  0C 94 FE 0A       jmp 0x15FC
0x15D8:  9D 99             sbic 0x13, 5
0x15DA:  0C C0             rjmp 0x15F4
0x15DC:  0E 94 20 10       call 0x2040
0x15E0:  10 F4             brbc 0, .+2
0x15E2:  0C 94 F9 0A       jmp 0x15F2
0x15E6:  E1 E0             ldi r30, 0x01
0x15E8:  E0 93 7C 01       sts 0x017C, r30
0x15EC:  E0 E0             ldi r30, 0x00
0x15EE:  E0 93 79 01       sts 0x0179, r30
0x15F2:  03 C0             rjmp 0x15FA
0x15F4:  E0 E0             ldi r30, 0x00
0x15F6:  E0 93 79 01       sts 0x0179, r30
0x15FA:  10 C0             rjmp 0x161C
0x15FC:  9D 9B             sbis 0x13, 5
0x15FE:  0B C0             rjmp 0x1616
0x1600:  0E 94 20 10       call 0x2040
0x1604:  10 F4             brbc 0, .+2
0x1606:  0C 94 0A 0B       jmp 0x1614
0x160A:  E0 E0             ldi r30, 0x00
0x160C:  E0 93 7C 01       sts 0x017C, r30
0x1610:  E0 93 79 01       sts 0x0179, r30
0x1614:  03 C0             rjmp 0x161C
0x1616:  E0 E0             ldi r30, 0x00
0x1618:  E0 93 79 01       sts 0x0179, r30
0x161C:  E0 91 7D 01       lds r30, 0x017D
0x1620:  E0 30             cpi r30, 0x00
0x1622:  11 F0             brbs 1, .+2
0x1624:  0C 94 2C 0B       jmp 0x1658
0x1628:  0E 94 3A 0E       call 0x1C74
0x162C:  E8 70             andi r30, 0x08
0x162E:  0E 94 3F 0E       call 0x1C7E
0x1632:  11 F0             brbs 1, .+2
0x1634:  0C 94 28 0B       jmp 0x1650
0x1638:  0E 94 29 10       call 0x2052
0x163C:  10 F4             brbc 0, .+2
0x163E:  0C 94 27 0B       jmp 0x164E
0x1642:  E1 E0             ldi r30, 0x01
0x1644:  E0 93 7D 01       sts 0x017D, r30
0x1648:  E0 E0             ldi r30, 0x00
0x164A:  E0 93 7B 01       sts 0x017B, r30
0x164E:  03 C0             rjmp 0x1656
0x1650:  E0 E0             ldi r30, 0x00
0x1652:  E0 93 7B 01       sts 0x017B, r30
0x1656:  16 C0             rjmp 0x1684
0x1658:  0E 94 3A 0E       call 0x1C74
0x165C:  E8 70             andi r30, 0x08
0x165E:  0E 94 3F 0E       call 0x1C7E
0x1662:  11 F4             brbc 1, .+2
0x1664:  0C 94 3F 0B       jmp 0x167E
0x1668:  0E 94 29 10       call 0x2052
0x166C:  10 F4             brbc 0, .+2
0x166E:  0C 94 3E 0B       jmp 0x167C
0x1672:  E0 E0             ldi r30, 0x00
0x1674:  E0 93 7D 01       sts 0x017D, r30
0x1678:  E0 93 7B 01       sts 0x017B, r30
0x167C:  03 C0             rjmp 0x1684
0x167E:  E0 E0             ldi r30, 0x00
0x1680:  E0 93 7B 01       sts 0x017B, r30
0x1684:  08 95             ret
0x1686:  E0 E0             ldi r30, 0x00
0x1688:  EA BB             out 0x1A, r30
0x168A:  E9 B3             in r30, 0x19
0x168C:  E0 93 75 01       sts 0x0175, r30
0x1690:  08 95             ret
0x1692:  EF EF             ldi r30, 0xFF
0x1694:  E1 BB             out 0x11, r30
0x1696:  87 E0             ldi r24, 0x07
0x1698:  8A 95             dec r24
0x169A:  F1 F7             brbc 1, .-2
0x169C:  E2 BB             out 0x12, r30
0x169E:  C2 9A             sbi 0x18, 2
0x16A0:  C3 9A             sbi 0x18, 3
0x16A2:  C4 9A             sbi 0x18, 4
0x16A4:  E0 91 A4 01       lds r30, 0x01A4
0x16A8:  E0 30             cpi r30, 0x00
0x16AA:  11 F0             brbs 1, .+2
0x16AC:  0C 94 5C 0B       jmp 0x16B8
0x16B0:  0E 94 D6 0B       call 0x17AC
0x16B4:  C2 98             cbi 0x18, 2
0x16B6:  10 C0             rjmp 0x16D8
0x16B8:  E1 30             cpi r30, 0x01
0x16BA:  11 F0             brbs 1, .+2
0x16BC:  0C 94 64 0B       jmp 0x16C8
0x16C0:  0E 94 E2 0B       call 0x17C4
0x16C4:  C3 98             cbi 0x18, 3
0x16C6:  08 C0             rjmp 0x16D8
0x16C8:  E2 30             cpi r30, 0x02
0x16CA:  11 F0             brbs 1, .+2
0x16CC:  0C 94 6C 0B       jmp 0x16D8
0x16D0:  0E 94 E7 0B       call 0x17CE
0x16D4:  C4 98             cbi 0x18, 4
0x16D6:  00 C0             rjmp 0x16D8
0x16D8:  E0 91 A4 01       lds r30, 0x01A4
0x16DC:  EF 5F             subi r30, 0xFF
0x16DE:  E0 93 A4 01       sts 0x01A4, r30
0x16E2:  A0 91 A4 01       lds r26, 0x01A4
0x16E6:  A3 30             cpi r26, 0x03
0x16E8:  10 F4             brbc 0, .+2
0x16EA:  0C 94 7A 0B       jmp 0x16F4
0x16EE:  E0 E0             ldi r30, 0x00
0x16F0:  E0 93 A4 01       sts 0x01A4, r30
0x16F4:  08 95             ret
0x16F6:  AA 93             st -Y, r26
0x16F8:  1A 93             st -Y, r17
0x16FA:  0A 93             st -Y, r16
0x16FC:  10 E0             ldi r17, 0x00
0x16FE:  00 E0             ldi r16, 0x00
0x1700:  EA 81             ldd r30, Y+2
0x1702:  E1 70             andi r30, 0x01
0x1704:  1E 2F             mov r17, r30
0x1706:  11 30             cpi r17, 0x01
0x1708:  11 F0             brbs 1, .+2
0x170A:  0C 94 89 0B       jmp 0x1712
0x170E:  0E 7F             andi r16, 0xFE
0x1710:  01 C0             rjmp 0x1714
0x1712:  01 60             ori r16, 0x01
0x1714:  EA 81             ldd r30, Y+2
0x1716:  E2 70             andi r30, 0x02
0x1718:  1E 2F             mov r17, r30
0x171A:  12 30             cpi r17, 0x02
0x171C:  11 F0             brbs 1, .+2
0x171E:  0C 94 93 0B       jmp 0x1726
0x1722:  0D 7F             andi r16, 0xFD
0x1724:  01 C0             rjmp 0x1728
0x1726:  02 60             ori r16, 0x02
0x1728:  EA 81             ldd r30, Y+2
0x172A:  E4 70             andi r30, 0x04
0x172C:  1E 2F             mov r17, r30
0x172E:  14 30             cpi r17, 0x04
0x1730:  11 F0             brbs 1, .+2
0x1732:  0C 94 9D 0B       jmp 0x173A
0x1736:  0B 7F             andi r16, 0xFB
0x1738:  01 C0             rjmp 0x173C
0x173A:  04 60             ori r16, 0x04
0x173C:  EA 81             ldd r30, Y+2
0x173E:  E8 70             andi r30, 0x08
0x1740:  1E 2F             mov r17, r30
0x1742:  18 30             cpi r17, 0x08
0x1744:  11 F0             brbs 1, .+2
0x1746:  0C 94 A7 0B       jmp 0x174E
0x174A:  07 7F             andi r16, 0xF7
0x174C:  01 C0             rjmp 0x1750
0x174E:  08 60             ori r16, 0x08
0x1750:  EA 81             ldd r30, Y+2
0x1752:  E0 71             andi r30, 0x10
0x1754:  1E 2F             mov r17, r30
0x1756:  10 31             cpi r17, 0x10
0x1758:  11 F0             brbs 1, .+2
0x175A:  0C 94 B1 0B       jmp 0x1762
0x175E:  0F 7E             andi r16, 0xEF
0x1760:  01 C0             rjmp 0x1764
0x1762:  00 61             ori r16, 0x10
0x1764:  EA 81             ldd r30, Y+2
0x1766:  E0 72             andi r30, 0x20
0x1768:  1E 2F             mov r17, r30
0x176A:  10 32             cpi r17, 0x20
0x176C:  11 F0             brbs 1, .+2
0x176E:  0C 94 BB 0B       jmp 0x1776
0x1772:  0F 7D             andi r16, 0xDF
0x1774:  01 C0             rjmp 0x1778
0x1776:  00 62             ori r16, 0x20
0x1778:  EA 81             ldd r30, Y+2
0x177A:  E0 74             andi r30, 0x40
0x177C:  1E 2F             mov r17, r30
0x177E:  10 34             cpi r17, 0x40
0x1780:  11 F0             brbs 1, .+2
0x1782:  0C 94 C5 0B       jmp 0x178A
0x1786:  0F 7B             andi r16, 0xBF
0x1788:  01 C0             rjmp 0x178C
0x178A:  00 64             ori r16, 0x40
0x178C:  EA 81             ldd r30, Y+2
0x178E:  E0 78             andi r30, 0x80
0x1790:  1E 2F             mov r17, r30
0x1792:  10 38             cpi r17, 0x80
0x1794:  11 F0             brbs 1, .+2
0x1796:  0C 94 CF 0B       jmp 0x179E
0x179A:  0F 77             andi r16, 0x7F
0x179C:  01 C0             rjmp 0x17A0
0x179E:  00 68             ori r16, 0x80
0x17A0:  00 93 D3 01       sts 0x01D3, r16
0x17A4:  19 81             ldd r17, Y+1
0x17A6:  08 81             ld r16, Y
0x17A8:  23 96             adiw r28, 0x03
0x17AA:  08 95             ret
0x17AC:  E0 91 9F 01       lds r30, 0x019F
0x17B0:  E1 60             ori r30, 0x01
0x17B2:  E0 93 9F 01       sts 0x019F, r30
0x17B6:  E0 93 9F 01       sts 0x019F, r30
0x17BA:  A0 91 9F 01       lds r26, 0x019F
0x17BE:  0E 94 32 10       call 0x2064
0x17C2:  08 95             ret
0x17C4:  A0 91 A0 01       lds r26, 0x01A0
0x17C8:  0E 94 32 10       call 0x2064
0x17CC:  08 95             ret
0x17CE:  0E 94 FC 0D       call 0x1BF8
0x17D2:  11 F0             brbs 1, .+2
0x17D4:  0C 94 86 0C       jmp 0x190C
0x17D8:  E0 91 9C 01       lds r30, 0x019C
0x17DC:  E0 30             cpi r30, 0x00
0x17DE:  11 F4             brbc 1, .+2
0x17E0:  0C 94 F7 0B       jmp 0x17EE
0x17E4:  0E 94 38 10       call 0x2070
0x17E8:  0E 94 3E 10       call 0x207C
0x17EC:  2D C0             rjmp 0x1848
0x17EE:  E0 91 C5 01       lds r30, 0x01C5
0x17F2:  E0 30             cpi r30, 0x00
0x17F4:  11 F4             brbc 1, .+2
0x17F6:  0C 94 24 0C       jmp 0x1848
0x17FA:  E0 91 C9 01       lds r30, 0x01C9
0x17FE:  E0 30             cpi r30, 0x00
0x1800:  11 F0             brbs 1, .+2
0x1802:  0C 94 14 0C       jmp 0x1828
0x1806:  E0 91 A1 01       lds r30, 0x01A1
0x180A:  EF 77             andi r30, 0x7F
0x180C:  0E 94 41 10       call 0x2082
0x1810:  0E 94 46 10       call 0x208C
0x1814:  10 F4             brbc 0, .+2
0x1816:  0C 94 13 0C       jmp 0x1826
0x181A:  E0 E0             ldi r30, 0x00
0x181C:  E0 93 CA 01       sts 0x01CA, r30
0x1820:  E1 E0             ldi r30, 0x01
0x1822:  E0 93 C9 01       sts 0x01C9, r30
0x1826:  10 C0             rjmp 0x1848
0x1828:  0E 94 38 10       call 0x2070
0x182C:  E0 93 A1 01       sts 0x01A1, r30
0x1830:  0E 94 3E 10       call 0x207C
0x1834:  0E 94 46 10       call 0x208C
0x1838:  10 F4             brbc 0, .+2
0x183A:  0C 94 24 0C       jmp 0x1848
0x183E:  E0 E0             ldi r30, 0x00
0x1840:  E0 93 CA 01       sts 0x01CA, r30
0x1844:  E0 93 C9 01       sts 0x01C9, r30
0x1848:  0E 94 3E 10       call 0x207C
0x184C:  E0 91 84 01       lds r30, 0x0184
0x1850:  F0 91 85 01       lds r31, 0x0185
0x1854:  30 97             sbiw r30, 0x00
0x1856:  11 F0             brbs 1, .+2
0x1858:  0C 94 5F 0C       jmp 0x18BE
0x185C:  E0 91 C6 01       lds r30, 0x01C6
0x1860:  E0 30             cpi r30, 0x00
0x1862:  11 F4             brbc 1, .+2
0x1864:  0C 94 39 0C       jmp 0x1872
0x1868:  E0 91 A1 01       lds r30, 0x01A1
0x186C:  E8 60             ori r30, 0x08
0x186E:  0E 94 41 10       call 0x2082
0x1872:  E0 91 C7 01       lds r30, 0x01C7
0x1876:  E0 30             cpi r30, 0x00
0x1878:  11 F4             brbc 1, .+2
0x187A:  0C 94 44 0C       jmp 0x1888
0x187E:  E0 91 A1 01       lds r30, 0x01A1
0x1882:  E4 60             ori r30, 0x04
0x1884:  0E 94 41 10       call 0x2082
0x1888:  E0 91 C8 01       lds r30, 0x01C8
0x188C:  E0 30             cpi r30, 0x00
0x188E:  11 F4             brbc 1, .+2
0x1890:  0C 94 5E 0C       jmp 0x18BC
0x1894:  E0 91 A1 01       lds r30, 0x01A1
0x1898:  E2 60             ori r30, 0x02
0x189A:  0E 94 4F 10       call 0x209E
0x189E:  A9 9B             sbis 0x15, 1
0x18A0:  06 C0             rjmp 0x18AE
0x18A2:  E0 91 A1 01       lds r30, 0x01A1
0x18A6:  E0 62             ori r30, 0x20
0x18A8:  0E 94 4F 10       call 0x209E
0x18AC:  05 C0             rjmp 0x18B8
0x18AE:  E0 91 A1 01       lds r30, 0x01A1
0x18B2:  EF 7D             andi r30, 0xDF
0x18B4:  0E 94 4F 10       call 0x209E
0x18B8:  0E 94 3E 10       call 0x207C
0x18BC:  26 C0             rjmp 0x190A
0x18BE:  E0 91 D2 01       lds r30, 0x01D2
0x18C2:  E0 30             cpi r30, 0x00
0x18C4:  11 F0             brbs 1, .+2
0x18C6:  0C 94 76 0C       jmp 0x18EC
0x18CA:  E0 91 A1 01       lds r30, 0x01A1
0x18CE:  E3 7F             andi r30, 0xF3
0x18D0:  0E 94 41 10       call 0x2082
0x18D4:  0E 94 54 10       call 0x20A8
0x18D8:  10 F4             brbc 0, .+2
0x18DA:  0C 94 75 0C       jmp 0x18EA
0x18DE:  E0 E0             ldi r30, 0x00
0x18E0:  E0 93 D1 01       sts 0x01D1, r30
0x18E4:  E1 E0             ldi r30, 0x01
0x18E6:  E0 93 D2 01       sts 0x01D2, r30
0x18EA:  0F C0             rjmp 0x190A
0x18EC:  E0 91 A1 01       lds r30, 0x01A1
0x18F0:  EE 60             ori r30, 0x0E
0x18F2:  0E 94 41 10       call 0x2082
0x18F6:  0E 94 54 10       call 0x20A8
0x18FA:  10 F4             brbc 0, .+2
0x18FC:  0C 94 85 0C       jmp 0x190A
0x1900:  E0 E0             ldi r30, 0x00
0x1902:  E0 93 D1 01       sts 0x01D1, r30
0x1906:  E0 93 D2 01       sts 0x01D2, r30
0x190A:  2F C0             rjmp 0x196A
0x190C:  E0 91 9C 01       lds r30, 0x019C
0x1910:  E0 30             cpi r30, 0x00
0x1912:  11 F4             brbc 1, .+2
0x1914:  0C 94 92 0C       jmp 0x1924
0x1918:  0E 94 38 10       call 0x2070
0x191C:  E0 93 A1 01       sts 0x01A1, r30
0x1920:  0E 94 3E 10       call 0x207C
0x1924:  E0 91 97 01       lds r30, 0x0197
0x1928:  E0 30             cpi r30, 0x00
0x192A:  11 F0             brbs 1, .+2
0x192C:  0C 94 9F 0C       jmp 0x193E
0x1930:  E0 91 9E 01       lds r30, 0x019E
0x1934:  E0 30             cpi r30, 0x00
0x1936:  11 F0             brbs 1, .+2
0x1938:  0C 94 9F 0C       jmp 0x193E
0x193C:  10 C0             rjmp 0x195E
0x193E:  A9 9B             sbis 0x15, 1
0x1940:  06 C0             rjmp 0x194E
0x1942:  E0 91 A1 01       lds r30, 0x01A1
0x1946:  E2 62             ori r30, 0x22
0x1948:  0E 94 4F 10       call 0x209E
0x194C:  05 C0             rjmp 0x1958
0x194E:  E0 91 A1 01       lds r30, 0x01A1
0x1952:  ED 7D             andi r30, 0xDD
0x1954:  0E 94 4F 10       call 0x209E
0x1958:  0E 94 3E 10       call 0x207C
0x195C:  02 C0             rjmp 0x1962
0x195E:  0E 94 3E 10       call 0x207C
0x1962:  0E 94 3E 10       call 0x207C
0x1966:  0E 94 3E 10       call 0x207C
0x196A:  08 95             ret
0x196C:  AA 20             and r10, r10
0x196E:  11 F0             brbs 1, .+2
0x1970:  0C 94 F4 0C       jmp 0x19E8
0x1974:  E0 91 60 01       lds r30, 0x0160
0x1978:  F0 91 61 01       lds r31, 0x0161
0x197C:  0E 94 5D 10       call 0x20BA
0x1980:  0E 94 68 10       call 0x20D0
0x1984:  0E 94 71 10       call 0x20E2
0x1988:  E0 93 A9 01       sts 0x01A9, r30
0x198C:  F0 93 AA 01       sts 0x01AA, r31
0x1990:  60 93 AB 01       sts 0x01AB, r22
0x1994:  70 93 AC 01       sts 0x01AC, r23
0x1998:  AB EB             ldi r26, 0xBB
0x199A:  B1 E0             ldi r27, 0x01
0x199C:  0E 94 F6 0D       call 0x1BEC
0x19A0:  A0 91 BB 01       lds r26, 0x01BB
0x19A4:  B0 91 BC 01       lds r27, 0x01BC
0x19A8:  1A 97             sbiw r26, 0x0A
0x19AA:  10 F4             brbc 0, .+2
0x19AC:  0C 94 F3 0C       jmp 0x19E6
0x19B0:  0E 94 68 10       call 0x20D0
0x19B4:  0E 94 7C 10       call 0x20F8
0x19B8:  EA E0             ldi r30, 0x0A
0x19BA:  F0 E0             ldi r31, 0x00
0x19BC:  60 E0             ldi r22, 0x00
0x19BE:  70 E0             ldi r23, 0x00
0x19C0:  0E 94 8D 10       call 0x211A
0x19C4:  E0 93 B9 01       sts 0x01B9, r30
0x19C8:  F0 93 BA 01       sts 0x01BA, r31
0x19CC:  E0 E0             ldi r30, 0x00
0x19CE:  E0 93 A9 01       sts 0x01A9, r30
0x19D2:  E0 93 AA 01       sts 0x01AA, r30
0x19D6:  E0 93 AB 01       sts 0x01AB, r30
0x19DA:  E0 93 AC 01       sts 0x01AC, r30
0x19DE:  E0 93 BB 01       sts 0x01BB, r30
0x19E2:  E0 93 BC 01       sts 0x01BC, r30
0x19E6:  39 C0             rjmp 0x1A5A
0x19E8:  E0 91 62 01       lds r30, 0x0162
0x19EC:  F0 91 63 01       lds r31, 0x0163
0x19F0:  0E 94 5D 10       call 0x20BA
0x19F4:  0E 94 94 10       call 0x2128
0x19F8:  0E 94 71 10       call 0x20E2
0x19FC:  E0 93 AD 01       sts 0x01AD, r30
0x1A00:  F0 93 AE 01       sts 0x01AE, r31
0x1A04:  60 93 AF 01       sts 0x01AF, r22
0x1A08:  70 93 B0 01       sts 0x01B0, r23
0x1A0C:  AD EB             ldi r26, 0xBD
0x1A0E:  B1 E0             ldi r27, 0x01
0x1A10:  0E 94 F6 0D       call 0x1BEC
0x1A14:  A0 91 BD 01       lds r26, 0x01BD
0x1A18:  B0 91 BE 01       lds r27, 0x01BE
0x1A1C:  D2 97             sbiw r26, 0x32
0x1A1E:  10 F4             brbc 0, .+2
0x1A20:  0C 94 2D 0D       jmp 0x1A5A
0x1A24:  0E 94 94 10       call 0x2128
0x1A28:  0E 94 7C 10       call 0x20F8
0x1A2C:  E2 E3             ldi r30, 0x32
0x1A2E:  F0 E0             ldi r31, 0x00
0x1A30:  60 E0             ldi r22, 0x00
0x1A32:  70 E0             ldi r23, 0x00
0x1A34:  0E 94 8D 10       call 0x211A
0x1A38:  E0 93 8D 01       sts 0x018D, r30
0x1A3C:  F0 93 8E 01       sts 0x018E, r31
0x1A40:  E0 E0             ldi r30, 0x00
0x1A42:  E0 93 AD 01       sts 0x01AD, r30
0x1A46:  E0 93 AE 01       sts 0x01AE, r30
0x1A4A:  E0 93 AF 01       sts 0x01AF, r30
0x1A4E:  E0 93 B0 01       sts 0x01B0, r30
0x1A52:  E0 93 BD 01       sts 0x01BD, r30
0x1A56:  E0 93 BE 01       sts 0x01BE, r30
0x1A5A:  08 95             ret
0x1A5C:  A0 91 BF 01       lds r26, 0x01BF
0x1A60:  B0 91 C0 01       lds r27, 0x01C0
0x1A64:  1B 97             sbiw r26, 0x0B
0x1A66:  10 F4             brbc 0, .+2
0x1A68:  0C 94 68 0D       jmp 0x1AD0
0x1A6C:  EB E0             ldi r30, 0x0B
0x1A6E:  F0 E0             ldi r31, 0x00
0x1A70:  E0 93 BF 01       sts 0x01BF, r30
0x1A74:  F0 93 C0 01       sts 0x01C0, r31
0x1A78:  E0 91 B9 01       lds r30, 0x01B9
0x1A7C:  F0 91 BA 01       lds r31, 0x01BA
0x1A80:  E0 93 C1 01       sts 0x01C1, r30
0x1A84:  F0 93 C2 01       sts 0x01C2, r31
0x1A88:  0E 94 9D 10       call 0x213A
0x1A8C:  A8 3E             cpi r26, 0xE8
0x1A8E:  E3 E0             ldi r30, 0x03
0x1A90:  BE 07             .word 0x07BE
0x1A92:  10 F4             brbc 0, .+2
0x1A94:  0C 94 53 0D       jmp 0x1AA6
0x1A98:  E8 EE             ldi r30, 0xE8
0x1A9A:  F3 E0             ldi r31, 0x03
0x1A9C:  E0 93 B5 01       sts 0x01B5, r30
0x1AA0:  F0 93 B6 01       sts 0x01B6, r31
0x1AA4:  15 C0             rjmp 0x1AD0
0x1AA6:  0E 94 9D 10       call 0x213A
0x1AAA:  13 97             sbiw r26, 0x03
0x1AAC:  10 F0             brbs 0, .+2
0x1AAE:  0C 94 5F 0D       jmp 0x1ABE
0x1AB2:  E0 E0             ldi r30, 0x00
0x1AB4:  E0 93 B5 01       sts 0x01B5, r30
0x1AB8:  E0 93 B6 01       sts 0x01B6, r30
0x1ABC:  09 C0             rjmp 0x1AD0
0x1ABE:  0E 94 9D 10       call 0x213A
0x1AC2:  E6 E0             ldi r30, 0x06
0x1AC4:  0E 94 AC 10       call 0x2158
0x1AC8:  E0 93 B5 01       sts 0x01B5, r30
0x1ACC:  F0 93 B6 01       sts 0x01B6, r31
0x1AD0:  08 95             ret
0x1AD2:  E0 E0             ldi r30, 0x00
0x1AD4:  EA BB             out 0x1A, r30
0x1AD6:  EC EF             ldi r30, 0xFC
0x1AD8:  EB BB             out 0x1B, r30
0x1ADA:  EF EF             ldi r30, 0xFF
0x1ADC:  E7 BB             out 0x17, r30
0x1ADE:  EF ED             ldi r30, 0xDF
0x1AE0:  E8 BB             out 0x18, r30
0x1AE2:  E3 EC             ldi r30, 0xC3
0x1AE4:  E4 BB             out 0x14, r30
0x1AE6:  EC EB             ldi r30, 0xBC
0x1AE8:  E5 BB             out 0x15, r30
0x1AEA:  EF EF             ldi r30, 0xFF
0x1AEC:  E1 BB             out 0x11, r30
0x1AEE:  E2 BB             out 0x12, r30
0x1AF0:  E0 E0             ldi r30, 0x00
0x1AF2:  E2 BD             out 0x22, r30
0x1AF4:  E5 E0             ldi r30, 0x05
0x1AF6:  E3 BF             out 0x33, r30
0x1AF8:  EB E9             ldi r30, 0x9B
0x1AFA:  E2 BF             out 0x32, r30
0x1AFC:  E0 E0             ldi r30, 0x00
0x1AFE:  EC BF             out 0x3C, r30
0x1B00:  EF BD             out 0x2F, r30
0x1B02:  E5 E0             ldi r30, 0x05
0x1B04:  EE BD             out 0x2E, r30
0x1B06:  EF EF             ldi r30, 0xFF
0x1B08:  ED BD             out 0x2D, r30
0x1B0A:  E1 EB             ldi r30, 0xB1
0x1B0C:  EC BD             out 0x2C, r30
0x1B0E:  E0 E0             ldi r30, 0x00
0x1B10:  EB BD             out 0x2B, r30
0x1B12:  EA BD             out 0x2A, r30
0x1B14:  E9 BD             out 0x29, r30
0x1B16:  E8 BD             out 0x28, r30
0x1B18:  E5 BD             out 0x25, r30
0x1B1A:  E4 BD             out 0x24, r30
0x1B1C:  E3 BD             out 0x23, r30
0x1B1E:  E5 E0             ldi r30, 0x05
0x1B20:  E9 BF             out 0x39, r30
0x1B22:  EB B7             in r30, 0x3B
0x1B24:  EB BF             out 0x3B, r30
0x1B26:  E0 E0             ldi r30, 0x00
0x1B28:  E5 BF             out 0x35, r30
0x1B2A:  E4 BF             out 0x34, r30
0x1B2C:  EA BF             out 0x3A, r30
0x1B2E:  EB B9             out 0x0B, r30
0x1B30:  EA B9             out 0x0A, r30
0x1B32:  E0 BD             out 0x20, r30
0x1B34:  E0 BD             out 0x20, r30
0x1B36:  E9 B9             out 0x09, r30
0x1B38:  ED B9             out 0x0D, r30
0x1B3A:  EE B9             out 0x0E, r30
0x1B3C:  E0 E8             ldi r30, 0x80
0x1B3E:  E8 B9             out 0x08, r30
0x1B40:  E0 E0             ldi r30, 0x00
0x1B42:  E0 BF             out 0x30, r30
0x1B44:  E0 E4             ldi r30, 0x40
0x1B46:  E7 B9             out 0x07, r30
0x1B48:  ED EC             ldi r30, 0xCD
0x1B4A:  E6 B9             out 0x06, r30
0x1B4C:  0E 94 FC 0D       call 0x1BF8
0x1B50:  11 F0             brbs 1, .+2
0x1B52:  0C 94 B0 0D       jmp 0x1B60
0x1B56:  A0 E0             ldi r26, 0x00
0x1B58:  B0 E0             ldi r27, 0x00
0x1B5A:  E1 E0             ldi r30, 0x01
0x1B5C:  0E 94 E7 10       call 0x21CE
0x1B60:  08 95             ret
0x1B62:  0E 94 69 0D       call 0x1AD2
0x1B66:  C5 98             cbi 0x18, 5
0x1B68:  F8 94             cli
0x1B6A:  E0 E0             ldi r30, 0x00
0x1B6C:  E0 93 96 01       sts 0x0196, r30
0x1B70:  0E 94 1A 0E       call 0x1C34
0x1B74:  E0 93 89 01       sts 0x0189, r30
0x1B78:  A9 98             cbi 0x15, 1
0x1B7A:  A8 98             cbi 0x15, 0
0x1B7C:  AE 98             cbi 0x15, 6
0x1B7E:  EF EF             ldi r30, 0xFF
0x1B80:  E2 BB             out 0x12, r30
0x1B82:  C2 98             cbi 0x18, 2
0x1B84:  C3 98             cbi 0x18, 3
0x1B86:  C4 98             cbi 0x18, 4
0x1B88:  E1 E0             ldi r30, 0x01
0x1B8A:  E0 93 95 01       sts 0x0195, r30
0x1B8E:  78 94             sei
0x1B90:  0E 94 13 09       call 0x1226
0x1B94:  FF CF             rjmp 0x1B94
0x1B96:  0A 92             st -Y, r0
0x1B98:  1A 92             st -Y, r1
0x1B9A:  FA 92             st -Y, r15
0x1B9C:  6A 93             st -Y, r22
0x1B9E:  7A 93             st -Y, r23
0x1BA0:  8A 93             st -Y, r24
0x1BA2:  9A 93             st -Y, r25
0x1BA4:  AA 93             st -Y, r26
0x1BA6:  BA 93             st -Y, r27
0x1BA8:  EA 93             st -Y, r30
0x1BAA:  FA 93             st -Y, r31
0x1BAC:  EF B7             in r30, 0x3F
0x1BAE:  EA 93             st -Y, r30
0x1BB0:  08 95             ret
0x1BB2:  E9 91             ld r30, Y+
0x1BB4:  EF BF             out 0x3F, r30
0x1BB6:  F9 91             ld r31, Y+
0x1BB8:  E9 91             ld r30, Y+
0x1BBA:  B9 91             ld r27, Y+
0x1BBC:  A9 91             ld r26, Y+
0x1BBE:  99 91             ld r25, Y+
0x1BC0:  89 91             ld r24, Y+
0x1BC2:  79 91             ld r23, Y+
0x1BC4:  69 91             ld r22, Y+
0x1BC6:  F9 90             ld r15, Y+
0x1BC8:  19 90             ld r1, Y+
0x1BCA:  09 90             ld r0, Y+
0x1BCC:  08 95             ret
0x1BCE:  E0 91 69 01       lds r30, 0x0169
0x1BD2:  EF 5F             subi r30, 0xFF
0x1BD4:  E0 93 69 01       sts 0x0169, r30
0x1BD8:  A0 91 69 01       lds r26, 0x0169
0x1BDC:  A5 30             cpi r26, 0x05
0x1BDE:  08 95             ret
0x1BE0:  E0 E0             ldi r30, 0x00
0x1BE2:  E0 93 6A 01       sts 0x016A, r30
0x1BE6:  E0 93 6B 01       sts 0x016B, r30
0x1BEA:  08 95             ret
0x1BEC:  ED 91             ld r30, X+
0x1BEE:  FD 91             ld r31, X+
0x1BF0:  31 96             adiw r30, 0x01
0x1BF2:  FE 93             st -X, r31
0x1BF4:  EE 93             st -X, r30
0x1BF6:  08 95             ret
0x1BF8:  A0 E0             ldi r26, 0x00
0x1BFA:  B0 E0             ldi r27, 0x00
0x1BFC:  0E 94 DB 10       call 0x21B6
0x1C00:  E0 30             cpi r30, 0x00
0x1C02:  08 95             ret
0x1C04:  A0 E0             ldi r26, 0x00
0x1C06:  B0 E0             ldi r27, 0x00
0x1C08:  E1 E0             ldi r30, 0x01
0x1C0A:  0E 94 E7 10       call 0x21CE
0x1C0E:  E0 93 65 01       sts 0x0165, r30
0x1C12:  E6 CF             rjmp 0x1BE0
0x1C14:  E0 91 81 01       lds r30, 0x0181
0x1C18:  EF 5F             subi r30, 0xFF
0x1C1A:  E0 93 81 01       sts 0x0181, r30
0x1C1E:  A0 91 81 01       lds r26, 0x0181
0x1C22:  08 95             ret
0x1C24:  E1 E0             ldi r30, 0x01
0x1C26:  E0 93 96 01       sts 0x0196, r30
0x1C2A:  08 95             ret
0x1C2C:  E0 E0             ldi r30, 0x00
0x1C2E:  E0 93 81 01       sts 0x0181, r30
0x1C32:  08 95             ret
0x1C34:  E0 E0             ldi r30, 0x00
0x1C36:  E0 93 8A 01       sts 0x018A, r30
0x1C3A:  08 95             ret
0x1C3C:  E0 E0             ldi r30, 0x00
0x1C3E:  E0 93 96 01       sts 0x0196, r30
0x1C42:  08 95             ret
0x1C44:  E0 93 89 01       sts 0x0189, r30
0x1C48:  E1 E0             ldi r30, 0x01
0x1C4A:  E0 93 8A 01       sts 0x018A, r30
0x1C4E:  E2 CF             rjmp 0x1C14
0x1C50:  E0 E0             ldi r30, 0x00
0x1C52:  E0 93 8A 01       sts 0x018A, r30
0x1C56:  EA CF             rjmp 0x1C2C
0x1C58:  E0 91 9B 01       lds r30, 0x019B
0x1C5C:  EF 5F             subi r30, 0xFF
0x1C5E:  E0 93 9B 01       sts 0x019B, r30
0x1C62:  A0 91 9B 01       lds r26, 0x019B
0x1C66:  08 95             ret
0x1C68:  E0 E0             ldi r30, 0x00
0x1C6A:  E0 93 86 01       sts 0x0186, r30
0x1C6E:  E0 93 87 01       sts 0x0187, r30
0x1C72:  08 95             ret
0x1C74:  E0 91 75 01       lds r30, 0x0175
0x1C78:  E0 93 78 01       sts 0x0178, r30
0x1C7C:  08 95             ret
0x1C7E:  E0 93 78 01       sts 0x0178, r30
0x1C82:  E0 30             cpi r30, 0x00
0x1C84:  08 95             ret
0x1C86:  E0 E0             ldi r30, 0x00
0x1C88:  E0 93 89 01       sts 0x0189, r30
0x1C8C:  E0 93 86 01       sts 0x0186, r30
0x1C90:  08 95             ret
0x1C92:  E1 E0             ldi r30, 0x01
0x1C94:  E0 93 96 01       sts 0x0196, r30
0x1C98:  E0 E0             ldi r30, 0x00
0x1C9A:  E0 93 9C 01       sts 0x019C, r30
0x1C9E:  08 95             ret
0x1CA0:  E0 E0             ldi r30, 0x00
0x1CA2:  E0 93 C5 01       sts 0x01C5, r30
0x1CA6:  E0 93 C6 01       sts 0x01C6, r30
0x1CAA:  E0 93 C7 01       sts 0x01C7, r30
0x1CAE:  E0 93 C8 01       sts 0x01C8, r30
0x1CB2:  E0 93 D0 01       sts 0x01D0, r30
0x1CB6:  E0 93 84 01       sts 0x0184, r30
0x1CBA:  E0 93 85 01       sts 0x0185, r30
0x1CBE:  08 95             ret
0x1CC0:  E0 E0             ldi r30, 0x00
0x1CC2:  E0 93 89 01       sts 0x0189, r30
0x1CC6:  08 95             ret
0x1CC8:  E1 E0             ldi r30, 0x01
0x1CCA:  E0 93 C6 01       sts 0x01C6, r30
0x1CCE:  E0 E0             ldi r30, 0x00
0x1CD0:  E0 93 C7 01       sts 0x01C7, r30
0x1CD4:  E0 93 C8 01       sts 0x01C8, r30
0x1CD8:  08 95             ret
0x1CDA:  E0 E0             ldi r30, 0x00
0x1CDC:  E0 93 C5 01       sts 0x01C5, r30
0x1CE0:  E0 93 D0 01       sts 0x01D0, r30
0x1CE4:  E0 93 84 01       sts 0x0184, r30
0x1CE8:  E0 93 85 01       sts 0x0185, r30
0x1CEC:  08 95             ret
0x1CEE:  E0 93 7E 01       sts 0x017E, r30
0x1CF2:  9C CF             rjmp 0x1C2C
0x1CF4:  E0 93 7F 01       sts 0x017F, r30
0x1CF8:  E0 E0             ldi r30, 0x00
0x1CFA:  E0 93 80 01       sts 0x0180, r30
0x1CFE:  A8 98             cbi 0x15, 0
0x1D00:  AE 98             cbi 0x15, 6
0x1D02:  A9 98             cbi 0x15, 1
0x1D04:  E0 93 9C 01       sts 0x019C, r30
0x1D08:  E0 93 A3 01       sts 0x01A3, r30
0x1D0C:  E0 93 86 01       sts 0x0186, r30
0x1D10:  E0 93 87 01       sts 0x0187, r30
0x1D14:  08 95             ret
0x1D16:  E0 91 94 01       lds r30, 0x0194
0x1D1A:  EF 5F             subi r30, 0xFF
0x1D1C:  E0 93 94 01       sts 0x0194, r30
0x1D20:  A0 91 94 01       lds r26, 0x0194
0x1D24:  A6 30             cpi r26, 0x06
0x1D26:  08 95             ret
0x1D28:  E1 E0             ldi r30, 0x01
0x1D2A:  E0 93 C7 01       sts 0x01C7, r30
0x1D2E:  E0 93 8A 01       sts 0x018A, r30
0x1D32:  E0 E0             ldi r30, 0x00
0x1D34:  E0 93 94 01       sts 0x0194, r30
0x1D38:  E1 E0             ldi r30, 0x01
0x1D3A:  E0 93 93 01       sts 0x0193, r30
0x1D3E:  08 95             ret
0x1D40:  E1 E0             ldi r30, 0x01
0x1D42:  E0 93 C5 01       sts 0x01C5, r30
0x1D46:  A8 9A             sbi 0x15, 0
0x1D48:  08 95             ret
0x1D4A:  E0 91 DA 01       lds r30, 0x01DA
0x1D4E:  F0 91 DB 01       lds r31, 0x01DB
0x1D52:  60 91 DC 01       lds r22, 0x01DC
0x1D56:  70 91 DD 01       lds r23, 0x01DD
0x1D5A:  08 95             ret
0x1D5C:  A0 91 B9 01       lds r26, 0x01B9
0x1D60:  B0 91 BA 01       lds r27, 0x01BA
0x1D64:  88 27             eor r24, r24
0x1D66:  99 27             eor r25, r25
0x1D68:  0E 94 A2 10       call 0x2144
0x1D6C:  E0 93 DA 01       sts 0x01DA, r30
0x1D70:  F0 93 DB 01       sts 0x01DB, r31
0x1D74:  60 93 DC 01       sts 0x01DC, r22
0x1D78:  70 93 DD 01       sts 0x01DD, r23
0x1D7C:  08 95             ret
0x1D7E:  E0 93 DA 01       sts 0x01DA, r30
0x1D82:  F0 93 DB 01       sts 0x01DB, r31
0x1D86:  60 93 DC 01       sts 0x01DC, r22
0x1D8A:  70 93 DD 01       sts 0x01DD, r23
0x1D8E:  A0 91 DA 01       lds r26, 0x01DA
0x1D92:  B0 91 DB 01       lds r27, 0x01DB
0x1D96:  80 91 DC 01       lds r24, 0x01DC
0x1D9A:  90 91 DD 01       lds r25, 0x01DD
0x1D9E:  08 95             ret
0x1DA0:  AF 3F             cpi r26, 0xFF
0x1DA2:  E3 E0             ldi r30, 0x03
0x1DA4:  BE 07             .word 0x07BE
0x1DA6:  E0 E0             ldi r30, 0x00
0x1DA8:  8E 07             .word 0x078E
0x1DAA:  E0 E0             ldi r30, 0x00
0x1DAC:  9E 07             .word 0x079E
0x1DAE:  08 95             ret
0x1DB0:  A0 91 DA 01       lds r26, 0x01DA
0x1DB4:  B0 91 DB 01       lds r27, 0x01DB
0x1DB8:  80 91 DC 01       lds r24, 0x01DC
0x1DBC:  90 91 DD 01       lds r25, 0x01DD
0x1DC0:  08 95             ret
0x1DC2:  EF EF             ldi r30, 0xFF
0x1DC4:  F3 E0             ldi r31, 0x03
0x1DC6:  60 E0             ldi r22, 0x00
0x1DC8:  70 E0             ldi r23, 0x00
0x1DCA:  08 95             ret
0x1DCC:  0E 94 E1 0E       call 0x1DC2
0x1DD0:  0E 94 D7 10       call 0x21AE
0x1DD4:  08 95             ret
0x1DD6:  0E 94 A2 10       call 0x2144
0x1DDA:  E0 93 EE 01       sts 0x01EE, r30
0x1DDE:  F0 93 EF 01       sts 0x01EF, r31
0x1DE2:  60 93 F0 01       sts 0x01F0, r22
0x1DE6:  70 93 F1 01       sts 0x01F1, r23
0x1DEA:  A0 91 EE 01       lds r26, 0x01EE
0x1DEE:  B0 91 EF 01       lds r27, 0x01EF
0x1DF2:  80 91 F0 01       lds r24, 0x01F0
0x1DF6:  90 91 F1 01       lds r25, 0x01F1
0x1DFA:  08 95             ret
0x1DFC:  A0 91 EE 01       lds r26, 0x01EE
0x1E00:  B0 91 EF 01       lds r27, 0x01EF
0x1E04:  80 91 F0 01       lds r24, 0x01F0
0x1E08:  90 91 F1 01       lds r25, 0x01F1
0x1E0C:  08 95             ret
0x1E0E:  0E 94 E1 0E       call 0x1DC2
0x1E12:  0E 94 B2 10       call 0x2164
0x1E16:  08 95             ret
0x1E18:  0E 94 D8 0E       call 0x1DB0
0x1E1C:  0E 94 A2 10       call 0x2144
0x1E20:  E0 93 DA 01       sts 0x01DA, r30
0x1E24:  F0 93 DB 01       sts 0x01DB, r31
0x1E28:  60 93 DC 01       sts 0x01DC, r22
0x1E2C:  70 93 DD 01       sts 0x01DD, r23
0x1E30:  08 95             ret
0x1E32:  E0 91 FA 01       lds r30, 0x01FA
0x1E36:  F0 91 FB 01       lds r31, 0x01FB
0x1E3A:  60 91 FC 01       lds r22, 0x01FC
0x1E3E:  70 91 FD 01       lds r23, 0x01FD
0x1E42:  08 95             ret
0x1E44:  E0 E0             ldi r30, 0x00
0x1E46:  E0 93 EE 01       sts 0x01EE, r30
0x1E4A:  E0 93 EF 01       sts 0x01EF, r30
0x1E4E:  E0 93 F0 01       sts 0x01F0, r30
0x1E52:  E0 93 F1 01       sts 0x01F1, r30
0x1E56:  08 95             ret
0x1E58:  A0 91 FA 01       lds r26, 0x01FA
0x1E5C:  B0 91 FB 01       lds r27, 0x01FB
0x1E60:  80 91 FC 01       lds r24, 0x01FC
0x1E64:  90 91 FD 01       lds r25, 0x01FD
0x1E68:  08 95             ret
0x1E6A:  E0 91 16 02       lds r30, 0x0216
0x1E6E:  F0 91 17 02       lds r31, 0x0217
0x1E72:  60 91 18 02       lds r22, 0x0218
0x1E76:  70 91 19 02       lds r23, 0x0219
0x1E7A:  08 95             ret
0x1E7C:  E0 93 16 02       sts 0x0216, r30
0x1E80:  F0 93 17 02       sts 0x0217, r31
0x1E84:  60 93 18 02       sts 0x0218, r22
0x1E88:  70 93 19 02       sts 0x0219, r23
0x1E8C:  08 95             ret
0x1E8E:  E0 E0             ldi r30, 0x00
0x1E90:  E0 93 FA 01       sts 0x01FA, r30
0x1E94:  E0 93 FB 01       sts 0x01FB, r30
0x1E98:  E0 93 FC 01       sts 0x01FC, r30
0x1E9C:  E0 93 FD 01       sts 0x01FD, r30
0x1EA0:  08 95             ret
0x1EA2:  A0 91 16 02       lds r26, 0x0216
0x1EA6:  B0 91 17 02       lds r27, 0x0217
0x1EAA:  80 91 18 02       lds r24, 0x0218
0x1EAE:  90 91 19 02       lds r25, 0x0219
0x1EB2:  08 95             ret
0x1EB4:  E0 E0             ldi r30, 0x00
0x1EB6:  E0 93 12 02       sts 0x0212, r30
0x1EBA:  E0 93 13 02       sts 0x0213, r30
0x1EBE:  E0 93 14 02       sts 0x0214, r30
0x1EC2:  E0 93 15 02       sts 0x0215, r30
0x1EC6:  08 95             ret
0x1EC8:  A0 91 E6 01       lds r26, 0x01E6
0x1ECC:  B0 91 E7 01       lds r27, 0x01E7
0x1ED0:  80 91 E8 01       lds r24, 0x01E8
0x1ED4:  90 91 E9 01       lds r25, 0x01E9
0x1ED8:  08 95             ret
0x1EDA:  E0 91 02 02       lds r30, 0x0202
0x1EDE:  F0 91 03 02       lds r31, 0x0203
0x1EE2:  60 91 04 02       lds r22, 0x0204
0x1EE6:  70 91 05 02       lds r23, 0x0205
0x1EEA:  08 95             ret
0x1EEC:  E0 93 02 02       sts 0x0202, r30
0x1EF0:  F0 93 03 02       sts 0x0203, r31
0x1EF4:  60 93 04 02       sts 0x0204, r22
0x1EF8:  70 93 05 02       sts 0x0205, r23
0x1EFC:  08 95             ret
0x1EFE:  E0 E0             ldi r30, 0x00
0x1F00:  E0 93 E6 01       sts 0x01E6, r30
0x1F04:  E0 93 E7 01       sts 0x01E7, r30
0x1F08:  E0 93 E8 01       sts 0x01E8, r30
0x1F0C:  E0 93 E9 01       sts 0x01E9, r30
0x1F10:  08 95             ret
0x1F12:  A0 91 02 02       lds r26, 0x0202
0x1F16:  B0 91 03 02       lds r27, 0x0203
0x1F1A:  80 91 04 02       lds r24, 0x0204
0x1F1E:  90 91 05 02       lds r25, 0x0205
0x1F22:  08 95             ret
0x1F24:  E0 E0             ldi r30, 0x00
0x1F26:  E0 93 02 02       sts 0x0202, r30
0x1F2A:  E0 93 03 02       sts 0x0203, r30
0x1F2E:  E0 93 04 02       sts 0x0204, r30
0x1F32:  E0 93 05 02       sts 0x0205, r30
0x1F36:  08 95             ret
0x1F38:  A0 91 0A 02       lds r26, 0x020A
0x1F3C:  B0 91 0B 02       lds r27, 0x020B
0x1F40:  80 91 0C 02       lds r24, 0x020C
0x1F44:  90 91 0D 02       lds r25, 0x020D
0x1F48:  08 95             ret
0x1F4A:  E0 E0             ldi r30, 0x00
0x1F4C:  E0 93 0A 02       sts 0x020A, r30
0x1F50:  E0 93 0B 02       sts 0x020B, r30
0x1F54:  E0 93 0C 02       sts 0x020C, r30
0x1F58:  E0 93 0D 02       sts 0x020D, r30
0x1F5C:  08 95             ret
0x1F5E:  E0 E0             ldi r30, 0x00
0x1F60:  E0 93 DA 01       sts 0x01DA, r30
0x1F64:  E0 93 DB 01       sts 0x01DB, r30
0x1F68:  E0 93 DC 01       sts 0x01DC, r30
0x1F6C:  E0 93 DD 01       sts 0x01DD, r30
0x1F70:  08 95             ret
0x1F72:  E0 93 D6 01       sts 0x01D6, r30
0x1F76:  F0 93 D7 01       sts 0x01D7, r31
0x1F7A:  08 95             ret
0x1F7C:  A1 E9             ldi r26, 0x91
0x1F7E:  B1 E0             ldi r27, 0x01
0x1F80:  35 CE             rjmp 0x1BEC
0x1F82:  A0 91 91 01       lds r26, 0x0191
0x1F86:  B0 91 92 01       lds r27, 0x0192
0x1F8A:  16 97             sbiw r26, 0x06
0x1F8C:  08 95             ret
0x1F8E:  E0 E0             ldi r30, 0x00
0x1F90:  E0 93 91 01       sts 0x0191, r30
0x1F94:  E0 93 92 01       sts 0x0192, r30
0x1F98:  08 95             ret
0x1F9A:  E0 93 91 01       sts 0x0191, r30
0x1F9E:  E0 93 92 01       sts 0x0192, r30
0x1FA2:  E0 E0             ldi r30, 0x00
0x1FA4:  E0 93 97 01       sts 0x0197, r30
0x1FA8:  08 95             ret
0x1FAA:  E0 91 9A 01       lds r30, 0x019A
0x1FAE:  EF 5F             subi r30, 0xFF
0x1FB0:  E0 93 9A 01       sts 0x019A, r30
0x1FB4:  A0 91 9A 01       lds r26, 0x019A
0x1FB8:  08 95             ret
0x1FBA:  E1 E0             ldi r30, 0x01
0x1FBC:  E0 93 96 01       sts 0x0196, r30
0x1FC0:  A8 98             cbi 0x15, 0
0x1FC2:  AE 98             cbi 0x15, 6
0x1FC4:  E0 E0             ldi r30, 0x00
0x1FC6:  E0 93 9A 01       sts 0x019A, r30
0x1FCA:  E1 E0             ldi r30, 0x01
0x1FCC:  E0 93 98 01       sts 0x0198, r30
0x1FD0:  A9 98             cbi 0x15, 1
0x1FD2:  E0 E0             ldi r30, 0x00
0x1FD4:  E0 93 9C 01       sts 0x019C, r30
0x1FD8:  E0 93 A3 01       sts 0x01A3, r30
0x1FDC:  E0 93 86 01       sts 0x0186, r30
0x1FE0:  E0 93 87 01       sts 0x0187, r30
0x1FE4:  08 95             ret
0x1FE6:  E1 E0             ldi r30, 0x01
0x1FE8:  E0 93 9D 01       sts 0x019D, r30
0x1FEC:  A0 91 71 01       lds r26, 0x0171
0x1FF0:  A5 31             cpi r26, 0x15
0x1FF2:  08 95             ret
0x1FF4:  A0 91 73 01       lds r26, 0x0173
0x1FF8:  B0 91 74 01       lds r27, 0x0174
0x1FFC:  08 95             ret
0x1FFE:  E0 93 A0 01       sts 0x01A0, r30
0x2002:  E0 E3             ldi r30, 0x30
0x2004:  E0 93 A1 01       sts 0x01A1, r30
0x2008:  08 95             ret
0x200A:  E0 93 A0 01       sts 0x01A0, r30
0x200E:  E0 E0             ldi r30, 0x00
0x2010:  E0 93 A1 01       sts 0x01A1, r30
0x2014:  08 95             ret
0x2016:  E0 93 A0 01       sts 0x01A0, r30
0x201A:  E3 E0             ldi r30, 0x03
0x201C:  E0 93 A1 01       sts 0x01A1, r30
0x2020:  08 95             ret
0x2022:  EF EF             ldi r30, 0xFF
0x2024:  E0 93 9F 01       sts 0x019F, r30
0x2028:  08 95             ret
0x202A:  A0 91 B5 01       lds r26, 0x01B5
0x202E:  B0 91 B6 01       lds r27, 0x01B6
0x2032:  EA 17             .word 0x17EA
0x2034:  FB 07             .word 0x07FB
0x2036:  08 95             ret
0x2038:  E0 93 9F 01       sts 0x019F, r30
0x203C:  E0 E0             ldi r30, 0x00
0x203E:  E5 CF             rjmp 0x200A
0x2040:  E0 91 79 01       lds r30, 0x0179
0x2044:  EF 5F             subi r30, 0xFF
0x2046:  E0 93 79 01       sts 0x0179, r30
0x204A:  A0 91 79 01       lds r26, 0x0179
0x204E:  A3 33             cpi r26, 0x33
0x2050:  08 95             ret
0x2052:  E0 91 7B 01       lds r30, 0x017B
0x2056:  EF 5F             subi r30, 0xFF
0x2058:  E0 93 7B 01       sts 0x017B, r30
0x205C:  A0 91 7B 01       lds r26, 0x017B
0x2060:  A3 33             cpi r26, 0x33
0x2062:  08 95             ret
0x2064:  0E 94 7B 0B       call 0x16F6
0x2068:  E0 91 D3 01       lds r30, 0x01D3
0x206C:  E2 BB             out 0x12, r30
0x206E:  08 95             ret
0x2070:  E0 91 A1 01       lds r30, 0x01A1
0x2074:  E0 61             ori r30, 0x10
0x2076:  E0 93 A1 01       sts 0x01A1, r30
0x207A:  08 95             ret
0x207C:  A0 91 A1 01       lds r26, 0x01A1
0x2080:  F1 CF             rjmp 0x2064
0x2082:  E0 93 A1 01       sts 0x01A1, r30
0x2086:  E0 93 A1 01       sts 0x01A1, r30
0x208A:  F8 CF             rjmp 0x207C
0x208C:  E0 91 CA 01       lds r30, 0x01CA
0x2090:  EF 5F             subi r30, 0xFF
0x2092:  E0 93 CA 01       sts 0x01CA, r30
0x2096:  A0 91 CA 01       lds r26, 0x01CA
0x209A:  A9 32             cpi r26, 0x29
0x209C:  08 95             ret
0x209E:  E0 93 A1 01       sts 0x01A1, r30
0x20A2:  E0 93 A1 01       sts 0x01A1, r30
0x20A6:  08 95             ret
0x20A8:  E0 91 D1 01       lds r30, 0x01D1
0x20AC:  EF 5F             subi r30, 0xFF
0x20AE:  E0 93 D1 01       sts 0x01D1, r30
0x20B2:  A0 91 D1 01       lds r26, 0x01D1
0x20B6:  A9 32             cpi r26, 0x29
0x20B8:  08 95             ret
0x20BA:  66 27             eor r22, r22
0x20BC:  77 27             eor r23, r23
0x20BE:  E0 93 A5 01       sts 0x01A5, r30
0x20C2:  F0 93 A6 01       sts 0x01A6, r31
0x20C6:  60 93 A7 01       sts 0x01A7, r22
0x20CA:  70 93 A8 01       sts 0x01A8, r23
0x20CE:  08 95             ret
0x20D0:  E0 91 A9 01       lds r30, 0x01A9
0x20D4:  F0 91 AA 01       lds r31, 0x01AA
0x20D8:  60 91 AB 01       lds r22, 0x01AB
0x20DC:  70 91 AC 01       lds r23, 0x01AC
0x20E0:  08 95             ret
0x20E2:  A0 91 A5 01       lds r26, 0x01A5
0x20E6:  B0 91 A6 01       lds r27, 0x01A6
0x20EA:  80 91 A7 01       lds r24, 0x01A7
0x20EE:  90 91 A8 01       lds r25, 0x01A8
0x20F2:  0E 94 A2 10       call 0x2144
0x20F6:  08 95             ret
0x20F8:  E0 93 B1 01       sts 0x01B1, r30
0x20FC:  F0 93 B2 01       sts 0x01B2, r31
0x2100:  60 93 B3 01       sts 0x01B3, r22
0x2104:  70 93 B4 01       sts 0x01B4, r23
0x2108:  A0 91 B1 01       lds r26, 0x01B1
0x210C:  B0 91 B2 01       lds r27, 0x01B2
0x2110:  80 91 B3 01       lds r24, 0x01B3
0x2114:  90 91 B4 01       lds r25, 0x01B4
0x2118:  08 95             ret
0x211A:  0E 94 B2 10       call 0x2164
0x211E:  E0 93 C3 01       sts 0x01C3, r30
0x2122:  F0 93 C4 01       sts 0x01C4, r31
0x2126:  08 95             ret
0x2128:  E0 91 AD 01       lds r30, 0x01AD
0x212C:  F0 91 AE 01       lds r31, 0x01AE
0x2130:  60 91 AF 01       lds r22, 0x01AF
0x2134:  70 91 B0 01       lds r23, 0x01B0
0x2138:  08 95             ret
0x213A:  A0 91 C1 01       lds r26, 0x01C1
0x213E:  B0 91 C2 01       lds r27, 0x01C2
0x2142:  08 95             ret
0x2144:  EA 0F             add r30, r26
0x2146:  FB 1F             adc r31, r27
0x2148:  68 1F             adc r22, r24
0x214A:  79 1F             adc r23, r25
0x214C:  08 95             ret
0x214E:  EE 0F             add r30, r30
0x2150:  FF 1F             adc r31, r31
0x2152:  66 1F             adc r22, r22
0x2154:  77 1F             adc r23, r23
0x2156:  08 95             ret
0x2158:  6E 2F             mov r22, r30
0x215A:  6A 9F             mul r22, r26
0x215C:  F0 01             movw r30, r0
0x215E:  6B 9F             mul r22, r27
0x2160:  F0 0D             add r31, r0
0x2162:  08 95             ret
0x2164:  3F 93             .word 0x933F
0x2166:  4F 93             .word 0x934F
0x2168:  5F 93             .word 0x935F
0x216A:  00 24             eor r0, r0
0x216C:  11 24             eor r1, r1
0x216E:  44 27             eor r20, r20
0x2170:  55 27             eor r21, r21
0x2172:  30 E2             ldi r19, 0x20
0x2174:  AA 0F             add r26, r26
0x2176:  BB 1F             adc r27, r27
0x2178:  88 1F             adc r24, r24
0x217A:  99 1F             adc r25, r25
0x217C:  00 1C             adc r0, r0
0x217E:  11 1C             adc r1, r1
0x2180:  44 1F             adc r20, r20
0x2182:  55 1F             adc r21, r21
0x2184:  0E 1A             sub r0, r30
0x2186:  1F 0A             sbc r1, r31
0x2188:  46 0B             sbc r20, r22
0x218A:  57 0B             sbc r21, r23
0x218C:  28 F4             brbc 0, .+5
0x218E:  0E 0E             add r0, r30
0x2190:  1F 1E             adc r1, r31
0x2192:  46 1F             adc r20, r22
0x2194:  57 1F             adc r21, r23
0x2196:  01 C0             rjmp 0x219A
0x2198:  A1 60             ori r26, 0x01
0x219A:  3A 95             dec r19
0x219C:  59 F7             brbc 1, .-21
0x219E:  FD 01             movw r30, r26
0x21A0:  BC 01             movw r22, r24
0x21A2:  D0 01             movw r26, r0
0x21A4:  CA 01             movw r24, r20
0x21A6:  5F 91             .word 0x915F
0x21A8:  4F 91             .word 0x914F
0x21AA:  3F 91             .word 0x913F
0x21AC:  08 95             ret
0x21AE:  DA DF             rcall 0x2164
0x21B0:  FD 01             movw r30, r26
0x21B2:  BC 01             movw r22, r24
0x21B4:  08 95             ret
0x21B6:  E1 99             sbic 0x1C, 1
0x21B8:  FE CF             rjmp 0x21B6
0x21BA:  FF 93             .word 0x93FF
0x21BC:  FF B7             in r31, 0x3F
0x21BE:  F8 94             cli
0x21C0:  AE BB             out 0x1E, r26
0x21C2:  BF BB             out 0x1F, r27
0x21C4:  E0 9A             sbi 0x1C, 0
0x21C6:  ED B3             in r30, 0x1D
0x21C8:  FF BF             out 0x3F, r31
0x21CA:  FF 91             .word 0x91FF
0x21CC:  08 95             ret
0x21CE:  E1 9B             sbis 0x1C, 1
0x21D0:  02 C0             rjmp 0x21D6
0x21D2:  A8 95             wdr
0x21D4:  FC CF             rjmp 0x21CE
0x21D6:  9F B7             in r25, 0x3F
0x21D8:  F8 94             cli
0x21DA:  AE BB             out 0x1E, r26
0x21DC:  BF BB             out 0x1F, r27
0x21DE:  E0 9A             sbi 0x1C, 0
0x21E0:  8D B3             in r24, 0x1D
0x21E2:  E8 17             .word 0x17E8
0x21E4:  19 F0             brbs 1, .+3
0x21E6:  ED BB             out 0x1D, r30
0x21E8:  E2 9A             sbi 0x1C, 2
0x21EA:  E1 9A             sbi 0x1C, 1
0x21EC:  9F BF             out 0x3F, r25
0x21EE:  08 95             ret
0x21F0:  AC 0F             add r26, r28
0x21F2:  BD 1F             adc r27, r29
0x21F4:  05 90             lpm r0, Z+
0x21F6:  0D 92             st X+, r0
0x21F8:  8A 95             dec r24
0x21FA:  E1 F7             brbc 1, .-4
0x21FC:  08 95             ret