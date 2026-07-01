; ModuleID = 'ifa_output'
source_filename = "recursive_list_is_none.py"
target triple = "arm64-apple-darwin25.5.0"

%Node.3070 = type { ptr, i64, ptr }

@Node = internal global ptr null
@head = internal global ptr null
@prev = internal global ptr null
@i = internal global i64 0
@int = internal global ptr null
@cur = internal global ptr null
@None = internal global ptr null
@n1 = internal global ptr null
@n2 = internal global ptr null
@n3 = internal global ptr null
@g2186 = internal global ptr null
@str = internal global ptr null
@.str.lit = private constant <{ i64, [17 x i8] }> <{ i64 16, [17 x i8] c"_CG_str_from_int\00" }>

define i1 @_CG_f_299_0(i1 %0) {
entry:
  br label %L27

L27:                                              ; preds = %entry
  ret i1 %0
}

define ptr @_CG_f_882_1(i64 %0) {
entry:
  %1 = call ptr @_CG_str_from_int(i64 %0)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %1
}

define ptr @_CG_f_1953_2(ptr %0, i64 %1) {
entry:
  %2 = getelementptr inbounds nuw %Node.3070, ptr %0, i32 0, i32 1
  store i64 %1, ptr %2, align 4
  %g = load ptr, ptr @None, align 8
  %g1 = load ptr, ptr @None, align 8
  %3 = getelementptr inbounds nuw %Node.3070, ptr %0, i32 0, i32 2
  store ptr %g1, ptr %3, align 8
  ret ptr null
}

define ptr @_CG_f_2968_3(i64 %0) {
entry:
  %g = load ptr, ptr @g2186, align 8
  %clone = call ptr @GC_malloc(i64 24)
  call void @llvm.memcpy.p0.p0.i64(ptr %clone, ptr %g, i64 24, i1 false)
  %1 = call ptr @_CG_f_1953_2(ptr %clone, i64 %0)
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
  br i1 %is, label %t_edge, label %f_edge

L276:                                             ; preds = %t_edge
  store i64 0, ptr %2, align 4
  br label %L194

L277:                                             ; preds = %f_edge
  %4 = load ptr, ptr %1, align 8
  %5 = load ptr, ptr %1, align 8
  %6 = getelementptr inbounds nuw %Node.3070, ptr %5, i32 0, i32 1
  %7 = load i64, ptr %6, align 4
  %8 = load ptr, ptr %1, align 8
  %9 = load ptr, ptr %1, align 8
  %10 = getelementptr inbounds nuw %Node.3070, ptr %9, i32 0, i32 2
  %11 = load ptr, ptr %10, align 8
  %12 = call i64 @_CG_f_1965_4(ptr %11)
  %13 = add i64 %7, %12
  store i64 %13, ptr %2, align 4
  %14 = load i64, ptr %2, align 4
  store i64 %14, ptr %2, align 4
  %15 = load i64, ptr %2, align 4
  store i64 %15, ptr %2, align 4
  br label %L194

L194:                                             ; preds = %L277, %L276
  %16 = load i64, ptr %2, align 4
  ret i64 %16

t_edge:                                           ; preds = %entry
  br label %L276

f_edge:                                           ; preds = %entry
  %17 = load ptr, ptr %1, align 8
  store ptr %17, ptr %1, align 8
  br label %L277
}

