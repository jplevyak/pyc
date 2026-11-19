/*
  issues/195: `typing` and `dataclasses`, lowered on the AST before
  build_syms (as the ctypes lowering is, issues/188).

  typing. pyc never evaluates an annotation, so pyc_lib/typing.py only binds
  names. Two statements CPython never runs, or runs only to make a value
  nothing reads, are removed here:

    if TYPE_CHECKING: ...     the body (an else stays). CPython never runs
                              it; pyc resolves imports at compile time, so
                              an import in it would otherwise be loaded.
    X: TypeAlias = <type>     a type expression (`int | str`, `tuple[int,
                              str]`) that pyc cannot type and the program
                              only names in annotations.

  dataclasses. A module-level `@dataclass` class becomes the class CPython's
  decorator builds:

    - each field default is evaluated ONCE, in the class body, into a module
      global the class body declares (`global __pyc_dc_K_f__`), so the
      class attribute and the `__init__` default are one object, as in
      CPython, and class-scope names stay visible to the expression;
    - `field(default_factory=F)`: F is stored the same way, and the
      `__init__` default is flagged (PyDAST::is_default_factory) so the
      default wrapper CALLS it for every call that omits the argument
      (PycCompiler::default_wrapper);
    - `__init__`, `__repr__`, `__eq__` and `__hash__` as CPython generates
      them, each only if the class body does not define it;
    - `__eq__` compares the fields only when the two objects' classes are
      the SAME class: every dataclass answers `__pyc_dc_tag__()` with its
      own number, which is why a subclass of a dataclass must itself be
      one (checked after build_syms, pyc_dataclass_check);
    - frozen: a store to a field is the setter `__pyc_set_FIELD__`, which
      raises FrozenInstanceError; `__init__`'s own stores are marked
      is_raw_field, as CPython's go through object.__setattr__.

  Names are resolved through the module's imports (`from dataclasses import
  dataclass`, `import dataclasses as dc`), never by their spelling alone.
  What CPython supports and this does not is a compile error naming it.
*/
#include "python_ifa_int.h"
#include "python_parse.h"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

std::string fmt(const char *f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char *f, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return buf;
}

std::string trimmed(cchar *s) {
  std::string r = s ? s : "";
  size_t b = r.find_first_not_of(" \t\r\n"), e = r.find_last_not_of(" \t\r\n");
  return b == std::string::npos ? "" : r.substr(b, e - b + 1);
}

// The names a module binds to `typing` / `dataclasses` and their members.
struct Binds {
  std::set<std::string> mods;                // `import M` / `import M as m`
  std::map<std::string, std::string> names;  // `from M import X as y`: y -> X
  bool star = false;                         // `from M import *`
};

struct Env {
  cchar *filename = nullptr;
  Binds typing, dc;
};

[[noreturn]] void err(PyDAST *at, const char *f, ...) __attribute__((format(printf, 2, 3)));
void err(PyDAST *at, const char *f, ...) {
  char buf[2048];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  fail("error %s:%d: %s", at && at->filename ? at->filename : "?", at ? at->line : 0, buf);
}

void scan_import(Binds &b, cchar *modname, PyDAST *n) {
  if (n->kind == PY_import_name) {
    Vec<PyDAST *> items;
    for (PyDAST *c : n->children) {
      if (c->kind == PY_testlist)
        for (PyDAST *d : c->children) items.add(d);
      else
        items.add(c);
    }
    for (PyDAST *d : items) {
      if (d->kind != PY_dotted_as_name || !d->children.n) continue;
      if (trimmed(d->children[0]->str_val) != modname) continue;
      b.mods.insert(d->children.n > 1 && d->children[1]->str_val ? d->children[1]->str_val : modname);
    }
    return;
  }
  if (n->kind != PY_import_from || !n->children.n || trimmed(n->children[0]->str_val) != modname) return;
  if (n->children.n < 2) {
    b.star = true;
    return;
  }
  Vec<PyDAST *> names;
  for (int i = 1; i < n->children.n; i++) {
    if (n->children[i]->kind == PY_import_as_name)
      names.add(n->children[i]);
    else
      for (PyDAST *c : n->children[i]->children) names.add(c);
  }
  for (PyDAST *c : names) {
    if (c->kind != PY_import_as_name || !c->children.n || !c->children[0]->str_val) continue;
    std::string name = c->children[0]->str_val;
    b.names[c->children.n > 1 && c->children[1]->str_val ? c->children[1]->str_val : name] = name;
  }
}

