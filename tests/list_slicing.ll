; ModuleID = 'ifa_output'
source_filename = "list_slicing.py"
target triple = "arm64-apple-darwin25.5.0"

%range.1437 = type { ptr, ptr, ptr, ptr, i64, i64, i64 }
%list.3076 = type { i64, i64, i64 }

@int = internal global ptr null
@list = internal global ptr null
@range = internal global ptr null
@g1761 = internal global i64 0
@a = internal global ptr null
@str = internal global ptr null
@g1756 = internal global ptr null
@b = internal global ptr null
@c = internal global ptr null
@.str.lit = private constant <{ i64, [17 x i8] }> <{ i64 16, [17 x i8] c"_CG_str_from_int\00" }>
@.str.lit.1 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"[\00" }>
@.str.lit.2 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c", \00" }>
@.str.lit.3 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"]\00" }>

define ptr @_CG_f_154_0(i64 %0) {
entry:
  %1 = call ptr @_CG_f_882_1(i64 %0)
  br label %L5

L5:                                               ; preds = %entry
  ret ptr %1
}

define ptr @_CG_f_882_1(i64 %0) {
entry:
  %1 = call ptr @_CG_str_from_int(i64 %0)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %1
}

define i1 @_CG_f_890_2(i64 %0) {
entry:
  %1 = icmp ne i64 %0, 0
  br label %L81

L81:                                              ; preds = %entry
  ret i1 %1
}

define ptr @_CG_f_1283_3(ptr %0, i64 %1, i64 %2, i64 %3) {
entry:
  %4 = call ptr @_CG_list_getslice(ptr %0, i64 8, i64 %1, i64 %2, i64 %3)
  br label %L125

L125:                                             ; preds = %entry
  ret ptr %4
}

define ptr @_CG_f_1300_4(ptr %0, ptr %1) {
entry:
  %2 = call ptr @_CG_list_setslice(ptr %0, i64 8, i64 2, i64 2147483647, ptr %1)
  br label %L126

L126:                                             ; preds = %entry
  ret ptr %2
}

define ptr @_CG_f_1486_5(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_12(ptr %4)
  %6 = call ptr @_CG_f_3050_14(i64 0, i64 %5)
  store ptr %6, ptr %2, align 8
  %7 = load ptr, ptr %2, align 8
  store ptr %7, ptr %2, align 8
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %1, align 8
  %8 = load ptr, ptr %2, align 8
  store ptr %8, ptr %2, align 8
  %9 = load ptr, ptr %3, align 8
  store ptr %9, ptr %3, align 8
  br label %L144

L144:                                             ; preds = %L218, %entry
  %10 = load ptr, ptr %2, align 8
  %11 = call i1 @_CG_f_1699_7(ptr %10)
  br i1 %11, label %t_edge, label %f_edge

L219:                                             ; preds = %t_edge
  %12 = load ptr, ptr %2, align 8
  %13 = call i64 @_CG_f_1729_9(ptr %12)
  %14 = call i1 @_CG_f_890_2(i64 %13)
  br i1 %14, label %t_edge1, label %f_edge2

L145:                                             ; preds = %f_edge
  %15 = load ptr, ptr %1, align 8
  %16 = load ptr, ptr %1, align 8
  %concat4 = call ptr @_CG_strcat(ptr %16, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0))
  br label %L143

L143:                                             ; preds = %L145
  ret ptr %concat4

L217:                                             ; preds = %t_edge1
  %17 = load ptr, ptr %1, align 8
  %18 = load ptr, ptr %1, align 8
  %concat = call ptr @_CG_strcat(ptr %18, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.2, i32 0, i32 1, i32 0))
  store ptr %concat, ptr %1, align 8
  %19 = load ptr, ptr %1, align 8
  store ptr %19, ptr %1, align 8
  %20 = load ptr, ptr %1, align 8
  store ptr %20, ptr %1, align 8
  br label %L218

