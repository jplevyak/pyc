; ModuleID = 'ifa_output'
source_filename = "builtins.py"
target triple = "arm64-apple-darwin25.5.0"

%__list_iter__.1214 = type { ptr, ptr, ptr, i64, ptr }
%range.1437 = type { ptr, ptr, ptr, ptr, i64, i64, i64 }
%bytearray.3300 = type { ptr, ptr, ptr, ptr, ptr, ptr, i64 }
%bytearray.3298 = type { ptr, ptr, ptr, ptr, ptr, ptr, i64 }
%list.3306 = type { i64, i64, i64 }
%list.3296 = type { ptr }

@int = internal global ptr null
@__list_iter__ = internal global ptr null
@bytearray = internal global ptr null
@str = internal global ptr null
@float64 = internal global double 0.000000e+00
@bool = internal global i1 false
@x = internal global ptr null
@list = internal global ptr null
@buffer = internal global ptr null
@g1756 = internal global ptr null
@g2143 = internal global ptr null
@uint8 = internal global i8 0
@int64 = internal global i64 0
@range = internal global ptr null
@float = internal global ptr null
@None = internal global ptr null
@g1761 = internal global i64 0
@g1063 = internal global ptr null
@.str.lit = private constant <{ i64, [5 x i8] }> <{ i64 4, [5 x i8] c"True\00" }>
@.str.lit.1 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>
@.str.lit.2 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"'\00" }>
@.str.lit.3 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c" \00" }>
@.str.lit.4 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"a\00" }>
@.str.lit.5 = private constant <{ i64, [17 x i8] }> <{ i64 16, [17 x i8] c"_CG_str_from_int\00" }>
@.str.lit.6 = private constant <{ i64, [19 x i8] }> <{ i64 18, [19 x i8] c"_CG_str_from_float\00" }>
@.str.lit.7 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c")\00" }>
@.str.lit.8 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"[\00" }>
@.str.lit.9 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c", \00" }>
@.str.lit.10 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"]\00" }>
@.str.lit.11 = private constant <{ i64, [4 x i8] }> <{ i64 3, [4 x i8] c"-0b\00" }>
@.str.lit.12 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"0\00" }>
@.str.lit.13 = private constant <{ i64, [1 x i8] }> zeroinitializer
@.str.lit.14 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"1\00" }>
@.str.lit.15 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"0b\00" }>
@.str.lit.16 = private constant <{ i64, [7 x i8] }> <{ i64 6, [7 x i8] c"foobar\00" }>
@.str.lit.17 = private constant <{ i64, [8 x i8] }> <{ i64 7, [8 x i8] c"_CG_chr\00" }>
@.str.lit.18 = private constant <{ i64, [8 x i8] }> <{ i64 7, [8 x i8] c"_CG_ord\00" }>
@.str.lit.19 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"0x\00" }>
@.str.lit.20 = private constant <{ i64, [13 x i8] }> <{ i64 12, [13 x i8] c"bytearray(b'\00" }>
@.str.lit.21 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"\\t\00" }>
@.str.lit.22 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"\\n\00" }>
@.str.lit.23 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"\\r\00" }>
@.str.lit.24 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"\\\\\00" }>
@.str.lit.25 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"\\'\00" }>
@.str.lit.26 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"\\x\00" }>
@.str.lit.27 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"')\00" }>
@.str.lit.28 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"A\00" }>
@.str.lit.29 = private constant <{ i64, [14 x i8] }> <{ i64 13, [14 x i8] c"<class 'str'>\00" }>
@.str.lit.30 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"b\00" }>

define ptr @_CG_f_293_0(i1 %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = call i1 @_CG_f_299_66(i1 %0)
  br i1 %2, label %t_edge, label %f_edge

L203:                                             ; preds = %t_edge
  store ptr getelementptr inbounds (<{ i64, [5 x i8] }>, ptr @.str.lit, i32 0, i32 1, i32 0), ptr %1, align 8
  br label %L26

L204:                                             ; preds = %f_edge
  store ptr getelementptr inbounds (<{ i64, [6 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %1, align 8
  br label %L26

L26:                                              ; preds = %L204, %L203
  %3 = load ptr, ptr %1, align 8
  ret ptr %3

t_edge:                                           ; preds = %entry
  br label %L203

f_edge:                                           ; preds = %entry
  br label %L204
}

define i1 @_CG_f_299_1(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

define i1 @_CG_f_2234_2(i64 %0) {
entry:
  %1 = trunc i64 %0 to i1
  ret i1 %1
}

define ptr @_CG_f_378_3(ptr %0) {
entry:
  br label %L35

L35:                                              ; preds = %entry
  ret ptr %0
}

define ptr @_CG_f_384_4(ptr %0) {
entry:
  %concat = call ptr @_CG_strcat(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.2, i32 0, i32 1, i32 0), ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0))
  %concat1 = call ptr @_CG_strcat(ptr %concat, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.2, i32 0, i32 1, i32 0))
  br label %L36

L36:                                              ; preds = %entry
  ret ptr %concat1
}

define ptr @_CG_f_415_5() {
entry:
  %0 = call ptr @_CG_string_mult(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.4, i32 0, i32 1, i32 0), i64 10)
  br label %L40

L40:                                              ; preds = %entry
  ret ptr %0
}

define ptr @_CG_f_882_6() {
entry:
  %0 = call ptr @_CG_str_from_int(i64 1)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %0
}

define i64 @_CG_f_2466_7() {
entry:
  ret i64 3
}

define ptr @_CG_f_1125_8(double %0) {
entry:
  %1 = call ptr @_CG_str_from_float(double %0)
  br label %L109

L109:                                             ; preds = %entry
  ret ptr %1
}

define double @_CG_f_2539_9() {
entry:
  ret double 3.000000e+00
}

define i1 @_CG_f_1232_10(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_44(ptr %4)
  %6 = icmp slt i64 %2, 3
  br label %L120

L120:                                             ; preds = %entry
  ret i1 %6
}

define i64 @_CG_f_1246_11(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = add i64 %2, 1
  %4 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  store i64 %3, ptr %4, align 4
  %5 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %6 = load ptr, ptr %5, align 8
  br label %L121

L121:                                             ; preds = %entry
  ret i64 0
}

define ptr @_CG_f_1216_12(ptr %0) {
entry:
  %g = load ptr, ptr @None, align 8
  %g1 = load ptr, ptr @None, align 8
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  store ptr %g1, ptr %1, align 8
  %2 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  store i64 0, ptr %2, align 4
  %g2 = load ptr, ptr @g1063, align 8
  %g3 = load ptr, ptr @g1063, align 8
  %g4 = load ptr, ptr @g1063, align 8
  %g5 = load ptr, ptr @g1063, align 8
  ret ptr %0
}

define ptr @_CG_f_2612_13(ptr %0) {
entry:
  %g = load ptr, ptr @g1063, align 8
  %clone = call ptr @GC_malloc(i64 40)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 40, i1 false)
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %clone, i32 0, i32 4
  store ptr %0, ptr %1, align 8
  ret ptr %clone
}

define ptr @_CG_f_1336_14(ptr %0) {
entry:
  %1 = call ptr @_CG_f_2612_13(ptr %0)
  br label %L129

L129:                                             ; preds = %entry
  ret ptr %1
}

define ptr @_CG_f_1380_15(ptr %0, i64 %1) {
entry:
  %g = load ptr, ptr @int, align 8
  %2 = call ptr @_CG_list_mult(ptr %0, i64 %1, i64 8)
  br label %L134

L134:                                             ; preds = %entry
  ret ptr %2
}

define ptr @_CG_f_1486_16(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_42(ptr %4)
  %6 = call ptr @_CG_f_3261_38(i64 0, i64 %5)
  store ptr %6, ptr %2, align 8
  %7 = load ptr, ptr %2, align 8
  store ptr %7, ptr %2, align 8
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.8, i32 0, i32 1, i32 0), ptr %1, align 8
  %8 = load ptr, ptr %2, align 8
  store ptr %8, ptr %2, align 8
  %9 = load ptr, ptr %3, align 8
  store ptr %9, ptr %3, align 8
  br label %L144

