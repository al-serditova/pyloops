import numpy as np
import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

ptr = pyloops.IReg()
n = pyloops.IReg()
pyloops.start_func("non_negative_odd", ptr, n)
i = pyloops.IReg(0)
res = pyloops.IReg(-4)
n *= 4

pyloops.while_(i < n)
x = pyloops.IReg(pyloops.load_(np.int32, ptr, i))
pyloops.if_(x < 0)
i += 4
pyloops.continue_()
pyloops.endif_()

mod = x % 2
pyloops.if_(mod != 0)
res.assign = i
pyloops.break_()
pyloops.endif_()

i += 4
pyloops.endwhile_()

res //= 4
pyloops.return_(res)
pyloops.end_func()

func = pyloops.get_func("non_negative_odd")
func.print_ir()
func.print_assembly()

addr = func.ptr()

executable_func = ctypes.CFUNCTYPE(ctypes.c_int64, ctypes.POINTER(ctypes.c_int32), ctypes.c_int64)(addr)

data = np.array([8, 2, -5, 7, -16], dtype=np.int32)
data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

res = executable_func(data_ptr, len(data))
print(res)