/****************************************************************************
** Meta object code from reading C++ file 'workspace_monitor.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/workspace_monitor.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'workspace_monitor.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_Workspace_monitor_t {
    QByteArrayData data[26];
    char stringdata0[392];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Workspace_monitor_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Workspace_monitor_t qt_meta_stringdata_Workspace_monitor = {
    {
QT_MOC_LITERAL(0, 0, 17), // "Workspace_monitor"
QT_MOC_LITERAL(1, 18, 15), // "desktopsChanged"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 21), // "claudeStatusesChanged"
QT_MOC_LITERAL(4, 57, 18), // "on_desktop_created"
QT_MOC_LITERAL(5, 76, 12), // "QDBusMessage"
QT_MOC_LITERAL(6, 89, 7), // "message"
QT_MOC_LITERAL(7, 97, 18), // "on_desktop_removed"
QT_MOC_LITERAL(8, 116, 23), // "on_desktop_data_changed"
QT_MOC_LITERAL(9, 140, 17), // "on_status_changed"
QT_MOC_LITERAL(10, 158, 14), // "workspace_name"
QT_MOC_LITERAL(11, 173, 5), // "state"
QT_MOC_LITERAL(12, 179, 9), // "tool_name"
QT_MOC_LITERAL(13, 189, 11), // "wait_reason"
QT_MOC_LITERAL(14, 201, 12), // "wait_message"
QT_MOC_LITERAL(15, 214, 14), // "state_since_ms"
QT_MOC_LITERAL(16, 229, 20), // "on_daemon_registered"
QT_MOC_LITERAL(17, 250, 22), // "on_daemon_unregistered"
QT_MOC_LITERAL(18, 273, 19), // "on_desktops_fetched"
QT_MOC_LITERAL(19, 293, 24), // "QDBusPendingCallWatcher*"
QT_MOC_LITERAL(20, 318, 7), // "watcher"
QT_MOC_LITERAL(21, 326, 19), // "on_statuses_fetched"
QT_MOC_LITERAL(22, 346, 15), // "switchToDesktop"
QT_MOC_LITERAL(23, 362, 5), // "index"
QT_MOC_LITERAL(24, 368, 8), // "desktops"
QT_MOC_LITERAL(25, 377, 14) // "claudeStatuses"

    },
    "Workspace_monitor\0desktopsChanged\0\0"
    "claudeStatusesChanged\0on_desktop_created\0"
    "QDBusMessage\0message\0on_desktop_removed\0"
    "on_desktop_data_changed\0on_status_changed\0"
    "workspace_name\0state\0tool_name\0"
    "wait_reason\0wait_message\0state_since_ms\0"
    "on_daemon_registered\0on_daemon_unregistered\0"
    "on_desktops_fetched\0QDBusPendingCallWatcher*\0"
    "watcher\0on_statuses_fetched\0switchToDesktop\0"
    "index\0desktops\0claudeStatuses"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Workspace_monitor[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       2,  104, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   69,    2, 0x06 /* Public */,
       3,    0,   70,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    1,   71,    2, 0x08 /* Private */,
       7,    1,   74,    2, 0x08 /* Private */,
       8,    1,   77,    2, 0x08 /* Private */,
       9,    6,   80,    2, 0x08 /* Private */,
      16,    0,   93,    2, 0x08 /* Private */,
      17,    0,   94,    2, 0x08 /* Private */,
      18,    1,   95,    2, 0x08 /* Private */,
      21,    1,   98,    2, 0x08 /* Private */,

 // methods: name, argc, parameters, tag, flags
      22,    1,  101,    2, 0x02 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong,   10,   11,   12,   13,   14,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 19,   20,
    QMetaType::Void, 0x80000000 | 19,   20,

 // methods: parameters
    QMetaType::Void, QMetaType::Int,   23,

 // properties: name, type, flags
      24, QMetaType::QVariantList, 0x00495001,
      25, QMetaType::QVariantMap, 0x00495001,

 // properties: notify_signal_id
       0,
       1,

       0        // eod
};

void Workspace_monitor::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Workspace_monitor *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->desktopsChanged(); break;
        case 1: _t->claudeStatusesChanged(); break;
        case 2: _t->on_desktop_created((*reinterpret_cast< const QDBusMessage(*)>(_a[1]))); break;
        case 3: _t->on_desktop_removed((*reinterpret_cast< const QDBusMessage(*)>(_a[1]))); break;
        case 4: _t->on_desktop_data_changed((*reinterpret_cast< const QDBusMessage(*)>(_a[1]))); break;
        case 5: _t->on_status_changed((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5])),(*reinterpret_cast< qlonglong(*)>(_a[6]))); break;
        case 6: _t->on_daemon_registered(); break;
        case 7: _t->on_daemon_unregistered(); break;
        case 8: _t->on_desktops_fetched((*reinterpret_cast< QDBusPendingCallWatcher*(*)>(_a[1]))); break;
        case 9: _t->on_statuses_fetched((*reinterpret_cast< QDBusPendingCallWatcher*(*)>(_a[1]))); break;
        case 10: _t->switchToDesktop((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusMessage >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusMessage >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusMessage >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusPendingCallWatcher* >(); break;
            }
            break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusPendingCallWatcher* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Workspace_monitor::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Workspace_monitor::desktopsChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Workspace_monitor::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Workspace_monitor::claudeStatusesChanged)) {
                *result = 1;
                return;
            }
        }
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<Workspace_monitor *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QVariantList*>(_v) = _t->desktops(); break;
        case 1: *reinterpret_cast< QVariantMap*>(_v) = _t->claude_statuses(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    }
#endif // QT_NO_PROPERTIES
}

QT_INIT_METAOBJECT const QMetaObject Workspace_monitor::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_Workspace_monitor.data,
    qt_meta_data_Workspace_monitor,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *Workspace_monitor::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Workspace_monitor::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Workspace_monitor.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Workspace_monitor::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 2;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void Workspace_monitor::desktopsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Workspace_monitor::claudeStatusesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
