; ModuleID = 'ifa_output'
source_filename = "is_not_none_narrow.py"
target triple = "arm64-apple-darwin25.5.0"

%Node.3014 = type { ptr, i64, ptr }

@Node = internal global ptr null
@g2186 = internal global ptr null
@str = internal global ptr null
@int = internal global ptr null
@None = internal global ptr null
@.str.lit = private constant <{ i64, [17 x i8] }> <{ i64 16, [17 x i8] c"_CG_str_from_int\00" }>

define i1 @_CG_f_287_0(i1 %0) {
entry:
  %1 = alloca i1, align 1
  br i1 %0, label %t_edge, label %f_edge

L203:                                             ; preds = %t_edge
  store i1 false, ptr %1, align 1
  br label %L25

L204:                                             ; preds = %f_edge
  store i1 true, ptr %1, align 1
  br label %L25

L25:                                              ; preds = %L204, %L203
  %2 = load i1, ptr %1, align 1
  ret i1 %2

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

define ptr @_CG_f_882_2(i64 %0) {
entry:
  %1 = call ptr @_CG_str_from_int(i64 %0)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %1
}

define ptr @_CG_f_2934_3(i64 %0) {
entry:
  %g = load ptr, ptr @g2186, align 8
  %clone = call ptr @GC_malloc(i64 24)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 24, i1 false)
  %1 = getelementptr inbounds nuw %Node.3014, ptr %clone, i32 0, i32 1
  store i64 5, ptr %1, align 4
  ret ptr %clone
}

define i64 @_CG_f_1965_4(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  %2 = alloca i64, align 8
  store ptr %0, ptr %1, align 8
  %g = load ptr, ptr @None, align 8
  %3 = load ptr, ptr %1, align 8
  %g1 = load ptr, ptr @None, align 8
  %is = icmp eq ptr %3, %g1
  %4 = call i1 @_CG_f_287_0(i1 %is)
  br i1 %4, label %t_edge, label %f_edge

L274:                                             ; preds = %t_edge
  store i64 5, ptr %2, align 4
  br label %L194

L275:                                             ; preds = %f_edge
  store i64 0, ptr %2, align 4
  br label %L194

L194:                                             ; preds = %L275, %L274
  %5 = load i64, ptr %2, align 4
  ret i64 %5

t_edge:                                           ; preds = %entry
  %6 = load ptr, ptr %1, align 8
  store ptr %6, ptr %1, align 8
  br label %L274

f_edge:                                           ; preds = %entry
  %7 = load ptr, ptr %1, align 8
  store ptr %7, ptr %1, align 8
  br label %L275
}

define ptr @_CG_f_105_5() {
main_prelude:
  %proto = call ptr @GC_malloc(i64 24)
  store ptr %proto, ptr @g2186, align 8
  %proto2 = call ptr @GC_malloc(i64 24)
  store ptr %proto2, ptr @Node, align 8
  br label %entry

entry:                                            ; preds = %main_prelude
  %new = call ptr @GC_malloc(i64 24)
  %0 = call ptr @_CG_f_2934_3(i64 5)
  %1 = call i64 @_CG_f_1965_4(ptr %0)
  %2 = call ptr @_CG_f_882_2(i64 %1)
  %3 = call ptr @_CG_write(ptr %2)
  %4 = call ptr @_CG_writeln()
  %g = load ptr, ptr @None, align 8
  %5 = call i64 @_CG_f_1965_4(ptr %g)
  %6 = call ptr @_CG_f_882_2(i64 %5)
  %7 = call ptr @_CG_write(ptr %6)
  %8 = call ptr @_CG_writeln()
  %g1 = load ptr, ptr @None, align 8
  ret ptr %g1
}

declare ptr @_CG_str_from_int(i64)

declare ptr @GC_malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_5()
  ret i32 0
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "is_not_none_narrow.py", directory: "tests")