// The member of M a node names (`X` bound by `from M import X`, or `m.X`),
// or "".
std::string member_of(Binds &b, PyDAST *n) {
  if (!n) return "";
  if (n->kind == PY_name && n->str_val) {
    auto it = b.names.find(n->str_val);
    return it != b.names.end() ? it->second : "";
  }
  if (n->kind == PY_power && n->children.n >= 2 && n->children[0]->kind == PY_name &&
      b.mods.count(n->children[0]->str_val ? n->children[0]->str_val : "") && n->children[1]->kind == PY_attribute)
    return n->children[1]->children[0]->str_val;
  return "";
}

// The same for a dotted name written as text (a decorator, a string
// annotation's head).
std::string member_of_text(Binds &b, std::string s) {
  size_t dot = s.rfind('.');
  if (dot == std::string::npos) {
    auto it = b.names.find(s);
    return it != b.names.end() ? it->second : "";
  }
  return b.mods.count(s.substr(0, dot)) ? s.substr(dot + 1) : "";
}

// An annotation's head as typing names it: `ClassVar[int]`, `t.ClassVar`,
// "ClassVar[int]".
std::string annotation_member(Binds &b, PyDAST *ann) {
  if (!ann) return "";
  if (ann->kind == PY_string && ann->str_val) {
    std::string s = trimmed(ann->str_val);
    size_t q = s.find_first_of("'\"");
    if (q == std::string::npos) return "";
    s = s.substr(q);
    while (s.size() && (s[0] == '\'' || s[0] == '"')) s.erase(0, 1);
    size_t e = s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.");
    return member_of_text(b, trimmed(s.substr(0, e).c_str()));
  }
  if (ann->kind == PY_power && ann->children.n >= 2 && ann->children.last()->kind == PY_subscript) {
    // Drop the subscript: `ClassVar[int]` -> `ClassVar`.
    PyDAST head = *ann;
    head.children.clear();
    for (int i = 0; i < ann->children.n - 1; i++) head.children.add(ann->children[i]);
    if (head.children.n == 1) return member_of(b, head.children[0]);
    return member_of(b, &head);
  }
  return member_of(b, ann);
}

PyDAST *new_node(PyASTKind kind, PyDAST *like) {
  PyDAST *n = new PyDAST();
  n->kind = kind;
  n->filename = like->filename;
  n->line = like->line;
  return n;
}

// ---------------------------------------------------------------- typing

// `if TYPE_CHECKING:` loses its body; `X: TypeAlias = ...` is removed.
void lower_typing(Env &env, PyDAST *n) {
  if (!n) return;
  if (n->kind == PY_if_stmt && n->children.n >= 2 && member_of(env.typing, n->children[0]) == "TYPE_CHECKING") {
    PyDAST *suite = new_node(PY_suite, n->children[1]);
    suite->add(new_node(PY_pass_stmt, n->children[1]));
    n->children.v[1] = suite;
    suite->parent = n;
  }
  Vec<PyDAST *> keep;
  bool dropped = false;
  for (PyDAST *c : n->children) {
    if (c && c->kind == PY_annassign && c->children.n == 3 && annotation_member(env.typing, c->children[1]) == "TypeAlias") {
      dropped = true;
      continue;
    }
    keep.add(c);
  }
  if (dropped) {
    n->children.clear();
    for (PyDAST *c : keep) n->children.add(c);
    if (!n->children.n && (n->kind == PY_suite || n->kind == PY_module)) n->add(new_node(PY_pass_stmt, n));
  }
  for (PyDAST *c : n->children) lower_typing(env, c);
}

// ----------------------------------------------------------- dataclasses

struct Field {
  std::string name;
  bool has_default = false, factory = false;
  bool init = true, repr = true, compare = true;
  std::string global;  // the module global holding the default / factory
};

struct DC {
  int id;
  std::string name;
  PyDAST *cdef;
  DC *base = nullptr;  // its dataclass base in this module, if any
  bool init = true, repr = true, eq = true, frozen = false;
  std::vector<Field> fields;  // in __init__ order, inherited first
  std::set<std::string> defines;  // methods the class body defines
  bool post_init = false;         // it or a dataclass base defines __post_init__
};

