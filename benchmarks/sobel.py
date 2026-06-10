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


def sobel_python_arrays(in_array, out_array, width, height, k_size):
    """
    Реализация 2D фильтра Собеля Gx над стандартным массивом Python (array.array).
    Размер ядра k_size параметризуется динамически.
    """
    radius = k_size // 2
    kernel_y, kernel_x = generate_sobel_vectors(k_size)

    # Очищаем выходной массив нулями
    for i in range(len(out_array)):
        out_array[i] = 0.0

    # Основной цикл по внутренним пикселям изображения
    for y in range(radius, height - radius):
        for x in range(radius, width - radius):
            pixel_sum = 0.0
            
            # Свёртка скользящим окном
            for ky in range(k_size):
                row_offset = (y + ky - radius) * width
                weight_y = kernel_y[ky]
                
                for kx in range(k_size):
                    pixel_idx = row_offset + (x + kx - radius)
                    weight_x = kernel_x[kx]
                    
                    pixel_sum += in_array[pixel_idx] * (weight_y * weight_x)
            
            out_array[y * width + x] = pixel_sum
 
import numpy as np

def sobel_numpy(input_array, output_array, width, height, k_size):
    """
    Высокооптимизированная NumPy реализация 2D Собеля Gx.
    Принимает одномерные массивы float32, трансформирует в 2D 
    и использует sliding_window_view для универсальной свертки.
    """
    radius = k_size // 2
    
    # Генерируем 1D-векторы
    # Вектор сглаживания через полиномиальное умножение [1, 1] само на себя
    smoothing = np.array([1], dtype=np.float32)
    for _ in range(k_size - 1):
        smoothing = np.convolve(smoothing, [1, 1])
        
    # Вектор дифференцирования
    x_range = np.arange(-radius, radius + 1, dtype=np.float32)
    diff = np.where(x_range == 0, 0.0, np.sign(x_range) * (radius - np.abs(x_range) + 1)).astype(np.float32)
    
    # Собираем полноценное 2D-ядро Собеля через внешнее произведение (Outer Product)
    # kernel_2d[y, x] = smoothing[y] * diff[x]
    kernel_2d = np.outer(smoothing, diff).astype(np.float32)
    
    # Решейп входного плоского массива в 2D-картинку
    img_2d = input_array.reshape((height, width))
    
    # Обнуляем выходной массив
    output_array.fill(0.0)
    out_2d = output_array.reshape((height, width))
    
    # Скользящие окна NumPy
    windows = np.lib.stride_tricks.sliding_window_view(img_2d, (k_size, k_size))
    
    # Перемножаем все окна на наше 2D-ядро по двум последним осям (свёртка)
    # np.sum(windows * kernel_2d, axis=(2, 3))
    # 'ij...,...kl->ij...' перемножаем последние оси окон на матрицу ядра и складываем
    conv_result = np.einsum('ijkl,kl->ij', windows, kernel_2d)
    
    # Записываем результат во внутреннюю область выходного массива (сохраняя границы нулевыми)
    out_2d[radius:height-radius, radius:width-radius] = conv_result

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
    width = pyloops.IReg()
    height = pyloops.IReg()

    func_name = f"sobel_gx_fma_k{k_size}"
    # Теперь у JIT-функции 4 входных параметра
    pyloops.start_func(func_name, iptr, optr, width, height)

    # Вычисляем общее количество байт во входном массиве: N_bytes = width * height * 4
    n_bytes = pyloops.IReg(width)
    n_bytes *= height
    n_bytes *= 4

    # Вычисляем шаг одной строки в байтах: row_stride_bytes = width * 4
    row_stride_bytes = pyloops.IReg(width)
    row_stride_bytes *= 4

    # Границы обработки, чтобы не вылетать за верхний и нижний края (пропускаем radius строк)
    start_offset = pyloops.IReg(row_stride_bytes)
    start_offset *= radius

    end_limit = pyloops.IReg(n_bytes)
    end_limit -= start_offset

    # Основной рабочий индекс смещения в байтах
    offset = pyloops.IReg(start_offset)

    pyloops.while_(offset < end_limit)
    with indent():
        # Инициализируем вектор-аккумулятор нулями
        acc = pyloops.VReg(np.float32, 0.0)
        
        # Разворачиваем цикл свёртки (Loop Unrolling) в коде Python
        for dy, dx, weight in active_weights:
            # Считаем смещение для текущего соседа в рантайме JIT
            # Базовое смещение = текущий offset
            current_offset = pyloops.IReg(offset)
            
            # 1. Добавляем смещение по вертикали (dy * row_stride_bytes)
            if dy != 0:
                # Временный регистр для смещения строки
                row_shift = pyloops.IReg(row_stride_bytes)
                if abs(dy) > 1:
                    row_shift *= abs(dy)
                
                if dy > 0:
                    current_offset += row_shift
                else:
                    current_offset -= row_shift

            # 2. Добавляем смещение по горизонтали (dx * 4 байта)
            if dx != 0:
                byte_shift_x = dx * 4
                if byte_shift_x > 0:
                    current_offset += byte_shift_x
                else:
                    current_offset -= abs(byte_shift_x)
            
            # Загружаем вектор данных (current_offset теперь гарантированно IReg)
            v_data = pyloops.VReg(np.float32, pyloops.loadvec(np.float32, iptr, current_offset))
            
            # Создаем вектор-константу для веса
            v_weight = pyloops.VReg(np.float32, weight)
            
            # Накапливаем через fma
            acc = pyloops.fma(v_data, v_weight, acc)

        # Сохраняем результат
        pyloops.storevec(optr, offset, acc)

        # Сдвигаем базовое смещение на ширину векторного регистра (в байтах)
        offset += pyloops.vbytes()

    pyloops.endwhile_()
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
        TOTAL_PIXELS = WIDTH * HEIGHT
        in_py = array.array('f', [float(i % 256) for i in range(TOTAL_PIXELS)])
        out_py = array.array('f', [0.0] * TOTAL_PIXELS)
        in_np = np.array(in_py, dtype=np.float32)
        out_np = np.zeros(TOTAL_PIXELS, dtype=np.float32)
        out_loops = np.zeros(TOTAL_PIXELS, dtype=np.float32)
        sobel_python_arrays(in_py, out_py, WIDTH, HEIGHT, K_SIZE)
        sobel_numpy(in_np, out_np, WIDTH, HEIGHT, K_SIZE)
        run_sobel_loops(in_np, out_loops, WIDTH, HEIGHT, K_SIZE)
        out_py_np = np.array(out_py, dtype=np.float32)
        py_vs_np_absdiff = np.abs(out_py_np - out_np)
        loops_vs_np_absdiff = np.abs(out_loops - out_np)
        py_vs_np_maxdiff = np.max(py_vs_np_absdiff)
        print(py_vs_np_maxdiff)
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
    TOTAL_PIXELS = WIDTH * HEIGHT

    # Данные для чистого Python (array.array)
    in_data = array.array('f', [float(i % 256) for i in range(TOTAL_PIXELS)])
    out_data = array.array('f', [0.0] * TOTAL_PIXELS)

    # Данные для NumPy и pyloops
    in_data_np = np.array(in_data, dtype=np.float32)
    out_data_np = np.zeros(TOTAL_PIXELS, dtype=np.float32)

    # Запуск бенчмарков
    print(f"Тест на стандартных массивах Python. KSize = {K_SIZE}")
    run_benchmark(sobel_python_arrays, in_data, out_data, WIDTH, HEIGHT, K_SIZE, num_runs=10, warmup_runs=1)

    print(f"Тест на массивах NumPy. KSize = {K_SIZE}")
    run_benchmark(sobel_numpy, in_data_np, out_data_np, WIDTH, HEIGHT, K_SIZE, num_runs=10, warmup_runs=1)

    print(f"Тест на векторизованном JIT pyloops. KSize = {K_SIZE}")
    run_benchmark(run_sobel_loops, in_data_np, out_data_np, WIDTH, HEIGHT, K_SIZE, num_runs=10, warmup_runs=1)