L144:                                             ; preds = %L218, %entry
  %10 = load ptr, ptr %2, align 8
  %11 = call i1 @_CG_f_1699_21(ptr %10)
  br i1 %11, label %t_edge, label %f_edge

L219:                                             ; preds = %t_edge
  %12 = load ptr, ptr %2, align 8
  %13 = call i64 @_CG_f_1729_23(ptr %12)
  %14 = call i1 @_CG_f_890_49(i64 %13)
  br i1 %14, label %t_edge1, label %f_edge2

L145:                                             ; preds = %f_edge
  %15 = load ptr, ptr %1, align 8
  %16 = load ptr, ptr %1, align 8
  %concat4 = call ptr @_CG_strcat(ptr %16, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.10, i32 0, i32 1, i32 0))
  br label %L143

L143:                                             ; preds = %L145
  ret ptr %concat4

L217:                                             ; preds = %t_edge1
  %17 = load ptr, ptr %1, align 8
  %18 = load ptr, ptr %1, align 8
  %concat = call ptr @_CG_strcat(ptr %18, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.9, i32 0, i32 1, i32 0))
  store ptr %concat, ptr %1, align 8
  %19 = load ptr, ptr %1, align 8
  store ptr %19, ptr %1, align 8
  %20 = load ptr, ptr %1, align 8
  store ptr %20, ptr %1, align 8
  br label %L218

L218:                                             ; preds = %L217, %f_edge2
  %21 = call ptr @_CG_f_384_4(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0))
  %22 = load ptr, ptr %1, align 8
  %23 = load ptr, ptr %1, align 8
  %concat3 = call ptr @_CG_strcat(ptr %23, ptr %21)
  store ptr %concat3, ptr %1, align 8
  %24 = load ptr, ptr %1, align 8
  store ptr %24, ptr %1, align 8
  %25 = load ptr, ptr %1, align 8
  store ptr %25, ptr %1, align 8
  %26 = load ptr, ptr %2, align 8
  store ptr %26, ptr %2, align 8
  %27 = load ptr, ptr %3, align 8
  store ptr %27, ptr %3, align 8
  br label %L144

t_edge:                                           ; preds = %L144
  %28 = load ptr, ptr %1, align 8
  store ptr %28, ptr %1, align 8
  %29 = load ptr, ptr %2, align 8
  store ptr %29, ptr %2, align 8
  %30 = load ptr, ptr %3, align 8
  store ptr %30, ptr %3, align 8
  br label %L219

f_edge:                                           ; preds = %L144
  %31 = load ptr, ptr %1, align 8
  store ptr %31, ptr %1, align 8
  %32 = load ptr, ptr %2, align 8
  store ptr %32, ptr %2, align 8
  br label %L145

t_edge1:                                          ; preds = %L219
  %33 = load ptr, ptr %1, align 8
  store ptr %33, ptr %1, align 8
  br label %L217

f_edge2:                                          ; preds = %L219
  %34 = load ptr, ptr %1, align 8
  store ptr %34, ptr %1, align 8
  %35 = load ptr, ptr %1, align 8
  store ptr %35, ptr %1, align 8
  br label %L218
}

define i1 @_CG_f_1620_17(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i1, align 1
  %3 = call ptr @_CG_f_1336_61(ptr %0)
  store ptr %3, ptr %1, align 8
  %4 = load ptr, ptr %1, align 8
  store ptr %4, ptr %1, align 8
  br label %L160

L160:                                             ; preds = %L223, %entry
  %5 = load ptr, ptr %1, align 8
  %6 = call i1 @_CG_f_1232_52(ptr %5)
  br i1 %6, label %t_edge, label %f_edge

L224:                                             ; preds = %t_edge
  %7 = load ptr, ptr %1, align 8
  %8 = call i64 @_CG_f_1246_55(ptr %7)
  %9 = icmp eq i64 %8, 0
  %10 = call i1 @_CG_f_299_65(i1 %9)
  br i1 %10, label %t_edge1, label %f_edge2

L161:                                             ; preds = %f_edge
  store i1 true, ptr %2, align 1
  br label %L159

L159:                                             ; preds = %L161, %L222
  %11 = load i1, ptr %2, align 1
  ret i1 %11

L222:                                             ; preds = %t_edge1
  store i1 false, ptr %2, align 1
  br label %L159

L223:                                             ; preds = %f_edge2
  %12 = load ptr, ptr %1, align 8
  store ptr %12, ptr %1, align 8
  br label %L160

t_edge:                                           ; preds = %L160
  %13 = load ptr, ptr %1, align 8
  store ptr %13, ptr %1, align 8
  br label %L224

f_edge:                                           ; preds = %L160
  %14 = load ptr, ptr %1, align 8
  store ptr %14, ptr %1, align 8
  br label %L161

t_edge1:                                          ; preds = %L224
  %15 = load ptr, ptr %1, align 8
  store ptr %15, ptr %1, align 8
  br label %L222

f_edge2:                                          ; preds = %L224
  %16 = load ptr, ptr %1, align 8
  store ptr %16, ptr %1, align 8
  br label %L223
}

define i1 @_CG_f_1627_18(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i1, align 1
  %3 = call ptr @_CG_f_1336_14(ptr %0)
  store ptr %3, ptr %1, align 8
  %4 = load ptr, ptr %1, align 8
  store ptr %4, ptr %1, align 8
  br label %L163

L163:                                             ; preds = %L226, %entry
  %5 = load ptr, ptr %1, align 8
  %6 = call i1 @_CG_f_1232_10(ptr %5)
  br i1 %6, label %t_edge, label %f_edge

L227:                                             ; preds = %t_edge
  %7 = load ptr, ptr %1, align 8
  %8 = call i64 @_CG_f_1246_11(ptr %7)
  %9 = load ptr, ptr %1, align 8
  store ptr %9, ptr %1, align 8
  br label %L226

L164:                                             ; preds = %f_edge
  store i1 false, ptr %2, align 1
  br label %L162

L162:                                             ; preds = %L164, %L225
  ret i1 false

L225:                                             ; No predecessors!
  %10 = load i1, ptr %2, align 1
  store i1 %10, ptr %2, align 1
  br label %L162

L226:                                             ; preds = %L227
  %11 = load ptr, ptr %1, align 8
  store ptr %11, ptr %1, align 8
  br label %L163

t_edge:                                           ; preds = %L163
  %12 = load ptr, ptr %1, align 8
  store ptr %12, ptr %1, align 8
  br label %L227

f_edge:                                           ; preds = %L163
  %13 = load ptr, ptr %1, align 8
  store ptr %13, ptr %1, align 8
  br label %L164
}

define ptr @_CG_f_1633_19(i64 %0) {
entry:
  %1 = alloca i64, align 8
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  store i64 %0, ptr %1, align 4
  %5 = load i64, ptr %1, align 4
  %6 = icmp slt i64 %5, 0
  %7 = call i1 @_CG_f_299_63(i1 %6)
  br i1 %7, label %t_edge, label %f_edge

L228:                                             ; preds = %t_edge
  %8 = load i64, ptr %1, align 4
  %9 = sub i64 0, %8
  store i64 %9, ptr %1, align 4
  %10 = load i64, ptr %1, align 4
  store i64 %10, ptr %1, align 4
  store ptr getelementptr inbounds (<{ i64, [4 x i8] }>, ptr @.str.lit.11, i32 0, i32 1, i32 0), ptr %2, align 8
  %11 = load i64, ptr %1, align 4
  store i64 %11, ptr %1, align 4
  br label %L230

L229:                                             ; preds = %f_edge
  store ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.15, i32 0, i32 1, i32 0), ptr %2, align 8
  %12 = load i64, ptr %1, align 4
  store i64 %12, ptr %1, align 4
  br label %L230

L230:                                             ; preds = %L229, %L228
  %13 = load i64, ptr %1, align 4
  %14 = icmp eq i64 %13, 0
  %15 = call i1 @_CG_f_299_63(i1 %14)
  br i1 %15, label %t_edge1, label %f_edge2

