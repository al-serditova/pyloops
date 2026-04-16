#include <Python.h>
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

static PyObject* PyLoad_i32(PyObject* self, PyObject* args) {
    PyObject *obj_ptr = nullptr;
    PyObject *obj_offset = nullptr;

    // Парсим: первый обязательный (IReg), второй опциональный (int или IReg)
    if (!PyArg_ParseTuple(args, "O|O", &obj_ptr, &obj_offset)) {
        return NULL;
    }

    // Проверка базового указателя (обязательно IReg)
    if (!PyObject_TypeCheck(obj_ptr, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be an IReg (base pointer)");
        return NULL;
    }
    PyIReg* py_ptr = (PyIReg*)obj_ptr;

    // Подготавливаем выражение для адреса
    // Начинаем с базового адреса
    IExpr address_expr = py_ptr->getExpr();

    // Если передан второй аргумент (смещение)
    if (obj_offset != nullptr && obj_offset != Py_None) {
        if (PyObject_TypeCheck(obj_offset, &PyIRegType)) {
            // Смещение — это другой регистр
            address_expr = address_expr + ((PyIReg*)obj_offset)->getExpr();
        } 
        else if (PyLong_Check(obj_offset)) {
            // Смещение — это константа
            int64_t offset_val = PyLong_AsLongLong(obj_offset);
            address_expr = address_expr + offset_val;
        } 
        else {
            PyErr_SetString(PyExc_TypeError, "Offset must be an IReg or an integer");
            return NULL;
        }
    }

    // Создаем новый объект PyIReg для результата
    PyIReg* py_res = PyObject_New(PyIReg, &PyIRegType);
    if (!py_res) return NULL;

    // Инициализируем: по твоей новой логике используем expr
    py_res->reg = nullptr;
    // Генерируем load от итогового выражения адреса
    py_res->expr = new IExpr(load_<int32_t>(address_expr));

    return (PyObject*)py_res;
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

static PyObject* PyReturn(PyObject* self, PyObject* obj_a) {
    if (!PyObject_TypeCheck(obj_a, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Expected an IReg object");
        return NULL;
    }

    // Приведение типов к нашей структуре
    PyIReg* reg_a = (PyIReg*)obj_a;
    USE_CONTEXT_(ctx);
    RETURN_(*(reg_a->reg));

    Py_RETURN_NONE;
}

static PyObject *PyEndFunc(PyObject *self, PyObject *args) {
    getImpl(&ctx)->endFunc();
    Py_RETURN_NONE;
}

// static PyObject *PyHi(PyObject *self, PyObject *args)
// {
//     using namespace loops;
//     IReg ptr, n, minpos_addr, maxpos_addr;
//     USE_CONTEXT_(ctx);
//     STARTFUNC_("minmaxloc", &ptr, &n, &minpos_addr, &maxpos_addr)
//     {
//         IReg i = CONST_(0);
//         IReg minpos = CONST_(0);
//         IReg maxpos = CONST_(0);
//         IReg minval = load_<int>(ptr);
//         IReg maxval = minval;
//         n *= sizeof(int);
//         WHILE_(i < n)
//         {
//             IReg x = load_<int>(ptr, i);
//             IF_(x < minval)
//             {
//                 minval = x;
//                 minpos = i;
//             }
//             IF_(x > maxval)
//             {
//                 maxval = x;
//                 maxpos = i;
//             }
//             i += sizeof(int);
//         }
//         IReg elemsize = CONST_(sizeof(int));
//         minpos /= elemsize;
//         maxpos /= elemsize;
//         store_<int>(minpos_addr, minpos);
//         store_<int>(maxpos_addr, maxpos);
//         RETURN_(0);
//     }
//     Py_RETURN_NONE;
// }

static PyObject *PyHi(PyObject *self, PyObject *args)
{
    using namespace loops;
    IReg ptr, n, minpos_addr, maxpos_addr;
    USE_CONTEXT_(ctx);
    STARTFUNC_("minmaxloc", &ptr, &n, &minpos_addr, &maxpos_addr)
    {
        IReg i = CONST_(0);
        IReg minpos = CONST_(0);
        IReg maxpos = CONST_(0);
        IReg minval = load_<int>(ptr);
        IReg maxval = minval;
        n *= sizeof(int);

        getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->newiopNoret(OP_STEM_CSTART, {});
        CodeCollecting* coll = getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting();
        IExpr condition = (i < n);
        Expr condition_(condition.notype());        
        coll->while_(condition_);
        {
            IReg x = load_<int>(ptr, i);

            getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->newiopNoret(OP_STEM_CSTART, {});
            CodeCollecting* coll1 = getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting();
            IExpr condition1 = (x < minval);
            Expr condition1_(condition1.notype());
            coll1->if_(condition1_);
            {
                minval = x;
                minpos = i;
            }
            getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->endif_();

            getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->newiopNoret(OP_STEM_CSTART, {});
            CodeCollecting* coll2 = getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting();
            IExpr condition2 = (x > maxval);
            Expr condition2_(condition2.notype());
            coll2->if_(condition2_);
            {
                maxval = x;
                maxpos = i;
            }
            getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->endif_();
            i += sizeof(int);
        }
        getImpl((getImpl(&ctx)->getCurrentFunc()))->get_code_collecting()->endwhile_();        
        IReg elemsize = CONST_(sizeof(int));
        minpos /= elemsize;
        maxpos /= elemsize;
        store_<int>(minpos_addr, minpos);
        store_<int>(maxpos_addr, maxpos);
        RETURN_(0);
    }
    Py_RETURN_NONE;
}

}

// Таблица методов
static PyMethodDef PyloopsMethods[] = {
    {"start_func", PyStartFunc, METH_VARARGS, "Create new loops function."},
    {"return_", PyReturn, METH_O, "Loops function's return."},
    {"end_func", PyEndFunc, METH_NOARGS, "End function."},
    {"get_func", PyGetFunc, METH_VARARGS, "Returns a Func object by name"},
    {"hi", PyHi, METH_NOARGS, "End function."},
    {"load_i32", (PyCFunction)PyLoad_i32, METH_VARARGS, "Load i32 from base [ + offset]"},
    {"if_",       (PyCFunction)PyIf,       METH_O,      "Start an if block"},
    {"endif_",    (PyCFunction)PyEndIf,    METH_NOARGS, "End an if block"},
    {"while_",    (PyCFunction)PyWhile,    METH_O,      "Start a while block"},
    {"endwhile_", (PyCFunction)PyEndWhile, METH_NOARGS, "End a while block"},
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