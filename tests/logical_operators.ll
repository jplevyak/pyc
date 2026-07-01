; ModuleID = 'ifa_output'
source_filename = "logical_operators.py"
target triple = "arm64-apple-darwin25.5.0"

%closure_closure.3363 = type { ptr, ptr }
%range.1437 = type { ptr, ptr, ptr, ptr, i64, i64, i64 }
%list.3368 = type { i64, i64, i64 }

@__primitive = internal global ptr null
@reply = internal global ptr null
@".=" = internal global ptr null
@__pyc_to_bool__ = internal global ptr null
@__pyc_c_call__ = internal global ptr null
@__str__ = internal global ptr null
@i = internal global ptr null
@c = internal global ptr null
@__repr__ = internal global ptr null
@"!" = internal global ptr null
@write = internal global ptr null
@__len__ = internal global ptr null
@. = internal global ptr null
@"::" = internal global ptr null
@str = internal global ptr null
@writeln = internal global ptr null
@__getitem__ = internal global ptr null
@int = internal global ptr null
@x = internal global ptr null
@j = internal global ptr null
@index_object = internal global ptr null
@__not__ = internal global ptr null
@__operator = internal global ptr null
@__iter__ = internal global ptr null
@range = internal global ptr null
@new = internal global ptr null
@s = internal global ptr null
@"+" = internal global ptr null
@len = internal global ptr null
@g = internal global i64 0
@__gt__ = internal global ptr null
@__pyc_more__ = internal global ptr null
@float = internal global ptr null
@list = internal global ptr null
@__ge__ = internal global ptr null
@__next__ = internal global ptr null
@"!=" = internal global ptr null
@range.1 = internal global ptr null
@g.2 = internal global ptr null
@"<" = internal global ptr null
@clone = internal global ptr null
@__init__ = internal global ptr null
@make = internal global ptr null
@.str.lit = private constant <{ i64, [5 x i8] }> <{ i64 4, [5 x i8] c"True\00" }>
@.str.lit.3 = private constant <{ i64, [5 x i8] }> <{ i64 4, [5 x i8] c"True\00" }>
@.str.lit.4 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>
@.str.lit.5 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>
@.str.lit.6 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"[\00" }>
@.str.lit.7 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"[\00" }>
@.str.lit.8 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c"]\00" }>
@.str.lit.9 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c", \00" }>
@.str.lit.10 = private constant <{ i64, [9 x i8] }> <{ i64 8, [9 x i8] c"and true\00" }>
@.str.lit.11 = private constant <{ i64, [10 x i8] }> <{ i64 9, [10 x i8] c"and false\00" }>
@.str.lit.12 = private constant <{ i64, [8 x i8] }> <{ i64 7, [8 x i8] c"or true\00" }>
@.str.lit.13 = private constant <{ i64, [9 x i8] }> <{ i64 8, [9 x i8] c"or false\00" }>
@.str.lit.14 = private constant <{ i64, [11 x i8] }> <{ i64 10, [11 x i8] c"and true *\00" }>
@.str.lit.15 = private constant <{ i64, [10 x i8] }> <{ i64 9, [10 x i8] c"or true *\00" }>
@.str.lit.16 = private constant <{ i64, [12 x i8] }> <{ i64 11, [12 x i8] c"and false *\00" }>
@.str.lit.17 = private constant <{ i64, [11 x i8] }> <{ i64 10, [11 x i8] c"or false *\00" }>
@.str.lit.18 = private constant <{ i64, [12 x i8] }> <{ i64 11, [12 x i8] c"and false *\00" }>
@.str.lit.19 = private constant <{ i64, [10 x i8] }> <{ i64 9, [10 x i8] c"or true *\00" }>
@.str.lit.20 = private constant <{ i64, [12 x i8] }> <{ i64 11, [12 x i8] c"and false *\00" }>
@.str.lit.21 = private constant <{ i64, [10 x i8] }> <{ i64 9, [10 x i8] c"or true *\00" }>
@.str.lit.22 = private constant <{ i64, [12 x i8] }> <{ i64 11, [12 x i8] c"and false *\00" }>
@.str.lit.23 = private constant <{ i64, [10 x i8] }> <{ i64 9, [10 x i8] c"or true *\00" }>
@.str.lit.24 = private constant <{ i64, [12 x i8] }> <{ i64 11, [12 x i8] c"and false *\00" }>
@.str.lit.25 = private constant <{ i64, [10 x i8] }> <{ i64 9, [10 x i8] c"or true *\00" }>

define internal ptr @_CG_f_154_0(i64 %self) {
entry:
  %v = call ptr @_CG_f_882_17(i64 %self)
  br label %L5

L5:                                               ; preds = %entry
  ret ptr %v
}

