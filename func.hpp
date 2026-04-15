#pragma once
#include <Python.h>
#include "loops/loops.hpp"

using namespace loops;

extern "C"
{
extern PyTypeObject PyFuncType;

typedef struct {
    PyObject_HEAD
    loops::Func* func; // Указатель на объект Func из библиотеки
} PyFunc;

PyObject* PyFunc_printAssembly(PyFunc* self, PyObject* args);
PyObject* PyFunc_printIR(PyFunc* self, PyObject* args);
PyObject* PyFunc_ptr(PyFunc* self, PyObject* args);

}
