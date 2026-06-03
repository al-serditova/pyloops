#include <Python.h>
#include <string>
#include "common.hpp"
#include "ireg.hpp"
#include "vreg.hpp"
#include "func.hpp"

#include <iostream>
#include "loops/loops.hpp"
#include "/home/vtdrs/work/loops/src/common.hpp"
#include "/home/vtdrs/work/loops/src/code_collecting.hpp"
#include "/home/vtdrs/work/loops/src/func_impl.hpp"

using namespace loops;
// Context ctx;

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
    int t = type_from_numpy(obj_type);

    try {
        switch (t)
        {
        case (TYPE_I8):   return generic_load<int8_t>(base_expr, obj_offset);
        case (TYPE_U8):   return generic_load<uint8_t>(base_expr, obj_offset);
        case (TYPE_I16):  return generic_load<int16_t>(base_expr, obj_offset);
        case (TYPE_U16):  return generic_load<uint16_t>(base_expr, obj_offset);
        case (TYPE_I32):  return generic_load<int32_t>(base_expr, obj_offset);
        case (TYPE_U32):  return generic_load<uint32_t>(base_expr, obj_offset);
        case (TYPE_I64):  return generic_load<int64_t>(base_expr, obj_offset);
        case (TYPE_U64):  return generic_load<uint64_t>(base_expr, obj_offset);
        case (TYPE_FP16): return generic_load<f16_t>(base_expr, obj_offset);
        case (TYPE_FP32): return generic_load<float>(base_expr, obj_offset);
        case (TYPE_FP64): return generic_load<double>(base_expr, obj_offset);
        default:
            break;
        }
        return NULL;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }
}