define internal i1 @_CG_f_287_1(i1 %self) {
entry:
  %v = alloca i1, align 1
  %v1 = call i1 @_CG_f_299_23(i1 %self)
  br i1 %v1, label %L202, label %L203

L202:                                             ; preds = %entry
  store i1 false, ptr %v, align 1
  br label %L25

L203:                                             ; preds = %entry
  store i1 true, ptr %v, align 1
  br label %L25

L25:                                              ; preds = %L203, %L202
  %v2 = load i1, ptr %v, align 1
  ret i1 %v2
}

define internal ptr @_CG_f_293_2(i1 %self) {
entry:
  %v = alloca ptr, align 8
  %v1 = call i1 @_CG_f_299_23(i1 %self)
  br i1 %v1, label %L205, label %L206

L205:                                             ; preds = %entry
  store ptr getelementptr inbounds (<{ i64, [5 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0), ptr %v, align 8
  br label %L26

L206:                                             ; preds = %entry
  store ptr getelementptr inbounds (<{ i64, [6 x i8] }>, ptr @.str.lit.5, i32 0, i32 1, i32 0), ptr %v, align 8
  br label %L26

L26:                                              ; preds = %L206, %L205
  %v2 = load ptr, ptr %v, align 8
  ret ptr %v2
}

define internal ptr @_CG_f_882_3() {
entry:
  %v = call ptr @_CG_str_from_int(i64 3)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_1125_4(double %self) {
entry:
  %v = call ptr @_CG_str_from_float(double %self)
  br label %L109

L109:                                             ; preds = %entry
  ret ptr %v
}

define internal i1 @_CG_f_1131_5(double %self) {
entry:
  %v = fcmp one double %self, 0.000000e+00
  br label %L110

L110:                                             ; preds = %entry
  ret i1 %v
}

define internal ptr @_CG_f_1486_6(ptr %self) {
entry:
  %x = alloca ptr, align 8
  %v = alloca ptr, align 8
  %self1 = alloca ptr, align 8
  %x2 = alloca ptr, align 8
  %v3 = alloca i1, align 1
  %v4 = alloca ptr, align 8
  %self5 = alloca ptr, align 8
  %x6 = alloca ptr, align 8
  %v7 = alloca i1, align 1
  %v8 = alloca ptr, align 8
  %self9 = alloca ptr, align 8
  %x10 = alloca ptr, align 8
  %x11 = alloca ptr, align 8
  %x12 = alloca ptr, align 8
  %v13 = call i64 @_CG_f_326_13(ptr %self)
  %v14 = call ptr @_CG_f_3338_16(i64 0, i64 3)
  store ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.7, i32 0, i32 1, i32 0), ptr %x, align 8
  store ptr %v14, ptr %v, align 8
  store ptr %self, ptr %self1, align 8
  br label %L144

L144:                                             ; preds = %L220, %entry
  %v15 = call ptr @GC_malloc(i64 16)
  %__pyc_more__ = load ptr, ptr @__pyc_more__, align 8
  %0 = getelementptr inbounds nuw %closure_closure.3363, ptr %v15, i32 0, i32 0
  store ptr %__pyc_more__, ptr %0, align 8
  %v16 = load ptr, ptr %v, align 8
  %1 = getelementptr inbounds nuw %closure_closure.3363, ptr %v15, i32 0, i32 1
  store ptr %v16, ptr %1, align 8
  %v17 = load ptr, ptr %v, align 8
  %v18 = call i1 @_CG_f_1699_8(ptr %v17)
  %x19 = load ptr, ptr %x, align 8
  store ptr %x19, ptr %x2, align 8
  store i1 %v18, ptr %v3, align 1
  %v20 = load ptr, ptr %v, align 8
  store ptr %v20, ptr %v4, align 8
  %self21 = load ptr, ptr %self1, align 8
  store ptr %self21, ptr %self5, align 8
  %x22 = load ptr, ptr %x, align 8
  store ptr %x22, ptr %x6, align 8
  store i1 %v18, ptr %v7, align 1
  %v23 = load ptr, ptr %v, align 8
  store ptr %v23, ptr %v8, align 8
  %self24 = load ptr, ptr %self1, align 8
  store ptr %self24, ptr %self9, align 8
  br i1 %v18, label %L221, label %L145

L221:                                             ; preds = %L144
  %v25 = call ptr @GC_malloc(i64 16)
  %__next__ = load ptr, ptr @__next__, align 8
  %2 = getelementptr inbounds nuw %closure_closure.3363, ptr %v25, i32 0, i32 0
  store ptr %__next__, ptr %2, align 8
  %v26 = load ptr, ptr %v4, align 8
  %3 = getelementptr inbounds nuw %closure_closure.3363, ptr %v25, i32 0, i32 1
  store ptr %v26, ptr %3, align 8
  %v27 = load ptr, ptr %v4, align 8
  %v28 = call i64 @_CG_f_1729_10(ptr %v27)
  %v29 = call i1 @_CG_f_890_21(i64 %v28)
  %x30 = load ptr, ptr %x2, align 8
  store ptr %x30, ptr %x10, align 8
  %x31 = load ptr, ptr %x2, align 8
  store ptr %x31, ptr %x11, align 8
  %x32 = load ptr, ptr %x11, align 8
  store ptr %x32, ptr %x12, align 8
  br i1 %v29, label %L219, label %L220

L145:                                             ; preds = %L144
  %x33 = load ptr, ptr %x6, align 8
  %v34 = call ptr @_CG_strcat(ptr %x33, ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.8, i32 0, i32 1, i32 0))
  br label %L143

L143:                                             ; preds = %L145
  ret ptr %v34

L219:                                             ; preds = %L221
  %x35 = load ptr, ptr %x10, align 8
  %v36 = call ptr @_CG_strcat(ptr %x35, ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.9, i32 0, i32 1, i32 0))
  store ptr %v36, ptr %x12, align 8
  br label %L220

L220:                                             ; preds = %L219, %L221
  %self37 = load ptr, ptr %self5, align 8
  %4 = getelementptr i64, ptr %self37, i64 %v28
  %v38 = load i64, ptr %4, align 4
  %v39 = call ptr @_CG_f_154_0(i64 %v38)
  %x40 = load ptr, ptr %x12, align 8
  %v41 = call ptr @_CG_strcat(ptr %x40, ptr %v39)
  store ptr %v41, ptr %x, align 8
  %v42 = load ptr, ptr %v4, align 8
  store ptr %v42, ptr %v, align 8
  %self43 = load ptr, ptr %self5, align 8
  store ptr %self43, ptr %self1, align 8
  br label %L144
}

