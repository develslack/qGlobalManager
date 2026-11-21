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
    QByteArrayData data[20];
    char stringdata0[333];
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
QT_MOC_LITERAL(6, 76, 21), // "onSmtpSecurityChanged"
QT_MOC_LITERAL(7, 98, 5), // "index"
QT_MOC_LITERAL(8, 104, 23), // "onSmtpAuthMethodChanged"
QT_MOC_LITERAL(9, 128, 22), // "onAuthorizeGoogleOAuth"
QT_MOC_LITERAL(10, 151, 18), // "onClearGoogleOAuth"
QT_MOC_LITERAL(11, 170, 17), // "onRefreshPrinters"
QT_MOC_LITERAL(12, 188, 14), // "onSavePrinters"
QT_MOC_LITERAL(13, 203, 15), // "onPrintTestPage"
QT_MOC_LITERAL(14, 219, 21), // "onChangeAdminPassword"
QT_MOC_LITERAL(15, 241, 20), // "onSaveSecurityTimers"
QT_MOC_LITERAL(16, 262, 18), // "onSaveLicenseToken"
QT_MOC_LITERAL(17, 281, 18), // "onGenerateDevToken"
QT_MOC_LITERAL(18, 300, 14), // "onCreateBackup"
QT_MOC_LITERAL(19, 315, 17) // "onRefreshAuditLog"

    },
    "PanelControlModule\0onSelectCompanyLogo\0"
    "\0onSaveCompany\0onSaveSmtp\0onTestSmtp\0"
    "onSmtpSecurityChanged\0index\0"
    "onSmtpAuthMethodChanged\0onAuthorizeGoogleOAuth\0"
    "onClearGoogleOAuth\0onRefreshPrinters\0"
    "onSavePrinters\0onPrintTestPage\0"
    "onChangeAdminPassword\0onSaveSecurityTimers\0"
    "onSaveLicenseToken\0onGenerateDevToken\0"
    "onCreateBackup\0onRefreshAuditLog"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_PanelControlModule[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   99,    2, 0x08 /* Private */,
       3,    0,  100,    2, 0x08 /* Private */,
       4,    0,  101,    2, 0x08 /* Private */,
       5,    0,  102,    2, 0x08 /* Private */,
       6,    1,  103,    2, 0x08 /* Private */,
       8,    1,  106,    2, 0x08 /* Private */,
       9,    0,  109,    2, 0x08 /* Private */,
      10,    0,  110,    2, 0x08 /* Private */,
      11,    0,  111,    2, 0x08 /* Private */,
      12,    0,  112,    2, 0x08 /* Private */,
      13,    0,  113,    2, 0x08 /* Private */,
      14,    0,  114,    2, 0x08 /* Private */,
      15,    0,  115,    2, 0x08 /* Private */,
      16,    0,  116,    2, 0x08 /* Private */,
      17,    0,  117,    2, 0x08 /* Private */,
      18,    0,  118,    2, 0x08 /* Private */,
      19,    0,  119,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void, QMetaType::Int,    7,
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
        case 4: _t->onSmtpSecurityChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 5: _t->onSmtpAuthMethodChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->onAuthorizeGoogleOAuth(); break;
        case 7: _t->onClearGoogleOAuth(); break;
        case 8: _t->onRefreshPrinters(); break;
        case 9: _t->onSavePrinters(); break;
        case 10: _t->onPrintTestPage(); break;
        case 11: _t->onChangeAdminPassword(); break;
        case 12: _t->onSaveSecurityTimers(); break;
        case 13: _t->onSaveLicenseToken(); break;
        case 14: _t->onGenerateDevToken(); break;
        case 15: _t->onCreateBackup(); break;
        case 16: _t->onRefreshAuditLog(); break;
        default: ;
        }
    }
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
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 17;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
