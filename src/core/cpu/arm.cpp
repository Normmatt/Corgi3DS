#include <sstream>
#include "arm.hpp"
#include "arm_disasm.hpp"
#include "arm_interpret.hpp"
#include "../common/common.hpp"
#include "../emulator.hpp"

uint32_t PSR_Flags::get()
{
    uint32_t reg = 0;
    reg |= negative << 31;
    reg |= zero << 30;
    reg |= carry << 29;
    reg |= overflow << 28;
    reg |= q_overflow << 27;

    reg |= irq_disable << 7;
    reg |= fiq_disable << 6;
    reg |= thumb << 5;

    reg |= mode;
    return reg;
}

void PSR_Flags::set(uint32_t value)
{
    negative = value & (1 << 31);
    zero = value & (1 << 30);
    carry = value & (1 << 29);
    overflow = value & (1 << 28);
    q_overflow = value & (1 << 27);

    irq_disable = value & (1 << 7);
    fiq_disable = value & (1 << 6);
    thumb = value & (1 << 5);

    mode = (PSR_MODE)(value & 0x1F);
}

ARM_CPU::ARM_CPU(Emulator* e, int id, CP15* cp15) : e(e), id(id), cp15(cp15)
{

}

std::string ARM_CPU::get_reg_name(int id)
{
    if (id < 10)
    {
        std::stringstream output;
        output << "r" << id;
        return output.str();
    }
    else
    {
        switch (id)
        {
            case 10:
                return "sl";
            case 11:
                return "fp";
            case 12:
                return "ip";
            case 13:
                return "sp";
            case 14:
                return "lr";
            case 15:
                return "pc";
        }
    }
    return "??";
}

void ARM_CPU::reset()
{
    CPSR.mode = PSR_SUPERVISOR;
    CPSR.fiq_disable = true;
    CPSR.irq_disable = true;
    if (id == 9)
        jp(0xFFFF0000, true);
    else
        jp(0x0, true);

    can_disassemble = false;
}

void ARM_CPU::run()
{
    if (halted)
        return;
    if (CPSR.thumb)
    {
        uint16_t instr = read16(gpr[15] - 2);
        gpr[15] += 2;
        if (can_disassemble)
            printf("[$%08X] $%04X  %s\n", gpr[15] - 4, instr, ARM_Disasm::disasm_thumb(*this, instr).c_str());
        ARM_Interpreter::interpret_thumb(*this, instr);
    }
    else
    {
        uint32_t instr = read32(gpr[15] - 4);
        gpr[15] += 4;
        if (can_disassemble)
        {
            printf("[$%08X] $%08X  %s\n", gpr[15] - 8, instr, ARM_Disasm::disasm_arm(*this, instr).c_str());
            //print_state();
        }
        ARM_Interpreter::interpret_arm(*this, instr);
    }

    if (int_pending)
        int_check();
}

void ARM_CPU::print_state()
{
    for (int i = 0; i < 16; i++)
    {
        printf("%s:$%08X", get_reg_name(i).c_str(), gpr[i]);
        if (i % 4 == 3)
            printf("\n");
        else
            printf("\t");
    }
    printf("CPSR:$%08X\n", CPSR.get());
}

void ARM_CPU::jp(uint32_t addr, bool change_thumb_state)
{
    //if (id == 9)
        //printf("jp: $%08X\n", addr);
    if (addr == 0x801B000)
        can_disassemble = true;
    gpr[15] = addr;

    if (change_thumb_state)
        CPSR.thumb = addr & 0x1;

    if (CPSR.thumb)
    {
        gpr[15] &= ~0x1;
        gpr[15] += 2;
    }
    else
    {
        gpr[15] &= ~0x3;
        gpr[15] += 4;
    }
}

void ARM_CPU::int_check()
{
    //printf("[ARM%d] Interrupt check\n", id);
    if (!CPSR.irq_disable && int_pending)
    {
        printf("Interrupt!\n");
        uint32_t value = CPSR.get();
        SPSR[PSR_IRQ].set(value);

        //Update new CPSR
        LR_irq = gpr[15] + ((CPSR.thumb) ? 2 : 0);
        update_reg_mode(PSR_IRQ);
        CPSR.mode = PSR_IRQ;
        CPSR.irq_disable = true;

        if (id == 9)
            jp(0xFFFF0000 + 0x18, true);
        else
            jp(0x18, true);
    }
}

