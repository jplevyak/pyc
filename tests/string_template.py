# string.Template (issues/207).
from string import Template
t = Template("$who likes ${what}, $$5, $who_2 x$who")
print(t.substitute(who="tim", what="kung pao", who_2="al"), t.substitute({"who": "a", "what": "b", "who_2": "c"}))
print(t.substitute({"who": "a", "what": "b", "who_2": "c"}, who="override"), t.get_identifiers(), t.is_valid())
print(Template("$a $b ${c}").safe_substitute(a="1"), Template("$ x ${1} $").safe_substitute(), Template("a$").is_valid())
print(Template("${n}th").substitute(n=3), Template("$n.5").substitute(n=7), Template("$__x").substitute(__x="u"))
try:
    Template("$a $b").substitute(a="1")
except KeyError as e:
    print("KeyError", e)
for bad in ["x $ y", "line1\nline2 ${3}", "end$", "${a"]:
    try:
        Template(bad).substitute(a="1")
    except ValueError as e:
        print("ValueError", e)