L231:                                             ; preds = %t_edge1
  %16 = load ptr, ptr %2, align 8
  %17 = load ptr, ptr %2, align 8
  %concat = call ptr @_CG_strcat(ptr %17, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.12, i32 0, i32 1, i32 0))
  store ptr %concat, ptr %4, align 8
  %18 = load ptr, ptr %4, align 8
  store ptr %18, ptr %4, align 8
  %19 = load ptr, ptr %4, align 8
  store ptr %19, ptr %4, align 8
  br label %L165

L232:                                             ; preds = %f_edge2
  store ptr getelementptr inbounds (<{ i64, [1 x i8] }>, ptr @.str.lit.13, i32 0, i32 1, i32 0), ptr %3, align 8
  %20 = load i64, ptr %1, align 4
  store i64 %20, ptr %1, align 4
  br label %L166

L166:                                             ; preds = %L235, %L232
  %21 = load i64, ptr %1, align 4
  %22 = icmp sgt i64 %21, 0
  br i1 %22, label %t_edge3, label %f_edge4

L236:                                             ; preds = %t_edge3
  %23 = load i64, ptr %1, align 4
  %24 = and i64 %23, 1
  %25 = icmp eq i64 %24, 0
  %26 = call i1 @_CG_f_299_63(i1 %25)
  br i1 %26, label %t_edge5, label %f_edge6

L167:                                             ; preds = %f_edge4
  %27 = load ptr, ptr %2, align 8
  %28 = load ptr, ptr %3, align 8
  %29 = load ptr, ptr %2, align 8
  %30 = load ptr, ptr %3, align 8
  %concat9 = call ptr @_CG_strcat(ptr %29, ptr %30)
  store ptr %concat9, ptr %4, align 8
  %31 = load ptr, ptr %4, align 8
  store ptr %31, ptr %4, align 8
  %32 = load ptr, ptr %4, align 8
  store ptr %32, ptr %4, align 8
  br label %L165

L165:                                             ; preds = %L167, %L231
  %33 = load ptr, ptr %4, align 8
  ret ptr %33

L233:                                             ; preds = %t_edge5
  %34 = load ptr, ptr %3, align 8
  %35 = load ptr, ptr %3, align 8
  %concat7 = call ptr @_CG_strcat(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.12, i32 0, i32 1, i32 0), ptr %35)
  store ptr %concat7, ptr %3, align 8
  %36 = load ptr, ptr %3, align 8
  store ptr %36, ptr %3, align 8
  %37 = load ptr, ptr %3, align 8
  store ptr %37, ptr %3, align 8
  br label %L235

L234:                                             ; preds = %f_edge6
  %38 = load ptr, ptr %3, align 8
  %39 = load ptr, ptr %3, align 8
  %concat8 = call ptr @_CG_strcat(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.14, i32 0, i32 1, i32 0), ptr %39)
  store ptr %concat8, ptr %3, align 8
  %40 = load ptr, ptr %3, align 8
  store ptr %40, ptr %3, align 8
  %41 = load ptr, ptr %3, align 8
  store ptr %41, ptr %3, align 8
  br label %L235

L235:                                             ; preds = %L234, %L233
  %42 = load i64, ptr %1, align 4
  %43 = ashr i64 %42, 1
  store i64 %43, ptr %1, align 4
  %44 = load i64, ptr %1, align 4
  store i64 %44, ptr %1, align 4
  %45 = load ptr, ptr %3, align 8
  store ptr %45, ptr %3, align 8
  %46 = load i64, ptr %1, align 4
  store i64 %46, ptr %1, align 4
  br label %L166

t_edge:                                           ; preds = %entry
  %47 = load i64, ptr %1, align 4
  store i64 %47, ptr %1, align 4
  br label %L228

f_edge:                                           ; preds = %entry
  %48 = load i64, ptr %1, align 4
  store i64 %48, ptr %1, align 4
  br label %L229

t_edge1:                                          ; preds = %L230
  %49 = load ptr, ptr %2, align 8
  store ptr %49, ptr %2, align 8
  br label %L231

f_edge2:                                          ; preds = %L230
  %50 = load ptr, ptr %2, align 8
  store ptr %50, ptr %2, align 8
  %51 = load i64, ptr %1, align 4
  store i64 %51, ptr %1, align 4
  br label %L232

t_edge3:                                          ; preds = %L166
  %52 = load ptr, ptr %3, align 8
  store ptr %52, ptr %3, align 8
  %53 = load i64, ptr %1, align 4
  store i64 %53, ptr %1, align 4
  br label %L236

f_edge4:                                          ; preds = %L166
  %54 = load ptr, ptr %3, align 8
  store ptr %54, ptr %3, align 8
  br label %L167

t_edge5:                                          ; preds = %L236
  %55 = load ptr, ptr %3, align 8
  store ptr %55, ptr %3, align 8
  br label %L233

f_edge6:                                          ; preds = %L236
  %56 = load ptr, ptr %3, align 8
  store ptr %56, ptr %3, align 8
  br label %L234
}

define ptr @_CG_f_1684_20(ptr %0, i64 %1, i64 %2, i64 %3) {
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

define i1 @_CG_f_1699_21(ptr %0) {
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

define ptr @_CG_f_1723_22(ptr %0) {
entry:
  br label %L173

L173:                                             ; preds = %entry
  ret ptr %0
}

define i64 @_CG_f_1729_23(ptr %0) {
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

define ptr @_CG_f_1667_24(ptr %0) {
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

define ptr @_CG_f_2867_25(i64 %0, i64 %1, i64 %2) {
entry:
  %g = load ptr, ptr @g1756, align 8
  %clone = call ptr @GC_malloc(i64 56)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 56, i1 false)
  %3 = call ptr @_CG_f_1684_20(ptr %clone, i64 0, i64 %1, i64 1)
  ret ptr %clone
}

define i64 @_CG_f_326_26(ptr %0) {
entry:
  %len = load i64, ptr getelementptr (i8, ptr getelementptr inbounds (<{ i64, [7 x i8] }>, ptr @.str.lit.16, i32 0, i32 1, i32 0), i64 -8), align 4
  br label %L175

L175:                                             ; preds = %entry
  ret i64 %len
}

define ptr @_CG_f_1748_27(i64 %0) {
entry:
  %1 = call ptr @_CG_chr(i64 %0)
  br label %L176

L176:                                             ; preds = %entry
  ret ptr %1
}

define i64 @_CG_f_1754_28(ptr %0) {
entry:
  %1 = call i64 @_CG_ord(ptr %0)
  br label %L177

L177:                                             ; preds = %entry
  ret i64 %1
}

define ptr @_CG_f_1760_29(i64 %0) {
entry:
  %1 = alloca i64, align 8
  %2 = alloca ptr, align 8
  store i64 %0, ptr %1, align 4
  %3 = load i64, ptr %1, align 4
  %4 = icmp slt i64 %3, 10
  %5 = call i1 @_CG_f_299_67(i1 %4)
  br i1 %5, label %t_edge, label %f_edge

L243:                                             ; preds = %t_edge
  %6 = call i64 @_CG_f_1754_28(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.12, i32 0, i32 1, i32 0))
  %7 = load i64, ptr %1, align 4
  %8 = add i64 %6, %7
  %9 = call ptr @_CG_f_1748_27(i64 %8)
  store ptr %9, ptr %2, align 8
  %10 = load ptr, ptr %2, align 8
  store ptr %10, ptr %2, align 8
  %11 = load ptr, ptr %2, align 8
  store ptr %11, ptr %2, align 8
  br label %L178

L244:                                             ; preds = %f_edge
  %12 = load i64, ptr %1, align 4
  %13 = icmp slt i64 %12, 16
  %14 = call i1 @_CG_f_299_67(i1 %13)
  br i1 %14, label %t_edge1, label %f_edge2

L240:                                             ; preds = %t_edge1
  %15 = call i64 @_CG_f_1754_28(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.4, i32 0, i32 1, i32 0))
  %16 = load i64, ptr %1, align 4
  %17 = add i64 %15, %16
  %18 = sub i64 %17, 10
  %19 = call ptr @_CG_f_1748_27(i64 %18)
  store ptr %19, ptr %2, align 8
  %20 = load ptr, ptr %2, align 8
  store ptr %20, ptr %2, align 8
  %21 = load ptr, ptr %2, align 8
  store ptr %21, ptr %2, align 8
  br label %L178

L241:                                             ; preds = %f_edge2
  %22 = load i64, ptr %1, align 4
  %23 = sdiv i64 %22, 16
  %24 = call ptr @_CG_f_1760_29(i64 %23)
  %25 = load i64, ptr %1, align 4
  %26 = srem i64 %25, 16
  %27 = call ptr @_CG_f_1760_29(i64 %26)
  %concat = call ptr @_CG_strcat(ptr %24, ptr %27)
  store ptr %concat, ptr %2, align 8
  %28 = load ptr, ptr %2, align 8
  store ptr %28, ptr %2, align 8
  %29 = load ptr, ptr %2, align 8
  store ptr %29, ptr %2, align 8
  br label %L178

L178:                                             ; preds = %L241, %L240, %L243
  %30 = load ptr, ptr %2, align 8
  ret ptr %30

t_edge:                                           ; preds = %entry
  %31 = load i64, ptr %1, align 4
  store i64 %31, ptr %1, align 4
  br label %L243

f_edge:                                           ; preds = %entry
  %32 = load i64, ptr %1, align 4
  store i64 %32, ptr %1, align 4
  br label %L244

t_edge1:                                          ; preds = %L244
  %33 = load i64, ptr %1, align 4
  store i64 %33, ptr %1, align 4
  br label %L240

f_edge2:                                          ; preds = %L244
  %34 = load i64, ptr %1, align 4
  store i64 %34, ptr %1, align 4
  br label %L241
}

define ptr @_CG_f_1789_30(i64 %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i64, align 8
  %3 = alloca ptr, align 8
  store i64 23165, ptr %2, align 4
  br label %L247

L246:                                             ; No predecessors!
  %4 = load ptr, ptr %3, align 8
  store ptr %4, ptr %3, align 8
  br label %L179

L247:                                             ; preds = %entry
  %5 = call ptr @_CG_f_1760_29(i64 23165)
  %concat = call ptr @_CG_strcat(ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.19, i32 0, i32 1, i32 0), ptr %5)
  store ptr %concat, ptr %3, align 8
  %6 = load ptr, ptr %3, align 8
  store ptr %6, ptr %3, align 8
  %7 = load ptr, ptr %3, align 8
  store ptr %7, ptr %3, align 8
  br label %L179

L179:                                             ; preds = %L247, %L246
  %8 = load ptr, ptr %3, align 8
  ret ptr %8
}

