/****************************************************************************
** Meta object code from reading C++ file 'control_panel.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/modules/control_panel/control_panel.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'control_panel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_PanelControlModule_t {
    QByteArrayData data[15];
    char stringdata0[239];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_PanelControlModule_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_PanelControlModule_t qt_meta_stringdata_PanelControlModule = {
    {
QT_MOC_LITERAL(0, 0, 18), // "PanelControlModule"
QT_MOC_LITERAL(1, 19, 19), // "onSelectCompanyLogo"
QT_MOC_LITERAL(2, 39, 0), // ""
QT_MOC_LITERAL(3, 40, 13), // "onSaveCompany"
QT_MOC_LITERAL(4, 54, 10), // "onSaveSmtp"
QT_MOC_LITERAL(5, 65, 10), // "onTestSmtp"
QT_MOC_LITERAL(6, 76, 17), // "onRefreshPrinters"
QT_MOC_LITERAL(7, 94, 14), // "onSavePrinters"
QT_MOC_LITERAL(8, 109, 15), // "onPrintTestPage"
QT_MOC_LITERAL(9, 125, 21), // "onChangeAdminPassword"
QT_MOC_LITERAL(10, 147, 20), // "onSaveSecurityTimers"
QT_MOC_LITERAL(11, 168, 18), // "onSaveLicenseToken"
QT_MOC_LITERAL(12, 187, 18), // "onGenerateDevToken"
QT_MOC_LITERAL(13, 206, 14), // "onCreateBackup"
QT_MOC_LITERAL(14, 221, 17) // "onRefreshAuditLog"

    },
    "PanelControlModule\0onSelectCompanyLogo\0"
    "\0onSaveCompany\0onSaveSmtp\0onTestSmtp\0"
    "onRefreshPrinters\0onSavePrinters\0"
    "onPrintTestPage\0onChangeAdminPassword\0"
    "onSaveSecurityTimers\0onSaveLicenseToken\0"
    "onGenerateDevToken\0onCreateBackup\0"
    "onRefreshAuditLog"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_PanelControlModule[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   79,    2, 0x08 /* Private */,
       3,    0,   80,    2, 0x08 /* Private */,
       4,    0,   81,    2, 0x08 /* Private */,
       5,    0,   82,    2, 0x08 /* Private */,
       6,    0,   83,    2, 0x08 /* Private */,
       7,    0,   84,    2, 0x08 /* Private */,
       8,    0,   85,    2, 0x08 /* Private */,
       9,    0,   86,    2, 0x08 /* Private */,
      10,    0,   87,    2, 0x08 /* Private */,
      11,    0,   88,    2, 0x08 /* Private */,
      12,    0,   89,    2, 0x08 /* Private */,
      13,    0,   90,    2, 0x08 /* Private */,
      14,    0,   91,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void PanelControlModule::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PanelControlModule *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onSelectCompanyLogo(); break;
        case 1: _t->onSaveCompany(); break;
        case 2: _t->onSaveSmtp(); break;
        case 3: _t->onTestSmtp(); break;
        case 4: _t->onRefreshPrinters(); break;
        case 5: _t->onSavePrinters(); break;
        case 6: _t->onPrintTestPage(); break;
        case 7: _t->onChangeAdminPassword(); break;
        case 8: _t->onSaveSecurityTimers(); break;
        case 9: _t->onSaveLicenseToken(); break;
        case 10: _t->onGenerateDevToken(); break;
        case 11: _t->onCreateBackup(); break;
        case 12: _t->onRefreshAuditLog(); break;
        default: ;
        }
    }
    (void)_a;
}

QT_INIT_METAOBJECT const QMetaObject PanelControlModule::staticMetaObject = { {
    QMetaObject::SuperData::link<ModuleBase::staticMetaObject>(),
    qt_meta_stringdata_PanelControlModule.data,
    qt_meta_data_PanelControlModule,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *PanelControlModule::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PanelControlModule::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_PanelControlModule.stringdata0))
        return static_cast<void*>(this);
    return ModuleBase::qt_metacast(_clname);
}

int PanelControlModule::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = ModuleBase::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
