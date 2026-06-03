#include "vreg.hpp"
#include "common.hpp"
template<typename _Tp>
loops::VReg<_Tp>& getVReg(void* reg)
{
    return *(static_cast<loops::VReg<_Tp>*>(reg));
}

template <typename _Tp>
VExpr<_Tp> restoreExprType(const Expr& e)
{
    VExpr<_Tp> result;
    result.super = e; //DUBUG: since we have reference counting here, it can be dangerous, check it out.
    return result;
}

extern "C"
{


Expr& PyVReg::getExpr()
{
    assert(expr != nullptr || reg != nullptr);
    if(expr == NULL)
    {
        switch (type)
        {
            case (TYPE_U8):   expr = new Expr(loops::VExpr<uint8_t> (getVReg<uint8_t>(reg)).notype()); break;
            case (TYPE_I8):   expr = new Expr(loops::VExpr<int8_t>  (getVReg<int8_t>(reg)).notype()); break;
            case (TYPE_U16):  expr = new Expr(loops::VExpr<uint16_t>(getVReg<uint16_t>(reg)).notype()); break;
            case (TYPE_I16):  expr = new Expr(loops::VExpr<int16_t> (getVReg<int16_t>(reg)).notype()); break;
            case (TYPE_U32):  expr = new Expr(loops::VExpr<uint32_t>(getVReg<uint32_t>(reg)).notype()); break;
            case (TYPE_I32):  expr = new Expr(loops::VExpr<int32_t> (getVReg<int32_t>(reg)).notype()); break;
            case (TYPE_U64):  expr = new Expr(loops::VExpr<uint64_t>(getVReg<uint64_t>(reg)).notype()); break;
            case (TYPE_I64):  expr = new Expr(loops::VExpr<int64_t> (getVReg<int64_t>(reg)).notype()); break;
            case (TYPE_FP16): expr = new Expr(loops::VExpr<f16_t>   (getVReg<f16_t>(reg)).notype()); break;
            case (TYPE_FP32): expr = new Expr(loops::VExpr<float>   (getVReg<float>(reg)).notype()); break;
            case (TYPE_FP64): expr = new Expr(loops::VExpr<double>  (getVReg<double>(reg)).notype()); break;
        default:
            PyErr_SetString(PyExc_RuntimeError, "PyLoops: Unsupported vector type.");
        }
    }
    return *expr;
}

PyObject *PyVReg_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyVReg *self;
    self = (PyVReg *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        // Инициализируем указатель на C++ объект нулевым значением
        self->reg = nullptr;
        self->expr = nullptr;
    }
    return (PyObject *)self;
}