define internal ptr @_CG_f_1684_7(ptr %self, i64 %ai, i64 %aj, i64 %ak) {
entry:
  %0 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 4
  store i64 0, ptr %0, align 4
  %1 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 5
  store i64 3, ptr %1, align 4
  %2 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 6
  store i64 1, ptr %2, align 4
  br label %L171

L171:                                             ; preds = %entry
  ret ptr %self
}

define internal i1 @_CG_f_1699_8(ptr %self) {
entry:
  %self1 = alloca ptr, align 8
  %self2 = alloca ptr, align 8
  %v = alloca i1, align 1
  store ptr %self, ptr %self1, align 8
  store ptr %self, ptr %self2, align 8
  br i1 true, label %L239, label %L240

L239:                                             ; preds = %entry
  %self3 = load ptr, ptr %self1, align 8
  %0 = getelementptr inbounds nuw %range.1437, ptr %self3, i32 0, i32 4
  %v4 = load i64, ptr %0, align 4
  %self5 = load ptr, ptr %self1, align 8
  %1 = getelementptr inbounds nuw %range.1437, ptr %self5, i32 0, i32 5
  %v6 = load i64, ptr %1, align 4
  %v7 = icmp slt i64 %v4, %v6
  store i1 %v7, ptr %v, align 1
  br label %L172

L240:                                             ; preds = %entry
  br label %L172

L172:                                             ; preds = %L240, %L239
  %v8 = load i1, ptr %v, align 1
  ret i1 %v8
}

define internal ptr @_CG_f_1723_9(ptr %this) {
entry:
  br label %L173

L173:                                             ; preds = %entry
  ret ptr %this
}

define internal i64 @_CG_f_1729_10(ptr %self) {
entry:
  %0 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 4
  %v = load i64, ptr %0, align 4
  %1 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 4
  %v1 = load i64, ptr %1, align 4
  %v2 = add i64 %v1, 1
  %2 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 4
  store i64 %v2, ptr %2, align 4
  br label %L174

L174:                                             ; preds = %entry
  ret i64 %v
}

define internal ptr @_CG_f_1667_11(ptr %arg) {
entry:
  %0 = getelementptr inbounds nuw %range.1437, ptr %arg, i32 0, i32 4
  store i64 0, ptr %0, align 4
  %1 = getelementptr inbounds nuw %range.1437, ptr %arg, i32 0, i32 5
  store i64 0, ptr %1, align 4
  %2 = getelementptr inbounds nuw %range.1437, ptr %arg, i32 0, i32 6
  store i64 1, ptr %2, align 4
  %3 = load ptr, ptr @g.2, align 8
  %4 = load ptr, ptr @g.2, align 8
  %5 = getelementptr inbounds nuw %range.1437, ptr %4, i32 0, i32 0
  store ptr @_CG_f_1684_7, ptr %5, align 8
  store i64 1, ptr @g, align 4
  %6 = load ptr, ptr @g.2, align 8
  %7 = getelementptr inbounds nuw %range.1437, ptr %6, i32 0, i32 3
  store ptr @_CG_f_1699_8, ptr %7, align 8
  %8 = load ptr, ptr @g.2, align 8
  %9 = getelementptr inbounds nuw %range.1437, ptr %8, i32 0, i32 1
  store ptr @_CG_f_1723_9, ptr %9, align 8
  %10 = load ptr, ptr @g.2, align 8
  %11 = getelementptr inbounds nuw %range.1437, ptr %10, i32 0, i32 2
  store ptr @_CG_f_1729_10, ptr %11, align 8
  ret ptr %arg
}

