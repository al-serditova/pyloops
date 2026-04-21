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
a = pyloops.IReg()
meow = pyloops.IReg()

pyloops.start_func("aaaa", ptr, a, meow)
woof = pyloops.IReg(10)
meow = 36 + woof - 12 * 2 

# pyloops.while_(a > 0)
# pyloops.if_(a == 10)
# pyloops.break_()

# pyloops.elif_(a == 5)
# a += 2
# pyloops.continue_()

# pyloops.else_()
# a -= 1
# pyloops.endif_()
# pyloops.endwhile_()

pyloops.return_(meow)
pyloops.end_func()

func = pyloops.get_func("aaaa")
func.print_ir()
func.print_assembly()

addr = func.ptr()
executable_func = ctypes.CFUNCTYPE(
    ctypes.c_int64, 
    ctypes.POINTER(ctypes.c_int32),
    ctypes.c_int64
)(addr)

data = np.array([8, 2, -5, 7, 6], dtype = np.int32)

# # Получаем указатель на данные массива
data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

res = executable_func(data_ptr, 14)
print(res)