#pragma once
#include "UnitedMemoryLinks.h"
#include <map>
#include <string>

#include "nlohmann/json.hpp"

using namespace std;
using namespace nlohmann;

/*
МО в теории множеств представляется как множество 4х взаимосвязанных множеств кортежей длины 3:

RM = {E, O, S, R}, где
E ⊆ O×S×R = {(o,s,r): o ∈ O, s ∈ S, r ∈ R} - множество кортежей сущностей
O ⊆ E×R×S = {(e,r,s): e ∈ E, r ∈ R, s ∈ S} - множество кортежей объектов
S ⊆ R×E×O = {(r,e,o): r ∈ R, e ∈ E, o ∈ O} - множество кортежей субъектов
R ⊆ S×O×E = {(s,o,e): s ∈ S, o ∈ O, e ∈ E} - множество кортежей отношений

МО в теории множеств как множество взаимосвязанных множеств кортежей длины 2:

OS = E - множество сущностей эквивалентно множеству связей объектов  -> субъектов
ER = O - множество объектов  эквивалентно множеству связей сущностей -> отношений
RE = S - множество субъектов эквивалентно множеству связей отношений -> сущностей
SO = R - множество отношений эквивалентно множеству связей субъектов -> объектов
*/

// две структуры относятся к хранению МО, а две к древовидному контексту исполнения
struct ent_os; //  объект субъективации отношения
struct obj_er; //  сущность отношения субъективации
struct sub_re; //  отношение сущности объекта
struct rel_so; //  субъективации объекта сущности

/*      simple implementation
struct ent_os { // E = (O x S) x R = (OS×R) = (E×R) = ER
    obj_er* obj;
    sub_re* sub;
};

struct obj_er { // O = (E x R) x S = (ER×S) = (O×S) = OS
    ent_os* ent;
    rel_so* rel;
};

struct sub_re { // S = (R x E) x O = (RE×O) = (SxO) = SO
    rel_so* rel;
    ent_os* ent;
};

struct rel_so { // R = (S x O) x E = (SO×E) = (RxE) = RE
    sub_re* sub;
    obj_er* obj;
};
*/

template <typename impl_t, typename m_t>
struct member_t;

template <typename impl_t>
struct member_t<impl_t, ent_os> {
    union {
        impl_t *map;
        ent_os *ent{nullptr};
        ent_os *val;
    };
};

template <typename impl_t>
struct member_t<impl_t, obj_er> {
    union {
        impl_t *map;
        obj_er *obj{nullptr};
        obj_er *val;
    };
};

template <typename impl_t>
struct member_t<impl_t, rel_so> {
    union {
        impl_t *map;
        rel_so *rel{nullptr};
        rel_so *val;
    };
};

template <typename impl_t>
struct member_t<impl_t, sub_re> {
    union {
        impl_t *map;
        sub_re *sub{nullptr};
        sub_re *val;
    };
};

template<typename T, typename... Ts>
concept any_of = (std::same_as<T, Ts> || ...);

//template<typename T>
//concept any_oers = any_of<T, ent_os, obj_er, sub_re, rel_so>;

template <typename src_t, typename dst_t, typename key_t, typename my_t>
struct link_t
    : member_t<link_t<my_t, key_t, dst_t, src_t>, src_t>,
      member_t<link_t<key_t, my_t, src_t, dst_t>, dst_t>,
      map<key_t const *, src_t *>
{
    using src = member_t<link_t<my_t, key_t, dst_t, src_t>, src_t>;
    using dst = member_t<link_t<key_t, my_t, src_t, dst_t>, dst_t>;

    link_t(src_t *source = nullptr, dst_t *dest = nullptr) {
        update(source ? source : as<src_t>(), dest ? dest : as<dst_t>());
    }
    ~link_t() {
        // 1. удаляем все дочерние связи
        // 2. удаляемся из родителя
        update(); 
    }

    void update(src_t *source = nullptr, dst_t *dest = nullptr) {
        if (src::map) src::map->erase(src::map->find(dst::val));
        src::val = source;
        dst::val = dest;
        if (src::map) (*src::map)[dst::val] = static_cast<my_t*>(this);
    }

    template <typename _link_t>
    _link_t *as() { return reinterpret_cast<_link_t *>(this); }
};


//  0. Множество кортежей - сущностей Модели Отношений
//  Кортеж сущность ассоциируется только с отношением образуя объекты:
//  e = (o,s,r) = ((o,s),r) = (os,r) = (e,r) = o
//  Множество кортежей объект-субъекта эквивалентно множеству кортежей сущностей:
//  OS ⊆ O×S = {(o,s): o ∈ O, s ∈ S}
//  OS = E
//  ent_os имеет карту поиска объектов сущности ent|+-obj->|rel по отношению map<rel_so, obj_er>;
//  т.е. множество объектов сущности
struct ent_os : link_t<obj_er, sub_re, rel_so, ent_os> {
    template <typename... Args>
    ent_os(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};


//  1. Множество кортежей - объектов Модели Отношений
//  Кортеж объект ассоциируется только с субъектом образуя сущности:
//  o = (e,r,s) = ((e,r),s) = (er,s) = (o,s) = r
//  Множество кортежей сущность-отношения эквивалентно множеству кортежей объектов:
//  ER ⊆ E×R = {(e,r): e ∈ E, r ∈ R}
//  ER = O
//  obj_er имеет карту поиска сущностей объекта obj|+-ent->|sub по субъекту map<sub_re, ent_os>;
//  т.е. множество сущностей объекта
struct obj_er : link_t<ent_os, rel_so, sub_re, obj_er> {
    template <typename... Args>
    obj_er(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};


//  2. Множество кортежей - субъектов Модели Отношений
//  Кортеж субъект ассоциируется только с объектом образуя отношения:
//  s = (r,e,o) = ((r,e),o) = (re,o) = (s,o) = r
//  Множество кортежей отношение-сущности эквивалентно множеству кортежей субъектов:
//  RE ⊆ R×E = {(r,e): r ∈ R, e ∈ E}
//  RE = S
//  sub_re имеет карту поиска отношений субъекта sub|+-rel->|obj по объекту map<obj_er, rel_so>;
//  т.е. множество отношений субъекта
struct sub_re : link_t<rel_so, ent_os, obj_er, sub_re> {
    template <typename... Args>
    sub_re(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};


//  3. Множество кортежей - отношений Модели Отношений
//  Кортеж отношение ассоциируется только с сущностью образуя субъекты:
//  r = (s,o,e) = ((s,o),e) = (so,e) = (r,e) = s
//  Множество кортежей субъект-объекта эквивалентно множеству кортежей отношений:
//  SO ⊆ S×O = {(s,o): s ∈ S, o ∈ O}
//  SO = R
//  rel_so имеет карту поиска субъектов отношения rel|+-sub->|ent по сущности map<ent_os, sub_re>;
//  т.е. множество субъектов отношения
struct rel_so : link_t<sub_re, obj_er, ent_os, rel_so> {
    template <typename... Args>
    rel_so(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};
