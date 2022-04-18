#pragma once
#include "UnitedMemoryLinks.h"
#include <map>
#include <string>

#include "nlohmann/json.hpp"

using namespace std;
using namespace nlohmann;

/*
МО в теории множеств представляется как множество 4х взаимосвязанных множеств кортежей длины 3:

RM = {E, O, R, S}, где
R ⊆ O×S×E = {(o,s,e): o ∈ S, s ∈ O, e ∈ E} - множество кортежей отношений
O ⊆ E×R×S = {(e,r,s): e ∈ E, r ∈ R, s ∈ S} - множество кортежей объектов
S ⊆ R×E×O = {(r,e,o): r ∈ R, e ∈ E, o ∈ O} - множество кортежей субъектов
E ⊆ S×O×R = {(s,o,r): s ∈ O, o ∈ S, r ∈ R} - множество кортежей сущностей

МО в теории множеств как множество взаимосвязанных множеств кортежей длины 2:

SO = E = ER - множество сущностей эквивалентно множеству субъектов-объектов
ER = O = OS - множество объектов эквивалентно множеству сущностей-отношений
OS = R = RE - множество отношений эквивалентно множеству объектов-субъектов
RE = S = SO - множество субъектов  эквивалентно множеству отношений-сущностей
*/

struct ent_t; //  субъективация объекта отношения
struct obj_t; //  сущность отношения субъективации
struct rel_t; //  объект субъективации сущности
struct sub_t; //  отношение сущности объекта

template <typename impl_t, typename m_t>
struct member_t;

template <typename impl_t>
struct member_t<impl_t, ent_t>
{
    union
    {
        impl_t *map;
        ent_t *ent{nullptr};
        ent_t *val;
    };
};

template <typename impl_t>
struct member_t<impl_t, obj_t>
{
    union
    {
        impl_t *map;
        obj_t *obj{nullptr};
        obj_t *val;
    };
};

template <typename impl_t>
struct member_t<impl_t, rel_t>
{
    union
    {
        impl_t *map;
        rel_t *rel{nullptr};
        rel_t *val;
    };
};

template <typename impl_t>
struct member_t<impl_t, sub_t>
{
    union
    {
        impl_t *map;
        sub_t *sub{nullptr};
        sub_t *val;
    };
};

template <typename c1_t, typename c2_t, typename key_t>
struct link_t : member_t<link_t<c1_t, c2_t, key_t>, c1_t>, member_t<link_t<c1_t, c2_t, key_t>, c2_t>, map<key_t const *, c2_t *>
{
    using m1_t = member_t<link_t<c1_t, c2_t, key_t>, c1_t>;
    using m2_t = member_t<link_t<c1_t, c2_t, key_t>, c2_t>;

    link_t(c1_t *с1 = nullptr, c2_t *с2 = nullptr)
    {
        update(с1 ? с1 : as<c1_t>(), с2 ? с2 : as<c2_t>());
    }

    ~link_t()
    {
        // 1. удаляем все дочерние связи
        update(); // 2. удаляемся из родителя
    }

    void update(c1_t *c1 = nullptr, c2_t *c2 = nullptr)
    {
        if (m1_t::map)
            m1_t::map->erase(m1_t::map->find(reinterpret_cast<key_t *>(m2_t::val)));
        m1_t::val = c1;
        m2_t::val = c2;
        if (m1_t::map)
            (*m1_t::map)[reinterpret_cast<key_t *>(m2_t::val)] = reinterpret_cast<c2_t *>(this);
    }

    template <typename l_t>
    l_t *as() { return reinterpret_cast<l_t *>(this); }
};

//  Множество кортежей - сущностей Модели Отношений
//  Кортеж сущность эквивалентен кортежу сущность-отношения:
//  e = (s,o,r) = ((s,o),r) = (so,r) = (e,r) = o
//  Находим, что множество кортежей субъект-объекта эквивалентно множеству кортежей сущность-отношения:
//  SO ⊆ S×O = {(s,o): s ∈ S, o ∈ O}
//  SO = E = ER
//  ent_t наследует карту поиска объектов сущности (obj->ent) по отношению map<rel_t, obj_t>;
//  т.е. множество объектов сущности
struct ent_t : link_t<sub_t, obj_t, rel_t>
{
    template <typename... Args>
    ent_t(Args &&...args)
        : link_t(std::forward<Args>(args)...)
    {
    }
};

//  Множество кортежей - объектов Модели Отношений
//  Кортеж объект эквивалентен кортежу объект-субъекта:
//  o = (e,r,s) = ((e,r),s) = (er,s) = (o,s) = r
//  Множество кортежей сущность-отношения эквивалентно множеству кортежей объект-субъекта:
//  ER ⊆ E×R = {(e,r): e ∈ E, r ∈ R}
//  ER = O = OS
//  obj_t наследует карту поиска отношений объекта (obj->ent) по субъекту map<sub_t, rel_t>;
//  т.е. множество отношений объекта
struct obj_t : link_t<ent_t, rel_t, sub_t>
{
    template <typename... Args>
    obj_t(Args &&...args)
        : link_t(std::forward<Args>(args)...)
    {
    }
};

//  Множество кортежей - отношений Модели Отношений
//  Кортеж отношение эквивалентен кортежу отношение-сущности:
//  r = (o,s,e) = ((o,s),e) = (os,e) = (r,e) = s
//  Множество кортежей объект-субъекта эквивалентно множеству кортежей отношений-сущностей:
//  OS ⊆ O×S = {(o,s): o ∈ O, s ∈ S}
//  OS = R = RE
//  rel_t наследует карту поиска субъектов отношения (obj->ent) по сущности map<ent_t, sub_t>;
//  т.е. множество субъектов отношения
struct rel_t : link_t<obj_t, sub_t, ent_t>
{
    template <typename... Args>
    rel_t(Args &&...args)
        : link_t(std::forward<Args>(args)...)
    {
    }
};

//  Множество кортежей - субъектов Модели Отношений
//  Кортеж субъект эквивалентен кортежу субъект-объекта:
//  s = (r,e,o) = ((r,e),o) = (re,o) = (s,o)
//  Множество кортежей отношение-сущности эквивалентно множеству кортежей субъект-объекта:
//  RE ⊆ R×E = {(r,e): r ∈ R, e ∈ E}
//  RE = S = SO
//  sub_t наследует карту поиска сущностей субъекта (obj->ent) по объекту map<obj_t, ent_t>;
//  т.е. множество сущностей субъекта
struct sub_t : link_t<rel_t, ent_t, obj_t>
{
    template <typename... Args>
    sub_t(Args &&...args)
        : link_t(std::forward<Args>(args)...)
    {
    }
};
