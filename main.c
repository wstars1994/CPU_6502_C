#include <stdio.h>
#include <assert.h>

#define Byte unsigned char
#define byte char
#define Short unsigned short

struct CPU {
    Short PC;
    Byte SP;
    Short Mem[0XFFFF];
    Byte A, X, Y;
    Byte F_N;
    Byte F_V;
    Byte F_B;
    Byte F_D;
    Byte F_I;
    Byte F_Z;
    Byte F_C;
    Byte INS_Cycles;
} CPU;

void CPU_Reset(Short PCAddr) {
    CPU.PC = PCAddr;
    CPU.SP = 0xFD;
    CPU.A = CPU.X = CPU.Y = 0;
    CPU.F_B = 1;
    CPU.F_N = CPU.F_V = CPU.F_D = CPU.F_I = CPU.F_Z = CPU.F_C = 0;
}

Byte CPU_Read_Addr(Short addr) {
    CPU.INS_Cycles += 1;
    return CPU.Mem[addr];
}

Byte CPU_Get_Byte() {
    return CPU_Read_Addr(CPU.PC++);
}

Short concat_byte(Byte low, Byte high) {
    return low | (high << 8);
}

Byte CPU_Write_Addr(Short addr, Byte value) {
    CPU.INS_Cycles += 1;
    return CPU.Mem[addr] = value;
}

//-------------寻址方式开始-----------------
// AM: Addressing Mode

/**
 * 直接寻址
 * @param reg
 * @return
 */
Byte AM_IMM() {

    return CPU_Get_Byte();
}

/**
 * 绝对寻址
 * Absolute: a
 * @param low
 * @param high
 * @return
 */
Short AM_Abs() {
    Byte low = CPU_Get_Byte();
    Byte high = CPU_Get_Byte();
    return concat_byte(low, high);
}

/**
 * 绝对索引寻址
 * Absolute Indexed with REG: a,REG
 * REG = CPU.X/CPU.Y
 * @param low
 * @param high
 * @param reg
 * @return
 */
Short AM_Abs_XY(Byte reg) {
    Short abs = AM_Abs();
    Short absRegAddr = abs + reg;
    //page boundary is crossed
    CPU.INS_Cycles += ((abs ^ absRegAddr) >> 8) > 0;
    return absRegAddr;
}

/**
 * 零页寻址
 * @return
 */
Short AM_ZP() {

    return CPU_Get_Byte();
}

/**
 * 零页索引寻址
 * @param reg
 * @return
 */
Short AM_ZP_XY(Byte reg) {
    CPU.INS_Cycles++;
    return AM_ZP() + reg;
}

/**
 * 零页索引直接寻址 X
 * @return
 */
Short AM_ZP_IND_X() {
    Short low = AM_ZP_XY(CPU.X);
    return concat_byte(CPU_Read_Addr(low), CPU_Read_Addr(low + 1));
}

/**
 * 零页直接索引寻址 Y
 * @return
 */
Short AM_ZP_IND_Y() {
    Short low = AM_ZP();
    Short address_1 = concat_byte(CPU_Read_Addr(low), CPU_Read_Addr(low + 1));
    Short address_2 = address_1 + CPU.Y;
    //page boundary is crossed
    CPU.INS_Cycles += ((address_1 ^ address_2) >> 8) > 0;
    return address_2;
}

//-------------寻址方式结束-----------------

//-------------FLAG设置开始-----------------

void CPU_F_NZ(Byte data) {
    CPU.F_N = data >> 7;
    CPU.F_Z = data == 0;
}

/**
 *  A - M
 *  注意: 实际使用的是 REG_x - input(M) 是减法
 *          	        N	Z	C
 *  Register < Memory	1	0	0
 *  Register = Memory	0	1	1
 *  Register > Memory	0	0	1
 * @param input
 */
void CPU_F_Compare(Byte reg, Byte input) {
    CPU.INS_Cycles += 1;
    Byte res = reg - input;
    CPU_F_NZ(res);
    //unsigned
    CPU.F_C = reg >= input;
}

//-------------FLAG设置结束-----------------

//-------------指令开始-----------------

