import sys
import ctypes
sys.path.append("/home/vtdrs/work/pyloops/build/")
import pyloops
import numpy as np

def condition_painter_reference():
    # Константы области отрисовки
    xmin, xmax = -5, 5
    ymin, ymax = -5, 5
    w = xmax - xmin + 1
    h = ymax - ymin + 1
    
    # Создаем массив для хранения результата (аналог буфера ptr)
    # Используем int64, так как в C++ указано sizeof(int64_t)
    data = np.zeros(w * h, dtype=np.int64)
    
    y = ymin
    while y <= ymax:
        # Смещение по строке: (y - ymin) * ширина
        offsety = (y - ymin) * w
        
        x = xmin
        while x <= xmax:
            # 1. Первое сложное логическое условие (результат 0 или 1)
            # (y >= x + 3 && y <= 4 && x >= -2 && x <= 0) OR
            # (y <= x - 1 && x >= 0 && y <= 0 && x*x + y*y <= 9)
            cond1 = (y >= x + 3 and y <= 4 and x >= -2 and x <= 0)
            cond2 = (y <= x - 1 and x >= 0 and y <= 0 and (x*x + y*y <= 9))
            
            out = 1 if (cond1 or cond2) else 0
            
            # 2. Обработка SELECT
            # Добавляем 2, если условие истинно, иначе 0
            # ((x >= 2 && x <= 4) OR (y >= 1 && y <= 3)) AND (x*x + y*y <= 16)
            select_cond = ((2 <= x <= 4) or (1 <= y <= 3)) and (x*x + y*y <= 16)
            out += 2 if select_cond else 0
            
            # 3. Блок IF_
            # ((2*y >= x AND y <= -(x+3)^2) OR (y <= 2*x AND y >= -(x+3)^2)) AND x <= 0
            poly = -(x + 3) * (x + 3)
            if_cond = (((2*y >= x and y <= poly) or (y <= 2*x and y >= poly)) and x <= 0)
            if if_cond:
                out += 3
            
            # 4. Сохранение (store_)
            # Вычисляем индекс: offsety + (x - xmin)
            # В C++ было (x - xmin) << 3, так как смещение в байтах для int64
            pixel_idx = offsety + (x - xmin)
            data[pixel_idx] = out
            
            x += 1
        y += 1
        
    return data.reshape((h, w))

ptr = pyloops.IReg()

xmin, xmax = -5, 5
ymin, ymax = -5, 5
w = xmax - xmin + 1

pyloops.start_func("condition_painter", ptr)

y = pyloops.IReg(ymin)

# Внешний цикл по Y
pyloops.while_(y <= ymax)
# offsety = (y - ymin) * w * sizeof(int64_t)
offsety = pyloops.IReg((y - ymin) * w * 8)
    
x = pyloops.IReg(xmin)
    
# Внутренний цикл по X
pyloops.while_(x <= xmax)
        
# --- OUT ---
cond1 = pyloops.and_(pyloops.and_(pyloops.and_(y >= x + 3, y <= 4), x >= -2), x <= 0)
cond2 = pyloops.and_(pyloops.and_(pyloops.and_(y <= x - 1, x >= 0), y <= 0), x*x + y*y <= 9)
        
# out = (cond1 || cond2)
out = pyloops.IReg(pyloops.or_(cond1, cond2))
        
# --- SELECT ---
s_part1 = pyloops.or_(pyloops.and_(x >= 2, x <= 4), pyloops.and_(y >= 1, y <= 3))
s_cond = pyloops.and_(s_part1, x*x + y*y <= 16)
        
out += pyloops.select_(s_cond, 2, 0)
        
# --- IF ---
# poly = -(x+3)*(x+3)
poly = pyloops.IReg(-(x + 3) * (x + 3))
if_logic = pyloops.and_(pyloops.or_(
pyloops.and_(2*y >= x, y <= poly),
pyloops.and_(y <= 2*x, y >= poly)
),
x <= 0
)
        
pyloops.if_(if_logic)
out += 3
pyloops.endif_()
        
# --- STORE ---
pyloops.store_(np.int64, ptr, offsety + ((x - xmin) << 3), out)
        
x += 1
pyloops.endwhile_()
    
y += 1
pyloops.endwhile_()

pyloops.return_()
pyloops.end_func()

func = pyloops.get_func("condition_painter")
func.print_ir()
func.print_assembly()

addr = func.ptr()
executable = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_int64))(addr)

# Тест:
print("=======REFERENCE RESULT=======")
result = condition_painter_reference()
print(result)

print("=======LOOPSMADE RESULT=======")
canvas = np.zeros((ymax - ymin + 1, xmax - xmin + 1), dtype=np.int64)
canvas_ptr = canvas.ctypes.data_as(ctypes.POINTER(ctypes.c_int64))
executable(canvas_ptr)
print(canvas)