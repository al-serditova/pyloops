#pragma once
#include <Python.h>
#include "loops/loops.hpp"
// #include <iostream>
// #include "/home/vtdrs/work/loops/src/common.hpp"

using namespace loops;

extern "C"
{
extern PyTypeObject PyVRegType;

typedef struct {
    PyObject_HEAD
    int type;
    void* reg;    // Указатель на VReg
    Expr* expr;
    Expr& getExpr();
    inline bool initialized() { return reg != 0 || expr != 0;} 
} PyVReg;
PyObject* PyVReg_binary(PyObject* v, PyObject* w, int type, bool maskedtypeout = false);
PyObject *PyVReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
int PyVReg_init(PyVReg *self, PyObject *args, PyObject *kwds);
void PyVReg_dealloc(PyVReg *self);
int PyVReg_set_assign(PyVReg* self, PyObject* value, void* closure);
}

template <typename _Tp>
VExpr<_Tp> restoreExprType(const Expr& e)
{
    VExpr<_Tp> result;
    result.super = e; //DUBUG: since we have reference counting here, it can be dangerous, check it out.
    return result;
}