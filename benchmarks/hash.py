from benchmarker import run_benchmark
import array
import numpy as np
import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops

def hash_python_arrays(input_array, output_array):
    """
    Реализация побитового хэширования MurmurHash3 Avalanche 
    на стандартных массивах Python (array.array).
    """
    # Константы фиксируем локально
    C1 = 0x85ebca6b
    C2 = 0xc2b2ae35
    MASK_32 = 0xFFFFFFFF
    
    for i in range(len(input_array)):
        h = input_array[i]
        
        # Каскад побитового перемешивания бит
        h ^= (h >> 16)
        h = (h * C1) & MASK_32
        
        h ^= (h >> 13)
        h = (h * C2) & MASK_32
        
        h ^= (h >> 16)
        
        output_array[i] = h
        

def hash_numpy(input_array, output_array):
    """
    Реализация побитового хэширования MurmurHash3 Avalanche
    с использованием векторизации NumPy (массивы np.uint32).
    """
    C1 = np.uint32(0x85ebca6b)
    C2 = np.uint32(0xc2b2ae35)
    
    h = input_array.copy()
    
    # Сдвигаем, явно указав тип константы сдвига как np.uint32
    h ^= (h >> np.uint32(16))
    h *= C1
    
    h ^= (h >> np.uint32(13))
    h *= C2
    
    h ^= (h >> np.uint32(16))
    
    output_array[:] = h
    
# =============================== Генерация loops-функции ===============================

iptr = pyloops.IReg()
optr = pyloops.IReg()
n = pyloops.IReg()

pyloops.start_func("murmur_hash_vector", iptr, optr, n)

offset = pyloops.IReg(0)
n *= 4  # uint32 (4 байта на элемент)

c1 = pyloops.VReg(np.uint32, 0x85ebca6b)
c2 = pyloops.VReg(np.uint32, 0xc2b2ae35)


pyloops.while_(offset < n)
v = pyloops.VReg(np.uint32, pyloops.loadvec(np.uint32, iptr, offset))

v ^= pyloops.ushift_right(v, 16)
v *= c1

v ^= pyloops.ushift_right(v, 13)
v *= c2

v ^= pyloops.ushift_right(v, 16)

pyloops.storevec(optr, offset, v)

# Сдвигаем смещение на ширину векторного регистра в байтах
offset += pyloops.vbytes()

pyloops.endwhile_()
pyloops.return_()
pyloops.end_func()

# Извлекаем скомпилированную функцию
_func = pyloops.get_func("murmur_hash_vector")
_addr = _func.ptr()

executable_func = ctypes.CFUNCTYPE(
    None,
    ctypes.POINTER(ctypes.c_uint32), # iptr
    ctypes.POINTER(ctypes.c_uint32), # optr
    ctypes.c_int64                   # n (размер массива)
)(_addr)

def hash_pyloops(input_array, output_array):
    """
    Обертка для бенчмарка, которая берет родные NumPy массивы,
    достает из них сырые C-указатели и отдает JIT-коду.
    """
    v_ptr = input_array.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
    out_ptr = output_array.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
    
    # Вызываем нашу скомпилированную JIT-функцию
    executable_func(v_ptr, out_ptr, len(input_array))

def verify_correctness():
    print("=== ЗАПУСК ПРОВЕРКИ КОРРЕКТНОСТИ АЛГОРИТМОВ ===")
    SIZE = 10_000
    in_py = array.array('I', (x for x in range(SIZE)))
    out_py = array.array('I', [0] * SIZE)
    in_np = np.arange(SIZE, dtype=np.uint32)
    out_np = np.zeros_like(in_np)
    in_loops = np.arange(SIZE, dtype=np.uint32)
    out_loops = np.zeros_like(in_loops)
    hash_python_arrays(in_py, out_py)
    hash_numpy(in_np, out_np)
    hash_pyloops(in_loops, out_loops)
    out_py_np = np.array(out_py, dtype=np.uint32)
    py_vs_np = np.array_equal(out_py_np, out_np)
    np_vs_loops = np.array_equal(out_np, out_loops)
    if  py_vs_np and np_vs_loops:
        print("Тест на корректность пройден!")
    else:
        print("Тест на корректность не пройден!")

verify_correctness()
# =============================== Запуск бенчмарков ===============================
print("=== ЗАПУСК БЕНЧМАРКА ===")
ARRAY_SIZE = 1_000_000 

# Данные для чистого Python (array.array)
in_data = array.array('I', (x for x in range(ARRAY_SIZE)))
out_data = array.array('I', [0] * ARRAY_SIZE)

# Данные для NumPy и pyloops
in_data_np = np.arange(ARRAY_SIZE, dtype=np.uint32)
out_data_np = np.zeros_like(in_data_np)

# Запуск бенчмарков
print("Тест на стандартных массивах Python")
run_benchmark(hash_python_arrays, in_data, out_data, num_runs=10, warmup_runs=1)

print("Тест на массивах NumPy")
run_benchmark(hash_numpy, in_data_np, out_data_np, num_runs=10, warmup_runs=1)

print("Тест на векторизованном JIT pyloops")
run_benchmark(hash_pyloops, in_data_np, out_data_np, num_runs=10, warmup_runs=1)