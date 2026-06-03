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

pyloops.start_func("aaaa", ptr)
# v1 = pyloops.loadVec(np.int32, ptr)
res = pyloops.VReg(np.int32, 2)
v2 = pyloops.loadVec(np.int32, ptr, 0)

pyloops.storevec(ptr, v2 > res)

pyloops.end_func()

func = pyloops.get_func("aaaa")
func.print_ir() 
func.print_assembly()

addr = func.ptr()
executable_func = ctypes.CFUNCTYPE(
    ctypes.c_int64, 
    ctypes.POINTER(ctypes.c_int32),
)(addr)

data = np.array([8, 2, -5, 7, 6, -2, 1, 8, 3, 6, 7, 1, -7, 0, 9, 10], dtype = np.int32)
# Получаем указатель на данные массива
data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

executable_func(data_ptr)
print(data)


# cond1 = pyloops.and_(pyloops.and_(pyloops.and_(y >= x + 3, y <= 4), x >= -2), x <= 0)
# cond2 = pyloops.and_(pyloops.and_(pyloops.and_(y <= x - 1, x >= 0), y <= 0), x*x + y*y <= 9)
# out = pyloops.IReg(pyloops.or_(cond1, cond2))

# out += pyloops.select_(s_cond, 2, 0)