/**
 * AXY寄存器设置值
 * @param value 值
 * @param reg AXY寄存器
 */
void INS_Set_REG(Byte value, Byte *reg) {
    *reg = value;
    CPU_F_NZ(*reg);
    CPU.INS_Cycles += 1;
}

/**
 * 保存寄存器的值到内存地址
 * @param value 值
 * @param reg AXY寄存器
 */
void INS_REG_To_MEM(Short address, Byte REG_Value) {
    CPU.INS_Cycles += 1;
    CPU_Write_Addr(address, REG_Value);
}

/**
 * 相加保存寄存器A
 * A + M + C -> A
 * Flags: N, V, Z, C
 * @param value M值
 */
void INS_ADC(Byte value) {
    CPU.INS_Cycles += 1;
    //结果为0x80代表是相同符号 返回0代表不同符号 同符号相加减才会溢出/进位
    Byte same_sign = (CPU.A ^ value) >> 7;
    Short sum_value = CPU.A + value + CPU.F_C;
    CPU.A = sum_value & 0xFF;
    CPU_F_NZ(CPU.A);
    //无符号数越界->进位 0-256
    CPU.F_C = sum_value > 0xFF;
    //有符号数越界->溢出 -128-127
    CPU.F_V = same_sign && ((CPU.A ^ value)>>7);
}

/**
 * 相减保存寄存器A
 * A - M - ~C -> A
 * Flags: N, V, Z, C
 * @param value M值
 */
void INS_SBC(Byte value) {
    INS_ADC(~value);
}

/**
 * 内存值加减
 * M + 1 -> M
 * Flags: N, Z
 * @param value M值
 * @param value2 1/-1
 */
void INS_INC_DEC(Short address,byte value) {
    CPU.INS_Cycles += 2;
    Byte data = CPU_Read_Addr(address);
    data = data + value;
    CPU_Write_Addr(address, data);
    CPU_F_NZ(data);
}

/**
 * 内存值加减
 * M + 1 -> M
 * Flags: N, Z
 * @param value M值
 * @param value2 1/-1
 */
void INS_INC_DEC_XY(Byte *REG,byte value) {
    *REG = *REG + value;
    CPU_F_NZ(*REG);
}

/**
 * 算术左移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_ASL(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = value>>7;
    value<<=1;
    CPU_F_NZ(value);
    return value;
}

/**
 * 逻辑右移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_LSR(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = value & 1;
    value>>=1;
    CPU_F_NZ(value);
    return value;
}


/**
 * 循环左移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_ROL(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = (value>>7) & 0x1;
    value<<=1;
    value = CPU.F_C ? value|0x1 : value&0xFE;
    CPU_F_NZ(value);
    return value;
}

/**
 * 循环右移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_ROR(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = value & 0x1;
    value>>=1;
    value = CPU.F_C ? value|0x80 : value&~0x80;
    CPU_F_NZ(value);
    return value;
}

/**
 * 与
 * A & M -> A
 * Flags: N, Z
 * @param value 值
 */
void INS_AND(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.A&=value;
    CPU_F_NZ(CPU.A);
}

/**
 * 或
 * A | M -> A
 * Flags: N, Z
 * @param value 值
 */
void INS_ORA(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.A|=value;
    CPU_F_NZ(CPU.A);
}

/**
 * 异或
 * A ^ M -> A
 * Flags: N, Z
 * @param value 值
 */
void INS_EOR(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.A^=value;
    CPU_F_NZ(CPU.A);
}

/**
 *  N = M7, V = M6, Z = A & M
 * @param input
 */
void INS_BIT(Byte input) {
    CPU.INS_Cycles += 1;
    CPU.F_Z = (CPU.A & input) == 0;
    CPU.F_V = (input>>6)&1;
    CPU.F_N = input >> 7;
}

/**
 *  Branch on Carry Clear
 *  Branch if C = 0
 * @param input
 */
void INS_Branch(byte input,Byte condition) {
    if(condition) {
        CPU.INS_Cycles += (((CPU.PC+input) ^ CPU.PC) >> 8) > 0?2:1;
        CPU.PC += input;
    }
}