void ARM_CPU::set_int_signal(bool pending)
{
    int_pending = pending;
    if (int_pending)
        unhalt();
    int_check();
}

void ARM_CPU::halt()
{
    printf("[ARM%d] Halting...\n", id);
    halted = true;
}

void ARM_CPU::unhalt()
{
    halted = false;
}

void ARM_CPU::set_zero_neg_flags(uint32_t value)
{
    CPSR.negative = value & (1 << 31);
    CPSR.zero = !value;
}

//Code here from melonDS's ALU core (my original implementation was incorrect in many ways)
void ARM_CPU::set_CV_add_flags(uint32_t a, uint32_t b, uint32_t result)
{
    CPSR.carry = ((0xFFFFFFFF-a) < b);
    CPSR.overflow = ADD_OVERFLOW(a, b, result);
}

void ARM_CPU::set_CV_sub_flags(uint32_t a, uint32_t b, uint32_t result)
{
    CPSR.carry = a >= b;
    CPSR.overflow = SUB_OVERFLOW(a, b, result);
}

void ARM_CPU::update_reg_mode(PSR_MODE mode)
{
    if (mode != CPSR.mode)
    {
        switch (CPSR.mode)
        {
            case PSR_SYSTEM:
            case PSR_USER:
                break;
            case PSR_IRQ:
                std::swap(gpr[13], SP_irq);
                std::swap(gpr[14], LR_irq);
                break;
            case PSR_FIQ:
                std::swap(gpr[8], fiq_regs[0]);
                std::swap(gpr[9], fiq_regs[1]);
                std::swap(gpr[10], fiq_regs[2]);
                std::swap(gpr[11], fiq_regs[3]);
                std::swap(gpr[12], fiq_regs[4]);
                std::swap(gpr[13], SP_fiq);
                std::swap(gpr[14], LR_fiq);
                break;
            case PSR_SUPERVISOR:
                std::swap(gpr[13], SP_svc);
                std::swap(gpr[14], LR_svc);
                break;
            case PSR_ABORT:
                std::swap(gpr[13], SP_abt);
                std::swap(gpr[14], LR_abt);
                break;
            case PSR_UNDEFINED:
                std::swap(gpr[13], SP_und);
                std::swap(gpr[14], LR_und);
                break;
            default:
                EmuException::die("[ARM%d] Unrecognized old PSR mode %d\n", id, CPSR.mode);
        }

        switch (mode)
        {
            case PSR_SYSTEM:
            case PSR_USER:
                break;
            case PSR_IRQ:
                std::swap(gpr[13], SP_irq);
                std::swap(gpr[14], LR_irq);
                break;
            case PSR_FIQ:
                std::swap(gpr[8], fiq_regs[0]);
                std::swap(gpr[9], fiq_regs[1]);
                std::swap(gpr[10], fiq_regs[2]);
                std::swap(gpr[11], fiq_regs[3]);
                std::swap(gpr[12], fiq_regs[4]);
                std::swap(gpr[13], SP_fiq);
                std::swap(gpr[14], LR_fiq);
                break;
            case PSR_SUPERVISOR:
                std::swap(gpr[13], SP_svc);
                std::swap(gpr[14], LR_svc);
                break;
            case PSR_ABORT:
                std::swap(gpr[13], SP_abt);
                std::swap(gpr[14], LR_abt);
                break;
            case PSR_UNDEFINED:
                std::swap(gpr[13], SP_und);
                std::swap(gpr[14], LR_und);
                break;
            default:
                EmuException::die("[ARM%d] Unrecognized new PSR mode %d\n", id, mode);
        }
    }
}

void ARM_CPU::spsr_to_cpsr()
{
    uint32_t new_CPSR = SPSR[CPSR.mode].get();
    update_reg_mode((PSR_MODE)(new_CPSR & 0x1F));
    CPSR.set(new_CPSR);
}