static PyObject* PyLoadVec(PyObject* self, PyObject* args) {
    USE_CONTEXT_(ctx);
    PyObject *obj_type = nullptr;
    PyObject *obj_base = nullptr;
    PyObject *obj_offset = nullptr;

    // loadVec(type, ptr, [offset])
    if (!PyArg_ParseTuple(args, "OO|O", &obj_type, &obj_base, &obj_offset)) {
        return NULL;
    }
    // Переводим питоновский numpy-тип в наш внутренний enum
    int t = type_from_numpy(obj_type);
    if(t == -1)
        return NULL;

    // База должна быть строго скалярным IReg
    if (!PyObject_TypeCheck(obj_base, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: loadVec: Pointer must be an IReg");
        return NULL;
    }
    loops::IExpr base_expr = ((PyIReg*)obj_base)->getExpr();


    loops::Expr* result_expr = nullptr;
    try {
        // Сценарий 1: loadVec(type, base) -> Без смещения
        if (obj_offset == nullptr || obj_offset == Py_None) {
            switch (t) {
                case (TYPE_I8):   result_expr = new loops::Expr(loops::loadvec<int8_t>(base_expr).notype()); break;
                case (TYPE_U8):   result_expr = new loops::Expr(loops::loadvec<uint8_t>(base_expr).notype()); break;
                case (TYPE_I16):  result_expr = new loops::Expr(loops::loadvec<int16_t>(base_expr).notype()); break;
                case (TYPE_U16):  result_expr = new loops::Expr(loops::loadvec<uint16_t>(base_expr).notype()); break;
                case (TYPE_I32):  result_expr = new loops::Expr(loops::loadvec<int32_t>(base_expr).notype()); break;
                case (TYPE_U32):  result_expr = new loops::Expr(loops::loadvec<uint32_t>(base_expr).notype()); break;
                case (TYPE_I64):  result_expr = new loops::Expr(loops::loadvec<int64_t>(base_expr).notype()); break;
                case (TYPE_U64):  result_expr = new loops::Expr(loops::loadvec<uint64_t>(base_expr).notype()); break;
                case (TYPE_FP16): result_expr = new loops::Expr(loops::loadvec<loops::f16_t>(base_expr).notype()); break;
                case (TYPE_FP32): result_expr = new loops::Expr(loops::loadvec<float>(base_expr).notype()); break;
                case (TYPE_FP64): result_expr = new loops::Expr(loops::loadvec<double>(base_expr).notype()); break;
                default: break;
            }
        }
        // Сценарий 2: loadVec(type, base, offset) -> Смещение в IReg
        else if (PyObject_TypeCheck(obj_offset, &PyIRegType)) {
            loops::IExpr offset_expr = ((PyIReg*)obj_offset)->getExpr();
            switch (t) {
                case (TYPE_I8):   result_expr = new loops::Expr(loops::loadvec<int8_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_U8):   result_expr = new loops::Expr(loops::loadvec<uint8_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_I16):  result_expr = new loops::Expr(loops::loadvec<int16_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_U16):  result_expr = new loops::Expr(loops::loadvec<uint16_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_I32):  result_expr = new loops::Expr(loops::loadvec<int32_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_U32):  result_expr = new loops::Expr(loops::loadvec<uint32_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_I64):  result_expr = new loops::Expr(loops::loadvec<int64_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_U64):  result_expr = new loops::Expr(loops::loadvec<uint64_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_FP16): result_expr = new loops::Expr(loops::loadvec<loops::f16_t>(base_expr, offset_expr).notype()); break;
                case (TYPE_FP32): result_expr = new loops::Expr(loops::loadvec<float>(base_expr, offset_expr).notype()); break;
                case (TYPE_FP64): result_expr = new loops::Expr(loops::loadvec<double>(base_expr, offset_expr).notype()); break;
                default: break;
            }
        }
        // Сценарий 3: loadVec(type, base, offset) -> Смещение в виде обычного числа (int)
        else if (PyLong_Check(obj_offset)) {
            int64_t offset_val = PyLong_AsLongLong(obj_offset);
            switch (t) {
                case (TYPE_I8):   result_expr = new loops::Expr(loops::loadvec<int8_t>(base_expr, offset_val).notype()); break;
                case (TYPE_U8):   result_expr = new loops::Expr(loops::loadvec<uint8_t>(base_expr, offset_val).notype()); break;
                case (TYPE_I16):  result_expr = new loops::Expr(loops::loadvec<int16_t>(base_expr, offset_val).notype()); break;
                case (TYPE_U16):  result_expr = new loops::Expr(loops::loadvec<uint16_t>(base_expr, offset_val).notype()); break;
                case (TYPE_I32):  result_expr = new loops::Expr(loops::loadvec<int32_t>(base_expr, offset_val).notype()); break;
                case (TYPE_U32):  result_expr = new loops::Expr(loops::loadvec<uint32_t>(base_expr, offset_val).notype()); break;
                case (TYPE_I64):  result_expr = new loops::Expr(loops::loadvec<int64_t>(base_expr, offset_val).notype()); break;
                case (TYPE_U64):  result_expr = new loops::Expr(loops::loadvec<uint64_t>(base_expr, offset_val).notype()); break;
                case (TYPE_FP16): result_expr = new loops::Expr(loops::loadvec<loops::f16_t>(base_expr, offset_val).notype()); break;
                case (TYPE_FP32): result_expr = new loops::Expr(loops::loadvec<float>(base_expr, offset_val).notype()); break;
                case (TYPE_FP64): result_expr = new loops::Expr(loops::loadvec<double>(base_expr, offset_val).notype()); break;
                default: break;
            }
        }
        else {
            PyErr_SetString(PyExc_TypeError, "PyLoops: loadVec: Offset must be an IReg or an integer");
            return NULL;
        }
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }

    // Создаем результирующий векторный регистр для Python
    PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->expr = result_expr;
    py_res->type = t; // Присваиваем тип элемента вектору

    return (PyObject*)py_res;

type_error:
    PyErr_SetString(PyExc_TypeError, "PyLoops: loadVec: Unsupported data type");
    return NULL;
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
    int t = type_from_numpy(obj_type);

    try {
        switch (t)
        {
        case (TYPE_I8):   return generic_store<int8_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_U8):   return generic_store<uint8_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_I16):  return generic_store<int16_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_U16):  return generic_store<uint16_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_I32):  return generic_store<int32_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_U32):  return generic_store<uint32_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_I64):  return generic_store<int64_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_U64):  return generic_store<uint64_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_FP16): return generic_store<f16_t>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_FP32): return generic_store<float>(base_expr, obj_off_or_val, obj_val);
        case (TYPE_FP64): return generic_store<double>(base_expr, obj_off_or_val, obj_val);
        default:
            break;
        }
        return NULL;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }
}

