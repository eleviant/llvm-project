; RUN: llc -filetype=asm -print-after=cfguardcheck -O3 %s -o %t.s 2>&1 | FileCheck %s --check-prefix=MIR
; RUN: cat %t.s | FileCheck %s --check-prefix=ASM

; MIR:       cfguard-check BLR killed renamable $x9, <regmask {{.*}}>, implicit-def dead $lr, implicit $sp, implicit killed $x15
; MIR-NEXT:  BLR killed renamable $x8

; ASM:      blr x9
; ASM-NEXT: blr x8 

target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-p:64:64-i32:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-pc-windows-msvc19.33.0"

@__guard_check_icall_fptr = external dso_local global ptr

; Function Attrs: mustprogress nounwind sspstrong uwtable
define dso_local noundef i32 @"?test@@YAHPEAUFoo@@@Z"(ptr noundef %foo) local_unnamed_addr #0 {
entry:
  %call = call noundef double @"?xf@@YANXZ"() #2
  %vtable = load ptr, ptr %foo, align 8
  %vfn = getelementptr inbounds nuw i8, ptr %vtable, i64 8
  %0 = load ptr, ptr %vfn, align 8
  %1 = load ptr, ptr @__guard_check_icall_fptr, align 8
  call cfguard_checkcc void %1(ptr %0)
  %call1 = call noundef i32 %0(ptr noundef nonnull align 8 dereferenceable(8) %foo, double noundef %call, i32 noundef 1, i32 noundef 2, i32 noundef 3, i32 noundef 4, i32 noundef 5, i32 noundef 6, i32 noundef 7, i32 noundef 8, i32 noundef 9, i32 noundef 10) #2
  ret i32 %call1
}

declare dso_local noundef double @"?xf@@YANXZ"() local_unnamed_addr #1

attributes #0 = { mustprogress nounwind sspstrong uwtable "disable-tail-calls"="true" "frame-pointer"="reserved" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+v8a,-fmv" }
attributes #1 = { "disable-tail-calls"="true" "frame-pointer"="reserved" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+v8a,-fmv" }
attributes #2 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.linker.options = !{!2, !3}
!llvm.module.flags = !{!4, !5, !6, !7, !8, !9}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 23.0.0git (git@github.com:eleviant/llvm-project.git 53f9aea5aa6c3c9e23cc6ee17c85d68e4a551184)", isOptimized: true, runtimeVersion: 0, emissionKind: NoDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test3.cpp", directory: "/home/evgeny/work/MS/cfg_test")
!2 = !{!"/DEFAULTLIB:libcmt.lib"}
!3 = !{!"/DEFAULTLIB:oldnames.lib"}
!4 = !{i32 2, !"cfguard", i32 2}
!5 = !{i32 2, !"cfguard-mechanism", i32 1}
!6 = !{i32 2, !"Debug Info Version", i32 3}
!7 = !{i32 8, !"PIC Level", i32 2}
!8 = !{i32 7, !"uwtable", i32 2}
!9 = !{i32 7, !"frame-pointer", i32 3}