L218:                                             ; preds = %L217, %f_edge2
  %21 = load ptr, ptr %3, align 8
  store ptr %21, ptr %3, align 8
  %22 = load ptr, ptr %3, align 8
  %23 = getelementptr i64, ptr %22, i64 %13
  %24 = load i64, ptr %23, align 4
  %25 = call ptr @_CG_f_154_0(i64 %24)
  %26 = load ptr, ptr %1, align 8
  %27 = load ptr, ptr %1, align 8
  %concat3 = call ptr @_CG_strcat(ptr %27, ptr %25)
  store ptr %concat3, ptr %1, align 8
  %28 = load ptr, ptr %1, align 8
  store ptr %28, ptr %1, align 8
  %29 = load ptr, ptr %1, align 8
  store ptr %29, ptr %1, align 8
  %30 = load ptr, ptr %2, align 8
  store ptr %30, ptr %2, align 8
  %31 = load ptr, ptr %3, align 8
  store ptr %31, ptr %3, align 8
  br label %L144

t_edge:                                           ; preds = %L144
  %32 = load ptr, ptr %1, align 8
  store ptr %32, ptr %1, align 8
  %33 = load ptr, ptr %2, align 8
  store ptr %33, ptr %2, align 8
  %34 = load ptr, ptr %3, align 8
  store ptr %34, ptr %3, align 8
  br label %L219

f_edge:                                           ; preds = %L144
  %35 = load ptr, ptr %1, align 8
  store ptr %35, ptr %1, align 8
  %36 = load ptr, ptr %2, align 8
  store ptr %36, ptr %2, align 8
  %37 = load ptr, ptr %3, align 8
  store ptr %37, ptr %3, align 8
  br label %L145

t_edge1:                                          ; preds = %L219
  %38 = load ptr, ptr %1, align 8
  store ptr %38, ptr %1, align 8
  br label %L217

f_edge2:                                          ; preds = %L219
  %39 = load ptr, ptr %1, align 8
  store ptr %39, ptr %1, align 8
  %40 = load ptr, ptr %1, align 8
  store ptr %40, ptr %1, align 8
  br label %L218
}

define ptr @_CG_f_1684_6(ptr %0, i64 %1, i64 %2, i64 %3) {
entry:
  %4 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 4
  store i64 0, ptr %4, align 4
  %5 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 5
  store i64 %2, ptr %5, align 4
  %6 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 6
  store i64 1, ptr %6, align 4
  br label %L171

L171:                                             ; preds = %entry
  ret ptr %0
}

define i1 @_CG_f_1699_7(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i1, align 1
  store ptr %0, ptr %1, align 8
  %3 = load ptr, ptr %1, align 8
  store ptr %3, ptr %1, align 8
  br label %L237

L237:                                             ; preds = %entry
  %4 = load ptr, ptr %1, align 8
  %5 = load ptr, ptr %1, align 8
  %6 = getelementptr inbounds nuw %range.1437, ptr %5, i32 0, i32 4
  %7 = load i64, ptr %6, align 4
  %8 = load ptr, ptr %1, align 8
  %9 = load ptr, ptr %1, align 8
  %10 = getelementptr inbounds nuw %range.1437, ptr %9, i32 0, i32 5
  %11 = load i64, ptr %10, align 4
  %12 = icmp slt i64 %7, %11
  store i1 %12, ptr %2, align 1
  %13 = load i1, ptr %2, align 1
  store i1 %13, ptr %2, align 1
  %14 = load i1, ptr %2, align 1
  store i1 %14, ptr %2, align 1
  br label %L172

L238:                                             ; No predecessors!
  %15 = load i1, ptr %2, align 1
  store i1 %15, ptr %2, align 1
  br label %L172

L172:                                             ; preds = %L238, %L237
  %16 = load i1, ptr %2, align 1
  ret i1 %16
}

define ptr @_CG_f_1723_8(ptr %0) {
entry:
  br label %L173

L173:                                             ; preds = %entry
  ret ptr %0
}

define i64 @_CG_f_1729_9(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 4
  %2 = load i64, ptr %1, align 4
  %3 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 4
  %4 = load i64, ptr %3, align 4
  %5 = add i64 %4, 1
  %6 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 4
  store i64 %5, ptr %6, align 4
  br label %L174

L174:                                             ; preds = %entry
  ret i64 %2
}