/**
 * 寄存器间值转移
 * @param source 源寄存器
 * @param target 目标寄存器
 * @param set_flag 是否要设置状态寄存器
 */
void INS_Transfer(Byte source,Byte *target,Byte set_flag) {
    CPU.INS_Cycles += 2;
    *target = source;
    if(set_flag) {
        CPU_F_NZ(*target);
    }
}

/**
 * 寄存器间值转移
 * @param source 源寄存器
 * @param target 目标寄存器
 * @param set_flag 是否要设置状态寄存器
 */
void INS_SET_CLEAR(Byte *FLAG,Byte value) {
    CPU.INS_Cycles += 2;
    *FLAG = value;
}

//-------------指令结束-----------------

void CPU_Exec() {
    Byte opcode = CPU_Get_Byte();
    CPU.INS_Cycles = 0;
    Short addr;
    switch (opcode) {
        // ------------Load(加载到寄存器)------------
            //LDA #
        case 0xA9:
            INS_Set_REG(AM_IMM(), &CPU.A);
            break;
            //LDA a
        case 0xAD:
            INS_Set_REG(CPU_Read_Addr(AM_Abs()), &CPU.A);
            break;
            //LDA a,x
        case 0xBD:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.X)), &CPU.A);
            break;
            //LDA a,y
        case 0xB9:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.Y)), &CPU.A);
            break;
            //LDA zp
        case 0xA5:
            INS_Set_REG(CPU_Read_Addr(AM_ZP()), &CPU.A);
            break;
            //LDA zp,x
        case 0xB5:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_XY(CPU.X)), &CPU.A);
            break;
            //LDA (Indirect,X)
        case 0xA1:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_IND_X()), &CPU.A);
            break;
            //LDA (Indirect),Y
        case 0xB1:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_IND_Y()), &CPU.A);
            break;

            //LDX #
        case 0xA2:
            INS_Set_REG(AM_IMM(), &CPU.X);
            break;
            //LDX a
        case 0xAE:
            INS_Set_REG(CPU_Read_Addr(AM_Abs()), &CPU.X);
            break;
            //LDX a,y
        case 0xBE:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.Y)), &CPU.X);
            break;
            //LDX zp
        case 0xA6:
            INS_Set_REG(CPU_Read_Addr(AM_ZP()), &CPU.X);
            break;
            //LDX zp,y
        case 0xB6:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_XY(CPU.Y)), &CPU.X);
            break;

            //LDY #
        case 0xA0:
            INS_Set_REG(AM_IMM(), &CPU.Y);
            break;
            //LDY a
        case 0xAC:
            INS_Set_REG(CPU_Read_Addr(AM_Abs()), &CPU.Y);
            break;
            //LDY a,x
        case 0xBC:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.X)), &CPU.Y);
            break;
            //LDY zp
        case 0xA4:
            INS_Set_REG(CPU_Read_Addr(AM_ZP()), &CPU.Y);
            break;
            //LDY zp,x
        case 0xB4:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_XY(CPU.X)), &CPU.Y);
            break;
        // ------------Store(寄存器存储到内存)------------
            //STA a
        case 0x8D:
            INS_REG_To_MEM(AM_Abs(), CPU.A);
            break;
            //STA a,x
        case 0x9D:
            INS_REG_To_MEM(AM_Abs_XY(CPU.X), CPU.A);
            break;
            //STA a,y
        case 0x99:
            INS_REG_To_MEM(AM_Abs_XY(CPU.Y), CPU.A);
            break;
            //STA zp
        case 0x85:
            INS_REG_To_MEM(AM_ZP(), CPU.A);
            break;
            //STA zp,x
        case 0x95:
            INS_REG_To_MEM(AM_ZP_XY(CPU.X), CPU.A);
            break;
            //STA (zp,x)
        case 0x81:
            INS_REG_To_MEM(AM_ZP_IND_X(), CPU.A);
            break;
            //STA (zp),y
        case 0x91:
            INS_REG_To_MEM(AM_ZP_IND_Y(), CPU.A);
            break;

            //STX a
        case 0x8E:
            INS_REG_To_MEM(AM_Abs(), CPU.X);
            break;
            //STX zp
        case 0x86:
            INS_REG_To_MEM(AM_ZP(), CPU.X);
            break;
            //STX zp,y
        case 0x96:
            INS_REG_To_MEM(AM_ZP_XY(CPU.Y), CPU.X);
            break;

            //STY a
        case 0x8C:
            INS_REG_To_MEM(AM_Abs(), CPU.Y);
            break;
            //STY zp
        case 0x84:
            INS_REG_To_MEM(AM_ZP(), CPU.Y);
            break;
            //STY zp,x
        case 0x94:
            INS_REG_To_MEM(AM_ZP_XY(CPU.X), CPU.Y);
            break;
        // ------------Arithmetic(算数)------------
            //ADC #   (Add Memory to Accumulator with Carry)
        case 0x69:
            INS_ADC(AM_IMM());
            break;
            //ADC a
        case 0x6D:
            INS_ADC(CPU_Read_Addr(AM_Abs()));
            break;
            //ADC a,x
        case 0x7D:
            INS_ADC(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //ADC a,y
        case 0x79:
            INS_ADC(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //ADC zp
        case 0x65:
            INS_ADC(CPU_Read_Addr(AM_ZP()));
            break;
            //ADC zp,x
        case 0x75:
            INS_ADC(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //ADC (Indirect,X)
        case 0x61:
            INS_ADC(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //ADC (Indirect),Y
        case 0x71:
            INS_ADC(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

            //SBC #   (Subtract Memory from Accumulator with Borrow)
        case 0xE9:
            INS_SBC(AM_IMM());
            break;
            //SBC a
        case 0xED:
            INS_SBC(CPU_Read_Addr(AM_Abs()));
            break;
            //SBC a,x
        case 0xFD:
            INS_SBC(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //SBC a,y
        case 0xF9:
            INS_SBC(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //SBC zp
        case 0xE5:
            INS_SBC(CPU_Read_Addr(AM_ZP()));
            break;
            //SBC zp,x
        case 0xF5:
            INS_SBC(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //SBC (Indirect,X)
        case 0xE1:
            INS_SBC(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //SBC (Indirect),Y
        case 0xF1:
            INS_SBC(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

        // ------------Increment and Decrement(加减)------------
            //INC a
        case 0xEE:
            INS_INC_DEC(AM_Abs(),1);
            break;
            //INC a,x
        case 0xFE:
            INS_INC_DEC(AM_Abs_XY(CPU.X),1);
            break;
            //INC zp
        case 0xE6:
            INS_INC_DEC(AM_ZP(),1);
            break;
            //INC zp,x
        case 0xF6:
            INS_INC_DEC(AM_ZP_XY(CPU.X),1);
            break;
            //INX #
        case 0xE8:
            INS_INC_DEC_XY(&CPU.X,1);
            break;
            //INY #
        case 0xC8:
            INS_INC_DEC_XY(&CPU.Y,1);
            break;

            //DEC a
        case 0xCE:
            INS_INC_DEC(AM_Abs(),-1);
            break;
            //DEC a,x
        case 0xDE:
            INS_INC_DEC(AM_Abs_XY(CPU.X),-1);
            break;
            //DEC zp
        case 0xC6:
            INS_INC_DEC(AM_ZP(),-1);
            break;
            //DEC zp,x
        case 0xD6:
            INS_INC_DEC(AM_ZP_XY(CPU.X),-1);
            break;
            //DEX #
        case 0xCA:
            INS_INC_DEC_XY(&CPU.X,-1);
            break;
            //DEY #
        case 0x88:
            INS_INC_DEC_XY(&CPU.Y,-1);
            break;

        // ------------Shift and Rotate(位运算与位翻转)------------
            //ASL a
        case 0x0E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;
            //ASL a,x
        case 0x1E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;
            //ASL A
        case 0x0A:
            CPU.A = INS_ASL(CPU.A);
            break;
            //ASL zp
        case 0x06:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;
            //ASL zp,x
        case 0x16:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;

            //LSR a
        case 0x4E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;
            //LSR a,x
        case 0x5E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;
            //LSR A 寄存器
        case 0x4A:
            CPU.A = INS_LSR(CPU.A);
            break;
            //LSR zp
        case 0x46:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;
            //LSR zp,x
        case 0x56:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;

            //ROL a
        case 0x2E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;
            //ROL a,x
        case 0x3E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;
            //ROL A 寄存器
        case 0x2A:
            CPU.A = INS_ROL(CPU.A);
            break;
            //ROL zp
        case 0x26:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;
            //ROL zp,x
        case 0x36:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;

            //ROR a
        case 0x6E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;
            //ROR a,x
        case 0x7E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;
            //ROR A 寄存器
        case 0x6A:
            CPU.A = INS_ROR(CPU.A);
            break;
            //ROR zp
        case 0x66:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;
            //ROR zp,x
        case 0x76:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;

        // ------------Logic(逻辑运算)------------
            //AND a  AND Memory with Accumulator
        case 0x2D:
            INS_AND(CPU_Read_Addr(AM_Abs()));
            break;
            //AND a,x
        case 0x3D:
            INS_AND(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //AND a,y
        case 0x39:
            INS_AND(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //AND #
        case 0x29:
            INS_AND(AM_IMM());
            break;
            //AND zp
        case 0x25:
            INS_AND(CPU_Read_Addr(AM_ZP()));
            break;
            //AND (zp,x)
        case 0x21:
            INS_AND(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //AND zp,x
        case 0x35:
            INS_AND(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //AND (zp),y
        case 0x31:
            INS_AND(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

            //ORA a  OR Memory with Accumulator
        case 0x0D:
            INS_ORA(CPU_Read_Addr(AM_Abs()));
            break;
            //ORA a,x
        case 0x1D:
            INS_ORA(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //ORA a,y
        case 0x19:
            INS_ORA(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //ORA #
        case 0x09:
            INS_ORA(AM_IMM());
            break;
            //ORA zp
        case 0x05:
            INS_ORA(CPU_Read_Addr(AM_ZP()));
            break;
            //ORA (zp,x)
        case 0x01:
            INS_ORA(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //ORA zp,x
        case 0x15:
            INS_ORA(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //ORA (zp),y
        case 0x11:
            INS_ORA(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

            //EOR a   Exclusive-OR Memory with Accumulator
        case 0x4D:
            INS_EOR(CPU_Read_Addr(AM_Abs()));
            break;
            //EOR a,x
        case 0x5D:
            INS_EOR(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //EOR a,y
        case 0x59:
            INS_EOR(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //EOR #
        case 0x49:
            INS_EOR(AM_IMM());
            break;
            //EOR zp
        case 0x45:
            INS_EOR(CPU_Read_Addr(AM_ZP()));
            break;
            //EOR (zp,x)
        case 0x41:
            INS_EOR(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //EOR zp,x
        case 0x55:
            INS_EOR(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //EOR (zp),y
        case 0x51:
            INS_EOR(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

        // ------------Compare and Test Bit(比较和检测位)------------
            //CMP a   Compare Memory and Accumulator
        case 0xCD:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_Abs()));
            break;
            //CMP a,x
        case 0xDD:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //CMP a,y
        case 0xD9:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //CMP #
        case 0xC9:
            CPU_F_Compare(CPU.A, AM_IMM());
            break;
            //CMP zp
        case 0xC5:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_ZP()));
            break;
            //CMP (zp,x)
        case 0xC1:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //CMP zp,x
        case 0xD5:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //CMP (zp),y
        case 0xD1:
            CPU_F_Compare(CPU.A, CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

            //CPX a   Compare Memory and Index X
        case 0xEC:
            CPU_F_Compare(CPU.X, CPU_Read_Addr(AM_Abs()));
            break;
            //CPX #
        case 0xE0:
            CPU_F_Compare(CPU.X, AM_IMM());
            break;
            //CPX zp
        case 0xE4:
            CPU_F_Compare(CPU.X, CPU_Read_Addr(AM_ZP()));
            break;

            //CPY a   Compare Memory and Index X
        case 0xCC:
            CPU_F_Compare(CPU.Y, CPU_Read_Addr(AM_Abs()));
            break;
            //CPY #
        case 0xC0:
            CPU_F_Compare(CPU.Y, AM_IMM());
            break;
            //CPY zp
        case 0xC4:
            CPU_F_Compare(CPU.Y, CPU_Read_Addr(AM_ZP()));
            break;

            //BIT a   Test Bits in Memory with Accumulator
        case 0x2C:
            INS_BIT(CPU_Read_Addr(AM_Abs()));
            break;
            //BIT #
        case 0x89:
            INS_BIT(AM_IMM());
            break;
            //BIT zp
        case 0x24:
            INS_BIT(CPU_Read_Addr(AM_ZP()));
            break;
        default:
            break;

        // ------------Branch(分支跳转)------------
            //BCC r
        case 0x90:
            INS_Branch(AM_IMM(),CPU.F_C == 0);
            break;
            //BCS r
        case 0xB0:
            INS_Branch(AM_IMM(),CPU.F_C == 1);
            break;
            //BNE r
        case 0xD0:
            INS_Branch(AM_IMM(),CPU.F_Z == 0);
            break;
            //BEQ r
        case 0xF0:
            INS_Branch(AM_IMM(),CPU.F_Z == 1);
            break;
            //BPL r
        case 0x10:
            INS_Branch(AM_IMM(),CPU.F_N == 0);
            break;
            //BMI r
        case 0x30:
            INS_Branch(AM_IMM(),CPU.F_N == 1);
            break;
            //BVC r
        case 0x50:
            INS_Branch(AM_IMM(),CPU.F_V == 0);
            break;
            //BVS r
        case 0x70:
            INS_Branch(AM_IMM(),CPU.F_V == 1);
            break;

        // ------------Transfer(转移)------------
            //TAX
        case 0xAA:
            INS_Transfer(CPU.A,&CPU.X,1);
            break;
            //TXA
        case 0x8A:
            INS_Transfer(CPU.X,&CPU.A,1);
            break;
            //TAY
        case 0xA8:
            INS_Transfer(CPU.A,&CPU.Y,1);
            break;
            //TYA
        case 0x98:
            INS_Transfer(CPU.Y,&CPU.A,1);
            break;
            //TSX
        case 0xBA:
            INS_Transfer(CPU.SP,&CPU.X,1);
            break;
            //TXS
        case 0x9A:
            INS_Transfer(CPU.X,&CPU.SP,0);
            break;

        // ------------Set and Clear------------
            //CLC
        case 0x18:
            INS_SET_CLEAR(&CPU.F_C,0);
            break;
            //SEC
        case 0x38:
            INS_SET_CLEAR(&CPU.F_C,1);
            break;
            //CLD
        case 0xD8:
            INS_SET_CLEAR(&CPU.F_D,0);
            break;
            //SED
        case 0xF8:
            INS_SET_CLEAR(&CPU.F_D,1);
            break;
            //CLI
        case 0x58:
            INS_SET_CLEAR(&CPU.F_I,0);
            break;
            //SEI
        case 0x78:
            INS_SET_CLEAR(&CPU.F_I,1);
            break;
            //CLV
        case 0xB8:
            INS_SET_CLEAR(&CPU.F_V,0);
            break;
            //NOP
        case 0xEA:
            CPU.INS_Cycles += 2;
            break;


        // ------------Stack(栈)------------

//            //PHA
//        case 0x48:
//            INS_Stack(CPU.A,&CPU.SP,0);
//            break;
//            //PLA
//        case 0x68:
//            INS_Transfer(CPU.SP,&CPU.A,1);
//            break;
//            //PHP
//        case 0x08:
//            INS_Transfer_Stack_Satus(CPU.SP,&CPU.A,0);
//            break;
    }
}

int main() {
    CPU_Reset(0x1000);
    CPU.Mem[CPU.PC]   = 0x90;
    CPU.Mem[CPU.PC+1] = 0xD9;
    CPU_Exec();

//    assert(CPU.F_N == 1);
//    assert(CPU.F_Z == 0);
//    assert(CPU.F_V == 1);
    printf("SUCCESS!\n");
    return 0;
}
