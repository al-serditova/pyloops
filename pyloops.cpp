#include <Python.h>
#include <iostream>
#include "loops/loops.hpp"
#include "/home/vtdrs/work/loops/src/common.hpp"

using namespace loops;
Context ctx;

// Функция hi()
extern "C"
{

static PyObject *py_hi(PyObject *self, PyObject *args)
{
    USE_CONTEXT_(ctx); //DUBUG: use it in every wrapper.
    IReg a, b;
    getImpl(&ctx)->startFunc("a_plus_b", {&a, &b});
    a += b;
    RETURN_(a);
    getImpl(&ctx)->endFunc();
    loops::Func func = ctx.getFunc("a_plus_b");
    func.printIR(std::cout);
    func.printAssembly(std::cout);
    typedef int64_t (*a_plus_b_f)(int64_t a, int64_t b);
    a_plus_b_f tested = reinterpret_cast<a_plus_b_f>(func.ptr());
    printf("PyLoops: %d + %d = %d\n", 5, 1, (int)tested(5, 1));
    Py_RETURN_NONE;
}

}

// Таблица методов
static PyMethodDef PyloopsMethods[] = {
    {"hi", py_hi, METH_NOARGS, "Print something."}, // Description of function.
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
    return PyModule_Create(&pyloopsmodule);
}