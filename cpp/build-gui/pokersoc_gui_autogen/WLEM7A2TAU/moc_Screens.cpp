/****************************************************************************
** Meta object code from reading C++ file 'Screens.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/qt/Screens.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Screens.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::Screen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::Screen",
        "closed",
        "",
        "changed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'closed'
        QtMocHelpers::SignalData<void(bool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Signal 'closed'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Screen, qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::Screen::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::Screen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Screen *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->closed((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->closed(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Screen::*)(bool )>(_a, &Screen::closed, 0))
            return;
    }
}

const QMetaObject *pokersoc::ui::qt::Screen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::Screen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt6ScreenE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int pokersoc::ui::qt::Screen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void pokersoc::ui::qt::Screen::closed(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::BuyInScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::BuyInScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BuyInScreen, qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::BuyInScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::BuyInScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BuyInScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::BuyInScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::BuyInScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt11BuyInScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::BuyInScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::CashOutScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::CashOutScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<CashOutScreen, qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::CashOutScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::CashOutScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CashOutScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::CashOutScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::CashOutScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashOutScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::CashOutScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::CashboxScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::CashboxScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<CashboxScreen, qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::CashboxScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::CashboxScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CashboxScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::CashboxScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::CashboxScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13CashboxScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::CashboxScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::LostChipsScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::LostChipsScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LostChipsScreen, qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::LostChipsScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::LostChipsScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LostChipsScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::LostChipsScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::LostChipsScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt15LostChipsScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::LostChipsScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::FoodScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::FoodScreen",
        "openSettings",
        ""
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'openSettings'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FoodScreen, qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::FoodScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::FoodScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FoodScreen *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->openSettings(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FoodScreen::*)()>(_a, &FoodScreen::openSettings, 0))
            return;
    }
}

const QMetaObject *pokersoc::ui::qt::FoodScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::FoodScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt10FoodScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::FoodScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void pokersoc::ui::qt::FoodScreen::openSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::PlayersScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::PlayersScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PlayersScreen, qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::PlayersScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::PlayersScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PlayersScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::PlayersScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::PlayersScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13PlayersScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::PlayersScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::SettingsScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::SettingsScreen",
        "switchSession",
        "",
        "path",
        "std::optional<domain::SessionModeType>",
        "newMode",
        "closeSession"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'switchSession'
        QtMocHelpers::SignalData<void(const QString &, std::optional<domain::SessionModeType>)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { 0x80000000 | 4, 5 },
        }}),
        // Signal 'closeSession'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SettingsScreen, qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::SettingsScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::SettingsScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SettingsScreen *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->switchSession((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<std::optional<domain::SessionModeType>>>(_a[2]))); break;
        case 1: _t->closeSession(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SettingsScreen::*)(const QString & , std::optional<domain::SessionModeType> )>(_a, &SettingsScreen::switchSession, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SettingsScreen::*)()>(_a, &SettingsScreen::closeSession, 1))
            return;
    }
}

const QMetaObject *pokersoc::ui::qt::SettingsScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::SettingsScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt14SettingsScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::SettingsScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void pokersoc::ui::qt::SettingsScreen::switchSession(const QString & _t1, std::optional<domain::SessionModeType> _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void pokersoc::ui::qt::SettingsScreen::closeSession()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::TournamentSettingsScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::TournamentSettingsScreen",
        "configurePricing",
        "",
        "structureChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'configurePricing'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'structureChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TournamentSettingsScreen, qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::TournamentSettingsScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::TournamentSettingsScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TournamentSettingsScreen *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->configurePricing(); break;
        case 1: _t->structureChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TournamentSettingsScreen::*)()>(_a, &TournamentSettingsScreen::configurePricing, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TournamentSettingsScreen::*)()>(_a, &TournamentSettingsScreen::structureChanged, 1))
            return;
    }
}

const QMetaObject *pokersoc::ui::qt::TournamentSettingsScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::TournamentSettingsScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt24TournamentSettingsScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::TournamentSettingsScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void pokersoc::ui::qt::TournamentSettingsScreen::configurePricing()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void pokersoc::ui::qt::TournamentSettingsScreen::structureChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::TournamentPricingScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::TournamentPricingScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TournamentPricingScreen, qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::TournamentPricingScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::TournamentPricingScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TournamentPricingScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::TournamentPricingScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::TournamentPricingScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt23TournamentPricingScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::TournamentPricingScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::PrizingScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::PrizingScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PrizingScreen, qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::PrizingScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::PrizingScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PrizingScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::PrizingScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::PrizingScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt13PrizingScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::PrizingScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::ComingSoonScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::ComingSoonScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ComingSoonScreen, qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::ComingSoonScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::ComingSoonScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ComingSoonScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::ComingSoonScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::ComingSoonScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt16ComingSoonScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::ComingSoonScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::TournamentBoardControlScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::TournamentBoardControlScreen",
        "openBoardRequested",
        "",
        "closeBoardRequested",
        "sendBoardToTvRequested",
        "toggleBoardFullscreenRequested",
        "boardDisplayChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'openBoardRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'closeBoardRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sendBoardToTvRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'toggleBoardFullscreenRequested'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'boardDisplayChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TournamentBoardControlScreen, qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::TournamentBoardControlScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::TournamentBoardControlScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TournamentBoardControlScreen *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->openBoardRequested(); break;
        case 1: _t->closeBoardRequested(); break;
        case 2: _t->sendBoardToTvRequested(); break;
        case 3: _t->toggleBoardFullscreenRequested(); break;
        case 4: _t->boardDisplayChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TournamentBoardControlScreen::*)()>(_a, &TournamentBoardControlScreen::openBoardRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TournamentBoardControlScreen::*)()>(_a, &TournamentBoardControlScreen::closeBoardRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TournamentBoardControlScreen::*)()>(_a, &TournamentBoardControlScreen::sendBoardToTvRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TournamentBoardControlScreen::*)()>(_a, &TournamentBoardControlScreen::toggleBoardFullscreenRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TournamentBoardControlScreen::*)()>(_a, &TournamentBoardControlScreen::boardDisplayChanged, 4))
            return;
    }
}

const QMetaObject *pokersoc::ui::qt::TournamentBoardControlScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::TournamentBoardControlScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt28TournamentBoardControlScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::TournamentBoardControlScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void pokersoc::ui::qt::TournamentBoardControlScreen::openBoardRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void pokersoc::ui::qt::TournamentBoardControlScreen::closeBoardRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void pokersoc::ui::qt::TournamentBoardControlScreen::sendBoardToTvRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void pokersoc::ui::qt::TournamentBoardControlScreen::toggleBoardFullscreenRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void pokersoc::ui::qt::TournamentBoardControlScreen::boardDisplayChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
namespace {
struct qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t {};
} // unnamed namespace

template <> constexpr inline auto pokersoc::ui::qt::BreakdownScreen::qt_create_metaobjectdata<qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pokersoc::ui::qt::BreakdownScreen"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BreakdownScreen, qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pokersoc::ui::qt::BreakdownScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<Screen::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t>.metaTypes,
    nullptr
} };

void pokersoc::ui::qt::BreakdownScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BreakdownScreen *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *pokersoc::ui::qt::BreakdownScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pokersoc::ui::qt::BreakdownScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8pokersoc2ui2qt15BreakdownScreenE_t>.strings))
        return static_cast<void*>(this);
    return Screen::qt_metacast(_clname);
}

int pokersoc::ui::qt::BreakdownScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Screen::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
