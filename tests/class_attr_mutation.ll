; ModuleID = 'ifa_output'
source_filename = "class_attr_mutation.py"
target triple = "arm64-apple-darwin25.5.0"

%A.1947 = type { i64 }
%B.1966 = type { i64, i64 }

@__str__ = internal global ptr null
@A = internal global ptr null
@write = internal global ptr null
@__primitive = internal global ptr null
@int = internal global ptr null
@. = internal global ptr null
@".=" = internal global ptr null
@g = internal global ptr null
@reply = internal global ptr null
@__operator = internal global ptr null
@__pyc_c_call__ = internal global ptr null
@n = internal global ptr null
@str = internal global ptr null
@y = internal global ptr null
@writeln = internal global ptr null
@g.1 = internal global ptr null
@clone = internal global ptr null
@B = internal global ptr null
@new = internal global ptr null
@o = internal global ptr null

define internal ptr @_CG_f_882_0() {
entry:
  %v = call ptr @_CG_str_from_int(i64 3)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_1949_1(ptr %arg) {
entry:
  %0 = getelementptr inbounds nuw %A.1947, ptr %arg, i32 0, i32 0
  store i64 2, ptr %0, align 4
  %v = call ptr @_CG_f_882_12()
  %v1 = call ptr @_CG_write(ptr %v)
  %v2 = call ptr @_CG_writeln()
  %1 = getelementptr inbounds nuw %A.1947, ptr %arg, i32 0, i32 0
  %v3 = load i64, ptr %1, align 4
  %v4 = call ptr @_CG_f_882_8(i64 %v3)
  %v5 = call ptr @_CG_write(ptr %v4)
  %v6 = call ptr @_CG_writeln()
  ret ptr %arg
}

define internal ptr @_CG_f_2985_2(ptr %arg) {
entry:
  ret ptr %arg
}

define internal ptr @_CG_f_2990_3() {
entry:
  %0 = load ptr, ptr @g, align 8
  %v = call ptr @GC_malloc(i64 8)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %v, ptr align 8 %0, i64 8, i1 false)
  ret ptr %v
}

define internal ptr @_CG_f_1968_4(ptr %arg) {
entry:
  %0 = load ptr, ptr @g, align 8
  %1 = getelementptr inbounds nuw %A.1947, ptr %0, i32 0, i32 0
  %v = load i64, ptr %1, align 4
  %2 = getelementptr inbounds nuw %B.1966, ptr %arg, i32 0, i32 0
  store i64 %v, ptr %2, align 4
  %3 = getelementptr inbounds nuw %B.1966, ptr %arg, i32 0, i32 1
  store i64 3, ptr %3, align 4
  %v1 = call ptr @_CG_f_882_9()
  %v2 = call ptr @_CG_write(ptr %v1)
  %v3 = call ptr @_CG_writeln()
  %v4 = call ptr @_CG_f_882_10()
  %v5 = call ptr @_CG_write(ptr %v4)
  %v6 = call ptr @_CG_writeln()
  ret ptr %arg
}

define internal ptr @_CG_f_3016_5(ptr %arg) {
entry:
  ret ptr %arg
}

define internal ptr @_CG_f_3021_6() {
entry:
  %0 = load ptr, ptr @g.1, align 8
  %v = call ptr @GC_malloc(i64 16)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %v, ptr align 8 %0, i64 16, i1 false)
  ret ptr %v
}

