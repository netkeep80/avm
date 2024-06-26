#pragma once
#include "UnitedMemoryLinks.h"
#include <map>
#include <iostream>
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

R = E[E] - отношение есть отношение сущности (двойное отрицание)
E = R[E] - сущность есть сущность отношения (отрицание бытия или отношения)
E = E[E][E] - сущность есть кортеж длины 3 из 3х сущностей


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
//  Философское понятие "отношение" соответствует понятию "передача информации" в информатике
//  Философское понятие "сущность" соответствует понятию "информация" в информатике,
//  а суть информации есть соотнесение (настройка) приёмника информации с передатчиком
// 4 аспекта:
// rel_t;   //  Контекст
// obj_t;   //  Источник сущности
// sub_t;   //  Приёмник сущности
// ent_t;   //  Индекс
//  https://habr.com/ru/post/127327/

template <typename impl_t>
struct obj_aspect
{
    union
    {
        impl_t *rel;
        impl_t *obj;
    };
};

template <typename impl_t>
struct sub_aspect
{
    union
    {
        impl_t *rel;
        impl_t *sub;
    };
};

template <typename impl_t>
struct ent_aspect : map<impl_t const *, impl_t *>
{
};

struct rel_t : obj_aspect<rel_t>,
               sub_aspect<rel_t>,
               ent_aspect<rel_t>    //  сущности данного отношения
{
    using obj_t = obj_aspect<rel_t>;
    using sub_t = sub_aspect<rel_t>;
    using ent_t = ent_aspect<rel_t>;

    ~rel_t()
    {
        // 1. удаляем все дочерние связи
        // 2. удаляемся из родителя
        if (obj)
            obj->erase(obj->find(sub));
        __deleted++;
    }

    void update(rel_t *src = nullptr, rel_t *dst = nullptr)
    {
        if (obj)
            obj->erase(obj->find(sub));
        obj = src;
        sub = dst;
        if (obj)
            (*obj)[sub] = this;
    }

    template <typename to_t>
    to_t *to() { return static_cast<to_t *>(this); }

    static rel_t *rel()
    {
        rel_t *r;
        db->emplace_back(r = new rel_t());
        return r;
    }
    static rel_t *rel(rel_t *src)
    {
        rel_t *r;
        db->emplace_back(r = new rel_t(src));
        return r;
    }
    static rel_t *rel(rel_t *src, rel_t *dst)
    {
        auto it = src->find(dst);
        if (it != src->end())
            return it->second;
        rel_t *r;
        db->emplace_back(r = new rel_t(src, dst));
        return r;
    }
    static auto count() { return db->size(); }
    static auto created() { return __created; }
    static auto deleted() { return __deleted; }

    static inline rel_t *R; //  [], is type for array
    static inline rel_t *E; //  {}, is type for binary relation
    static inline rel_t *True;
    static inline rel_t *False;
    static inline rel_t *Unsigned;
    static inline rel_t *Integer;
    static inline rel_t *Float;

protected:
    rel_t()
    {
        obj = this; //  null link
        sub = this; //  null link
        (*obj)[sub] = this;
        __created++;
    }
    rel_t(rel_t *src)
    {
        obj = src;
        sub = this; //  null link
        (*obj)[sub] = this;
        __created++;
    }
    rel_t(rel_t *src, rel_t *dst)
    {
        obj = src;
        sub = dst;
        (*obj)[sub] = this;
        __created++;
    }

private:
    static inline unique_ptr<vector<unique_ptr<rel_t>>> db;
    static inline int __created{0};
    static inline int __deleted{0};

#define add_rel(name) rel_t::name = rel_t::rel();

    static inline struct base_voc
    {
        base_voc()
        {
            //	Configure base vocabulary
            //  базовый словарь необходим для проекции понятий теории множеств в МАО
            db = make_unique<vector<unique_ptr<rel_t>>>();
            add_rel(R);       //  [], корневая связь определяет корневой тип - УП (следование)
            add_rel(E);       //  null, {},  корневая связь определяет корневой тип - последовательность УП
            add_rel(False);     //  ложь субъективна
            add_rel(True);      //  правда объективна
            add_rel(Unsigned);
            add_rel(Integer);
            add_rel(Float);

            //  всё связи к E это множество определений соответствий
            //  всё связи к R это начало списков сущностей, это начало вложенных УП для представления кортежей длины N
            //  множество кортежей длины 2 есть отображение соответствия в соответствие
            R->update(E, E); //  [] = E[E] = <0,1>
            //  соответствие есть отображение множества кортежей длины 2 в соответствие
            E->update(R, E); //  {} = R[E] = <1>
            //  таким образом Ent = { (Rel,Ent), (Ent,Ent) }
            /*
            Судя по всему в ТС типизируются связи, потому что другого и быть не может
            Типизацию описывает тоже связь: субьективации
            Связь ( D, L ) субъективно привязывает дуплет D к типу L
            При этом образуется экземпляр типа L, который тоже может выступить новым типом
            Таким образом получаются списки наследников
            Списки могут образовывать новые дуплеты, которые можно вновь субъективно типизировать
            Термин типизация возможно не очень подходит для ТС
            */            

            False->update(E, R); //  False is R
            True->update(R, R);  //  True is R

            //  Определяем отображение соответствия Boolean
            //  (Boolean,E)
            //  ((True,True),Boolean)
            //  ((False,False),Boolean)

            Unsigned->update(Unsigned, R);  //  Unsigned is subtype of R
            Integer->update(Integer, R);    //  Integer is subtype of R
            Float->update(Float, R);        //  Float is subtype of R

            //  все связи не к E или R это подтипы от E и R, либо просто null
            //  json object {} определяет соответствие (ФБО) между множеством ключей и множеством значений
            //  если среди значений есть null значит
            //  null != []

            //	нужен формат сериализации относительного адреса в строку
            //	строка значение и строка ключ могут различаться в АМО
            //	не стоит путать [] в json и в выражении индекса
        }
        ~base_voc()
        {
            db->clear();
            // std::cout << "rel_t::count() = " << rel_t::count() << std::endl;
            // std::cout << "rel_t::created() = " << rel_t::created() << std::endl;
            // std::cout << "rel_t::deleted() = " << rel_t::deleted() << std::endl;
        }
    } voc;

    friend base_voc;
};

/*
Правда, приносит разрушение в мир иллюзий.
Ложь, как иллюзия, приносит разрушение в мир правды.
Но, и в своём мире иллюзий, ложь полнит его и уплотняет.
Правда, в своём мире, синхронизирует и развивает.

void is a keyword that means that a function does not result a value.

java.lang.Void is a reference type, then the following is valid:

Void nil = null;
(so far it is not interesting...)

As a result type (a function with a return value of type Void) it means that the function *always * return null (it cannot return anything other than null, because Void has no instances).

Void function(int a, int b) {
    //do something
    return null;
}

null is not an object, it is a primitive value.
For example, you cannot add properties to it.
Sometimes people wrongly assume that it is an object,
because typeof null returns "object".
But that is actually a bug (that might even be fixed in ECMAScript 6).

null: used by programmers to indicate “no value”, e.g. as a parameter to a function.

> Boolean(null)
false
> Boolean("")
false
> Boolean(3-3)
false
> Boolean({})
true
> Boolean([])
true
*/
