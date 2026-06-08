import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops
import numpy as np

def nullify_msb_lsb_v_python(input_array):
    data_in = input_array.astype(np.uint32)
    
    one = np.uint32(1)
 
    msb = data_in | (data_in >> 1)
    msb |= (msb >> 2)
    msb |= (msb >> 4)
    msb |= (msb >> 8)
    msb |= (msb >> 16)

    msb_plus_one = msb + one

    msb_final = (msb_plus_one >> 1) ^ data_in
    
    lsb = data_in & ~(data_in - one)

    lsb_final = lsb ^ data_in
    
    return msb_final, lsb_final
   
    
iptr = pyloops.IReg()
omptr = pyloops.IReg()
olptr = pyloops.IReg()
n = pyloops.IReg()
pyloops.start_func("nullify_msb_lsb_v", iptr, omptr, olptr, n)
offset = pyloops.IReg(0)
n *= 4
one = pyloops.VReg(np.uint32, 1)

pyloops.while_(offset < n)
v_in = pyloops.loadvec(np.uint32, iptr, offset)
msb = pyloops.VReg(np.uint32, v_in | pyloops.ushift_right(v_in, 1))

# Размножаем старший бит (MSB)
msb |= pyloops.ushift_right(msb, 2)
msb |= pyloops.ushift_right(msb, 4)
msb |= pyloops.ushift_right(msb, 8)
msb |= pyloops.ushift_right(msb, 16)

# Округляем до степени двойки и сдвигаем
msb += one
msb.assign = pyloops.ushift_right(msb, 1)

# Маскируем исходный вектор:
msb ^= v_in

# Сохраняем результат MSB в omptr
pyloops.storevec(omptr, offset, msb)

# --- Блок LSB  ---
lsb = pyloops.VReg(np.uint32, v_in & ~(v_in - one))
lsb ^= v_in

# Сохраняем результат LSB в olptr
pyloops.storevec(olptr, offset, lsb)
offset += pyloops.vbytes()

pyloops.endwhile_()
pyloops.return_()

pyloops.end_func()
func = pyloops.get_func("nullify_msb_lsb_v")
func.print_ir()
func.print_assembly()
addr = func.ptr()

executable_func = ctypes.CFUNCTYPE(
    None,
    ctypes.POINTER(ctypes.c_uint32), 
    ctypes.POINTER(ctypes.c_uint32), 
    ctypes.POINTER(ctypes.c_uint32), 
    ctypes.c_int64
)(addr)


v = np.array([0x60000000, 2, 0xf0, 7, 0x0fffffff, 0b101010101, 1234, 4321], dtype=np.uint32)
lsb = np.array([0, 0, 0, 0, 0, 0, 0, 0], dtype=np.uint32)
msb = np.array([0, 0, 0, 0, 0, 0, 0, 0], dtype=np.uint32)

print("=======REFERENCE RESULT=======")
lsb_out, msb_out = nullify_msb_lsb_v_python(v)
print(lsb_out)
print(msb_out)

print("=======LOOPSMADE RESULT=======")
v_ptr = v.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
lsb_ptr = lsb.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
msb_ptr = msb.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))

executable_func(v_ptr, lsb_ptr, msb_ptr, len(v))

print(lsb)
print(msb)