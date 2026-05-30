#pragma once
#include "loops/loops.hpp"
#include <Python.h>

using namespace loops;
extern Context ctx;

int type_from_numpy(PyObject* obj);