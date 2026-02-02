#include "json_io.h"
#include <iostream>
using namespace std;

void debug_rel(const rel_t* ent, string indent = "") {
    if (ent == rel_t::E) { cout << indent << "E" << endl; return; }
    if (ent == rel_t::R) { cout << indent << "R" << endl; return; }
    if (ent == rel_t::True) { cout << indent << "True" << endl; return; }
    if (ent == rel_t::False) { cout << indent << "False" << endl; return; }
    if (ent == rel_t::Unsigned) { cout << indent << "Unsigned" << endl; return; }
    if (ent == rel_t::Integer) { cout << indent << "Integer" << endl; return; }
    if (ent == rel_t::Float) { cout << indent << "Float" << endl; return; }

    cout << indent << "rel(" << (void*)ent << "):" << endl;
    cout << indent << "  obj: ";
    if (ent->obj == rel_t::E) cout << "E" << endl;
    else if (ent->obj == rel_t::R) cout << "R" << endl;
    else if (ent->obj == rel_t::True) cout << "True" << endl;
    else if (ent->obj == rel_t::False) cout << "False" << endl;
    else cout << "rel(" << (void*)ent->obj << ")" << endl;

    cout << indent << "  sub: ";
    if (ent->sub == rel_t::E) cout << "E" << endl;
    else if (ent->sub == rel_t::R) cout << "R" << endl;
    else if (ent->sub == rel_t::True) cout << "True" << endl;
    else if (ent->sub == rel_t::False) cout << "False" << endl;
    else cout << "rel(" << (void*)ent->sub << ")" << endl;
}

void trace_string_encoding(const rel_t* ent, int depth = 0) {
    if (depth > 20) { cout << "  ... (max depth)" << endl; return; }
    debug_rel(ent, string(depth*2, ' '));
    if (ent != rel_t::E && ent != rel_t::R && ent != rel_t::True &&
        ent != rel_t::False && ent != rel_t::Unsigned &&
        ent != rel_t::Integer && ent != rel_t::Float) {
        cout << string(depth*2, ' ') << "  -> obj detail:" << endl;
        trace_string_encoding(ent->obj, depth+1);
        cout << string(depth*2, ' ') << "  -> sub detail:" << endl;
        trace_string_encoding(ent->sub, depth+1);
    }
}

int main() {
    // Import string "a"
    json j = json("a");
    rel_t* str_rel = import_json(j);

    cout << "=== String 'a' structure ===" << endl;
    trace_string_encoding(str_rel);

    cout << endl << "=== Export back ===" << endl;
    json result;
    export_json(str_rel, result);
    cout << "Result: " << result.dump() << endl;

    return 0;
}
