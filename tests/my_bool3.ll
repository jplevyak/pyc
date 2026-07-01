; ModuleID = 'ifa_output'
source_filename = "my_bool3.py"
target triple = "arm64-apple-darwin25.5.0"

@new = internal global ptr null
@__pyc_to_bool__ = internal global ptr null
@__primitive = internal global ptr null
@__str__ = internal global ptr null
@writeln = internal global ptr null
@write = internal global ptr null
@reply = internal global ptr null
@__operator = internal global ptr null
@. = internal global ptr null
@.str.lit = private constant <{ i64, [4 x i8] }> <{ i64 3, [4 x i8] c"and\00" }>
@.str.lit.1 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c" \00" }>
@.str.lit.2 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>
@.str.lit.3 = private constant <{ i64, [3 x i8] }> <{ i64 2, [3 x i8] c"or\00" }>
@.str.lit.4 = private constant <{ i64, [2 x i8] }> <{ i64 1, [2 x i8] c" \00" }>
@.str.lit.5 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>

define internal ptr @_CG_f_105_0() {
entry:
  %v = alloca ptr, align 8
  %v1 = alloca i1, align 1
  %v2 = alloca i1, align 1
  %v3 = alloca ptr, align 8
  %v4 = alloca i1, align 1
  %v5 = alloca i1, align 1
  %v6 = call ptr @GC_malloc(i64 48)
  %v7 = call ptr @GC_malloc(i64 40)
  %v8 = call ptr @GC_malloc(i64 40)
  %v9 = call ptr @GC_malloc(i64 40)
  %v10 = call ptr @GC_malloc(i64 40)
  %v11 = call ptr @GC_malloc(i64 56)
  %v12 = call ptr @GC_malloc(i64 56)
  store i1 false, ptr %v, align 1
  store i1 false, ptr %v1, align 1
  store i1 false, ptr %v2, align 1
  br i1 false, label %L272, label %L271

L272:                                             ; preds = %entry
  store i1 false, ptr %v2, align 1
  br label %L271

L271:                                             ; preds = %L272, %entry
  %v13 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [4 x i8] }>, ptr @.str.lit, i32 0, i32 1, i32 0))
  %v14 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0))
  %v15 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [6 x i8] }>, ptr @.str.lit.2, i32 0, i32 1, i32 0))
  %v16 = call ptr @_CG_writeln()
  store i1 false, ptr %v3, align 1
  %v17 = load ptr, ptr %v3, align 8
  store ptr %v17, ptr %v4, align 8
  store i1 false, ptr %v5, align 1
  br i1 false, label %L273, label %L274

L273:                                             ; preds = %L274, %L271
  %v18 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [3 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0))
  %v19 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [2 x i8] }>, ptr @.str.lit.4, i32 0, i32 1, i32 0))
  %v20 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [6 x i8] }>, ptr @.str.lit.5, i32 0, i32 1, i32 0))
  %v21 = call ptr @_CG_writeln()
  ret ptr undef

L274:                                             ; preds = %L271
  store i1 false, ptr %v4, align 1
  br label %L273
}

declare ptr @GC_malloc(i64)

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_0()
  ret i32 0
}

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "my_bool3.py", directory: "tests")
