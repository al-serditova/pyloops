import numpy as np
import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

ptr = pyloops.IReg()
n = pyloops.IReg()
minpos_addr = pyloops.IReg()
maxpos_addr = pyloops.IReg()

pyloops.start_func("minmaxloc", ptr, n, minpos_addr, maxpos_addr)
i = pyloops.IReg(0)
minpos = pyloops.IReg(0)
maxpos = pyloops.IReg(0)
minval = pyloops.IReg(pyloops.load_i32(ptr))
maxval = pyloops.IReg(minval)
n *= 4
pyloops.while_(i < n)
x = pyloops.IReg(pyloops.load_i32(ptr, i))
pyloops.if_(x < minval)
minval.assign = x
minpos.assign = i
pyloops.endif_()
pyloops.if_(x > maxval)
maxval.assign = x
maxpos.assign = i
pyloops.endif_()
i += 4
pyloops.endwhile_()
elemsize = pyloops.IReg(4)
minpos //= elemsize
maxpos //= elemsize
pyloops.store_i32(minpos_addr, minpos)
pyloops.store_i32(maxpos_addr, maxpos)
pyloops.return_(0)
pyloops.end_func()

func = pyloops.get_func("minmaxloc")
func.print_ir()
func.print_assembly()

addr = func.ptr()

executable_func = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_int32), ctypes.c_int64, ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_int32))(addr)

data = np.array([8, 2, -5, 7, -16], dtype=np.int32)
minpos = np.array([-1], dtype=np.int32)
maxpos = np.array([-1], dtype=np.int32)

data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
minpos_ptr = minpos.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
maxpos_ptr = maxpos.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

res = executable_func(data_ptr, 5, minpos_ptr, maxpos_ptr)
print(minpos)
print(maxpos)