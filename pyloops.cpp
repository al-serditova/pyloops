#include <Python.h>
#include <string>
#include "common.hpp"
#include "ireg.hpp"
#include "func.hpp"

#include <iostream>
#include "loops/loops.hpp"
#include "/home/vtdrs/work/loops/src/common.hpp"
#include "/home/vtdrs/work/loops/src/code_collecting.hpp"
#include "/home/vtdrs/work/loops/src/func_impl.hpp"

using namespace loops;
// Context ctx;

std::string get_numpy_type_name(PyObject* obj) {
    if (!obj) return "";
    PyObject* name_attr = nullptr;

    // Сценарий 1: Нам передали np.dtype('int32') или объект, у которого уже есть .name
    name_attr = PyObject_GetAttrString(obj, "name");

    // Сценарий 2: Нам передали сам класс (np.int32). 
    // У классов в Python есть атрибут __name__, который вернет "int32"
    if (!name_attr) {
        PyErr_Clear();
        name_attr = PyObject_GetAttrString(obj, "__name__");
    }

    // Сценарий 3: Нам передали массив, берем obj.dtype.name
    if (!name_attr) {
        PyErr_Clear();
        PyObject* dtype = PyObject_GetAttrString(obj, "dtype");
        if (dtype) {
            name_attr = PyObject_GetAttrString(dtype, "name");
            Py_DECREF(dtype);
        }
    }

    if (!name_attr) {
        PyErr_Clear();
        // Крайний случай: пробуем repr(obj) и смотрим, есть ли там знакомые слова, 
        // но лучше просто вернуть пустоту и выдать ошибку.
        return "";
    }

    // Извлекаем строку
    const char* name_str = PyUnicode_AsUTF8(name_attr);
    std::string result = name_str ? name_str : "";
    Py_DECREF(name_attr);

    // ВАЖНО: NumPy иногда возвращает имена вроде "intc" или "longlong" 
    // в зависимости от платформы. Возможно, стоит добавить нормализацию.
    return result;
}

template<typename T>
PyObject* generic_load(const loops::IExpr& base, PyObject* obj_offset) {
    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;

    // Сценарий 1: load(type, base) -> Без смещения
    if (obj_offset == nullptr || obj_offset == Py_None) {
        py_res->expr = new loops::IExpr(loops::load_<T>(base));
    }
    // Сценарий 2: load(type, base, offset)
    else {
        if (PyObject_TypeCheck(obj_offset, &PyIRegType)) {
            // Используем форму (base, IExpr offset)
            loops::IExpr offset = ((PyIReg*)obj_offset)->getExpr();
            py_res->expr = new loops::IExpr(loops::load_<T>(base, offset));
        } 
        else if (PyLong_Check(obj_offset)) {
            // Используем форму (base, int64_t offset)
            int64_t offset_val = PyLong_AsLongLong(obj_offset);
            py_res->expr = new loops::IExpr(loops::load_<T>(base, offset_val));
        } 
        else {
            Py_DECREF(py_res);
            PyErr_SetString(PyExc_TypeError, "Offset must be an IReg or an integer");
            return NULL;
        }
    }
    return (PyObject*)py_res;
}

