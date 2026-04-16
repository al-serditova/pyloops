import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops
import numpy as np

# a = pyloops.IReg()
# b = pyloops.IReg()
# c = pyloops.IReg()

# pyloops.start_func("a_plus_b", a, b, c)
# x = pyloops.IReg(25)
# a += x
# pyloops.return_(a)
# pyloops.end_func()

# func = pyloops.get_func("a_plus_b")
# func.print_ir()
# func.print_assembly()

# addr = func.ptr()
# executable_func = ctypes.CFUNCTYPE(ctypes.c_int64, ctypes.c_int64, ctypes.c_int64, ctypes.c_int64)(addr)
# res = executable_func(5, 3, 1)
# print(res)


a = pyloops.IReg()
ptr = pyloops.IReg()
offset = pyloops.IReg()

pyloops.start_func("a_plus_ptr_offset", a, ptr, offset)
x = pyloops.IReg(pyloops.load_i32(ptr, offset))
a += x
pyloops.return_(a)
pyloops.end_func()

func = pyloops.get_func("a_plus_ptr_offset")
func.print_ir()
func.print_assembly()

addr = func.ptr()
executable_func = ctypes.CFUNCTYPE(
    ctypes.c_int64, 
    ctypes.c_int64, 
    ctypes.POINTER(ctypes.c_int32),
    ctypes.c_int64
)(addr)

data = np.array([8, 2, -5, 7, 6], dtype = np.int32)

# Получаем указатель на данные массива
data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

res = executable_func(5, data_ptr, 16)
print(res)