define internal ptr @_CG_f_2893_12(i64 %arg, i64 %arg1, i64 %arg2) {
entry:
  %0 = load ptr, ptr @g.2, align 8
  %v = call ptr @GC_malloc(i64 56)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %v, ptr align 8 %0, i64 56, i1 false)
  %v3 = call ptr @_CG_f_1684_7(ptr %v, i64 0, i64 3, i64 1)
  ret ptr %v
}

define internal i64 @_CG_f_326_13(ptr %x) {
entry:
  br label %L175

L175:                                             ; preds = %entry
  ret i64 3
}

define internal ptr @_CG_f_1946_14(i1 %a, i1 %b, i1 %c) {
entry:
  %c1 = alloca i1, align 1
  %b2 = alloca i1, align 1
  %v = alloca i1, align 1
  %c3 = alloca i1, align 1
  %b4 = alloca i1, align 1
  %v5 = alloca i1, align 1
  %v6 = alloca i1, align 1
  %c7 = alloca i1, align 1
  %b8 = alloca i1, align 1
  %c9 = alloca i1, align 1
  %v10 = alloca i1, align 1
  %b11 = alloca i1, align 1
  %v12 = alloca i1, align 1
  %c13 = alloca i1, align 1
  %b14 = alloca i1, align 1
  %v15 = alloca i1, align 1
  %v16 = alloca i1, align 1
  %c17 = alloca i1, align 1
  %v18 = alloca i1, align 1
  %c19 = alloca i1, align 1
  %b20 = alloca i1, align 1
  %v21 = alloca i1, align 1
  %c22 = alloca i1, align 1
  %c23 = alloca i1, align 1
  %v24 = alloca i1, align 1
  %b25 = alloca i1, align 1
  %v26 = call i1 @_CG_f_299_22(i1 %a)
  store i1 %c, ptr %c1, align 1
  store i1 %b, ptr %b2, align 1
  store i1 %a, ptr %v, align 1
  store i1 %c, ptr %c3, align 1
  store i1 %b, ptr %b4, align 1
  store i1 %a, ptr %v5, align 1
  %v27 = load i1, ptr %v5, align 1
  store i1 %v27, ptr %v6, align 1
  %c28 = load i1, ptr %c3, align 1
  store i1 %c28, ptr %c7, align 1
  %b29 = load i1, ptr %b4, align 1
  store i1 %b29, ptr %b8, align 1
  br i1 %v26, label %L274, label %L273

L274:                                             ; preds = %entry
  %b30 = load i1, ptr %b2, align 1
  %b31 = load i1, ptr %b2, align 1
  %v32 = call i1 @_CG_f_299_22(i1 %b31)
  %c33 = load i1, ptr %c1, align 1
  store i1 %c33, ptr %c9, align 1
  store i1 %b30, ptr %v10, align 1
  %b34 = load i1, ptr %b2, align 1
  store i1 %b34, ptr %b11, align 1
  %v35 = load i1, ptr %v10, align 1
  store i1 %v35, ptr %v6, align 1
  %c36 = load i1, ptr %c9, align 1
  store i1 %c36, ptr %c7, align 1
  %b37 = load i1, ptr %b11, align 1
  store i1 %b37, ptr %b8, align 1
  %c38 = load i1, ptr %c1, align 1
  store i1 %c38, ptr %c23, align 1
  store i1 %b30, ptr %v24, align 1
  %b39 = load i1, ptr %b2, align 1
  store i1 %b39, ptr %b25, align 1
  br i1 %v32, label %L275, label %L273

L273:                                             ; preds = %L275, %L274, %entry
  %v40 = load i1, ptr %v6, align 1
  %v41 = call i1 @_CG_f_299_22(i1 %v40)
  br i1 %v41, label %L276, label %L277

L276:                                             ; preds = %L273
  %v42 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [9 x i8] }>, ptr @.str.lit.10, i32 0, i32 1, i32 0))
  %v43 = call ptr @_CG_writeln()
  br label %L278

L277:                                             ; preds = %L273
  %v44 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [10 x i8] }>, ptr @.str.lit.11, i32 0, i32 1, i32 0))
  %v45 = call ptr @_CG_writeln()
  br label %L278

