#include "machine.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define PROG_BUF_STEP 64

// Default malloc
void *defMalloc(size_t n) {
    void *p = malloc(n);
    if (p == NULL) {
        printf("Out of memory\n");
        exit(1);
    }
    return p;
}

// Default realloc
void *defRealloc(void *p, size_t n) {
    void *np = realloc(p, n);
    if (np == NULL) {
        printf("Out of memory\n");
        exit(1);
    }
    return np;
}

// Default free
void defFree(void *p) {
    free(p);
}



void freeData(kdl_state_t s, kdl_data_t *d) {
    s.free(d->data);
    memset(d, 0, sizeof(kdl_data_t));
}

void freeEntry(kdl_state_t s, kdl_entry_t *entry) {
    s.free(entry->name);
    freeData(s, &entry->data);
    memset(entry, 0, sizeof(kdl_entry_t));
}

void freeEntry_fwd(kdl_state_t s, void *data) {
    freeEntry(s, (kdl_entry_t *) data);
    s.free(data);
}

void freeVerb_fwd(kdl_state_t s, void *data) {
    s.free(data);
}

void appendRules(kdl_machine_t *m, kdl_rule_t *rules, size_t length, kdl_programBuffer_t *dest) {
    if (dest->length + length > dest->size) {
        // Integer division
        dest->size = (dest->length + length + PROG_BUF_STEP) / PROG_BUF_STEP * PROG_BUF_STEP;
        dest->rules = (kdl_rule_t *) m->s.realloc(dest->rules, sizeof(kdl_rule_t) * dest->size);
    }
    memcpy(dest->rules + dest->length, rules, sizeof(kdl_rule_t) * length);
    dest->length += length;
}

void appendProgram(kdl_machine_t *m, kdl_program_t *src, kdl_programBuffer_t *dest) {
    appendRules(m, src->rules, src->length, dest);
}

void copyString(kdl_machine_t *m, const char *src, char **dest) {
    size_t len = strlen(src) + 1;
    *dest = m->s.malloc(sizeof(char) * len);
    memcpy(*dest, src, len);
}

void copyData(kdl_machine_t *m, int type, void *data, kdl_data_t *dest) {
    dest->datatype = type;
    switch(type) {
    case KDL_DT_INT:
        dest->data = (kdl_int_t *) m->s.malloc(sizeof(kdl_int_t));
        *((kdl_int_t *)dest->data) = *((kdl_int_t *) data);
        break;
    case KDL_DT_PRC: // Fallthrough
    case KDL_DT_FLT:
        dest->data = (kdl_float_t *) m->s.malloc(sizeof(kdl_float_t));
        *((kdl_float_t *)dest->data) = *((kdl_float_t *) data);
        break;
    case KDL_DT_STR:
        copyString(m, (char *) data, (char **) &dest->data);
        break;
    case KDL_DT_NIL: // Fallthrough
    default:
        assert(false);
    }
}

void mkName(kdl_machine_t *m, const char *context, const char *name, char **result) {
    size_t lenC = context ? strlen(context) : 0;
    size_t lenI = context && context[0] ? 1 : 0;
    size_t lenN = strlen(name);
    size_t size = lenC + lenI + lenN + 1;
    char *lookup = (char *) m->s.malloc(sizeof(char) * size);
    memcpy(lookup, context, sizeof(char) * lenC);
    memcpy(lookup + lenC, " ", sizeof(char) * lenI);
    memcpy(lookup + lenC + lenI, name, sizeof(char) * lenN);
    lookup[lenC + lenI + lenN] = '\0';
    *result = lookup;
}

kdl_entry_t *mkBlankVar(kdl_machine_t *m, const char *fullName) {
    kdl_entry_t *val = (kdl_entry_t *) m->s.malloc(sizeof(kdl_entry_t));
    size_t size = strlen(fullName) + 1;
    val->name = (char *) m->s.malloc(sizeof(char) * size);
    memcpy(val->name, fullName, size);
    val->watcher = NULL;
    val->data.datatype = KDL_DT_NIL;
    val->data.data = NULL;
    return val;
}