// 2. tp_init: Инициализация (аналог __init__)
int PyVReg_init(PyVReg *self, PyObject *args, PyObject *kwds)
{
    self->reg = nullptr;
    self->expr = nullptr;

    PyObject *obj_type = nullptr;
    PyObject *maybe_other = nullptr;

    if (!PyArg_ParseTuple(args, "O|O", &obj_type, &maybe_other))
        return 0;
    self->type = type_from_numpy(obj_type);
    if(self->type == -1)
        return 0;

    const bool isSignedInt = self->type == TYPE_I8 || self->type == TYPE_I16 || self->type == TYPE_I32 || self->type == TYPE_I64;
    const bool isUnsignedInt = self->type == TYPE_U8 || self->type == TYPE_U16 || self->type == TYPE_U32 || self->type == TYPE_U64;
    const bool isFloat = self->type == TYPE_FP16 || self->type == TYPE_FP32 || self->type == TYPE_FP64;
    USE_CONTEXT_(ctx);

    if (maybe_other != nullptr && maybe_other != Py_None) {
        // Сценарий 1: Передан другой VReg
        if (PyObject_TypeCheck(maybe_other, &PyVRegType)) {
            PyVReg* other = (PyVReg *)maybe_other;
            if (other->initialized())
            {
                switch (self->type)
                {
                    case (TYPE_I8):   self->reg = new VReg<int8_t>(restoreExprType<int8_t>(other->getExpr())); break;
                    case (TYPE_U8):   self->reg = new VReg<uint8_t>(restoreExprType<uint8_t>(other->getExpr())); break;
                    case (TYPE_I16):  self->reg = new VReg<int16_t>(restoreExprType<int16_t>(other->getExpr())); break;
                    case (TYPE_U16):  self->reg = new VReg<uint16_t>(restoreExprType<uint16_t>(other->getExpr())); break;
                    case (TYPE_I32):  self->reg = new VReg<int32_t>(restoreExprType<int32_t>(other->getExpr())); break;
                    case (TYPE_U32):  self->reg = new VReg<uint32_t>(restoreExprType<uint32_t>(other->getExpr())); break;
                    case (TYPE_I64):  self->reg = new VReg<int64_t>(restoreExprType<int64_t>(other->getExpr())); break;
                    case (TYPE_U64):  self->reg = new VReg<uint64_t>(restoreExprType<uint64_t>(other->getExpr())); break;
                    case (TYPE_FP16): self->reg = new VReg<f16_t>(restoreExprType<f16_t>(other->getExpr())); break;
                    case (TYPE_FP32): self->reg = new VReg<float>(restoreExprType<float>(other->getExpr())); break;
                    case (TYPE_FP64): self->reg = new VReg<double>(restoreExprType<double>(other->getExpr())); break;
                default:
                    break;
                }
            }
            else
            {
                PyErr_SetString(PyExc_RuntimeError, "Source register is uninitialized");
                return -1;
            }
        }
        // Сценарий 2.1: Передано целое число (int/long) со стороны Python
        else if ((isSignedInt || isUnsignedInt || isFloat) && PyLong_Check(maybe_other))
        {
            if(isSignedInt)
            {
                int64_t val = (int64_t)PyLong_AsLongLong(maybe_other);
                if (val == -1 && PyErr_Occurred()) {
                    return -1;
                }
                switch (self->type)
                {
                    case (TYPE_I8):   self->reg = new VReg<int8_t>(VCONST_(int8_t, (int8_t)val)); break;
                    case (TYPE_I16):  self->reg = new VReg<int16_t>(VCONST_(int16_t, (int16_t)val)); break;
                    case (TYPE_I32):  self->reg = new VReg<int32_t>(VCONST_(int32_t, (int32_t)val)); break;
                    case (TYPE_I64):  self->reg = new VReg<int64_t>(VCONST_(int64_t, (int64_t)val)); break;
                    default: break;
                }
            }
            else if(isUnsignedInt)
            {
                uint64_t val = (int64_t)PyLong_AsUnsignedLongLong(maybe_other);
                if (val == -1 && PyErr_Occurred()) {
                    return -1;
                }
                switch (self->type)
                {
                    case (TYPE_U8):   self->reg = new VReg<uint8_t>(VCONST_(uint8_t, (uint8_t)val)); break;
                    case (TYPE_U16):  self->reg = new VReg<uint16_t>(VCONST_(uint16_t, (uint16_t)val)); break;
                    case (TYPE_U32):  self->reg = new VReg<uint32_t>(VCONST_(uint32_t, (uint32_t)val)); break;
                    case (TYPE_U64):  self->reg = new VReg<uint64_t>(VCONST_(uint64_t, (uint64_t)val)); break;
                    default: break;
                }
            }
            else if(isFloat)
            {
                int64_t val = (int64_t)PyLong_AsLongLong(maybe_other);
                switch (self->type)
                {
                    case (TYPE_FP16): self->reg = new VReg<f16_t>(VCONST_(f16_t, (f16_t)(float)val)); break;
                    case (TYPE_FP32): self->reg = new VReg<float>(VCONST_(float, (float)val)); break;
                    case (TYPE_FP64): self->reg = new VReg<double>(VCONST_(double, val)); break;
                }
            }
        }
        // Сценарий 2.2: Передано число с плавающей точкой (float) со стороны Python
        else if (isFloat && PyFloat_Check(maybe_other)) {
            double val = PyFloat_AsDouble(maybe_other);
            if (val == -1.0 && PyErr_Occurred()) {
                return -1;
            }
            switch (self->type)
            {
                case (TYPE_FP16): self->reg = new VReg<f16_t>(VCONST_(f16_t, (f16_t)(float)val)); break;
                case (TYPE_FP32): self->reg = new VReg<float>(VCONST_(float, (float)val)); break;
                case (TYPE_FP64): self->reg = new VReg<double>(VCONST_(double, val)); break;
                
                // Если пытаются проинициализировать целочисленный вектор дробным числом — кидаем ошибку
                default:
                    PyErr_SetString(PyExc_TypeError, "Cannot initialize integer vector register with a float constant");
                    return -1;
            }
        }
        else {
            PyErr_SetString(PyExc_TypeError, "Argument must be an VReg object, an integer, or a float");
            return -1;
        }
    }
    else {
        // Сценарий 3: Без аргументов
        switch (self->type)
        {
            case (TYPE_I8):   self->reg = new VReg<int8_t>(); break;
            case (TYPE_U8):   self->reg = new VReg<uint8_t>(); break;
            case (TYPE_I16):  self->reg = new VReg<int16_t>(); break;
            case (TYPE_U16):  self->reg = new VReg<uint16_t>(); break;
            case (TYPE_I32):  self->reg = new VReg<int32_t>(); break;
            case (TYPE_U32):  self->reg = new VReg<uint32_t>(); break;
            case (TYPE_I64):  self->reg = new VReg<int64_t>(); break;
            case (TYPE_U64):  self->reg = new VReg<uint64_t>(); break;
            case (TYPE_FP16): self->reg = new VReg<f16_t>(); break;
            case (TYPE_FP32): self->reg = new VReg<float>(); break;
            case (TYPE_FP64): self->reg = new VReg<double>(); break;
        default:
            break;
        }        
    }

    if (self->reg == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "Could not allocate VReg");
        return -1;
    }

    return 0;
}