std::vector<DC *> all_dcs;  // every dataclass in the program, for pyc_dataclass_check
int next_id = 1;

// The decorators of a PY_decorated: a PY_suite of them, or one.
Vec<PyDAST *> decorators_of(PyDAST *decorated) {
  Vec<PyDAST *> out;
  PyDAST *d = decorated->children[0];
  if (d->kind == PY_suite)
    for (PyDAST *x : d->children) out.add(x);
  else
    out.add(d);
  return out;
}

bool literal_bool(PyDAST *v, bool &out) {
  if (v && v->kind == PY_name && v->str_val) {
    if (!strcmp(v->str_val, "True")) return out = true, true;
    if (!strcmp(v->str_val, "False")) return out = false, true;
  }
  return false;
}

// `@dataclass` / `@dataclass(...)` / `@dataclasses.dataclass`: its argument
// list (null when bare) if this decorator is one.
bool is_dataclass_decorator(Env &env, PyDAST *dec, PyDAST *&args) {
  if (dec->kind != PY_decorator || !dec->children.n) return false;
  if (member_of_text(env.dc, trimmed(dec->children[0]->str_val)) != "dataclass") return false;
  args = dec->children.n > 1 ? dec->children[1] : nullptr;
  return true;
}

void keyword_args(PyDAST *arglist, PyDAST *at, const char *what, std::vector<std::pair<std::string, PyDAST *>> &out) {
  if (!arglist) return;
  Vec<PyDAST *> items;
  if (arglist->kind == PY_arglist)
    for (PyDAST *c : arglist->children) items.add(c);
  else
    items.add(arglist);
  for (PyDAST *a : items) {
    if (a->kind != PY_keyword_arg || a->children.n != 2 || !a->children[0]->str_val)
      err(at, "%s takes keyword arguments only here", what);
    out.push_back({a->children[0]->str_val, a->children[1]});
  }
}

void options(DC *d, PyDAST *args, PyDAST *at) {
  std::vector<std::pair<std::string, PyDAST *>> kw;
  keyword_args(args, at, "@dataclass", kw);
  for (auto &[k, v] : kw) {
    bool b;
    bool lit = literal_bool(v, b);
    if (k == "init" || k == "repr" || k == "eq" || k == "frozen" || k == "match_args") {
      if (!lit) err(at, "@dataclass(%s=...) must be True or False", k.c_str());
      if (k == "init") d->init = b;
      if (k == "repr") d->repr = b;
      if (k == "eq") d->eq = b;
      if (k == "frozen") d->frozen = b;
    } else if (k == "order" || k == "unsafe_hash" || k == "kw_only" || k == "slots" || k == "weakref_slot") {
      if (!lit) err(at, "@dataclass(%s=...) must be True or False", k.c_str());
      if (b) err(at, "@dataclass(%s=True) is not supported (issues/195)", k.c_str());
    } else
      err(at, "@dataclass has no parameter '%s'", k.c_str());
  }
}

PyDAST *&class_body(PyDAST *cdef) {
  PyDAST *&body = cdef->children.v[cdef->children.n - 1];
  if (body->kind != PY_suite) {  // `class C: STMT`
    PyDAST *s = new_node(PY_suite, body);
    s->add(body);
    s->parent = cdef;
    body = s;
  }
  return body;
}

bool mutable_literal(PyDAST *v) {
  return v && (v->kind == PY_list || v->kind == PY_dict || v->kind == PY_set || v->kind == PY_listcomp);
}

void set_origin(PyDAST *n, cchar *filename, int line) {
  if (!n) return;
  n->filename = filename;
  n->line = line;
  n->col = 0;
  for (PyDAST *c : n->children) set_origin(c, filename, line);
}

