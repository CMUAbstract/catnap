'''
|   lr(MSB) | <- ~~~ + 2
|   r2      | <- ##_local_var_byte_size+48(r1) = ##_local_var_byte_size+4*##_pushed_reg_num
|	r15(MSB)|
|	r15(LSB)| <- ##_local_var_byte_size+44(r1)
|	...     |
|	r5(MSB) |
|	r5(LSB) | <- ##_local_var_byte_size+4(r1)
|	r4(MSB) |
|	r4(LSB) | <- ##_local_var_byte_size(r1)
|	local   |
|	...     |
|	local   | <- r1
'''
def get_inst(inst, op1, op2):
    return "\t__asm__ volatile (\"" + inst + " " + op1 + ", " + op2 + "\");\\\n"

def get_mov_offset(offset, src, dst):
    return get_inst("MOV", str(offset) + "(" + src + ")", dst)

def get_movx_offset(offset, src, dst):
    return get_movx(str(offset) + "(" + src + ")", dst)

def get_movx(src, dst):
    return get_inst("MOVX.A", src, dst)

def get_add(imm, dst):
    return get_inst("ADD", "#" + str(imm), dst)

def get_sub(imm, dst):
    return get_inst("SUB", "#" + str(imm), dst)
    
compISR_pushed_reg_num = 12
compISR_local_var_byte_size = 0
timerISR_pushed_reg_num = 12
timerISR_local_var_byte_size = 12

def get_checkpoint_code(local_byte, reg_num):
    # If not, we need to directly grab
    # the value from the reg, which gets messier
    assert(reg_num == 12)

    result = ""

    # R0
    offset = reg_num*4 + local_byte + 2
    result += get_mov_offset(offset, "R1", "&0x4000")

    # R2
    offset -= 2
    result += get_mov_offset(offset, "R1", "&0x4008")

    # R3 is not used
    # R4 - R15
    reg_num_cpy = reg_num
    for i in range(12):
        dst = "&" + hex(16400 + 4*i)
        if reg_num_cpy == 0:
            # Reg is not pushed
            # I am too lazy to implement this part!
            assert(False)
        else:
            # Reg is pushed in the stack
            offset = local_byte + 4*i
            result += get_movx_offset(offset, "R1", dst)
        reg_num_cpy -= 1

    # R1
    offset = reg_num*4 + local_byte + 4
    result += get_add(offset, "R1")
    result += get_movx("R1", "&0x4004")
    result += get_sub(offset, "R1")
    result += "\n"

    return result


# For compISR
result = ""
result += "#define checkpoint() \\\n"
result += get_checkpoint_code(compISR_local_var_byte_size,
        compISR_pushed_reg_num)

# For timerISR
result += "#define checkpoint_before_event() \\\n"
result += get_checkpoint_code(timerISR_local_var_byte_size,
        timerISR_pushed_reg_num)

f = open("../src/include/libos/checkpoint.h", "w+")
f.write(result)
f.close()