define ptr @_CG_f_1803_31(i64 %0) {
entry:
  %1 = alloca i64, align 8
  %2 = alloca ptr, align 8
  store i64 %0, ptr %1, align 4
  %3 = load i64, ptr %1, align 4
  %4 = icmp slt i64 %3, 16
  br i1 %4, label %t_edge, label %f_edge

L248:                                             ; preds = %t_edge
  %5 = load i64, ptr %1, align 4
  %6 = call ptr @_CG_f_1760_29(i64 %5)
  %concat = call ptr @_CG_strcat(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.12, i32 0, i32 1, i32 0), ptr %6)
  store ptr %concat, ptr %2, align 8
  %7 = load ptr, ptr %2, align 8
  store ptr %7, ptr %2, align 8
  %8 = load ptr, ptr %2, align 8
  store ptr %8, ptr %2, align 8
  br label %L180

L249:                                             ; preds = %f_edge
  %9 = load i64, ptr %1, align 4
  %10 = call ptr @_CG_f_1760_29(i64 %9)
  store ptr %10, ptr %2, align 8
  %11 = load ptr, ptr %2, align 8
  store ptr %11, ptr %2, align 8
  %12 = load ptr, ptr %2, align 8
  store ptr %12, ptr %2, align 8
  br label %L180

L180:                                             ; preds = %L249, %L248
  %13 = load ptr, ptr %2, align 8
  ret ptr %13

t_edge:                                           ; preds = %entry
  %14 = load i64, ptr %1, align 4
  store i64 %14, ptr %1, align 4
  br label %L248

f_edge:                                           ; preds = %entry
  %15 = load i64, ptr %1, align 4
  store i64 %15, ptr %1, align 4
  br label %L249
}

define i64 @_CG_f_1854_32(ptr %0, i64 %1) {
entry:
  %2 = getelementptr i8, ptr %0, i64 %1
  %3 = load i8, ptr %2, align 1
  %4 = sext i8 %3 to i64
  br label %L185

L185:                                             ; preds = %entry
  ret i64 %4
}

define i8 @_CG_f_1869_33(ptr %0, i64 %1) {
entry:
  %2 = trunc i64 %1 to i8
  %3 = getelementptr i8, ptr %0, i64 2
  store i8 %2, ptr %3, align 1
  br label %L186

L186:                                             ; preds = %entry
  ret i8 0
}

define ptr @_CG_f_1903_34(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  %4 = alloca i64, align 8
  %5 = alloca i1, align 1
  store ptr %0, ptr %3, align 8
  %6 = load ptr, ptr %3, align 8
  %7 = call i64 @_CG_f_326_39(ptr %6)
  %8 = call ptr @_CG_f_3261_38(i64 0, i64 %7)
  store ptr %8, ptr %2, align 8
  %9 = load ptr, ptr %2, align 8
  store ptr %9, ptr %2, align 8
  store ptr getelementptr inbounds (<{ i64, [13 x i8] }>, ptr @.str.lit.20, i32 0, i32 1, i32 0), ptr %1, align 8
  %10 = load ptr, ptr %2, align 8
  store ptr %10, ptr %2, align 8
  %11 = load ptr, ptr %3, align 8
  store ptr %11, ptr %3, align 8
  br label %L190

L190:                                             ; preds = %L269, %entry
  %12 = load ptr, ptr %2, align 8
  %13 = call i1 @_CG_f_1699_21(ptr %12)
  br i1 %13, label %t_edge, label %f_edge

L270:                                             ; preds = %t_edge
  %14 = load ptr, ptr %2, align 8
  %15 = call i64 @_CG_f_1729_23(ptr %14)
  %16 = load ptr, ptr %3, align 8
  %17 = call i64 @_CG_f_1854_32(ptr %16, i64 %15)
  store i64 %17, ptr %4, align 4
  %18 = load i64, ptr %4, align 4
  store i64 %18, ptr %4, align 4
  %19 = load i64, ptr %4, align 4
  %20 = icmp eq i64 %19, 9
  %21 = call i1 @_CG_f_299_66(i1 %20)
  br i1 %21, label %t_edge1, label %f_edge2

L191:                                             ; preds = %f_edge
  %22 = load ptr, ptr %1, align 8
  %23 = load ptr, ptr %1, align 8
  %concat22 = call ptr @_CG_strcat(ptr %23, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.27, i32 0, i32 1, i32 0))
  br label %L189

L189:                                             ; preds = %L191
  ret ptr %concat22

L267:                                             ; preds = %t_edge1
  %24 = load ptr, ptr %1, align 8
  %25 = load ptr, ptr %1, align 8
  %concat = call ptr @_CG_strcat(ptr %25, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.21, i32 0, i32 1, i32 0))
  store ptr %concat, ptr %1, align 8
  %26 = load ptr, ptr %1, align 8
  store ptr %26, ptr %1, align 8
  %27 = load ptr, ptr %1, align 8
  store ptr %27, ptr %1, align 8
  br label %L269

L268:                                             ; preds = %f_edge2
  %28 = load i64, ptr %4, align 4
  %29 = icmp eq i64 %28, 10
  %30 = call i1 @_CG_f_299_66(i1 %29)
  br i1 %30, label %t_edge3, label %f_edge4

L264:                                             ; preds = %t_edge3
  %31 = load ptr, ptr %1, align 8
  %32 = load ptr, ptr %1, align 8
  %concat5 = call ptr @_CG_strcat(ptr %32, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.22, i32 0, i32 1, i32 0))
  store ptr %concat5, ptr %1, align 8
  %33 = load ptr, ptr %1, align 8
  store ptr %33, ptr %1, align 8
  %34 = load ptr, ptr %1, align 8
  store ptr %34, ptr %1, align 8
  br label %L266