define ptr @_CG_f_1667_10(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 4
  store i64 0, ptr %1, align 4
  %2 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 5
  store i64 0, ptr %2, align 4
  %3 = getelementptr inbounds nuw %range.1437, ptr %0, i32 0, i32 6
  store i64 1, ptr %3, align 4
  %g = load ptr, ptr @g1756, align 8
  %g1 = load ptr, ptr @g1756, align 8
  %g2 = load ptr, ptr @g1756, align 8
  %g3 = load ptr, ptr @g1756, align 8
  ret ptr %0
}

define ptr @_CG_f_2765_11(i64 %0, i64 %1, i64 %2) {
entry:
  %g = load ptr, ptr @g1756, align 8
  %clone = call ptr @GC_malloc(i64 56)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 56, i1 false)
  %3 = call ptr @_CG_f_1684_6(ptr %clone, i64 0, i64 %1, i64 1)
  ret ptr %clone
}

define i64 @_CG_f_326_12(ptr %0) {
entry:
  %1 = getelementptr i8, ptr %0, i64 -16
  %len = load i64, ptr %1, align 4
  br label %L175

L175:                                             ; preds = %entry
  ret i64 %len
}

define ptr @_CG_f_105_13() {
main_prelude:
  %proto = call ptr @GC_malloc(i64 56)
  store ptr %proto, ptr @range, align 8
  %proto19 = call ptr @GC_malloc(i64 56)
  store ptr %proto19, ptr @g1756, align 8
  br label %entry

entry:                                            ; preds = %main_prelude
  %new = call ptr @GC_malloc(i64 56)
  %g = load ptr, ptr @g1756, align 8
  %0 = call ptr @_CG_f_1667_10(ptr %g)
  %list = call ptr @_CG_prim_tuple_list_internal(i32 8, i32 4)
  %1 = getelementptr i64, ptr %list, i64 0
  store i64 1, ptr %1, align 4
  %2 = getelementptr i64, ptr %list, i64 1
  store i64 2, ptr %2, align 4
  %3 = getelementptr i64, ptr %list, i64 2
  store i64 3, ptr %3, align 4
  %4 = getelementptr i64, ptr %list, i64 3
  store i64 4, ptr %4, align 4
  store ptr %list, ptr @a, align 8
  %g1 = load ptr, ptr @a, align 8
  %5 = call ptr @_CG_f_1486_17(ptr %g1)
  %6 = call ptr @_CG_write(ptr %5)
  %7 = call ptr @_CG_writeln()
  %g2 = load ptr, ptr @a, align 8
  %8 = call ptr @_CG_f_1283_3(ptr %g2, i64 2, i64 4, i64 1)
  store ptr %8, ptr @b, align 8
  %g3 = load ptr, ptr @b, align 8
  %9 = call ptr @_CG_f_1486_18(ptr %g3)
  %10 = call ptr @_CG_write(ptr %9)
  %11 = call ptr @_CG_writeln()
  %list4 = call ptr @_CG_prim_tuple_list_internal(i32 8, i32 4)
  %12 = getelementptr i64, ptr %list4, i64 0
  store i64 1, ptr %12, align 4
  %13 = getelementptr i64, ptr %list4, i64 1
  store i64 2, ptr %13, align 4
  %14 = getelementptr i64, ptr %list4, i64 2
  store i64 3, ptr %14, align 4
  %15 = getelementptr i64, ptr %list4, i64 3
  store i64 4, ptr %15, align 4
  store ptr %list4, ptr @a, align 8
  %g5 = load ptr, ptr @a, align 8
  %16 = call ptr @_CG_f_1486_17(ptr %g5)
  %17 = call ptr @_CG_write(ptr %16)
  %18 = call ptr @_CG_writeln()
  %g6 = load ptr, ptr @a, align 8
  %19 = call ptr @_CG_f_1283_3(ptr %g6, i64 2, i64 2147483647, i64 1)
  store ptr %19, ptr @b, align 8
  %g7 = load ptr, ptr @a, align 8
  store ptr %g7, ptr @c, align 8
  %tmp = call ptr @_CG_prim_tuple_list_internal(i32 24, i32 4)
  %20 = getelementptr inbounds nuw %list.3076, ptr %tmp, i32 0, i32 0
  store i64 6, ptr %20, align 4
  %21 = getelementptr inbounds nuw %list.3076, ptr %tmp, i32 0, i32 1
  store i64 7, ptr %21, align 4
  %22 = getelementptr inbounds nuw %list.3076, ptr %tmp, i32 0, i32 2
  store i64 8, ptr %22, align 4
  %dst = call ptr @_CG_to_list_runtime(ptr %tmp, i32 24, i32 3)
  %g8 = load ptr, ptr @a, align 8
  %23 = call ptr @_CG_f_1300_4(ptr %g8, ptr %dst)
  %g9 = load ptr, ptr @a, align 8
  %24 = call ptr @_CG_f_1486_17(ptr %g9)
  %25 = call ptr @_CG_write(ptr %24)
  %26 = call ptr @_CG_writeln()
  %g10 = load ptr, ptr @b, align 8
  %27 = call ptr @_CG_f_1486_18(ptr %g10)
  %28 = call ptr @_CG_write(ptr %27)
  %29 = call ptr @_CG_writeln()
  %g11 = load ptr, ptr @c, align 8
  %30 = call ptr @_CG_f_1486_5(ptr %g11)
  %31 = call ptr @_CG_write(ptr %30)
  %32 = call ptr @_CG_writeln()
  %list12 = call ptr @_CG_prim_tuple_list_internal(i32 8, i32 3)
  %33 = getelementptr i64, ptr %list12, i64 0
  store i64 1, ptr %33, align 4
  %34 = getelementptr i64, ptr %list12, i64 1
  store i64 2, ptr %34, align 4
  %35 = getelementptr i64, ptr %list12, i64 2
  store i64 3, ptr %35, align 4
  store ptr %list12, ptr @a, align 8
  %g13 = load ptr, ptr @a, align 8
  %36 = call ptr @_CG_f_1283_3(ptr %g13, i64 1, i64 2147483647, i64 2)
  store ptr %36, ptr @b, align 8
  %g14 = load ptr, ptr @b, align 8
  %37 = call ptr @_CG_f_1486_18(ptr %g14)
  %38 = call ptr @_CG_write(ptr %37)
  %39 = call ptr @_CG_writeln()
  %g15 = load ptr, ptr @b, align 8
  %g16 = load ptr, ptr @b, align 8
  %40 = getelementptr i64, ptr %g16, i64 0
  store i64 4, ptr %40, align 4
  %g17 = load ptr, ptr @b, align 8
  %41 = call ptr @_CG_f_1486_18(ptr %g17)
  %42 = call ptr @_CG_write(ptr %41)
  %43 = call ptr @_CG_writeln()
  %g18 = load ptr, ptr @a, align 8
  %44 = call ptr @_CG_f_1486_17(ptr %g18)
  %45 = call ptr @_CG_write(ptr %44)
  %46 = call ptr @_CG_writeln()
  ret ptr null
}