// 3. Не забываем про деструктор (tp_dealloc), чтобы не было утечек памяти!
void PyVReg_dealloc(PyVReg *self)
{
    if (self->reg)
    {
        switch (self->type)
        {
            case (TYPE_U8):   delete &(getVReg<uint8_t>(self->reg)); break;
            case (TYPE_I8):   delete &(getVReg<int8_t>(self->reg)); break;
            case (TYPE_U16):  delete &(getVReg<uint16_t>(self->reg)); break;
            case (TYPE_I16):  delete &(getVReg<int16_t>(self->reg)); break;
            case (TYPE_U32):  delete &(getVReg<uint32_t>(self->reg)); break;
            case (TYPE_I32):  delete &(getVReg<int32_t>(self->reg)); break;
            case (TYPE_U64):  delete &(getVReg<uint64_t>(self->reg)); break;
            case (TYPE_I64):  delete &(getVReg<int64_t>(self->reg)); break;
            case (TYPE_FP16): delete &(getVReg<f16_t>(self->reg)); break;
            case (TYPE_FP32): delete &(getVReg<float>(self->reg)); break;
            case (TYPE_FP64): delete &(getVReg<double>(self->reg)); break;
        }        
    }
    if (self->expr)
        delete self->expr; // Удаляем C++ объект
    Py_TYPE(self)->tp_free((PyObject *)self);
}

// static PyObject* PyVReg_inplace(PyObject* self, PyObject* other, int type) {
//     PyVReg* a = (PyVReg*)self;

//     // 1. Проверка инициализации целевого регистра
//     if (!a->initialized()) {
//         PyErr_SetString(PyExc_RuntimeError, "Target register is uninitialized");
//         return NULL;
//     }

//     // 2. Логика выбора операнда 
//     if (PyObject_TypeCheck(other, &PyVRegType)) {
//         // Сценарий: VReg += VReg
//         PyVReg* b = (PyVReg*)other;
//         if (b->initialized()) {
//             switch (type)
//             {
//             case OP_ADD: *(a->reg) += b->getExpr(); break;
//             case OP_SUB: *(a->reg) -= b->getExpr(); break;
//             case OP_MUL: *(a->reg) *= b->getExpr(); break;
//             case OP_DIV: *(a->reg) /= b->getExpr(); break;
//             case OP_MOD: *(a->reg) %= b->getExpr(); break;
//             case OP_AND: *(a->reg) &= b->getExpr(); break;
//             case OP_OR:  *(a->reg) |= b->getExpr(); break;
//             case OP_XOR: *(a->reg) ^= b->getExpr(); break;
//             case OP_SHL: *(a->reg) <<= b->getExpr(); break;
//             case OP_SHR: *(a->reg) >>= b->getExpr(); break;
//             default: break;
//             }
//         } else {
//             PyErr_SetString(PyExc_RuntimeError, "Source register is uninitialized");
//             return NULL;
//         }
//     }
//     else if (PyLong_Check(other)) {
//         // Сценарий: VReg += int64_t
//         int64_t val = (int64_t)PyLong_AsLongLong(other);
//         if (val == -1 && PyErr_Occurred()) {
//             return NULL;
//         }
//         switch (type)
//         {
//         case OP_ADD: *(a->reg) += val; break;
//         case OP_SUB: *(a->reg) -= val; break;
//         case OP_MUL: *(a->reg) *= val; break;
//         case OP_DIV: *(a->reg) /= val; break;
//         case OP_MOD: *(a->reg) %= val; break;
//         case OP_AND: *(a->reg) &= val; break;
//         case OP_OR:  *(a->reg) |= val; break;
//         case OP_XOR: *(a->reg) ^= val; break;
//         case OP_SHL: *(a->reg) <<= val; break;
//         case OP_SHR: *(a->reg) >>= val; break;
//         default: break;
//         }
//     }
//     else {
//         Py_RETURN_NOTIMPLEMENTED;
//     }

