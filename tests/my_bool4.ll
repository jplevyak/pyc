; ModuleID = 'ifa_output'
source_filename = "my_bool4.py"
target triple = "arm64-apple-darwin25.5.0"

@.str.lit = private constant <{ i64, [9 x i8] }> <{ i64 8, [9 x i8] c"and true\00" }>

define ptr @_CG_f_1946_0() {
entry:
  br label %L273

L273:                                             ; preds = %entry
  br i1 true, label %L274, label %L272

L272:                                             ; preds = %L274, %L273
  br label %L275

L275:                                             ; preds = %L272
  %0 = call ptr @_CG_write(ptr getelementptr inbounds (<{ i64, [9 x i8] }>, ptr @.str.lit, i32 0, i32 1, i32 0))
  %1 = call ptr @_CG_writeln()
  br label %L277

L276:                                             ; No predecessors!
  br label %L277

L277:                                             ; preds = %L276, %L275
  ret ptr null

L274:                                             ; preds = %L273
  br label %L272
}

define ptr @_CG_f_105_1() {
main_prelude:
  br label %entry

entry:                                            ; preds = %main_prelude
  %0 = call ptr @_CG_f_1946_0()
  ret ptr null
}

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_1()
  ret i32 0
}

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "my_bool4.py", directory: "tests")
