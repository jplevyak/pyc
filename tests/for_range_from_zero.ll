; ModuleID = 'ifa_output'
source_filename = "for_range_from_zero.py"
target triple = "arm64-apple-darwin25.5.0"

%range.1437 = type { ptr, ptr, ptr, ptr, i64, i64, i64 }
%closure_closure.2959 = type { ptr, ptr }

@__next__ = internal global ptr null
@reply = internal global ptr null
@__primitive = internal global ptr null
@".=" = internal global ptr null
@"<" = internal global ptr null
@. = internal global ptr null
@new = internal global ptr null
@__operator = internal global ptr null
@__str__ = internal global ptr null
@__pyc_to_bool__ = internal global ptr null
@__init__ = internal global ptr null
@j = internal global ptr null
@s = internal global ptr null
@i = internal global ptr null
@__pyc_more__ = internal global ptr null
@"+" = internal global ptr null
@writeln = internal global ptr null
@str = internal global ptr null
@write = internal global ptr null
@g = internal global ptr null
@__gt__ = internal global ptr null
@range = internal global ptr null
@__ge__ = internal global ptr null
@clone = internal global ptr null
@int = internal global ptr null
@x = internal global i64 0
@__iter__ = internal global ptr null
@__pyc_c_call__ = internal global ptr null

define internal ptr @_CG_f_882_0(i64 %self) {
entry:
  %v = call ptr @_CG_str_from_int(i64 %self)
  br label %L80

L80:                                              ; preds = %entry
  ret ptr %v
}

define internal ptr @_CG_f_1675_1(ptr %self, i64 %aj) {
entry:
  %0 = getelementptr inbounds nuw %range.1437, ptr %self, i32 0, i32 5
  store i64 10, ptr %0, align 4
  br label %L170

L170:                                             ; preds = %entry
  ret ptr %self
}

define internal i1 @_CG_f_1699_2(ptr %self) {
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

define internal ptr @_CG_f_1723_3(ptr %this) {
entry:
  br label %L173

L173:                                             ; preds = %entry
  ret ptr %this
}

define internal i64 @_CG_f_1729_4(ptr %self) {
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

define internal ptr @_CG_f_1667_5(ptr %arg) {
entry:
  %0 = getelementptr inbounds nuw %range.1437, ptr %arg, i32 0, i32 4
  store i64 0, ptr %0, align 4
  %1 = getelementptr inbounds nuw %range.1437, ptr %arg, i32 0, i32 5
  store i64 0, ptr %1, align 4
  %2 = getelementptr inbounds nuw %range.1437, ptr %arg, i32 0, i32 6
  store i64 1, ptr %2, align 4
  %3 = load ptr, ptr @g, align 8
  %4 = getelementptr inbounds nuw %range.1437, ptr %3, i32 0, i32 0
  store ptr @_CG_f_1675_1, ptr %4, align 8
  %5 = load ptr, ptr @g, align 8
  %6 = load ptr, ptr @g, align 8
  %7 = getelementptr inbounds nuw %range.1437, ptr %6, i32 0, i32 3
  store ptr @_CG_f_1699_2, ptr %7, align 8
  %8 = load ptr, ptr @g, align 8
  %9 = getelementptr inbounds nuw %range.1437, ptr %8, i32 0, i32 1
  store ptr @_CG_f_1723_3, ptr %9, align 8
  %10 = load ptr, ptr @g, align 8
  %11 = getelementptr inbounds nuw %range.1437, ptr %10, i32 0, i32 2
  store ptr @_CG_f_1729_4, ptr %11, align 8
  ret ptr %arg
}

define internal ptr @_CG_f_2740_6(i64 %arg) {
entry:
  %0 = load ptr, ptr @g, align 8
  %v = call ptr @GC_malloc(i64 56)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %v, ptr align 8 %0, i64 56, i1 false)
  %v1 = call ptr @_CG_f_1675_1(ptr %v, i64 10)
  ret ptr %v
}

define internal ptr @_CG_f_105_7() {
entry:
  %range = call ptr @GC_malloc(i64 56)
  store ptr %range, ptr @range, align 8
  %proto = call ptr @GC_malloc(i64 56)
  store ptr %proto, ptr @g, align 8
  %v = alloca ptr, align 8
  %v1 = alloca i1, align 1
  %v2 = alloca ptr, align 8
  %v3 = alloca i1, align 1
  %v4 = alloca ptr, align 8
  %v5 = call ptr @GC_malloc(i64 48)
  %v6 = call ptr @GC_malloc(i64 40)
  %v7 = call ptr @GC_malloc(i64 40)
  %v8 = call ptr @GC_malloc(i64 40)
  %v9 = call ptr @GC_malloc(i64 40)
  %p = call ptr @GC_malloc(i64 56)
  store ptr %p, ptr @g, align 8
  %0 = load ptr, ptr @g, align 8
  %v10 = call ptr @_CG_f_1667_5(ptr %0)
  %v11 = call ptr @GC_malloc(i64 56)
  %v12 = call ptr @_CG_f_2740_6(i64 10)
  store ptr %v12, ptr %v, align 8
  br label %L192

L192:                                             ; preds = %L273, %entry
  %v13 = call ptr @GC_malloc(i64 16)
  %__pyc_more__ = load ptr, ptr @__pyc_more__, align 8
  %1 = getelementptr inbounds nuw %closure_closure.2959, ptr %v13, i32 0, i32 0
  store ptr %__pyc_more__, ptr %1, align 8
  %v14 = load ptr, ptr %v, align 8
  %2 = getelementptr inbounds nuw %closure_closure.2959, ptr %v13, i32 0, i32 1
  store ptr %v14, ptr %2, align 8
  %v15 = load ptr, ptr %v, align 8
  %v16 = call i1 @_CG_f_1699_2(ptr %v15)
  store i1 %v16, ptr %v1, align 1
  %v17 = load ptr, ptr %v, align 8
  store ptr %v17, ptr %v2, align 8
  store i1 %v16, ptr %v3, align 1
  %v18 = load ptr, ptr %v, align 8
  store ptr %v18, ptr %v4, align 8
  br i1 %v16, label %L273, label %L193

L273:                                             ; preds = %L192
  %v19 = call ptr @GC_malloc(i64 16)
  %__next__ = load ptr, ptr @__next__, align 8
  %3 = getelementptr inbounds nuw %closure_closure.2959, ptr %v19, i32 0, i32 0
  store ptr %__next__, ptr %3, align 8
  %v20 = load ptr, ptr %v2, align 8
  %4 = getelementptr inbounds nuw %closure_closure.2959, ptr %v19, i32 0, i32 1
  store ptr %v20, ptr %4, align 8
  %v21 = load ptr, ptr %v2, align 8
  %v22 = call i64 @_CG_f_1729_4(ptr %v21)
  store i64 %v22, ptr @x, align 4
  %x = load i64, ptr @x, align 4
  %v23 = call ptr @_CG_f_882_0(i64 %x)
  %v24 = call ptr @_CG_write(ptr %v23)
  %v25 = call ptr @_CG_writeln()
  %v26 = load ptr, ptr %v2, align 8
  store ptr %v26, ptr %v, align 8
  br label %L192

L193:                                             ; preds = %L192
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
!4 = !DIFile(filename: "for_range_from_zero.py", directory: "..")
