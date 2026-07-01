; ModuleID = 'ifa_output'
source_filename = "my_bool2.py"
target triple = "arm64-apple-darwin25.5.0"

@__operator = internal global ptr null
@. = internal global ptr null
@__pyc_to_bool__ = internal global ptr null
@__str__ = internal global ptr null
@new = internal global ptr null
@__primitive = internal global ptr null
@writeln = internal global ptr null
@write = internal global ptr null
@reply = internal global ptr null
@.str.lit = private constant <{ i64, [11 x i8] }> <{ i64 10, [11 x i8] c"a is false\00" }>

define internal ptr @_CG_f_105_0() {
entry:
  %v = call ptr @GC_malloc(i64 48)
  %v1 = call ptr @GC_malloc(i64 40)
  %v2 = call ptr @GC_malloc(i64 40)
  %v3 = call ptr @GC_malloc(i64 40)
  %v4 = call ptr @GC_malloc(i64 40)
  %v5 = call ptr @GC_malloc(i64 56)
  %v6 = call ptr @GC_malloc(i64 56)
  br i1 false, label %L271, label %L272

L271:                                             ; preds = %entry
  %v7 = call ptr @_CG_writeln()
  br label %L273

L272:                                             ; preds = %entry
  %v8 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [11 x i8] }>, ptr @.str.lit, i32 0, i32 1, i32 0))
  %v9 = call ptr @_CG_writeln()
  br label %L273

L273:                                             ; preds = %L272, %L271
  ret ptr undef
}

declare ptr @GC_malloc(i64)

declare ptr @_CG_writeln()

declare ptr @_CG_write(ptr)

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
!4 = !DIFile(filename: "my_bool2.py", directory: "tests")
