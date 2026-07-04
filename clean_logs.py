import re

with open("ifa/codegen/cg_emit_llvm.cc", "r") as f:
    lines = f.readlines()

new_lines = []
skip = 0

for i, line in enumerate(lines):
    if skip > 0:
        if '{' in line:
            skip += line.count('{')
        if '}' in line:
            skip -= line.count('}')
        continue

    # Detect start of our debug blocks
    if '1620_45' in line and 'if' in line and '{' in line:
        skip = line.count('{') - line.count('}')
        # Single line block like "if (...) { ... }"
        if skip == 0:
            pass # skip it entirely
        continue

    # Handled special cases
    if '1620_45' in line:
        continue # skip single line if statements without brace maybe

    # Specifically we also added `} else if (s && ctx.fn && ...`
    if 'else if (s && ctx.fn' in line and '1620_45' in line:
        # This was part of `} else if (s && ctx.fn && ctx.fn->cg_string && strstr(ctx.fn->cg_string, "1620_45")) {`
        # We need to change it back to `}`
        new_lines.append(line[:line.find('}') + 1] + '\n')
        skip = line.count('{') - line.count('}')
        continue

    # Another specific case: `} else {` before the `if (ctx.fn ...`
    # Let's just fix the rest manually if needed, Python script is decent.
    new_lines.append(line)

with open("ifa/codegen/cg_emit_llvm.cc", "w") as f:
    f.writelines(new_lines)