L278:                                             ; preds = %L277, %L276
  %v46 = call i1 @_CG_f_299_22(i1 %a)
  store i1 %a, ptr %v12, align 1
  %c47 = load i1, ptr %c7, align 1
  store i1 %c47, ptr %c13, align 1
  %b48 = load i1, ptr %b8, align 1
  store i1 %b48, ptr %b14, align 1
  %v49 = load i1, ptr %v12, align 1
  store i1 %v49, ptr %v15, align 1
  store i1 %a, ptr %v18, align 1
  %c50 = load i1, ptr %c7, align 1
  store i1 %c50, ptr %c19, align 1
  %b51 = load i1, ptr %b8, align 1
  store i1 %b51, ptr %b20, align 1
  br i1 %v46, label %L279, label %L280

L279:                                             ; preds = %L281, %L280, %L278
  %v52 = load i1, ptr %v15, align 1
  %v53 = call i1 @_CG_f_299_22(i1 %v52)
  br i1 %v53, label %L282, label %L283

L280:                                             ; preds = %L278
  %b54 = load i1, ptr %b20, align 1
  %b55 = load i1, ptr %b20, align 1
  %v56 = call i1 @_CG_f_299_22(i1 %b55)
  store i1 %b54, ptr %v16, align 1
  %c57 = load i1, ptr %c19, align 1
  store i1 %c57, ptr %c17, align 1
  %v58 = load i1, ptr %v16, align 1
  store i1 %v58, ptr %v15, align 1
  store i1 %b54, ptr %v21, align 1
  %c59 = load i1, ptr %c19, align 1
  store i1 %c59, ptr %c22, align 1
  br i1 %v56, label %L279, label %L281

L281:                                             ; preds = %L280
  %c60 = load i1, ptr %c22, align 1
  store i1 %c60, ptr %v15, align 1
  br label %L279

L282:                                             ; preds = %L279
  %v61 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [8 x i8] }>, ptr @.str.lit.12, i32 0, i32 1, i32 0))
  %v62 = call ptr @_CG_writeln()
  br label %L284

L283:                                             ; preds = %L279
  %v63 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [9 x i8] }>, ptr @.str.lit.13, i32 0, i32 1, i32 0))
  %v64 = call ptr @_CG_writeln()
  br label %L284

L284:                                             ; preds = %L283, %L282
  ret ptr undef

L275:                                             ; preds = %L274
  %c65 = load i1, ptr %c23, align 1
  store i1 %c65, ptr %v6, align 1
  %c66 = load i1, ptr %c23, align 1
  store i1 %c66, ptr %c7, align 1
  %b67 = load i1, ptr %b25, align 1
  store i1 %b67, ptr %b8, align 1
  br label %L273
}

