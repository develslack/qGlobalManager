/****************************************************************************
** Meta object code from reading C++ file 'supportTablesWidget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/modules/control_panel/supportTablesWidget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'supportTablesWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SupportTablesWidget_t {
    QByteArrayData data[21];
    char stringdata0[279];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SupportTablesWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SupportTablesWidget_t qt_meta_stringdata_SupportTablesWidget = {
    {
QT_MOC_LITERAL(0, 0, 19), // "SupportTablesWidget"
QT_MOC_LITERAL(1, 20, 10), // "onSaveRole"
QT_MOC_LITERAL(2, 31, 0), // ""
QT_MOC_LITERAL(3, 32, 15), // "onResetRoleForm"
QT_MOC_LITERAL(4, 48, 10), // "onEditRole"
QT_MOC_LITERAL(5, 59, 6), // "roleId"
QT_MOC_LITERAL(6, 66, 4), // "name"
QT_MOC_LITERAL(7, 71, 11), // "description"
QT_MOC_LITERAL(8, 83, 26), // "onConfigureRolePermissions"
QT_MOC_LITERAL(9, 110, 8), // "roleName"
QT_MOC_LITERAL(10, 119, 8), // "isSystem"
QT_MOC_LITERAL(11, 128, 18), // "onToggleRoleActive"
QT_MOC_LITERAL(12, 147, 15), // "currentlyActive"
QT_MOC_LITERAL(13, 163, 16), // "onCatalogChanged"
QT_MOC_LITERAL(14, 180, 5), // "index"
QT_MOC_LITERAL(15, 186, 17), // "onSaveCatalogItem"
QT_MOC_LITERAL(16, 204, 18), // "onResetCatalogForm"
QT_MOC_LITERAL(17, 223, 17), // "onEditCatalogItem"
QT_MOC_LITERAL(18, 241, 6), // "itemId"
QT_MOC_LITERAL(19, 248, 4), // "code"
QT_MOC_LITERAL(20, 253, 25) // "onToggleCatalogItemActive"

    },
    "SupportTablesWidget\0onSaveRole\0\0"
    "onResetRoleForm\0onEditRole\0roleId\0"
    "name\0description\0onConfigureRolePermissions\0"
    "roleName\0isSystem\0onToggleRoleActive\0"
    "currentlyActive\0onCatalogChanged\0index\0"
    "onSaveCatalogItem\0onResetCatalogForm\0"
    "onEditCatalogItem\0itemId\0code\0"
    "onToggleCatalogItemActive"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SupportTablesWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   64,    2, 0x08 /* Private */,
       3,    0,   65,    2, 0x08 /* Private */,
       4,    3,   66,    2, 0x08 /* Private */,
       8,    3,   73,    2, 0x08 /* Private */,
      11,    3,   80,    2, 0x08 /* Private */,
      13,    1,   87,    2, 0x08 /* Private */,
      15,    0,   90,    2, 0x08 /* Private */,
      16,    0,   91,    2, 0x08 /* Private */,
      17,    3,   92,    2, 0x08 /* Private */,
      20,    2,   99,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString,    5,    6,    7,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Bool,    5,    9,   10,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Bool,    5,    9,   12,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString,   18,   19,    6,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool,   18,   12,

       0        // eod
};

void SupportTablesWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SupportTablesWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onSaveRole(); break;
        case 1: _t->onResetRoleForm(); break;
        case 2: _t->onEditRole((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 3: _t->onConfigureRolePermissions((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 4: _t->onToggleRoleActive((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 5: _t->onCatalogChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->onSaveCatalogItem(); break;
        case 7: _t->onResetCatalogForm(); break;
        case 8: _t->onEditCatalogItem((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 9: _t->onToggleCatalogItemActive((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject SupportTablesWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_SupportTablesWidget.data,
    qt_meta_data_SupportTablesWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *SupportTablesWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SupportTablesWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SupportTablesWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SupportTablesWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 10;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