L265:                                             ; preds = %f_edge4
  %35 = load i64, ptr %4, align 4
  %36 = icmp eq i64 %35, 13
  %37 = call i1 @_CG_f_299_66(i1 %36)
  br i1 %37, label %t_edge6, label %f_edge7

L261:                                             ; preds = %t_edge6
  %38 = load ptr, ptr %1, align 8
  %39 = load ptr, ptr %1, align 8
  %concat8 = call ptr @_CG_strcat(ptr %39, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.23, i32 0, i32 1, i32 0))
  store ptr %concat8, ptr %1, align 8
  %40 = load ptr, ptr %1, align 8
  store ptr %40, ptr %1, align 8
  %41 = load ptr, ptr %1, align 8
  store ptr %41, ptr %1, align 8
  br label %L263

L262:                                             ; preds = %f_edge7
  %42 = load i64, ptr %4, align 4
  %43 = icmp eq i64 %42, 92
  %44 = call i1 @_CG_f_299_66(i1 %43)
  br i1 %44, label %t_edge9, label %f_edge10

L258:                                             ; preds = %t_edge9
  %45 = load ptr, ptr %1, align 8
  %46 = load ptr, ptr %1, align 8
  %concat11 = call ptr @_CG_strcat(ptr %46, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.24, i32 0, i32 1, i32 0))
  store ptr %concat11, ptr %1, align 8
  %47 = load ptr, ptr %1, align 8
  store ptr %47, ptr %1, align 8
  %48 = load ptr, ptr %1, align 8
  store ptr %48, ptr %1, align 8
  br label %L260

L259:                                             ; preds = %f_edge10
  %49 = load i64, ptr %4, align 4
  %50 = icmp eq i64 %49, 39
  %51 = call i1 @_CG_f_299_66(i1 %50)
  br i1 %51, label %t_edge12, label %f_edge13

L255:                                             ; preds = %t_edge12
  %52 = load ptr, ptr %1, align 8
  %53 = load ptr, ptr %1, align 8
  %concat14 = call ptr @_CG_strcat(ptr %53, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.25, i32 0, i32 1, i32 0))
  store ptr %concat14, ptr %1, align 8
  %54 = load ptr, ptr %1, align 8
  store ptr %54, ptr %1, align 8
  %55 = load ptr, ptr %1, align 8
  store ptr %55, ptr %1, align 8
  br label %L257

L256:                                             ; preds = %f_edge13
  %56 = load i64, ptr %4, align 4
  %57 = icmp sge i64 %56, 32
  store i1 %57, ptr %5, align 1
  %58 = load i1, ptr %5, align 1
  store i1 %58, ptr %5, align 1
  %59 = load i1, ptr %5, align 1
  %60 = call i1 @_CG_f_299_66(i1 %59)
  br i1 %60, label %t_edge15, label %f_edge16

L251:                                             ; preds = %t_edge15
  %61 = load i64, ptr %4, align 4
  %62 = icmp slt i64 %61, 127
  store i1 %62, ptr %5, align 1
  %63 = load i1, ptr %5, align 1
  store i1 %63, ptr %5, align 1
  %64 = load i1, ptr %5, align 1
  store i1 %64, ptr %5, align 1
  %65 = load i64, ptr %4, align 4
  store i64 %65, ptr %4, align 4
  br label %L250

L250:                                             ; preds = %L251, %f_edge16
  %66 = load i1, ptr %5, align 1
  %67 = call i1 @_CG_f_299_66(i1 %66)
  br i1 %67, label %t_edge17, label %f_edge18

L252:                                             ; preds = %t_edge17
  %68 = load i64, ptr %4, align 4
  %69 = call ptr @_CG_f_1748_27(i64 %68)
  %70 = load ptr, ptr %1, align 8
  %71 = load ptr, ptr %1, align 8
  %concat19 = call ptr @_CG_strcat(ptr %71, ptr %69)
  store ptr %concat19, ptr %1, align 8
  %72 = load ptr, ptr %1, align 8
  store ptr %72, ptr %1, align 8
  %73 = load ptr, ptr %1, align 8
  store ptr %73, ptr %1, align 8
  br label %L254

L253:                                             ; preds = %f_edge18
  %74 = load i64, ptr %4, align 4
  %75 = call ptr @_CG_f_1803_31(i64 %74)
  %concat20 = call ptr @_CG_strcat(ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.26, i32 0, i32 1, i32 0), ptr %75)
  %76 = load ptr, ptr %1, align 8
  %77 = load ptr, ptr %1, align 8
  %concat21 = call ptr @_CG_strcat(ptr %77, ptr %concat20)
  store ptr %concat21, ptr %1, align 8
  %78 = load ptr, ptr %1, align 8
  store ptr %78, ptr %1, align 8
  %79 = load ptr, ptr %1, align 8
  store ptr %79, ptr %1, align 8
  br label %L254

L254:                                             ; preds = %L253, %L252
  %80 = load ptr, ptr %1, align 8
  store ptr %80, ptr %1, align 8
  br label %L257

L257:                                             ; preds = %L254, %L255
  %81 = load ptr, ptr %1, align 8
  store ptr %81, ptr %1, align 8
  br label %L260

L260:                                             ; preds = %L257, %L258
  %82 = load ptr, ptr %1, align 8
  store ptr %82, ptr %1, align 8
  br label %L263

L263:                                             ; preds = %L260, %L261
  %83 = load ptr, ptr %1, align 8
  store ptr %83, ptr %1, align 8
  br label %L266

L266:                                             ; preds = %L263, %L264
  %84 = load ptr, ptr %1, align 8
  store ptr %84, ptr %1, align 8
  br label %L269

L269:                                             ; preds = %L266, %L267
  %85 = load ptr, ptr %1, align 8
  store ptr %85, ptr %1, align 8
  %86 = load ptr, ptr %2, align 8
  store ptr %86, ptr %2, align 8
  %87 = load ptr, ptr %3, align 8
  store ptr %87, ptr %3, align 8
  br label %L190

t_edge:                                           ; preds = %L190
  %88 = load ptr, ptr %1, align 8
  store ptr %88, ptr %1, align 8
  %89 = load ptr, ptr %2, align 8
  store ptr %89, ptr %2, align 8
  %90 = load ptr, ptr %3, align 8
  store ptr %90, ptr %3, align 8
  br label %L270

f_edge:                                           ; preds = %L190
  %91 = load ptr, ptr %1, align 8
  store ptr %91, ptr %1, align 8
  %92 = load ptr, ptr %2, align 8
  store ptr %92, ptr %2, align 8
  %93 = load ptr, ptr %3, align 8
  store ptr %93, ptr %3, align 8
  br label %L191

t_edge1:                                          ; preds = %L270
  %94 = load ptr, ptr %1, align 8
  store ptr %94, ptr %1, align 8
  br label %L267

f_edge2:                                          ; preds = %L270
  %95 = load ptr, ptr %1, align 8
  store ptr %95, ptr %1, align 8
  %96 = load i64, ptr %4, align 4
  store i64 %96, ptr %4, align 4
  br label %L268

t_edge3:                                          ; preds = %L268
  %97 = load ptr, ptr %1, align 8
  store ptr %97, ptr %1, align 8
  br label %L264

f_edge4:                                          ; preds = %L268
  %98 = load ptr, ptr %1, align 8
  store ptr %98, ptr %1, align 8
  %99 = load i64, ptr %4, align 4
  store i64 %99, ptr %4, align 4
  br label %L265

t_edge6:                                          ; preds = %L265
  %100 = load ptr, ptr %1, align 8
  store ptr %100, ptr %1, align 8
  br label %L261

f_edge7:                                          ; preds = %L265
  %101 = load ptr, ptr %1, align 8
  store ptr %101, ptr %1, align 8
  %102 = load i64, ptr %4, align 4
  store i64 %102, ptr %4, align 4
  br label %L262

t_edge9:                                          ; preds = %L262
  %103 = load ptr, ptr %1, align 8
  store ptr %103, ptr %1, align 8
  br label %L258