// Parse generated statements as a class body, placeholders `__pyc_dc_ph_N__`
// replaced by the user's expressions, which keep their own origin.
Vec<PyDAST *> *parse_body(const std::string &stmts, PyDAST *at, std::vector<PyDAST *> &ph) {
  std::string text = "class __pyc_dc_tmp__:\n" + stmts;
  if (getenv("PYC_DBG_DATACLASS")) fprintf(stderr, "--- dataclass: generated for %s:%d\n%s", at->filename, at->line, text.c_str());
  PyDAST *g = dparse_python_buf_to_ast("<dataclass>", text.c_str(), (int)text.size());
  if (!g || !g->children.n || g->children[0]->kind != PY_classdef)
    fail("internal error: the dataclass lowering generated code that does not parse (%s:%d)", at->filename, at->line);
  set_origin(g, at->filename, at->line);
  std::function<void(PyDAST *)> fill = [&](PyDAST *n) {
    for (int i = 0; i < n->children.n; i++) {
      PyDAST *c = n->children[i];
      int k;
      if (c && c->kind == PY_name && c->str_val && sscanf(c->str_val, "__pyc_dc_ph_%d__", &k) == 1) {
        n->children.v[i] = ph[k];
        ph[k]->parent = n;
      } else if (c)
        fill(c);
    }
  };
  fill(g);
  PyDAST *body = g->children[0]->children.last();
  Vec<PyDAST *> *out = new Vec<PyDAST *>;
  if (body->kind == PY_suite)
    for (PyDAST *c : body->children) out->add(c);
  else
    out->add(body);
  return out;
}

std::string ph_name(std::vector<PyDAST *> &ph, PyDAST *e) {
  ph.push_back(e);
  return fmt("__pyc_dc_ph_%d__", (int)ph.size() - 1);
}

Field *find_field(DC *d, const std::string &name) {
  for (Field &f : d->fields)
    if (f.name == name) return &f;
  return nullptr;
}

// One `name: T [= value]` of a dataclass body: the field it declares (if
// any) and the statements that replace it.
void lower_field(Env &env, DC *d, PyDAST *st, std::string &out, std::vector<PyDAST *> &ph, bool &keep) {
  keep = true;
  if (st->children[0]->kind != PY_name) return;  // `self.x: int = 0` is not a field
  std::string name = st->children[0]->str_val;
  std::string tm = annotation_member(env.typing, st->children[1]);
  if (tm == "ClassVar") return;  // a class variable, not a field
  std::string dm = annotation_member(env.dc, st->children[1]);
  if (dm == "InitVar" || dm == "KW_ONLY") err(st, "dataclasses.%s is not supported (issues/195)", dm.c_str());
  Field nf;
  nf.name = name;
  nf.global = fmt("__pyc_dc_%d_%s__", d->id, name.c_str());
  PyDAST *value = st->children.n == 3 ? st->children[2] : nullptr;
  PyDAST *dflt = nullptr, *factory = nullptr;
  // `field(...)`: PY_power(field, PY_call(arglist?)).
  if (value && value->kind == PY_power && value->children.n >= 2 && value->children.last()->kind == PY_call) {
    PyDAST head = *value;
    head.children.clear();
    for (int i = 0; i < value->children.n - 1; i++) head.children.add(value->children[i]);
    PyDAST *callee = head.children.n == 1 ? head.children[0] : &head;
    if (member_of(env.dc, callee) == "field") {
      PyDAST *call = value->children.last();
      std::vector<std::pair<std::string, PyDAST *>> kw;
      keyword_args(call->children.n ? call->children[0] : nullptr, st, "field()", kw);
      for (auto &[k, v] : kw) {
        bool b;
        if (k == "default")
          dflt = v;
        else if (k == "default_factory")
          factory = v;
        else if (k == "init" || k == "repr" || k == "compare") {
          if (!literal_bool(v, b)) err(st, "field(%s=...) must be True or False", k.c_str());
          (k == "init" ? nf.init : k == "repr" ? nf.repr : nf.compare) = b;
        } else if (k == "hash") {
          if (!(v->kind == PY_name && v->str_val && !strcmp(v->str_val, "None")))
            err(st, "field(hash=...) other than None is not supported (issues/195)");
        } else if (k == "metadata") {
          // Evaluated by CPython, read by nothing pyc supports (fields()).
        } else if (k == "kw_only")
          err(st, "field(kw_only=...) is not supported (issues/195)");
        else
          err(st, "field() has no parameter '%s'", k.c_str());
      }
      if (dflt && factory) err(st, "cannot specify both default and default_factory");
      keep = false;
    } else
      dflt = value;
  } else
    dflt = value;
  if (dflt && mutable_literal(dflt))
    err(st, "mutable default for field %s is not allowed: use default_factory", name.c_str());
  if (dflt) {
    // The class attribute and the __init__ default: one evaluation, here.
    out += fmt("    global %s\n    %s = %s\n    %s = %s\n", nf.global.c_str(), nf.global.c_str(), ph_name(ph, dflt).c_str(),
               name.c_str(), nf.global.c_str());
    nf.has_default = true;
    keep = false;
  } else if (factory) {
    out += fmt("    global %s\n    %s = %s\n", nf.global.c_str(), nf.global.c_str(), ph_name(ph, factory).c_str());
    nf.has_default = nf.factory = true;
  }
  // A redeclared inherited field keeps its place and takes the new default.
  if (Field *old = find_field(d, name))
    *old = nf;
  else
    d->fields.push_back(nf);
}

