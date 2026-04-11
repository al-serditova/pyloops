#include <Python.h>

// Функция hi()
static PyObject *py_hi(PyObject *self, PyObject *args)
{
    printf("Hi, buddy!\n");
    Py_RETURN_NONE;
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