//     // 3. Возвращаем self с увеличенным счетчиком
//     Py_INCREF(self);
//     return (PyObject*)self;
// }

// static PyObject* PyVReg_iadd(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_ADD);
// }

// static PyObject* PyVReg_isub(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_SUB);
// }

// static PyObject* PyVReg_imul(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_MUL);
// }

// static PyObject* PyVReg_idiv(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_DIV);
// }

// static PyObject* PyVReg_imod(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_MOD);
// }

// static PyObject* PyVReg_iand(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_AND);

// }
// static PyObject* PyVReg_ior(PyObject* self, PyObject* other)  {
//     return PyVReg_inplace(self, other, OP_OR);
// }

// static PyObject* PyVReg_ixor(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_XOR);
// }

// static PyObject* PyVReg_ilshift(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_SHL);
// }

// static PyObject* PyVReg_irshift(PyObject* self, PyObject* other) {
//     return PyVReg_inplace(self, other, OP_SHR);
// }

PyObject* PyVReg_binary(PyObject* v, PyObject* w, int opcode, bool maskedtypeout) {
    USE_CONTEXT_(ctx);
    Expr* expr;
    loops::Expr left;
    loops::Expr right;
    if (PyObject_TypeCheck(v, &PyVRegType) && PyObject_TypeCheck(w, &PyVRegType)) {
        left = ((PyVReg*)v)->getExpr();
        right = ((PyVReg*)w)->getExpr();
        if(((PyVReg*)v)->type != ((PyVReg*)w)->type)
            Py_RETURN_NOTIMPLEMENTED;
    }
    else if(PyObject_TypeCheck(v, &PyVRegType) && PyLong_Check(w)) {
        left = ((PyVReg*)v)->getExpr();
        int64_t val = (int64_t)PyLong_AsLongLong(w);
        right = Expr(val);
    }
    else 
        Py_RETURN_NOTIMPLEMENTED;
    int outtype = ((PyVReg*)w)->type;
    if(maskedtypeout)
    {
        outtype = outtype == TYPE_U8 ? TYPE_U8 :
                  outtype == TYPE_I8 ? TYPE_U8 :
                  outtype == TYPE_U16 ? TYPE_U16 :
                  outtype == TYPE_I16 ? TYPE_U16 :
                  outtype == TYPE_U32 ? TYPE_U32 :
                  outtype == TYPE_I32 ? TYPE_U32 :
                  outtype == TYPE_U64 ? TYPE_U64 :
                  outtype == TYPE_I64 ? TYPE_U64 :
                  outtype == TYPE_FP16 ? TYPE_U16 :
                  outtype == TYPE_FP32 ? TYPE_U32 :
                  outtype == TYPE_FP64 ? TYPE_U64 : -1;
        if(outtype == -1)
            Py_RETURN_NOTIMPLEMENTED;
        switch (((PyVReg*)v)->type) {
            case (TYPE_U8):   expr = new Expr(loops::VExpr<ElemTraits<uint8_t>::masktype> (opcode, {left, right}).notype()); break;
            case (TYPE_I8):   expr = new Expr(loops::VExpr<ElemTraits<int8_t>::masktype>  (opcode, {left, right}).notype()); break;
            case (TYPE_U16):  expr = new Expr(loops::VExpr<ElemTraits<uint16_t>::masktype>(opcode, {left, right}).notype()); break;
            case (TYPE_I16):  expr = new Expr(loops::VExpr<ElemTraits<int16_t>::masktype> (opcode, {left, right}).notype()); break;
            case (TYPE_U32):  expr = new Expr(loops::VExpr<ElemTraits<uint32_t>::masktype>(opcode, {left, right}).notype()); break;
            case (TYPE_I32):  expr = new Expr(loops::VExpr<ElemTraits<int32_t>::masktype> (opcode, {left, right}).notype()); break;
            case (TYPE_U64):  expr = new Expr(loops::VExpr<ElemTraits<uint64_t>::masktype>(opcode, {left, right}).notype()); break;
            case (TYPE_I64):  expr = new Expr(loops::VExpr<ElemTraits<int64_t>::masktype> (opcode, {left, right}).notype()); break;
            case (TYPE_FP16): expr = new Expr(loops::VExpr<ElemTraits<f16_t>::masktype>   (opcode, {left, right}).notype()); break;
            case (TYPE_FP32): expr = new Expr(loops::VExpr<ElemTraits<float>::masktype>   (opcode, {left, right}).notype()); break;
            case (TYPE_FP64): expr = new Expr(loops::VExpr<ElemTraits<double>::masktype>  (opcode, {left, right}).notype()); break;
            default: Py_RETURN_NOTIMPLEMENTED;
        }
    }
    else
    {
        switch (((PyVReg*)v)->type) {
            case (TYPE_U8):   expr = new Expr(loops::VExpr<uint8_t> (opcode, {left, right}).notype()); break;
            case (TYPE_I8):   expr = new Expr(loops::VExpr<int8_t>  (opcode, {left, right}).notype()); break;
            case (TYPE_U16):  expr = new Expr(loops::VExpr<uint16_t>(opcode, {left, right}).notype()); break;
            case (TYPE_I16):  expr = new Expr(loops::VExpr<int16_t> (opcode, {left, right}).notype()); break;
            case (TYPE_U32):  expr = new Expr(loops::VExpr<uint32_t>(opcode, {left, right}).notype()); break;
            case (TYPE_I32):  expr = new Expr(loops::VExpr<int32_t> (opcode, {left, right}).notype()); break;
            case (TYPE_U64):  expr = new Expr(loops::VExpr<uint64_t>(opcode, {left, right}).notype()); break;
            case (TYPE_I64):  expr = new Expr(loops::VExpr<int64_t> (opcode, {left, right}).notype()); break;
            case (TYPE_FP16): expr = new Expr(loops::VExpr<f16_t>   (opcode, {left, right}).notype()); break;
            case (TYPE_FP32): expr = new Expr(loops::VExpr<float>   (opcode, {left, right}).notype()); break;
            case (TYPE_FP64): expr = new Expr(loops::VExpr<double>  (opcode, {left, right}).notype()); break;
            default: Py_RETURN_NOTIMPLEMENTED;
        }                
    }
    // Оборачиваем итог в новый объект PyVReg
    PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
    if (!py_res) return NULL;
    py_res->reg = nullptr;
    py_res->type = outtype;
    py_res->expr = expr;
    return (PyObject*)py_res;
}

