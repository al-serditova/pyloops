import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops
import numpy as np

def bresenham_python(canvas, w, x0, y0, x1, y1, filler):
    """
    Python-реализация алгоритма Брезенхема.
    canvas: одномерный numpy массив (dtype=np.uint8)
    w: ширина холста (stride)
    """
    # Расчет дельт
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    
    # Расчет направлений (sx, sy)
    # Аналог C++: (x1 - x0) == 0 ? 0 : ((x1 - x0) > 0 ? 1 : -1)
    sx = 0 if (x1 - x0) == 0 else (1 if (x1 - x0) > 0 else -1)
    sy = 0 if (y1 - y0) == 0 else (1 if (y1 - y0) > 0 else -1)
    
    error = dx + dy
    
    # Работаем с локальными копиями координат, чтобы не менять входные данные
    curr_x, curr_y = x0, y0
    
    while True:
        # canvas[y0 * w + x0] = (uint8_t)filler
        # В numpy используем одномерную индексацию
        canvas[curr_y * w + curr_x] = np.uint8(filler)
        
        # Проверка выхода
        if curr_x == x1 and curr_y == y1:
            break
            
        e2 = 2 * error
        
        if e2 >= dy:
            if curr_x == x1:
                break
            error += dy
            curr_x += sx
            
        if e2 <= dx:
            if curr_y == y1:
                break
            error += dx
            curr_y += sy
            
# Объявляем регистры-аргументы
canvas_ptr = pyloops.IReg()
w = pyloops.IReg()
x0 = pyloops.IReg()
y0 = pyloops.IReg()
x1 = pyloops.IReg()
y1 = pyloops.IReg()
filler = pyloops.IReg()

# 1. Начало функции
pyloops.start_func("bresenham", canvas_ptr, w, x0, y0, x1, y1, filler)
# 2. Инициализация (dx = abs(x1 - x0), dy = -abs(y1 - y0))
dx = pyloops.IReg(abs(x1 - x0))
sx = pyloops.IReg(pyloops.sign(x1 - x0))

dy = pyloops.IReg(-abs(y1 - y0))
sy = pyloops.IReg(pyloops.sign(y1 - y0))

# 4. Ошибка
error = pyloops.IReg(dx + dy)

# 5. Главный цикл
pyloops.while_(canvas_ptr != 0)
pyloops.store_(np.uint8, canvas_ptr, y0 * w + x0, filler)
    
# if (x0 == x1 && y0 == y1) break
pyloops.if_(pyloops.and_(x0 == x1, y0 == y1))
pyloops.break_()
pyloops.endif_()
    
# e2 = 2 * error
e2 = pyloops.IReg(error << 1)
    
# Первый блок коррекции (по X)
pyloops.if_(e2 >= dy)
pyloops.if_(x0 == x1)
pyloops.break_()
pyloops.endif_()
error.assign = error + dy
x0.assign = x0 + sx
pyloops.endif_()
    
# Второй блок коррекции (по Y)
pyloops.if_(e2 <= dx)
pyloops.if_(y0 == y1)
pyloops.break_()
pyloops.endif_()
error.assign = error + dx
y0.assign = y0 + sy
pyloops.endif_()

pyloops.endwhile_()

pyloops.return_()
pyloops.end_func()

# --- Извлечение и запуск ---
func = pyloops.get_func("bresenham")
# func.print_assembly() # Проверь, как красиво легли инструкции!
func.print_ir()
func.print_assembly()

addr = func.ptr()

# Сигнатура: void(uint8*, int64, int64, int64, int64, int64, uint64)
executable_func = ctypes.CFUNCTYPE(
    None, 
    ctypes.POINTER(ctypes.c_uint8), # canvas
    ctypes.c_int64,                # w
    ctypes.c_int64,                # x0
    ctypes.c_int64,                # y0
    ctypes.c_int64,                # x1
    ctypes.c_int64,                # y1
    ctypes.c_uint64                # filler
)(addr)

# Тест:
print("=======REFERENCE RESULT=======")
width, height = 15, 15
canvas = np.zeros(width * height, dtype=np.uint8)
bresenham_python(canvas, width, 0, 0, 14, 14, 1)
bresenham_python(canvas, width, 0, 11, 7, 7, 2)
bresenham_python(canvas, width, 14, 7, 11, 14, 3)
bresenham_python(canvas, width, 5, 3, 8, 3, 4)
bresenham_python(canvas, width, 9, 2, 9, 13, 5)
bresenham_python(canvas, width, 14, 1, 14, 1, 6)
print(canvas.reshape((height, width)))

print("=======LOOPSMADE RESULT=======")
canvas = np.zeros(width * height, dtype=np.uint8)
canvas_ptr_data = canvas.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8))
executable_func(canvas_ptr_data, width, 0, 0, 14, 14, 1)
executable_func(canvas_ptr_data, width, 0, 11, 7, 7, 2)
executable_func(canvas_ptr_data, width, 14, 7, 11, 14, 3)
executable_func(canvas_ptr_data, width, 5, 3, 8, 3, 4)
executable_func(canvas_ptr_data, width, 9, 2, 9, 13, 5)
executable_func(canvas_ptr_data, width, 14, 1, 14, 1, 6)
print(canvas.reshape((height, width)))