static PyObject* PyStoreVec(PyObject* self, PyObject* args) {
    PyObject *obj_type = nullptr;
    PyObject *obj_base = nullptr;
    PyObject *obj_off_or_val = nullptr;
    PyObject *obj_val = nullptr;

    // "OOOO" — 3 обязательных, 1 опциональный (|)
    if (!PyArg_ParseTuple(args, "OO|O", &obj_base, &obj_off_or_val, &obj_val)) {
        return NULL;
    }

    // Проверяем базу (адрес)
    if (!PyObject_TypeCheck(obj_base, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Base must be an IReg pointer");
        return NULL;
    }
    loops::IExpr base_expr = ((PyIReg*)obj_base)->getExpr();
    try {
        if (obj_val == nullptr) {
            obj_val = obj_off_or_val;
            if (PyObject_TypeCheck(obj_val, &PyVRegType)) {
                newiopNoret(VOP_STORE, {base_expr.notype(), ((PyVReg*)obj_val)->getExpr()});
            } else {
                PyErr_SetString(PyExc_TypeError, "PyLoops: storevec: Value must be VReg");
                return NULL;
            }
        }
        else
        {
            if (PyObject_TypeCheck(obj_val, &PyVRegType)) {
                Expr val = ((PyVReg*)obj_val)->getExpr();
                if (PyObject_TypeCheck(obj_off_or_val, &PyIRegType)) {
                    loops::IExpr offset = ((PyIReg*)obj_off_or_val)->getExpr();
                    newiopNoret(VOP_STORE, {base_expr.notype(), offset.notype(), val});
                } else if (PyLong_Check(obj_off_or_val)) {
                    int64_t offset = PyLong_AsLongLong(obj_off_or_val);
                    newiopNoret(VOP_STORE, {base_expr.notype(), Expr(offset), val});
                } else {
                    PyErr_SetString(PyExc_TypeError, "PyLoops: storevec: Offset must be IReg or int");
                    return NULL;
                }
            } else {
                PyErr_SetString(PyExc_TypeError, "PyLoops: storevec: Value must be VReg");
                return NULL;
            }
        }
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }
    return Py_None;
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
            RETURN_(val);
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

PyObject* Py_ushift_right(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    // v - что сдвигаем, w - на сколько битов
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) {
        return NULL;
    }
    if (PyObject_TypeCheck(v, &PyVRegType)) {
        return PyVReg_binary(v, w, VOP_SHR);
    }
   
    return PyIReg_binary(v, w, OP_SHR);
}

PyObject* PyIReg_ule(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) return NULL;
    return PyIReg_binary(v, w, OP_ULE);
}

PyObject* PyIReg_uge(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) return NULL;
    return PyIReg_binary(w, v, OP_ULE); // Меняем местами!
}

PyObject* PyIReg_ugt(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) return NULL;
    return PyIReg_binary(v, w, OP_UGT);
}

PyObject* PyIReg_ult(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) return NULL;
    return PyIReg_binary(w, v, OP_UGT); // Меняем местами!
}

PyObject* Py_min(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) {
        return NULL;
    }

    // Если оба операнда — векторные регистры, вызываем векторный MIN
    if (PyObject_TypeCheck(v, &PyVRegType) && PyObject_TypeCheck(w, &PyVRegType)) {
        return PyVReg_binary(v, w, VOP_MIN);
    }
    
    // Во всех остальных случаях отдаем скалярному бинарному обработчику
    return PyIReg_binary(v, w, OP_MIN);
}

PyObject* Py_max(PyObject* self, PyObject* args) {
    PyObject *v, *w;
    if (!PyArg_ParseTuple(args, "OO", &v, &w)) {
        return NULL;
    }

    // Если оба операнда — векторные регистры, вызываем векторный MAX
    if (PyObject_TypeCheck(v, &PyVRegType) && PyObject_TypeCheck(w, &PyVRegType)) {
        return PyVReg_binary(v, w, VOP_MAX); 
    }

    return PyIReg_binary(v, w, OP_MAX);
}