define internal ptr @_CG_f_105_7() {
entry:
  %A = call ptr @GC_malloc(i64 8)
  store ptr %A, ptr @A, align 8
  %proto = call ptr @GC_malloc(i64 16)
  store ptr %proto, ptr @g.1, align 8
  %y = call ptr @GC_malloc(i64 16)
  store ptr %y, ptr @y, align 8
  %B = call ptr @GC_malloc(i64 16)
  store ptr %B, ptr @B, align 8
  %proto1 = call ptr @GC_malloc(i64 8)
  store ptr %proto1, ptr @g, align 8
  %v = call ptr @GC_malloc(i64 48)
  %v2 = call ptr @GC_malloc(i64 40)
  %v3 = call ptr @GC_malloc(i64 40)
  %v4 = call ptr @GC_malloc(i64 40)
  %v5 = call ptr @GC_malloc(i64 40)
  %v6 = call ptr @GC_malloc(i64 56)
  %v7 = call ptr @GC_malloc(i64 56)
  %p = call ptr @GC_malloc(i64 8)
  store ptr %p, ptr @g, align 8
  %0 = load ptr, ptr @g, align 8
  %v8 = call ptr @_CG_f_1949_1(ptr %0)
  %1 = load ptr, ptr @g, align 8
  %2 = getelementptr inbounds nuw %A.1947, ptr %1, i32 0, i32 0
  %v9 = load i64, ptr %2, align 4
  %v10 = call ptr @_CG_f_882_11(i64 %v9)
  %v11 = call ptr @_CG_write(ptr %v10)
  %v12 = call ptr @_CG_writeln()
  %p13 = call ptr @GC_malloc(i64 16)
  store ptr %p13, ptr @g.1, align 8
  %3 = load ptr, ptr @g.1, align 8
  %v14 = call ptr @_CG_f_1968_4(ptr %3)
  %4 = load ptr, ptr @g.1, align 8
  %5 = getelementptr inbounds nuw %B.1966, ptr %4, i32 0, i32 0
  %v15 = load i64, ptr %5, align 4
  %v16 = call ptr @_CG_f_882_11(i64 %v15)
  %v17 = call ptr @_CG_write(ptr %v16)
  %v18 = call ptr @_CG_writeln()
  %6 = load ptr, ptr @g, align 8
  %7 = getelementptr inbounds nuw %A.1947, ptr %6, i32 0, i32 0
  store i64 4, ptr %7, align 4
  %8 = load ptr, ptr @g, align 8
  %9 = getelementptr inbounds nuw %A.1947, ptr %8, i32 0, i32 0
  %v19 = load i64, ptr %9, align 4
  %v20 = call ptr @_CG_f_882_11(i64 %v19)
  %v21 = call ptr @_CG_write(ptr %v20)
  %v22 = call ptr @_CG_writeln()
  %10 = load ptr, ptr @g.1, align 8
  %11 = getelementptr inbounds nuw %B.1966, ptr %10, i32 0, i32 0
  %v23 = load i64, ptr %11, align 4
  %v24 = call ptr @_CG_f_882_11(i64 %v23)
  %v25 = call ptr @_CG_write(ptr %v24)
  %v26 = call ptr @_CG_writeln()
  %v27 = call ptr @_CG_f_2990_3()
  %12 = getelementptr inbounds nuw %A.1947, ptr %v27, i32 0, i32 0
  %v28 = load i64, ptr %12, align 4
  %v29 = call ptr @_CG_f_882_11(i64 %v28)
  %v30 = call ptr @_CG_write(ptr %v29)
  %v31 = call ptr @_CG_writeln()
  %v32 = call ptr @_CG_f_3021_6()
  %13 = getelementptr inbounds nuw %B.1966, ptr %v32, i32 0, i32 0
  %v33 = load i64, ptr %13, align 4
  %v34 = call ptr @_CG_f_882_11(i64 %v33)
  %v35 = call ptr @_CG_write(ptr %v34)
  %v36 = call ptr @_CG_writeln()
  %14 = load ptr, ptr @g.1, align 8
  %15 = getelementptr inbounds nuw %B.1966, ptr %14, i32 0, i32 0
  store i64 5, ptr %15, align 4
  %16 = load ptr, ptr @g, align 8
  %17 = getelementptr inbounds nuw %A.1947, ptr %16, i32 0, i32 0
  %v37 = load i64, ptr %17, align 4
  %v38 = call ptr @_CG_f_882_11(i64 %v37)
  %v39 = call ptr @_CG_write(ptr %v38)
  %v40 = call ptr @_CG_writeln()
  %v41 = call ptr @_CG_f_3021_6()
  store ptr %v41, ptr @y, align 8
  %y42 = load ptr, ptr @y, align 8
  %18 = getelementptr inbounds nuw %B.1966, ptr %y42, i32 0, i32 0
  %v43 = load i64, ptr %18, align 4
  %v44 = call ptr @_CG_f_882_11(i64 %v43)
  %v45 = call ptr @_CG_write(ptr %v44)
  %v46 = call ptr @_CG_writeln()
  %v47 = call ptr @_CG_f_882_0()
  %v48 = call ptr @_CG_write(ptr %v47)
  %v49 = call ptr @_CG_writeln()
  ret ptr undef
}

define internal ptr @_CG_f_882_8(i64 %self) {
entry:
  %v = call ptr @_CG_str_from_int(i64 %self)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_9() {
entry:
  %v = call ptr @_CG_str_from_int(i64 1)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_10() {
entry:
  %v = call ptr @_CG_str_from_int(i64 3)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_11(i64 %self) {
entry:
  %v = call ptr @_CG_str_from_int(i64 %self)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_882_12() {
entry:
  %v = call ptr @_CG_str_from_int(i64 1)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

declare ptr @_CG_str_from_int(i64)

declare ptr @_CG_write(ptr)

declare ptr @_CG_writeln()

declare ptr @GC_malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

define i32 @main() {
entry:
  %0 = call ptr @_CG_f_105_7()
  ret i32 0
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.dbg.cu = !{!3}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 0}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DICompileUnit(language: DW_LANG_C, file: !4, producer: "ifa-compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!4 = !DIFile(filename: "class_attr_mutation.py", directory: "tests")
