#include <iostream>
#include "func.hpp"
#include "common.hpp"
extern "C"
{

PyObject* PyFunc_printAssembly(PyFunc* self, PyObject* args)
{
    if (self->func) {
        self->func->printAssembly(std::cout);
    }
    Py_RETURN_NONE;
}

PyObject* PyFunc_printIR(PyFunc* self, PyObject* args)
{
    if (self->func) {
        self->func->printIR(std::cout);
    }
    Py_RETURN_NONE;
}

PyObject* PyFunc_ptr(PyFunc* self, PyObject* args)
{
    if (!self->func) {
        PyErr_SetString(PyExc_RuntimeError, "Func object is not initialized");
        return NULL;
    }
    
    // Получаем указатель на машинный код
    void* p = self->func->ptr();
    
    // Возвращаем его в Python как целое число (адрес)
    // В Python 3 это PyLong_FromVoidPtr
    return PyLong_FromVoidPtr(p);
} 

// Таблица методов для класса Func
static PyMethodDef PyFunc_methods[] = {
    {"print_assembly", (PyCFunction)PyFunc_printAssembly, METH_NOARGS, "Print assembly code"},
    {"print_ir", (PyCFunction)PyFunc_printIR, METH_NOARGS, "Print IR code"},
    {"ptr", (PyCFunction)PyFunc_ptr, METH_NOARGS, "Returns the machine code address."},
    {NULL, NULL, 0, NULL}
};

static void PyFunc_dealloc(PyFunc* self) {
    if (self->func) {
        delete self->func;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyTypeObject PyFuncType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyloops.Func",
    .tp_basicsize = sizeof(PyFunc),
    .tp_dealloc = (destructor)PyFunc_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Loops Function Object",
    .tp_methods = PyFunc_methods, // Подключаем методы выше
};


}