bool getVarRef(kdl_machine_t *m, const char *fullName, kdl_entry_t **out) {
    kdl_hashmap_result_t r;
    kdl_hashmap_search(&m->vars, fullName, &r);
    if (r.code != KDL_HASHMAP_EOK) {
        return false;
    }
    kdl_hashmap_get(&m->vars, r, (void **) &out);
    return true;
}

bool varExist(kdl_machine_t *m, const char *fullName) {
    kdl_hashmap_result_t r;
    kdl_hashmap_search(&m->vars, fullName, &r);
    return r.code == KDL_HASHMAP_EOK;
}

void setVar(kdl_machine_t *m, const char *fullName, int type, void *data) {
    kdl_entry_t *ptr;
    if (getVarRef(m, fullName, &ptr)) {
        freeData(m->s, &ptr->data);
    } else {
        ptr = mkBlankVar(m, fullName);
        kdl_hashmap_insert(&m->vars, fullName, ptr);
    }
    copyData(m, type, data, &ptr->data);
    if (ptr->watcher) {
        ptr->watcher(m, fullName, &ptr->data);
    }
}

bool getVarByName(kdl_machine_t *m, const char *fullName, kdl_data_t *out) {
    kdl_entry_t *data;
    getVarRef(m, fullName, &data);
    copyData(m, data->data.datatype, data->data.data, out);
    return true;
}

bool getVar(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *out) {
    char *lookup = NULL;
    mkName(m, context, name, &lookup);
    bool result = getVarByName(m, lookup, out);
    m->s.free(lookup);
    return result;
}

// Does not do any copy
bool getVerb(kdl_machine_t *m, const char *name, kdl_verb_t **out) {
    kdl_hashmap_result_t r;
    kdl_hashmap_search(&m->verbs, name, &r);
    if (r.code != KDL_HASHMAP_EOK) {
        return false;
    }
    kdl_hashmap_get(&m->verbs, r, (void **) out);
    return true;
}

void setVerb(kdl_machine_t *m, const char *name, kdl_verb_t verb) {
    kdl_verb_t *ptr;
    if (getVerb(m, name, &ptr)) {
        *ptr = verb;
    } else {
        ptr = (kdl_verb_t *) m->s.malloc(sizeof(kdl_verb_t));
        *ptr = verb;
        kdl_hashmap_insert(&m->verbs, name, ptr);
    }
}

