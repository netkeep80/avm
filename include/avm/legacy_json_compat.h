#pragma once

#include "avm.h"

rel_t *eval(rel_t *function, rel_t *argument);
rel_t *eval(rel_t *function, rel_t *first, rel_t *second);

rel_t *import_json(const json &value);
void export_json(const rel_t *entity, json &value);

// Historical API kept as a projection facade while callers migrate.
// Execution itself is performed by JsonCompatibilitySession.
rel_t *interpret(const json &expression);
void clear_func_env();
