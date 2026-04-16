import numpy as np
import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

# ptr = pyloops.IReg()
# n = pyloops.IReg()
# minpos_addr = pyloops.IReg()
# maxpos_addr = pyloops.IReg()
# # 1. pyloops.load_i32(ptr) (принимает на вход IExpr). Обычная функция, по аналогии с getFunc[который возвращает Func], но возвращает IReg.
# # 2. if_, endif_, while_, endwhile_ (похоже на startFunc)
# # 3. iadd с константами(например, i+=4), isub, imul, idiv, тоже в двух. 
# # 4. pyloops.store_i32(ptr, val) (не возвращает IReg, меняет память, обычная функция)


# pyloops.start_func("minmaxloc", ptr, n, minpos_addr, maxpos_addr)
# i = pyloops.IReg(0)
# minpos = pyloops.IReg(0)
# maxpos = pyloops.IReg(0)
# minval = pyloops.IReg(pyloops.load_i32(ptr))
# maxval = pyloops.IReg(minval)
# n *= 4
# pyloops.while_(i < n)
# x = pyloops.IReg(pyloops.load_i32(ptr, i))
# pyloops.if_(x < minval)
# minval.assign = x
# minpos.assign = i
# pyloops.endif()
# pyloops.if_(x > maxval)
# maxval.assign = x
# maxpos.assign = i
# pyloops.endif()
# i += 4
# pyloops.endwhile()
# elemsize = pyloops.const_(4)
# minpos /= elemsize
# maxpos /= elemsize
# pyloops.store_i32(minpos_addr, minpos)
# pyloops.store_i32(maxpos_addr, maxpos)
# pyloops.return_(0)
# pyloops.end_func()

pyloops.hi()
func = pyloops.get_func("minmaxloc")
func.print_ir()
func.print_assembly()

addr = func.ptr()

# 3. Создаем "вызываемую" функцию через ctypes
# Мы говорим ctypes: "По этому адресу лежит функция, которая возвращает void"
# (None для restype означает void)
executable_func = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_int32), ctypes.c_int64, ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_int32))(addr)

# 4. Подготовка данных
data = np.array([8, 2, -5, 7, 6], dtype=np.int32)
minpos = np.array([-1], dtype=np.int32)
maxpos = np.array([-1], dtype=np.int32)

# Получаем указатель на данные массива
data_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
minpos_ptr = minpos.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
maxpos_ptr = maxpos.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))

res = executable_func(data_ptr, 5, minpos_ptr, maxpos_ptr)
print(minpos)
print(maxpos)