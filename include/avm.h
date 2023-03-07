#pragma once
#include "UnitedMemoryLinks.h"
#include <map>
#include <string>

#include "nlohmann/json.hpp"

using namespace std;
using namespace nlohmann;

/*
МО в теории множеств представляется как множество взаимосвязанных кортежей длины 3:

R ⊆ S×O×E = {(s,o,e): s ∈ S, o ∈ O, e ∈ E} - множество кортежей отношений

Ent = Rel[Rel] = "[]" = json null
Rel = Sub[Obj] = "[[]][[,]]" = "[[],[,]]" = "" = json root
Sub = Rel[Ent] = "[[]]"
Obj = Ent[Rel] = "[][]" = "[,]"
0 = Rel[Sub] = "[[[]]]" = json false
1 = Rel[Obj] = "[[][]]" = "[[,]]" = json true


*/

/*/ две структуры относятся к хранению МО, а две к древовидному контексту исполнения
struct ent_so; //  субъективация объекта отношения
struct obj_er; //  сущность отношения субъективации
struct sub_re; //  отношение сущности объекта
struct rel_os; //  объективация субъекта сущности

template <typename impl_t, typename m_t> struct member_t;

template <typename impl_t>
struct member_t<impl_t, ent_so> {
    union {
        impl_t *map;
        ent_so *ent{nullptr};
        ent_so *val;
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
struct member_t<impl_t, rel_os> {
    union {
        impl_t *map;
        rel_os *rel{nullptr};
        rel_os *val;
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

//template<typename T, typename... Ts> concept any_of = (std::same_as<T, Ts> || ...);
//template<typename T> concept any_oers = any_of<T, ent_so, obj_er, sub_re, rel_os>;

template <typename src_t, typename dst_t, typename key_t, typename my_t>
struct link_t
    : member_t<link_t<key_t,  my_t, dst_t, src_t>, src_t>,
      member_t<link_t< my_t, key_t, src_t, dst_t>, dst_t>,
      map<key_t const *, src_t *>
{
    using src = member_t<link_t<key_t,  my_t, dst_t, src_t>, src_t>;
    using dst = member_t<link_t< my_t, key_t, src_t, dst_t>, dst_t>;

    link_t(src_t *source = nullptr, dst_t *dest = nullptr) {
        update(source ? source : as<src_t>(), dest ? dest : as<dst_t>());
    }
    ~link_t() {
        // 1. удаляем все дочерние связи
        // 2. удаляемся из родителя
        update();
    }

    void update(src_t *source = nullptr, dst_t *dest = nullptr) {
        if (dst::map) dst::map->erase(dst::map->find(src::val));
        src::val = source;
        dst::val = dest;
        if (dst::map) (*dst::map)[src::val] = static_cast<my_t*>(this);
    }

    template <typename _link_t>
    _link_t *as() { return reinterpret_cast<_link_t *>(this); }
};


//  0. Множество кортежей - сущностей Модели Отношений
//  Кортеж сущность ассоциируется только с отношением образуя объекты:
//  e = (s,o,r) = ((s,o),r) = (so,r) = (e,r) = o
//  Множество кортежей объект-субъекта эквивалентно множеству кортежей сущностей:
//  SO ⊆ S×O = {(s,o): s ∈ S, o ∈ O}
//  SO = E
//  ent_so имеет карту поиска субъектов ent<-sub-+rel по отношению map<rel_os, sub_er>;
//  т.е. множество субъектов сущности
struct ent_so : link_t<sub_re, obj_er, rel_os, ent_so> {
    template <typename... Args>
    ent_so(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};


//  1. Множество кортежей - объектов Модели Отношений
//  Кортеж объект ассоциируется только с субъектом образуя сущности:
//  o = (e,r,s) = ((e,r),s) = (er,s) = (o,s) = r
//  Множество кортежей сущность-отношения эквивалентно множеству кортежей объектов:
//  ER ⊆ E×R = {(e,r): e ∈ E, r ∈ R}
//  ER = O
//  obj_er имеет карту поиска сущностей obj|<-ent-+|sub по субъекту map<sub_re, ent_so>;
//  т.е. множество сущностей объекта
struct obj_er : link_t<ent_so, rel_os, sub_re, obj_er> {
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
//  sub_re имеет карту поиска отношений sub|<-rel-+|obj по объекту map<obj_er, rel_os>;
//  т.е. множество отношений субъекта
struct sub_re : link_t<rel_os, ent_so, obj_er, sub_re> {
    template <typename... Args>
    sub_re(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};


//  3. Множество кортежей - отношений Модели Отношений
//  Кортеж отношение ассоциируется только с сущностью образуя субъекты:
//  r = (o,s,e) = ((o,s),e) = (os,e) = (r,e) = s
//  Множество кортежей субъект-объекта эквивалентно множеству кортежей отношений:
//  OS ⊆ O×S = {(o,s): o ∈ O, s ∈ S}
//  OS = R
//  rel_os имеет карту поиска объектов rel|<-obj-+|ent по сущности map<ent_so, obj_er>;
//  т.е. множество объектов отношения
struct rel_os : link_t<obj_er, sub_re, ent_so, rel_os> {
    template <typename... Args>
    rel_os(Args &&...args)
        : link_t(std::forward<Args>(args)...) {
    }
};


*/
/////////////////////////////////////////////////////////////////////////////
//  философское понятие "отношение" соответствует понятию "адресация" в информатике
//  т.е. соотнесение (настройка) приёмника информации с передатчиком
// 4 аспекта:
// rel_t;   //  Контекст
// sub_t;   //  Источник
// obj_t;   //  Назначение
// ent_t;   //  Индекс

template <typename impl_t>
struct sub_aspect
{
    union
    {
        impl_t *rel;
        impl_t *sub{nullptr};
    };
};

template <typename impl_t>
struct obj_aspect
{
    union
    {
        impl_t *rel;
        impl_t *obj{nullptr};
    };
};

template <typename impl_t>
struct ent_aspect : map<impl_t const *, impl_t *>
{
};

struct rel_t : sub_aspect<rel_t>, obj_aspect<rel_t>, ent_aspect<rel_t>
{
    using sub_t = sub_aspect<rel_t>;
    using obj_t = obj_aspect<rel_t>;
    using ent_t = ent_aspect<rel_t>;

    rel_t(rel_t *src = nullptr, rel_t *dst = nullptr)
    {
        update(src ? src : sub, dst ? dst : obj);
    }
    ~rel_t()
    {
        // 1. удаляем все дочерние связи
        // 2. удаляемся из родителя
        update();
    }

    void update(rel_t *src = nullptr, rel_t *dst = nullptr)
    {
        if (sub)
            sub->erase(sub->find(obj));
        sub = src;
        obj = dst;
        if (sub)
            (*sub)[obj] = this;
    }

    template <typename to_t>
    to_t *to() { return static_cast<to_t *>(this); }

    static inline rel_t *R;
    static inline rel_t *E;
    static inline rel_t *S;
    static inline rel_t *O;
    static inline rel_t *True;
    static inline rel_t *False;

private:
#define static_rel(name)                                       \
    static auto unique##name = unique_ptr<rel_t>(new rel_t()); \
    rel_t::name = unique##name.get();

    static inline struct base_voc
    {
        base_voc()
        {
            static_rel(E); //	null
            static_rel(O);
            static_rel(R);
            static_rel(S);
            static_rel(True);
            static_rel(False);
            //	Configure base vocabulary
            E->update(R, R);
            R->update(S, O);
            O->update(E, R);
            S->update(R, E);
            False->update(R, S);
            True->update(R, O);
        }
    } voc;
};
