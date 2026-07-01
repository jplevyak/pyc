; ModuleID = 'ifa_output'
source_filename = "my_bool.py"
target triple = "arm64-apple-darwin25.5.0"

@writeln = internal global ptr null
@__primitive = internal global ptr null
@write = internal global ptr null
@__str__ = internal global ptr null
@__operator = internal global ptr null
@reply = internal global ptr null
@__pyc_to_bool__ = internal global ptr null
@new = internal global ptr null
@a = internal global i1 false
@b = internal global i1 false
@. = internal global ptr null
@.str.lit = private constant <{ i64, [5 x i8] }> <{ i64 4, [5 x i8] c"True\00" }>
@.str.lit.1 = private constant <{ i64, [5 x i8] }> <{ i64 4, [5 x i8] c"True\00" }>
@.str.lit.2 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>
@.str.lit.3 = private constant <{ i64, [6 x i8] }> <{ i64 5, [6 x i8] c"False\00" }>

define internal ptr @_CG_f_293_0(i1 %self) {
entry:
  %v = alloca ptr, align 8
  br i1 %self, label %L203, label %L204

L203:                                             ; preds = %entry
  store ptr getelementptr inbounds (<{ i64, [5 x i8] }>, ptr @.str.lit.1, i32 0, i32 1, i32 0), ptr %v, align 8
  br label %L26

L204:                                             ; preds = %entry
  store ptr getelementptr inbounds (<{ i64, [6 x i8] }>, ptr @.str.lit.3, i32 0, i32 1, i32 0), ptr %v, align 8
  br label %L26

L26:                                              ; preds = %L204, %L203
  %v1 = load ptr, ptr %v, align 8
  ret ptr %v1
}

define internal i1 @_CG_f_299_1(i1 %self) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %self
}

define internal ptr @_CG_f_105_2() {
entry:
  %v = call ptr @GC_malloc(i64 48)
  %v1 = call ptr @GC_malloc(i64 40)
  %v2 = call ptr @GC_malloc(i64 40)
  %v3 = call ptr @GC_malloc(i64 40)
  %v4 = call ptr @GC_malloc(i64 40)
  %v5 = call ptr @GC_malloc(i64 56)
  %v6 = call ptr @GC_malloc(i64 56)
  store i1 true, ptr @a, align 1
  store i1 false, ptr @b, align 1
  %v7 = call ptr @_CG_f_293_0(i1 true)
  %v8 = call ptr @_CG_write(ptr %v7)
  %v9 = call ptr @_CG_writeln()
  %v10 = call ptr @_CG_f_293_0(i1 false)
  %v11 = call ptr @_CG_write(ptr %v10)
  %v12 = call ptr @_CG_writeln()
  ret ptr undef
}

declare ptr @GC_malloc(i64)

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_2()
  ret i32 0
}

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "my_bool.py", directory: "tests")