static PyObject* PyVReg_add(PyObject* v, PyObject* w) { return PyVReg_binary(v, w, VOP_ADD); }
static PyObject* PyVReg_sub(PyObject* v, PyObject* w) { return PyVReg_binary(v, w, VOP_SUB); }
static PyObject* PyVReg_mul(PyObject* v, PyObject* w) { return PyVReg_binary(v, w, VOP_MUL); }
static PyObject* PyVReg_div(PyObject* v, PyObject* w) { return PyVReg_binary(v, w, VOP_DIV); }

static PyObject* PyVReg_and(PyObject* v, PyObject* w) { return PyVReg_binary(v, w, VOP_AND); }
static PyObject* PyVReg_or(PyObject* v, PyObject* w)  { return PyVReg_binary(v, w, VOP_OR); }
static PyObject* PyVReg_xor(PyObject* v, PyObject* w) { return PyVReg_binary(v, w, VOP_XOR); }

static PyObject* PyVReg_lshift(PyObject* self, PyObject* args) { return PyVReg_binary(self, args, VOP_SAL); }
static PyObject* PyVReg_rshift(PyObject* self, PyObject* args) { return PyVReg_binary(self, args, VOP_SAR); }

// static PyObject* PyVReg_unary(PyObject* v, int type) {
//     if (!PyObject_TypeCheck(v, &PyVRegType)) {
//         Py_RETURN_NOTIMPLEMENTED;
//     }