define internal ptr @_CG_f_105_15() {
entry:
  %range = call ptr @GC_malloc(i64 56)
  store ptr %range, ptr @range, align 8
  %c = call ptr @GC_malloc(i64 24)
  store ptr %c, ptr @c, align 8
  %proto = call ptr @GC_malloc(i64 56)
  store ptr %proto, ptr @g.2, align 8
  %v = alloca i64, align 8
  %v1 = alloca ptr, align 8
  %v2 = alloca i64, align 8
  %v3 = alloca i64, align 8
  %v4 = alloca ptr, align 8
  %v5 = alloca i64, align 8
  %v6 = alloca i64, align 8
  %v7 = alloca ptr, align 8
  %v8 = alloca i64, align 8
  %v9 = alloca ptr, align 8
  %v10 = alloca i64, align 8
  %v11 = alloca i64, align 8
  %v12 = alloca double, align 8
  %v13 = alloca double, align 8
  %v14 = alloca double, align 8
  %v15 = alloca double, align 8
  %v16 = alloca double, align 8
  %v17 = alloca double, align 8
  %v18 = alloca i64, align 8
  %v19 = alloca i64, align 8
  %v20 = alloca ptr, align 8
  %v21 = alloca i64, align 8
  %v22 = alloca i64, align 8
  %v23 = alloca ptr, align 8
  %v24 = alloca double, align 8
  %v25 = alloca double, align 8
  %v26 = alloca double, align 8
  %v27 = alloca double, align 8
  %v28 = alloca double, align 8
  %v29 = alloca double, align 8
  %v30 = alloca ptr, align 8
  %v31 = alloca ptr, align 8
  %v32 = alloca ptr, align 8
  %v33 = alloca ptr, align 8
  %v34 = alloca ptr, align 8
  %v35 = alloca ptr, align 8
  %v36 = alloca ptr, align 8
  %v37 = alloca i1, align 1
  %v38 = alloca i1, align 1
  %v39 = alloca ptr, align 8
  %v40 = alloca i64, align 8
  %v41 = alloca i1, align 1
  %v42 = call ptr @GC_malloc(i64 48)
  %v43 = call ptr @GC_malloc(i64 40)
  %v44 = call ptr @GC_malloc(i64 40)
  %v45 = call ptr @GC_malloc(i64 40)
  %v46 = call ptr @GC_malloc(i64 40)
  %p = call ptr @GC_malloc(i64 56)
  store ptr %p, ptr @g.2, align 8
  %0 = load ptr, ptr @g.2, align 8
  %v47 = call ptr @_CG_f_1667_11(ptr %0)
  %v48 = call ptr @GC_malloc(i64 56)
  %v49 = call ptr @_CG_f_1946_14(i1 true, i1 true, i1 true)
  %v50 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [11 x i8] }>, ptr @.str.lit.14, i32 0, i32 1, i32 0))
  %v51 = call ptr @_CG_writeln()
  %v52 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [10 x i8] }>, ptr @.str.lit.15, i32 0, i32 1, i32 0))
  %v53 = call ptr @_CG_writeln()
  %v54 = call ptr @_CG_f_1946_14(i1 false, i1 false, i1 false)
  %v55 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [12 x i8] }>, ptr @.str.lit.16, i32 0, i32 1, i32 0))
  %v56 = call ptr @_CG_writeln()
  %v57 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [11 x i8] }>, ptr @.str.lit.17, i32 0, i32 1, i32 0))
  %v58 = call ptr @_CG_writeln()
  %v59 = call ptr @_CG_f_1946_14(i1 true, i1 false, i1 true)
  %v60 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [12 x i8] }>, ptr @.str.lit.18, i32 0, i32 1, i32 0))
  %v61 = call ptr @_CG_writeln()
  %v62 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [10 x i8] }>, ptr @.str.lit.19, i32 0, i32 1, i32 0))
  %v63 = call ptr @_CG_writeln()
  %v64 = call ptr @_CG_f_1946_14(i1 false, i1 true, i1 false)
  %v65 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [12 x i8] }>, ptr @.str.lit.20, i32 0, i32 1, i32 0))
  %v66 = call ptr @_CG_writeln()
  %v67 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [10 x i8] }>, ptr @.str.lit.21, i32 0, i32 1, i32 0))
  %v68 = call ptr @_CG_writeln()
  %v69 = call ptr @_CG_f_1946_14(i1 true, i1 true, i1 false)
  %v70 = call ptr @_CG_f_1946_14(i1 false, i1 false, i1 true)
  %v71 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [12 x i8] }>, ptr @.str.lit.22, i32 0, i32 1, i32 0))
  %v72 = call ptr @_CG_writeln()
  %v73 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [10 x i8] }>, ptr @.str.lit.23, i32 0, i32 1, i32 0))
  %v74 = call ptr @_CG_writeln()
  %v75 = call ptr @_CG_f_1946_14(i1 true, i1 false, i1 false)
  %v76 = call ptr @_CG_f_1946_14(i1 false, i1 true, i1 true)
  %v77 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [12 x i8] }>, ptr @.str.lit.24, i32 0, i32 1, i32 0))
  %v78 = call ptr @_CG_writeln()
  %v79 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [10 x i8] }>, ptr @.str.lit.25, i32 0, i32 1, i32 0))
  %v80 = call ptr @_CG_writeln()
  store i64 1, ptr %v, align 4
  store i64 1, ptr %v1, align 4
  %v81 = load ptr, ptr %v1, align 8
  store ptr %v81, ptr %v2, align 8
  br i1 true, label %L286, label %L285

L286:                                             ; preds = %entry
  store i64 2, ptr %v2, align 4
  br label %L285

L285:                                             ; preds = %L286, %entry
  %v82 = call ptr @_CG_f_882_19()
  %v83 = call ptr @_CG_write(ptr %v82)
  %v84 = call ptr @_CG_writeln()
  store i64 1, ptr %v3, align 4
  store i64 1, ptr %v4, align 4
  %v85 = load ptr, ptr %v4, align 8
  store ptr %v85, ptr %v5, align 8
  br i1 true, label %L288, label %L287

L288:                                             ; preds = %L285
  store i64 45, ptr %v5, align 4
  br label %L287

L287:                                             ; preds = %L288, %L285
  %v86 = call ptr @_CG_f_882_20()
  %v87 = call ptr @_CG_write(ptr %v86)
  %v88 = call ptr @_CG_writeln()
  store i64 1, ptr %v6, align 4
  store i64 1, ptr %v7, align 4
  %v89 = load ptr, ptr %v7, align 8
  store ptr %v89, ptr %v8, align 8
  br i1 true, label %L290, label %L289

L290:                                             ; preds = %L287
  store i64 0, ptr %v8, align 4
  br label %L289