void doCompute(kdl_machine_t *m, kdl_compute_t *c, kdl_data_t *result) {
    kdl_data_t *stack = m->s.malloc(sizeof(kdl_data_t) * c->length);
    size_t stackLen = 0;

    for (size_t i = 0; i < c->length; i++) {
        kdl_op_t *op = c->opers + i;
        kdl_data_t e;
        e.data = NULL;
        e.datatype = KDL_DT_NIL;
        switch(op->op) {
        case KDL_OP_NOOP:
            // Do nothing
            break;
        case KDL_OP_PINT:
            e.datatype = KDL_DT_INT;
            copyData(m, KDL_DT_INT, op->value, &e);
            break;
        case KDL_OP_PFLOAT:
            e.datatype = KDL_DT_FLT;
            copyData(m, KDL_DT_FLT, op->value, &e);
            break;
        case KDL_OP_PSTR:
            e.datatype = KDL_DT_STR;
            copyData(m, KDL_DT_STR, op->value, &e);
            break;
        case KDL_OP_PPERC:
            e.datatype = KDL_DT_PRC;
            copyData(m, KDL_DT_PRC, op->value, &e);
            break;
        case KDL_OP_PVAR:
            if (!getVar(m, op->context, (char *) op->value, &e)) {
                printf("Error: variable not found\n"); // tmp
                // Error: variable not found
            }
            break;
        case KDL_OP_ADD:
            assert(stackLen >= 2);
            kdl_data_t *da = &stack[stackLen - 2];
            kdl_data_t *db = &stack[stackLen - 1];
            assert(da->datatype == KDL_DT_FLT || da->datatype == KDL_DT_INT);
            assert(db->datatype == KDL_DT_FLT || db->datatype == KDL_DT_INT);
            if (da->datatype == KDL_DT_FLT || db->datatype == KDL_DT_FLT) {
                e.datatype = KDL_DT_FLT;
                kdl_float_t a = (kdl_float_t)(da->datatype == KDL_DT_FLT ? *((kdl_float_t *)da->data) : *((kdl_int_t *)db->data));
                kdl_float_t b = (kdl_float_t)(db->datatype == KDL_DT_FLT ? *((kdl_float_t *)db->data) : *((kdl_int_t *)db->data));
                kdl_float_t r = a + b;
                freeData(m->s, da);
                freeData(m->s, db);
                stackLen -= 2;
                e.data = (kdl_float_t *) m->s.malloc(sizeof(kdl_float_t));
                *((kdl_float_t *)e.data) = r;
            } else {
                e.datatype = KDL_DT_INT;
                kdl_int_t a = *((kdl_int_t *)da->data);
                kdl_int_t b = *((kdl_int_t *)db->data);
                kdl_int_t r = a + b;
                freeData(m->s, da);
                freeData(m->s, db);
                stackLen -= 2;
                e.data = (kdl_int_t *) m->s.malloc(sizeof(kdl_int_t));
                *((kdl_int_t *)e.data) = r;
            }
            break;
        case KDL_OP_SUB:
            assert(false);
            break;
        case KDL_OP_DIV:
            assert(false);
            break;
        case KDL_OP_MUL:
            assert(false);
            break;
        case KDL_OP_EQU:
            assert(false);
            break;
        case KDL_OP_LEQ:
            assert(false);
            break;
        case KDL_OP_GEQ:
            assert(false);
            break;
        case KDL_OP_LTH:
            assert(false);
            break;
        case KDL_OP_GTH:
            assert(false);
            break;
        case KDL_OP_AND:
            assert(false);
            break;
        case KDL_OP_OR:
            assert(false);
            break;
        case KDL_OP_NOT:
            assert(false);
            break;
        default:
            assert(false);
        }
        assert(e.datatype != KDL_DT_NIL);
        stack[stackLen++] = e;
    }
    assert(stackLen == 1); // Tmp; error check
    *result = stack[0];
    m->s.free(stack);
}

void doExecute(kdl_machine_t *m, kdl_execute_t *c) {
    kdl_verb_t *verb;
    if (!getVerb(m, c->order.verb, &verb)) {
        assert(false); // Error: verb not found
    }
    if (c->order.nParams != verb->datatypesLen) {
        assert(false); // Error: invalid number of parameters
    }
    kdl_data_t *params = (kdl_data_t *) m->s.malloc(sizeof(kdl_data_t) * c->order.nParams);
    size_t paramsLen = 0;
    for (size_t i = 0; i < c->order.nParams; i++) {
        kdl_data_t result;
        doCompute(m, &c->order.params[i], &result);
        if (result.datatype != verb->datatypes[i]) {
            // TODO: handle correctly
            assert(false); // Error: datatype mismatch
        }
        params[paramsLen++] = result;
    }

    verb->func(m, c->order.context, c->order.verb, params, paramsLen);

    for (size_t i = 0; i < paramsLen; i++) {
        freeData(m->s, &params[i]);
    }
    m->s.free(params);

    // Now add child elements to program

    appendProgram(m, &c->child, &m->pbuf[m->back]);
}

void kdl_machine_setInt(kdl_machine_t *m, const char *name, kdl_int_t value) {
    setVar(m, name, KDL_DT_INT, &value);
}

void kdl_machine_setString(kdl_machine_t *m, const char *name, const char *value) {
    setVar(m, name, KDL_DT_STR, &value);
}

void kdl_machine_setFloat(kdl_machine_t *m, const char *name, kdl_float_t value) {
    setVar(m, name, KDL_DT_FLT, &value);
}