template<typename T>
PyObject* generic_store(const loops::IExpr& base, PyObject* obj_offset, PyObject* obj_val)
{
    if (obj_val == nullptr) {
        obj_val = obj_offset;
        if (PyObject_TypeCheck(obj_val, &PyIRegType)) {
            loops::store_<T>(base, ((PyIReg*)obj_val)->getExpr());
        } else if (PyLong_Check(obj_val)) {
            loops::store_<T>(base, (int64_t)PyLong_AsLongLong(obj_val));
        } else {
            PyErr_SetString(PyExc_TypeError, "Value must be IReg or int");
            return NULL;
        }
    }
    // Сценарий 2: store_(base, offset, value) -> 3 аргумента
    else {
        if (PyObject_TypeCheck(obj_offset, &PyIRegType))
        {
            loops::IExpr offset = ((PyIReg*)obj_offset)->getExpr();
            if (PyObject_TypeCheck(obj_val, &PyIRegType))
            {
                loops::IExpr val = ((PyIReg*)obj_val)->getExpr();
                loops::store_<T>(base, offset, val);
            }
            else if (PyLong_Check(obj_val))
            {
                int64_t val = PyLong_AsLongLong(obj_val);
                loops::store_<T>(base, offset, val);
            }
            else
            {
                PyErr_SetString(PyExc_TypeError, "Stored must be IReg or int");
                return NULL;
            }
        }
        else if (PyLong_Check(obj_offset))
        {
            int64_t offset = PyLong_AsLongLong(obj_offset);
            if (PyObject_TypeCheck(obj_val, &PyIRegType))
            {
                loops::IExpr val = ((PyIReg*)obj_val)->getExpr();
                loops::store_<T>(base, offset, val);
            }
            else if (PyLong_Check(obj_val))
            {
                int64_t val = PyLong_AsLongLong(obj_val);
                loops::store_<T>(base, offset, val);
            }
            else
            {
                PyErr_SetString(PyExc_TypeError, "Stored must be IReg or int");
                return NULL;
            }                
        }
        else
        {
            PyErr_SetString(PyExc_TypeError, "Offset must be IReg or int");
            return NULL;
        }
    }
    return Py_None;
}

extern "C"
{

static PyObject* PyGetFunc(PyObject* self, PyObject* args) {
    const char* name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    // 1. Получаем реальный Func из контекста loops
    loops::Func f = ctx.getFunc(name);

    // 2. Создаем объект PyFunc для Python
    PyFunc* py_f = PyObject_New(PyFunc, &PyFuncType);
    if (py_f) {
        // Копируем объект Func в кучу, чтобы PyFunc владел им
        py_f->func = new loops::Func(f); 
    }
    return (PyObject*)py_f;
}

static PyObject* PyLoad(PyObject* self, PyObject* args) {
    PyObject *obj_type = nullptr;
    PyObject *obj_ptr = nullptr;
    PyObject *obj_offset = nullptr;

    // load(type, ptr, [offset]) -> "OO|O"
    if (!PyArg_ParseTuple(args, "OO|O", &obj_type, &obj_ptr, &obj_offset)) {
        return NULL;
    }

    // Проверка: база должна быть IReg (указатель)
    if (!PyObject_TypeCheck(obj_ptr, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "The second argument (pointer) must be an IReg");
        return NULL;
    }
    loops::IExpr base_expr = ((PyIReg*)obj_ptr)->getExpr();

    // Определяем тип через нашу обновленную функцию
    std::string t = get_numpy_type_name(obj_type);

    try {
        if (t == "int32")   return generic_load<int32_t>(base_expr, obj_offset);
        if (t == "uint32")  return generic_load<uint32_t>(base_expr, obj_offset);
        if (t == "int64")   return generic_load<int64_t>(base_expr, obj_offset);
        if (t == "uint64")  return generic_load<uint64_t>(base_expr, obj_offset);
        if (t == "int16")   return generic_load<int16_t>(base_expr, obj_offset);
        if (t == "uint16")  return generic_load<uint16_t>(base_expr, obj_offset);
        if (t == "int8")    return generic_load<int8_t>(base_expr, obj_offset);
        if (t == "uint8")   return generic_load<uint8_t>(base_expr, obj_offset);
        if (t == "float32") return generic_load<float>(base_expr, obj_offset);
        if (t == "float64") return generic_load<double>(base_expr, obj_offset);

        PyErr_Format(PyExc_TypeError, "Loops load doesn't support numpy type: '%s'", t.c_str());
        return NULL;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }
}

static PyObject* PyStore(PyObject* self, PyObject* args) {
    PyObject *obj_type = nullptr;
    PyObject *obj_base = nullptr;
    PyObject *obj_off_or_val = nullptr;
    PyObject *obj_val = nullptr;

    // "OOOO" — 3 обязательных, 1 опциональный (|)
    if (!PyArg_ParseTuple(args, "OOO|O", &obj_type, &obj_base, &obj_off_or_val, &obj_val)) {
        return NULL;
    }

    // Проверяем базу (адрес)
    if (!PyObject_TypeCheck(obj_base, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Base must be an IReg pointer");
        return NULL;
    }
    loops::IExpr base_expr = ((PyIReg*)obj_base)->getExpr();

    // Получаем имя типа NumPy
    std::string t = get_numpy_type_name(obj_type);

    try {
        if (t == "int32")   return generic_store<int32_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "uint32")  return generic_store<uint32_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "int64")   return generic_store<int64_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "uint64")  return generic_store<uint64_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "int16")   return generic_store<int16_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "uint16")  return generic_store<uint16_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "int8")    return generic_store<int8_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "uint8")   return generic_store<uint8_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "float16") return generic_store<f16_t>(base_expr, obj_off_or_val, obj_val);
        if (t == "float32") return generic_store<float>(base_expr, obj_off_or_val, obj_val);
        if (t == "float64") return generic_store<double>(base_expr, obj_off_or_val, obj_val);
        
        PyErr_Format(PyExc_TypeError, "Loops doesn't support numpy type: %s", t.c_str());
        return NULL;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }
}