bool ARM_CPU::meets_condition(int cond)
{
    switch (cond)
    {
        case 0x0:
            //EQ - equal
            return CPSR.zero;
        case 0x1:
            //NE - not equal
            return !CPSR.zero;
        case 0x2:
            //CS - unsigned higher or same
            return CPSR.carry;
        case 0x3:
            //CC - unsigned lower
            return !CPSR.carry;
        case 0x4:
            //MI - negative
            return CPSR.negative;
        case 0x5:
            //PL - positive or zero
            return !CPSR.negative;
        case 0x6:
            //VS - overflow
            return CPSR.overflow;
        case 0x7:
            //VC - no overflow
            return !CPSR.overflow;
        case 0x8:
            //HI - unsigned higher
            return CPSR.carry && !CPSR.zero;
        case 0x9:
            //LS - unsigned lower or same
            return !CPSR.carry || CPSR.zero;
        case 0xA:
            //GE - greater than or equal
            return (CPSR.negative == CPSR.overflow);
        case 0xB:
            //LT - less than
            return (CPSR.negative != CPSR.overflow);
        case 0xC:
            //GT - greater than
            return !CPSR.zero && (CPSR.negative == CPSR.overflow);
        case 0xD:
            //LE - less than or equal to
            return CPSR.zero || (CPSR.negative != CPSR.overflow);
        case 0xE:
            //AL - always
            return true;
        case 0xF:
            //Some instructions have the 0xF condition required, so let them pass
            return true;
        default:
            EmuException::die("[ARM_CPU] Unrecognized condition %d\n", cond);
            return false;
    }
}

uint8_t ARM_CPU::read8(uint32_t addr)
{
    if (cp15)
    {
        if (addr < cp15->itcm_size)
            return cp15->ITCM[addr & 0x7FFF];
        if (addr >= cp15->dtcm_base && addr < cp15->dtcm_base + cp15->dtcm_size)
            return cp15->DTCM[addr & 0x3FFF];
    }
    if (id == 9)
        return e->arm9_read8(addr);
    return e->arm11_read8(addr);
}

uint16_t ARM_CPU::read16(uint32_t addr)
{
    if (cp15)
    {
        if (addr < cp15->itcm_size)
            return *(uint16_t*)&cp15->ITCM[addr & 0x7FFF];
        if (addr >= cp15->dtcm_base && addr < cp15->dtcm_base + cp15->dtcm_size)
            return *(uint16_t*)&cp15->DTCM[addr & 0x3FFF];
    }
    if (id == 9)
        return e->arm9_read16(addr);
    return e->arm11_read16(addr);
}

uint32_t ARM_CPU::read32(uint32_t addr)
{
    if (cp15)
    {
        if (addr < cp15->itcm_size)
            return *(uint32_t*)&cp15->ITCM[addr & 0x7FFF];
        if (addr >= cp15->dtcm_base && addr < cp15->dtcm_base + cp15->dtcm_size)
            return *(uint32_t*)&cp15->DTCM[addr & 0x3FFF];
    }
    if (id == 9)
        return e->arm9_read32(addr);
    return e->arm11_read32(addr);
}

void ARM_CPU::write8(uint32_t addr, uint8_t value)
{
    if (cp15)
    {
        if (addr < cp15->itcm_size)
        {
            cp15->ITCM[addr & 0x7FFF] = value;
            return;
        }
        if (addr >= cp15->dtcm_base && addr < cp15->dtcm_base + cp15->dtcm_size)
        {
            cp15->DTCM[addr & 0x3FFF] = value;
            return;
        }
    }
    if (id == 9)
        e->arm9_write8(addr, value);
    else
        e->arm11_write8(addr, value);
}

void ARM_CPU::write16(uint32_t addr, uint16_t value)
{
    if (cp15)
    {
        if (addr < cp15->itcm_size)
        {
            *(uint16_t*)&cp15->ITCM[addr & 0x7FFF] = value;
            return;
        }
        if (addr >= cp15->dtcm_base && addr < cp15->dtcm_base + cp15->dtcm_size)
        {
            *(uint16_t*)&cp15->DTCM[addr & 0x3FFF] = value;
            return;
        }
    }
    if (id == 9)
        e->arm9_write16(addr, value);
    else
        e->arm11_write16(addr, value);
}

void ARM_CPU::write32(uint32_t addr, uint32_t value)
{
    if (addr == 0x08077438 + 4)
        printf("blorp $%08X\n", value);
    if (cp15)
    {
        if (addr < cp15->itcm_size)
        {
            *(uint32_t*)&cp15->ITCM[addr & 0x7FFF] = value;
            return;
        }
        if (addr >= cp15->dtcm_base && addr < cp15->dtcm_base + cp15->dtcm_size)
        {
            *(uint32_t*)&cp15->DTCM[addr & 0x3FFF] = value;
            return;
        }
    }
    if (id == 9)
        e->arm9_write32(addr, value);
    else
        e->arm11_write32(addr, value);
}

