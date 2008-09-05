// Copyright 2006-2008 Google Inc. All Rights Reserved.
// <<license>>

#include <stdlib.h>

#include "v8.h"

#include "disassembler.h"
#include "factory.h"
#include "macro-assembler.h"
#include "platform.h"
#include "serialize.h"

using namespace v8::internal;


typedef int (*F0)();
typedef int (*F1)(int x);
typedef int (*F2)(int x, int y);


static v8::Persistent<v8::Context> env;


static void InitializeVM() {
  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}


#define __ assm.

static void Test0() {
  InitializeVM();
  v8::HandleScope scope;

  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);

  __ mov(eax, Operand(esp, 4));
  __ add(eax, Operand(esp, 8));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
  int res = f(3, 4);
  ::printf("f() = %d\n", res);
  CHECK_EQ(7, res);
}


static void Test1() {
  InitializeVM();
  v8::HandleScope scope;

  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);
  Label L, C;

  __ mov(edx, Operand(esp, 4));
  __ xor_(eax, Operand(eax));  // clear eax
  __ jmp(&C);

  __ bind(&L);
  __ add(eax, Operand(edx));
  __ sub(Operand(edx), Immediate(1));

  __ bind(&C);
  __ test(edx, Operand(edx));
  __ j(not_zero, &L, taken);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = f(100);
  ::printf("f() = %d\n", res);
  CHECK_EQ(5050, res);
}


static void Test2() {
  InitializeVM();
  v8::HandleScope scope;

  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);
  Label L, C;

  __ mov(edx, Operand(esp, 4));
  __ mov(eax, 1);
  __ jmp(&C);

  __ bind(&L);
  __ imul(eax, Operand(edx));
  __ sub(Operand(edx), Immediate(1));

  __ bind(&C);
  __ test(edx, Operand(edx));
  __ j(not_zero, &L, taken);
  __ ret(0);

  // some relocated stuff here, not executed
  __ mov(eax, Factory::true_value());
  __ jmp(NULL, runtime_entry);

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = f(10);
  ::printf("f() = %d\n", res);
  CHECK_EQ(3628800, res);
}


typedef int (*F3)(float x);

static void Test3() {
  InitializeVM();
  v8::HandleScope scope;

  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);

  Serializer::disable();  // Needed for Probe when running without snapshot.
  CpuFeatures::Probe();
  CHECK(CpuFeatures::IsSupported(CpuFeatures::SSE2));
  { CpuFeatures::Scope fscope(CpuFeatures::SSE2);
    __ cvttss2si(eax, Operand(esp, 4));
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code =
      Code::cast(Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB)));
  // don't print the code - our disassembler can't handle cvttss2si
  // instead print bytes
  Disassembler::Dump(stdout,
                     code->instruction_start(),
                     code->instruction_start() + code->instruction_size());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  int res = f(static_cast<float>(-3.1415));
  ::printf("f() = %d\n", res);
  CHECK_EQ(-3, res);
}


typedef int (*F4)(double x);

static void Test4() {
  InitializeVM();
  v8::HandleScope scope;

  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);

  Serializer::disable();  // Needed for Probe when running without snapshot.
  CpuFeatures::Probe();
  CHECK(CpuFeatures::IsSupported(CpuFeatures::SSE2));
  CpuFeatures::Scope fscope(CpuFeatures::SSE2);
  __ cvttsd2si(eax, Operand(esp, 4));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code =
      Code::cast(Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB)));
  // don't print the code - our disassembler can't handle cvttsd2si
  // instead print bytes
  Disassembler::Dump(stdout,
                     code->instruction_start(),
                     code->instruction_start() + code->instruction_size());
  F4 f = FUNCTION_CAST<F4>(code->entry());
  int res = f(2.718281828);
  ::printf("f() = %d\n", res);
  CHECK_EQ(2, res);
}


static int baz = 42;
static void Test5() {
  InitializeVM();
  v8::HandleScope scope;

  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);

  __ mov(eax, Operand(reinterpret_cast<intptr_t>(&baz), no_reloc));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code =
      Code::cast(Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB)));
  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


