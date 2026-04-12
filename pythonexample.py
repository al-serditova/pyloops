import sys
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

a = pyloops.IReg()
b = pyloops.IReg()

pyloops.start_func("a_plus_b", a, b) #Only a,b signature.
a += b
pyloops.Return(a)
pyloops.end_func()

pyloops.finish()