//     PyVReg* self = (PyVReg*)v;
//     loops::VExpr result_expr;

//     switch (type) {
//         case OP_NEG:
//             result_expr = -(self->getExpr());
//             break;
//         case OP_ABS:
//             result_expr = loops::abs(self->getExpr());
//             break;
//         case OP_NOT:
//             result_expr = ~(self->getExpr());
//             break;
//         default:
//             Py_RETURN_NOTIMPLEMENTED;
//     }

//     PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
//     if (!py_res) return NULL;
//     py_res->reg = nullptr;
//     py_res->expr = new loops::VExpr(result_expr);
//     return (PyObject*)py_res;
// }

// static PyObject* PyVReg_negative(PyObject* v) { return PyVReg_unary(v, OP_NEG); }
// static PyObject* PyVReg_abs(PyObject* v)      { return PyVReg_unary(v, OP_ABS); }
// static PyObject* PyVReg_invert(PyObject* v)   { return PyVReg_unary(v, OP_NOT); }

// static PyObject* PyVReg_pow(PyObject* v, PyObject* w, PyObject* z) {
//     // 1. Проверяем, что первый аргумент - наш VReg
//     if (!PyObject_TypeCheck(v, &PyVRegType)) {
//         Py_RETURN_NOTIMPLEMENTED;
//     }

//     // 2. Проверяем, что степень (второй аргумент) - это целое число
//     if (!PyLong_Check(w)) {
//         PyErr_SetString(PyExc_TypeError, "Exponent must be an integer constant");
//         return NULL;
//     }

//     int p = (int)PyLong_AsLong(w);
//     USE_CONTEXT_(ctx); // Твой макрос для контекста

//     // Достаем VExpr из объекта v
//     loops::VExpr base = ((PyVReg*)v)->getExpr();
    
//     // Вызываем ту самую функцию из loops напрямую!
//     loops::VExpr result_expr = loops::pow(base, p);

//     // Оборачиваем результат в новый PyVReg
//     PyVReg* py_res = PyObject_New(PyVReg, &PyVRegType);
//     if (!py_res) return NULL;
//     py_res->reg = nullptr;
//     py_res->expr = new loops::VExpr(result_expr);
//     return (PyObject*)py_res;
// }

static PyNumberMethods PyVReg_as_number = {
    .nb_add = (binaryfunc)PyVReg_add,
    .nb_subtract = (binaryfunc)PyVReg_sub,
    .nb_multiply = (binaryfunc)PyVReg_mul,
    .nb_lshift = (binaryfunc)PyVReg_lshift,
    .nb_rshift = (binaryfunc)PyVReg_rshift,
    .nb_and = (binaryfunc)PyVReg_and,  
    .nb_xor = (binaryfunc)PyVReg_xor,        
    .nb_or = (binaryfunc)PyVReg_or,
    // .nb_int = 0,     
    // .nb_float = 0,                       
    // .nb_inplace_add = (binaryfunc)PyVReg_iadd, 
    // .nb_inplace_subtract = (binaryfunc)PyVReg_isub,
    // .nb_inplace_multiply = (binaryfunc)PyVReg_imul,
    // .nb_inplace_remainder = (binaryfunc)PyVReg_imod,
    // .nb_inplace_lshift = (binaryfunc)PyVReg_ilshift,
    // .nb_inplace_rshift = (binaryfunc)PyVReg_irshift,
    // .nb_inplace_and = (binaryfunc)PyVReg_iand,
    // .nb_inplace_xor = (binaryfunc)PyVReg_ixor,
    // .nb_inplace_or = (binaryfunc)PyVReg_ior,
    // .nb_floor_divide = (binaryfunc)PyVReg_div,
    .nb_true_divide = (binaryfunc)PyVReg_div,
    // .nb_inplace_floor_divide = (binaryfunc)PyVReg_idiv,
};


