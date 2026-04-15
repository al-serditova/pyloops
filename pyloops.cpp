#include <Python.h>
#include "common.hpp"
#include "ireg.hpp"
#include "func.hpp"

#include <iostream>
#include "loops/loops.hpp"
#include "/home/vtdrs/work/loops/src/common.hpp"

using namespace loops;
// Context ctx;

extern "C"
{

static PyObject* py_getFunc(PyObject* self, PyObject* args) {
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

static PyObject *PyEndFunc(PyObject *self, PyObject *args)
{
    getImpl(&ctx)->endFunc();
    Py_RETURN_NONE;
}

}

// Таблица методов
static PyMethodDef PyloopsMethods[] = {
    {"start_func", PyStartFunc, METH_VARARGS, "Create new loops function."},
    {"return_", PyReturn, METH_O, "Loops function's return."},
    {"end_func", PyEndFunc, METH_NOARGS, "End function."},
    {"get_func", py_getFunc, METH_VARARGS, "Returns a Func object by name"},
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