std::string generate_methods(DC *d) {
  std::string cname = d->name, s;
  auto defines = [&](const char *m) { return d->defines.count(m) > 0; };
  s += fmt("    def __pyc_dc_tag__(self):\n        return %d\n", d->id);
  if (d->init && !defines("__init__")) {
    std::string params = "self", body;
    bool seen_default = false;
    for (Field &f : d->fields) {
      if (f.init) {
        if (f.has_default)
          params += ", " + f.name + "=" + f.global, seen_default = true;
        else if (seen_default)
          err(d->cdef, "dataclass %s: non-default argument '%s' follows default argument", cname.c_str(), f.name.c_str());
        else
          params += ", " + f.name;
        body += fmt("        self.%s = %s\n", f.name.c_str(), f.name.c_str());
      } else if (f.factory)
        body += fmt("        self.%s = %s()\n", f.name.c_str(), f.global.c_str());
      else if (f.has_default)
        body += fmt("        self.%s = %s\n", f.name.c_str(), f.global.c_str());
    }
    if (d->post_init) body += "        self.__post_init__()\n";
    if (body.empty()) body = "        pass\n";
    s += "    def __init__(" + params + "):\n" + body;
  }
  if (d->repr && !defines("__repr__")) {
    std::string e = "\"" + cname + "(";
    bool first = true;
    for (Field &f : d->fields) {
      if (!f.repr) continue;
      e += fmt("%s%s=\" + repr(self.%s) + \"", first ? "" : ", ", f.name.c_str(), f.name.c_str());
      first = false;
    }
    e += ")\"";
    s += "    def __repr__(self):\n        return " + e + "\n";
  }
  std::string mine = "(", theirs = "(";
  for (Field &f : d->fields)
    if (f.compare) mine += "self." + f.name + ", ", theirs += "other." + f.name + ", ";
  mine += ")", theirs += ")";
  if (d->eq && !defines("__eq__"))
    s += fmt("    def __eq__(self, other):\n"
             "        if isinstance(other, %s) and other.__pyc_dc_tag__() == self.__pyc_dc_tag__():\n"
             "            return %s == %s\n"
             "        return False\n",
             cname.c_str(), mine.c_str(), theirs.c_str());
  if (d->eq && !defines("__hash__")) {
    if (d->frozen)
      s += "    def __hash__(self):\n        return hash(" + mine + ")\n";
    else
      s += fmt("    def __hash__(self):\n        raise TypeError(\"unhashable type: '%s'\")\n", cname.c_str());
  }
  if (d->frozen)
    for (Field &f : d->fields) {
      s += fmt("    def __pyc_set_%s__(self, v):\n"
               "        raise __pyc_dc_FrozenInstanceError__(\"cannot assign to field '%s'\")\n",
               f.name.c_str(), f.name.c_str());
      pyc_add_setter_name(f.name.c_str());
    }
  return s;
}

