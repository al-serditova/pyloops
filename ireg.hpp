#pragma once
#include <Python.h>
#include "loops/loops.hpp"
// #include <iostream>
// #include "/home/vtdrs/work/loops/src/common.hpp"

using namespace loops;

extern "C"
{
extern PyTypeObject PyIRegType;

typedef struct {
    PyObject_HEAD
    IReg* reg; // Указатель на реальный IReg из loops
} PyIReg;

// 1. PyIReg_new - аллокатор
PyObject *PyIReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
// 2. tp_init: Инициализация (аналог __init__)
int PyIReg_init(PyIReg *self, PyObject *args, PyObject *kwds);
// 3. Не забываем про деструктор (tp_dealloc), чтобы не было утечек памяти!
void PyIReg_dealloc(PyIReg *self);

int PyIReg_set_assign(PyIReg* self, PyObject* value, void* closure);
PyObject* PyIReg_iadd(PyObject* self, PyObject* other);

}
