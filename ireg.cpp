#include "ireg.hpp"
#include "common.hpp"
extern "C"
{

IExpr& PyIReg::getExpr()
{
    assert(expr != nullptr || reg != nullptr);
    if(expr == NULL)
        expr = new IExpr(*reg);
    return *expr;
}

PyObject *PyIReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyIReg *self;
    self = (PyIReg *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        // Инициализируем указатель на C++ объект нулевым значением
        self->reg = nullptr;
        self->expr = nullptr;
    }
    return (PyObject *)self;
}

// 2. tp_init: Инициализация (аналог __init__)
int PyIReg_init(PyIReg *self, PyObject *args, PyObject *kwds)
{
    if (self->reg)
        delete self->reg;
    if (self->expr)
        delete self->expr;

    self->reg = nullptr;
    self->expr = nullptr;

    PyObject *maybe_other = nullptr;

    // Парсим аргументы: "|" означает, что аргументы после него опциональны.
    // "O" — ожидаем объект.
    if (!PyArg_ParseTuple(args, "|O", &maybe_other)) {
        return -1; // ParseTuple сам выставит ошибку типа
    }

    if (maybe_other != nullptr && maybe_other != Py_None) {
        // Сценарий 1: Передан другой IReg
        if (PyObject_TypeCheck(maybe_other, &PyIRegType)) {
            PyIReg *other = (PyIReg *)maybe_other;
            if (other->initialized()) {
                self->reg = new loops::IReg(other->getExpr());
            } else {
                PyErr_SetString(PyExc_RuntimeError, "Source register is uninitialized");
                return -1;
            }
        }
        // Сценарий 2: Передано целое число (int64_t)
        else if (PyLong_Check(maybe_other)) {
            int64_t val = (int64_t)PyLong_AsLongLong(maybe_other);
            
            // Если возникла ошибка конвертации (число слишком большое)
            if (val == -1 && PyErr_Occurred()) {
                return -1;
            }
            USE_CONTEXT_(ctx);
            // Создаем регистр с константой (immediate)
            self->reg = new loops::IReg(CONST_(val));
        }
        else {
            PyErr_SetString(PyExc_TypeError, "Argument must be an IReg object or an integer");
            return -1;
        }
    } 
    else {
        // Сценарий 3: Без аргументов
        self->reg = new loops::IReg();
    }

    if (self->reg == nullptr) {
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
    if (self->expr)
        delete self->expr; // Удаляем C++ объект
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* PyIReg_inplace(PyObject* self, PyObject* other, int type) {
        PyIReg* a = (PyIReg*)self;

    // 1. Проверка инициализации целевого регистра
    if (!a->initialized()) {
        PyErr_SetString(PyExc_RuntimeError, "Target register is uninitialized");
        return NULL;
    }

    // 2. Логика выбора операнда 
    if (PyObject_TypeCheck(other, &PyIRegType)) {
        // Сценарий: IReg += IReg
        PyIReg* b = (PyIReg*)other;
        if (b->initialized()) {
            switch (type)
            {
            case OP_ADD: *(a->reg) += b->getExpr(); break;
            case OP_SUB: *(a->reg) -= b->getExpr(); break;
            case OP_MUL: *(a->reg) *= b->getExpr(); break;
            case OP_DIV: *(a->reg) /= b->getExpr(); break;
            case OP_MOD: *(a->reg) %= b->getExpr(); break;
            default:
                break;
            }
        } else {
            PyErr_SetString(PyExc_RuntimeError, "Source register is uninitialized");
            return NULL;
        }
    }
    else if (PyLong_Check(other)) {
        // Сценарий: IReg += int64_t
        int64_t val = (int64_t)PyLong_AsLongLong(other);
        if (val == -1 && PyErr_Occurred()) {
            return NULL;
        }
        // Используем перегрузку loops для int64_t (или CONST_(val))
        switch (type)
        {
        case OP_ADD: *(a->reg) += val; break;
        case OP_SUB: *(a->reg) -= val; break;
        case OP_MUL: *(a->reg) *= val; break;
        case OP_DIV: *(a->reg) /= val; break;
        case OP_MOD: *(a->reg) %= val; break;
        default:
            break;
        }
    }
    else {
        Py_RETURN_NOTIMPLEMENTED;
    }

    // 3. Возвращаем self с увеличенным счетчиком
    Py_INCREF(self);
    return (PyObject*)self;
}

static PyObject* PyIReg_iadd(PyObject* self, PyObject* other) {
    return PyIReg_inplace(self, other, OP_ADD);
}

static PyObject* PyIReg_isub(PyObject* self, PyObject* other) {
    return PyIReg_inplace(self, other, OP_SUB);
}

static PyObject* PyIReg_imul(PyObject* self, PyObject* other) {
    return PyIReg_inplace(self, other, OP_MUL);
}

static PyObject* PyIReg_idiv(PyObject* self, PyObject* other) {
    return PyIReg_inplace(self, other, OP_DIV);
}

static PyObject* PyIReg_imod(PyObject* self, PyObject* other) {
    return PyIReg_inplace(self, other, OP_MOD);
}

static PyNumberMethods PyIReg_as_number = {
    .nb_add = 0,    
    .nb_subtract = 0,
    .nb_multiply = 0,
    .nb_remainder = 0,
    .nb_divmod = 0,  
    .nb_power = 0,  
    .nb_negative = 0,
    .nb_positive = 0,
    .nb_absolute = 0,
    .nb_bool = 0,   
    .nb_invert = 0,  
    .nb_lshift = 0,  
    .nb_rshift = 0,  
    .nb_and = 0,    
    .nb_xor = 0,    
    .nb_or = 0,     
    .nb_int = 0,    
    // .reserved = 0,  
    .nb_float = 0,                       
    .nb_inplace_add = (binaryfunc)PyIReg_iadd, 
    .nb_inplace_subtract = (binaryfunc)PyIReg_isub,
    .nb_inplace_multiply = (binaryfunc)PyIReg_imul,
    .nb_inplace_remainder = (binaryfunc)PyIReg_imod,
    .nb_inplace_floor_divide = (binaryfunc)PyIReg_idiv,
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
    if (self->reg && other->initialized()) {
        *(self->reg) = other->getExpr(); // Генерируем MOV инструкцию
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

static PyObject* PyIReg_RichCompare(PyObject* v, PyObject* w, int op)
{
    // v — это может быть PyIReg, а w — число, или наоборот.
    // Нам нужно извлечь IExpr из обоих.
    
    USE_CONTEXT_(ctx);
    
    loops::IExpr result_expr;
    if (PyObject_TypeCheck(v, &PyIRegType) && PyObject_TypeCheck(w, &PyIRegType))
    {
        loops::IExpr left = ((PyIReg*)v)->getExpr();
        loops::IExpr right = ((PyIReg*)w)->getExpr();
        switch (op)
        {
            case Py_LT: result_expr = (left < right);  break;
            case Py_LE: result_expr = (left <= right); break;
            case Py_EQ: result_expr = (left == right); break;
            case Py_NE: result_expr = (left != right); break;
            case Py_GT: result_expr = (left > right);  break;
            case Py_GE: result_expr = (left >= right); break;
            default: Py_RETURN_NOTIMPLEMENTED;
        }
    }
    else if (PyObject_TypeCheck(v, &PyIRegType) && PyLong_Check(w))
    {
        loops::IExpr left = ((PyIReg*)v)->getExpr();
        int64_t right = (int64_t)PyLong_AsLongLong(w);
        switch (op)
        {
            case Py_LT: result_expr = (left < right);  break;
            case Py_LE: result_expr = (left <= right); break;
            case Py_EQ: result_expr = (left == right); break;
            case Py_NE: result_expr = (left != right); break;
            case Py_GT: result_expr = (left > right);  break;
            case Py_GE: result_expr = (left >= right); break;
            default: Py_RETURN_NOTIMPLEMENTED;
        }
    }
    else if (PyLong_Check(v) && PyObject_TypeCheck(w, &PyIRegType))
    {
        int64_t  left = (int64_t)PyLong_AsLongLong(v);
        loops::IExpr right = ((PyIReg*)w)->getExpr();
        switch (op)
        {
            case Py_LT: result_expr = (left < right);  break;
            case Py_LE: result_expr = (left <= right); break;
            case Py_EQ: result_expr = (left == right); break;
            case Py_NE: result_expr = (left != right); break;
            case Py_GT: result_expr = (left > right);  break;
            case Py_GE: result_expr = (left >= right); break;
            default: Py_RETURN_NOTIMPLEMENTED;
        }
    }
    else
    {
        Py_RETURN_NOTIMPLEMENTED;
    }

    // Оборачиваем результат (IExpr) в новый PyIReg
    PyIReg* res_obj = PyObject_New(PyIReg, &PyIRegType);
    res_obj->reg = nullptr;
    res_obj->expr = new loops::IExpr(result_expr);
    return (PyObject*)res_obj;
}

PyTypeObject PyIRegType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyloops.IReg",
    .tp_basicsize = sizeof(PyIReg),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyIReg_dealloc,
    .tp_as_number = &PyIReg_as_number,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Loops Register",
    .tp_richcompare = (richcmpfunc)PyIReg_RichCompare,
    .tp_getset = PyIReg_getset,
    .tp_init = (initproc)PyIReg_init,
    .tp_new = PyIReg_new,
};
}