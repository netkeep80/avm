#pragma once
#include "UnitedMemoryLinks.h"
#include <map>
#include <string>

#include "nlohmann/json.hpp"

using namespace std;
using namespace nlohmann;

/*
у нас 2 условных пространства:
E - пространство сущностей
R - пространство отношений

условных потому что к одной и той же связи можно обратиться и как к сущности и как к отношению

связи типа (Rel: Rel Ent) связывают субъект(отношение)-объект(сущность) и образуют отношения
связи типа (Ent: Ent Rel) связывают сущность-отношение и образуют сущности

если обращаемся к связи как к сущности то имеем (Ent: Ent Rel)
если обращаемся к связи как к отношению то имеем (Rel: Rel Ent)

struct link
{
    link&   source;
    link&   target;
};

*/

//  кэш поиска дуплетов по source
//var query = new Link<uint>(index: any, source: id, target: any);
template<typename key_t, typename val_t>
using linkmap = map<key_t const*, val_t*>;
//struct linkmap : public map<key_t*, val_t&> {};


//////////////////////////////////////////////////////////////////////
//      индексные карты использования

struct obj_t;   //  сущность отношения субъективации
struct sub_t;   //  отношение сущности объекта

//  карта сущностей (sub<-ent) для поиска по субъекту
using entmap_t = linkmap<sub_t, obj_t>;

//  карта отношений (obj<-rel) для поиска по объекту
using relmap_t = linkmap<obj_t, sub_t>;

//  Множество кортежей сущность-отношения эквивалентно множеству кортежей объектов:
//  ER ⊆ E×R = {(e,r): e ∈ E, r ∈ R}
//  ER = O = OS
struct obj_t :
    public entmap_t //  карта сущностей (sub<-ent) для поиска по субъекту
                    //  т.е. множество экземпляров
{
    obj_t* ent{nullptr};  //  сущность отношения субъективации
    sub_t* rel{nullptr};  //  отношение сущности объекта

    obj_t(obj_t* entity = nullptr, sub_t* relation = nullptr)
    {
        ent = entity ? entity : this;
        rel = relation ? relation : &as_sub();
        (*ent)[rel] = this;
    }

    ~obj_t()
    {
        //  удаляем все дочерние сущности
        //  удаляемся из родителя
    }

    void    set(obj_t& entity, sub_t& relation)
    {
        ent = &entity; rel = &relation;
    }

    sub_t& as_sub() { return *reinterpret_cast<sub_t*>(this); }

    //  отнести объект к субъекту
    /*auto  new_rel(sub_t* subject = nullptr)
    {
        if (!subject) subject = &as_sub();
        return new sub_t(subject, this);
    }*/

    //  осуществить в отношении
    auto  new_ent(sub_t* subject = nullptr)
    {
        if (!subject) subject = rel;
        return new obj_t(this, subject);
    }
};


//  Множество кортежей отношение-сущности эквивалентно множеству кортежей субъектов:
//  RE ⊆ R×E = {(r,e): r ∈ R, e ∈ E}
//  RE = S = SO
struct sub_t :
    public relmap_t //  карта отношений (obj<-rel) для поиска по объекту
                    //  т.е. множество атрибутов
{
    sub_t* rel{nullptr};  //  отношение-субъект
    obj_t* ent{nullptr};  //  сущность-объект

    sub_t(sub_t* relation = nullptr, obj_t* entity = nullptr)
    {
        rel = relation ? relation : this;
        ent = entity ? entity : &as_obj();
        (*rel)[ent] = this;
    }

    ~sub_t()
    {
        //  удаляем все дочерние отношения
        //  удаляемся из родителя
    }

    void    set(sub_t& relation, obj_t& entity)
    {
        rel = &relation; ent = &entity;
    }

    obj_t& as_obj() { return *reinterpret_cast<obj_t*>(this); }

    //  отнести субъект к объекту
    auto  new_rel(obj_t* object = nullptr)
    {
        if (!object) object = ent;
        return new sub_t(this, object);
    }

    //  осуществить сущность
    /*auto  new_ent(obj_t* object = nullptr)
    {
        if (!object) object = &as_sub();
        return new obj_t(object, this);
    }*/
};


/*
//////////////////////////////////////////////////////////////////////
//      индексные карты использования

struct ent_t;
struct rel_t;

//    карта (1 to 1) относительных объективаций сущностей (rel -> ent)
using objmap_t = linkmap<rel_t, ent_t>;

//    карта (1 to 1) сущностных субъективаций отношения (ent -> rel)
using submap_t = linkmap<ent_t, rel_t>;


//  сущность это субъективное отношение объекта
//  сущность есть всегда результат субъективации отношения
struct ent_t :
    public objmap_t // карта относительных объективаций сущности (rel -> ent)
                    //  т.е. множество экземпляров
{
    ent_t* ent{nullptr};  //  сущность характеризующая данное
    rel_t* rel{nullptr};  //  отношение

    ent_t(ent_t* entity = nullptr, rel_t* relation = nullptr)
    {
        ent = entity ? entity : this;
        rel = relation ? relation : &to_rel();
    }

    void    set(ent_t& entity, rel_t& relation)
    {
        ent = &entity; rel = &relation;
    }

    rel_t& to_rel() { return *reinterpret_cast<rel_t*>(this); }

    //  To make something (such as an abstract idea) possible
    //  to be perceived by the senses.
    auto  objectify_to(rel_t* relation = nullptr)
    {
        if (!relation) relation = &to_rel();
        return new ent_t();
    }
};


//  отношение это субъективное осуществление объекта
//  отношение есть всегда результат объективации сущности
struct rel_t :
    public submap_t //  карта сущностных субъективаций отношения (ent -> rel)
                    //  т.е. множество атрибутов
{
    rel_t* sub{nullptr};  //  отношение-субъект
    ent_t* obj{nullptr};  //  сущность-объект

    rel_t(rel_t* subject = nullptr, ent_t* object = nullptr)
    {
        sub = subject ? subject : this;
        obj = object ? object : &to_ent();
    }

    void    set(rel_t& subject, ent_t& object)
    {
        sub = &subject; obj = &object;
    }

    ent_t& to_ent() { return *reinterpret_cast<ent_t*>(this); }

    //  The act or process of subjectivizing; the process of change
    //  by which words develop a subjective in place of or alongside
    //  an objective sense.
    auto  subjectivize_to(ent_t* entity = nullptr)
    {
        if (!entity) entity = &to_ent();
        return new rel_t();
    }
};
*/