define ptr @_CG_f_3050_14(i64 %0, i64 %1) {
entry:
  %2 = call ptr @_CG_f_2765_11(i64 0, i64 %1, i64 1)
  ret ptr %2
}

define i64 @_CG_f_326_15(ptr %0) {
entry:
  %1 = getelementptr i8, ptr %0, i64 -16
  %len = load i64, ptr %1, align 4
  br label %L175

L175:                                             ; preds = %entry
  ret i64 %len
}

define i64 @_CG_f_326_16(ptr %0) {
entry:
  %1 = getelementptr i8, ptr %0, i64 -16
  %len = load i64, ptr %1, align 4
  br label %L175

L175:                                             ; preds = %entry
  ret i64 %len
}

define ptr @_CG_f_1486_17(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_15(ptr %4)
  %6 = call ptr @_CG_f_3050_14(i64 0, i64 %5)
  store ptr %6, ptr %2, align 8
  %7 = load ptr, ptr %2, align 8
  store ptr %7, ptr %2, align 8
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %1, align 8
  %8 = load ptr, ptr %2, align 8
  store ptr %8, ptr %2, align 8
  %9 = load ptr, ptr %3, align 8
  store ptr %9, ptr %3, align 8
  br label %L144

L144:                                             ; preds = %L218, %entry
  %10 = load ptr, ptr %2, align 8
  %11 = call i1 @_CG_f_1699_7(ptr %10)
  br i1 %11, label %t_edge, label %f_edge

L219:                                             ; preds = %t_edge
  %12 = load ptr, ptr %2, align 8
  %13 = call i64 @_CG_f_1729_9(ptr %12)
  %14 = call i1 @_CG_f_890_2(i64 %13)
  br i1 %14, label %t_edge1, label %f_edge2

L145:                                             ; preds = %f_edge
  %15 = load ptr, ptr %1, align 8
  %16 = load ptr, ptr %1, align 8
  %concat4 = call ptr @_CG_strcat(ptr %16, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0))
  br label %L143

