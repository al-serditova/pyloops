import numpy as np
import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

iptr = pyloops.IReg()
omptr = pyloops.IReg()
olptr = pyloops.IReg()
n = pyloops.IReg()
pyloops.start_func("nullify_msb_lsb_v", iptr, omptr, olptr, n)
offset = pyloops.IReg(0)
n *= 4

one = pyloops.VReg(np.uint32, 1)
pyloops.while_(offset < n)
pyloops.storevec(olptr, offset, one)
one.assign = one + one
pyloops.storevec(omptr, offset, one)
offset += pyloops.vbytes()

pyloops.endwhile_()
pyloops.return_()
    # {
    #     WHILE_(offset < n)
    #     {
    #         VReg<uint32_t> in = loadvec<uint32_t>(iptr, offset);
    #         VReg<uint32_t> msb = in | ushift_right(in,1);
    #         msb |= ushift_right(msb,  2);
    #         msb |= ushift_right(msb,  4);
    #         msb |= ushift_right(msb,  8);
    #         msb |= ushift_right(msb, 16);
    #         msb += one;  //It's assumed, that 0x80000000 bit is switched off.
    #         msb = ushift_right(msb, 1);
    #         msb ^= in;
    #         storevec(omptr, offset, msb);
    #         VReg<uint32_t> lsb = in & ~(in - one);
    #         lsb ^= in;
    #         storevec(olptr, offset, lsb);
    #         offset += ctx.vbytes();
    #     }
    #     RETURN_();
    # }
pyloops.end_func()
func = pyloops.get_func("nullify_msb_lsb_v")
func.print_ir()
func.print_assembly()
addr = func.ptr()

executable_func = ctypes.CFUNCTYPE(ctypes.c_int64, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32), ctypes.c_int64)(addr)

v = np.array([0x60000000, 2, 0xf0, 7, 0x0fffffff, 0b101010101, 1234, 4321], dtype=np.uint32)
lsb = np.array([0, 0, 0, 0, 0, 0, 0, 0], dtype=np.uint32)
msb = np.array([0, 0, 0, 0, 0, 0, 0, 0], dtype=np.uint32)
v_ptr = v.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
lsb_ptr = lsb.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
msb_ptr = msb.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
res = executable_func(v_ptr, lsb_ptr, msb_ptr, len(v))

print(lsb)
print(msb)

# for vnum in range(len(v)):
#     tchk  = v[vnum]
#     relsb = tchk ^ (tchk & ~np.uint32(tchk - 1))
#     remsb = tchk | np.uint32(tchk >> 1)
#     remsb |= np.uint32(remsb >> 2)
#     remsb |= np.uint32(remsb >> 4)
#     remsb |= np.uint32(remsb >> 8)
#     remsb |= np.uint32(remsb >> 16)
#     remsb  = np.uint32((remsb + 1) >> 1)
#     remsb ^= tchk
#     if lsb[vnum] != relsb:
#         print("Incorrect LSB")
#     if msb[vnum] != remsb:
#         print("Incorrect MSB")



# DUBUG0: True
# DUBUG1: False
# DUBUG2: False
# DUBUG3: True
# DUBUG4: False

        # printf("DUBUG0: %s\n", target.is_leaf()? "True":"False");
        # printf("DUBUG1: %s\n", target.leaf().tag == Arg::IREG? "True":"False");
        # printf("DUBUG2: %s\n", !fromwho.is_vector()? "True":"False");
        # printf("DUBUG3: %s\n", (target.leaf().tag == Arg::VREG)? "True":"False");
        # printf("DUBUG4: %s\n", fromwho.type() == target.leaf().elemtype? "True":"False");
        # LOOPS_ASSERT(target.is_leaf() && ((target.leaf().tag == Arg::IREG && !fromwho.is_vector()) || (target.leaf().tag == Arg::VREG && fromwho.type() == target.leaf().elemtype)));
        # LOOPS_ASSERT(True             && ((False                          && False               ) || (True                           && fromwho.type() == target.leaf().elemtype)));