f_edge10:                                         ; preds = %L262
  %104 = load ptr, ptr %1, align 8
  store ptr %104, ptr %1, align 8
  %105 = load i64, ptr %4, align 4
  store i64 %105, ptr %4, align 4
  br label %L259

t_edge12:                                         ; preds = %L259
  %106 = load ptr, ptr %1, align 8
  store ptr %106, ptr %1, align 8
  br label %L255

f_edge13:                                         ; preds = %L259
  %107 = load ptr, ptr %1, align 8
  store ptr %107, ptr %1, align 8
  %108 = load i64, ptr %4, align 4
  store i64 %108, ptr %4, align 4
  br label %L256

t_edge15:                                         ; preds = %L256
  %109 = load i64, ptr %4, align 4
  store i64 %109, ptr %4, align 4
  br label %L251

f_edge16:                                         ; preds = %L256
  %110 = load i64, ptr %4, align 4
  store i64 %110, ptr %4, align 4
  %111 = load i1, ptr %5, align 1
  store i1 %111, ptr %5, align 1
  %112 = load i1, ptr %5, align 1
  store i1 %112, ptr %5, align 1
  %113 = load i64, ptr %4, align 4
  store i64 %113, ptr %4, align 4
  br label %L250

t_edge17:                                         ; preds = %L250
  %114 = load ptr, ptr %1, align 8
  store ptr %114, ptr %1, align 8
  %115 = load i64, ptr %4, align 4
  store i64 %115, ptr %4, align 4
  br label %L252

f_edge18:                                         ; preds = %L250
  %116 = load ptr, ptr %1, align 8
  store ptr %116, ptr %1, align 8
  %117 = load i64, ptr %4, align 4
  store i64 %117, ptr %4, align 4
  br label %L253
}

define ptr @_CG_f_1839_35(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %bytearray.3300, ptr %0, i32 0, i32 6
  store i64 0, ptr %1, align 4
  %g = load ptr, ptr @g2143, align 8
  %g1 = load ptr, ptr @g2143, align 8
  %g2 = load ptr, ptr @g2143, align 8
  %g3 = load ptr, ptr @g2143, align 8
  %g4 = load ptr, ptr @g2143, align 8
  %g5 = load ptr, ptr @g2143, align 8
  %g6 = load ptr, ptr @g2143, align 8
  %g7 = load ptr, ptr @g2143, align 8
  ret ptr %0
}

define ptr @_CG_f_3020_36(i64 %0) {
entry:
  %g = load ptr, ptr @g2143, align 8
  %clone_v = call ptr @_CG_prim_clone_vector_runtime(ptr %g, i64 56, i64 10)
  %1 = getelementptr inbounds nuw %bytearray.3298, ptr %clone_v, i32 0, i32 6
  store i64 10, ptr %1, align 4
  ret ptr %clone_v
}