void lower_dataclass(Env &env, PyDAST *decorated, PyDAST *cdef, PyDAST *args, std::map<std::string, DC *> &by_name,
                     bool &need_frozen_import) {
  DC *d = new DC;
  d->id = next_id++;
  d->name = cdef->children[0]->str_val;
  d->cdef = cdef;
  options(d, args, decorated);
  // Its dataclass base, if any, is one in this module.
  for (int i = 1; i < cdef->children.n - 1; i++) {
    Vec<PyDAST *> bases;
    if (cdef->children[i]->kind == PY_tuple)
      for (PyDAST *b : cdef->children[i]->children) bases.add(b);
    else
      bases.add(cdef->children[i]);
    for (PyDAST *b : bases)
      if (b->kind == PY_name && by_name.count(b->str_val)) {
        if (d->base) err(cdef, "dataclass %s: more than one dataclass base is not supported (issues/195)", d->name.c_str());
        d->base = by_name[b->str_val];
      }
  }
  if (d->base) {
    if (d->base->frozen != d->frozen)
      err(cdef, "cannot inherit %s dataclass from a %s one", d->frozen ? "frozen" : "non-frozen",
          d->frozen ? "non-frozen" : "frozen");
    d->fields = d->base->fields;
    d->post_init = d->base->post_init;
  }
  PyDAST *&body = class_body(cdef);
  for (PyDAST *st : body->children)
    if (st->kind == PY_funcdef && st->children.n && st->children[0]->str_val) {
      d->defines.insert(st->children[0]->str_val);
      if (!strcmp(st->children[0]->str_val, "__post_init__")) d->post_init = true;
    }
  // The fields, in order; each is replaced where it stands.
  Vec<PyDAST *> nbody;
  for (PyDAST *st : body->children) {
    if (st->kind != PY_annassign) {
      nbody.add(st);
      continue;
    }
    std::string gen;
    std::vector<PyDAST *> ph;
    bool keep;
    lower_field(env, d, st, gen, ph, keep);
    if (keep) nbody.add(st);
    if (!gen.empty())
      for (PyDAST *c : *parse_body(gen, st, ph)) nbody.add(c);
  }
  std::vector<PyDAST *> none;
  Vec<PyDAST *> *methods = parse_body(generate_methods(d), cdef, none);
  for (PyDAST *m : *methods) {
    if (m->kind != PY_funcdef || !m->children[0]->str_val) {
      nbody.add(m);
      continue;
    }
    cchar *mn = m->children[0]->str_val;
    if (!strcmp(mn, "__init__")) {
      // The factory defaults are called per call (default_wrapper); a frozen
      // class's own stores are the field itself.
      PyDAST *params = m->children[1];
      if (params->children.n)
        for (PyDAST *p : params->children[0]->children)
          if (p->kind == PY_arg_default && p->children[1]->kind == PY_name)
            for (Field &f : d->fields)
              if (f.factory && f.global == p->children[1]->str_val) p->children[1]->is_default_factory = 1;
      if (d->frozen) {
        std::function<void(PyDAST *)> raw = [&](PyDAST *n) {
          if (n->kind == PY_assign && n->children[0]->kind == PY_power) n->children[0]->is_raw_field = 1;
          for (PyDAST *c : n->children) raw(c);
        };
        raw(m);
      }
    }
    if (!strncmp(mn, "__pyc_set_", 10)) need_frozen_import = true;
    nbody.add(m);
  }
  body->children.clear();
  for (PyDAST *c : nbody) body->add(c);
  // The decorator is applied.
  Vec<PyDAST *> rest;
  for (PyDAST *x : decorators_of(decorated)) {
    PyDAST *a;
    if (!is_dataclass_decorator(env, x, a)) rest.add(x);
  }
  PyDAST *decs = new_node(PY_suite, decorated);
  for (PyDAST *x : rest) decs->add(x);
  decorated->children.v[0] = decs;
  decs->parent = decorated;
  by_name[d->name] = d;
  all_dcs.push_back(d);
}

// What the lowering did not consume is a use of `dataclasses` it does not
// support (fields(), asdict(), a dataclass that is not at module level).
void check_unsupported(Env &env, PyDAST *n) {
  if (!n || n->kind == PY_import_from || n->kind == PY_import_name) return;
  PyDAST *args;
  if (is_dataclass_decorator(env, n, args))
    err(n, "@dataclass is supported on a class at module level only (issues/195)");
  std::string m = member_of(env.dc, n);
  if (!m.empty() && m != "FrozenInstanceError") {
    err(n, "dataclasses.%s is not supported (issues/195)", m.c_str());
  }
  for (PyDAST *c : n->children) check_unsupported(env, c);
}