L143:                                             ; preds = %L145
  ret ptr %concat4

L217:                                             ; preds = %t_edge1
  %17 = load ptr, ptr %1, align 8
  %18 = load ptr, ptr %1, align 8
  %concat = call ptr @_CG_strcat(ptr %18, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.2, i32 0, i32 1, i32 0))
  store ptr %concat, ptr %1, align 8
  %19 = load ptr, ptr %1, align 8
  store ptr %19, ptr %1, align 8
  %20 = load ptr, ptr %1, align 8
  store ptr %20, ptr %1, align 8
  br label %L218

L218:                                             ; preds = %L217, %f_edge2
  %21 = load ptr, ptr %3, align 8
  store ptr %21, ptr %3, align 8
  %22 = load ptr, ptr %3, align 8
  %23 = getelementptr i64, ptr %22, i64 %13
  %24 = load i64, ptr %23, align 4
  %25 = call ptr @_CG_f_154_0(i64 %24)
  %26 = load ptr, ptr %1, align 8
  %27 = load ptr, ptr %1, align 8
  %concat3 = call ptr @_CG_strcat(ptr %27, ptr %25)
  store ptr %concat3, ptr %1, align 8
  %28 = load ptr, ptr %1, align 8
  store ptr %28, ptr %1, align 8
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %1, align 8
  %29 = load ptr, ptr %2, align 8
  store ptr %29, ptr %2, align 8
  %30 = load ptr, ptr %3, align 8
  store ptr %30, ptr %3, align 8
  br label %L144

t_edge:                                           ; preds = %L144
  %31 = load ptr, ptr %1, align 8
  store ptr %31, ptr %1, align 8
  %32 = load ptr, ptr %2, align 8
  store ptr %32, ptr %2, align 8
  %33 = load ptr, ptr %3, align 8
  store ptr %33, ptr %3, align 8
  br label %L219

f_edge:                                           ; preds = %L144
  %34 = load ptr, ptr %1, align 8
  store ptr %34, ptr %1, align 8
  %35 = load ptr, ptr %2, align 8
  store ptr %35, ptr %2, align 8
  %36 = load ptr, ptr %3, align 8
  store ptr %36, ptr %3, align 8
  br label %L145

t_edge1:                                          ; preds = %L219
  %37 = load ptr, ptr %1, align 8
  store ptr %37, ptr %1, align 8
  br label %L217

f_edge2:                                          ; preds = %L219
  %38 = load ptr, ptr %1, align 8
  store ptr %38, ptr %1, align 8
  %39 = load ptr, ptr %1, align 8
  store ptr %39, ptr %1, align 8
  br label %L218
}