define ptr @_CG_f_105_37() {
main_prelude:
  %proto = call ptr @GC_malloc(i64 56)
  store ptr %proto, ptr @range, align 8
  %proto21 = call ptr @GC_malloc(i64 56)
  store ptr %proto21, ptr @x, align 8
  %proto22 = call ptr @GC_malloc(i64 40)
  store ptr %proto22, ptr @__list_iter__, align 8
  %proto23 = call ptr @GC_malloc(i64 56)
  store ptr %proto23, ptr @bytearray, align 8
  %proto24 = call ptr @GC_malloc(i64 56)
  store ptr %proto24, ptr @g1756, align 8
  %proto25 = call ptr @GC_malloc(i64 56)
  store ptr %proto25, ptr @g2143, align 8
  %proto26 = call ptr @GC_malloc(i64 40)
  store ptr %proto26, ptr @g1063, align 8
  br label %entry

entry:                                            ; preds = %main_prelude
  %new = call ptr @GC_malloc(i64 40)
  %g = load ptr, ptr @g1063, align 8
  %0 = call ptr @_CG_f_1216_12(ptr %g)
  %new1 = call ptr @GC_malloc(i64 56)
  %g2 = load ptr, ptr @g1756, align 8
  %1 = call ptr @_CG_f_1667_24(ptr %g2)
  %new3 = call ptr @GC_malloc(i64 56)
  %g4 = load ptr, ptr @g2143, align 8
  %2 = call ptr @_CG_f_1839_35(ptr %g4)
  %3 = call ptr @_CG_f_1748_27(i64 65)
  %4 = call ptr @_CG_write(ptr %3)
  %5 = call ptr @_CG_writeln()
  %6 = call i64 @_CG_f_1754_28(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.28, i32 0, i32 1, i32 0))
  %7 = call ptr @_CG_f_882_47(i64 %6)
  %8 = call ptr @_CG_write(ptr %7)
  %9 = call ptr @_CG_writeln()
  %10 = call ptr @_CG_f_1789_30(i64 23165)
  %11 = call ptr @_CG_write(ptr %10)
  %12 = call ptr @_CG_writeln()
  %13 = call ptr @_CG_f_882_6()
  %14 = call ptr @_CG_write(ptr %13)
  %15 = call ptr @_CG_writeln()
  %tmp = call ptr @_CG_prim_tuple_list_internal(i32 24, i32 4)
  %16 = getelementptr inbounds nuw %list.3306, ptr %tmp, i32 0, i32 0
  store i64 1, ptr %16, align 4
  %17 = getelementptr inbounds nuw %list.3306, ptr %tmp, i32 0, i32 1
  store i64 2, ptr %17, align 4
  %18 = getelementptr inbounds nuw %list.3306, ptr %tmp, i32 0, i32 2
  store i64 3, ptr %18, align 4
  %dst = call ptr @_CG_to_list_runtime(ptr %tmp, i32 24, i32 3)
  %19 = call i1 @_CG_f_1620_45(ptr %dst)
  %20 = call ptr @_CG_f_293_0(i1 %19)
  %21 = call ptr @_CG_write(ptr %20)
  %22 = call ptr @_CG_writeln()
  %tmp5 = call ptr @_CG_prim_tuple_list_internal(i32 24, i32 4)
  %23 = getelementptr inbounds nuw %list.3306, ptr %tmp5, i32 0, i32 0
  store i64 1, ptr %23, align 4
  %24 = getelementptr inbounds nuw %list.3306, ptr %tmp5, i32 0, i32 1
  store i64 0, ptr %24, align 4
  %25 = getelementptr inbounds nuw %list.3306, ptr %tmp5, i32 0, i32 2
  store i64 3, ptr %25, align 4
  %dst6 = call ptr @_CG_to_list_runtime(ptr %tmp5, i32 24, i32 3)
  %26 = call i1 @_CG_f_1620_17(ptr %dst6)
  %27 = call ptr @_CG_f_293_0(i1 %26)
  %28 = call ptr @_CG_write(ptr %27)
  %29 = call ptr @_CG_writeln()
  %tmp7 = call ptr @_CG_prim_tuple_list_internal(i32 24, i32 4)
  %30 = getelementptr inbounds nuw %list.3306, ptr %tmp7, i32 0, i32 0
  store i64 0, ptr %30, align 4
  %31 = getelementptr inbounds nuw %list.3306, ptr %tmp7, i32 0, i32 1
  store i64 2, ptr %31, align 4
  %32 = getelementptr inbounds nuw %list.3306, ptr %tmp7, i32 0, i32 2
  store i64 0, ptr %32, align 4
  %dst8 = call ptr @_CG_to_list_runtime(ptr %tmp7, i32 24, i32 3)
  %33 = call i1 @_CG_f_1627_46(ptr %dst8)
  %34 = call ptr @_CG_f_293_0(i1 %33)
  %35 = call ptr @_CG_write(ptr %34)
  %36 = call ptr @_CG_writeln()
  %tmp9 = call ptr @_CG_prim_tuple_list_internal(i32 24, i32 4)
  %37 = getelementptr inbounds nuw %list.3306, ptr %tmp9, i32 0, i32 0
  store i64 0, ptr %37, align 4
  %38 = getelementptr inbounds nuw %list.3306, ptr %tmp9, i32 0, i32 1
  store i64 0, ptr %38, align 4
  %39 = getelementptr inbounds nuw %list.3306, ptr %tmp9, i32 0, i32 2
  store i64 0, ptr %39, align 4
  %dst10 = call ptr @_CG_to_list_runtime(ptr %tmp9, i32 24, i32 3)
  %40 = call i1 @_CG_f_1627_18(ptr %dst10)
  %41 = call ptr @_CG_f_293_0(i1 false)
  %42 = call ptr @_CG_write(ptr %41)
  %43 = call ptr @_CG_writeln()
  %44 = call ptr @_CG_f_293_0(i1 true)
  %45 = call ptr @_CG_write(ptr %44)
  %46 = call ptr @_CG_writeln()
  %47 = call ptr @_CG_f_293_0(i1 false)
  %48 = call ptr @_CG_write(ptr %47)
  %49 = call ptr @_CG_writeln()
  %50 = call ptr @_CG_f_293_0(i1 true)
  %51 = call ptr @_CG_write(ptr %50)
  %52 = call ptr @_CG_writeln()
  %53 = call ptr @_CG_f_293_0(i1 true)
  %54 = call ptr @_CG_write(ptr %53)
  %55 = call ptr @_CG_writeln()
  %56 = call ptr @_CG_f_293_0(i1 true)
  %57 = call ptr @_CG_write(ptr %56)
  %58 = call ptr @_CG_writeln()
  %59 = call ptr @_CG_f_293_0(i1 true)
  %60 = call ptr @_CG_write(ptr %59)
  %61 = call ptr @_CG_writeln()
  %62 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [14 x i8] }>, ptr @.str.lit.29, i32 0, i32 1, i32 0))
  %63 = call ptr @_CG_writeln()
  %64 = call ptr @_CG_f_1633_19(i64 -1)
  %65 = call ptr @_CG_write(ptr %64)
  %66 = call ptr @_CG_writeln()
  %67 = call ptr @_CG_f_1633_19(i64 10)
  %68 = call ptr @_CG_write(ptr %67)
  %69 = call ptr @_CG_writeln()
  %70 = call i1 @_CG_f_2234_2(i64 10)
  %71 = call ptr @_CG_f_293_0(i1 %70)
  %72 = call ptr @_CG_write(ptr %71)
  %73 = call ptr @_CG_writeln()
  %74 = call i1 @_CG_f_2234_2(i64 0)
  %75 = call ptr @_CG_f_293_0(i1 %74)
  %76 = call ptr @_CG_write(ptr %75)
  %77 = call ptr @_CG_writeln()
  %78 = call i64 @_CG_f_2466_7()
  %79 = call ptr @_CG_f_882_47(i64 %78)
  %80 = call ptr @_CG_write(ptr %79)
  %81 = call ptr @_CG_writeln()
  %82 = call double @_CG_f_2539_9()
  %83 = call ptr @_CG_f_1125_8(double %82)
  %84 = call ptr @_CG_write(ptr %83)
  %85 = call ptr @_CG_writeln()
  %86 = call ptr @_CG_f_3020_36(i64 10)
  store ptr %86, ptr @x, align 8
  %87 = call i64 @_CG_f_1754_28(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.4, i32 0, i32 1, i32 0))
  %g11 = load ptr, ptr @x, align 8
  %88 = call i8 @_CG_f_1869_62(ptr %g11, i64 %87)
  %89 = call i64 @_CG_f_1754_28(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.30, i32 0, i32 1, i32 0))
  %g12 = load ptr, ptr @x, align 8
  %90 = call i8 @_CG_f_1869_33(ptr %g12, i64 %89)
  %g13 = load ptr, ptr @x, align 8
  %91 = call ptr @_CG_f_1903_34(ptr %g13)
  %92 = call ptr @_CG_write(ptr %91)
  %93 = call ptr @_CG_writeln()
  %94 = call ptr @_CG_f_415_5()
  %95 = call ptr @_CG_write(ptr %94)
  %96 = call ptr @_CG_writeln()
  %tmp14 = call ptr @_CG_prim_tuple_list_internal(i32 8, i32 2)
  %97 = getelementptr inbounds nuw %list.3296, ptr %tmp14, i32 0, i32 0
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0), ptr %97, align 8
  %dst15 = call ptr @_CG_to_list_runtime(ptr %tmp14, i32 8, i32 1)
  %98 = call i64 @_CG_f_326_26(ptr getelementptr inbounds (<{ i64, [7 x i8] }>, ptr @.str.lit.16, i32 0, i32 1, i32 0))
  %99 = call ptr @_CG_f_1380_15(ptr %dst15, i64 %98)
  store ptr %99, ptr @buffer, align 8
  %g16 = load ptr, ptr @buffer, align 8
  %100 = call ptr @_CG_f_1486_16(ptr %g16)
  %101 = call ptr @_CG_write(ptr %100)
  %102 = call ptr @_CG_writeln()
  %103 = call ptr @_CG_f_3020_36(i64 10)
  store ptr %103, ptr @x, align 8
  %g17 = load ptr, ptr @x, align 8
  %104 = call i8 @_CG_f_1869_62(ptr %g17, i64 44)
  %g18 = load ptr, ptr @x, align 8
  %105 = call i64 @_CG_f_326_39(ptr %g18)
  %106 = call ptr @_CG_f_882_47(i64 %105)
  %107 = call ptr @_CG_write(ptr %106)
  %108 = call ptr @_CG_writeln()
  %g19 = load ptr, ptr @x, align 8
  %109 = call ptr @_CG_f_1903_34(ptr %g19)
  %110 = call ptr @_CG_write(ptr %109)
  %111 = call ptr @_CG_writeln()
  %g20 = load ptr, ptr @None, align 8
  ret ptr %g20
}

define ptr @_CG_f_3261_38(i64 %0, i64 %1) {
entry:
  %2 = call ptr @_CG_f_2867_25(i64 0, i64 %1, i64 1)
  ret ptr %2
}

define i64 @_CG_f_326_39(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %bytearray.3298, ptr %0, i32 0, i32 6
  %2 = load i64, ptr %1, align 4
  br label %L175

L175:                                             ; preds = %entry
  ret i64 %2
}

define i64 @_CG_f_326_40(ptr %0) {
entry:
  br label %L175

L175:                                             ; preds = %entry
  ret i64 3
}

define i64 @_CG_f_326_41(ptr %0) {
entry:
  br label %L175

L175:                                             ; preds = %entry
  ret i64 3
}

define i64 @_CG_f_326_42(ptr %0) {
entry:
  %1 = getelementptr i8, ptr %0, i64 -16
  %len = load i64, ptr %1, align 4
  br label %L175

L175:                                             ; preds = %entry
  ret i64 %len
}

define i64 @_CG_f_326_43(ptr %0) {
entry:
  br label %L175

L175:                                             ; preds = %entry
  ret i64 3
}

define i64 @_CG_f_326_44(ptr %0) {
entry:
  br label %L175

L175:                                             ; preds = %entry
  ret i64 3
}

define i1 @_CG_f_1620_45(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i1, align 1
  %3 = call ptr @_CG_f_1336_59(ptr %0)
  store ptr %3, ptr %1, align 8
  %4 = load ptr, ptr %1, align 8
  store ptr %4, ptr %1, align 8
  br label %L160

L160:                                             ; preds = %L223, %entry
  %5 = load ptr, ptr %1, align 8
  %6 = call i1 @_CG_f_1232_50(ptr %5)
  br i1 %6, label %t_edge, label %f_edge

L224:                                             ; preds = %t_edge
  %7 = load ptr, ptr %1, align 8
  %8 = call i64 @_CG_f_1246_53(ptr %7)
  %9 = icmp eq i64 %8, 0
  %10 = call i1 @_CG_f_299_64(i1 %9)
  br i1 %10, label %t_edge1, label %f_edge2

L161:                                             ; preds = %f_edge
  store i1 false, ptr %2, align 1
  br label %L159

L159:                                             ; preds = %L161, %L222
  %11 = load i1, ptr %2, align 1
  ret i1 %11

L222:                                             ; preds = %t_edge1
  store i1 false, ptr %2, align 1
  br label %L159

L223:                                             ; preds = %f_edge2
  %12 = load ptr, ptr %1, align 8
  store ptr %12, ptr %1, align 8
  br label %L160

t_edge:                                           ; preds = %L160
  %13 = load ptr, ptr %1, align 8
  store ptr %13, ptr %1, align 8
  br label %L224

f_edge:                                           ; preds = %L160
  %14 = load ptr, ptr %1, align 8
  store ptr %14, ptr %1, align 8
  br label %L161

t_edge1:                                          ; preds = %L224
  %15 = load ptr, ptr %1, align 8
  store ptr %15, ptr %1, align 8
  br label %L222

f_edge2:                                          ; preds = %L224
  %16 = load ptr, ptr %1, align 8
  store ptr %16, ptr %1, align 8
  br label %L223
}

