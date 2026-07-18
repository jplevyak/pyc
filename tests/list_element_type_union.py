# issue 025 rubik2: `list` is a single generic class program-wide, so
# its element type is the union of every list's element type in the
# program. Mixing a list-of-lists (affected_cubies, below -- element
# type `list` itself, Type_PRIMITIVE) with a list-of-class-instances
# (states/next_states -- element type `cube_state`, Type_RECORD)
# produced a Type_SUM neither of whose members satisfied the old
# "every member is Type_RECORD" check in P_prim_sizeof_element
# (ifa/codegen/cg.cc), even though both members are equally
# pointer-sized/boxed. That made list.append's resize call see
# sizeof_element==0 -- storage never grew, and reads returned
# corrupted/aliased objects (BFS state accumulation went straight to
# a segfault a few levels in). A trimmed, deterministic slice of the
# real cube_state/apply_move/BFS shape from shedskin_examples/rubik2.
#
# Compile-only here (no .exec.check): the C backend runs this
# correctly (verified manually -- matches CPython's output for all 3
# BFS levels) and that's the durable, executed check via
# shedskin_examples/rubik2 itself. The LLVM backend (-b) hits a
# separate, deeper, still-open bug on this same union shape --
# crashes during the very first apply_move, before this fix's
# sizeof_element path is even exercised meaningfully -- see
# ifa/issues/051.
affected_cubies = [[0, 1, 2, 3, 0, 1, 2, 3], [4, 7, 6, 5, 4, 5, 6, 7], [0, 9, 4, 8, 0, 3, 5, 4], [2, 10, 6, 11, 2, 1, 7, 6], [3, 11, 7, 9, 3, 2, 6, 5], [1, 8, 5, 10, 1, 0, 4, 7]]
phase_moves = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17]

class cube_state:
    def __init__(self, state, route=None):
        self.state = state
        self.route = route or []

    def id_(self):
        return tuple(self.state[20:32])

    def apply_move(self, move):
        face, turns = move // 3, move % 3 + 1
        newstate = self.state[:]
        for turn in range(turns):
            oldstate = newstate[:]
            for i in range(8):
                isCorner = int(i > 3)
                target = affected_cubies[face][i] + isCorner*12
                killer = affected_cubies[face][(i-3) if (i&3)==3 else i+1] + isCorner*12
                orientationDelta = int(1<face<4) if i<4 else (0 if face<2 else 2 - (i&1))
                newstate[target] = oldstate[killer]
                newstate[target+20] = oldstate[killer+20] + orientationDelta
                if turn == turns-1:
                    newstate[target+20] %= 2 + isCorner
        return cube_state(newstate, self.route+[move])

goal_state = cube_state(list(range(20))+20*[0])
state = cube_state(goal_state.state[:])
for move in [3, 11, 5, 0, 14, 8, 2, 17, 6, 9]:
    state = state.apply_move(move)
state.route = []

current_id = state.id_()
states = [state]
state_ids = set([current_id])
for level in range(3):
    next_states = []
    for cur_state in states:
        for move in phase_moves:
            next_state = cur_state.apply_move(move)
            next_id = next_state.id_()
            if next_id not in state_ids:
                state_ids.add(next_id)
                next_states.append(next_state)
    states = next_states
    print(level, len(states), len(state_ids))