int kdl_machine_getInt(kdl_machine_t *m, const char *name, kdl_int_t *value) {
    kdl_entry_t *e;
    if (getVarRef(m, name, &e)) {
        if (e->data.datatype == KDL_DT_INT) {
            *value = *((kdl_int_t *)e->data.data);
            return KDL_ERR_OK;
        } else {
            return KDL_ERR_TYP;
        }
    } else {
        return KDL_ERR_NTF;
    }
}

int kdl_machine_getString(kdl_machine_t *m, const char *name, const char **value) {
    kdl_entry_t *e;
    if (getVarRef(m, name, &e)) {
        if (e->data.datatype == KDL_DT_STR) {
            *value = (char *) e->data.data;
            return KDL_ERR_OK;
        } else {
            return KDL_ERR_TYP;
        }
    } else {
        return KDL_ERR_NTF;
    }
}

int kdl_machine_getFloat(kdl_machine_t *m, const char *name, kdl_float_t *value) {
    kdl_entry_t *e;
    if (getVarRef(m, name, &e)) {
        if (e->data.datatype == KDL_DT_FLT) {
            *value = *((kdl_float_t *)e->data.data);
            return KDL_ERR_OK;
        } else {
            return KDL_ERR_TYP;
        }
    } else {
        return KDL_ERR_NTF;
    }
}



int kdl_machine_addWatcher(kdl_machine_t *m, const char *target, kdl_watcher_t callback) {
    kdl_entry_t *e;
    if (getVarRef(m, target, &e)) {
        e->watcher = callback;
        return KDL_ERR_OK;
    } else {
        return KDL_ERR_NTF;
    }
}


void kdl_machine_addVerb(kdl_machine_t *m, const char *target, kdl_verb_t v) {
    setVerb(m, target, v);
}


void kdl_mkMachine(kdl_machine_t *out) {
    kdl_machine_t m;
    memset(&m, 0, sizeof(kdl_machine_t));

    m.s.malloc = defMalloc;
    m.s.realloc = defRealloc;
    m.s.free = defFree;

    memset(&m.start, 0, sizeof(kdl_program_t));
    memset(m.pbuf, 0, sizeof(m.pbuf));

    m.front = 0;
    m.back = 1;

    kdl_hashmap_init(m.s, &m.verbs, 4, freeVerb_fwd);
    kdl_hashmap_init(m.s, &m.vars, 4, freeEntry_fwd);

    *out = m;
}


void rewindToStart(kdl_machine_t *m) {
    appendProgram(m, &m->start, &m->pbuf[m->front]);
    appendProgram(m, &m->start, &m->pbuf[m->back]);
}

kdl_error_t kdl_machine_load(kdl_machine_t *m, const char *input) {
    kdl_program_t p;
    kdl_error_t e = kdl_parse(m->s, input, &p);
    if (e.code != KDL_ERR_OK) {
        return e;
    }
    m->start = p;
    rewindToStart(m);
    return e;
}

void kdl_machine_run(kdl_machine_t *m) {
    kdl_programBuffer_t *front = &m->pbuf[m->front];
    for (size_t i = 0; i < front->length; i++) {
        kdl_rule_t *r = &front->rules[i];
        bool run = false;
        if (r->compute.length > 0) {
            kdl_data_t result;
            doCompute(m, &r->compute, &result);
            if (result.datatype != KDL_DT_INT) {
                assert(false); // Error: conditional expression did not return int
            }
            run = *((kdl_int_t*)result.data) == 0 ? false : true;
        } else {
            run = true;
        }
        if (run) {
            doExecute(m, &r->execute);
        }
    }

    size_t tmp = m->front;
    m->front = m->back;
    m->back = tmp;

    m->pbuf[m->back].length = 0;
    appendRules(m, m->pbuf[m->front].rules, m->pbuf[m->front].length, &m->pbuf[m->back]);
}

void kdl_machine_free(kdl_machine_t *machine) {
    kdl_freeProgram(machine->s, &machine->start);
    machine->s.free(machine->pbuf[0].rules);
    machine->s.free(machine->pbuf[1].rules);
    kdl_hashmap_free(&machine->vars);
    kdl_hashmap_free(&machine->verbs);
    memset(machine, 0, sizeof(kdl_machine_t));
}