define i1 @_CG_f_1627_46(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i1, align 1
  %3 = call ptr @_CG_f_1336_60(ptr %0)
  store ptr %3, ptr %1, align 8
  %4 = load ptr, ptr %1, align 8
  store ptr %4, ptr %1, align 8
  br label %L163

L163:                                             ; preds = %L226, %entry
  %5 = load ptr, ptr %1, align 8
  %6 = call i1 @_CG_f_1232_51(ptr %5)
  br i1 %6, label %t_edge, label %f_edge

L227:                                             ; preds = %t_edge
  %7 = load ptr, ptr %1, align 8
  %8 = call i64 @_CG_f_1246_54(ptr %7)
  %9 = call i1 @_CG_f_890_48(i64 %8)
  br i1 %9, label %t_edge1, label %f_edge2

L164:                                             ; preds = %f_edge
  store i1 true, ptr %2, align 1
  br label %L162

L162:                                             ; preds = %L164, %L225
  %10 = load i1, ptr %2, align 1
  ret i1 %10

L225:                                             ; preds = %t_edge1
  store i1 true, ptr %2, align 1
  br label %L162

L226:                                             ; preds = %f_edge2
  %11 = load ptr, ptr %1, align 8
  store ptr %11, ptr %1, align 8
  br label %L163

t_edge:                                           ; preds = %L163
  %12 = load ptr, ptr %1, align 8
  store ptr %12, ptr %1, align 8
  br label %L227

f_edge:                                           ; preds = %L163
  %13 = load ptr, ptr %1, align 8
  store ptr %13, ptr %1, align 8
  br label %L164

t_edge1:                                          ; preds = %L227
  %14 = load ptr, ptr %1, align 8
  store ptr %14, ptr %1, align 8
  br label %L225

f_edge2:                                          ; preds = %L227
  %15 = load ptr, ptr %1, align 8
  store ptr %15, ptr %1, align 8
  br label %L226
}

define ptr @_CG_f_882_47(i64 %0) {
entry:
  %1 = call ptr @_CG_str_from_int(i64 %0)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %1
}

define i1 @_CG_f_890_48(i64 %0) {
entry:
  %1 = icmp ne i64 %0, 0
  br label %L81

L81:                                              ; preds = %entry
  ret i1 %1
}

define i1 @_CG_f_890_49(i64 %0) {
entry:
  %1 = icmp ne i64 %0, 0
  br label %L81

L81:                                              ; preds = %entry
  ret i1 %1
}

define i1 @_CG_f_1232_50(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_40(ptr %4)
  %6 = icmp slt i64 %2, 3
  br label %L120

L120:                                             ; preds = %entry
  ret i1 %6
}

define i1 @_CG_f_1232_51(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_41(ptr %4)
  %6 = icmp slt i64 %2, 3
  br label %L120

L120:                                             ; preds = %entry
  ret i1 %6
}

define i1 @_CG_f_1232_52(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %4 = load ptr, ptr %3, align 8
  %5 = call i64 @_CG_f_326_43(ptr %4)
  %6 = icmp slt i64 %2, 3
  br label %L120

L120:                                             ; preds = %entry
  ret i1 %6
}

define i64 @_CG_f_1246_53(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = add i64 %2, 1
  %4 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  store i64 %3, ptr %4, align 4
  %5 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %8 = load i64, ptr %7, align 4
  %9 = sub i64 %8, 1
  %10 = getelementptr i64, ptr %6, i64 %9
  %11 = load i64, ptr %10, align 4
  br label %L121

L121:                                             ; preds = %entry
  ret i64 %11
}

define i64 @_CG_f_1246_54(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = add i64 %2, 1
  %4 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  store i64 %3, ptr %4, align 4
  %5 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %8 = load i64, ptr %7, align 4
  %9 = sub i64 %8, 1
  %10 = getelementptr i64, ptr %6, i64 %9
  %11 = load i64, ptr %10, align 4
  br label %L121

L121:                                             ; preds = %entry
  ret i64 %11
}

define i64 @_CG_f_1246_55(ptr %0) {
entry:
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %2 = load i64, ptr %1, align 4
  %3 = add i64 %2, 1
  %4 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  store i64 %3, ptr %4, align 4
  %5 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 4
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr inbounds nuw %__list_iter__.1214, ptr %0, i32 0, i32 3
  %8 = load i64, ptr %7, align 4
  %9 = sub i64 %8, 1
  %10 = getelementptr i64, ptr %6, i64 %9
  %11 = load i64, ptr %10, align 4
  br label %L121

L121:                                             ; preds = %entry
  ret i64 %11
}

define ptr @_CG_f_2612_56(ptr %0) {
entry:
  %g = load ptr, ptr @g1063, align 8
  %clone = call ptr @GC_malloc(i64 40)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 40, i1 false)
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %clone, i32 0, i32 4
  store ptr %0, ptr %1, align 8
  ret ptr %clone
}

define ptr @_CG_f_2612_57(ptr %0) {
entry:
  %g = load ptr, ptr @g1063, align 8
  %clone = call ptr @GC_malloc(i64 40)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 40, i1 false)
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %clone, i32 0, i32 4
  store ptr %0, ptr %1, align 8
  ret ptr %clone
}

define ptr @_CG_f_2612_58(ptr %0) {
entry:
  %g = load ptr, ptr @g1063, align 8
  %clone = call ptr @GC_malloc(i64 40)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 40, i1 false)
  %1 = getelementptr inbounds nuw %__list_iter__.1214, ptr %clone, i32 0, i32 4
  store ptr %0, ptr %1, align 8
  ret ptr %clone
}

define ptr @_CG_f_1336_59(ptr %0) {
entry:
  %1 = call ptr @_CG_f_2612_56(ptr %0)
  br label %L129

L129:                                             ; preds = %entry
  ret ptr %1
}

define ptr @_CG_f_1336_60(ptr %0) {
entry:
  %1 = call ptr @_CG_f_2612_57(ptr %0)
  br label %L129

L129:                                             ; preds = %entry
  ret ptr %1
}

define ptr @_CG_f_1336_61(ptr %0) {
entry:
  %1 = call ptr @_CG_f_2612_58(ptr %0)
  br label %L129

L129:                                             ; preds = %entry
  ret ptr %1
}

define i8 @_CG_f_1869_62(ptr %0, i64 %1) {
entry:
  %2 = trunc i64 %1 to i8
  %3 = getelementptr i8, ptr %0, i64 1
  store i8 %2, ptr %3, align 1
  br label %L186

L186:                                             ; preds = %entry
  ret i8 0
}

define i1 @_CG_f_299_63(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

define i1 @_CG_f_299_64(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

define i1 @_CG_f_299_65(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

define i1 @_CG_f_299_66(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

define i1 @_CG_f_299_67(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

declare ptr @_CG_strcat(ptr, ptr)

declare ptr @_CG_string_mult(ptr, i64)

declare ptr @_CG_str_from_int(i64)

declare ptr @_CG_str_from_float(double)

declare ptr @GC_malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

declare ptr @_CG_list_mult(ptr, i64, i64)

declare ptr @_CG_chr(i64)

declare i64 @_CG_ord(ptr)

declare ptr @_CG_prim_clone_vector_runtime(ptr, i64, i64)

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

declare ptr @_CG_prim_tuple_list_internal(i32, i32)

declare ptr @_CG_to_list_runtime(ptr, i32, i32)

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_37()
  ret i32 0
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "builtins.py", directory: "tests")
