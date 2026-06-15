from benchmarker import run_benchmark
import array
import math
import numpy as np
import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops
import contextlib

@contextlib.contextmanager
def while_(cond):
    pyloops.while_(cond)
    try:
        yield
    finally:
        pyloops.endwhile_()

@contextlib.contextmanager 
def if_(cond):
    pyloops.if_(cond)
    try:
        yield
    finally:
        pyloops.endif_()
        
def indent():
    """Исключительно визуальный сдвиг кода для генератора"""
    yield


def generate_sobel_vectors(k_size):
    """Динамическая генерация векторов сглаживания и дифференцирования для Собеля."""
    if k_size % 2 == 0 or k_size < 3:
        raise ValueError("Размер ядра должен быть нечетным и >= 3")
        
    # Вектор сглаживания (строка треугольника Паскаля для k_size - 1)
    smoothing = [1]
    for i in range(1, k_size):
        smoothing.append(smoothing[-1] * (k_size - i) // i)
        
    # Вектор дифференцирования (коэффициенты центральной разности)
    # Для окна 3: [-1, 0, 1], для 5: [-1, -2, 0, 2, 1] и т.д.
    radius = k_size // 2
    diff = []
    for x in range(-radius, radius + 1):
        if x == 0:
            diff.append(0.0)
        else:
            # Масштабируем веса в зависимости от удаления от центра
            diff.append(float(x) / abs(x) * (radius - abs(x) + 1))
            
    return smoothing, diff


def sobel_python_arrays(in_array, out_array, owidth, oheight, k_size):
    """
    Реализация 2D фильтра Собеля Gx над стандартным массивом Python (array.array).
    Размер ядра k_size параметризуется динамически.
    """
    iwidth = owidth + k_size - 1
    kernel_y, kernel_x = generate_sobel_vectors(k_size)

    # Очищаем выходной массив нулями
    for i in range(len(out_array)):
        out_array[i] = 0.0

    # Основной цикл по внутренним пикселям изображения
    for y in range(0, oheight):
        for x in range(0, owidth):
            pixel_sum = 0.0
            
            # Свёртка скользящим окном
            for ky in range(k_size):
                row_offset = (y + ky) * iwidth
                weight_y = kernel_y[ky]
                
                for kx in range(k_size):
                    pixel_idx = row_offset + (x + kx)
                    weight_x = kernel_x[kx]
                    
                    pixel_sum += in_array[pixel_idx] * (weight_y * weight_x)
            
            out_array[y * owidth + x] = pixel_sum
 
def sobel_numpy(input_array, output_array, owidth, oheight, k_size):
    """
    Высокооптимизированная NumPy реализация 2D Собеля Gx (без паддинга).
    Принимает одномерные массивы float32.
    Размеры owidth и oheight задают геометрию ВЫХОДНОГО массива.
    """
    radius = k_size // 2
    
    # Вычисляем размеры исходного изображения на основе выходных размеров и k_size
    width = owidth + k_size - 1
    height = oheight + k_size - 1
    
    # Генерируем 1D-векторы
    # Вектор сглаживания через полиномиальное умножение [1, 1] само на себя
    smoothing = np.array([1], dtype=np.float32)
    for _ in range(k_size - 1):
        smoothing = np.convolve(smoothing, [1, 1])
        
    # Вектор дифференцирования
    x_range = np.arange(-radius, radius + 1, dtype=np.float32)
    diff = np.where(x_range == 0, 0.0, np.sign(x_range) * (radius - np.abs(x_range) + 1)).astype(np.float32)
    
    # Собираем полноценное 2D-ядро Собеля через внешнее произведение (Outer Product)
    kernel_2d = np.outer(smoothing, diff).astype(np.float32)
    
    # Решейп входного плоского массива в полную 2D-картинку (с учетом вычисленных width и height)
    img_2d = input_array.reshape((height, width))
    
    # Обнуляем и решейпим выходной массив под oheight и owidth
    output_array.fill(0.0)
    out_2d = output_array.reshape((oheight, owidth))
    
    # Скользящие окна NumPy. Так как img_2d имеет размер (oheight + k_size - 1, owidth + k_size - 1),
    # то windows автоматически получит форму (oheight, owidth, k_size, k_size)
    windows = np.lib.stride_tricks.sliding_window_view(img_2d, (k_size, k_size))
    
    # Перемножаем окна на матрицу ядра и записываем результат прямо в out_2d.
    # Благодаря валидным размерам входных данных, результат einsum идеально ложится в out_2d.
    np.einsum('ijkl,kl->ij', windows, kernel_2d, out=out_2d)

def vertical_sum(k_size, iptr, offset, iwidth):
    kernel_y, _ = generate_sobel_vectors(k_size)
    loadoffset = pyloops.IReg(offset)
    vsum = pyloops.VReg(np.float32, pyloops.loadvec(np.float32, iptr, loadoffset))
    loadoffset += iwidth
    for kx in range(1, k_size-1):
        justloaded = pyloops.VReg(np.float32, pyloops.loadvec(np.float32, iptr, loadoffset))
        y_weight = pyloops.VReg(np.float32, kernel_y[kx])
        vsum.assign = pyloops.fma(vsum, y_weight, justloaded)
        loadoffset += iwidth
    vsum += pyloops.VReg(np.float32, pyloops.loadvec(np.float32, iptr, loadoffset))
    return vsum

def extract_subvec(vsums, vsum, kpos):
    lanes = pyloops.vbytes() // 4
    v1num = kpos // lanes
    v1 = vsums[v1num] if v1num < len(vsums) else vsum
    offset = kpos % lanes
    if offset == 0:
        return v1
    v2 = vsums[v1num + 1] if (v1num + 1) < len(vsums) else vsum
    return pyloops.VReg(np.float32, pyloops.ext(v1,v2,offset))

def compile_sobel_gx(k_size):
    """
    Генератор JIT-функции Собеля Gx.
    Компилируется один раз для конкретного k_size.
    Сгенерированная функция принимает: iptr, optr, width, height.
    """
    radius = k_size // 2
    kernel_y, kernel_x = generate_sobel_vectors(k_size)
    
    # Собираем все ненулевые коэффициенты.
    # Вместо готового byte_offset храним отдельно (delta_y, delta_x) в пикселях
    print(f"Kernel X:{kernel_x}" )
    print(f"Kernel Y:{kernel_y}" )
    active_weights = []
    for ky in range(k_size):
        dy = ky - radius  # Смещение по вертикали (в строках)
        wy = kernel_y[ky]
        for kx in range(k_size):
            dx = kx - radius  # Смещение по горизонтали (в пикселях)
            wx = kernel_x[kx]
            weight = float(wy * wx)
            if weight != 0.0:
                active_weights.append((dy, dx, weight))
    # =============================== Генерация loops-функции ===============================
    iptr = pyloops.IReg()
    optr = pyloops.IReg()
    owidth = pyloops.IReg()
    oheight = pyloops.IReg()
    lanes = pyloops.vbytes() // 4
    inputs_per_output = math.ceil(((k_size - 1) + (lanes - 1)) / lanes)

    func_name = f"sobel_gx_fma_k{k_size}"
    # Теперь у JIT-функции 4 входных параметра
    pyloops.start_func(func_name, iptr, optr, owidth, oheight)
    orow = pyloops.IReg(0)
    owidth *= 4 # sizeof(float)
    iwidth = pyloops.IReg(owidth + (4 * (k_size - 1)))
    
    with while_(orow < oheight):
        vsums = []
        ioffset = pyloops.IReg(0)
        ooffset = pyloops.IReg(0)
        for i in range(inputs_per_output - 1):
            vsums.append(vertical_sum(k_size, iptr, ioffset, iwidth))
            ioffset += pyloops.vbytes()
        with while_(ooffset < owidth):
            # Halide trick
            with if_(ooffset + pyloops.vbytes() > owidth):
                halideshift = pyloops.IReg(ooffset - owidth + pyloops.vbytes())
                ooffset -= halideshift
                halideshift += (inputs_per_output - 1) * pyloops.vbytes()
                ioffset -= halideshift
                for i in range(inputs_per_output - 1):
                    vsums.append(vertical_sum(k_size, iptr, ioffset, iwidth))
                    ioffset += pyloops.vbytes()
            vsum = vertical_sum(k_size, iptr, ioffset, iwidth)
            ioffset += pyloops.vbytes()
            m1 = pyloops.VReg(np.float32, -1.0)
            sum = pyloops.VReg(np.float32, m1 * vsums[0])
            multiplier = m1
            if k_size//2 > 1:
                multiplier = pyloops.VReg(np.float32, m1 + m1)
            for i in range(1, k_size//2):
                kpos = i
                extracted = extract_subvec(vsums, vsum, kpos)
                sum.assign = pyloops.fma(sum, extracted, m1) 
                if (i < k_size//2 - 1):
                    multiplier += m1
            multiplier *= m1
            for i in range(k_size//2 + 1, k_size - 1):
                kpos = i
                extracted = extract_subvec(vsums, vsum, kpos)
                sum.assign = pyloops.fma(sum, extracted, m1) 
                if (i < k_size - 1):
                    multiplier += m1
            extracted = extract_subvec(vsums, vsum, k_size - 1)
            sum.assign = pyloops.fma(sum, extracted, m1)
            pyloops.storevec(optr, ooffset, sum)
            for i in range(inputs_per_output - 1):
                src = vsums[i + 1] if (i + 1) < len(vsums) else vsum
                vsums[i].assign = src
            ooffset += pyloops.vbytes()
        iptr += iwidth
        optr += owidth
        orow += 1
    pyloops.return_()
    pyloops.end_func()

    # Извлекаем скомпилированную функцию
    _func = pyloops.get_func(func_name)
    _addr = _func.ptr()
    
    # Возвращаем исполняемый ctypes-объект с новой чистой сигнатурой
    return ctypes.CFUNCTYPE(
        None,
        ctypes.POINTER(ctypes.c_float), # iptr
        ctypes.POINTER(ctypes.c_float), # optr
        ctypes.c_int64,                 # width
        ctypes.c_int64                  # height
    )(_addr)

loops_kernels_dict = dict()
def run_sobel_loops(img_in, img_out, width, height, ksize):
    in_ptr = img_in.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    out_ptr = img_out.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    kernel = loops_kernels_dict.get(ksize)
    if kernel == None:
        loops_kernels_dict[ksize] = compile_sobel_gx(ksize)
        kernel = loops_kernels_dict[ksize]
    kernel(in_ptr, out_ptr, width, height)


def verify_correctness():
    correctness_epsilon = 0.0005
    ksizes = [3,5,9]
    print("=== ЗАПУСК ПРОВЕРКИ КОРРЕКТНОСТИ АЛГОРИТМОВ ===")
    for K_SIZE in ksizes:
        WIDTH = 256
        HEIGHT = 256
        OWIDTH = WIDTH - K_SIZE + 1
        OHEIGHT = HEIGHT - K_SIZE + 1
        TOTAL_PIXELS = WIDTH * HEIGHT
        OTOTAL_PIXELS = OWIDTH * OHEIGHT
        in_py = array.array('f', [float(i % 256) for i in range(TOTAL_PIXELS)])
        out_py = array.array('f', [0.0] * OTOTAL_PIXELS)
        in_np = np.array(in_py, dtype=np.float32)
        out_np = np.zeros(OTOTAL_PIXELS, dtype=np.float32)
        out_loops = np.zeros(OTOTAL_PIXELS, dtype=np.float32)
        sobel_python_arrays(in_py, out_py, OWIDTH, OHEIGHT, K_SIZE)
        sobel_numpy(in_np, out_np, OWIDTH, OHEIGHT, K_SIZE)
        run_sobel_loops(in_np, out_loops, OWIDTH, OHEIGHT, K_SIZE)
        out_py_np = np.array(out_py, dtype=np.float32)
        py_vs_np_absdiff = np.abs(out_py_np - out_np)
        loops_vs_np_absdiff = np.abs(out_loops - out_np)
        py_vs_np_maxdiff = np.max(py_vs_np_absdiff)
        loops_vs_np_maxdiff = np.max(loops_vs_np_absdiff)
        if  py_vs_np_maxdiff < correctness_epsilon and loops_vs_np_maxdiff < correctness_epsilon:
            print(f"Тест на корректность пройден для размера ядра = {K_SIZE}!")
        else:
            print(f"Тест на корректность не пройден для размера ядра = {K_SIZE}!")

verify_correctness()
# =============================== Запуск бенчмарков ===============================
print("=== ЗАПУСК БЕНЧМАРКА ===")
ksizes = [3, 5, 9]
for K_SIZE in ksizes:
    WIDTH = 1024
    HEIGHT = 1024
    OWIDTH = WIDTH - K_SIZE + 1
    OHEIGHT = HEIGHT - K_SIZE + 1
    TOTAL_PIXELS = WIDTH * HEIGHT
    OTOTAL_PIXELS = OWIDTH * OHEIGHT

    # Данные для чистого Python (array.array)
    in_data = array.array('f', [float(i % 256) for i in range(TOTAL_PIXELS)])
    out_data = array.array('f', [0.0] * OTOTAL_PIXELS)

    # Данные для NumPy и pyloops
    in_data_np = np.array(in_data, dtype=np.float32)
    out_data_np = np.zeros(OTOTAL_PIXELS, dtype=np.float32)

    # Запуск бенчмарков
    print(f"Тест на стандартных массивах Python. KSize = {K_SIZE}")
    run_benchmark(sobel_python_arrays, in_data, out_data, OWIDTH, OHEIGHT, K_SIZE, num_runs=10, warmup_runs=1)

    print(f"Тест на массивах NumPy. KSize = {K_SIZE}")
    run_benchmark(sobel_numpy, in_data_np, out_data_np, OWIDTH, OHEIGHT, K_SIZE, num_runs=10, warmup_runs=1)

    print(f"Тест на векторизованном JIT pyloops. KSize = {K_SIZE}")
    run_benchmark(run_sobel_loops, in_data_np, out_data_np, OWIDTH, OHEIGHT, K_SIZE, num_runs=10, warmup_runs=1)