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


ptr = pyloops.IReg()
amount = pyloops.IReg()

pyloops.start_func("a_plus_ptr_offset", ptr, amount)
i = pyloops.IReg(0)
sum = pyloops.IReg(0)
offset = pyloops.IReg(0)
# pyloops.while_(i<amount)
# sum += pyloops.load_i32(ptr, offset)
# i += 1
# offset += pyloops.IReg(4)
# pyloops.endwhile_()

sum += 10
sum -= 2
sum *= 6
sum %= 5
pyloops.return_(sum)
pyloops.end_func()

func = pyloops.get_func("a_plus_ptr_offset")
func.print_ir()
func.print_assembly()

addr = func.ptr()
executable_func = ctypes.CFUNCTYPE(
    ctypes.c_int64, 
    ctypes.POINTER(ctypes.c_int32),
    ctypes.c_int64
)(addr)

data = np.array([8, 2, -5, 7, 6], dtype = np.int32)

# Получаем указатель на данные массива
data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

res = executable_func(data_ptr, 2)
print(res)