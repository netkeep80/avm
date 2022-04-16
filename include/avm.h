#pragma once
#include "UnitedMemoryLinks.h"
#include <map>
#include <string>

#include "nlohmann/json.hpp"

using namespace std;
using namespace nlohmann;

/*
МО в теории множеств как множество взаимосвязанных множеств кортежей длины 2:
SO = E = ER - множество сущностей эквивалентно множеству субъектов-объектов
ER = O = OS - множество объектов эквивалентно множеству сущностей-отношений
OS = R = RE - множество отношений эквивалентно множеству объектов-субъектов
RE = S = SO - множество субъектов  эквивалентно множеству отношений-сущностей
*/

//  кэш поиска связей по source
// var query = new Link<uint>(index: any, source: id, target: any);
template <typename key_t, typename val_t>
using linkmap = map<key_t const *, val_t *>;
// struct linkmap : public map<key_t*, val_t&> {};

struct ent_t; //  субъективация объекта отношения
struct obj_t; //  сущность отношения субъективации
struct rel_t; //  объект субъективации сущности
struct sub_t; //  отношение сущности объекта

//////////////////////////////////////////////////////////////////////
//      индексные карты использования

//  карта поиска сущностей субъекта (obj->ent) по объекту
using entmap_t = linkmap<obj_t, ent_t>;

//  карта поиска объектов сущности (obj->ent) по отношению
using objmap_t = linkmap<rel_t, obj_t>;

//  карта поиска отношений объекта (obj->ent) по субъекту
using relmap_t = linkmap<sub_t, rel_t>;

//  карта поиска субъектов отношения (obj->ent) по сущности
using submap_t = linkmap<ent_t, sub_t>;

//  Множество кортежей - сущностей Модели Отношений
//  Кортеж сущность эквивалентен кортежу сущность-отношения:
//  e = (s,o,r) = ((s,o),r) = (so,r) = (e,r) = o
//  Находим, что множество кортежей субъект-объекта эквивалентно множеству кортежей сущность-отношения:
//  SO ⊆ S×O = {(s,o): s ∈ S, o ∈ O}
//  SO = E = ER
struct ent_t : public objmap_t //  т.е. множество объектов сущности
{
    union
    {
        entmap_t *entmap;    // сущности субъекта
        sub_t *sub{nullptr}; // субъект сущности
    };
    obj_t *obj{nullptr}; // объект сущности

    ent_t(sub_t *s = nullptr, obj_t *o = nullptr)
    {
        update(s ? s : &as<sub_t>(), o ? o : &as<obj_t>());
    }

    ~ent_t()
    {
        // 1. удаляем все дочерние связи
        update(); // 2. удаляемся из родителя
    }

    void update(sub_t *s = nullptr, obj_t *o = nullptr)
    {
        if (sub)
            entmap->erase(entmap->find(obj));
        sub = s;
        obj = o;
        (*entmap)[obj] = this;
    }

    template <typename _T>
    _T &as() { return *reinterpret_cast<_T *>(this); }
};

//  Множество кортежей - объектов Модели Отношений
//  Кортеж объект эквивалентен кортежу объект-субъекта:
//  o = (e,r,s) = ((e,r),s) = (er,s) = (o,s) = r
//  Множество кортежей сущность-отношения эквивалентно множеству кортежей объект-субъекта:
//  ER ⊆ E×R = {(e,r): e ∈ E, r ∈ R}
//  ER = O = OS
struct obj_t : public relmap_t //  т.е. множество отношений объекта
{
    union
    {
        objmap_t *objmap;    // множество объектов сущности
        ent_t *ent{nullptr}; // сущность объекта
    };
    rel_t *rel{nullptr}; // отношение объекта

    obj_t(ent_t *s = nullptr, rel_t *o = nullptr)
    {
        update(s ? s : &as<ent_t>(), o ? o : &as<rel_t>());
    }

    ~obj_t()
    {
        // 1. удаляем все дочерние связи
        update(); // 2. удаляемся из родителя
    }

    void update(ent_t *e = nullptr, rel_t *r = nullptr)
    {
        if (ent)
            objmap->erase(objmap->find(rel));
        ent = e;
        rel = r;
        (*objmap)[rel] = this;
    }

    template <typename _T>
    _T &as() { return *reinterpret_cast<_T *>(this); }
};

//  Множество кортежей - отношений Модели Отношений
//  Кортеж отношение эквивалентен кортежу отношение-сущности:
//  r = (o,s,e) = ((o,s),e) = (os,e) = (r,e) = s
//  Множество кортежей объект-субъекта эквивалентно множеству кортежей отношений-сущностей:
//  OS ⊆ O×S = {(o,s): o ∈ O, s ∈ S}
//  OS = R = RE
struct rel_t : public submap_t //  т.е. множество субъектов отношения
{
    union
    {
        relmap_t *relmap;    // множество отношений объекта
        obj_t *obj{nullptr}; // объект отношения
    };
    sub_t *sub{nullptr}; // субъект отношения

    rel_t(obj_t *o = nullptr, sub_t *s = nullptr)
    {
        update(o ? o : &as<obj_t>(), s ? s : &as<sub_t>());
    }

    ~rel_t()
    {
        // 1. удаляем все дочерние связи
        update(); // 2. удаляемся из родителя
    }

    void update(obj_t *o = nullptr, sub_t *s = nullptr)
    {
        if (obj)
            relmap->erase(relmap->find(sub));
        obj = o;
        sub = s;
        (*relmap)[sub] = this;
    }

    template <typename _T>
    _T &as() { return *reinterpret_cast<_T *>(this); }
};

//  Множество кортежей - субъектов Модели Отношений
//  Кортеж субъект эквивалентен кортежу субъект-объекта:
//  s = (r,e,o) = ((r,e),o) = (re,o) = (s,o)
//  Множество кортежей отношение-сущности эквивалентно множеству кортежей субъект-объекта:
//  RE ⊆ R×E = {(r,e): r ∈ R, e ∈ E}
//  RE = S = SO
struct sub_t : public entmap_t //  т.е. множество сущностей субъекта
{
    union
    {
        submap_t *submap;    // множество субъектов отношения
        rel_t *rel{nullptr}; // отношение субъекта
    };
    ent_t *ent{nullptr}; // сущность субъекта

    sub_t(rel_t *r = nullptr, ent_t *e = nullptr)
    {
        update(r ? r : &as<rel_t>(), e ? e : &as<ent_t>());
    }

    ~sub_t()
    {
        // 1. удаляем все дочерние связи
        update(); // 2. удаляемся из родителя
    }

    void update(rel_t *r = nullptr, ent_t *e = nullptr)
    {
        if (rel)
            submap->erase(submap->find(ent));
        rel = r;
        ent = e;
        (*submap)[ent] = this;
    }

    template <typename _T>
    _T &as() { return *reinterpret_cast<_T *>(this); }
};