// Функция-сеттер для атрибута "assign"
int PyVReg_set_assign(PyVReg* self, PyObject* value, void* closure) {
    // 1. Проверяем, что нам передают VReg
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the assign attribute");
        return -1;
    }
    if (!PyObject_TypeCheck(value, &PyVRegType)) {
        PyErr_SetString(PyExc_TypeError, "Assign requires an VReg object");
        return -1;
    }

    PyVReg* other = (PyVReg*)value;

    // 2. Выполняем присваивание в loops
    if (self->reg && other->initialized()) {
        switch (self->type)
        {
            case (TYPE_I8):   getVReg<int8_t>(self->reg)   = restoreExprType<int8_t>(other->getExpr()); break;
            case (TYPE_U8):   getVReg<uint8_t>(self->reg)  = restoreExprType<uint8_t>(other->getExpr()); break;
            case (TYPE_I16):  getVReg<int16_t>(self->reg)  = restoreExprType<int16_t>(other->getExpr()); break;
            case (TYPE_U16):  getVReg<uint16_t>(self->reg) = restoreExprType<uint16_t>(other->getExpr()); break;
            case (TYPE_I32):  getVReg<int32_t>(self->reg)  = restoreExprType<int32_t>(other->getExpr()); break;
            case (TYPE_U32):  getVReg<uint32_t>(self->reg) = restoreExprType<uint32_t>(other->getExpr()); break;
            case (TYPE_I64):  getVReg<int64_t>(self->reg)  = restoreExprType<int64_t>(other->getExpr()); break;
            case (TYPE_U64):  getVReg<uint64_t>(self->reg) = restoreExprType<uint64_t>(other->getExpr()); break;
            case (TYPE_FP16): getVReg<f16_t>(self->reg)    = restoreExprType<f16_t>(other->getExpr()); break;
            case (TYPE_FP32): getVReg<float>(self->reg)    = restoreExprType<float>(other->getExpr()); break;
            case (TYPE_FP64): getVReg<double>(self->reg)   = restoreExprType<double>(other->getExpr()); break;
        default:
            break;
        }
    }

    return 0; // Успех
}

// Таблица геттеров и сеттеров
static PyGetSetDef PyVReg_getset[] = { 
    {"assign", 
     NULL,                        // Геттер (нам не нужно читать a.assign)
     (setter)PyVReg_set_assign,   // Сеттер (запись в a.assign = ...)
     "Assignment helper", 
     NULL},
    {NULL} // Конец таблицы
};

// Вспомогательная функция для определения типа беззнаковой маски
static int get_mask_type(int source_type) {
    switch (source_type) {
        case TYPE_I8:  case TYPE_U8:   return TYPE_U8;
        case TYPE_I16: case TYPE_U16:  case TYPE_FP16: return TYPE_U16;
        case TYPE_I32: case TYPE_U32:  case TYPE_FP32: return TYPE_U32;
        case TYPE_I64: case TYPE_U64:  case TYPE_FP64: return TYPE_U64;
        default: return -1;
    }
}

static PyObject* PyVReg_RichCompare(PyObject* v, PyObject* w, int op)
{
    USE_CONTEXT_(ctx);

    // Сравнение векторов работает только если оба операнда — VReg
    if (!PyObject_TypeCheck(v, &PyVRegType) || !PyObject_TypeCheck(w, &PyVRegType)) {
        Py_RETURN_NOTIMPLEMENTED;
    }
    switch (op)
    {
        case Py_LT: return PyVReg_binary(v, w, VOP_LT, true);
        case Py_LE: return PyVReg_binary(v, w, VOP_LE, true);
        case Py_EQ: return PyVReg_binary(v, w, VOP_EQ, true);
        case Py_NE: return PyVReg_binary(v, w, VOP_NE, true);
        case Py_GT: return PyVReg_binary(v, w, VOP_GT, true);
        case Py_GE: return PyVReg_binary(v, w, VOP_GE, true);
        default: Py_RETURN_NOTIMPLEMENTED;
    }
}

PyTypeObject PyVRegType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyloops.VReg",
    .tp_basicsize = sizeof(PyVReg),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyVReg_dealloc,
    .tp_as_number = &PyVReg_as_number,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Loops Register",
    .tp_richcompare = PyVReg_RichCompare,
    .tp_getset = PyVReg_getset,
    .tp_init = (initproc)PyVReg_init,
    .tp_new = PyVReg_new,
};
}