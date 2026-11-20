/****************************************************************************
** Meta object code from reading C++ file 'users.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/modules/users/users.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'users.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_UsuariosModule_t {
    QByteArrayData data[16];
    char stringdata0[193];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_UsuariosModule_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_UsuariosModule_t qt_meta_stringdata_UsuariosModule = {
    {
QT_MOC_LITERAL(0, 0, 14), // "UsuariosModule"
QT_MOC_LITERAL(1, 15, 10), // "onSaveUser"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 11), // "onResetForm"
QT_MOC_LITERAL(4, 39, 19), // "onSearchTextChanged"
QT_MOC_LITERAL(5, 59, 4), // "text"
QT_MOC_LITERAL(6, 64, 10), // "onEditUser"
QT_MOC_LITERAL(7, 75, 6), // "userId"
QT_MOC_LITERAL(8, 82, 8), // "fullName"
QT_MOC_LITERAL(9, 91, 5), // "email"
QT_MOC_LITERAL(10, 97, 6), // "roleId"
QT_MOC_LITERAL(11, 104, 21), // "onEditUserPermissions"
QT_MOC_LITERAL(12, 126, 12), // "isSystemRole"
QT_MOC_LITERAL(13, 139, 19), // "onResendNewPassword"
QT_MOC_LITERAL(14, 159, 17), // "onToggleBlockUser"
QT_MOC_LITERAL(15, 177, 15) // "currentlyActive"

    },
    "UsuariosModule\0onSaveUser\0\0onResetForm\0"
    "onSearchTextChanged\0text\0onEditUser\0"
    "userId\0fullName\0email\0roleId\0"
    "onEditUserPermissions\0isSystemRole\0"
    "onResendNewPassword\0onToggleBlockUser\0"
    "currentlyActive"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_UsuariosModule[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x08 /* Private */,
       3,    0,   50,    2, 0x08 /* Private */,
       4,    1,   51,    2, 0x08 /* Private */,
       6,    4,   54,    2, 0x08 /* Private */,
      11,    4,   63,    2, 0x08 /* Private */,
      13,    4,   72,    2, 0x08 /* Private */,
      14,    5,   81,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::Int,    7,    8,    9,   10,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Int, QMetaType::Bool,    7,    8,   10,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::Bool,    7,    8,    9,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::Bool, QMetaType::Bool,    7,    8,    9,   15,   12,

       0        // eod
};

void UsuariosModule::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<UsuariosModule *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onSaveUser(); break;
        case 1: _t->onResetForm(); break;
        case 2: _t->onSearchTextChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->onEditUser((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< int(*)>(_a[4]))); break;
        case 4: _t->onEditUserPermissions((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< bool(*)>(_a[4]))); break;
        case 5: _t->onResendNewPassword((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< bool(*)>(_a[4]))); break;
        case 6: _t->onToggleBlockUser((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< bool(*)>(_a[4])),(*reinterpret_cast< bool(*)>(_a[5]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject UsuariosModule::staticMetaObject = { {
    QMetaObject::SuperData::link<ModuleBase::staticMetaObject>(),
    qt_meta_stringdata_UsuariosModule.data,
    qt_meta_data_UsuariosModule,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *UsuariosModule::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UsuariosModule::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_UsuariosModule.stringdata0))
        return static_cast<void*>(this);
    return ModuleBase::qt_metacast(_clname);
}

int UsuariosModule::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = ModuleBase::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
