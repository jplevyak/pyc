; ModuleID = 'ifa_output'
source_filename = "issue_027.py"
target triple = "arm64-apple-darwin25.5.0"

%Node.3015 = type { ptr, i64, ptr }

@None = internal global ptr null
@n1 = internal global ptr null
@str = internal global ptr null
@Node = internal global ptr null
@n2 = internal global ptr null
@int = internal global ptr null
@g2186 = internal global ptr null
@.str.lit = private constant <{ i64, [17 x i8] }> <{ i64 16, [17 x i8] c"_CG_str_from_int\00" }>

define i1 @_CG_f_287_0(i1 %0) {
entry:
  %1 = alloca i1, align 1
  br i1 %0, label %t_edge, label %f_edge

L205:                                             ; preds = %t_edge
  store i1 false, ptr %1, align 1
  br label %L25

L206:                                             ; preds = %f_edge
  store i1 true, ptr %1, align 1
  br label %L25

L25:                                              ; preds = %L206, %L205
  %2 = load i1, ptr %1, align 1
  ret i1 %2

t_edge:                                           ; preds = %entry
  br label %L205

f_edge:                                           ; preds = %entry
  br label %L206
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

define ptr @_CG_f_1953_3(ptr %0, i64 %1) {
entry:
  %2 = getelementptr inbounds nuw %Node.3015, ptr %0, i32 0, i32 1
  store i64 %1, ptr %2, align 4
  %g = load ptr, ptr @None, align 8
  %g1 = load ptr, ptr @None, align 8
  %3 = getelementptr inbounds nuw %Node.3015, ptr %0, i32 0, i32 2
  store ptr %g1, ptr %3, align 8
  ret ptr null
}

define ptr @_CG_f_2939_4(i64 %0) {
entry:
  %g = load ptr, ptr @g2186, align 8
  %clone = call ptr @GC_malloc(i64 24)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 24, i1 false)
  %1 = call ptr @_CG_f_1953_3(ptr %clone, i64 %0)
  ret ptr %clone
}

define i64 @_CG_f_1965_5(ptr %0) {
entry:
  %1 = alloca i64, align 8
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  store ptr %3, ptr %2, align 8
  store i64 0, ptr %1, align 4
  %4 = load ptr, ptr %2, align 8
  store ptr %4, ptr %2, align 8
  br label %L195

L195:                                             ; preds = %L276, %entry
  %5 = load ptr, ptr %2, align 8
  %is = icmp eq ptr %5, null
  %6 = call i1 @_CG_f_287_0(i1 %is)
  br i1 %6, label %t_edge, label %f_edge

L276:                                             ; preds = %t_edge
  %7 = load ptr, ptr %2, align 8
  %8 = load ptr, ptr %2, align 8
  %9 = getelementptr inbounds nuw %Node.3015, ptr %8, i32 0, i32 1
  %10 = load i64, ptr %9, align 4
  %11 = load i64, ptr %1, align 4
  %12 = add i64 %11, %10
  store i64 %12, ptr %1, align 4
  %13 = load i64, ptr %1, align 4
  store i64 %13, ptr %1, align 4
  %14 = load ptr, ptr %2, align 8
  %15 = load ptr, ptr %2, align 8
  %16 = getelementptr inbounds nuw %Node.3015, ptr %15, i32 0, i32 2
  %17 = load ptr, ptr %16, align 8
  store ptr %17, ptr %2, align 8
  %18 = load ptr, ptr %2, align 8
  store ptr %18, ptr %2, align 8
  %19 = load i64, ptr %1, align 4
  store i64 %19, ptr %1, align 4
  %20 = load ptr, ptr %2, align 8
  store ptr %20, ptr %2, align 8
  br label %L195

L196:                                             ; preds = %f_edge
  %21 = load i64, ptr %1, align 4
  store i64 %21, ptr %1, align 4
  br label %L194

L194:                                             ; preds = %L196
  %22 = load i64, ptr %1, align 4
  ret i64 %22

t_edge:                                           ; preds = %L195
  %23 = load i64, ptr %1, align 4
  store i64 %23, ptr %1, align 4
  %24 = load ptr, ptr %2, align 8
  store ptr %24, ptr %2, align 8
  br label %L276

f_edge:                                           ; preds = %L195
  %25 = load i64, ptr %1, align 4
  store i64 %25, ptr %1, align 4
  %26 = load ptr, ptr %2, align 8
  store ptr %26, ptr %2, align 8
  br label %L196
}

define ptr @_CG_f_105_6() {
main_prelude:
  %proto = call ptr @GC_malloc(i64 24)
  store ptr %proto, ptr @n2, align 8
  %proto5 = call ptr @GC_malloc(i64 24)
  store ptr %proto5, ptr @Node, align 8
  %proto6 = call ptr @GC_malloc(i64 24)
  store ptr %proto6, ptr @n1, align 8
  %proto7 = call ptr @GC_malloc(i64 24)
  store ptr %proto7, ptr @g2186, align 8
  br label %entry

entry:                                            ; preds = %main_prelude
  %new = call ptr @GC_malloc(i64 24)
  %0 = call ptr @_CG_f_2939_4(i64 10)
  store ptr %0, ptr @n1, align 8
  %1 = call ptr @_CG_f_2939_4(i64 20)
  store ptr %1, ptr @n2, align 8
  %g = load ptr, ptr @n2, align 8
  %g1 = load ptr, ptr @n1, align 8
  %g2 = load ptr, ptr @n2, align 8
  %2 = getelementptr inbounds nuw %Node.3015, ptr %g1, i32 0, i32 2
  store ptr %g2, ptr %2, align 8
  %g3 = load ptr, ptr @n1, align 8
  %3 = call i64 @_CG_f_1965_5(ptr %g3)
  %4 = call ptr @_CG_f_882_2(i64 %3)
  %5 = call ptr @_CG_write(ptr %4)
  %6 = call ptr @_CG_writeln()
  %g4 = load ptr, ptr @None, align 8
  ret ptr %g4
}

declare ptr @_CG_str_from_int(i64)

declare ptr @GC_malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_6()
  ret i32 0
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "issue_027.py", directory: "tests")