L289:                                             ; preds = %L290, %L287
  %v90 = call ptr @_CG_f_882_18()
  %v91 = call ptr @_CG_write(ptr %v90)
  %v92 = call ptr @_CG_writeln()
  store i64 0, ptr %v9, align 4
  store i64 0, ptr %v10, align 4
  store i64 0, ptr %v11, align 4
  br i1 false, label %L292, label %L291

L292:                                             ; preds = %L289
  store i64 2, ptr %v11, align 4
  br label %L291

L291:                                             ; preds = %L292, %L289
  %v93 = call ptr @_CG_f_882_18()
  %v94 = call ptr @_CG_write(ptr %v93)
  %v95 = call ptr @_CG_writeln()
  %v96 = call i1 @_CG_f_1131_5(double 1.000000e+00)
  store double 1.000000e+00, ptr %v12, align 8
  store double 1.000000e+00, ptr %v13, align 8
  store double 1.000000e+00, ptr %v14, align 8
  br i1 %v96, label %L294, label %L293

L294:                                             ; preds = %L291
  store double 0.000000e+00, ptr %v14, align 8
  br label %L293

L293:                                             ; preds = %L294, %L291
  %v97 = load double, ptr %v14, align 8
  %v98 = call ptr @_CG_f_1125_4(double %v97)
  %v99 = call ptr @_CG_write(ptr %v98)
  %v100 = call ptr @_CG_writeln()
  %v101 = call i1 @_CG_f_1131_5(double 0.000000e+00)
  store double 0.000000e+00, ptr %v15, align 8
  store double 0.000000e+00, ptr %v16, align 8
  store double 0.000000e+00, ptr %v17, align 8
  br i1 %v101, label %L296, label %L295

L296:                                             ; preds = %L293
  store double 2.000000e+00, ptr %v17, align 8
  br label %L295

L295:                                             ; preds = %L296, %L293
  %v102 = load double, ptr %v17, align 8
  %v103 = call ptr @_CG_f_1125_4(double %v102)
  %v104 = call ptr @_CG_write(ptr %v103)
  %v105 = call ptr @_CG_writeln()
  store i64 2, ptr %v18, align 4
  store i64 2, ptr %v19, align 4
  store i64 2, ptr %v20, align 4
  br i1 true, label %L297, label %L298

L297:                                             ; preds = %L298, %L295
  %v106 = call ptr @_CG_f_882_19()
  %v107 = call ptr @_CG_write(ptr %v106)
  %v108 = call ptr @_CG_writeln()
  store i64 2, ptr %v21, align 4
  store i64 2, ptr %v22, align 4
  store i64 2, ptr %v23, align 4
  br i1 true, label %L299, label %L300

L298:                                             ; preds = %L295
  store i64 3, ptr %v19, align 4
  br label %L297

L299:                                             ; preds = %L300, %L297
  %v109 = call ptr @_CG_f_882_19()
  %v110 = call ptr @_CG_write(ptr %v109)
  %v111 = call ptr @_CG_writeln()
  %v112 = call i1 @_CG_f_1131_5(double 2.000000e+00)
  store double 2.000000e+00, ptr %v24, align 8
  store double 2.000000e+00, ptr %v25, align 8
  store double 2.000000e+00, ptr %v26, align 8
  br i1 %v112, label %L301, label %L302

L300:                                             ; preds = %L297
  store i64 0, ptr %v22, align 4
  br label %L299

L301:                                             ; preds = %L302, %L299
  %v113 = load double, ptr %v25, align 8
  %v114 = call ptr @_CG_f_1125_4(double %v113)
  %v115 = call ptr @_CG_write(ptr %v114)
  %v116 = call ptr @_CG_writeln()
  %v117 = call i1 @_CG_f_1131_5(double 2.000000e+00)
  store double 2.000000e+00, ptr %v27, align 8
  store double 2.000000e+00, ptr %v28, align 8
  store double 2.000000e+00, ptr %v29, align 8
  br i1 %v117, label %L303, label %L304

L302:                                             ; preds = %L299
  store double 3.000000e+00, ptr %v25, align 8
  br label %L301

L303:                                             ; preds = %L304, %L301
  %v118 = load double, ptr %v28, align 8
  %v119 = call ptr @_CG_f_1125_4(double %v118)
  %v120 = call ptr @_CG_write(ptr %v119)
  %v121 = call ptr @_CG_writeln()
  %v122 = call ptr @_CG_f_293_2(i1 true)
  %v123 = call ptr @_CG_write(ptr %v122)
  %v124 = call ptr @_CG_writeln()
  %v125 = call i1 @_CG_f_287_1(i1 false)
  %v126 = call ptr @_CG_f_293_2(i1 %v125)
  %v127 = call ptr @_CG_write(ptr %v126)
  %v128 = call ptr @_CG_writeln()
  %v129 = call i1 @_CG_f_287_1(i1 true)
  %v130 = call ptr @_CG_f_293_2(i1 %v129)
  %v131 = call ptr @_CG_write(ptr %v130)
  %v132 = call ptr @_CG_writeln()
  %v133 = call ptr @GC_malloc(i64 8)
  %v134 = load ptr, ptr %v30, align 8
  store ptr %v134, ptr %v31, align 8
  br i1 false, label %L305, label %L306