static PyObject* PyIReg_select(PyObject* self, PyObject* args) {
    PyObject *py_cond, *py_true, *py_false;
    
    // Парсим три аргумента: условие, значение-если-истина, значение-если-ложь
    if (!PyArg_ParseTuple(args, "OOO", &py_cond, &py_true, &py_false)) {
        return NULL;
    }

    // 1. Условие обязательно должно быть IReg (IExpr)
    if (!PyObject_TypeCheck(py_cond, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Condition must be an IReg");
        return NULL;
    }
    loops::IExpr cond = ((PyIReg*)py_cond)->getExpr();
    
    USE_CONTEXT_(ctx);
    loops::IExpr result_expr;

    // Проверяем типы true_ и false_ для выбора правильной перегрузки loops::select
    bool true_is_reg = PyObject_TypeCheck(py_true, &PyIRegType);
    bool false_is_reg = PyObject_TypeCheck(py_false, &PyIRegType);

    // Обрабатываем 4 комбинации, которые мы видели в loops.hpp
    if (true_is_reg && false_is_reg) {
        result_expr = loops::select(cond, ((PyIReg*)py_true)->getExpr(), ((PyIReg*)py_false)->getExpr());
    }
    else if (!true_is_reg && false_is_reg) {
        int64_t val_t = PyLong_AsLongLong(py_true);
        result_expr = loops::select(cond, val_t, ((PyIReg*)py_false)->getExpr());
    }
    else if (true_is_reg && !false_is_reg) {
        int64_t val_f = PyLong_AsLongLong(py_false);
        result_expr = loops::select(cond, ((PyIReg*)py_true)->getExpr(), val_f);
    }
    else { // Оба - константы
        int64_t val_t = PyLong_AsLongLong(py_true);
        int64_t val_f = PyLong_AsLongLong(py_false);
        result_expr = loops::select(cond, val_t, val_f);
    }

    // Оборачиваем результат в новый PyIReg
    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->expr = new loops::IExpr(result_expr);
    return (PyObject*)py_res;
}

static PyObject* PyVReg_select(PyObject* self, PyObject* args) {
    PyObject *py_cond, *py_true, *py_false;
    
    if (!PyArg_ParseTuple(args, "OOO", &py_cond, &py_true, &py_false)) {
        return NULL;
    }

    if (!PyObject_TypeCheck(py_cond, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: select: Condition must be a VReg mask");
        return NULL;
    }

    Expr cond_expr = ((PyVReg*)py_cond)->getExpr();
    Expr t_expr;
    Expr f_expr;
    int res_type = -1;

    // 1. Разбираемся с типами true/false аргументов и приводим их к Expr
    if (PyObject_TypeCheck(py_true, &PyVRegType)) {
        res_type = ((PyVReg*)py_true)->type;
        t_expr = ((PyVReg*)py_true)->getExpr();
    } else {
        PyErr_SetString(PyExc_TypeError, "PyLoops: select: Only VRegs are supported");
        return NULL;
    }

    if (PyObject_TypeCheck(py_false, &PyVRegType)) {
        if (res_type != ((PyVReg*)py_false)->type) {
            PyErr_SetString(PyExc_TypeError, "PyLoops: select: True and False VReg types must match");
            return NULL;
        }
        f_expr = ((PyVReg*)py_false)->getExpr();
    } else {
        PyErr_SetString(PyExc_TypeError, "PyLoops: select: Only VRegs are supported");
        return NULL;
    }

    int cond_type = res_type == TYPE_U8 ? TYPE_U8 :
                    res_type == TYPE_I8 ? TYPE_U8 :
                    res_type == TYPE_U16 ? TYPE_U16 :
                    res_type == TYPE_I16 ? TYPE_U16 :
                    res_type == TYPE_U32 ? TYPE_U32 :
                    res_type == TYPE_I32 ? TYPE_U32 :
                    res_type == TYPE_U64 ? TYPE_U64 :
                    res_type == TYPE_I64 ? TYPE_U64 :
                    res_type == TYPE_FP16 ? TYPE_U16 :
                    res_type == TYPE_FP32 ? TYPE_U32 :
                    res_type == TYPE_FP64 ? TYPE_U64 : -1;
    if (cond_type != ((PyVReg*)py_cond)->type) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: select: Condition type have to be unsigned int of true/false values type");
        return NULL;
    }

    USE_CONTEXT_(ctx);
    Expr* result_expr = nullptr;

    // 2. Напрямую вызываем конструктор VExpr<T> с опкодом VOP_SELECT и списком аргументов.
    // В конце вызываем .notype(), чтобы сохранить результат как базовый Expr.
    switch (res_type) {
        case (TYPE_I8):   result_expr = new Expr(VExpr<int8_t>  (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_U8):   result_expr = new Expr(VExpr<uint8_t> (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_I16):  result_expr = new Expr(VExpr<int16_t> (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_U16):  result_expr = new Expr(VExpr<uint16_t>(VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_I32):  result_expr = new Expr(VExpr<int32_t> (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_U32):  result_expr = new Expr(VExpr<uint32_t>(VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_I64):  result_expr = new Expr(VExpr<int64_t> (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_U64):  result_expr = new Expr(VExpr<uint64_t>(VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_FP16): result_expr = new Expr(VExpr<f16_t>   (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_FP32): result_expr = new Expr(VExpr<float>   (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        case (TYPE_FP64): result_expr = new Expr(VExpr<double>  (VOP_SELECT, {cond_expr, t_expr, f_expr}).notype()); break;
        default: Py_RETURN_NOTIMPLEMENTED;
    }

    // 3. Создаем результирующий Python-объект
    PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->type = res_type;
    py_res->expr = result_expr;

    return (PyObject*)py_res;
}

static PyObject* PyVReg_fma(PyObject* self, PyObject* args) {
    PyObject *py_a, *py_b, *py_c;

    if (!PyArg_ParseTuple(args, "OOO", &py_a, &py_b, &py_c)) {
        return NULL;
    }

    if (!PyObject_TypeCheck(py_a, &PyVRegType) || 
        !PyObject_TypeCheck(py_b, &PyVRegType) || 
        !PyObject_TypeCheck(py_c, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: fma: All three arguments must be VReg objects");
        return NULL;
    }

    PyVReg* a = (PyVReg*)py_a;
    PyVReg* b = (PyVReg*)py_b;
    PyVReg* c = (PyVReg*)py_c;

    if (a->type != b->type || a->type != c->type) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: fma: Types of all VReg arguments must match");
        return NULL;
    }

    int res_type = a->type;

    // Проверяем, что тип относится к плавающей точке
    if (res_type != TYPE_FP16 && res_type != TYPE_FP32 && res_type != TYPE_FP64) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: fma: Only floating-point types (fp16, fp32, fp64) are supported");
        return NULL;
    }

    USE_CONTEXT_(ctx);

    Expr expr_a = a->getExpr();
    Expr expr_b = b->getExpr();
    Expr expr_c = c->getExpr();
    Expr* result_expr = nullptr;

    switch (res_type) {
        case (TYPE_FP16): result_expr = new Expr(fma(restoreExprType<f16_t>(expr_a),  restoreExprType<f16_t>(expr_b),  restoreExprType<f16_t>(expr_c)).notype()); break;
        case (TYPE_FP32): result_expr = new Expr(fma(restoreExprType<float>(expr_a),  restoreExprType<float>(expr_b),  restoreExprType<float>(expr_c)).notype()); break;
        case (TYPE_FP64): result_expr = new Expr(fma(restoreExprType<double>(expr_a), restoreExprType<double>(expr_b), restoreExprType<double>(expr_c)).notype()); break;
        default:
            Py_RETURN_NOTIMPLEMENTED;
    }

    PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->type = res_type;
    py_res->expr = result_expr;

    return (PyObject*)py_res;
}

static PyObject* PyVReg_ext(PyObject* self, PyObject* args) {
    PyObject *py_n, *py_m, *py_index;

    // Парсим три аргумента: ext(n, m, index)
    if (!PyArg_ParseTuple(args, "OOO", &py_n, &py_m, &py_index)) {
        return NULL;
    }

    // 1. Проверяем, что первые два аргумента — это наши VReg
    if (!PyObject_TypeCheck(py_n, &PyVRegType) || !PyObject_TypeCheck(py_m, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: ext: First two arguments must be VReg objects");
        return NULL;
    }

    // 2. Проверяем, что третий аргумент — это константное целое число
    if (!PyLong_Check(py_index)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: ext: Index must be an integer constant");
        return NULL;
    }

    PyVReg* n = (PyVReg*)py_n;
    PyVReg* m = (PyVReg*)py_m;
    int64_t index = (int64_t)PyLong_AsLongLong(py_index);

    // 3. Проверяем совпадение типов векторов
    if (n->type != m->type) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: ext: Vector types must match");
        return NULL;
    }

    int res_type = n->type;
    USE_CONTEXT_(ctx);

    Expr expr_n = n->getExpr();
    Expr expr_m = m->getExpr();
    Expr* result_expr = nullptr;

    // 4. Восстанавливаем типы через restoreExprType<T>
    switch (res_type) {
        case (TYPE_I8):   result_expr = new Expr(ext(restoreExprType<int8_t>(expr_n),   restoreExprType<int8_t>(expr_m),   index).notype()); break;
        case (TYPE_U8):   result_expr = new Expr(ext(restoreExprType<uint8_t>(expr_n),  restoreExprType<uint8_t>(expr_m),  index).notype()); break;
        case (TYPE_I16):  result_expr = new Expr(ext(restoreExprType<int16_t>(expr_n),  restoreExprType<int16_t>(expr_m),  index).notype()); break;
        case (TYPE_U16):  result_expr = new Expr(ext(restoreExprType<uint16_t>(expr_n), restoreExprType<uint16_t>(expr_m), index).notype()); break;
        case (TYPE_I32):  result_expr = new Expr(ext(restoreExprType<int32_t>(expr_n),  restoreExprType<int32_t>(expr_m),  index).notype()); break;
        case (TYPE_U32):  result_expr = new Expr(ext(restoreExprType<uint32_t>(expr_n), restoreExprType<uint32_t>(expr_m), index).notype()); break;
        case (TYPE_I64):  result_expr = new Expr(ext(restoreExprType<int64_t>(expr_n),  restoreExprType<int64_t>(expr_m),  index).notype()); break;
        case (TYPE_U64):  result_expr = new Expr(ext(restoreExprType<uint64_t>(expr_n), restoreExprType<uint64_t>(expr_m), index).notype()); break;
        case (TYPE_FP16): result_expr = new Expr(ext(restoreExprType<f16_t>(expr_n),    restoreExprType<f16_t>(expr_m), index).notype()); break;
        case (TYPE_FP32): result_expr = new Expr(ext(restoreExprType<float>(expr_n),    restoreExprType<float>(expr_m),    index).notype()); break;
        case (TYPE_FP64): result_expr = new Expr(ext(restoreExprType<double>(expr_n),   restoreExprType<double>(expr_m),   index).notype()); break;
        default:
            Py_RETURN_NOTIMPLEMENTED;
    }

    // 5. Оборачиваем результат в Python-объект PyVReg
    PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->type = res_type;
    py_res->expr = result_expr;

    return (PyObject*)py_res;
}

static PyObject* PyVReg_reduce_sum(PyObject* self, PyObject* args) {
    PyObject* py_r;

    // Парсим один векторный аргумент: reduce_sum(r)
    if (!PyArg_ParseTuple(args, "O", &py_r)) {
        return NULL;
    }

    // 1. Проверяем, что аргумент — это наш VReg
    if (!PyObject_TypeCheck(py_r, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: reduce_sum: Argument must be a VReg object");
        return NULL;
    }

    PyVReg* r = (PyVReg*)py_r;
    int res_type = r->type;
    USE_CONTEXT_(ctx);

    Expr expr_r = r->getExpr();
    Expr* result_expr = nullptr;

    // 2. Восстанавливаем типы через restoreExprType<T>
    switch (res_type) {
        case (TYPE_I8):   result_expr = new Expr(reduce_sum(restoreExprType<int8_t>(expr_r)).notype()); break;
        case (TYPE_U8):   result_expr = new Expr(reduce_sum(restoreExprType<uint8_t>(expr_r)).notype()); break;
        case (TYPE_I16):  result_expr = new Expr(reduce_sum(restoreExprType<int16_t>(expr_r)).notype()); break;
        case (TYPE_U16):  result_expr = new Expr(reduce_sum(restoreExprType<uint16_t>(expr_r)).notype()); break;
        case (TYPE_I32):  result_expr = new Expr(reduce_sum(restoreExprType<int32_t>(expr_r)).notype()); break;
        case (TYPE_U32):  result_expr = new Expr(reduce_sum(restoreExprType<uint32_t>(expr_r)).notype()); break;
        case (TYPE_I64):  result_expr = new Expr(reduce_sum(restoreExprType<int64_t>(expr_r)).notype()); break;
        case (TYPE_U64):  result_expr = new Expr(reduce_sum(restoreExprType<uint64_t>(expr_r)).notype()); break;
        case (TYPE_FP16): result_expr = new Expr(reduce_sum(restoreExprType<f16_t>(expr_r)).notype()); break;
        case (TYPE_FP32): result_expr = new Expr(reduce_sum(restoreExprType<float>(expr_r)).notype()); break;
        case (TYPE_FP64): result_expr = new Expr(reduce_sum(restoreExprType<double>(expr_r)).notype()); break;
        default:
            Py_RETURN_NOTIMPLEMENTED;
    }

    // 3. Оборачиваем результат в Python-объект PyVReg (так как возвращается VExpr)
    PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->type = res_type;
    py_res->expr = result_expr;

    return (PyObject*)py_res;
}

static PyObject* PyVReg_getlane(PyObject* self, PyObject* args) {
    PyObject* py_r;
    int lanenum;

    // Парсим аргументы: getlane(vreg, index)
    if (!PyArg_ParseTuple(args, "Oi", &py_r, &lanenum)) {
        return NULL;
    }

    // 1. Проверяем, что первый аргумент — это вектор
    if (!PyObject_TypeCheck(py_r, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: getlane: First argument must be a VReg object");
        return NULL;
    }

    PyVReg* r = (PyVReg*)py_r;
    int res_type = r->type;

    USE_CONTEXT_(ctx);
    Expr expr_r = r->getExpr();
    
    // Сюда сохраним полученный скалярный IExpr
    IExpr result_iexpr;

    // 3. Восстанавливаем тип вектора
    switch (res_type) {
        case (TYPE_I8):   result_iexpr = getlane(restoreExprType<int8_t>(expr_r),   lanenum); break;
        case (TYPE_U8):   result_iexpr = getlane(restoreExprType<uint8_t>(expr_r),  lanenum); break;
        case (TYPE_I16):  result_iexpr = getlane(restoreExprType<int16_t>(expr_r),  lanenum); break;
        case (TYPE_U16):  result_iexpr = getlane(restoreExprType<uint16_t>(expr_r), lanenum); break;
        case (TYPE_I32):  result_iexpr = getlane(restoreExprType<int32_t>(expr_r),  lanenum); break;
        case (TYPE_U32):  result_iexpr = getlane(restoreExprType<uint32_t>(expr_r), lanenum); break;
        case (TYPE_I64):  result_iexpr = getlane(restoreExprType<int64_t>(expr_r),  lanenum); break;
        case (TYPE_U64):  result_iexpr = getlane(restoreExprType<uint64_t>(expr_r), lanenum); break;
        case (TYPE_FP16): result_iexpr = getlane(restoreExprType<f16_t>(expr_r),    lanenum); break;
        case (TYPE_FP32): result_iexpr = getlane(restoreExprType<float>(expr_r),    lanenum); break;
        case (TYPE_FP64): result_iexpr = getlane(restoreExprType<double>(expr_r),   lanenum); break;
        default:
            Py_RETURN_NOTIMPLEMENTED;
    }

    // 4. Оборачиваем результат в СКАЛЯРНЫЙ Python-объект PyIReg
    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->expr = new loops::IExpr(result_iexpr);

    return (PyObject*)py_res;
}

static PyObject* PyVReg_setlane(PyObject* self, PyObject* args) {
    PyObject *py_v, *py_laneval;
    int lanenum;

    // Парсим три аргумента: setlane(vreg, index, ireg)
    if (!PyArg_ParseTuple(args, "OiO", &py_v, &lanenum, &py_laneval)) {
        return NULL;
    }

    // 1. Проверяем вектор
    if (!PyObject_TypeCheck(py_v, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "PyLoops: setlane: First argument must be a VReg object");
        return NULL;
    }

    // 3. Проверяем и извлекаем скалярное значение (поддерживаем PyIReg и обычные питоновские инты)
    IExpr value_iexpr;
    if (PyObject_TypeCheck(py_laneval, &PyIRegType)) {
        value_iexpr = ((PyIReg*)py_laneval)->getExpr();
    } else {
        PyErr_SetString(PyExc_TypeError, "PyLoops: setlane: Third argument must be an IReg");
        return NULL;
    }

    PyVReg* v = (PyVReg*)py_v;
    int res_type = v->type;

    USE_CONTEXT_(ctx);
    Expr expr_v = v->getExpr();

    // 4. Восстанавливаем тип вектора (возвращает void)
    switch (res_type) {
        case (TYPE_I8):   setlane(restoreExprType<int8_t>(expr_v),   lanenum, value_iexpr); break;
        case (TYPE_U8):   setlane(restoreExprType<uint8_t>(expr_v),  lanenum, value_iexpr); break;
        case (TYPE_I16):  setlane(restoreExprType<int16_t>(expr_v),  lanenum, value_iexpr); break;
        case (TYPE_U16):  setlane(restoreExprType<uint16_t>(expr_v), lanenum, value_iexpr); break;
        case (TYPE_I32):  setlane(restoreExprType<int32_t>(expr_v),  lanenum, value_iexpr); break;
        case (TYPE_U32):  setlane(restoreExprType<uint32_t>(expr_v), lanenum, value_iexpr); break;
        case (TYPE_I64):  setlane(restoreExprType<int64_t>(expr_v),  lanenum, value_iexpr); break;
        case (TYPE_U64):  setlane(restoreExprType<uint64_t>(expr_v), lanenum, value_iexpr); break;
        case (TYPE_FP16): setlane(restoreExprType<f16_t>(expr_v),    lanenum, value_iexpr); break;
        case (TYPE_FP32): setlane(restoreExprType<float>(expr_v),    lanenum, value_iexpr); break;
        case (TYPE_FP64): setlane(restoreExprType<double>(expr_v),   lanenum, value_iexpr); break;
        default:
            Py_RETURN_NOTIMPLEMENTED;
    }

    // Возвращаем None, так как функция void
    Py_RETURN_NONE;
}

static PyObject *PyVbytes(PyObject *self, PyObject *args) {
    return PyLong_FromLongLong(ctx.vbytes());
}

}

// Таблица методов
static PyMethodDef PyloopsMethods[] = {
    {"start_func", PyStartFunc, METH_VARARGS, "Create new loops function."},
    {"return_", (PyCFunction)PyReturn, METH_VARARGS, "Return from function"},
    {"end_func", PyEndFunc, METH_NOARGS, "End function."},
    {"get_func", PyGetFunc, METH_VARARGS, "Returns a Func object by name"},
    {"load_", (PyCFunction)PyLoad, METH_VARARGS, "Load value from memory with given numpy type"},
    {"loadVec",  (PyCFunction)PyLoadVec,  METH_VARARGS, "Load vector from memory (base, [offset])"},
    {"store_",  (PyCFunction)PyStore,  METH_VARARGS, "Store value to memory with given numpy type"},
    {"storevec",  (PyCFunction)PyStoreVec,  METH_VARARGS, "Store vector register to memory"},
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
    {"ushift_right", (PyCFunction)Py_ushift_right, METH_VARARGS, "Logical shift right (unsigned)"},
    {"ule_",         (PyCFunction)PyIReg_ule,    METH_VARARGS, "Unsigned less or equal"},
    {"uge_",         (PyCFunction)PyIReg_uge,    METH_VARARGS, "Unsigned greater or equal"},
    {"ult_",         (PyCFunction)PyIReg_ult,    METH_VARARGS, "Unsigned less than"},
    {"ugt_",         (PyCFunction)PyIReg_ugt,    METH_VARARGS, "Unsigned greater than"},
    {"min_",         (PyCFunction)Py_min,    METH_VARARGS, "Minimum of two values"},
    {"max_",         (PyCFunction)Py_max,    METH_VARARGS, "Maximum of two values"},
    {"select_", (PyCFunction)PyIReg_select, METH_VARARGS, "Conditional select: cond ? true : false"},
    {"vselect", (PyCFunction)PyVReg_select, METH_VARARGS, "Vector ternary select: vselect(mask, true_val, false_val)"},
    {"vbytes",  PyVbytes, METH_NOARGS, "Size of vector register in bytes."},
    {"fma", (PyCFunction)PyVReg_fma, METH_VARARGS, "Vector Fused Multiply-Add: fma(a, b, c) -> a * b + c"},
    {"ext", (PyCFunction)PyVReg_ext, METH_VARARGS, "Extract vector from two concatenated vectors: ext(n, m, index)"}, 
    {"reduce_sum", (PyCFunction)PyVReg_reduce_sum, METH_VARARGS, "Horizontal sum of vector elements: reduce_sum(r)"},
    {"getlane", (PyCFunction)PyVReg_getlane, METH_VARARGS, "Get scalar element from vector: getlane(vreg, index) -> IReg"},
{"setlane", (PyCFunction)PyVReg_setlane, METH_VARARGS, "Set scalar element in vector: setlane(vreg, index, value)"},
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
    
    if (PyType_Ready(&PyVRegType) < 0)
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

    // 4. Добавляем класс IReg в модуль
    Py_INCREF(&PyVRegType);
    if (PyModule_AddObject(m, "VReg", (PyObject *)&PyVRegType) < 0) {
        Py_DECREF(&PyVRegType);
        Py_XDECREF(m); // Используем XDECREF для безопасности
        return NULL;
    }

    // 5. Добавляем класс Func в модуль
    Py_INCREF(&PyFuncType);
    if (PyModule_AddObject(m, "Func", (PyObject *)&PyFuncType) < 0) {
        // Если не удалось добавить Func, нужно почистить всё
        Py_DECREF(&PyFuncType);
        Py_XDECREF(m); 
        return NULL;
    }

    return m;
}