#include "common.hpp"
Context ctx;

int type_from_numpy(PyObject* obj) {
    if (!obj) {
        PyErr_SetString(PyExc_RuntimeError, "PyLoops: Unsupported type.");
        return -1;
    }
    PyObject* name_attr = nullptr;

    // Сценарий 1: Нам передали np.dtype('int32') или объект, у которого уже есть .name
    name_attr = PyObject_GetAttrString(obj, "name");

    // Сценарий 2: Нам передали сам класс (np.int32). 
    // У классов в Python есть атрибут __name__, который вернет "int32"
    if (!name_attr) {
        PyErr_Clear();
        name_attr = PyObject_GetAttrString(obj, "__name__");
    }

    // Сценарий 3: Нам передали массив, берем obj.dtype.name //DUBUG: are you sure, you need it?
    if (!name_attr) {
        PyErr_Clear();
        PyObject* dtype = PyObject_GetAttrString(obj, "dtype");
        if (dtype) {
            name_attr = PyObject_GetAttrString(dtype, "name");
            Py_DECREF(dtype);
        }
    }

    if (!name_attr) {
        PyErr_Clear();
        // Крайний случай: пробуем repr(obj) и смотрим, есть ли там знакомые слова, 
        // но лучше просто вернуть пустоту и выдать ошибку.
        PyErr_SetString(PyExc_RuntimeError, "PyLoops: Unsupported type.");
        return -1;
    }

    // Извлекаем строку
    std::string name_str = PyUnicode_AsUTF8(name_attr);
    // ВАЖНО: NumPy иногда возвращает имена вроде "intc" или "longlong" 
    // в зависимости от платформы. Возможно, стоит добавить нормализацию.
    Py_DECREF(name_attr);
    int result = -1;
    if (name_str == "int8")    result = TYPE_I8;
    if (name_str == "uint8")   result = TYPE_U8;
    if (name_str == "int16")   result = TYPE_I16;
    if (name_str == "uint16")  result = TYPE_U16;
    if (name_str == "int32")   result = TYPE_I32;
    if (name_str == "uint32")  result = TYPE_U32;
    if (name_str == "int64")   result = TYPE_I64;
    if (name_str == "uint64")  result = TYPE_U64;
    if (name_str == "float16") result = TYPE_FP16;
    if (name_str == "float32") result = TYPE_FP32;
    if (name_str == "float64") result = TYPE_FP64;
    if (result == -1)
        PyErr_SetString(PyExc_RuntimeError, "PyLoops: Unsupported type.");

    return result;
}