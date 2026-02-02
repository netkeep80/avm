#include "json_io.h"
#include <iostream>
using namespace std;

void print_rel(const rel_t* ent, string name = "", int depth = 0) {
    string indent(depth*2, ' ');
    if (!name.empty()) cout << indent << name << ": ";
    else cout << indent;

    if (ent == rel_t::E) { cout << "E (null)" << endl; return; }
    if (ent == rel_t::R) { cout << "R (array)" << endl; return; }
    if (ent == rel_t::True) { cout << "True" << endl; return; }
    if (ent == rel_t::False) { cout << "False" << endl; return; }
    if (ent == rel_t::Unsigned) { cout << "Unsigned" << endl; return; }
    if (ent == rel_t::Integer) { cout << "Integer" << endl; return; }
    if (ent == rel_t::Float) { cout << "Float" << endl; return; }

    cout << "rel@" << (void*)ent << endl;
    if (depth < 10) {
        print_rel(ent->obj, "obj", depth+1);
        print_rel(ent->sub, "sub", depth+1);
    }
}

int main() {
    cout << "=== Base vocabulary ===" << endl;
    cout << "E: obj="; print_rel(rel_t::E->obj, "", 0);
    cout << "E: sub="; print_rel(rel_t::E->sub, "", 0);
    cout << "R: obj="; print_rel(rel_t::R->obj, "", 0);
    cout << "R: sub="; print_rel(rel_t::R->sub, "", 0);
    cout << "True: obj="; print_rel(rel_t::True->obj, "", 0);
    cout << "True: sub="; print_rel(rel_t::True->sub, "", 0);
    cout << "False: obj="; print_rel(rel_t::False->obj, "", 0);
    cout << "False: sub="; print_rel(rel_t::False->sub, "", 0);

    cout << endl << "=== null ===" << endl;
    json j_null = json(nullptr);
    rel_t* null_r = import_json(j_null);
    print_rel(null_r, "null");

    cout << endl << "=== [true] ===" << endl;
    json j_arr = json::array({true});
    rel_t* arr_r = import_json(j_arr);
    print_rel(arr_r, "[true]");

    cout << endl << "=== [true, false] ===" << endl;
    json j_arr2 = json::array({true, false});
    rel_t* arr2_r = import_json(j_arr2);
    print_rel(arr2_r, "[true, false]");

    cout << endl << "=== \"a\" ===" << endl;
    json j_str = json("a");
    rel_t* str_r = import_json(j_str);
    // Only show top 4 levels
    cout << "str: rel@" << (void*)str_r << endl;
    cout << "  obj: ";
    if (str_r->obj == rel_t::E) cout << "E" << endl;
    else if (str_r->obj == rel_t::R) cout << "R" << endl;
    else cout << "rel@" << (void*)str_r->obj << endl;
    cout << "  sub: ";
    if (str_r->sub == rel_t::E) cout << "E" << endl;
    else if (str_r->sub == rel_t::R) cout << "R" << endl;
    else cout << "rel@" << (void*)str_r->sub << endl;

    // Traverse the bit chain
    cout << endl << "=== Bit chain for \"a\" ===" << endl;
    const rel_t* cur = str_r;
    int level = 0;
    while (cur != rel_t::E && cur != rel_t::R && level < 20) {
        string obj_name = "?", sub_name = "?";
        if (cur->obj == rel_t::E) obj_name = "E";
        else if (cur->obj == rel_t::R) obj_name = "R";
        else if (cur->obj == rel_t::True) obj_name = "True";
        else if (cur->obj == rel_t::False) obj_name = "False";
        else obj_name = "rel@" + to_string((uintptr_t)cur->obj);

        if (cur->sub == rel_t::E) sub_name = "E";
        else if (cur->sub == rel_t::R) sub_name = "R";
        else if (cur->sub == rel_t::True) sub_name = "True";
        else if (cur->sub == rel_t::False) sub_name = "False";
        else sub_name = "rel@" + to_string((uintptr_t)cur->sub);

        cout << "Level " << level << ": obj=" << obj_name << " sub=" << sub_name << endl;

        // Go deeper through obj (the chain direction)
        cur = cur->obj;
        level++;
    }
    if (cur == rel_t::E) cout << "End: E" << endl;
    if (cur == rel_t::R) cout << "End: R" << endl;

    cout << endl << "=== 42u ===" << endl;
    json j_num = json(42u);
    rel_t* num_r = import_json(j_num);
    cout << "num: rel@" << (void*)num_r << endl;
    cout << "  obj: ";
    if (num_r->obj == rel_t::E) cout << "E" << endl;
    else if (num_r->obj == rel_t::R) cout << "R" << endl;
    else if (num_r->obj == rel_t::Unsigned) cout << "Unsigned" << endl;
    else cout << "rel@" << (void*)num_r->obj << endl;
    cout << "  sub: ";
    if (num_r->sub == rel_t::E) cout << "E" << endl;
    else if (num_r->sub == rel_t::R) cout << "R" << endl;
    else if (num_r->sub == rel_t::Unsigned) cout << "Unsigned" << endl;
    else cout << "rel@" << (void*)num_r->sub << endl;

    return 0;
}