void ARM_CPU::andd(int destination, int source, int operand, bool set_condition_codes)
{
    uint32_t result = source & operand;
    set_register(destination, result);

    if (set_condition_codes)
        set_zero_neg_flags(result);
}

void ARM_CPU::orr(int destination, int source, int operand, bool set_condition_codes)
{
    uint32_t result = source | operand;
    set_register(destination, result);

    if (set_condition_codes)
        set_zero_neg_flags(result);
}

void ARM_CPU::eor(int destination, int source, int operand, bool set_condition_codes)
{
    uint32_t result = source ^ operand;
    set_register(destination, result);

    if (set_condition_codes)
        set_zero_neg_flags(result);
}

void ARM_CPU::add(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint64_t unsigned_result = source + operand;
    if (destination == REG_PC)
    {
        if (set_condition_codes)
            throw "[CPU] Op 'adds pc' unsupported";
        else
            jp(unsigned_result & 0xFFFFFFFF, true);
    }
    else
    {
        set_register(destination, unsigned_result & 0xFFFFFFFF);

        if (set_condition_codes)
            cmn(source, operand);
    }
}

void ARM_CPU::sub(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint64_t unsigned_result = source - operand;
    if (destination == REG_PC)
    {
        if (set_condition_codes)
        {
            int index = static_cast<int>(CPSR.mode);
            update_reg_mode(SPSR[index].mode);
            CPSR.set(SPSR[index].get());
            jp(unsigned_result & 0xFFFFFFFF, false);
        }
        else
            jp(unsigned_result & 0xFFFFFFFF, true);
    }
    else
    {
        set_register(destination, unsigned_result & 0xFFFFFFFF);
        if (set_condition_codes)
            cmp(source, operand);
    }
}

void ARM_CPU::adc(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint8_t carry = (CPSR.carry) ? 1 : 0;
    add(destination, source + carry, operand, set_condition_codes);
    if (set_condition_codes)
    {
        uint32_t temp = source + operand;
        uint32_t res = temp + carry;
        CPSR.carry = CARRY_ADD(source, operand) | CARRY_ADD(temp, carry);
        CPSR.overflow = ADD_OVERFLOW(source, operand, temp) | ADD_OVERFLOW(temp, carry, res);
    }
}

void ARM_CPU::sbc(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    unsigned int borrow = (CPSR.carry) ? 0 : 1;
    sub(destination, source, operand + borrow, set_condition_codes);
    if (set_condition_codes)
    {
        uint32_t temp = source - operand;
        uint32_t res = temp - borrow;
        CPSR.carry = CARRY_SUB(source, operand) & CARRY_SUB(temp, borrow);
        CPSR.overflow = SUB_OVERFLOW(source, operand, temp) | SUB_OVERFLOW(temp, borrow, res);
    }
}

void ARM_CPU::tst(uint32_t x, uint32_t y)
{
    set_zero_neg_flags(x & y);
}

void ARM_CPU::teq(uint32_t x, uint32_t y)
{
    set_zero_neg_flags(x ^ y);
}

void ARM_CPU::cmn(uint32_t x, uint32_t y)
{
    uint32_t result = x + y;
    set_zero_neg_flags(result);
    set_CV_add_flags(x, y, result);
}

void ARM_CPU::cmp(uint32_t x, uint32_t y)
{
    uint32_t result = x - y;
    set_zero_neg_flags(result);
    set_CV_sub_flags(x, y, result);
}

void ARM_CPU::mov(uint32_t destination, uint32_t operand, bool alter_flags)
{
    if (destination == REG_PC)
    {
        if (alter_flags)
        {
            int index = static_cast<int>(CPSR.mode);
            update_reg_mode(SPSR[index].mode);
            CPSR.set(SPSR[index].get());
            jp(operand, false);
        }
        else
            jp(operand, true);
    }
    else
    {
        set_register(destination, operand);
        if (alter_flags)
            set_zero_neg_flags(operand);
    }
}

void ARM_CPU::mul(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint64_t result = source * operand;
    uint32_t truncated = result & 0xFFFFFFFF;
    set_register(destination, truncated);

    if (set_condition_codes)
        set_zero_neg_flags(truncated);
}

