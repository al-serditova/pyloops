import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

a = pyloops.IReg()
b = pyloops.IReg()

pyloops.start_func("a_plus_b", a, b) #Only a,b signature.
a += b
pyloops.return_(a)
pyloops.end_func()

func = pyloops.get_func("a_plus_b")
func.print_ir()
func.print_assembly()

addr = func.ptr()
executable_func = ctypes.CFUNCTYPE(ctypes.c_int64, ctypes.c_int64, ctypes.c_int64)(addr)
res = executable_func(5, 1)
print(res)