static PyObject* PyIf(PyObject* self, PyObject* obj_a) {
    if (!PyObject_TypeCheck(obj_a, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Expected an IReg object");
        return NULL;
    }
    PyIReg* py_cond = (PyIReg*)obj_a;

    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->newiopNoret(OP_STEM_CSTART, {});
    CodeCollecting* coll = getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting();
    IExpr condition = py_cond->getExpr();
    Expr condition_(condition.notype());
    coll->if_(condition_);

    Py_RETURN_NONE;
}

static PyObject* PyEndIf(PyObject* self, PyObject* args) {
    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->endif_();
    Py_RETURN_NONE;
}

static PyObject* PyElse(PyObject* self, PyObject* args) {
    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->else_();
    Py_RETURN_NONE;
}

static PyObject* PyElif(PyObject* self, PyObject* obj_a) {
    if (!PyObject_TypeCheck(obj_a, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Expected an IReg object for elif");
        return NULL;
    }
    PyIReg* py_cond = (PyIReg*)obj_a;
    
    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->newiopNoret(OP_STEM_CSTART, {});
    CodeCollecting* coll = getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting();
    IExpr condition = py_cond->getExpr();
    Expr condition_(condition.notype());
    
    coll->elif_(condition_);

    Py_RETURN_NONE;
}

static PyObject* PyWhile(PyObject* self, PyObject* obj_a) {
    if (!PyObject_TypeCheck(obj_a, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Expected an IReg object");
        return NULL;
    }
    PyIReg* py_cond = (PyIReg*)obj_a;

    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->newiopNoret(OP_STEM_CSTART, {});
    CodeCollecting* coll = getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting();
    IExpr condition = py_cond->getExpr();
    Expr condition_(condition.notype());
    coll->while_(condition_);

    Py_RETURN_NONE;
}

static PyObject* PyEndWhile(PyObject* self, PyObject* args) {
    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->endwhile_();
    Py_RETURN_NONE;
}

static PyObject* PyBreak(PyObject* self, PyObject* args) {
    int depth = 1; // По умолчанию выходим из текущего цикла
    if (!PyArg_ParseTuple(args, "|i", &depth)) {
        return NULL;
    }

    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->break_(depth);
    Py_RETURN_NONE;
}

static PyObject* PyContinue(PyObject* self, PyObject* args) {
    int depth = 1; // По умолчанию прыгаем в начало текущего цикла
    if (!PyArg_ParseTuple(args, "|i", &depth)) {
        return NULL;
    }

    getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->continue_(depth);
    Py_RETURN_NONE;
}

static PyObject* PyStartFunc(PyObject* self, PyObject* args) {
    Py_ssize_t nargs = PyTuple_Size(args);
    if (nargs < 1) {
        PyErr_SetString(PyExc_TypeError, "start_func expects at least 1 argument (name)");
        return NULL;
    }

    // 1. Извлекаем имя функции (первый аргумент)
    PyObject* py_name = PyTuple_GetItem(args, 0);
    if (!PyUnicode_Check(py_name)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a string (function name)");
        return NULL;
    }
    const char* name = PyUnicode_AsUTF8(py_name);

    // 2. Собираем остальные аргументы в вектор указателей на IReg
    std::vector<loops::IReg*> regs;
    for (Py_ssize_t i = 1; i < nargs; ++i) {
        PyObject* item = PyTuple_GetItem(args, i);

        // Проверяем, что это наш IReg
        if (!PyObject_TypeCheck(item, &PyIRegType)) {
            PyErr_Format(PyExc_TypeError, "Argument %zd must be of type IReg", i);
            return NULL;
        }

        PyIReg* py_reg = (PyIReg*)item;
        if (py_reg->reg == nullptr) {
            PyErr_Format(PyExc_RuntimeError, "Register %zd is uninitialized", i);
            return NULL;
        }
        
        regs.push_back(py_reg->reg);
    }

    // 3. Вызываем startFunc из библиотеки loops
    // Передаем вектор напрямую (если API принимает std::initializer_list, 
    // возможно, придется вызвать перегрузку, принимающую вектор или массив)
    getImpl(&ctx)->startFunc(name, regs);

    Py_RETURN_NONE;
}

static PyObject* PyReturn(PyObject* self, PyObject* args) {
    PyObject* obj = nullptr;

    // 1. Парсим аргументы. "|O" - аргумент опционален.
    if (!PyArg_ParseTuple(args, "|O", &obj)) {
        return NULL;
    }

    USE_CONTEXT_(ctx);

    try {
        // Сценарий 1: Вызов без аргументов — return_()
        if (obj == nullptr || obj == Py_None) {
            RETURN_();
        }
        // Сценарий 2: Передан регистр IReg
        else if (PyObject_TypeCheck(obj, &PyIRegType)) {
            PyIReg* py_reg = (PyIReg*)obj;
            if (!py_reg->initialized()) {
                PyErr_SetString(PyExc_RuntimeError, "Return register is uninitialized");
                return NULL;
            }
            RETURN_(py_reg->getExpr());
        }
        // Сценарий 3: Передано обычное число
        else if (PyLong_Check(obj)) {
            int64_t val = (int64_t)PyLong_AsLongLong(obj);
            if (val == -1 && PyErr_Occurred()) return NULL;
            
            // Вопрос: оборачиваем в CONST_ ??
            RETURN_(loops::IExpr(CONST_(val)));
        }
        else {
            PyErr_SetString(PyExc_TypeError, "Argument must be IReg, int or None");
            return NULL;
        }
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *PyEndFunc(PyObject *self, PyObject *args) {
    getImpl(&ctx)->endFunc();
    Py_RETURN_NONE;
}

static PyObject* PySign(PyObject* self, PyObject* arg) {
    // Проверяем, что нам передали именно IReg
    if (!PyObject_TypeCheck(arg, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "sign() expects a pyloops.IReg argument");
        return NULL;
    }

    PyIReg* input = (PyIReg*)arg;
    
    loops::IExpr result_expr = loops::sign(input->getExpr());

    // Оборачиваем результат в новый PyIReg объект
    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    if (!py_res) return NULL;
    
    py_res->reg = nullptr;
    py_res->expr = new loops::IExpr(result_expr);
    return (PyObject*)py_res;
}

static PyObject* PyLoops_and(PyObject* self, PyObject* args) {
    PyObject *obj_a, *obj_b;
    if (!PyArg_ParseTuple(args, "OO", &obj_a, &obj_b)) return NULL;

    if (!PyObject_TypeCheck(obj_a, &PyIRegType) || !PyObject_TypeCheck(obj_b, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "and_() expects two IReg arguments");
        return NULL;
    }

    loops::IExpr res = ((PyIReg*)obj_a)->getExpr() && ((PyIReg*)obj_b)->getExpr();

    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    py_res->reg = nullptr;
    py_res->expr = new loops::IExpr(res);
    return (PyObject*)py_res;
}

static PyObject* PyLoops_or(PyObject* self, PyObject* args) {
    PyObject *obj_a, *obj_b;
    if (!PyArg_ParseTuple(args, "OO", &obj_a, &obj_b)) return NULL;

    if (!PyObject_TypeCheck(obj_a, &PyIRegType) || !PyObject_TypeCheck(obj_b, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "or_() expects two IReg arguments");
        return NULL;
    }

    loops::IExpr res = ((PyIReg*)obj_a)->getExpr() || ((PyIReg*)obj_b)->getExpr();

    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    py_res->reg = nullptr;
    py_res->expr = new loops::IExpr(res);
    return (PyObject*)py_res;
}

static PyObject* PyLoops_not(PyObject* self, PyObject* arg) {
    if (!PyObject_TypeCheck(arg, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "not_() expects an IReg argument");
        return NULL;
    }

    loops::IExpr res = !(((PyIReg*)arg)->getExpr());

    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    py_res->reg = nullptr;
    py_res->expr = new loops::IExpr(res);
    return (PyObject*)py_res;
}

}

// Таблица методов
static PyMethodDef PyloopsMethods[] = {
    {"start_func", PyStartFunc, METH_VARARGS, "Create new loops function."},
    {"return_", (PyCFunction)PyReturn, METH_VARARGS, "Return from function"},
    {"end_func", PyEndFunc, METH_NOARGS, "End function."},
    {"get_func", PyGetFunc, METH_VARARGS, "Returns a Func object by name"},
    {"load_", (PyCFunction)PyLoad, METH_VARARGS, "Load value from memory with given numpy type"},
    {"store_",  (PyCFunction)PyStore,  METH_VARARGS, "Store value to memory with given numpy type"},
    {"if_",       (PyCFunction)PyIf,       METH_O,      "Start an if block"},
    {"endif_",    (PyCFunction)PyEndIf,    METH_NOARGS, "End an if block"},
    {"else_",     (PyCFunction)PyElse,     METH_NOARGS,  "Else block"},
    {"elif_",     (PyCFunction)PyElif,     METH_O,       "Else-if block"},
    {"while_",    (PyCFunction)PyWhile,    METH_O,      "Start a while block"},
    {"endwhile_", (PyCFunction)PyEndWhile, METH_NOARGS, "End a while block"},
    {"break_",    (PyCFunction)PyBreak,    METH_VARARGS,  "Break loop"},
    {"continue_", (PyCFunction)PyContinue, METH_VARARGS,  "Continue loop"},
    {"sign", (PyCFunction)PySign, METH_O, "Returns an IExpr representing the sign of the input register (-1, 0, or 1)."},
    {"and_", (PyCFunction)PyLoops_and, METH_VARARGS, "Logical AND"},
    {"or_",  (PyCFunction)PyLoops_or,  METH_VARARGS, "Logical OR"},
    {"not_", (PyCFunction)PyLoops_not, METH_O,       "Logical NOT"},
    {NULL, NULL, 0, NULL}};

// Описание модуля
static struct PyModuleDef pyloopsmodule = {
    PyModuleDef_HEAD_INIT,
    "pyloops", // имя модуля
    NULL,
    -1,
    PyloopsMethods};

// Инициализация модуля
PyMODINIT_FUNC PyInit_pyloops(void)
{
    PyObject *m;

    // 1. Готовим ВСЕ типы
    if (PyType_Ready(&PyIRegType) < 0)
        return NULL;
    
    if (PyType_Ready(&PyFuncType) < 0)
        return NULL;

    // 2. Создаем модуль
    m = PyModule_Create(&pyloopsmodule);
    if (m == NULL)
        return NULL;

    // 3. Добавляем класс IReg в модуль
    Py_INCREF(&PyIRegType);
    if (PyModule_AddObject(m, "IReg", (PyObject *)&PyIRegType) < 0) {
        Py_DECREF(&PyIRegType);
        Py_XDECREF(m); // Используем XDECREF для безопасности
        return NULL;
    }

    // 4. Добавляем класс Func в модуль
    Py_INCREF(&PyFuncType);
    if (PyModule_AddObject(m, "Func", (PyObject *)&PyFuncType) < 0) {
        // Если не удалось добавить Func, нужно почистить всё
        Py_DECREF(&PyFuncType);
        Py_XDECREF(m); 
        return NULL;
    }

    return m;
}