L304:                                             ; preds = %L301
  store double 0.000000e+00, ptr %v28, align 8
  br label %L303

L305:                                             ; preds = %L306, %L303
  %v135 = call ptr @_CG_f_882_19()
  %v136 = call ptr @_CG_write(ptr %v135)
  %v137 = call ptr @_CG_writeln()
  %list_tmp = call ptr @_CG_prim_tuple_list_internal(i32 0, i32 1)
  %v138 = call ptr @_CG_to_list_runtime(ptr %list_tmp, i32 0, i32 0)
  %list_tmp139 = call ptr @_CG_prim_tuple_list_internal(i32 24, i32 4)
  %1 = getelementptr inbounds nuw %list.3368, ptr %list_tmp139, i32 0, i32 0
  store i64 1, ptr %1, align 4
  %2 = getelementptr inbounds nuw %list.3368, ptr %list_tmp139, i32 0, i32 1
  store i64 2, ptr %2, align 4
  %3 = getelementptr inbounds nuw %list.3368, ptr %list_tmp139, i32 0, i32 2
  store i64 3, ptr %3, align 4
  %v140 = call ptr @_CG_to_list_runtime(ptr %list_tmp139, i32 24, i32 3)
  store ptr %v140, ptr @c, align 8
  store ptr %v138, ptr %v33, align 8
  %v141 = load ptr, ptr %v33, align 8
  store ptr %v141, ptr %v34, align 8
  store ptr %v138, ptr %v35, align 8
  br i1 false, label %L307, label %L308

L306:                                             ; preds = %L303
  br label %L305

L307:                                             ; preds = %L308, %L305
  %v142 = load ptr, ptr %v34, align 8
  %v143 = call ptr @_CG_f_1486_6(ptr %v142)
  %v144 = call ptr @_CG_write(ptr %v143)
  %v145 = call ptr @_CG_writeln()
  store i1 false, ptr %v36, align 1
  store i1 false, ptr %v37, align 1
  store i1 false, ptr %v38, align 1
  br i1 false, label %L310, label %L309

L308:                                             ; preds = %L305
  %c146 = load ptr, ptr @c, align 8
  store ptr %c146, ptr %v34, align 8
  br label %L307

L310:                                             ; preds = %L307
  store i64 0, ptr %v38, align 4
  br label %L309

L309:                                             ; preds = %L310, %L307
  %v147 = call ptr @_CG_f_293_2(i1 false)
  %v148 = call ptr @_CG_write(ptr %v147)
  %v149 = call ptr @_CG_writeln()
  store i1 false, ptr %v39, align 1
  %v150 = load ptr, ptr %v39, align 8
  store ptr %v150, ptr %v40, align 8
  store i1 false, ptr %v41, align 1
  br i1 false, label %L311, label %L312

L311:                                             ; preds = %L312, %L309
  %v151 = call ptr @_CG_f_882_3()
  %v152 = call ptr @_CG_write(ptr %v151)
  %v153 = call ptr @_CG_writeln()
  ret ptr undef

L312:                                             ; preds = %L309
  store i64 3, ptr %v40, align 4
  br label %L311
}

define internal ptr @_CG_f_3338_16(i64 %arg, i64 %arg1) {
entry:
  %v = call ptr @_CG_f_2893_12(i64 0, i64 3, i64 1)
  ret ptr %v
}

define internal ptr @_CG_f_882_17(i64 %self) {
entry:
  %v = call ptr @_CG_str_from_int(i64 %self)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_18() {
entry:
  %v = call ptr @_CG_str_from_int(i64 0)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_19() {
entry:
  %v = call ptr @_CG_str_from_int(i64 2)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_20() {
entry:
  %v = call ptr @_CG_str_from_int(i64 45)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal i1 @_CG_f_890_21(i64 %self) {
entry:
  %v = icmp ne i64 %self, 0
  br label %L81

L81:                                              ; preds = %entry
  ret i1 %v
}

define internal i1 @_CG_f_299_22(i1 %self) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %self
}

define internal i1 @_CG_f_299_23(i1 %self) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %self
}

declare ptr @_CG_str_from_int(i64)

declare ptr @_CG_str_from_float(double)

declare ptr @GC_malloc(i64)

declare ptr @_CG_strcat(ptr, ptr)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

declare ptr @_CG_prim_tuple_list_internal(i32, i32)

declare ptr @_CG_to_list_runtime(ptr, i32, i32)

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_15()
  ret i32 0
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "logical_operators.py", directory: "tests")