define ptr @_CG_f_1486_18(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_16(ptr %4)
  %6 = call ptr @_CG_f_3050_14(i64 0, i64 %5)
  store ptr %6, ptr %2, align 8
  %7 = load ptr, ptr %2, align 8
  store ptr %7, ptr %2, align 8
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %1, align 8
  %8 = load ptr, ptr %2, align 8
  store ptr %8, ptr %2, align 8
  %9 = load ptr, ptr %3, align 8
  store ptr %9, ptr %3, align 8
  br label %L144

L144:                                             ; preds = %L218, %entry
  %10 = load ptr, ptr %2, align 8
  %11 = call i1 @_CG_f_1699_7(ptr %10)
  br i1 %11, label %t_edge, label %f_edge

L219:                                             ; preds = %t_edge
  %12 = load ptr, ptr %2, align 8
  %13 = call i64 @_CG_f_1729_9(ptr %12)
  %14 = call i1 @_CG_f_890_2(i64 %13)
  br i1 %14, label %t_edge1, label %f_edge2

L145:                                             ; preds = %f_edge
  %15 = load ptr, ptr %1, align 8
  %16 = load ptr, ptr %1, align 8
  %concat4 = call ptr @_CG_strcat(ptr %16, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0))
  br label %L143

L143:                                             ; preds = %L145
  ret ptr %concat4

L217:                                             ; preds = %t_edge1
  %17 = load ptr, ptr %1, align 8
  %18 = load ptr, ptr %1, align 8
  %concat = call ptr @_CG_strcat(ptr %18, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.2, i32 0, i32 1, i32 0))
  store ptr %concat, ptr %1, align 8
  %19 = load ptr, ptr %1, align 8
  store ptr %19, ptr %1, align 8
  %20 = load ptr, ptr %1, align 8
  store ptr %20, ptr %1, align 8
  br label %L218

L218:                                             ; preds = %L217, %f_edge2
  %21 = load ptr, ptr %3, align 8
  store ptr %21, ptr %3, align 8
  %22 = load ptr, ptr %3, align 8
  %23 = getelementptr i64, ptr %22, i64 %13
  %24 = load i64, ptr %23, align 4
  %25 = call ptr @_CG_f_154_0(i64 %24)
  %26 = load ptr, ptr %1, align 8
  %27 = load ptr, ptr %1, align 8
  %concat3 = call ptr @_CG_strcat(ptr %27, ptr %25)
  store ptr %concat3, ptr %1, align 8
  %28 = load ptr, ptr %1, align 8
  store ptr %28, ptr %1, align 8
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %1, align 8
  %29 = load ptr, ptr %2, align 8
  store ptr %29, ptr %2, align 8
  %30 = load ptr, ptr %3, align 8
  store ptr %30, ptr %3, align 8
  br label %L144

t_edge:                                           ; preds = %L144
  %31 = load ptr, ptr %1, align 8
  store ptr %31, ptr %1, align 8
  %32 = load ptr, ptr %2, align 8
  store ptr %32, ptr %2, align 8
  %33 = load ptr, ptr %3, align 8
  store ptr %33, ptr %3, align 8
  br label %L219

f_edge:                                           ; preds = %L144
  %34 = load ptr, ptr %1, align 8
  store ptr %34, ptr %1, align 8
  %35 = load ptr, ptr %2, align 8
  store ptr %35, ptr %2, align 8
  br label %L145

t_edge1:                                          ; preds = %L219
  %36 = load ptr, ptr %1, align 8
  store ptr %36, ptr %1, align 8
  br label %L217

f_edge2:                                          ; preds = %L219
  %37 = load ptr, ptr %1, align 8
  store ptr %37, ptr %1, align 8
  %38 = load ptr, ptr %1, align 8
  store ptr %38, ptr %1, align 8
  br label %L218
}

declare ptr @_CG_str_from_int(i64)

declare ptr @_CG_list_getslice(ptr, i64, i64, i64, i64)

declare ptr @_CG_list_setslice(ptr, i64, i64, i64, ptr)

declare ptr @_CG_strcat(ptr, ptr)

declare ptr @GC_malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

declare ptr @_CG_prim_tuple_list_internal(i32, i32)

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

declare ptr @_CG_to_list_runtime(ptr, i32, i32)

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_13()
  ret i32 0
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "list_slicing.py", directory: "tests")
