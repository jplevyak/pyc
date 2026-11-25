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
#include <algorithm>
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
  Binds typing, dc, en, ft, op;
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

// What the whole program asks of its dataclasses (issues/216), found before
// any module is lowered, since a dataclass in one module may be converted or
// replaced in another. Each generated method exists only when used.
struct Uses {
  bool fields = false, astuple = false, asdict = false;
  std::vector<std::vector<std::string>> replace_sets;  // sorted keyword names, one method each
  int replace_id(const std::vector<std::string> &names) {
    for (size_t i = 0; i < replace_sets.size(); i++)
      if (replace_sets[i] == names) return (int)i;
    replace_sets.push_back(names);
    return (int)replace_sets.size() - 1;
  }
} uses;

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

// Each `__pyc_dc_ph_N__` name under g becomes the user's expression ph[N],
// which keeps its own origin.
void fill_placeholders(PyDAST *g, std::vector<PyDAST *> &ph) {
  for (int i = 0; i < g->children.n; i++) {
    PyDAST *c = g->children[i];
    int k;
    if (c && c->kind == PY_name && c->str_val && sscanf(c->str_val, "__pyc_dc_ph_%d__", &k) == 1) {
      g->children.v[i] = ph[k];
      ph[k]->parent = g;
    } else if (c)
      fill_placeholders(c, ph);
  }
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
  fill_placeholders(g, ph);
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
  if (uses.fields)
    s += fmt("    @classmethod\n    def __pyc_dc_fields__(cls):\n        return __pyc_dc_%d_fields__\n", d->id);
  auto per_field = [&](const char *open, const char *each, const char *close) {
    std::string e = open;
    for (Field &f : d->fields) e += fmt(each, f.name.c_str(), f.name.c_str());
    return e + close;
  };
  if (uses.astuple)
    s += "    def __pyc_dc_astuple__(self):\n        return " +
         per_field("(", "__pyc_dc_astuple_value__(self.%.0s%s), ", ")") + "\n";
  if (uses.asdict)
    s += "    def __pyc_dc_asdict__(self):\n        return " +
         per_field("{", "\"%s\": __pyc_dc_asdict_value__(self.%s), ", "}") + "\n";
  // replace(obj, **changes): CPython's _replace, for each set of keywords
  // the program passes. A name that is not an init field raises what
  // CPython's does.
  for (size_t id = 0; id < uses.replace_sets.size(); id++) {
    std::vector<std::string> &names = uses.replace_sets[id];
    std::string params = "self", err_stmt, args;
    for (const std::string &n : names) {
      params += ", " + n;
      Field *f = find_field(d, n);
      if (err_stmt.empty() && f && !f->init)
        err_stmt = fmt("        raise ValueError(\"field %s is declared with init=False, it cannot be specified with "
                       "replace()\")\n",
                       n.c_str());
      if (err_stmt.empty() && !f)
        err_stmt = fmt("        raise TypeError(\"%s.__init__() got an unexpected keyword argument '%s'\")\n",
                       cname.c_str(), n.c_str());
    }
    for (Field &f : d->fields) {
      if (!f.init) continue;
      bool changed = std::find(names.begin(), names.end(), f.name) != names.end();
      args += fmt("%s%s=%s%s", args.empty() ? "" : ", ", f.name.c_str(), changed ? "" : "self.", f.name.c_str());
    }
    s += fmt("    def __pyc_dc_replace_%d__(%s):\n", (int)id, params.c_str());
    s += err_stmt.empty() ? fmt("        return %s(%s)\n", cname.c_str(), args.c_str()) : err_stmt;
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
      for (PyDAST *c : *parse_body(gen, st, ph)) {
        // The class attribute `NAME = default` is a store to the class, not
        // through a frozen field's raising setter (__pyc_set_NAME__): that
        // raised at class creation, left pending to the next check
        // (issues/216).
        if (c->kind == PY_assign && c->children[0]->kind == PY_name) c->children[0]->is_raw_field = 1;
        nbody.add(c);
      }
  }
  if (uses.fields) {
    // One tuple of Field per class, made with it (CPython keeps them in
    // __dataclass_fields__, so fields(x) returns the same objects each time).
    std::string g = fmt("__pyc_dc_%d_fields__", d->id), t = "(";
    for (Field &f : d->fields)
      t += fmt("__pyc_dc_Field__(\"%s\", %s, %s, %s), ", f.name.c_str(), f.init ? "True" : "False",
               f.repr ? "True" : "False", f.compare ? "True" : "False");
    std::vector<PyDAST *> none;
    for (PyDAST *c : *parse_body(fmt("    global %s\n    %s = %s)\n", g.c_str(), g.c_str(), t.c_str()), cdef, none))
      nbody.add(c);
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
  // Last (it moves the body in cdef->children, which `body` refers into).
  if ((uses.astuple || uses.asdict) && !d->base) {
    // The marker the conversion's isinstance tests (pyc_lib/dataclasses.py),
    // last, so the class's own bases keep their order.
    PyDAST *mark = new_node(PY_name, cdef);
    mark->str_val = strdup("__pyc_dc_base__");
    PyDAST *suite = cdef->children.pop();
    cdef->children.add(mark);
    cdef->children.add(suite);
    mark->parent = cdef;
  }
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

// ------------------------------------------------------------------ enum
//
// issues/208. A module-level class whose base is an enum class -- Enum or
// ReprEnum named through the module's imports of `enum`, or an enum class
// defined earlier in the same module -- becomes the class CPython's EnumType
// builds. IntEnum and StrEnum are refused (their members are ints / strs). pyc_lib/enum.py has the rest, and says what it
// is.
//
// A body statement `NAME = value` (or `NAME: T = value`) is a MEMBER, by
// CPython's rule (enum._EnumDict): NAME is not a _sunder_, __dunder__ or
// __private name, and the value is not a function (a lambda; a def is not an
// assignment). The value is evaluated where it stands, in the class body,
// into a module global (as a dataclass default is), and the statement is
// removed. After the class, in definition order:
//
//     Color.__pyc_members__ = []; Color.__members__ = {}
//     Color.__pyc_enum_name__ = "Color"
//     G = <auto() value>                      # for each auto() member
//     M = __pyc_new_instance__(Color)         # not Color(...): that looks up
//     M._name_ = "RED"; M._value_ = G
//     M.__init__(*G)                          # only if the class has __init__
//     Color.RED = __pyc_enum__.__pyc_enum_add__(Color, M)
//     __pyc_enum__.__pyc_enum_unique__(Color) # for @unique

std::vector<PyDAST *> all_enums;  // every lowered enum classdef, for pyc_enum_check

struct EnumClass {
  std::string name;
  bool has_members = false;
  bool init = false;      // defines __init__ (or inherits it from an enum in this module)
  bool gen_next = false;  // defines _generate_next_value_ (or inherits one)
};

// The enum library's roots, by the name the module imports them under.
bool enum_root(Env &env, PyDAST *b, std::string &root) {
  root = member_of(env.en, b);
  return root == "Enum" || root == "IntEnum" || root == "StrEnum" || root == "ReprEnum";
}

// The bases of a classdef.
Vec<PyDAST *> class_bases(PyDAST *cdef) {
  Vec<PyDAST *> bases;
  for (int i = 1; i < cdef->children.n - 1; i++) {
    if (cdef->children[i]->kind == PY_tuple || cdef->children[i]->kind == PY_testlist)
      for (PyDAST *b : cdef->children[i]->children) bases.add(b);
    else
      bases.add(cdef->children[i]);
  }
  return bases;
}

// Is this classdef an enum class? Its enum base in this module, if any.
bool is_enum_class(Env &env, PyDAST *cdef, std::map<std::string, EnumClass *> &by_name, EnumClass *&base,
                   std::string &root) {
  bool is = false;
  base = nullptr;
  root.clear();
  for (PyDAST *b : class_bases(cdef)) {
    std::string r;
    if (enum_root(env, b, r)) {
      // An IntEnum / StrEnum member IS an int / a str; pyc has no subclass
      // of int or str that carries attributes, so it cannot make one.
      if (r == "IntEnum" || r == "StrEnum")
        err(cdef, "enum.%s is not supported: its members are %s instances with a name, and pyc has no %s subclass that "
                  "carries attributes (issues/208)",
            r.c_str(), r == "IntEnum" ? "int" : "str", r == "IntEnum" ? "int" : "str");
      is = true;
      root = r;
    } else if (member_of(env.en, b) == "Flag" || member_of(env.en, b) == "IntFlag")
      err(cdef, "enum.%s is not supported (issues/208)", member_of(env.en, b).c_str());
    else if (b->kind == PY_name && b->str_val && by_name.count(b->str_val)) {
      is = true;
      base = by_name[b->str_val];
    }
  }
  return is;
}

bool dunder_or_sunder(const std::string &n) {
  size_t l = n.size();
  if (l > 4 && n.compare(0, 2, "__") == 0 && n.compare(l - 2, 2, "__") == 0) return true;  // __dunder__
  if (l > 2 && n[0] == '_' && n[l - 1] == '_' && n[1] != '_' && n[l - 2] != '_') return true;  // _sunder_
  return false;
}

// `auto()`, through the module's imports of enum.
bool is_auto_call(Env &env, PyDAST *v) {
  if (!v || v->kind != PY_power || v->children.n != 2 || v->children[1]->kind != PY_call) return false;
  if (member_of(env.en, v->children[0]) != "auto") return false;
  if (v->children[1]->children.n) err(v, "auto() takes no arguments");
  return true;
}

// The length of a tuple display, or -1.
int tuple_literal(PyDAST *v) {
  if (v && (v->kind == PY_tuple || (v->kind == PY_testlist && v->children.n > 1))) return v->children.n;
  return -1;
}

void lower_enum(Env &env, PyDAST *cdef, Vec<PyDAST *> &decorators, std::map<std::string, EnumClass *> &by_name,
                EnumClass *base, const std::string &root, std::string &after) {
  static int next_enum = 1;
  int k = next_enum++;
  EnumClass *e = new EnumClass;
  e->name = cdef->children[0]->str_val;
  if (base) {
    if (base->has_members)
      err(cdef, "%s: cannot extend enum %s, which has members", e->name.c_str(), base->name.c_str());
    e->init = base->init;
    e->gen_next = base->gen_next;
  }
  bool unique = false;
  for (PyDAST *d : decorators) {
    if (d->kind == PY_decorator && d->children.n == 1 && member_of_text(env.en, trimmed(d->children[0]->str_val)) == "unique")
      unique = true;
    else
      err(cdef, "a decorator on enum class %s other than @unique is not supported (issues/208)", e->name.c_str());
  }
  PyDAST *&body = class_body(cdef);
  struct Member {
    std::string name, global;
    bool is_auto;
    int tuple;  // the value is a tuple display of this length, else -1
  };
  std::vector<Member> members;
  Vec<PyDAST *> nbody;
  for (PyDAST *st : body->children) {
    PyDAST *d = st->kind == PY_decorated ? st->children.last() : st;
    if (d->kind == PY_funcdef && d->children[0]->str_val) {
      std::string fname = d->children[0]->str_val;
      if (fname == "__init__") e->init = true;
      if (fname == "_generate_next_value_") e->gen_next = true;
      if (fname == "__new__") err(d, "%s: __new__ in an enum class is not supported (issues/208)", e->name.c_str());
    }
    PyDAST *target = nullptr, *value = nullptr;
    if (st->kind == PY_assign && st->children.n == 2) target = st->children[0], value = st->children[1];
    if (st->kind == PY_annassign && st->children.n == 3) target = st->children[0], value = st->children[2];
    if (!target || target->kind != PY_name || !target->str_val) {
      if (target && st->kind == PY_assign)
        err(st, "%s: only `NAME = value` defines an enum member here (issues/208)", e->name.c_str());
      nbody.add(st);
      continue;
    }
    std::string name = target->str_val;
    if (name == "_ignore_") err(st, "%s: _ignore_ is not supported (issues/208)", e->name.c_str());
    if (name == "_order_") continue;  // CPython only checks it against the definition order
    if (dunder_or_sunder(name) || name.compare(0, 2, "__") == 0 || value->kind == PY_lambda) {
      nbody.add(st);
      continue;
    }
    for (Member &m : members)
      if (m.name == name) err(st, "%s: '%s' already defined as %s", e->name.c_str(), name.c_str(), name.c_str());
    Member m{name, fmt("__pyc_en_%d_%s__", k, name.c_str()), is_auto_call(env, value), tuple_literal(value)};
    if (!m.is_auto) {
      std::vector<PyDAST *> ph;
      std::string gen = fmt("    global %s\n    %s = %s\n", m.global.c_str(), m.global.c_str(), ph_name(ph, value).c_str());
      for (PyDAST *c : *parse_body(gen, st, ph)) nbody.add(c);
    }
    members.push_back(m);
  }
  if (!nbody.n) nbody.add(new_node(PY_pass_stmt, body));
  body->children.clear();
  for (PyDAST *c : nbody) body->add(c);
  e->has_members = !members.empty();
  by_name[e->name] = e;
  all_enums.push_back(cdef);
  if (!e->has_members) {
    if (unique) after += fmt("__pyc_enum__.__pyc_enum_unique__(%s)\n", e->name.c_str());
    return;
  }
  cchar *c = e->name.c_str();
  after += fmt("%s.__pyc_members__ = []\n%s.__members__ = {}\n%s.__pyc_enum_name__ = \"%s\"\n", c, c, c, c);
  for (size_t i = 0; i < members.size(); i++) {
    Member &m = members[i];
    if (m.is_auto) {
      std::string last = "[";
      for (size_t j = 0; j < i; j++) last += (j ? ", " : "") + members[j].global;
      last += "]";
      if (e->gen_next)
        after += fmt("%s = %s._generate_next_value_(\"%s\", 1, %d, %s)\n", m.global.c_str(), c, m.name.c_str(), (int)i,
                     last.c_str());
      else if (!i)
        after += fmt("%s = 1\n", m.global.c_str());
      else
        after += fmt("%s = __pyc_enum__.__pyc_enum_next__(%s)\n", m.global.c_str(), last.c_str());
    }
    std::string mv = fmt("__pyc_en_%d_m_%s__", k, m.name.c_str());
    after += fmt("%s = __pyc_new_instance__(%s)\n%s._name_ = \"%s\"\n%s._value_ = %s\n", mv.c_str(), c, mv.c_str(),
                 m.name.c_str(), mv.c_str(), m.global.c_str());
    // CPython calls __init__(*value) for a tuple value, else
    // __init__(value). A tuple display's elements are passed by index:
    // its length is known here.
    if (e->init) {
      std::string args = m.global;
      if (m.tuple >= 0) {
        args.clear();
        for (int j = 0; j < m.tuple; j++) args += fmt("%s%s[%d]", j ? ", " : "", m.global.c_str(), j);
      }
      after += fmt("%s.__init__(%s)\n", mv.c_str(), args.c_str());
    }
    after += fmt("%s.%s = __pyc_enum__.__pyc_enum_add__(%s, %s)\n", c, m.name.c_str(), c, mv.c_str());
  }
  if (unique) after += fmt("__pyc_enum__.__pyc_enum_unique__(%s)\n", c);
}

// Every module-level enum class, lowered in place; the statements that make
// its members follow it.
void lower_enums(Env &env, PyDAST *mod) {
  std::map<std::string, EnumClass *> by_name;
  Vec<PyDAST *> out;
  bool any = false;
  for (PyDAST *st : mod->children) {
    PyDAST *cdef = st->kind == PY_decorated ? st->children.last() : st;
    EnumClass *base;
    std::string root;
    if (cdef->kind != PY_classdef || !is_enum_class(env, cdef, by_name, base, root)) {
      out.add(st);
      continue;
    }
    any = true;
    Vec<PyDAST *> decs;
    if (st->kind == PY_decorated)
      for (PyDAST *d : decorators_of(st)) decs.add(d);
    std::string after;
    lower_enum(env, cdef, decs, by_name, base, root, after);
    out.add(cdef);  // its decorators are consumed
    cdef->parent = mod;
    if (!after.empty()) {
      PyDAST *g = dparse_python_buf_to_ast("<enum>", after.c_str(), (int)after.size());
      if (getenv("PYC_DBG_ENUM")) fprintf(stderr, "--- enum: generated after %s:%d\n%s", cdef->filename, cdef->line, after.c_str());
      if (!g) fail("internal error: the enum lowering generated code that does not parse (%s:%d)", cdef->filename, cdef->line);
      set_origin(g, cdef->filename, cdef->line);
      for (PyDAST *c : g->children) out.add(c);
    }
  }
  if (!any) return;
  std::string imp = "import enum as __pyc_enum__\n";
  PyDAST *g = dparse_python_buf_to_ast("<enum>", imp.c_str(), (int)imp.size());
  set_origin(g, env.filename, 1);
  mod->children.clear();
  mod->add(g->children[0]);
  for (PyDAST *c : out) mod->add(c);
}

// An enum class the lowering did not see (not at module level).
void check_enum_unsupported(Env &env, PyDAST *n, bool top) {
  if (!n) return;
  if (!top && n->kind == PY_classdef)
    for (PyDAST *b : class_bases(n)) {
      std::string r;
      if (enum_root(env, b, r)) err(n, "an enum class is supported at module level only (issues/208)");
    }
  if (n->kind == PY_power && n->children.n >= 2 && n->children[1]->kind == PY_call) {
    std::string r;
    if (enum_root(env, n->children[0], r))
      err(n, "the functional API enum.%s(...) is not supported (issues/208)", r.c_str());
  }
  for (PyDAST *c : n->children) check_enum_unsupported(env, c, n->kind == PY_module || (top && n->kind == PY_decorated));
}

// `replace(obj, ...)` / `dataclasses.replace(obj, ...)` at the head of a
// PY_power: the number of children the call takes (2 or 3), or 0.
int replace_head(Env &env, PyDAST *n) {
  if (n->kind != PY_power || n->children.n < 2) return 0;
  if (n->children[1]->kind == PY_call && member_of(env.dc, n->children[0]) == "replace") return 2;
  if (n->children.n >= 3 && n->children[2]->kind == PY_call && member_of(env.dc, n) == "replace") return 3;
  return 0;
}

Vec<PyDAST *> call_args(PyDAST *call) {
  Vec<PyDAST *> items;
  if (!call->children.n) return items;
  PyDAST *al = call->children[0];
  if (al->kind == PY_arglist)
    for (PyDAST *c : al->children) items.add(c);
  else
    items.add(al);
  return items;
}

// The keyword names of a replace() call, sorted: which generated method it
// calls (issues/216).
std::vector<std::string> replace_names(PyDAST *n, PyDAST *call) {
  Vec<PyDAST *> items = call_args(call);
  if (!items.n || items[0]->kind == PY_keyword_arg || items[0]->kind == PY_star_arg || items[0]->kind == PY_dstar_arg)
    err(n, "dataclasses.replace() takes the object as its one positional argument here (issues/216)");
  std::vector<std::string> names;
  for (int i = 1; i < items.n; i++) {
    if (items[i]->kind != PY_keyword_arg || !items[i]->children[0]->str_val)
      err(n, "dataclasses.replace() takes keyword arguments after the object; '*' and '**' are not supported (issues/216)");
    names.push_back(items[i]->children[0]->str_val);
  }
  std::sort(names.begin(), names.end());
  return names;
}

// replace(obj, k=v, ...)  ->  obj.__pyc_dc_replace_N__(k=v, ...), in place.
// The keywords stay keywords, so they are evaluated in source order.
void lower_replace_call(Env &env, PyDAST *n, int head) {
  PyDAST *call = n->children[head - 1];
  Vec<PyDAST *> items = call_args(call);
  int id = uses.replace_id(replace_names(n, call));
  std::vector<PyDAST *> ph;
  std::string text = fmt("__pyc_dc_e__ = __pyc_dc_obj__.__pyc_dc_replace_%d__(", id);
  for (int i = 1; i < items.n; i++)
    text += fmt("%s%s=%s", i > 1 ? ", " : "", items[i]->children[0]->str_val, ph_name(ph, items[i]->children[1]).c_str());
  text += ")\n";
  PyDAST *g = dparse_python_buf_to_ast("<dataclass>", text.c_str(), (int)text.size());
  if (!g || !g->children.n) fail("internal error: the replace() lowering does not parse (%s:%d)", n->filename, n->line);
  set_origin(g, n->filename, n->line);
  fill_placeholders(g, ph);
  PyDAST *expr = g->children[0]->children.last();  // PY_power(__pyc_dc_obj__, .m, (...))
  PyDAST *obj = items[0];
  Vec<PyDAST *> out;
  if (obj->kind == PY_power)
    for (PyDAST *c : obj->children) out.add(c);
  else
    out.add(obj);
  for (int i = 1; i < expr->children.n; i++) out.add(expr->children[i]);
  for (int i = head; i < n->children.n; i++) out.add(n->children[i]);
  n->children.clear();
  for (PyDAST *c : out) {
    n->children.add(c);
    c->parent = n;
  }
}

// What the lowering did not consume is a use of `dataclasses` it does not
// support (make_dataclass, a dataclass that is not at module level); a
// replace() call is lowered here.
void check_unsupported(Env &env, PyDAST *n) {
  if (!n || n->kind == PY_import_from || n->kind == PY_import_name) return;
  PyDAST *args;
  if (is_dataclass_decorator(env, n, args))
    err(n, "@dataclass is supported on a class at module level only (issues/195)");
  if (int head = replace_head(env, n)) lower_replace_call(env, n, head);
  std::string m = member_of(env.dc, n);
  if (m == "replace") err(n, "dataclasses.replace is supported as a call only (issues/216)");
  if (!m.empty() && m != "FrozenInstanceError" && m != "Field" && m != "fields" && m != "astuple" && m != "asdict")
    err(n, "dataclasses.%s is not supported (issues/195)", m.c_str());
  for (PyDAST *c : n->children) check_unsupported(env, c);
}

// The program-wide scan behind `uses`: every name of fields/astuple/asdict,
// and the keyword set of every replace() call.
void scan_uses(Env &env, PyDAST *n) {
  if (!n || n->kind == PY_import_from || n->kind == PY_import_name) return;
  if (int head = replace_head(env, n)) uses.replace_id(replace_names(n, n->children[head - 1]));
  std::string m = member_of(env.dc, n);
  if (m == "fields") uses.fields = true;
  if (m == "astuple") uses.astuple = true;
  if (m == "asdict") uses.asdict = true;
  for (PyDAST *c : n->children) scan_uses(env, c);
}

// ------------------------------------------------------- total_ordering
//
// A module-level class decorated `@total_ordering` (functools, named
// through the module's imports) gets the comparison methods CPython's
// decorator adds: of `__lt__ __le__ __gt__ __ge__`, the ROOT is the greatest
// name the class defines or inherits (max() over the strings, so __lt__
// first), and each of the others it does not have is CPython's formula over
// the root (functools._convert). CPython's `if op_result is NotImplemented`
// step is left out: pyc has no NotImplemented value, so a root cannot
// return one. Inherited operations come from base classes defined earlier in
// the same module; a base pyc cannot see here (imported) is refused, since
// it may define one. Any other use of the name is refused.

const char *const ordering_ops[] = {"__lt__", "__le__", "__gt__", "__ge__"};

// CPython's functools._convert: {root: {op: body over `r` and `self == other`}}.
const char *ordering_formula(const std::string &root, const std::string &op) {
  static const std::map<std::pair<std::string, std::string>, const char *> f = {
      {{"__lt__", "__gt__"}, "not r and self != other"}, {{"__lt__", "__le__"}, "r or self == other"},
      {{"__lt__", "__ge__"}, "not r"},                   {{"__le__", "__ge__"}, "not r or self == other"},
      {{"__le__", "__lt__"}, "r and self != other"},     {{"__le__", "__gt__"}, "not r"},
      {{"__gt__", "__lt__"}, "not r and self != other"}, {{"__gt__", "__ge__"}, "r or self == other"},
      {{"__gt__", "__le__"}, "not r"},                   {{"__ge__", "__le__"}, "not r or self == other"},
      {{"__ge__", "__gt__"}, "r and self != other"},     {{"__ge__", "__lt__"}, "not r"},
  };
  return f.at({root, op});
}

bool is_total_ordering_decorator(Env &env, PyDAST *dec) {
  return dec->kind == PY_decorator && dec->children.n == 1 && dec->children[0]->str_val &&
         member_of_text(env.ft, trimmed(dec->children[0]->str_val)) == "total_ordering";
}

// The ordering operations each module-level class has, by class name, own
// and inherited (nullopt-like: `unknown` when a base is not visible).
struct Ordering {
  std::set<std::string> ops;
  bool unknown_base = false;
};

void lower_total_ordering(Env &env, PyDAST *mod) {
  std::map<std::string, Ordering> classes;
  for (int i = 0; i < mod->children.n; i++) {
    PyDAST *st = mod->children[i];
    PyDAST *cdef = st->kind == PY_decorated ? st->children.last() : st;
    if (cdef->kind != PY_classdef || !cdef->children.n || !cdef->children[0]->str_val) continue;
    Ordering o;
    for (int b = 1; b < cdef->children.n - 1; b++) {
      Vec<PyDAST *> bases;
      if (cdef->children[b]->kind == PY_tuple)
        for (PyDAST *x : cdef->children[b]->children) bases.add(x);
      else
        bases.add(cdef->children[b]);
      for (PyDAST *x : bases) {
        if (x->kind == PY_name && x->str_val && !strcmp(x->str_val, "object")) continue;
        auto it = x->kind == PY_name && x->str_val ? classes.find(x->str_val) : classes.end();
        if (it == classes.end()) {
          o.unknown_base = true;
          continue;
        }
        o.ops.insert(it->second.ops.begin(), it->second.ops.end());
        o.unknown_base |= it->second.unknown_base;
      }
    }
    PyDAST *&body = class_body(cdef);
    for (PyDAST *m : body->children) {
      PyDAST *fd = m->kind == PY_decorated ? m->children.last() : m;
      if (fd->kind == PY_funcdef && fd->children.n && fd->children[0]->str_val)
        for (const char *op : ordering_ops)
          if (!strcmp(fd->children[0]->str_val, op)) o.ops.insert(op);
    }
    std::string name = cdef->children[0]->str_val;
    bool decorated = false;
    if (st->kind == PY_decorated) {
      Vec<PyDAST *> rest;
      for (PyDAST *x : decorators_of(st)) {
        if (!is_total_ordering_decorator(env, x)) {
          rest.add(x);
          continue;
        }
        // CPython applies decorators bottom-up: one below this was applied
        // first, and pyc lowers only @dataclass, already done.
        decorated = true;
      }
      if (decorated) {
        if (o.ops.empty())
          err(cdef, "must define at least one ordering operation: < > <= >= (class %s, @total_ordering)", name.c_str());
        if (o.unknown_base)
          err(cdef,
              "@total_ordering on %s: a base class defined outside this module may define an ordering operation, "
              "which pyc cannot see here",
              name.c_str());
        std::string root = *std::max_element(o.ops.begin(), o.ops.end());
        std::string gen;
        for (const char *op : ordering_ops)
          if (!o.ops.count(op))
            gen += fmt("  def %s(self, other):\n    r = self.%s(other)\n    return %s\n", op, root.c_str(),
                       ordering_formula(root, op));
        std::vector<PyDAST *> none;
        if (!gen.empty())
          for (PyDAST *c : *parse_body(gen, cdef, none)) body->add(c);
        for (const char *op : ordering_ops) o.ops.insert(op);
        PyDAST *decs = new_node(PY_suite, st);
        for (PyDAST *x : rest) decs->add(x);
        st->children.v[0] = decs;
        decs->parent = st;
        if (!rest.n) {
          mod->children.v[i] = cdef;
          cdef->parent = mod;
        }
      }
    }
    classes[name] = o;
  }
  // Every other use of the name: a call, a nested class, a decorator on a
  // function.
  std::function<void(PyDAST *)> check = [&](PyDAST *n) {
    if (!n || n->kind == PY_import_name || n->kind == PY_import_from) return;
    if ((n->kind == PY_decorator && is_total_ordering_decorator(env, n)) || member_of(env.ft, n) == "total_ordering")
      err(n, "functools.total_ordering is supported as the decorator of a module-level class only");
    // partial(f, ..., k=v): binding the stored keywords needs `**` into named
    // parameters (pyc_lib/functools.py).
    if (n->kind == PY_power && n->children.n >= 2) {
      PyDAST *call = nullptr;
      if (n->children[0]->kind == PY_name && n->children[1]->kind == PY_call && member_of(env.ft, n->children[0]) == "partial")
        call = n->children[1];
      else if (n->children.n >= 3 && n->children[2]->kind == PY_call && member_of(env.ft, n) == "partial")
        call = n->children[2];
      if (call && call->children.n) {
        PyDAST *al = call->children[0];
        Vec<PyDAST *> items;
        if (al->kind == PY_arglist)
          for (PyDAST *c : al->children) items.add(c);
        else
          items.add(al);
        for (PyDAST *a : items)
          if (a->kind == PY_keyword_arg)
            err(n, "functools.partial with keyword arguments is not supported: binding them needs '**' into named "
                   "parameters (issues/189)");
      }
    }
    for (PyDAST *c : n->children) check(c);
  };
  check(mod);
}

Env module_env(PyDAST *mod, cchar *filename) {
  Env env;
  env.filename = filename;
  for (PyDAST *st : mod->children)
    if (st->kind == PY_import_name || st->kind == PY_import_from) {
      scan_import(env.typing, "typing", st);
      scan_import(env.dc, "dataclasses", st);
      scan_import(env.en, "enum", st);
      scan_import(env.ft, "functools", st);
      scan_import(env.op, "operator", st);
    }
  return env;
}

// ------------------------------------------------------------- operator
//
// issues/216. `operator.attrgetter(...)` and `operator.methodcaller(...)`
// read an attribute named by a string. pyc resolves attributes at compile
// time, so the names must be constants, and then each call site is a class
// of its own, generated at the top of the module:
//
//   attrgetter("a", "b.c")      class __pyc_attrgetter_N__:
//                                 def __call__(self, obj): return (obj.a, obj.b.c)
//                                 def __repr__(self): return "operator.attrgetter('a', 'b.c')"
//   methodcaller("m", e, k=f)   class __pyc_methodcaller_N__:
//                                 def __init__(self, a0, k0): ...
//                                 def __call__(self, obj): return obj.m(self.a0, k=self.k0)
//
// The call becomes `__pyc_attrgetter_N__()` / `__pyc_methodcaller_N__(e, f)`,
// so the arguments are evaluated once, where the caller is made, as in
// CPython. A name that is not a constant is refused.

int op_counter = 0;

// The text of a plain string literal ('a', "a.b"), or false.
bool literal_text(PyDAST *v, std::string &out) {
  if (!v || v->kind != PY_string || !v->str_val) return false;
  std::string s = trimmed(v->str_val);
  if (s.size() < 2 || (s[0] != '\'' && s[0] != '"') || s.back() != s[0]) return false;
  if (s.size() >= 6 && s.compare(0, 3, std::string(3, s[0])) == 0) return false;  // triple-quoted
  out = s.substr(1, s.size() - 2);
  return out.find('\\') == std::string::npos && out.find(s[0]) == std::string::npos;
}

bool identifier(const std::string &s) {
  if (s.empty() || !(isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
  for (char c : s)
    if (!(isalnum((unsigned char)c) || c == '_')) return false;
  return true;
}

// The callee a PY_power's head names in `operator`, and how many children
// the call takes (2: `attrgetter(...)`, 3: `operator.attrgetter(...)`).
std::string op_call(Env &env, PyDAST *n, int &head) {
  head = 0;
  if (n->kind != PY_power || n->children.n < 2) return "";
  if (n->children[1]->kind == PY_call) {
    std::string m = member_of(env.op, n->children[0]);
    if (!m.empty()) head = 2;
    return m;
  }
  if (n->children.n >= 3 && n->children[2]->kind == PY_call) {
    std::string m = member_of(env.op, n);
    if (!m.empty()) head = 3;
    return m;
  }
  return "";
}

// Replace the call at the head of n (its first `head` children) by `expr`,
// a PY_power, keeping any trailers after the call.
void splice_head(PyDAST *n, int head, PyDAST *expr) {
  Vec<PyDAST *> out;
  for (PyDAST *c : expr->children) out.add(c);
  for (int i = head; i < n->children.n; i++) out.add(n->children[i]);
  n->children.clear();
  for (PyDAST *c : out) {
    n->children.add(c);
    c->parent = n;
  }
}

// Parse `__pyc_e__ = EXPR` with placeholders; returns EXPR.
PyDAST *parse_expr(const std::string &expr, PyDAST *at, std::vector<PyDAST *> &ph) {
  std::string text = "__pyc_e__ = " + expr + "\n";
  PyDAST *g = dparse_python_buf_to_ast("<operator>", text.c_str(), (int)text.size());
  if (!g || !g->children.n) fail("internal error: generated code does not parse (%s:%d)", at->filename, at->line);
  set_origin(g, at->filename, at->line);
  fill_placeholders(g, ph);
  return g->children[0]->children.last();
}

void lower_operator(Env &env, PyDAST *mod) {
  std::string classes;
  std::function<void(PyDAST *)> walk = [&](PyDAST *n) {
    if (!n || n->kind == PY_import_name || n->kind == PY_import_from) return;
    int head;
    std::string m = op_call(env, n, head);
    if (head && (m == "attrgetter" || m == "methodcaller")) {
      Vec<PyDAST *> items = call_args(n->children[head - 1]);
      int id = op_counter++;
      if (m == "attrgetter") {
        if (!items.n) err(n, "attrgetter expected 1 argument, got 0");
        std::string body, rep;
        for (PyDAST *a : items) {
          std::string name;
          if (a->kind == PY_keyword_arg || a->kind == PY_star_arg || a->kind == PY_dstar_arg || !literal_text(a, name))
            err(n, "operator.attrgetter takes constant attribute names in pyc, which resolves attributes at compile "
                   "time (issues/216)");
          for (size_t b = 0;;) {
            size_t e = name.find('.', b);
            if (!identifier(name.substr(b, e == std::string::npos ? std::string::npos : e - b)))
              err(n, "operator.attrgetter: '%s' is not an attribute name", name.c_str());
            if (e == std::string::npos) break;
            b = e + 1;
          }
          body += (body.empty() ? "obj." : ", obj.") + name;
          rep += (rep.empty() ? "'" : ", '") + name + "'";
        }
        if (items.n > 1) body = "(" + body + ")";
        classes += fmt("class __pyc_attrgetter_%d__:\n"
                       "    def __call__(self, obj):\n        return %s\n"
                       "    def __repr__(self):\n        return \"operator.attrgetter(%s)\"\n",
                       id, body.c_str(), rep.c_str());
        std::vector<PyDAST *> none;
        splice_head(n, head, parse_expr(fmt("__pyc_attrgetter_%d__()", id), n, none));
      } else {
        std::string name;
        if (!items.n || items[0]->kind != PY_string || !literal_text(items[0], name) || !identifier(name))
          err(n, "operator.methodcaller takes a constant method name in pyc, which resolves attributes at compile "
                 "time (issues/216)");
        std::string params = "self", init, call, rep = "\"operator.methodcaller('" + name + "'\"", ctor;
        std::vector<PyDAST *> ph;
        for (int i = 1; i < items.n; i++) {
          PyDAST *a = items[i];
          if (a->kind == PY_star_arg || a->kind == PY_dstar_arg)
            err(n, "operator.methodcaller with '*' or '**' arguments is not supported (issues/216)");
          std::string slot = fmt("__pyc_mc_%d__", i);
          params += ", " + slot;
          init += fmt("        self.%s = %s\n", slot.c_str(), slot.c_str());
          PyDAST *value = a;
          std::string kw;
          if (a->kind == PY_keyword_arg) {
            kw = a->children[0]->str_val;
            value = a->children[1];
          }
          call += (call.empty() ? "" : ", ") + (kw.empty() ? "" : kw + "=") + "self." + slot;
          rep += " + \", " + (kw.empty() ? "" : kw + "=") + "\" + repr(self." + slot + ")";
          ctor += (ctor.empty() ? "" : ", ") + ph_name(ph, value);
        }
        if (init.empty()) init = "        pass\n";
        classes += fmt("class __pyc_methodcaller_%d__:\n"
                       "    def __init__(%s):\n%s"
                       "    def __call__(self, obj):\n        return obj.%s(%s)\n"
                       "    def __repr__(self):\n        return %s + \")\"\n",
                       id, params.c_str(), init.c_str(), name.c_str(), call.c_str(), rep.c_str());
        splice_head(n, head, parse_expr(fmt("__pyc_methodcaller_%d__(%s)", id, ctor.c_str()), n, ph));
      }
    } else if (member_of(env.op, n) == "attrgetter" || member_of(env.op, n) == "methodcaller")
      err(n, "operator.%s is supported as a call with constant names only (issues/216)", member_of(env.op, n).c_str());
    for (PyDAST *c : n->children) walk(c);
  };
  walk(mod);
  if (classes.empty()) return;
  PyDAST *g = dparse_python_buf_to_ast("<operator>", classes.c_str(), (int)classes.size());
  if (!g) fail("internal error: the operator lowering generated code that does not parse (%s)", env.filename);
  set_origin(g, env.filename, 1);
  Vec<PyDAST *> all;
  for (PyDAST *c : g->children) all.add(c);
  for (PyDAST *c : mod->children) all.add(c);
  mod->children.clear();
  for (PyDAST *c : all) {
    mod->add(c);
    c->parent = mod;
  }
}

void scan_module_uses(PyDAST *mod, cchar *filename) {
  if (!mod || mod->kind != PY_module) return;
  Env env = module_env(mod, filename);
  if (env.dc.mods.size() || env.dc.names.size()) scan_uses(env, mod);
}

void lower_module(PyDAST *mod, cchar *filename) {
  if (!mod || mod->kind != PY_module) return;
  Env env = module_env(mod, filename);
  if (env.typing.star || env.dc.star)
    for (PyDAST *st : mod->children)
      if (st->kind == PY_import_from && st->children.n < 2) {
        cchar *m = trimmed(st->children[0]->str_val) == "typing" ? "typing" : "dataclasses";
        err(st, "`from %s import *` is not supported; import the names (issues/195)", m);
      }
  if (env.typing.mods.size() || env.typing.names.size()) lower_typing(env, mod);
  if (env.en.mods.size() || env.en.names.size()) {
    if (env.en.star) err(mod, "`from enum import *` is not supported; import the names (issues/208)");
    check_enum_unsupported(env, mod, true);
    lower_enums(env, mod);
  }
  if (env.op.mods.size() || env.op.names.size()) lower_operator(env, mod);
  bool has_ft = env.ft.mods.size() || env.ft.names.size();
  if (!env.dc.mods.size() && !env.dc.names.size()) {
    if (has_ft) lower_total_ordering(env, mod);
    return;
  }
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
  if (has_ft) lower_total_ordering(env, mod);
  std::string text;
  if (need_frozen_import) text += "from dataclasses import FrozenInstanceError as __pyc_dc_FrozenInstanceError__\n";
  if (!by_name.empty()) {
    // What the generated methods name (issues/216).
    if (uses.fields) text += "from dataclasses import Field as __pyc_dc_Field__\n";
    if (uses.astuple || uses.asdict)
      text += "from dataclasses import __pyc_dataclass__ as __pyc_dc_base__\n"
              "from _dataclasses_convert import _astuple_value as __pyc_dc_astuple_value__\n"
              "from _dataclasses_convert import _asdict_value as __pyc_dc_asdict_value__\n";
  }
  if (!text.empty()) {
    PyDAST *g = dparse_python_buf_to_ast("<dataclass>", text.c_str(), (int)text.size());
    set_origin(g, filename, 1);
    Vec<PyDAST *> all;
    for (PyDAST *c : g->children) all.add(c);
    for (PyDAST *c : mod->children) all.add(c);
    mod->children.clear();
    for (PyDAST *c : all) mod->add(c);
  }
}

}  // namespace

void lower_typing_and_dataclasses(Vec<PycModule *> &mods, Vec<PyDAST *> &imported) {
  // First what the program asks of its dataclasses, in every module.
  auto scan = [](PyDAST *mod, cchar *filename) { scan_module_uses(mod, filename); };
  for (int i = 1; i < mods.n; i++) scan(mods[i]->pymod, mods[i]->filename);
  for (PyDAST *a : imported) scan(a, a && a->filename ? a->filename : "<imported module>");
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

// After build_syms: an enum class the lowering did not see. The lowering
// finds an enum class by its base as the module names it, so a subclass of
// an enum class imported from ANOTHER module (or reached through any name
// the imports do not bind) is left a plain class: its members would stay
// the raw values, `print(Sub.A)` printing `3` where CPython prints `Sub.A`.
// Such a class inherits the enum machinery's meta `__call__`; refuse it if
// it defines anything CPython would make a member (enum._EnumDict's rule).
void pyc_enum_check(PycCompiler &ctx) {
  Vec<Sym *> lowered;
  for (PyDAST *c : all_enums)
    if (Sym *s = getAST(c, ctx)->sym) lowered.set_add(s);
  Vec<Sym *> todo, seen;
  for (Sym *r : ctx.meta_call_classes) todo.add(r);
  while (todo.n) {
    Sym *x = todo.pop();
    if (!x || seen.set_in(x)) continue;
    seen.set_add(x);
    for (Sym *sub : x->specializers)
      if (sub && sub != x && sub->type_kind == Type_RECORD) todo.add(sub);
    if (ctx.meta_call_classes.set_in(x) || lowered.set_in(x)) continue;
    for (Sym *m : x->has) {
      if (!m || m->in != x || !m->name || (m->alias && m->alias->is_fun)) continue;
      std::string n = m->name;
      if (dunder_or_sunder(n) || n.compare(0, 2, "__") == 0) continue;
      fail("error: enum class %s: its base is not an enum class pyc can see here, so '%s' would not be a member "
           "(issues/208)",
           x->name ? x->name : "?", m->name);
    }
  }
}
