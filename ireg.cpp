#include "ireg.hpp"
#include "common.hpp"
extern "C"
{

PyObject *PyIReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyIReg *self;
    self = (PyIReg *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        // Инициализируем указатель на C++ объект нулевым значением
        self->reg = nullptr;
    }
    return (PyObject *)self;
}

// 2. tp_init: Инициализация (аналог __init__)
int PyIReg_init(PyIReg *self, PyObject *args, PyObject *kwds)
{
    // Здесь мы создаем реальный объект IReg из твоей библиотеки loops
    // Например, просто создаем новый экземпляр регистра
    self->reg = new loops::IReg(); 
    if (self->reg == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not allocate IReg");
        return -1;
    }
    return 0;
}

// 3. Не забываем про деструктор (tp_dealloc), чтобы не было утечек памяти!
void PyIReg_dealloc(PyIReg *self)
{
    if (self->reg)
        delete self->reg; // Удаляем C++ объект
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PyObject* PyIReg_iadd(PyObject* self, PyObject* other) {
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

    // 3. ВАЖНО: операторы inplace (+=, -=) ДОЛЖНЫ возвращать self
    // с увеличенным счетчиком ссылок.
    Py_INCREF(self);
    return self;
}

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
    (binaryfunc)PyIReg_iadd,    // nb_inplace_add
    // ... остальные поля можно оставить нулевыми
};

// Функция-сеттер для атрибута "assign"
int PyIReg_set_assign(PyIReg* self, PyObject* value, void* closure) {
    // 1. Проверяем, что нам передают IReg
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the assign attribute");
        return -1;
    }
    if (!PyObject_TypeCheck(value, &PyIRegType)) {
        PyErr_SetString(PyExc_TypeError, "Assign requires an IReg object");
        return -1;
    }

    PyIReg* other = (PyIReg*)value;

    // 2. Выполняем присваивание в loops
    if (self->reg && other->reg) {
        *(self->reg) = *(other->reg); // Генерируем MOV инструкцию
    }

    return 0; // Успех
}

// Таблица геттеров и сеттеров
static PyGetSetDef PyIReg_getset[] = {
    {"assign", 
     NULL,                        // Геттер (нам не нужно читать a.assign)
     (setter)PyIReg_set_assign,   // Сеттер (запись в a.assign = ...)
     "Assignment helper", 
     NULL},
    {NULL} // Конец таблицы
};

PyTypeObject PyIRegType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyloops.IReg",
    .tp_basicsize = sizeof(PyIReg),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyIReg_dealloc,
    .tp_as_number = &PyIReg_as_number,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Loops Register",
    .tp_getset = PyIReg_getset,
    .tp_init = (initproc)PyIReg_init,
    .tp_new = PyIReg_new,
};

}