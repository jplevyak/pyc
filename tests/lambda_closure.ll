; ModuleID = 'ifa_output'
source_filename = "lambda_closure.py"
target triple = "arm64-apple-darwin25.5.0"

%A.1946 = type { i64, ptr }
%closure_closure.2993 = type { ptr, ptr }

@".=" = internal global ptr null
@__primitive = internal global ptr null
@new = internal global ptr null
@__str__ = internal global ptr null
@A = internal global ptr null
@i = internal global ptr null
@reply = internal global ptr null
@x = internal global ptr null
@z = internal global ptr null
@int = internal global ptr null
@g = internal global ptr null
@__operator = internal global ptr null
@clone = internal global ptr null
@. = internal global ptr null
@writeln = internal global ptr null
@y = internal global ptr null
@write = internal global ptr null
@__pyc_c_call__ = internal global ptr null
@a = internal global ptr null
@str = internal global ptr null

define internal ptr @_CG_f_882_0(i64 %self) {
entry:
  %v = call ptr @_CG_str_from_int(i64 %self)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal i64 @_CG_f_1954_1(ptr %y) {
entry:
  %0 = getelementptr inbounds nuw %A.1946, ptr %y, i32 0, i32 0
  %v = load i64, ptr %0, align 4
  ret i64 %v
}

define internal ptr @_CG_f_1948_2(ptr %arg) {
entry:
  %0 = getelementptr inbounds nuw %A.1946, ptr %arg, i32 0, i32 0
  store i64 3, ptr %0, align 4
  %1 = getelementptr inbounds nuw %A.1946, ptr %arg, i32 0, i32 1
  store ptr @_CG_f_1954_1, ptr %1, align 8
  ret ptr %arg
}

define internal ptr @_CG_f_2923_3(ptr %arg) {
entry:
  ret ptr %arg
}

define internal ptr @_CG_f_2928_4() {
entry:
  %0 = load ptr, ptr @g, align 8
  %v = call ptr @GC_malloc(i64 16)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %v, ptr align 8 %0, i64 16, i1 false)
  ret ptr %v
}

define internal ptr @_CG_f_105_5() {
entry:
  %proto = call ptr @GC_malloc(i64 16)
  store ptr %proto, ptr @g, align 8
  %A = call ptr @GC_malloc(i64 16)
  store ptr %A, ptr @A, align 8
  %a = call ptr @GC_malloc(i64 16)
  store ptr %a, ptr @a, align 8
  %z = call ptr @GC_malloc(i64 16)
  store ptr %z, ptr @z, align 8
  %y = call ptr @GC_malloc(i64 16)
  store ptr %y, ptr @y, align 8
  %v = call ptr @GC_malloc(i64 48)
  %v1 = call ptr @GC_malloc(i64 40)
  %v2 = call ptr @GC_malloc(i64 40)
  %v3 = call ptr @GC_malloc(i64 40)
  %v4 = call ptr @GC_malloc(i64 40)
  %v5 = call ptr @GC_malloc(i64 56)
  %v6 = call ptr @GC_malloc(i64 56)
  %p = call ptr @GC_malloc(i64 16)
  store ptr %p, ptr @g, align 8
  %0 = load ptr, ptr @g, align 8
  %v7 = call ptr @_CG_f_1948_2(ptr %0)
  %v8 = call ptr @_CG_f_2928_4()
  store ptr %v8, ptr @a, align 8
  %v9 = call ptr @GC_malloc(i64 16)
  %x = load ptr, ptr @x, align 8
  %1 = getelementptr inbounds nuw %closure_closure.2993, ptr %v9, i32 0, i32 0
  store ptr %x, ptr %1, align 8
  %a10 = load ptr, ptr @a, align 8
  %2 = getelementptr inbounds nuw %closure_closure.2993, ptr %v9, i32 0, i32 1
  store ptr %a10, ptr %2, align 8
  store ptr %v9, ptr @z, align 8
  %a11 = load ptr, ptr @a, align 8
  %3 = getelementptr inbounds nuw %A.1946, ptr %a11, i32 0, i32 0
  store i64 4, ptr %3, align 4
  %z12 = load ptr, ptr @z, align 8
  %4 = getelementptr inbounds nuw %closure_closure.2993, ptr %z12, i32 0, i32 1
  %clo_arg = load ptr, ptr %4, align 8
  %v13 = call i64 @_CG_f_1954_1(ptr %clo_arg)
  %v14 = call ptr @_CG_f_882_0(i64 %v13)
  %v15 = call ptr @_CG_write(ptr %v14)
  %v16 = call ptr @_CG_writeln()
  ret ptr undef
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
!4 = !DIFile(filename: "lambda_closure.py", directory: "tests")