typedef double (*F5)(double x, double y);

static void Test6() {
  InitializeVM();
  v8::HandleScope scope;
  Serializer::disable();  // Needed for Probe when running without snapshot.
  CpuFeatures::Probe();
  CHECK(CpuFeatures::IsSupported(CpuFeatures::SSE2));
  CpuFeatures::Scope fscope(CpuFeatures::SSE2);
  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);

  __ movdbl(xmm0, Operand(esp, 1 * kPointerSize));
  __ movdbl(xmm1, Operand(esp, 3 * kPointerSize));
  __ addsd(xmm0, xmm1);
  __ mulsd(xmm0, xmm1);
  __ subsd(xmm0, xmm1);
  __ divsd(xmm0, xmm1);
  __ movdbl(Operand(esp, 4), xmm0);
  __ fld_d(Operand(esp, 4));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code =
      Code::cast(Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB)));
#ifdef DEBUG
  ::printf("\n---\n");
  // don't print the code - our disassembler can't handle SSE instructions
  // instead print bytes
  Disassembler::Dump(stdout,
                     code->instruction_start(),
                     code->instruction_start() + code->instruction_size());
#endif
  F5 f = FUNCTION_CAST<F5>(code->entry());
  double res = f(2.2, 1.1);
  ::printf("f() = %f\n", res);
  CHECK(2.29 < res && res < 2.31);
}


typedef double (*F6)(int x);

static void Test8() {
  InitializeVM();
  v8::HandleScope scope;
  Serializer::disable();  // Needed for Probe when running without snapshot.
  CpuFeatures::Probe();
  CHECK(CpuFeatures::IsSupported(CpuFeatures::SSE2));
  CpuFeatures::Scope fscope(CpuFeatures::SSE2);
  v8::internal::byte buffer[256];
  Assembler assm(buffer, sizeof buffer);
  __ mov(eax, Operand(esp, 4));
  __ cvtsi2sd(xmm0, Operand(eax));
  __ movdbl(Operand(esp, 4), xmm0);
  __ fld_d(Operand(esp, 4));
  __ ret(0);
  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code =
      Code::cast(Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB)));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F6 f = FUNCTION_CAST<F6>(Code::cast(code)->entry());
  double res = f(12);

  ::printf("f() = %f\n", res);
  CHECK(11.99 < res && res < 12.001);
}


typedef int (*F7)(double x, double y);

static void Test9() {
  InitializeVM();
  v8::HandleScope scope;
  v8::internal::byte buffer[256];
  MacroAssembler assm(buffer, sizeof buffer);
  enum { kEqual = 0, kGreater = 1, kLess = 2, kNaN = 3, kUndefined = 4 };
  Label equal_l, less_l, greater_l, nan_l;
  __ fld_d(Operand(esp, 3 * kPointerSize));
  __ fld_d(Operand(esp, 1 * kPointerSize));
  __ FCmp();
  __ j(parity_even, &nan_l, taken);
  __ j(equal, &equal_l, taken);
  __ j(below, &less_l, taken);
  __ j(above, &greater_l, taken);

  __ mov(eax, kUndefined);
  __ ret(0);

  __ bind(&equal_l);
  __ mov(eax, kEqual);
  __ ret(0);

  __ bind(&greater_l);
  __ mov(eax, kGreater);
  __ ret(0);

  __ bind(&less_l);
  __ mov(eax, kLess);
  __ ret(0);

  __ bind(&nan_l);
  __ mov(eax, kNaN);
  __ ret(0);


  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code =
      Code::cast(Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB)));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif

  F7 f = FUNCTION_CAST<F7>(Code::cast(code)->entry());
  CHECK_EQ(kLess, f(1.1, 2.2));
  CHECK_EQ(kEqual, f(2.2, 2.2));
  CHECK_EQ(kGreater, f(3.3, 2.2));
  CHECK_EQ(kNaN, f(OS::nan_value(), 1.1));
}

#undef __
