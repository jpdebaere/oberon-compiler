typedef enum {
  DECL_UNKNOWN,
  DECL_INCOMPLETE,
  DECL_IMPORT,
  DECL_CONST,
  DECL_TYPE,
  DECL_VAR,
  DECL_PARAM,
  DECL_VARPARAM,
  DECL_PROC,
} DeclKind;

const char *decl_kind_names[] = {
    [DECL_UNKNOWN] = "<unknown>", [DECL_INCOMPLETE] = "<incomplete>",
    [DECL_IMPORT] = "IMPORT",     [DECL_CONST] = "CONST",
    [DECL_TYPE] = "TYPE",         [DECL_VAR] = "VAR",
    [DECL_VARPARAM] = "VARPARAM", [DECL_PROC] = "PROC",
};

typedef struct Decl {
  DeclKind kind;
  const char *name;
  union {
    struct Decl *imported_decls;  // Only for DECL_IMPORT
    Type *type;                   // for everything else
  };
  bool is_exported;
} Decl;

typedef struct Module {
  const char *name;
  Decl *decls;  // buf
} Module;

#define SCOPE_SIZE 512
typedef struct Scope {
  Decl decls[SCOPE_SIZE];
  // Number of valid decls
  size_t size;
  struct Scope *parent;
} Scope;

// Typically, the current scope is stack allocated. E.g, when a procedure is
// defined, the function parsing the procedure allocates a new scope on the
// stack, and when it leaves, sets the global scope back. Thus, It is not safe
// to save Decl objects, but Types are persisted.
Scope *current_scope = NULL;

void enter_scope(Scope *scope) {
  scope->parent = current_scope;
  current_scope = scope;
  current_scope->size = 0;
}

void exit_scope(void) { current_scope = current_scope->parent; }

Decl *lookup_decl(const char *name) {
  for (Scope *s = current_scope; s != NULL; s = s->parent) {
    for (size_t i = 0; i < s->size; i++) {
      if (s->decls[i].name == name) {
        return &s->decls[i];
      }
    }
  }
  return NULL;
}

Decl *lookup_module_import(const char *moduleName, const char *name) {
  printf("Looking up %s.%s\n", moduleName, name);
  // TODO
  return NULL;
}

Decl *internal_add_decl(const char *name) {
  assert(name);
  assert(current_scope);
  assert(current_scope->size < SCOPE_SIZE);
  for (size_t i = 0; i < current_scope->size; i++) {
    if (current_scope->decls[i].name == name) {
      if (current_scope->decls[i].kind == DECL_INCOMPLETE) {
        return &current_scope->decls[i];
      } else {
        error("%s redefined", name);
      }
    }
  }
  Decl *d = &current_scope->decls[current_scope->size++];
  d->name = name;
  return d;
}

void add_import_decl(const char *name, Decl *decls) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_IMPORT;
  d->imported_decls = decls;
  d->is_exported = false;
}

Decl *add_incomplete_decl(const char *name) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_INCOMPLETE;
  d->type = make_incomplete_type();
  d->is_exported = false;
  return d;
}

void add_const_decl(const char *name, Type *type, bool is_exported) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_CONST;
  d->type = type;
  d->is_exported = is_exported;
}

void add_type_decl(const char *name, Type *type, bool is_exported) {
  Decl *d = internal_add_decl(name);
  if (d->kind == DECL_INCOMPLETE) {
    assert(d->type);
    assert(d->type->kind == TYPE_INCOMPLETE);
    *d->type = *type;
  } else {
    d->type = type;
  }
  d->kind = DECL_TYPE;
  d->is_exported = is_exported;
}

void add_var_decl(const char *name, Type *type, bool is_exported) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_VAR;
  d->type = type;
  d->is_exported = is_exported;
}

void add_param_decl(const char *name, Type *type) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_PARAM;
  d->type = type;
  d->is_exported = false;
}

void add_varparam_decl(const char *name, Type *type) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_VARPARAM;
  d->type = type;
  d->is_exported = false;
}

void add_proc_decl(const char *name, Type *type, bool is_exported) {
  Decl *d = internal_add_decl(name);
  d->kind = DECL_PROC;
  d->type = type;
  d->is_exported = is_exported;
}

Module *make_module(const char *name, Scope *s) {
  Module *m = xmalloc(sizeof(Module));
  m->name = name;
  m->decls = NULL;
  for (size_t i = 0; i < s->size; i++) {
    buf_push(m->decls, s->decls[i]);
  }
  return m;
}

void init_global_types() {
  assert(booleanType.kind == TYPE_BOOLEAN);
  add_type_decl(string_intern("BOOLEAN"), &booleanType, true);
  add_type_decl(string_intern("BYTE"), &byteType, true);
  add_type_decl(string_intern("CHAR"), &charType, true);
  add_type_decl(string_intern("INTEGER"), &integerType, true);
  add_type_decl(string_intern("REAL"), &realType, true);
  add_type_decl(string_intern("SET"), &setType, true);
}

void ast_test(void) {
  Scope outer;
  Scope inner;

  enter_scope(&outer);
  assert(current_scope == &outer);
  add_var_decl(string_intern("outer1"), &integerType, false);
  add_var_decl(string_intern("outer2"), &integerType, true);
  assert(current_scope->size == 2);
  Decl *o1 = lookup_decl(string_intern("outer1"));
  assert(o1->kind == DECL_VAR);
  assert(o1->name == string_intern("outer1"));
  assert(o1->type == &integerType);
  assert(o1->is_exported == false);
  Decl *o2 = lookup_decl(string_intern("outer2"));
  assert(o2->kind == DECL_VAR);
  assert(o2->name == string_intern("outer2"));
  assert(o2->type == &integerType);
  assert(o2->is_exported == true);

  enter_scope(&inner);
  assert(current_scope == &inner);
  assert(current_scope->parent == &outer);
  add_var_decl(string_intern("inner1"), &realType, true);
  Decl *i1 = lookup_decl(string_intern("inner1"));
  assert(i1->kind == DECL_VAR);
  assert(i1->name == string_intern("inner1"));
  assert(i1->type == &realType);
  assert(i1->is_exported == true);
  Decl *oRef = lookup_decl(string_intern("outer1"));
  assert(oRef == o1);
  exit_scope();
  assert(current_scope == &outer);
  exit_scope();
  assert(current_scope == NULL);
}