void lower_module(PyDAST *mod, cchar *filename) {
  if (!mod || mod->kind != PY_module) return;
  Env env;
  env.filename = filename;
  for (PyDAST *st : mod->children)
    if (st->kind == PY_import_name || st->kind == PY_import_from) {
      scan_import(env.typing, "typing", st);
      scan_import(env.dc, "dataclasses", st);
    }
  if (env.typing.star || env.dc.star)
    for (PyDAST *st : mod->children)
      if (st->kind == PY_import_from && st->children.n < 2) {
        cchar *m = trimmed(st->children[0]->str_val) == "typing" ? "typing" : "dataclasses";
        err(st, "`from %s import *` is not supported; import the names (issues/195)", m);
      }
  if (env.typing.mods.size() || env.typing.names.size()) lower_typing(env, mod);
  if (!env.dc.mods.size() && !env.dc.names.size()) return;
  std::map<std::string, DC *> by_name;
  bool need_frozen_import = false;
  for (int i = 0; i < mod->children.n; i++) {
    PyDAST *st = mod->children[i];
    if (st->kind != PY_decorated || st->children.last()->kind != PY_classdef) continue;
    PyDAST *args = nullptr;
    bool is_dc = false;
    for (PyDAST *x : decorators_of(st))
      if (is_dataclass_decorator(env, x, args)) is_dc = true;
    if (!is_dc) continue;
    PyDAST *cdef = st->children.last();
    lower_dataclass(env, st, cdef, args, by_name, need_frozen_import);
    if (!st->children[0]->children.n) {  // no decorator left: the class itself
      mod->children.v[i] = cdef;
      cdef->parent = mod;
    }
  }
  check_unsupported(env, mod);
  if (need_frozen_import) {
    std::string text = "from dataclasses import FrozenInstanceError as __pyc_dc_FrozenInstanceError__\n";
    PyDAST *g = dparse_python_buf_to_ast("<dataclass>", text.c_str(), (int)text.size());
    set_origin(g, filename, 1);
    Vec<PyDAST *> all;
    all.add(g->children[0]);
    for (PyDAST *c : mod->children) all.add(c);
    mod->children.clear();
    for (PyDAST *c : all) mod->add(c);
  }
}

}  // namespace

void lower_typing_and_dataclasses(Vec<PycModule *> &mods, Vec<PyDAST *> &imported) {
  for (int i = 1; i < mods.n; i++) lower_module(mods[i]->pymod, mods[i]->filename);
  for (PyDAST *a : imported) lower_module(a, a && a->filename ? a->filename : "<imported module>");
}

// After build_syms: the class hierarchy the lowering could not see.
//  - A subclass of a dataclass must be one itself. One that is not would
//    inherit __repr__ (printing the base's name) and __eq__ (comparing the
//    base's tag), where CPython uses the instance's own class.
//  - A dataclass's dataclass base must be the one it was lowered with (a
//    base in another module was not: its fields are not in __init__).
//  - A __post_init__ inherited from a class that is not a dataclass must be
//    called, and was not.
void pyc_dataclass_check(PycCompiler &ctx) {
  std::map<Sym *, DC *> dcs;
  for (DC *d : all_dcs)
    if (Sym *s = getAST(d->cdef, ctx)->sym) dcs[s] = d;
  for (auto &[s, d] : dcs) {
    for (Sym *sub : s->specializers)
      if (sub && sub != s && !dcs.count(sub) && sub->type_kind == Type_RECORD)
        fail("error %s:%d: class %s is a subclass of dataclass %s and must be a dataclass itself (issues/195)",
             d->cdef->filename, d->cdef->line, sub->name ? sub->name : "?", d->name.c_str());
    for (Sym *b : s->implements)
      if (b && dcs.count(b) && dcs[b] != d->base)
        fail("error %s:%d: dataclass %s: a dataclass base in another module is not supported (issues/195)",
             d->cdef->filename, d->cdef->line, d->name.c_str());
    if (!d->post_init) {
      Vec<Sym *> todo, seen;
      for (Sym *b : s->implements) todo.add(b);
      while (todo.n) {
        Sym *b = todo.pop();
        if (!b || seen.set_in(b)) continue;
        seen.set_add(b);
        for (Sym *m : b->has)
          if (m && m->name && !strcmp(m->name, "__post_init__"))
            fail("error %s:%d: dataclass %s inherits __post_init__ from a class that is not a dataclass, which is not "
                 "supported (issues/195)",
                 d->cdef->filename, d->cdef->line, d->name.c_str());
        for (Sym *x : b->implements) todo.add(x);
      }
    }
  }
}