define ptr @_CG_f_105_5() {
main_prelude:
  %proto = call ptr @GC_malloc(i64 24)
  store ptr %proto, ptr @Node, align 8
  %proto18 = call ptr @GC_malloc(i64 24)
  store ptr %proto18, ptr @cur, align 8
  %proto19 = call ptr @GC_malloc(i64 24)
  store ptr %proto19, ptr @g2186, align 8
  %proto20 = call ptr @GC_malloc(i64 24)
  store ptr %proto20, ptr @n1, align 8
  %proto21 = call ptr @GC_malloc(i64 24)
  store ptr %proto21, ptr @head, align 8
  %proto22 = call ptr @GC_malloc(i64 24)
  store ptr %proto22, ptr @n2, align 8
  %proto23 = call ptr @GC_malloc(i64 24)
  store ptr %proto23, ptr @prev, align 8
  %proto24 = call ptr @GC_malloc(i64 24)
  store ptr %proto24, ptr @n3, align 8
  br label %entry

entry:                                            ; preds = %main_prelude
  %new = call ptr @GC_malloc(i64 24)
  %0 = call ptr @_CG_f_2968_3(i64 1)
  store ptr %0, ptr @n1, align 8
  %1 = call ptr @_CG_f_2968_3(i64 2)
  store ptr %1, ptr @n2, align 8
  %2 = call ptr @_CG_f_2968_3(i64 3)
  store ptr %2, ptr @n3, align 8
  %g = load ptr, ptr @n2, align 8
  %g1 = load ptr, ptr @n1, align 8
  %g2 = load ptr, ptr @n2, align 8
  %3 = getelementptr inbounds nuw %Node.3070, ptr %g1, i32 0, i32 2
  store ptr %g2, ptr %3, align 8
  %g3 = load ptr, ptr @n3, align 8
  %g4 = load ptr, ptr @n2, align 8
  %g5 = load ptr, ptr @n3, align 8
  %4 = getelementptr inbounds nuw %Node.3070, ptr %g4, i32 0, i32 2
  store ptr %g5, ptr %4, align 8
  %g6 = load ptr, ptr @n1, align 8
  %5 = call i64 @_CG_f_1965_4(ptr %g6)
  %6 = call ptr @_CG_f_882_1(i64 %5)
  %7 = call ptr @_CG_write(ptr %6)
  %8 = call ptr @_CG_writeln()
  %9 = call ptr @_CG_f_2968_3(i64 1)
  store ptr %9, ptr @head, align 8
  %g7 = load ptr, ptr @head, align 8
  store ptr %g7, ptr @prev, align 8
  store i64 2, ptr @i, align 4
  br label %L195

L195:                                             ; preds = %L278, %entry
  %g8 = load i64, ptr @i, align 4
  %10 = icmp sle i64 %g8, 10
  br i1 %10, label %t_edge, label %f_edge

L278:                                             ; preds = %t_edge
  %g9 = load i64, ptr @i, align 4
  %11 = call ptr @_CG_f_2968_3(i64 %g9)
  store ptr %11, ptr @cur, align 8
  %g10 = load ptr, ptr @cur, align 8
  %g11 = load ptr, ptr @prev, align 8
  %g12 = load ptr, ptr @cur, align 8
  %12 = getelementptr inbounds nuw %Node.3070, ptr %g11, i32 0, i32 2
  store ptr %g12, ptr %12, align 8
  %g13 = load ptr, ptr @cur, align 8
  store ptr %g13, ptr @prev, align 8
  %g14 = load i64, ptr @i, align 4
  %13 = add i64 %g14, 1
  store i64 %13, ptr @i, align 4
  br label %L195

L196:                                             ; preds = %f_edge
  %g15 = load ptr, ptr @head, align 8
  %14 = call i64 @_CG_f_1965_4(ptr %g15)
  %15 = call ptr @_CG_f_882_1(i64 %14)
  %16 = call ptr @_CG_write(ptr %15)
  %17 = call ptr @_CG_writeln()
  %g16 = load ptr, ptr @None, align 8
  %18 = call i64 @_CG_f_1965_4(ptr %g16)
  %19 = call ptr @_CG_f_882_1(i64 %18)
  %20 = call ptr @_CG_write(ptr %19)
  %21 = call ptr @_CG_writeln()
  %g17 = load ptr, ptr @None, align 8
  ret ptr %g17

t_edge:                                           ; preds = %L195
  br label %L278

f_edge:                                           ; preds = %L195
  br label %L196
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
!4 = !DIFile(filename: "recursive_list_is_none.py", directory: "tests")