void ARM_CPU::bic(uint32_t destination, uint32_t source, uint32_t operand, bool alter_flags)
{
    uint32_t result = source & ~operand;
    set_register(destination, result);
    if (alter_flags)
        set_zero_neg_flags(result);
}

void ARM_CPU::mvn(uint32_t destination, uint32_t operand, bool alter_flags)
{
    set_register(destination, ~operand);
    if (alter_flags)
        set_zero_neg_flags(~operand);
}

void ARM_CPU::mrs(uint32_t instr)
{
    bool using_CPSR = (instr & (1 << 22)) == 0;
    uint32_t destination = (instr >> 12) & 0xF;

    if (using_CPSR)
    {
        set_register(destination, CPSR.get());
    }
    else
    {
        set_register(destination, SPSR[static_cast<int>(CPSR.mode)].get());
    }
}

void ARM_CPU::msr(uint32_t instr)
{
    bool is_imm = (instr & (1 << 25));
    bool using_CPSR = (instr & (1 << 22)) == 0;

    PSR_Flags* PSR;
    if (using_CPSR)
        PSR = &CPSR;
    else
        PSR = &SPSR[static_cast<int>(CPSR.mode)];

    uint32_t value = PSR->get();
    uint32_t source;
    if (is_imm)
    {
        source = instr & 0xFF;
        int shift = (instr & 0xF00) >> 7;
        source = rotr32(source, shift, false);
    }
    else
        source = get_register(instr & 0xF);

    uint32_t bitmask = 0;
    if (instr & (1 << 19))
        bitmask |= 0xFF000000;

    if (instr & (1 << 16))
        bitmask |= 0xFF;

    if (CPSR.mode == PSR_USER)
        bitmask &= 0xFFFFFF00; //prevent changes to control field

    if (using_CPSR)
        bitmask &= 0xFFFFFFDF; //prevent changes to thumb state

    value &= ~bitmask;
    value |= (source & bitmask);

    if (using_CPSR)
    {
        PSR_MODE new_mode = static_cast<PSR_MODE>(value & 0x1F);
        update_reg_mode(new_mode);
    }
    PSR->set(value);
}

void ARM_CPU::cps(uint32_t instr)
{
    PSR_MODE psr_mode = (PSR_MODE)(instr & 0x1F);
    bool f = (instr >> 6) & 0x1;
    bool i = (instr >> 7) & 0x1;
    bool a = (instr >> 8) & 0x1;
    bool mmod = (instr >> 17) & 0x1;
    int imod = (instr >> 18) & 0x3;

    if (mmod)
    {
        update_reg_mode(psr_mode);
        CPSR.mode = psr_mode;
    }

    if (imod == 2)
    {
        //TODO: data abort
        CPSR.fiq_disable &= ~f;
        CPSR.irq_disable &= ~i;
    }
    else if (imod == 3)
    {
        CPSR.fiq_disable |= f;
        CPSR.irq_disable |= i;
    }
}

void ARM_CPU::srs(uint32_t instr)
{
    bool is_writing_back = instr & (1 << 21);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    PSR_MODE mode = (PSR_MODE)(instr & 0x1F);

    uint32_t saved_LR = gpr[REG_LR];
    uint32_t saved_PSR = SPSR[CPSR.mode].get();

    PSR_MODE old_mode = CPSR.mode;
    update_reg_mode(mode);
    CPSR.mode = mode;

    uint32_t banked_addr = get_register(REG_SP);

    int offset;
    if (is_adding_offset)
    {
        offset = 4;

        if (is_preindexing)
        {
            write32(banked_addr + offset, saved_LR);
            write32(banked_addr + (offset * 2), saved_PSR);
        }
        else
        {
            write32(banked_addr, saved_LR);
            write32(banked_addr + offset, saved_PSR);
        }
    }
    else
    {
        offset = -4;

        if (is_preindexing)
        {
            write32(banked_addr + offset, saved_PSR);
            write32(banked_addr + (offset * 2), saved_LR);
        }
        else
        {
            write32(banked_addr, saved_PSR);
            write32(banked_addr + offset, saved_LR);
        }
    }

    if (is_writing_back)
        set_register(REG_SP, banked_addr + (offset * 2));

    //Restore previous state
    update_reg_mode(old_mode);
    CPSR.mode = old_mode;
}

