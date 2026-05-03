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
    IReg* reg;    // Указатель на IReg
    IExpr* expr;
    IExpr& getExpr();
    inline bool initialized() { return reg != 0 || expr != 0;} 
} PyIReg;
PyObject* PyIReg_binary(PyObject* v, PyObject* w, int type);
PyObject *PyIReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
int PyIReg_init(PyIReg *self, PyObject *args, PyObject *kwds);
void PyIReg_dealloc(PyIReg *self);

int PyIReg_set_assign(PyIReg* self, PyObject* value, void* closure);

}
