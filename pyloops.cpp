#include <Python.h>
#include <iostream>
#include "loops/loops.hpp"
#include "/home/vtdrs/work/loops/src/common.hpp"

using namespace loops;
Context ctx;

// Функция hi()
extern "C"
{
typedef struct {
    PyObject_HEAD
    IReg* reg; // Указатель на реальный IReg из loops
} PyIReg;

static PyObject *PyIReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyIReg *self;
    self = (PyIReg *)type->tp_alloc(type, 0);
    if (self != NULL) {
        // Инициализируем указатель на C++ объект нулевым значением
        self->reg = nullptr;
    }
    return (PyObject *)self;
}

// 2. tp_init: Инициализация (аналог __init__)
static int PyIReg_init(PyIReg *self, PyObject *args, PyObject *kwds) {
    // Здесь мы создаем реальный объект IReg из твоей библиотеки loops
    // Например, просто создаем новый экземпляр регистра
    self->reg = new loops::IReg(); 
    printf("Privet\n");
    if (self->reg == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "Could not allocate IReg");
        return -1;
    }
    return 0;
}

// 3. Не забываем про деструктор (tp_dealloc), чтобы не было утечек памяти!
static void PyIReg_dealloc(PyIReg *self) {
    if (self->reg) {
        delete self->reg; // Удаляем C++ объект
    }
    printf("Poka\n");
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* PyIReg_iadd(PyObject* self, PyObject* other);

static PyNumberMethods PyIReg_as_number = {
    0,                          // nb_add
    0,                          // nb_subtract
    0,                          // nb_multiply
    0,                          // nb_remainder
    0,                          // nb_divmod
    0,                          // nb_power
    0,                          // nb_negative
    0,                          // nb_positive
    0,                          // nb_absolute
    0,                          // nb_bool
    0,                          // nb_invert
    0,                          // nb_lshift
    0,                          // nb_rshift
    0,                          // nb_and
    0,                          // nb_xor
    0,                          // nb_or
    0,                          // nb_int
    0,                          // reserved
    0,                          // nb_float
    (binaryfunc)PyIReg_iadd,    // nb_inplace_add  <-- ВОТ ОН!
    // ... остальные поля можно оставить нулевыми
};

static PyTypeObject PyIRegType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyloops.IReg",
    .tp_basicsize = sizeof(PyIReg),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyIReg_dealloc,
    .tp_as_number = &PyIReg_as_number,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Loops Register",
    .tp_init = (initproc)PyIReg_init,
    .tp_new = PyIReg_new,
};

static PyObject* PyIReg_iadd(PyObject* self, PyObject* other) {
    // 1. Проверяем, что 'other' — это тоже наш регистр
    // (Если хочешь складывать с числами, тут нужно добавить проверку на PyLong)
    if (!PyObject_TypeCheck(other, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Operand must be an IReg");
        return NULL;
    }

    PyIReg* a = (PyIReg*)self;
    PyIReg* b = (PyIReg*)other;

    // 2. Выполняем реальную операцию в твоей библиотеке loops
    if (a->reg && b->reg) {
        *(a->reg) += *(b->reg); 
    }
    printf("hahahah\n");

    // 3. ВАЖНО: операторы inplace (+=, -=) ДОЛЖНЫ возвращать self
    // с увеличенным счетчиком ссылок.
    Py_INCREF(self);
    return self;
}

static PyObject* PyStartFunc(PyObject* self, PyObject* args) {
    const char* name;
    PyObject *obj_a, *obj_b;

    // Парсим аргументы: s (string), O (Object), O (Object)
    if (!PyArg_ParseTuple(args, "sOO", &name, &obj_a, &obj_b)) {
        return NULL; // Если аргументы не подошли, Python выбросит TypeError
    }

    // Проверка, что переданные объекты действительно являются нашими IReg
    if (!PyObject_TypeCheck(obj_a, &PyIRegType) || !PyObject_TypeCheck(obj_b, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be of type IReg");
        return NULL;
    }

    // Приведение типов к нашей структуре
    PyIReg* reg_a = (PyIReg*)obj_a;
    PyIReg* reg_b = (PyIReg*)obj_b;
    
    getImpl(&ctx)->startFunc(name, {reg_a->reg, reg_b->reg});

    // Теперь можно использовать reg_a->reg и reg_b->reg в логике loops
    std::cout << "Creating new loops function: " << name << std::endl;

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

    std::cout << "Returning some register" << std::endl;

    Py_RETURN_NONE;
}

static PyObject *PyEndFunc(PyObject *self, PyObject *args)
{
    getImpl(&ctx)->endFunc();
    Py_RETURN_NONE;
}

static PyObject *py_finish(PyObject *self, PyObject *args)
{
    USE_CONTEXT_(ctx); //DUBUG: use it in every wrapper.
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
    {"finish", py_finish, METH_NOARGS, "Print something."}, // Description of function.
    {"start_func", PyStartFunc, METH_VARARGS, "Create new loops function."},
    {"Return", PyReturn, METH_O, "Loops function's return."},
    {"end_func", PyEndFunc, METH_NOARGS, "End function."},
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
    // 1. Готовим тип (заполняем внутренние слоты Python)
    if (PyType_Ready(&PyIRegType) < 0)
        return NULL;

    // 2. Создаем модуль
    m = PyModule_Create(&pyloopsmodule);
    if (m == NULL)
        return NULL;

    // 3. Добавляем класс IReg в модуль
    Py_INCREF(&PyIRegType);
    if (PyModule_AddObject(m, "IReg", (PyObject *)&PyIRegType) < 0) {
        Py_DECREF(&PyIRegType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}