void ARM_CPU::rfe(uint32_t instr)
{
    bool is_writing_back = instr & (1 << 21);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    int base = (instr >> 16) & 0xF;

    uint32_t addr = get_register(base);
    uint32_t PC, PSR;
    int offset;
    if (is_adding_offset)
    {
        offset = 4;

        if (is_preindexing)
        {
            PC = read32(addr + offset);
            PSR = read32(addr + (offset * 2));
        }
        else
        {
            PC = read32(addr);
            PSR = read32(addr + offset);
        }
    }
    else
    {
        offset = -4;

        if (is_preindexing)
        {
            PSR = read32(addr + offset);
            PC = read32(addr + (offset * 2));
        }
        else
        {
            PSR = read32(addr);
            PC = read32(addr + offset);
        }
    }

    if (is_writing_back)
        set_register(base, addr + (offset * 2));

    update_reg_mode((PSR_MODE)(PSR & 0x1F));
    CPSR.set(PSR);

    jp(PC, true);
}

uint32_t ARM_CPU::mrc(int coprocessor_id, int operation_mode, int CP_reg,
                  int coprocessor_info, int coprocessor_operand)
{
    switch (coprocessor_id)
    {
        case 15:
            if (!cp15)
                return 0;
            return cp15->mrc(operation_mode, CP_reg, coprocessor_info, coprocessor_operand);
        default:
            return 0;
    }
}

void ARM_CPU::mcr(int coprocessor_id, int operation_mode,
                  int CP_reg, int coprocessor_info, int coprocessor_operand, uint32_t value)
{
    switch (coprocessor_id)
    {
        case 15:
            if (!cp15)
                return;
            cp15->mcr(operation_mode, CP_reg, coprocessor_info, coprocessor_operand, value);
    }
}

uint32_t ARM_CPU::lsl(uint32_t value, int shift, bool alter_flags)
{
    if (!shift)
    {
        if (alter_flags)
            set_zero_neg_flags(value);
        return value;
    }
    if (shift > 32)
    {
        if (alter_flags)
        {
            set_zero_neg_flags(0);
            CPSR.carry = false;
        }
        return 0;
    }
    if (shift > 31)
    {
        if (alter_flags)
        {
            set_zero_neg_flags(0);
            CPSR.carry = value & (1 << 0);
        }
        return 0;
    }
    uint32_t result = value << shift;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        CPSR.carry = value & (1 << (32 - shift));
    }
    return value << shift;
}

uint32_t ARM_CPU::lsr(uint32_t value, int shift, bool alter_flags)
{
    if (shift > 32)
    {
        if (alter_flags)
        {
            set_zero_neg_flags(0);
            CPSR.carry = false;
        }
        return 0;
    }
    if (shift > 31)
        return lsr_32(value, alter_flags);
    uint32_t result = value >> shift;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        if (shift)
            CPSR.carry = value & (1 << (shift - 1));
    }
    return result;
}

uint32_t ARM_CPU::lsr_32(uint32_t value, bool alter_flags)
{
    if (alter_flags)
    {
        set_zero_neg_flags(0);
        CPSR.carry = value & (1 << 31);
    }
    return 0;
}

uint32_t ARM_CPU::asr(uint32_t value, int shift, bool alter_flags)
{
    if (shift > 31)
        return asr_32(value, alter_flags);
    uint32_t result = static_cast<int32_t>(value) >> shift;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        if (shift)
            CPSR.carry = value & (1 << (shift - 1));
    }
    return result;
}

uint32_t ARM_CPU::asr_32(uint32_t value, bool alter_flags)
{
    uint32_t result = static_cast<int32_t>(value) >> 31;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        CPSR.carry = value & (1 << 31);
    }
    return result;
}

uint32_t ARM_CPU::rrx(uint32_t value, bool alter_flags)
{
    uint32_t result = value;
    result >>= 1;
    result |= (CPSR.carry) ? (1 << 31) : 0;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        CPSR.carry = value & 0x1;
    }
    return result;
}

//Thanks to Stack Overflow for providing a safe rotate function
uint32_t ARM_CPU::rotr32(uint32_t n, unsigned int c, bool alter_flags)
{
    const unsigned int mask = 0x1F;
    if (alter_flags && c)
    {
        if (c & 0x1F)
            CPSR.carry = n & (1 << (c - 1));
        else
            CPSR.carry = n & (1 << 31);
    }
    c &= mask;

    uint32_t result = (n>>c) | (n<<( (-(int)c)&mask ));

    if (alter_flags)
    {
        set_zero_neg_flags(result);
    }

    return result;
}
